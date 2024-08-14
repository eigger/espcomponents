#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "version.h"


#ifdef USE_ESP32
#include <esp_gattc_api.h>
namespace esphome {
namespace gicisky_esl {

namespace espbt = esphome::esp32_ble_tracker;

struct ColorPoint
{
    int x;
    int y;
    Color color;
    ColorPoint(int x, int y, Color color)
    {
        this->x = x;
        this->y = y;
        this->color = color;
    }
};

class GiciskyESL : public PollingComponent, public display::DisplayBuffer, public ble_client::BLEClientNode, public esp32_ble_tracker::ESPBTDeviceListener
{
public:

    float get_setup_priority() const { return setup_priority::PROCESSOR; }
    display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
    uint32_t get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }
    int get_width_internal() { return this->width_; }
    int get_height_internal() { return this->height_; }

    void update() override;
    void dump_config() override;
    void setup() override;
    void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
    void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
    void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
    void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
    void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
    void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
    void set_img_uuid16(uint16_t uuid) { this->img_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
    void set_img_uuid32(uint32_t uuid) { this->img_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
    void set_img_uuid128(uint8_t *uuid) { this->img_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
    void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) override;
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_bt_connected(binary_sensor::BinarySensor *bt_connected) { bt_connected_ = bt_connected; } 

    bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

protected:
    void draw_absolute_pixel_internal(int x, int y, Color color) override;
    void add_color_point(ColorPoint point);
    void draw_image_to_esl(const std::vector<Color> &image);
    void clear_display_buffer();
    Color get_display_color(int x, int y);
    bool is_display_empty();
    void display_();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    bool connected_{false};
    unsigned long timer_{0};
    std::vector<Color> image_buffer_;
    std::vector<ColorPoint> display_list_;
    std::vector<Color> old_image_buffer_;
    Color background_color_{Color::BLACK};
    int16_t width_{16};  ///< Display width as modified by current rotation
    int16_t height_{16}; ///< Display height as modified by current rotation
    uint16_t x_low_{0};
    uint16_t y_low_{0};
    uint16_t x_high_{0};
    uint16_t y_high_{0};

    bool write_data(std::vector<uint8_t> &data);
    std::string to_hex_string(const std::vector<unsigned char> &data);

    text_sensor::TextSensor *version_{nullptr};
    binary_sensor::BinarySensor *bt_connected_{nullptr};
    espbt::ESPBTUUID service_uuid_;
    espbt::ESPBTUUID char_uuid_;
    espbt::ESPBTUUID img_uuid_;
    espbt::ClientState client_state_;

    uint8_t type_{0};
    float battery_{0};
    uint16_t tag_version_{0};
};


}  // namespace gicisky_esl
}  // namespace esphome
#endif