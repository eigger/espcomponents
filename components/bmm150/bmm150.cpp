#include "bmm150.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cmath>
#include <cstring>
#include <numbers>

namespace esphome {
namespace bmm150 {

static const char *TAG = "bmm150";

// B-1 unverified: Bosch integer compensate_*() is documented as µT. Confirm on hardware
// (horizontal circle radius ~30 µT in Seoul, |B| ~50 µT). Change this if the scale is wrong.
static constexpr float MAG_UT_SCALE = 1.0f;
static constexpr float CAL_MIN_DELTA_UT = 20.0f;

int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
void delay_us(uint32_t period_us, void *intf_ptr);

static bool is_overflow(int16_t value) { return value == BMM150_OVERFLOW_OUTPUT; }

void BMM150Component::set_mag_axes(uint8_t x_src, int8_t x_sign, uint8_t y_src, int8_t y_sign, uint8_t z_src,
                                   int8_t z_sign) {
  this->mag_axes_.src[0] = x_src;
  this->mag_axes_.sign[0] = x_sign;
  this->mag_axes_.src[1] = y_src;
  this->mag_axes_.sign[1] = y_sign;
  this->mag_axes_.src[2] = z_src;
  this->mag_axes_.sign[2] = z_sign;
}

void BMM150Component::set_accel_axes(uint8_t x_src, int8_t x_sign, uint8_t y_src, int8_t y_sign, uint8_t z_src,
                                     int8_t z_sign) {
  this->accel_axes_.src[0] = x_src;
  this->accel_axes_.sign[0] = x_sign;
  this->accel_axes_.src[1] = y_src;
  this->accel_axes_.sign[1] = y_sign;
  this->accel_axes_.src[2] = z_src;
  this->accel_axes_.sign[2] = z_sign;
}

void BMM150Component::setup() {
  this->load_calibration_();
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
  LOG_SENSOR("  ", "Heading", this->heading_);
  ESP_LOGCONFIG(TAG, "  Declination: %.1f°", this->declination_);
  ESP_LOGCONFIG(TAG, "  Soft-iron: %s", YESNO(this->soft_iron_));
  ESP_LOGCONFIG(TAG, "  Tilt compensation: %s",
                (this->accel_x_ != nullptr) ? "accel sensors" : "disabled (planar fallback)");
  // B-4 unverified: identity maps assume mag and accel axes are parallel on the PCB.
  ESP_LOGCONFIG(TAG, "  Mag axes: %c%c %c%c %c%c (unverified)", this->mag_axes_.sign[0] < 0 ? '-' : '+',
                "XYZ"[this->mag_axes_.src[0]], this->mag_axes_.sign[1] < 0 ? '-' : '+', "XYZ"[this->mag_axes_.src[1]],
                this->mag_axes_.sign[2] < 0 ? '-' : '+', "XYZ"[this->mag_axes_.src[2]]);
  ESP_LOGCONFIG(TAG, "  Accel axes: %c%c %c%c %c%c (unverified)", this->accel_axes_.sign[0] < 0 ? '-' : '+',
                "XYZ"[this->accel_axes_.src[0]], this->accel_axes_.sign[1] < 0 ? '-' : '+',
                "XYZ"[this->accel_axes_.src[1]], this->accel_axes_.sign[2] < 0 ? '-' : '+',
                "XYZ"[this->accel_axes_.src[2]]);
  if (this->calib_.valid == 1) {
    ESP_LOGCONFIG(TAG, "  Calibration: offset=(%d,%d,%d) scale=(%.3f,%.3f,%.3f)", this->calib_.offset_x,
                  this->calib_.offset_y, this->calib_.offset_z, this->calib_.scale_x, this->calib_.scale_y,
                  this->calib_.scale_z);
  } else {
    ESP_LOGW(TAG, "  Calibration: not stored (heading will stay unknown until bmm150.calibrate)");
  }
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

  float raw[3] = {mag_data_.x * MAG_UT_SCALE, mag_data_.y * MAG_UT_SCALE, mag_data_.z * MAG_UT_SCALE};
  float mag[3];
  this->apply_axes_(raw, this->mag_axes_, mag);

  if (this->mag_x_ != nullptr)
    this->mag_x_->publish_state(mag[0]);
  if (this->mag_y_ != nullptr)
    this->mag_y_->publish_state(mag[1]);
  if (this->mag_z_ != nullptr)
    this->mag_z_->publish_state(mag[2]);

  if (this->calibrating_) {
    for (int i = 0; i < 3; i++) {
      if (mag[i] < this->cal_min_[i])
        this->cal_min_[i] = mag[i];
      if (mag[i] > this->cal_max_[i])
        this->cal_max_[i] = mag[i];
    }
    if (this->heading_ != nullptr)
      this->heading_->publish_state(NAN);
    return;
  }

  if (this->heading_ == nullptr)
    return;

  if (this->calib_.valid != 1) {
    this->heading_->publish_state(NAN);
    return;
  }

  float mx = (mag[0] - this->calib_.offset_x) * this->calib_.scale_x;
  float my = (mag[1] - this->calib_.offset_y) * this->calib_.scale_y;
  float mz = (mag[2] - this->calib_.offset_z) * this->calib_.scale_z;
  float heading = this->compute_heading_(mx, my, mz);
  this->heading_->publish_state(heading);
}

void BMM150Component::start_calibration(uint32_t duration_ms) {
  this->calibrating_ = true;
  for (int i = 0; i < 3; i++) {
    this->cal_min_[i] = 10000.0f;
    this->cal_max_[i] = -10000.0f;
  }
  this->cancel_timeout("bmm150_cal");
  this->set_timeout("bmm150_cal", duration_ms, [this]() { this->finish_calibration_(); });
  ESP_LOGI(TAG, "Calibration started (%u ms); rotate the device in a figure-8", duration_ms);
}

void BMM150Component::finish_calibration_() {
  this->calibrating_ = false;
  float dx = this->cal_max_[0] - this->cal_min_[0];
  float dy = this->cal_max_[1] - this->cal_min_[1];
  float dz = this->cal_max_[2] - this->cal_min_[2];
  if (dx < CAL_MIN_DELTA_UT || dy < CAL_MIN_DELTA_UT || dz < CAL_MIN_DELTA_UT) {
    ESP_LOGW(TAG, "Calibration rejected: axis delta (%.1f, %.1f, %.1f) µT, need > %.0f µT each", dx, dy, dz,
             CAL_MIN_DELTA_UT);
    this->calibration_finished_trigger_.trigger(false);
    return;
  }

  this->calib_.offset_x = (int16_t) ((this->cal_max_[0] + this->cal_min_[0]) / 2.0f);
  this->calib_.offset_y = (int16_t) ((this->cal_max_[1] + this->cal_min_[1]) / 2.0f);
  this->calib_.offset_z = (int16_t) ((this->cal_max_[2] + this->cal_min_[2]) / 2.0f);
  if (this->soft_iron_) {
    float avg = (dx + dy + dz) / 3.0f;
    this->calib_.scale_x = avg / dx;
    this->calib_.scale_y = avg / dy;
    this->calib_.scale_z = avg / dz;
  } else {
    this->calib_.scale_x = this->calib_.scale_y = this->calib_.scale_z = 1.0f;
  }
  this->calib_.valid = 1;
  this->save_calibration_();
  ESP_LOGI(TAG, "Calibration saved: offset=(%d,%d,%d) scale=(%.3f,%.3f,%.3f)", this->calib_.offset_x,
           this->calib_.offset_y, this->calib_.offset_z, this->calib_.scale_x, this->calib_.scale_y,
           this->calib_.scale_z);
  this->calibration_finished_trigger_.trigger(true);
}

void BMM150Component::load_calibration_() {
  // Component is not an EntityBase, so get_object_id_hash() is unavailable.
  uint32_t hash = fnv1_hash(str_sprintf("bmm150_cal_%02X", this->address_));
  this->pref_ = global_preferences->make_preference<BMM150Calibration>(hash, true);
  this->calib_.offset_x = this->calib_.offset_y = this->calib_.offset_z = 0;
  this->calib_.scale_x = this->calib_.scale_y = this->calib_.scale_z = 1.0f;
  this->calib_.valid = 0;
  BMM150Calibration loaded{};
  if (this->pref_.load(&loaded) && loaded.valid == 1) {
    this->calib_ = loaded;
    ESP_LOGI(TAG, "Loaded calibration offset=(%d,%d,%d) scale=(%.3f,%.3f,%.3f)", this->calib_.offset_x,
             this->calib_.offset_y, this->calib_.offset_z, this->calib_.scale_x, this->calib_.scale_y,
             this->calib_.scale_z);
  } else {
    ESP_LOGW(TAG, "No stored calibration; heading will stay unknown until bmm150.calibrate succeeds");
  }
}

void BMM150Component::save_calibration_() {
  if (!this->pref_.save(&this->calib_)) {
    ESP_LOGW(TAG, "Failed to save calibration");
  }
}

void BMM150Component::apply_axes_(const float in[3], const BMM150AxisMap &map, float out[3]) const {
  out[0] = map.sign[0] * in[map.src[0]];
  out[1] = map.sign[1] * in[map.src[1]];
  out[2] = map.sign[2] * in[map.src[2]];
}

bool BMM150Component::read_accel_(float accel[3]) const {
  if (this->accel_x_ == nullptr || this->accel_y_ == nullptr || this->accel_z_ == nullptr)
    return false;
  if (!this->accel_x_->has_state() || !this->accel_y_->has_state() || !this->accel_z_->has_state())
    return false;
  float in[3] = {this->accel_x_->state, this->accel_y_->state, this->accel_z_->state};
  if (std::isnan(in[0]) || std::isnan(in[1]) || std::isnan(in[2]))
    return false;
  this->apply_axes_(in, this->accel_axes_, accel);
  return true;
}

float BMM150Component::wrap_degrees_(float deg) {
  deg = fmodf(deg, 360.0f);
  if (deg < 0.0f)
    deg += 360.0f;
  return deg;
}

float BMM150Component::compute_heading_(float mx, float my, float mz) const {
  float heading;
  float accel[3];
  if (this->read_accel_(accel)) {
    // Standard tilt-compensated compass. Valid only if mag/accel axes are aligned (see mag_axes/accel_axes).
    const float ax = accel[0];
    const float ay = accel[1];
    const float az = accel[2];
    const float roll = atan2f(ay, az);
    const float pitch = atan2f(-ax, ay * sinf(roll) + az * cosf(roll));
    const float xh = mx * cosf(pitch) + mz * sinf(pitch);
    const float yh = mx * sinf(roll) * sinf(pitch) + my * cosf(roll) - mz * sinf(roll) * cosf(pitch);
    heading = atan2f(-yh, xh) * (180.0f / std::numbers::pi_v<float>);
  } else {
    heading = atan2f(-my, mx) * (180.0f / std::numbers::pi_v<float>);
  }
  return wrap_degrees_(heading + this->declination_);
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
