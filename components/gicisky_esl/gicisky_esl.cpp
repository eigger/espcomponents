#include "divoom_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <numeric>
#include <limits>

#ifdef USE_ESP32
namespace esphome {
namespace gicisky_esl {

static const char *const TAG = "gicisky_esl";

void GiciskyESL::dump_config()
{
    LOG_DISPLAY("", "gicisky_esl", this);
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", this->width_, this->height_);
    ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
    ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
    ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->cmd_uuid_.to_string().c_str());
    LOG_UPDATE_INTERVAL(this);
}

void GiciskyESL::update()
{
    this->do_update_();
    this->display_();
}

void GiciskyESL::setup()
{
    width_shift_offset_ = -this->width_;
    image_buffer_.resize(this->width_ * this->height_);
    clear_display_buffer();
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_connected_) this->bt_connected_->publish_state(false);
    timer_ = get_time();
    ESP_LOGI(TAG, "Initaialize.");
}


void GiciskyESL::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event)
    {
        case ESP_GATTC_OPEN_EVT: 
        {
            if (param->open.status == ESP_GATT_OK) 
            {
                connected_ = true;
                if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
                ESP_LOGI(TAG, "[%s] Connected successfully!", this->get_name().c_str());
                break;
            }
            break;
        }
        case ESP_GATTC_CLOSE_EVT: 
        {
            this->status_set_warning();
            connected_ = false;
            if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT: 
        {
            this->handle = 0;
            auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->cmd_uuid_);
            if (chr == nullptr) 
            {
                this->status_set_warning();
                ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                        this->cmd_uuid_.to_string().c_str());
                break;
            }
            this->handle = chr->handle;
            auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                            this->parent()->get_remote_bda(), chr->handle);
            if (status) {
                ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
            }
            break;
        }
        case ESP_GATTC_READ_CHAR_EVT: 
        {
            if (param->read.handle == this->handle) 
            {
                if (param->read.status != ESP_GATT_OK) {
                    ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
                    break;
                }
                this->status_clear_warning();
                this->parse_data(param->read.value, param->read.value_len);
            }
            break;
        }
        case ESP_GATTC_NOTIFY_EVT: 
        {
            if (param->notify.handle != this->handle)
                break;
            ESP_LOGV(TAG, "[%s] ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", this->get_name().c_str(),
                    param->notify.handle, param->notify.value[0]);
            this->parse_data(param->notify.value, param->notify.value_len);
            break;
        }
        case ESP_GATTC_REG_FOR_NOTIFY_EVT: 
        {
            if (param->reg_for_notify.status == ESP_GATT_OK && param->reg_for_notify.handle == this->handle)
                this->node_state = espbt::ClientState::ESTABLISHED;
            break;
        }
        default:
        break;
    }
}

void GiciskyESL::parse_data(uint8_t *value, uint16_t value_len)
{
    
}

unsigned long GiciskyESL::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long GiciskyESL::get_time()
{
    return millis();
}

void GiciskyESL::clear_display_buffer()
{
    display_list_.clear();
    this->x_high_ = 0;
    this->y_high_ = 0;
}

Color GiciskyESL::get_display_color(int x, int y)
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

bool GiciskyESL::is_display_empty()
{
    for (ColorPoint point : display_list_)
    {
        if (point.color != background_color_) return false;
    }
    return true;
}

void GiciskyESL::shift_image()
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
void GiciskyESL::display_()
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
    draw_image_to_esl(image_buffer_);
}

void GiciskyESL::draw_image_to_esl(const std::vector<Color> &image)
{
    this->parent()->connect();
    delay(500);
    this->write_cmd({0x01});



    //this->parent()->disconnect();
}


void HOT GiciskyESL::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x < 0) return;
    //if (x >= this->get_width_internal()) return;
    if (y >= this->get_height_internal() || y < 0) return;
    add_color_point(ColorPoint(x, y, color));
    //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", x, y, color.r, color.g, color.b);
    if (this->x_high_ < x) this->x_high_ = x;
    if (this->y_high_ < y) this->y_high_ = y;
}

void GiciskyESL::add_color_point(ColorPoint point)
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

bool GiciskyESL::write_cmd(std::vector<uint8_t> &data)
{
    if (this->client_state_ != espbt::ClientState::ESTABLISHED)
    {
        ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->cmd_uuid_.to_string().c_str());
        return false;
    }
    auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->cmd_uuid_);
    if (chr == nullptr)
    {
        ESP_LOGW(TAG, "[%s] Characteristic not found.  State update can not be written.", this->cmd_uuid_.to_string().c_str());
        return false;
    }
    chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
    return true;
}


bool GiciskyESL::write_img(std::vector<uint8_t> &data)
{
    if (this->client_state_ != espbt::ClientState::ESTABLISHED)
    {
        ESP_LOGW(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->img_uuid_.to_string().c_str());
        return false;
    }
    auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->img_uuid_);
    if (chr == nullptr)
    {
        ESP_LOGW(TAG, "[%s] Characteristic not found.  State update can not be written.", this->img_uuid_.to_string().c_str());
        return false;
    }
    chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
    return true;
}

std::string GiciskyESL::to_hex_string(const std::vector<unsigned char> &data)
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

bool GiciskyESL::write_protocol(std::vector<uint8_t> &data)
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




} // namespace gicisky_esl
}  // namespace esphome
#endif