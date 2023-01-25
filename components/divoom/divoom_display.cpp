#include "divoom_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace divoom {

static const char *const TAG = "divoom";

void DivoomDisplay::dump_config() {
    LOG_DISPLAY("", "divoom", this);
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d,  Rotation: %d", this->width_, this->height_, this->rotation_);
    LOG_UPDATE_INTERVAL(this);
}

void DivoomDisplay::update() {
    this->do_update_();
    this->display_();
}

void DivoomDisplay::setup()
{
    if (model_ == DivoomModel::DIVOOM11)
    {
        this->width_ = 11;
        this->height_ = 11;
    }
    else
    {
        this->width_ = 16;
        this->height_ = 16;
    }
    buffer_ = std::vector<Color>(this->width_ * this->height_, Color::BLACK);
    rx_parser_.set_checksum_len(2);
    rx_parser_.add_header(DIVOOM_HEADER);
    rx_parser_.add_footer(DIVOOM_FOOTER);
    //if (this->error_) this->error_->publish_state("None");
    //if (this->version_) this->version_->publish_state(BLUETOOTHEX_VERSION);
    serialbt_.begin("ESPHOME", true);
    connected_ = serialbt_.connect(address_);
    if(!connected_) while(!serialbt_.connected(10000));
    serialbt_.disconnect();
    serialbt_.connect();
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

void DivoomDisplay::display_() {
    // we will only update the changed window to the display
    uint16_t w = this->x_high_ - this->x_low_ + 1;
    uint16_t h = this->y_high_ - this->y_low_ + 1;

    // invalidate watermarks
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;
    
    //if (std::equal(buffer_.begin(), buffer_.end(), old_buffer_.begin())) return;
    old_buffer_ = buffer_;
    std::vector<uint8_t> protocol = {0x44, 0x00, 0x0A, 0x0A, 0x04};
    bool first = true;
    for(Color color : buffer_)
    {
        uint8_t r = (color.r * 0x0F) / 0xFF;
        uint8_t g = (color.g * 0x0F) / 0xFF;
        uint8_t b = (color.b * 0x0F) / 0xFF;
        if (first)
        {
            first = false;
            protocol.push_back(0x00);
            protocol.push_back(0x00);
            protocol[protocol.size() - 2] = r;
            protocol[protocol.size() - 2] += (g << 0x0F);
            protocol[protocol.size() - 1] = b;
            //G[0]R[0] R[1]B[0] B[1]G[1]
        }
        else
        {
            first = true;
            protocol.push_back(0x00);
            protocol[protocol.size() - 1] = g;
            protocol[protocol.size() - 1] += (b << 0x0F);
            protocol[protocol.size() - 2] += (r << 0x0F);
        }
    }
}

void DivoomDisplay::fill(Color color) {

}

void HOT DivoomDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  // low and high watermark may speed up drawing from buffer
  this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
  this->y_low_ = (y < this->y_low_) ? y : this->y_low_;
  this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
  this->y_high_ = (y > this->y_high_) ? y : this->y_high_;

  uint32_t pos = (y * width_) + x;
  buffer_[pos] = color;
}

// should return the total size: return this->get_width_internal() * this->get_height_internal() * 2 // 16bit color
// values per bit is huge
uint32_t DivoomDisplay::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }

int DivoomDisplay::get_width_internal() { return this->width_; }
int DivoomDisplay::get_height_internal() { return this->height_; }

void DivoomDisplay::write_data(const std::vector<uint8_t> &data)
{
    if (!connected_) return;
    this->serialbt_.write(&data[0], data.size());
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
    for(uint8_t temp : data)
    {
        buffer.push_back(temp);
        checksum += temp;
    }
    buffer.push_back(checksum & 0xFF);
    buffer.push_back((checksum >> 8) & 0xFF);
    buffer.push_back(DIVOOM_FOOTER);
    write_data(buffer);
}

}  // namespace divoom
}  // namespace esphome
