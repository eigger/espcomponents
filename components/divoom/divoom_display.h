#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/time/real_time_clock.h"
#include "version.h"


#ifdef USE_ESP32
#include <esp_gattc_api.h>
namespace esphome {
namespace divoom {

namespace espbt = esphome::esp32_ble_tracker;
enum DivoomModel
{
    DITOO = 0,
    DIVOOM11,
};

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

class DivoomDisplay : public PollingComponent, public display::DisplayBuffer, public ble_client::BLEClientNode
{
public:
    void set_model(DivoomModel model) { this->model_ = model; }
    float get_setup_priority() const { return setup_priority::PROCESSOR; }
    display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
    uint32_t get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }
    int get_width_internal() { return this->width_; }
    int get_height_internal() { return this->height_; }

    virtual void initialize() = 0;

    void update() override;
    void dump_config() override;
    void setup() override;
    void loop() override;
    void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
    void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
    void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
    void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
    void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
    void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
    void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) override;
    void set_require_response(bool response) { this->require_response_ = response; }
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_bt_connected(binary_sensor::BinarySensor *bt_connected) { bt_connected_ = bt_connected; } 
    void set_select_time(select::Select *select_time) { select_time_ = select_time; }
    void select_time_callback(std::string value, size_t index);
    void set_brightness(number::Number *brightness) { brightness_ = brightness; }
    void set_divoom_brightness(uint8_t value);
    bool set_divoom_time(uint8_t hours, uint8_t minutes, uint8_t seconds);
    void brightness_callback(float value);
    void set_time(time::RealTimeClock *time) { this->time_ = time; };

protected:
    void draw_absolute_pixel_internal(int x, int y, Color color) override;
    void add_color_point(ColorPoint point);
    void draw_image_to_divoom(const std::vector<Color> &image);
    void draw_animation_to_divoom(const std::vector<std::vector<Color>> &images, uint16_t time);
    std::vector<uint8_t> get_single_image_data(const std::vector<Color> &image, uint16_t time = 0);
    void turn_divoom_into_clock(uint8_t type);
    void clear_display_buffer();
    Color get_display_color(int x, int y);
    bool is_display_empty();
    void shift_image();
    void display_();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    bool connected_{false};
    unsigned long timer_{0};
    DivoomModel model_;
    std::vector<Color> image_buffer_;
    std::vector<ColorPoint> display_list_;
    std::vector<Color> old_image_buffer_;
    Color background_color_{Color::BLACK};
    int32_t width_shift_offset_{0};
    uint32_t packet_number_{1}; 
    int16_t width_{16};  ///< Display width as modified by current rotation
    int16_t height_{16}; ///< Display height as modified by current rotation
    uint16_t x_low_{0};
    uint16_t y_low_{0};
    uint16_t x_high_{0};
    uint16_t y_high_{0};

    bool write_data(std::vector<uint8_t> &data);
    bool write_protocol(std::vector<uint8_t> &data);
    std::string to_hex_string(const std::vector<unsigned char> &data);

    text_sensor::TextSensor *version_{nullptr};
    binary_sensor::BinarySensor *bt_connected_{nullptr};
    select::Select *select_time_{nullptr};
    number::Number *brightness_{nullptr};

    bool require_response_;
    espbt::ESPBTUUID service_uuid_;
    espbt::ESPBTUUID char_uuid_;
    espbt::ClientState client_state_;

    void sync_time_();
    time::RealTimeClock *time_{nullptr};
    bool synced_time_{false};
};

class DivoomDitoo : public DivoomDisplay
{
public:
    void initialize() override;
};

class Divoom11x11 : public DivoomDisplay
{
public:
    void initialize() override;
};

class SelectTime : public select::Select
{
public:
    void control(const std::string &value)
    {
        this->state = value;
        this->publish_state(value);
    }
};

class Brightness : public number::Number
{
public:
    void control(float value)
    {
        this->state = value;
        this->publish_state(value);
    }
};

}  // namespace divoom
}  // namespace esphome
#endif