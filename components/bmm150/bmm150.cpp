#include "bmm150.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace bmm150 {

static const char *TAG = "bmm150";


int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
void delay_us(uint32_t period_us, void *intf_ptr);

void BMM150Component::setup() 
{
    mag_data_.x = NAN;
    mag_data_.y = NAN;
    mag_data_.z = NAN;
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
    int8_t code = bmm150_read_mag_data(&mag_data_, &dev_);
    if (code == BMM150_OK)
    {
        if (this->mag_x_ != nullptr) this->mag_x_->publish_state(mag_data_.x);
        if (this->mag_y_ != nullptr) this->mag_y_->publish_state(mag_data_.y);
        if (this->mag_z_ != nullptr) this->mag_z_->publish_state(mag_data_.z);
    }
    else
    {
        ESP_LOGE(TAG, "Update Error %d", code);
    }
}

int8_t BMM150Component::bmm150_initialization()
{
    int8_t rslt = BMM150_OK;
    dev_.intf = BMM150_I2C_INTF; //SPI or I2C interface setup.
    dev_.read = reg_read;    //Read the bus pointer.
    dev_.write = reg_write;  //Write the bus pointer.
    dev_.delay_us = delay_us;
    dev_.intf_ptr = this;

    // Set the maximum range range
    mag_max_.x = -2000;
    mag_max_.y = -2000;
    mag_max_.z = -2000;

    // Set the minimum range
    mag_min_.x = 2000;
    mag_min_.y = 2000;
    mag_min_.z = 2000;

    rslt = bmm150_init(&dev_);   //Memory chip ID.

    struct bmm150_settings settings;
    settings.pwr_mode = BMM150_POWERMODE_NORMAL;
    rslt |= bmm150_set_op_mode(&settings, &dev_);
    settings.preset_mode = BMM150_PRESETMODE_ENHANCED;
    rslt |= bmm150_set_presetmode(&settings, &dev_);    //Set the preset mode of
    return rslt;
}

int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    return ((BMM150Component*)(intf_ptr))->read_bytes(reg_addr, reg_data, length);
}

int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    return ((BMM150Component*)(intf_ptr))->write_bytes(reg_addr, reg_data, length);
}

void delay_us(uint32_t period_us, void *intf_ptr)
{
    delay(period_us);
}

}
}

