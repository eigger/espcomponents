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
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d,  Rotation: %d", this->width_, this->height_, this->rotation_);
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
    rx_parser_.set_checksum_len(2);
    rx_parser_.add_header(DIVOOM_HEADER);
    rx_parser_.add_footer(DIVOOM_FOOTER);
    // if (this->error_) this->error_->publish_state("None");
    // if (this->version_) this->version_->publish_state(BLUETOOTHEX_VERSION);
    serialbt_.begin("ESPHOME", true);
    connected_ = serialbt_.connect(address_);
    if (!connected_) while (!serialbt_.connected(10000));
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

void DivoomDisplay::display_()
{
    // we will only update the changed window to the display
    uint16_t w = this->x_high_ - this->x_low_ + 1;
    uint16_t h = this->y_high_ - this->y_low_ + 1;

    // invalidate watermarks
    this->x_low_ = this->width_;
    this->y_low_ = this->height_;
    this->x_high_ = 0;
    this->y_high_ = 0;

    // create palette and pixel array
    std::vector<uint8_t> pixels;
    std::vector<Color> palette;
    for (int y = 0; y < this->height_; y++)
    {
        for (int x = 0; x < this->width_; x++)
        {
            Color color = image_buffer_[x][y];
            auto it = std::find(palette.begin(), palette.end(), color);
            if (it == palette.end())
            {
                palette.push_back(color);
                pixels.push_back(palette.size() - 1);
            }
            else
            {
                pixels.push_back(std::distance(palette.begin(), it));
            }
        }
    }

    // encode pixels
    int bitwidth = ceil(log2(palette.size()));
    int nbytes = ceil((256 * bitwidth) / 8.);
    std::vector<uint8_t> encoded_pixels;
    std::string encoded_byte;
    int idx = 0;
    uint8_t pixel = 0x00;
    std::vector<uint8_t> encoded_data;
    for (uint8_t pixel_idx : pixels)
    {
        pixel += (pixel_idx << idx);
        idx += 2;
        if (idx >= 8)
        {
            encoded_data.push_back(pixel);
            pixel = 0;
            idx = 0;
        }
    }

    // encode palette
    std::vector<uint8_t> encoded_palette;
    for (Color color : palette)
    {
        encoded_palette.push_back(color.r);
        encoded_palette.push_back(color.g);
        encoded_palette.push_back(color.b);
    }

    std::vector<uint8_t> protocol;
    uint16_t frame_size = 7 + encoded_palette.size() + encoded_data.size();
    protocol.push_back(0x44);
    protocol.push_back(0x00);
    protocol.push_back(0x0A);
    protocol.push_back(0x0A);
    protocol.push_back(0x04);

    protocol.push_back(0xAA);
    protocol.push_back(frame_size & 0xff);
    protocol.push_back((frame_size >> 8) & 0xff);
    protocol.push_back(0x00);
    protocol.push_back(0x00);
    protocol.push_back(0x00);
    protocol.push_back(encoded_palette.size());

    for (uint8_t data : encoded_palette)
    {
        protocol.push_back(data);
    }

    for (uint8_t data : encoded_data)
    {
        protocol.push_back(data);
    }
    write_protocol(protocol);
}

void DivoomDisplay::fill(Color color)
{
    // for (int w = 0; w < width_; w++)
    // {
    //     for (int h = 0; h < height_; h++)
    //     {
    //         image_buffer_[w][h] = color;
    //     }
    // }
}

void HOT DivoomDisplay::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
        return;

    // low and high watermark may speed up drawing from buffer
    this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
    this->y_low_ = (y < this->y_low_) ? y : this->y_low_;
    this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
    this->y_high_ = (y > this->y_high_) ? y : this->y_high_;

    uint32_t pos = (y * width_) + x;
    image_buffer_[x][y] = color;
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

void Divoom16x16::initialize()
{
    this->width_ = 16;
    this->height_ = 16;
    for (int w = 0; w < width_; w++)
    {
        for (int h = 0; h < height_; h++)
        {
            image_buffer_[w][h] = Color::BLACK;
        }
    }
}

void Divoom11x11::initialize()
{
    this->width_ = 11;
    this->height_ = 11;
    for (int w = 0; w < width_; w++)
    {
        for (int h = 0; h < height_; h++)
        {
            image_buffer_[w][h] = Color::BLACK;
        }
    }
}

} // namespace divoom
}  // namespace esphome
