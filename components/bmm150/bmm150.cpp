#include "bmm150.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstring>

namespace esphome {
namespace bmm150 {

static const char *TAG = "bmm150";

int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
void delay_us(uint32_t period_us, void *intf_ptr);

static bool is_overflow(int16_t value) { return value == BMM150_OVERFLOW_OUTPUT; }

void BMM150Component::setup() {
  int8_t code = this->bmm150_initialization();
  if (code == BMM150_OK) {
    this->initialized_ = true;
    return;
  }
  // Wrong/missing chip ID is definitive. Bus NAKs during boot are not — this bus
  // recovers after setup(), and mark_failed() has no retry path.
  if (code == BMM150_E_DEV_NOT_FOUND) {
    ESP_LOGE(TAG, "Init failed (%d)", code);
    this->mark_failed();
    return;
  }
  ESP_LOGW(TAG, "Init failed (%d), will retry", code);
  this->status_set_warning();
}

void BMM150Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMM150:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }
  if (!this->initialized_) {
    ESP_LOGW(TAG, "  Initialization pending, will retry");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Magnetic Field X", this->mag_x_);
  LOG_SENSOR("  ", "Magnetic Field Y", this->mag_y_);
  LOG_SENSOR("  ", "Magnetic Field Z", this->mag_z_);
}

float BMM150Component::get_setup_priority() const { return setup_priority::DATA; }

void BMM150Component::update() {
  if (this->is_failed())
    return;

  if (!this->initialized_) {
    int8_t code = this->bmm150_initialization();
    if (code == BMM150_E_DEV_NOT_FOUND) {
      ESP_LOGE(TAG, "Init failed (%d)", code);
      this->mark_failed();
      return;
    }
    if (code != BMM150_OK) {
      ESP_LOGW(TAG, "Init retry failed (%d)", code);
      this->status_set_warning();
      return;
    }
    this->initialized_ = true;
    ESP_LOGI(TAG, "Initialized after retry");
  }

  this->bus_error_ = false;
  int8_t code = bmm150_read_mag_data(&mag_data_, &dev_);
  // bmm150_get_regs() stores bus results in intf_rslt only and overwrites it per
  // transaction. The callback latch covers any I2C failure in this call.
  if (code != BMM150_OK || this->bus_error_) {
    ESP_LOGW(TAG, "Read failed (rslt=%d)", code);
    this->status_set_warning();
    return;
  }

  if (is_overflow(mag_data_.x) || is_overflow(mag_data_.y) || is_overflow(mag_data_.z)) {
    ESP_LOGW(TAG, "Compensation overflow (x=%d y=%d z=%d)", mag_data_.x, mag_data_.y, mag_data_.z);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  if (this->mag_x_ != nullptr)
    this->mag_x_->publish_state(mag_data_.x);
  if (this->mag_y_ != nullptr)
    this->mag_y_->publish_state(mag_data_.y);
  if (this->mag_z_ != nullptr)
    this->mag_z_->publish_state(mag_data_.z);
}

int8_t BMM150Component::bmm150_initialization() {
  memset(&dev_, 0, sizeof(dev_));
  int8_t rslt = BMM150_OK;
  dev_.intf = BMM150_I2C_INTF;
  dev_.read = reg_read;
  dev_.write = reg_write;
  dev_.delay_us = delay_us;
  dev_.intf_ptr = this;

  this->bus_error_ = false;
  rslt = bmm150_init(&dev_);
  // bmm150_init() only sets dev_.chip_id on ID match but still returns BMM150_OK otherwise.
  if (rslt != BMM150_OK)
    return rslt;
  if (dev_.chip_id != BMM150_CHIP_ID)
    return BMM150_E_DEV_NOT_FOUND;
  // read_trim_registers() runs three reads; intf_rslt only reflects the last one and partial
  // failures still commit zeroed trim_data. Latch any callback failure instead.
  if (this->bus_error_)
    return BMM150_E_COM_FAIL;

  struct bmm150_settings settings;
  settings.pwr_mode = BMM150_POWERMODE_NORMAL;
  rslt = bmm150_set_op_mode(&settings, &dev_);
  if (rslt != BMM150_OK)
    return rslt;
  if (this->bus_error_)
    return BMM150_E_COM_FAIL;

  settings.preset_mode = BMM150_PRESETMODE_ENHANCED;
  rslt = bmm150_set_presetmode(&settings, &dev_);
  if (rslt != BMM150_OK)
    return rslt;
  if (this->bus_error_)
    return BMM150_E_COM_FAIL;
  return BMM150_OK;
}

int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr) {
  auto *self = (BMM150Component *) intf_ptr;
  if (self->read_bytes(reg_addr, reg_data, (uint8_t) length))
    return BMM150_INTF_RET_SUCCESS;
  self->set_bus_error();
  return BMM150_E_COM_FAIL;
}

int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr) {
  auto *self = (BMM150Component *) intf_ptr;
  if (self->write_bytes(reg_addr, reg_data, (uint8_t) length))
    return BMM150_INTF_RET_SUCCESS;
  self->set_bus_error();
  return BMM150_E_COM_FAIL;
}

void delay_us(uint32_t period_us, void *intf_ptr) { delayMicroseconds(period_us); }

}  // namespace bmm150
}  // namespace esphome
