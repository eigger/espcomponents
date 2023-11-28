#include "jaalee_jht.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace jaalee_jht {

static const char *const TAG = "jaalee_jht";

void JaaleeJHT::dump_config() {
    ESP_LOGCONFIG(TAG, "Jaalee JHT");
    LOG_SENSOR("  ", "Temperature", this->temperature_);
    LOG_SENSOR("  ", "Humidity", this->humidity_);
    LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool JaaleeJHT::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
    // the address should match the address we declared


// [20:56:26][VV][esp32_ble_tracker:575]: Parse Result:
// [20:56:26][VV][esp32_ble_tracker:592]:   Address: FC:A7:2A:C1:33:07 (RANDOM)
// [20:56:26][VV][esp32_ble_tracker:594]:   RSSI: -57
// [20:56:26][VV][esp32_ble_tracker:595]:   Name: ''
// [20:56:26][VV][esp32_ble_tracker:603]:   Ad Flag: 4
// [20:56:26][VV][esp32_ble_tracker:606]:   Service UUID: 0xF525
// [20:56:26][VV][esp32_ble_tracker:609]:   Manufacturer data: 02.15.EB.EF.D0.83.70.A2.47.C8.98.37.E7.B5.63.4D.F5.25.67.84.76.9E.CB.64 (24)
// [20:56:26][VV][esp32_ble_tracker:621]:     UUID: 0xF525
// [20:56:26][VV][esp32_ble_tracker:622]:     Data: 64.07.33.C1.2A.A7.FC.67.84.76.9E (11)
// [20:56:26][VV][esp32_ble_tracker:625]: Adv data: 02.01.04.1B.FF.4C.00.02.15.EB.EF.D0.83.70.A2.47.C8.98.37.E7.B5.63.4D.F5.25.67.84.76.9E.CB.64.03.03.25.F5.0E.16.25.F5.64.07.33.C1.2A.A7.FC.67.84.76.9E (50)
    if (device.address_uint64() != this->address_) {
        ESP_LOGD(TAG, "parse_device(): unknown MAC address.");
        return false;
    }
    if (device.get_address_type() != BLE_ADDR_TYPE_PUBLIC) {
        ESP_LOGD(TAG, "parse_device(): address is not public");
        return false;
    }
    if (!device.get_service_datas().empty()) {
        ESP_LOGD(TAG, "parse_device(): service_data is expected to be empty");
        return false;
    }
    auto mnf_datas = device.get_manufacturer_datas();
    if (mnf_datas.size() != 1) {
        ESP_LOGD(TAG, "parse_device(): manufacturer_datas is expected to have a single element - size(%d)", mnf_datas.size());
        return false;
    }
    auto mnf_data = mnf_datas[0];
    if (mnf_data.uuid.get_uuid().len != ESP_UUID_LEN_16) {
        ESP_LOGD(TAG, "parse_device(): manufacturer data element is expected to have uuid of length 16");
        return false;
    }
    if (mnf_data.data.size() != 11) {
        ESP_LOGD(TAG, "parse_device(): manufacturer data element length is expected to be of length 11");
        return false;
    }


    // Create empty variables to pass automatic checks
    auto battery_level = mnf_data.data[0];
    auto humidity = (mnf_data.data[7] << 8) + mnf_data.data[8];
    auto temperature = (mnf_data.data[9] << 8) + mnf_data.data[10];

    int digits = 2;
    double multiplier = pow(10.0, digits);
    // temperature = round((175.72 * temperature / 65536 - 46.85) * multiplier) / multiplier;
    // humidity = round((125.0 * humidity / 65536 - 6) * multiplier) / multiplier;
    //http://sensor.jaalee.com/scan_api.html
    temperature = round(((temperature / 65536) * 175 - 45) * multiplier) / multiplier;
    humidity = round(((humidity / 65536) * 100) * multiplier) / multiplier;

    // Send temperature only if the value is set
    if (this->temperature_ != nullptr) {
        this->temperature_->publish_state(temperature);
    }
    if (this->humidity_ != nullptr) {
        this->humidity_->publish_state(humidity);
    }
    if (this->battery_level_ != nullptr) {
        this->battery_level_->publish_state(battery_level);
    }

    return true;
}

}  // namespace jaalee_jht
}  // namespace esphome

#endif
