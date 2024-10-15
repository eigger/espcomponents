#include "bmm150.h"
#include "esphome/core/log.h"
#include "esp_sleep.h"

namespace esphome {
namespace bmm150 {

static const char *TAG = "bmm150";

void BMM150Component::setup() 
{
    int8_t code = bmm150_initialization();
    if (code != BMM150_OK)
    {
        ESP_LOGE(TAG, "Init Error %d", code);
    }
}

void BMM150Component::dump_config()
{
    ESP_LOGCONFIG(TAG, "BMM150:");
    LOG_I2C_DEVICE(this);
}

float BMM150Component::get_setup_priority() const { return setup_priority::DATA; }

void BMM150Component::update()
{
    bmm150_read_mag_data(&dev_);
    if (this->mag_x_ != nullptr) this->mag_x_->publish_state(dev_.data.x);
    if (this->mag_y_ != nullptr) this->mag_y_->publish_state(dev_.data.y);
    if (this->mag_z_ != nullptr) this->mag_z_->publish_state(dev_.data.z);
}

int8_t BMM150Component::bmm150_initialization()
{
    int8_t rslt = BMM150_OK;
    dev_.intf = BMM150_I2C_INTF; //SPI or I2C interface setup.
    auto read = std::bind(&BMM150Component::i2c_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    auto write = std::bind(&BMM150Component::i2c_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    dev_.read = read;    //Read the bus pointer.
    dev_.write = write;  //Write the bus pointer.
    dev_.delay_ms = delay;

    // Set the maximum range range
    mag_max_.x = -2000;
    mag_max_.y = -2000;
    mag_max_.z = -2000;

    // Set the minimum range
    mag_min_.x = 2000;
    mag_min_.y = 2000;
    mag_min_.z = 2000;

    rslt = bmm150_init(&dev_);   //Memory chip ID.
    dev_.settings.pwr_mode = BMM150_NORMAL_MODE;
    rslt |= bmm150_set_op_mode(&dev_);   //Set the sensor power mode.
    dev_.settings.preset_mode = BMM150_PRESETMODE_ENHANCED;
    rslt |= bmm150_set_presetmode(&dev_);    //Set the preset mode of
    return rslt;
}

int8_t BMM150Component::i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
    this->read_bytes(reg_addr, read_data, len);
}

int8_t BMM150Component::i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
    this->write_bytes(reg_addr, read_data, len);
}

}
}
