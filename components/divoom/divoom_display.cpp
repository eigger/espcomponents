#include "divoom_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <numeric>
#include <limits>

#ifdef USE_ESP32
namespace esphome {
namespace divoom {

static const char *const TAG = "divoom";

void DivoomDisplay::dump_config()
{
    LOG_DISPLAY("", "divoom", this);
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", this->width_, this->height_);
    ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
    ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
    ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->char_uuid_.to_string().c_str());
    LOG_UPDATE_INTERVAL(this);
}

void DivoomDisplay::update()
{
    this->sync_time_();
    this->do_update_();
    this->display_();
}

void DivoomDisplay::setup()
{
    this->initialize();
    width_shift_offset_ = -this->width_;
    image_buffer_.resize(this->width_ * this->height_);
    clear_display_buffer();
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
    ESP_LOGI(TAG, "Initaialize.");
}

void DivoomDisplay::loop()
{
}


void DivoomDisplay::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    switch (event) 
    {
    case ESP_GATTC_OPEN_EVT:
        connected_ = true;
        if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
        this->client_state_ = espbt::ClientState::ESTABLISHED;
        ESP_LOGW(TAG, "[%s] Connected successfully!", this->char_uuid_.to_string().c_str());
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connected_ = false;
        synced_time_ = false;
        old_image_buffer_.clear();
        if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
        ESP_LOGW(TAG, "[%s] Disconnected", this->char_uuid_.to_string().c_str());
        this->client_state_ = espbt::ClientState::IDLE;
        break;
    case ESP_GATTC_WRITE_CHAR_EVT: 
        if (param->write.status == 0)
        {
            break;
        }
        auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
        if (chr == nullptr) 
        {
            ESP_LOGW(TAG, "[%s] Characteristic not found.", this->char_uuid_.to_string().c_str());
            break;
        }
        if (param->write.handle == chr->handle)
        {
            ESP_LOGW(TAG, "[%s] Write error, status=%d", this->char_uuid_.to_string().c_str(), param->write.status);
        }
        break;
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

void DivoomDisplay::clear_display_buffer()
{
    display_list_.clear();
    this->x_high_ = 0;
    this->y_high_ = 0;
}

Color DivoomDisplay::get_display_color(int x, int y)
{
    for (ColorPoint point : display_list_)
    {
        if (point.x == x && point.y == y)
        {
            //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", point.x, point.y, point.color.r, point.color.g, point.color.b);
            return point.color;
        }
    }
    return background_color_;
}

bool DivoomDisplay::is_display_empty()
{
    for (ColorPoint point : display_list_)
    {
        if (point.color != background_color_) return false;
    }
    return true;
}

void DivoomDisplay::shift_image()
{
    int32_t offset = width_shift_offset_;
    if (this->x_high_ <= this->width_) offset = 0;
    for (int x = 0; x < this->width_; x++)
    {
        for (int y = 0; y < this->height_; y++)
        {
            uint32_t pos = (y * width_) + x;
            image_buffer_[pos] = get_display_color(x + offset, y);
        }
    }
    if (this->x_high_ > this->width_) width_shift_offset_++;
    if (width_shift_offset_ > this->x_high_ + 1) width_shift_offset_ = -this->width_;

}
void DivoomDisplay::display_()
{
    if (!connected_) return;
    if (is_display_empty()) width_shift_offset_ = -this->width_;
    shift_image();
    clear_display_buffer();
    if (image_buffer_.size() == old_image_buffer_.size())
    {
        if (std::equal(image_buffer_.begin(), image_buffer_.end(), old_image_buffer_.begin())) return;
    }
    old_image_buffer_ = image_buffer_;
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
    uint8_t bitwidth = ceil(log10(palette.size()) / log10(2));
    for (uint8_t index : palette_index_list)
    {
        pixel += (index << offset);
        offset += bitwidth;
        if (offset >= 8)
        {
            pixel_data.push_back(pixel);
            pixel = (index >> (bitwidth - (offset - 8)));
            offset = offset - 8;
        }
    }
    if (offset > 0)
    {
        pixel_data.push_back(pixel);
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
    add_color_point(ColorPoint(x, y, color));
    //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", x, y, color.r, color.g, color.b);
    if (this->x_high_ < x) this->x_high_ = x;
    if (this->y_high_ < y) this->y_high_ = y;
}

void DivoomDisplay::add_color_point(ColorPoint point)
{
    if (point.color == background_color_) return;
    for (int i = 0; i < display_list_.size(); i++)
    {
        if (display_list_[i].x == point.x && display_list_[i].y == point.y)
        {
            display_list_[i].color = point.color;
            return;
        }
    }
    display_list_.push_back(point);
}

bool DivoomDisplay::write_data(std::vector<uint8_t> &data)
{
    if (this->client_state_ != espbt::ClientState::ESTABLISHED)
    {
        ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->char_uuid_.to_string().c_str());
        return false;
    }
    auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
    if (chr == nullptr)
    {
        ESP_LOGW(TAG, "[%s] Characteristic not found.  State update can not be written.", this->char_uuid_.to_string().c_str());
        return false;
    }
    if (this->require_response_)
    {
        chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    } 
    else 
    {
        chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_NO_RSP);
    }
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
    return true;
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

bool DivoomDisplay::write_protocol(std::vector<uint8_t> &data)
{
    std::vector<uint8_t> buffer;
    std::vector<uint8_t> option;
    if (this->require_response_)
    {
        option.push_back(0x01);
        option.push_back(packet_number_ & 0xFF);
        option.push_back((packet_number_ >> 8) & 0xFF);
        option.push_back((packet_number_ >> 16) & 0xFF);
        option.push_back((packet_number_ >> 24) & 0xFF);
        if (packet_number_ >= std::numeric_limits<uint32_t>::max())
        {
            packet_number_ = 1;
        }
        else 
        {
            packet_number_++;
        }
    }
    else
    {
        option.push_back(0x00);
    }
    uint16_t length = data.size() + option.size() + 2; //data + checksum
    uint8_t length_low = length & 0xFF;
    uint8_t length_high = (length >> 8) & 0xFF;
    uint16_t checksum = std::accumulate(data.begin(), data.end(), 0);
    checksum += std::accumulate(option.begin(), option.end(), length_high + length_low);
    uint8_t checksum_low = checksum & 0xFF;
    uint8_t checksum_high = (checksum >> 8) & 0xFF; 

    buffer.push_back(0xFE);
    buffer.push_back(0xEF);
    buffer.push_back(0xAA);
    buffer.push_back(0x55);
    buffer.push_back(length_low);
    buffer.push_back(length_high);
    buffer.insert(buffer.end(), option.begin(), option.end());
    buffer.insert(buffer.end(), data.begin(), data.end());
    buffer.push_back(checksum_low);
    buffer.push_back(checksum_high);
    return write_data(buffer);
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

bool DivoomDisplay::set_divoom_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    std::vector<uint8_t> protocol;
    protocol.push_back(0x18);
    protocol.push_back(0x11);
    protocol.push_back(0x14);
    protocol.push_back(0x0B);
    protocol.push_back(0x1C);
    protocol.push_back(hours);
    protocol.push_back(minutes);
    protocol.push_back(seconds);
    protocol.push_back(0x05);
    return write_protocol(protocol);
}



void DivoomDisplay::sync_time_()
{
    if (synced_time_) return;
    if (!connected_) return;
    if (this->time_ == nullptr) return;
    auto time = this->time_->now();
    if (!time.is_valid())
    {
        ESP_LOGW(TAG, "[%s] Time is not yet valid.  Time can not be synced.", this->parent_->address_str().c_str());
        return;
    }
    time.recalc_timestamp_utc(true);  // calculate timestamp of local time
    synced_time_ = set_divoom_time(time.hour, time.minute, time.second);
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
#endif