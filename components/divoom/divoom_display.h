#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "BluetoothSerial.h"
#include "parser.h"
#include "divoom_defines.h"

namespace esphome {
namespace divoom {

enum DivoomModel
{
    DITOO = 0,
    DIVOOM11,
};

enum BTStatus
{
    BT_INIT = 0,
    BT_DISCOVERY,
    BT_CONNECTING,
    BT_CONNECTED
};

struct ColorMap
{
    uint32_t x;
    uint32_t y;
    Color color;
    ColorMap(uint32_t x, uint32_y, Color color)
    {
        this->x = x;
        this->y = y;
        this->color = color;
    }
}

class DivoomDisplay : public PollingComponent, public display::DisplayBuffer
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
    void set_address(uint64_t address);
    void set_address_str(std::string address);
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_bt_connected(binary_sensor::BinarySensor *bt_connected) { bt_connected_ = bt_connected; } 
    void set_select_time(select::Select *select_time) { select_time_ = select_time; }
    void select_time_callback(std::string value, size_t index);
    void set_brightness(number::Number *brightness) { brightness_ = brightness; }
    void set_divoom_brightness(uint8_t value);
    void brightness_callback(float value);
protected:
    void draw_absolute_pixel_internal(int x, int y, Color color) override;
    void draw_image_to_divoom(const std::vector<Color> &image);
    void draw_animation_to_divoom(const std::vector<std::vector<Color>> &images, uint16_t time);
    std::vector<uint8_t> get_single_image_data(const std::vector<Color> &image, uint16_t time = 0);
    void turn_divoom_into_clock(uint8_t type);
    void clear_display_buffer();
    void shift_image();
    void display_();
    void connect_to_device();
    bool found_divoom();
    void read_from_bluetooth();
    void write_to_bluetooth();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    BluetoothSerial serialbt_;
    BTStatus bt_status_{BT_INIT};
    bool connected_{false};
    unsigned long timer_{0};
    Parser rx_parser_{};
    DivoomModel model_;
    std::vector<Color> image_buffer_;
    std::vector<Color> old_image_buffer_;
    std::vector<std::vector<Color>> animation_buffer_;
    std::unordered_map<<uint32_t, ColorMap> display_map_;
    int32_t width_shift_offset_{0};
    uint8_t address_[6];
    optional<std::string> address_str_{};
    int16_t width_{16};  ///< Display width as modified by current rotation
    int16_t height_{16}; ///< Display height as modified by current rotation
    uint16_t x_low_{0};
    uint16_t y_low_{0};
    uint16_t x_high_{0};
    uint16_t y_high_{0};

    void write_data(const std::vector<uint8_t> &data);
    void write_protocol(const std::vector<uint8_t> &data);
    void send_protocol(const std::vector<uint8_t> &data);
    std::string to_hex_string(const std::vector<unsigned char> &data);

    std::queue<std::vector<uint8_t>> protocol_queue_;

    text_sensor::TextSensor *version_{nullptr};
    binary_sensor::BinarySensor *bt_connected_{nullptr};
    select::Select *select_time_{nullptr};
    number::Number *brightness_{nullptr};
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
