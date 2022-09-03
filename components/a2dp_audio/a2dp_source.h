/*
 * a2dp_source.h
 *
 *  Created on: 23.08.2020
 *      Author: wolle
 */

#ifndef A2DP_SOURCE_H_
#define A2DP_SOURCE_H_

#include <Arduino.h>

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
namespace esphome {
namespace a2dp_audio {
#define BT_APP_HEART_BEAT_EVT             (0xff00)
#define BT_APP_SIG_WORK_DISPATCH          (0x01)

// event for handler "bt_av_hdl_stack_up
enum {
    BT_APP_EVT_STACK_UP = 0,
};

// A2DP global state
enum {
    APP_AV_STATE_IDLE,
    APP_AV_STATE_DISCOVERING,
    APP_AV_STATE_DISCOVERED,
    APP_AV_STATE_UNCONNECTED,
    APP_AV_STATE_CONNECTING,
    APP_AV_STATE_CONNECTED,
    APP_AV_STATE_DISCONNECTING,
};

// sub states of APP_AV_STATE_CONNECTED
enum {
    APP_AV_MEDIA_STATE_IDLE,
    APP_AV_MEDIA_STATE_STARTING,
    APP_AV_MEDIA_STATE_STARTED,
    APP_AV_MEDIA_STATE_STOPPING,
};
//
//typedef struct { // AVRC target notification event capability bit mask
//    uint16_t bits;                                /*!< bit mask representation of PASSTHROUGH commands */
//} esp_avrc_rn_evt_cap_mask_t;

typedef void (* bt_app_cb_t) (uint16_t event, void *param);

/* message to be sent */
typedef struct {
    uint16_t             sig;      /*!< signal to bt_app_task */
    uint16_t             event;    /*!< message event id */
    bt_app_cb_t          cb;       /*!< context switch callback */
    void                 *param;   /*!< parameter area needs to be last */
} bt_app_msg_t;

typedef void (* bt_app_copy_cb_t) (bt_app_msg_t *msg, void *p_dest, void *p_src);
int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);
class A2DPSource {
public:
    A2DPSource();
    bool bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len, bt_app_copy_cb_t p_copy_cback);
    bool bt_app_send_msg(bt_app_msg_t *msg);
    void bt_app_work_dispatched(bt_app_msg_t *msg);
    void bt_app_task_handler(void *arg);
    void bt_app_task_start_up(void);
    void bt_app_task_shut_down(void);
    char *bda2str(esp_bd_addr_t bda, char *str, size_t size);
    void perform_wipe_security_db(void);
    bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len);
    void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param);
    void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
    void bt_av_hdl_stack_evt(uint16_t event, void *p_param);
    void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
    
    void a2d_app_heart_beat(void *arg);
    void bt_app_av_sm_hdlr(uint16_t event, void *param);
    void bt_app_av_state_unconnected(uint16_t event, void *param);
    void bt_app_av_state_connecting(uint16_t event, void *param);
    void bt_app_av_media_proc(uint16_t event, void *param);
    void bt_app_av_state_connected(uint16_t event, void *param);
    void bt_app_av_state_disconnecting(uint16_t event, void *param);
    int get_APP_AV_STATE();
    bool a2dp_source_init(String deviceName, String pinCode);
private:
    xQueueHandle      s_bt_app_task_queue;
    xTaskHandle       s_bt_app_task_handle;
    esp_bd_addr_t     s_peer_bda;
    uint8_t           s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    int               s_a2d_state;
    int               s_media_state;
    String            s_BT_sink_name;
    esp_bt_pin_code_t s_pin_code;
    int               s_pin_code_length;
    TimerHandle_t     s_tmr;
    String            s_deviceName;
}

}
}
#endif /* A2DP_SOURCE_H_ */
