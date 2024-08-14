#include "gicisky_esl.h"
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
    // ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", this->width_, this->height_);
    // ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
    // ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
    // ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->char_uuid_.to_string().c_str());
    // LOG_UPDATE_INTERVAL(this);
}

void GiciskyESL::update()
{
    this->do_update_();
    this->display_();
}

void GiciskyESL::setup()
{
    image_buffer_.resize(this->width_ * this->height_);
    clear_display_buffer();
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_connected_) this->bt_connected_->publish_state(false);
    timer_ = get_time();
    ESP_LOGI(TAG, "Initaialize.");
}

// bool GiciskyESL::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
//     // the address should match the address we declared

//     // [15:32:06][D][ble_adv:021]: New BLE device
//     // [15:32:06][D][ble_adv:022]:   address: FF:FF:61:62:43:05
//     // [15:32:06][D][ble_adv:023]:   name: NEMR61624305
//     // [15:32:06][D][ble_adv:024]:   Advertised service UUIDs:
//     // [15:32:06][D][ble_adv:026]:     - 0xFEF0
//     // [15:32:06][D][ble_adv:029]:   Advertised service data:
//     // [15:32:06][D][ble_adv:041]:   Advertised manufacturer data:
//     // [15:32:06][D][ble_adv:049]:     - 0x5053: (length 5) A01D810140
//     // A0 1D 81 01 40
//     // Type, batteryMv * 100, tagSoftwareVersion = 81 << 8 | 01, 

//     if (device.address_uint64() != this->address_) {
//         ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
//         return false;
//     }
//     auto mnf_datas = device.get_manufacturer_datas();
//     if (mnf_datas.size() != 1) {
//         ESP_LOGD(TAG, "parse_device(): manufacturer_datas is expected to have a single element - size(%d)", mnf_datas.size());
//         return false;
//     }
//     auto mnf_data = mnf_datas[0];
//     if (mnf_data.data.size() != 5) {
//         ESP_LOGD(TAG, "parse_device(): manufacturer_data error - size(%d)", mnf_data.data.size());
//         return false;
//     }
//     uint8_t type = mnf_data.data[0];
//     float battery = mnf_data.data[1] * 100;
//     uint16_t tag_version = (mnf_data.data[2] << 8) + mnf_data.data[3];

//     type_ = type;
//     battery_ = battery;
//     tag_version_ = tag_version;
//     return true;
// }

void GiciskyESL::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
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

void GiciskyESL::display_()
{
    if (!connected_) return;
    if (type_ == 0) return;
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

bool GiciskyESL::write_data(std::vector<uint8_t> &data)
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
    // if (this->require_response_)
    // {
    //     chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    // } 
    // else 
    // {
    //     chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_NO_RSP);
    // }
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


} // namespace gicisky_esl
}  // namespace esphome
#endif