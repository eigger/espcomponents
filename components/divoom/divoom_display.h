#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "BluetoothSerial.h"
#include "parser.h"
#include "divoom_defines.h"

namespace esphome {
namespace divoom {

enum DivoomModel
{
    DIVOOM16 = 0,
    DIVOOM11,
};

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
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_bt_status(binary_sensor::BinarySensor *bt_status) { bt_status_ = bt_status; } 
protected:
    void draw_absolute_pixel_internal(int x, int y, Color color) override;
    void draw_image_to_divoom(const std::vector<Color> &image);
    void display_();
    void connect_to_device();
    void read_from_bluetooth();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    BluetoothSerial serialbt_;
    bool connected_{false};
    bool status_{false};
    unsigned long disconnected_time_{0};
    Parser rx_parser_{};
    DivoomModel model_;
    std::vector<Color> image_buffer_;
    uint8_t address_[6];
    int16_t width_{16};  ///< Display width as modified by current rotation
    int16_t height_{16}; ///< Display height as modified by current rotation
    uint16_t x_low_{0};
    uint16_t y_low_{0};
    uint16_t x_high_{0};
    uint16_t y_high_{0};

    void write_data(const std::vector<uint8_t> &data);
    void write_protocol(const std::vector<uint8_t> &data);
    std::string to_hex_string(const std::vector<unsigned char> &data);

    text_sensor::TextSensor *version_{nullptr};
    binary_sensor::BinarySensor *bt_status_{nullptr};
};

class Divoom16x16 : public DivoomDisplay
{
public:
    void initialize() override;
};

class Divoom11x11 : public DivoomDisplay
{
public:
    void initialize() override;
};

}  // namespace divoom
}  // namespace esphome
