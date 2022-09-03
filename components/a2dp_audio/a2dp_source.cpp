/*
 * a2dp_source.c
 *
 *  Created on: 27.08.2020
 *      Author: wolle
 *
 *  updated on: 07.05.2021
 *
 */

#include <Arduino.h>
#include "a2dp_source.h"

// #define ArduinoVers_2 /* uncomment this if vers >= 2.0.0, events run on core 1, Arduino runs on core 1 */

namespace esphome {
namespace a2dp_audio {

A2DPSource::A2DPSource()
{
    s_bt_app_task_queue = NULL;
    s_bt_app_task_handle = NULL;
    s_peer_bda = 0;
    s_a2d_state = APP_AV_STATE_IDLE;
    s_media_state = APP_AV_MEDIA_STATE_IDLE;
    s_BT_sink_name = "";
    //s_pin_code = "";
    s_pin_code_length = 0;
    s_deviceName = "ESP_A2DP_SRC";
}
//---------------------------------------------------------------------------------------------------------------------
bool A2DPSource::bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len, bt_app_copy_cb_t p_copy_cback)
{
    log_d("event 0x%x, param len %d", event, param_len);

    bt_app_msg_t msg;
    memset(&msg, 0, sizeof(bt_app_msg_t));

    msg.sig = BT_APP_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = p_cback;

    if (param_len == 0) {
        return bt_app_send_msg(&msg);
    } else if (p_params && param_len > 0) {
        if ((msg.param = malloc(param_len)) != NULL) {
            memcpy(msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy */
            if (p_copy_cback) {
                p_copy_cback(&msg, msg.param, p_params);
            }
            return bt_app_send_msg(&msg);
        }
    }

    return false;
}
//---------------------------------------------------------------------------------------------------------------------
bool A2DPSource::bt_app_send_msg(bt_app_msg_t *msg){
    if (msg == NULL) {
        return false;
    }

    if (xQueueSend(s_bt_app_task_queue, msg, 100 / portTICK_RATE_MS) != pdTRUE) {
        log_e("xQueue send failed");
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_work_dispatched(bt_app_msg_t *msg){
    if (msg->cb) {
        msg->cb(msg->event, msg->param);
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_task_handler(void *arg){
    bt_app_msg_t msg;
    for (;;) {
        if (pdTRUE == xQueueReceive(s_bt_app_task_queue, &msg, (portTickType)portMAX_DELAY)) {
            log_d("sig 0x%x, 0x%x", msg.sig, msg.event);
            switch (msg.sig) {
            case BT_APP_SIG_WORK_DISPATCH:
                bt_app_work_dispatched(&msg);
                break;
            default:
                log_e("unhandled sig: %d",msg.sig);
                break;
            } // switch (msg.sig)

            if (msg.param) {
                free(msg.param);
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_task_start_up(void)
{
    s_bt_app_task_queue = xQueueCreate(20, sizeof(bt_app_msg_t));
    xTaskCreate(bt_app_task_handler, "BtAppT", 2048, NULL, configMAX_PRIORITIES - 3, &s_bt_app_task_handle);
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_task_shut_down(void)
{
    if (s_bt_app_task_handle) {
        vTaskDelete(s_bt_app_task_handle);
        s_bt_app_task_handle = NULL;
    }
    if (s_bt_app_task_queue) {
        vQueueDelete(s_bt_app_task_queue);
        s_bt_app_task_queue = NULL;
    }
}
//---------------------------------------------------------------------------------------------------------------------
char *A2DPSource::bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::perform_wipe_security_db(){  // delete all paired devices
    uint8_t paired_device_addr[10][6];
    char bda_str[18];
    esp_err_t res;
    int count = esp_bt_gap_get_bond_device_num(); // get number of bound (pair) devices
    if(!count) {
        log_i("No pair devices found! Nothing to delete!");
        return;
    }
    log_i("Number of paired devices: %d", count);

    // get all of pair devices
    do{
        res = esp_bt_gap_get_bond_device_list(&count, paired_device_addr);
    } while(res != ESP_OK);

    for(int i = 0; i < count; i++) {
        log_i("Deleting paired device with MAC %s", bda2str(paired_device_addr[i], bda_str, 18));
        do{
            res = esp_bt_gap_remove_bond_device(paired_device_addr[i]);
        } while(res != ESP_OK);
    }

    // reboot ESP
    log_i("Rebooting ESP...");
    esp_restart();
}
//---------------------------------------------------------------------------------------------------------------------
bool A2DPSource::get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len){
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param){
    char bda_str[18];
    uint32_t cod = 0;
    int32_t rssi = -129; /* invalid value */
    uint8_t *eir = NULL;
    esp_bt_gap_dev_prop_t *p;

    char* sd = bda2str(param->disc_res.bda, bda_str, 18);
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            // NB enumeration is listed in
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_EIR:
            eir = (uint8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
        default:
            break;
        }
    }
    if (eir) get_name_from_eir(eir, s_peer_bdname, NULL); else s_peer_bdname[0] = 0;
    log_i("Scanned device: %s  --Class of Device: 0x%x, --RSSI %d, --%s", sd, cod, rssi, s_peer_bdname);

    /* search for device with MAJOR service class as "rendering" in COD */
    if (!esp_bt_gap_is_valid_cod(cod) ||
            !(esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING)) {
        log_e("cod is not valid");
        return;
    }

    /* search for device named  BT_SINK in its extended inqury response */
    if (eir) {
        get_name_from_eir(eir, s_peer_bdname, NULL);
        if (strcmp((char *)s_peer_bdname,  s_BT_sink_name.c_str()) != 0) {
            log_i("s_peer_bdname %s, s_BT_sink_name %s", s_peer_bdname, s_BT_sink_name.c_str());
            return;
        }

        log_i("Found a target device, address %s, name %s", bda_str, s_peer_bdname);
        s_a2d_state = APP_AV_STATE_DISCOVERED;
        memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
        log_d("Cancel device discovery ...");
        esp_bt_gap_cancel_discovery();
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param){
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        filter_inquiry_scan_result(param);
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            if (s_a2d_state == APP_AV_STATE_DISCOVERED) {
                s_a2d_state = APP_AV_STATE_CONNECTING;
                log_i("Device discovery stopped: a2dp connecting to peer: %s", s_peer_bdname);
                esp_a2d_source_connect(s_peer_bda);
            } else {
                // not discovered, continue to discover
                log_i("Device discovery failed, continue to discover...");
                esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
            }
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            log_i("Discovery started.");
        }
        break;
    }
    case ESP_BT_GAP_RMT_SRVCS_EVT:
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        break;
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            log_i("authentication success: %s", param->auth_cmpl.device_name);
        } else {
            log_e("authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT: {
        log_i("ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            log_i("Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
//            log_i("Input pin code: 1234");
//            esp_bt_pin_code_t pin_code;
//            pin_code[0] = '1';
//            pin_code[1] = '2';
//            pin_code[2] = '3';
//            pin_code[3] = '4';
//            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            log_i("Input pin code: %s", s_pin_code);
            esp_bt_gap_pin_reply(param->pin_req.bda, true, s_pin_code_length, s_pin_code);
        }
        break;
    }

    default: {
        log_e("unhandled event: %d", event);
        break;
    }
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_av_hdl_stack_evt(uint16_t event, void *p_param){
    log_d("event %d",event);
    switch (event) {
    case BT_APP_EVT_STACK_UP: {
        /* set up device name */
        esp_bt_dev_set_device_name(s_deviceName.c_str());

        /* register GAP callback function */
        esp_bt_gap_register_callback(bt_app_gap_cb);

        /* initialize A2DP source */
        esp_a2d_register_callback(&bt_app_a2d_cb);

        esp_a2d_source_register_data_callback(bt_app_a2d_data_cb);
        esp_a2d_source_init();

        /* set discoverable and connectable mode */
#ifdef ArduinoVers_2
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
        /* start device discovery */
        log_d("Starting device discovery...");
        s_a2d_state = APP_AV_STATE_DISCOVERING;
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);

        /* create and start heart beat timer */
        do {
            int tmr_id = 0;
            s_tmr = xTimerCreate("connTmr", (1000 / portTICK_RATE_MS),
                               pdTRUE, (void *)tmr_id, a2d_app_heart_beat);
            xTimerStart(s_tmr, portMAX_DELAY);
        } while (0);
        break;
    }
    default:
        log_e("unhandled evt %d", event);
        break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param){
    bt_app_work_dispatch(bt_app_av_sm_hdlr, event, param, sizeof(esp_a2d_cb_param_t), NULL);
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::a2d_app_heart_beat(void *arg){
    bt_app_work_dispatch(bt_app_av_sm_hdlr, BT_APP_HEART_BEAT_EVT, NULL, 0, NULL);
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_sm_hdlr(uint16_t event, void *param){
    switch (s_a2d_state) {
    case APP_AV_STATE_DISCOVERING:
        log_d("state %d, evt 0x%x: APP_AV_STATE_DISCOVERING",s_a2d_state, event);
        break;
    case APP_AV_STATE_DISCOVERED:
        log_d("state %d, evt 0x%x: APP_AV_STATE_DISCOVERED",s_a2d_state, event);
        break;
    case APP_AV_STATE_UNCONNECTED:
        log_d("state %d, evt 0x%x: APP_AV_STATE_UNCONNECTED",s_a2d_state, event);
        bt_app_av_state_unconnected(event, param);
        break;
    case APP_AV_STATE_CONNECTING:
        log_d("state %d, evt 0x%x: APP_AV_STATE_CONNECTING",s_a2d_state, event);
        bt_app_av_state_connecting(event, param);
        break;
    case APP_AV_STATE_CONNECTED:
        log_d("state %d, evt 0x%x: APP_AV_STATE_CONNECTED",s_a2d_state, event);
        bt_app_av_state_connected(event, param);
        break;
    case APP_AV_STATE_DISCONNECTING:
        log_d("state %d, evt 0x%x: APP_AV_STATE_DISCONNECTING",s_a2d_state, event);
        bt_app_av_state_disconnecting(event, param);
        break;
    default:
        log_e("invalid state %d", s_a2d_state);
        break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_state_unconnected(uint16_t event, void *param){
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        log_d("ESP_A2D_AUDIO_CFG_EVT");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;
    case BT_APP_HEART_BEAT_EVT: {
        uint8_t *p = s_peer_bda;
        log_i("Heartbeat Event: a2dp most recent peer connection: %s @ %02x:%02x:%02x:%02x:%02x:%02x",
                 s_peer_bdname, p[0], p[1], p[2], p[3], p[4], p[5]);
        esp_a2d_source_connect(s_peer_bda);
        s_a2d_state = APP_AV_STATE_CONNECTING;
        //s_connecting_intv = 0;
        break;
    }
    default:
        log_e("unhandled evt %d", event);
        break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_state_connecting(uint16_t event, void *param){
    log_d("event %i", event);
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            log_i("a2dp connected");
            s_a2d_state =  APP_AV_STATE_CONNECTED;
            s_media_state = APP_AV_MEDIA_STATE_IDLE;
#ifdef ArduinoVers_2
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
#else
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE);
#endif
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_a2d_state =  APP_AV_STATE_UNCONNECTED;
            log_i("a2dp unconnected");
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
            log_i("ESP_A2D_CONNECTION_STATE_CONNECTING");
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT:
        log_d("ESP_A2D_AUDIO_STATE_EVT");
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        log_d("ESP_A2D_AUDIO_CFG_EVT");
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        log_d("ESP_A2D_MEDIA_CTRL_ACK_EVT");
        break;
    case BT_APP_HEART_BEAT_EVT:
        a2d = (esp_a2d_cb_param_t *)(param);
        if(a2d) if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)  log_i("a2dp connected");

        s_a2d_state =  APP_AV_STATE_CONNECTED;
        s_media_state = APP_AV_MEDIA_STATE_IDLE;
#ifdef ArduinoVers_2
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
#else
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE);
#endif
//        if (++s_connecting_intv >= 2) {
//            s_a2d_state = APP_AV_STATE_UNCONNECTED;
//            s_connecting_intv = 0;
//        }
        break;
    default:
        log_e("unhandled evt %d\n", event);
        break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_media_proc(uint16_t event, void *param){
    esp_a2d_cb_param_t *a2d = NULL;
    a2d = (esp_a2d_cb_param_t *)(param);
    switch (s_media_state) {
        case APP_AV_MEDIA_STATE_IDLE: {
            if (event == BT_APP_HEART_BEAT_EVT) {
                log_i("a2dp media ready checking ...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
            } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
                if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
                        a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                    log_i("a2dp media ready, starting ...");
                    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                    s_media_state = APP_AV_MEDIA_STATE_STARTING;
                }
            }
            break;
        }
        case APP_AV_MEDIA_STATE_STARTING: {
            if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
                a2d = (esp_a2d_cb_param_t *)(param);
                if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
                        a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                    log_i("a2dp media start successfully.");
                    //s_intv_cnt = 0;
                    s_media_state = APP_AV_MEDIA_STATE_STARTED;
                } else {
                    // not started succesfully, transfer to idle state
                    log_e("a2dp media start failed.\n");
                    s_media_state = APP_AV_MEDIA_STATE_IDLE;
                }
            }
            break;
        }
        case APP_AV_MEDIA_STATE_STARTED: {
            if (event == BT_APP_HEART_BEAT_EVT) {
//                if (++s_intv_cnt >= 2) { // was 10
//                    log_i("a2dp media stopping...");
//                    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
//                    s_media_state = APP_AV_MEDIA_STATE_STOPPING;
//                    s_intv_cnt = 0;
//                }
            }
            break;
        }
        case APP_AV_MEDIA_STATE_STOPPING: {
            if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
                a2d = (esp_a2d_cb_param_t *)(param);
                if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP &&
                        a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
                    log_i("a2dp media stopped successfully, disconnecting...");
                    s_media_state = APP_AV_MEDIA_STATE_IDLE;
                    esp_a2d_source_disconnect(s_peer_bda);
                    s_a2d_state = APP_AV_STATE_DISCONNECTING;
                } else {
                    log_i("a2dp media stopping...");
                    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
                }
            }
            break;
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_state_connected(uint16_t event, void *param){
    log_d("Event called: %d",event);
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            a2d = (esp_a2d_cb_param_t *)(param);
            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                log_i("a2dp disconnected");
                s_a2d_state = APP_AV_STATE_UNCONNECTED;
#ifdef ArduinoVers_2
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
                esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
            }
            break;
        }
        case ESP_A2D_AUDIO_STATE_EVT: {
            a2d = (esp_a2d_cb_param_t *)(param);
            if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
                //s_pkt_cnt = 0;
            }
            break;
        }
        case ESP_A2D_AUDIO_CFG_EVT:
            log_d("ESP_A2D_AUDIO_CFG_EVT");
            // not suppposed to occur for A2DP source
            break;
        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        case BT_APP_HEART_BEAT_EVT: {
            bt_app_av_media_proc(event, param);
            break;
        }
        default:
            log_e("unhandled evt %d", event);
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
void A2DPSource::bt_app_av_state_disconnecting(uint16_t event, void *param){
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            log_i("a2dp disconnected");
            s_a2d_state =  APP_AV_STATE_UNCONNECTED;
#ifdef ArduinoVers_2
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
        break;
    default:
        log_e("unhandled evt %d", event);
        break;
    }
}
//---------------------------------------------------------------------------------------------------------------------
int A2DPSource::get_APP_AV_STATE(){
    return s_a2d_state;
}
//---------------------------------------------------------------------------------------------------------------------
bool A2DPSource::a2dp_source_init(String deviceName, String pinCode){
    esp_err_t res;
    s_BT_sink_name = deviceName;
    int s_pin_code_length = pinCode.length();
    for(int i=0; i<s_pin_code_length; i++)
        s_pin_code[i] = pinCode[i];

    do{  // release controller memory
        res= esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    }while(res==0);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); //controller init

    res = esp_bt_controller_init(&bt_cfg);
    if(res != ESP_OK){log_e("initialize controller failed %d", res); return false;}

    res = esp_bt_controller_enable(ESP_BT_MODE_BTDM); // enable BT
    if(res != ESP_OK){log_e("enable controller failed %d", res); return false;}

    res = esp_bluedroid_init(); // init bluedroid
    if(res != ESP_OK){log_e("initialize bluedroid failed"); return false;}

    res = esp_bluedroid_enable(); // activate bluedroid
    if(res != ESP_OK){log_e("enable bluedroid failed"); return false;}

    perform_wipe_security_db(); // delete pair devices
    bt_app_task_start_up(); // create application task
    /* Bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    // Set default parameters for Legacy Pairing, use variable pin, input pin code when pairing
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);


    return true;
}


}
}