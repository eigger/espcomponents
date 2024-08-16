#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
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

class GiciskyESL : public display::DisplayBuffer, public ble_client::BLEClientNode
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
    void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) override;
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_bt_connected(binary_sensor::BinarySensor *bt_connected) { bt_connected_ = bt_connected; } 
    void set_update(switch_::Switch *update) { update_ = update; }
    void set_width(uint16_t width) { this->width_ = width; }
    void set_height(uint16_t height) { this->height_ = height; }
protected:
    void parse_data(uint8_t *data, uint16_t len);
    void draw_absolute_pixel_internal(int x, int y, Color color) override;
    void add_color_point(ColorPoint point);
    Color get_display_color(int x, int y);
    void display_();
    void clear_display_buffer();
    void shift_image();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    bool connected_{false};
    unsigned long timer_{0};

    std::vector<Color> image_buffer_;
    std::vector<uint8_t> image_packet_;
    std::vector<ColorPoint> display_list_;
    std::vector<Color> old_image_buffer_;
    Color background_color_{Color::BLACK};

    int16_t width_{0};
    int16_t height_{0};
    uint16_t x_low_{0};
    uint16_t y_low_{0};
    uint16_t x_high_{0};
    uint16_t y_high_{0};
    void send_img(uint32_t part);
    void send_cmd(uint8_t cmd);
    bool write_cmd(std::vector<uint8_t> &data);
    bool write_img(std::vector<uint8_t> &data);
    std::string to_hex_string(const std::vector<unsigned char> &data);
    std::string to_hex_string(const uint8_t* data, const uint16_t len);
    void update_callback(bool state);
    text_sensor::TextSensor *version_{nullptr};
    binary_sensor::BinarySensor *bt_connected_{nullptr};
    switch_::Switch *update_{nullptr};

    esp32_ble_tracker::ESPBTUUID service_uuid_ =
        esp32_ble_tracker::ESPBTUUID::from_uint16(0xFEF0);
    esp32_ble_tracker::ESPBTUUID cmd_uuid_ =
        esp32_ble_tracker::ESPBTUUID::from_uint16(0xFEF1);
    esp32_ble_tracker::ESPBTUUID img_uuid_ =
        esp32_ble_tracker::ESPBTUUID::from_uint16(0xFEF2);

    espbt::ClientState client_state_;
    uint16_t handle;

};

class Update : public switch_::Switch
{
public:
    void write_state(bool state) override
    {
        this->state = state;
        this->publish_state(state);
    }
};


}  // namespace gicisky_esl
}  // namespace esphome
#endif