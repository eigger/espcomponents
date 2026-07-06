#include "m5unit_scales.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace m5unit_scales {

static const char *const TAG = "m5unit_scales";

// Registers for MINI model
static const uint8_t REG_MINI_RAW_ADC = 0x00;
static const uint8_t REG_MINI_CAL_DATA = 0x10;
static const uint8_t REG_MINI_BUTTON = 0x20;
static const uint8_t REG_MINI_RGB_LED = 0x30;
static const uint8_t REG_MINI_SET_GAP = 0x40;
static const uint8_t REG_MINI_SET_OFFSET = 0x50;
static const uint8_t REG_MINI_FILTER_BASE = 0x80;

// Registers for STANDARD model
static const uint8_t REG_STD_RAW_ADC = 0x10;
static const uint8_t REG_STD_CAL_DATA = 0x14;
static const uint8_t REG_STD_SET_OFFSET = 0x24;
static const uint8_t REG_STD_BUTTON = 0x22;
static const uint8_t REG_STD_RGB_LED = 0x50;

// Common Registers
static const uint8_t REG_FIRMWARE_VERSION = 0xFE;

void M5UnitScalesComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Unit-Scales (%s model)...", 
                this->model_ == M5UNIT_SCALES_MODEL_MINI ? "MINI" : "STANDARD");

  // Read firmware version as a basic connectivity check
  uint8_t fw_version = 0;
  if (!this->read_byte(REG_FIRMWARE_VERSION, &fw_version)) {
    ESP_LOGE(TAG, "Communication failed! Check connection and address.");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Connected successfully. Firmware version: %u", fw_version);
}

void M5UnitScalesComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Unit-Scales:");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_ == M5UNIT_SCALES_MODEL_MINI ? "MINI" : "STANDARD");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Connection failed!");
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Weight", this->weight_sensor_);
  LOG_SENSOR("  ", "Absolute Weight", this->absolute_weight_sensor_);
  LOG_SENSOR("  ", "Raw ADC", this->raw_adc_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Button", this->button_sensor_);
#endif
}

float M5UnitScalesComponent::get_setup_priority() const {
  return setup_priority::DATA;
}

void M5UnitScalesComponent::update() {
  if (this->is_failed()) {
    return;
  }

  // Perform initial synchronization of numbers/switches (MINI only)
  if (this->model_ == M5UNIT_SCALES_MODEL_MINI && !this->initial_sync_done_) {
    this->initial_sync_done_ = true;

#ifdef USE_SWITCH
    // LP Filter State
    uint8_t lp_val = 0;
    if (this->read_byte(REG_MINI_FILTER_BASE, &lp_val)) {
      if (this->lp_filter_switch_ != nullptr) {
        this->lp_filter_switch_->publish_state(lp_val != 0);
      }
    }
#endif

#ifdef USE_NUMBER
    // Avg Filter Value
    uint8_t avg_val = 0;
    if (this->read_byte(REG_MINI_FILTER_BASE + 1, &avg_val)) {
      if (this->avg_filter_number_ != nullptr) {
        this->avg_filter_number_->publish_state(avg_val);
      }
    }

    // EMA Filter Value
    uint8_t ema_val = 0;
    if (this->read_byte(REG_MINI_FILTER_BASE + 2, &ema_val)) {
      if (this->ema_filter_number_ != nullptr) {
        this->ema_filter_number_->publish_state(ema_val);
      }
    }

    // Gap Calibration Value
    uint8_t gap_bytes[4];
    if (this->read_bytes(REG_MINI_SET_GAP, gap_bytes, 4)) {
      float gap = 0.0f;
      memcpy(&gap, gap_bytes, 4);
      if (this->gap_number_ != nullptr) {
        this->gap_number_->publish_state(gap);
      }
    }
#endif
  } else if (this->model_ == M5UNIT_SCALES_MODEL_STANDARD && !this->initial_sync_done_) {
    this->initial_sync_done_ = true;
#ifdef USE_SWITCH
    if (this->lp_filter_switch_ != nullptr) {
      ESP_LOGW(TAG, "Low Pass Filter switch is only supported on MINI model.");
    }
#endif
#ifdef USE_NUMBER
    if (this->avg_filter_number_ != nullptr || this->ema_filter_number_ != nullptr) {
      ESP_LOGW(TAG, "Average and EMA filter numbers are only supported on MINI model.");
    }
    if (this->gap_number_ != nullptr) {
      ESP_LOGW(TAG, "Gap calibration number is only supported on MINI model. Use manual tare/zero calibration for STANDARD.");
    }
#endif
  }

#ifdef USE_SENSOR
  // Read Weight
  if (this->weight_sensor_ != nullptr || this->absolute_weight_sensor_ != nullptr) {
    uint8_t data[4];
    if (this->model_ == M5UNIT_SCALES_MODEL_MINI) {
      if (this->read_bytes(REG_MINI_CAL_DATA, data, 4)) {
        float weight = 0.0f;
        memcpy(&weight, data, 4);
        this->last_read_weight_ = weight;
        if (this->weight_sensor_ != nullptr) {
          this->weight_sensor_->publish_state(weight);
        }
        if (this->absolute_weight_sensor_ != nullptr) {
          this->absolute_weight_sensor_->publish_state(weight + this->tare_offset_);
        }
      } else {
        ESP_LOGW(TAG, "Failed to read weight data.");
      }
    } else {
      // STANDARD model reads Big-Endian raw 32-bit weight in 1/100g
      if (this->read_bytes(REG_STD_CAL_DATA, data, 4)) {
        int32_t weight_raw = ((int32_t)data[0] << 24) | 
                             ((int32_t)data[1] << 16) | 
                             ((int32_t)data[2] << 8) | 
                             data[3];
        float weight = (float)weight_raw / 100.0f;
        this->last_read_weight_ = weight;
        if (this->weight_sensor_ != nullptr) {
          this->weight_sensor_->publish_state(weight);
        }
        if (this->absolute_weight_sensor_ != nullptr) {
          this->absolute_weight_sensor_->publish_state(weight + this->tare_offset_);
        }
      } else {
        ESP_LOGW(TAG, "Failed to read weight data.");
      }
    }
  }

  // Read Raw ADC
  if (this->raw_adc_sensor_ != nullptr) {
    uint8_t data[4];
    if (this->model_ == M5UNIT_SCALES_MODEL_MINI) {
      if (this->read_bytes(REG_MINI_RAW_ADC, data, 4)) {
        int32_t raw_adc = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        this->raw_adc_sensor_->publish_state(raw_adc);
      } else {
        ESP_LOGW(TAG, "Failed to read raw ADC data.");
      }
    } else {
      // STANDARD model reads Big-Endian raw 32-bit ADC
      if (this->read_bytes(REG_STD_RAW_ADC, data, 4)) {
        int32_t raw_adc = ((int32_t)data[0] << 24) | 
                          ((int32_t)data[1] << 16) | 
                          ((int32_t)data[2] << 8) | 
                          data[3];
        this->raw_adc_sensor_->publish_state(raw_adc);
      } else {
        ESP_LOGW(TAG, "Failed to read raw ADC data.");
      }
    }
  }
#endif
}

void M5UnitScalesComponent::loop() {
#ifdef USE_BINARY_SENSOR
  if (this->is_failed() || this->button_sensor_ == nullptr) {
    return;
  }

  // Poll physical button status rapidly (every 50ms) to ensure responsiveness
  uint32_t now = millis();
  if (now - this->last_button_poll_ > 50) {
    this->last_button_poll_ = now;
    uint8_t btn_val = 0;
    uint8_t reg = (this->model_ == M5UNIT_SCALES_MODEL_MINI) ? REG_MINI_BUTTON : REG_STD_BUTTON;
    if (this->read_byte(reg, &btn_val)) {
      // 0 = pressed, 1 = no press
      bool pressed = (btn_val == 0);
      
      // Detect rising edge of physical button press
      if (pressed && !this->button_sensor_->state) {
        // Physical button transition to pressed.
        // The hardware will trigger a tare internally. Update tare_offset_ in software.
        float current_weight = 0.0f;
        bool read_success = false;
        uint8_t data[4];
        if (this->model_ == M5UNIT_SCALES_MODEL_MINI) {
          if (this->read_bytes(REG_MINI_CAL_DATA, data, 4)) {
            memcpy(&current_weight, data, 4);
            read_success = true;
          }
        } else {
          if (this->read_bytes(REG_STD_CAL_DATA, data, 4)) {
            int32_t weight_raw = ((int32_t)data[0] << 24) | 
                                 ((int32_t)data[1] << 16) | 
                                 ((int32_t)data[2] << 8) | 
                                 data[3];
            current_weight = (float)weight_raw / 100.0f;
            read_success = true;
          }
        }
        
        float weight_to_add = 0.0f;
        if (read_success) {
          // If the hardware hasn't zeroed the value yet, we use the on-demand read value.
          // However, if the hardware already tared and returns ~0, but our last_read_weight_ was significant,
          // we fall back to last_read_weight_.
          if (std::abs(current_weight) < 0.01f && std::abs(this->last_read_weight_) > 0.1f) {
            weight_to_add = this->last_read_weight_;
          } else {
            weight_to_add = current_weight;
          }
        } else {
          weight_to_add = this->last_read_weight_;
        }
        
        this->tare_offset_ += weight_to_add;
        ESP_LOGD(TAG, "Physical button pressed, updated tare offset to %.2f (added %.2f)", this->tare_offset_, weight_to_add);
      }
      
      this->button_sensor_->publish_state(pressed);
    }
  }
#endif
}

void M5UnitScalesComponent::tare() {
  ESP_LOGI(TAG, "Executing tare / zero calibration...");

  // Read current weight before tare command to update tare_offset_ in software
  float current_weight = 0.0f;
  bool read_success = false;
  uint8_t data[4];
  if (this->model_ == M5UNIT_SCALES_MODEL_MINI) {
    if (this->read_bytes(REG_MINI_CAL_DATA, data, 4)) {
      memcpy(&current_weight, data, 4);
      read_success = true;
    }
  } else {
    if (this->read_bytes(REG_STD_CAL_DATA, data, 4)) {
      int32_t weight_raw = ((int32_t)data[0] << 24) | 
                           ((int32_t)data[1] << 16) | 
                           ((int32_t)data[2] << 8) | 
                           data[3];
      current_weight = (float)weight_raw / 100.0f;
      read_success = true;
    }
  }

  if (read_success) {
    this->tare_offset_ += current_weight;
    ESP_LOGD(TAG, "Software tare triggered, updated tare offset to %.2f (added %.2f)", this->tare_offset_, current_weight);
  } else {
    // If on-demand read failed, fall back to last_read_weight_
    this->tare_offset_ += this->last_read_weight_;
    ESP_LOGW(TAG, "Failed to read current weight synchronously, fell back to last read weight: %.2f", this->last_read_weight_);
  }

  uint8_t cmd = 1;
  uint8_t reg = (this->model_ == M5UNIT_SCALES_MODEL_MINI) ? REG_MINI_SET_OFFSET : REG_STD_SET_OFFSET;
  if (!this->write_byte(reg, cmd)) {
    ESP_LOGE(TAG, "Failed to write tare command.");
  }
}

void M5UnitScalesComponent::set_lp_filter(bool enable) {
  if (this->model_ != M5UNIT_SCALES_MODEL_MINI) {
    ESP_LOGW(TAG, "Low Pass Filter is only supported on MINI model.");
    return;
  }
  uint8_t val = enable ? 1 : 0;
  ESP_LOGI(TAG, "Setting Low Pass Filter: %s", enable ? "Enabled" : "Disabled");
  if (!this->write_byte(REG_MINI_FILTER_BASE, val)) {
    ESP_LOGE(TAG, "Failed to write Low Pass Filter state.");
  }
}

void M5UnitScalesComponent::set_avg_filter(uint8_t avg) {
  if (this->model_ != M5UNIT_SCALES_MODEL_MINI) {
    ESP_LOGW(TAG, "Average Filter is only supported on MINI model.");
    return;
  }
  ESP_LOGI(TAG, "Setting Average Filter level: %u", avg);
  if (!this->write_byte(REG_MINI_FILTER_BASE + 1, avg)) {
    ESP_LOGE(TAG, "Failed to write Average Filter level.");
  }
}

void M5UnitScalesComponent::set_ema_filter(uint8_t ema) {
  if (this->model_ != M5UNIT_SCALES_MODEL_MINI) {
    ESP_LOGW(TAG, "EMA Filter is only supported on MINI model.");
    return;
  }
  ESP_LOGI(TAG, "Setting EMA Filter value: %u", ema);
  if (!this->write_byte(REG_MINI_FILTER_BASE + 2, ema)) {
    ESP_LOGE(TAG, "Failed to write EMA Filter value.");
  }
}

void M5UnitScalesComponent::set_gap_value(float gap) {
  if (this->model_ != M5UNIT_SCALES_MODEL_MINI) {
    ESP_LOGW(TAG, "Gap calibration is only supported on MINI model.");
    return;
  }
  ESP_LOGI(TAG, "Setting Gap calibration value: %.6f", gap);
  uint8_t data[4];
  memcpy(data, &gap, 4);
  if (!this->write_bytes(REG_MINI_SET_GAP, data, 4)) {
    ESP_LOGE(TAG, "Failed to write Gap calibration value.");
  }
}

void M5UnitScalesComponent::set_led_color(uint8_t r, uint8_t g, uint8_t b) {
  ESP_LOGD(TAG, "Setting LED Color: R=%u, G=%u, B=%u", r, g, b);
  if (this->model_ == M5UNIT_SCALES_MODEL_MINI) {
    uint8_t data[3] = {r, g, b};
    if (!this->write_bytes(REG_MINI_RGB_LED, data, 3)) {
      ESP_LOGE(TAG, "Failed to set LED color.");
    }
  } else {
    // STANDARD model expects 4 bytes (R, G, B, 0)
    uint8_t data[4] = {r, g, b, 0};
    if (!this->write_bytes(REG_STD_RGB_LED, data, 4)) {
      ESP_LOGE(TAG, "Failed to set LED color.");
    }
  }
}

}  // namespace m5unit_scales
}  // namespace esphome
