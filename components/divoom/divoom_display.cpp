#include "divoom_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace divoom {

static const char *const TAG = "divoom";

void DivoomDisplay::dump_config()
{
    LOG_DISPLAY("", "divoom", this);
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", this->width_, this->height_);
    LOG_UPDATE_INTERVAL(this);
}

void DivoomDisplay::update()
{
    this->do_update_();
    this->display_();
}

void DivoomDisplay::setup()
{
    this->initialize();
    image_buffer_ = std::vector<Color>(width_ * height_, Color::BLACK);
    rx_parser_.set_checksum_len(2);
    rx_parser_.add_header(DIVOOM_HEADER);
    rx_parser_.add_footer(DIVOOM_FOOTER);
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_status_) this->bt_status_->publish_state(false);
    select_time_callback_ = select_time_callback;
    if (this->select_time_) this->select_time_->add_on_state_callback(select_time_callback_);
    serialbt_.begin("ESPHOME", true);
    connected_ = serialbt_.connect(address_);
    // if (!connected_) while (!serialbt_.connected(10000));
    // serialbt_.disconnect();
    // serialbt_.connect();
    disconnected_time_ = get_time();
    ESP_LOGI(TAG, "Initaialize.");
}

void DivoomDisplay::loop()
{
    connect_to_device();
    read_from_bluetooth();
}

void DivoomDisplay::set_address(uint64_t address)
{
    this->address_[0] = (address >> 40) & 0xFF;
    this->address_[1] = (address >> 32) & 0xFF;
    this->address_[2] = (address >> 24) & 0xFF;
    this->address_[3] = (address >> 16) & 0xFF;
    this->address_[4] = (address >> 8) & 0xFF;
    this->address_[5] = (address >> 0) & 0xFF;
}

void DivoomDisplay::connect_to_device()
{
    connected_ = serialbt_.connected(10000);
    if (connected_ != status_)
    {
        status_ = connected_;
        if (this->bt_status_) this->bt_status_->publish_state(status_);
    }
    if (connected_)
    {
        disconnected_time_ = get_time();
        return;
    }
    if (elapsed_time(disconnected_time_) > 10000)
    {
        serialbt_.disconnect();
        serialbt_.connect();
        disconnected_time_ = get_time();
        ESP_LOGI(TAG, "Retry connection");
    }
}

void DivoomDisplay::read_from_bluetooth()
{
    rx_parser_.clear();
    bool valid_data = false;
    unsigned long timer = get_time();
    if (connected_ == false) return;
    while (elapsed_time(timer) < 10)
    {
        while (!valid_data && this->serialbt_.available())
        {
            uint8_t byte = this->serialbt_.read();
            if (rx_parser_.parse_byte(byte)) valid_data = true;
            timer = get_time();
        }
        if (valid_data) break;
        delay(1);
    }
}

unsigned long DivoomDisplay::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long DivoomDisplay::get_time()
{
    return millis();
}

void DivoomDisplay::display_()
{
    if (image_buffer_.size() == old_image_buffer_.size())
    {
        if (std::equal(image_buffer_.begin(), image_buffer_.end(), old_image_buffer_.begin())) return;
    }
    old_image_buffer_ = image_buffer_;
    draw_image_to_divoom(image_buffer_);
}

void DivoomDisplay::draw_image_to_divoom(const std::vector<Color> &image)
{
    std::vector<Color> palette;
    std::vector<uint8_t> palette_index_list;
    std::vector<uint8_t> pixel_data;
    std::vector<uint8_t> palette_data;
    std::vector<uint8_t> protocol;
    for(Color color : image)
    {
        uint8_t index = palette.size();
        auto it = std::find(palette.begin(), palette.end(), color);
        if (it == palette.end())    palette.push_back(color);
        else                        index = it - palette.begin();
        palette_index_list.push_back(index);
    }

    int offset = 0;
    uint8_t pixel = 0x00;
    for (uint8_t index : palette_index_list)
    {
        pixel += (index << offset++);
        if (offset >= 8)
        {
            pixel_data.push_back(pixel);
            pixel = 0;
            offset = 0;
        }
    }

    for (Color color : palette)
    {
        palette_data.push_back(color.r);
        palette_data.push_back(color.g);
        palette_data.push_back(color.b);
    }
    uint16_t size = 7 + palette_data.size() + pixel_data.size();
    protocol.push_back(0x44);
    protocol.push_back(0x00);
    protocol.push_back(0x0A);
    protocol.push_back(0x0A);
    protocol.push_back(0x04);
    protocol.push_back(0xAA);
    protocol.push_back(size & 0xff);
    protocol.push_back((size >> 8) & 0xff);
    protocol.push_back(0x00);
    protocol.push_back(0x00);
    protocol.push_back(0x00);
    protocol.push_back(palette.size());
    protocol.insert(protocol.end(), palette_data.begin(), palette_data.end());
    protocol.insert(protocol.end(), pixel_data.begin(), pixel_data.end());
    write_protocol(protocol);
}

void HOT DivoomDisplay::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x >= this->get_width_internal() || x < 0) return;
    if (y >= this->get_height_internal() || y < 0) return;
    uint32_t pos = (y * width_) + x;
    image_buffer_[pos] = color;
    //ESP_LOGI(TAG, "pos%d r%d g%d b%d", pos, color.r, color.g, color.b);
}

void DivoomDisplay::write_data(const std::vector<uint8_t> &data)
{
    if (!connected_) return;
    this->serialbt_.write(&data[0], data.size());
    ESP_LOGV(TAG, "Write array-> %s", to_hex_string(data).c_str());
}

std::string DivoomDisplay::to_hex_string(const std::vector<unsigned char> &data)
{
    char buf[20];
    std::string res;
    for (uint16_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "0x%02X ", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", data.size());
    res += buf;
    return res;
}

void DivoomDisplay::write_protocol(const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> buffer;
    uint32_t checksum = 0;
    buffer.push_back(DIVOOM_HEADER);
    uint32_t length = data.size() + 2;
    buffer.push_back(length & 0xFF);
    buffer.push_back((length >> 8) & 0xFF);
    checksum += length & 0xFF;
    checksum += (length >> 8) & 0xFF;
    for (uint8_t temp : data)
    {
        buffer.push_back(temp);
        checksum += temp;
    }
    buffer.push_back(checksum & 0xFF);
    buffer.push_back((checksum >> 8) & 0xFF);
    buffer.push_back(DIVOOM_FOOTER);
    write_data(buffer);
}


// Full String: 450001 TT XX WW EE CC RRGGBB

// 450001: Fixed AFAIK
// TT: Type of clock

// 00: Full screen
// 01: Rainbow
// 02: With Box
// 03: Analog Square
// 04: Full Screen negative
// 05: Analog Round
// XX: Show Time: 00 to not display it, 01 to show it
// WW: Show Weather: 00 to not display it, 01 to show it
// EE: Show Temperature: 00 to not display it, 01 to show it
// CC: Show Calendar: 00 to not display it, 01 to show it
// RRGGBB: Color of the clock in Hex
void DivoomDisplay::turn_divoom_into_clock()
{
    std::vector<uint8_t> protocol;
    protocol.push_back(0x45);
    protocol.push_back(0x00);
    write_protocol(protocol);
}

void DivoomDisplay::select_time_callback(std::string value, size_t index)
{
    ESP_LOGI(TAG, "time Callback.%s", value);
}

void Divoom16x16::initialize()
{
    this->width_ = 16;
    this->height_ = 16;
}

void Divoom11x11::initialize()
{
    this->width_ = 11;
    this->height_ = 11;
}

} // namespace divoom
}  // namespace esphome
