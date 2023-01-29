#include "divoom_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <numeric>

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
    //width_shift_offset_ = -this->width_;
    clear_display_buffer();
    rx_parser_.set_checksum_len(2);
    rx_parser_.add_header(DIVOOM_HEADER);
    rx_parser_.add_footer(DIVOOM_FOOTER);
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_connected_) this->bt_connected_->publish_state(false);
    if (this->select_time_) 
    {
        this->select_time_->add_on_state_callback(std::bind(&DivoomDisplay::select_time_callback, this, std::placeholders::_1, std::placeholders::_2));
        this->select_time_->publish_state(this->select_time_->at(0).value());
    }
    if (this->brightness_)
    {
        this->brightness_->add_on_state_callback(std::bind(&DivoomDisplay::brightness_callback, this, std::placeholders::_1));
        this->brightness_->publish_state(100);
    }
    serialbt_.begin("ESPHOME", true);
    ESP_LOGI(TAG, "Initaialize.");
}

void DivoomDisplay::loop()
{
    connect_to_device();
    read_from_bluetooth();
    write_to_bluetooth();
}

void DivoomDisplay::set_address(uint64_t address)
{
    this->address_[0] = (address >> 40) & 0xFF;
    this->address_[1] = (address >> 32) & 0xFF;
    this->address_[2] = (address >> 24) & 0xFF;
    this->address_[3] = (address >> 16) & 0xFF;
    this->address_[4] = (address >> 8) & 0xFF;
    this->address_[5] = (address >> 0) & 0xFF;

    this->address_str_ = "";
    for (int i = 0; i < 6; i++)
    {
        char buf[20];
        sprintf(buf, "%02x", this->address_[i]);
        if (i > 0) this->address_str_ += ":";
        this->address_str_ += buf;
    }
}

void DivoomDisplay::connect_to_device()
{
    switch(bt_status_)
    {
    case BT_INIT:
        timer_ = get_time();
        connected_ = false;
        serialbt_.discoverAsync(nullptr);
        bt_status_ = BT_DISCOVERY;
        ESP_LOGI(TAG, "BT_INIT -> DISCOVERY");
        break;
    case BT_DISCOVERY:
        if (elapsed_time(timer_) < 10000) break;
        if (found_divoom() == false)
        {
            ESP_LOGI(TAG, "BT_DISCOVERY -> INIT");
            bt_status_ = BT_INIT;
            break;
        }
        ESP_LOGI(TAG, "BT_DISCOVERY -> CONNECTING");
        bt_status_ = BT_CONNECTING;
        serialbt_.disconnect();
        serialbt_.connect(address_);
        timer_ = get_time();
        break;
    case BT_CONNECTING:
        if (elapsed_time(timer_) > 10000)
        {
            ESP_LOGI(TAG, "BT_CONNECTING -> INIT");
            bt_status_ = BT_INIT;
            break;
        }
        if (serialbt_.connected())
        {
            connected_ = true;
            if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
            ESP_LOGI(TAG, "BT_CONNECTING -> CONNECTED");
            bt_status_ = BT_CONNECTED;
            break;
        }
        break;
    case BT_CONNECTED:
        connected_ = serialbt_.connected();
        if (connected_)
        {
            timer_ = get_time();
            break;
        }
        if (elapsed_time(timer_) > 5000)
        {
            serialbt_.disconnect();
            if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
            ESP_LOGI(TAG, "BT_CONNECTED -> INIT");
            bt_status_ = BT_INIT;
            break;
        }
        break;
    }
}

bool DivoomDisplay::found_divoom()
{
    serialbt_.discoverAsyncStop();
    BTScanResults* scan_result = serialbt_.getScanResults();
    if (scan_result->getCount() == 0) return false;
    for (int i = 0; i < scan_result->getCount(); i++)
    {
        BTAdvertisedDevice *device = scan_result->getDevice(i);
        ESP_LOGI(TAG, "----- %s  %s %d", device->getAddress().toString().c_str(), device->getName().c_str(), device->getRSSI());
        if (address_str_ == device->getAddress().toString() && device->getName().size() > 0) return true;
    }
    return false;
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

void DivoomDisplay::write_to_bluetooth()
{
    if (connected_ == false) return;
}

unsigned long DivoomDisplay::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long DivoomDisplay::get_time()
{
    return millis();
}

void DivoomDisplay::clear_display_buffer()
{
    display_list_.clear();
    this->x_high_ = 0;
    this->y_high_ = 0;
}

void DivoomDisplay::shift_image()
{
    int32_t offset = width_shift_offset_;
    image_buffer_ = std::vector<Color>(this->width_ * this->height_, Color::BLACK);
    //if (this->x_high_ <= this->width_) offset = 0;
    offset = 0;
    for (ColorPoint point : display_list_)
    {
        int x = point.x + offset;
        uint32_t pos = (point.y * width_) + x;
        if (x >= 0 && x < width_ && pos >= 0 && pos < width_ * height_)
        {
            image_buffer_[pos] = point.color;
        }
    }
    if (this->x_high_ > this->width_) width_shift_offset_++;
    if (width_shift_offset_ > this->x_high_ + 1) width_shift_offset_ = -this->width_;

}
void DivoomDisplay::display_()
{
    if (!connected_) return;
    shift_image();
    clear_display_buffer();
    // if (image_buffer_.size() == old_image_buffer_.size())
    // {
    //     if (std::equal(image_buffer_.begin(), image_buffer_.end(), old_image_buffer_.begin())) return;
    // }
    // old_image_buffer_ = image_buffer_;
    draw_image_to_divoom(image_buffer_);
}

void DivoomDisplay::draw_image_to_divoom(const std::vector<Color> &image)
{
    std::vector<uint8_t> protocol;
    std::vector<uint8_t> image_data = get_single_image_data(image);
    protocol.push_back(0x44);
    protocol.push_back(0x00);
    protocol.push_back(0x0A);
    protocol.push_back(0x0A);
    protocol.push_back(0x04);
    protocol.insert(protocol.end(), image_data.begin(), image_data.end());
    write_protocol(protocol);
}

void DivoomDisplay::draw_animation_to_divoom(const std::vector<std::vector<Color>> &images, uint16_t time)
{
    std::vector<uint8_t> protocol;
    std::vector<uint8_t> image_data;
    uint16_t delay_time = 0;
    for(std::vector<Color> image : images)
    {
        std::vector<uint8_t> data = get_single_image_data(image, delay_time);
        image_data.insert(image_data.end(), data.begin(), data.end());
        delay_time += time;
    }
    
    uint32_t size = image_data.size();
    protocol.push_back(0x49);
    protocol.push_back(size & 0xff);
    protocol.push_back((size >> 8) & 0xff);
    protocol.push_back(0x00);
    protocol.push_back(0x00);
    protocol.insert(protocol.end(), image_data.begin(), image_data.end());
    write_protocol(protocol);
}

std::vector<uint8_t> DivoomDisplay::get_single_image_data(const std::vector<Color> &image, uint16_t time)
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
    protocol.push_back(0xAA);
    protocol.push_back(size & 0xff);
    protocol.push_back((size >> 8) & 0xff);
    protocol.push_back(0x00);
    protocol.push_back(time & 0xff);
    protocol.push_back((time >> 8) & 0xff);
    protocol.push_back(palette.size());
    protocol.insert(protocol.end(), palette_data.begin(), palette_data.end());
    protocol.insert(protocol.end(), pixel_data.begin(), pixel_data.end());
    return protocol;
}

void HOT DivoomDisplay::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x < 0) return;
    //if (x >= this->get_width_internal()) return;
    if (y >= this->get_height_internal() || y < 0) return;
    //display_list_.push_back(ColorPoint(x, y, color));
    if (this->x_high_ < x) this->x_high_ = x;
    if (this->y_high_ < y) this->y_high_ = y;
}

void DivoomDisplay::write_data(const std::vector<uint8_t> &data)
{
    if (!connected_) return;
    this->serialbt_.write(&data[0], data.size());
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
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
    uint32_t length = data.size() + 2;
    uint8_t length_low = length & 0xFF;
    uint8_t length_high = (length >> 8) & 0xFF;
    uint32_t checksum = std::accumulate(data.begin(), data.end(), length_high + length_low);
    uint8_t checksum_low = checksum & 0xFF;
    uint8_t checksum_high = (checksum >> 8) & 0xFF; 
    buffer.push_back(DIVOOM_HEADER);
    buffer.push_back(length_low);
    buffer.push_back(length_high);
    buffer.insert(buffer.end(), data.begin(), data.end());
    buffer.push_back(checksum_low);
    buffer.push_back(checksum_high);
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
void DivoomDisplay::turn_divoom_into_clock(uint8_t type)
{
    std::vector<uint8_t> protocol;
    protocol.push_back(0x45);
    protocol.push_back(0x00);
    protocol.push_back(0x01);   //0: 12h, 1: 24h
    protocol.push_back(type);
    protocol.push_back(0x01);   //Time
    protocol.push_back(0x00);   //Temp
    protocol.push_back(0x00);   //Cal
    protocol.push_back(0x00);   //Img
    protocol.push_back(0xFF);   //R
    protocol.push_back(0xFF);   //G
    protocol.push_back(0xFF);   //B
    write_protocol(protocol);
}

void DivoomDisplay::select_time_callback(std::string value, size_t index)
{
    turn_divoom_into_clock(index);
}

void DivoomDisplay::set_divoom_brightness(uint8_t value)
{
    std::vector<uint8_t> protocol;
    protocol.push_back(0x74);
    protocol.push_back(value);
    write_protocol(protocol);
}

void DivoomDisplay::brightness_callback(float value)
{
    set_divoom_brightness((uint8_t)value);
}

void DivoomDitoo::initialize()
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
