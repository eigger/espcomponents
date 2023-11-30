#include "focaltech.h"
#include "esphome/core/log.h"

namespace esphome {
namespace focaltech {

static const char *const TAG = "focaltech";

void FocalTechTouchscreenStore::gpio_intr(FocalTechTouchscreenStore *store) { store->touch = true; }

float FocalTechTouchscreen::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }

void FocalTechTouchscreen::setup() {
    ESP_LOGCONFIG(TAG, "Setting up FocalTech Touchscreen...");

    // Register interrupt pin
    if (this->interrupt_pin_ != nullptr) {
        this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
        this->interrupt_pin_->setup();
        this->store_.pin = this->interrupt_pin_->to_isr();
        this->interrupt_pin_->attach_interrupt(FocalTechTouchscreenStore::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
    }

    // Perform reset if necessary
    if (this->reset_pin_ != nullptr) {
        this->reset_pin_->setup();
        this->reset_();
    }

    // Update display dimensions if they were updated during display setup
    this->display_width_ = this->display_->get_width();
    this->display_height_ = this->display_->get_height();
    this->rotation_ = static_cast<TouchRotation>(this->display_->get_rotation());
}

void FocalTechTouchscreen::loop() {
    uint16_t x, y;
    if (getTouch(x, y))
    {
        TouchPoint tp;
        switch (this->rotation_) {
        case ROTATE_0_DEGREES:
            // Origin is top right, so mirror X by default
            tp.x = this->display_width_ - x;
            tp.y = y;
            break;
        case ROTATE_90_DEGREES:
            tp.x = y;
            tp.y = x;
            break;
        case ROTATE_180_DEGREES:
            tp.x = x;
            tp.y = this->display_height_ - y;
            break;
        case ROTATE_270_DEGREES:
            tp.x = this->display_height_ - y;
            tp.y = this->display_width_ - x;
            break;
        }
        tp.id = 0;
        tp.state = (uint8_t)getGesture();
        this->defer([this, tp]() { this->send_touch_(tp); });
    }
}

void FocalTechTouchscreen::reset_() {
    if (this->reset_pin_ != nullptr) {
        this->reset_pin_->digital_write(false);
        delay(10);
        this->reset_pin_->digital_write(true);
        delay(10);
    }
}

bool FocalTechTouchscreen::read_reg_(uint8_t reg, uint8_t *value)
{
    if (this->is_failed()) return false;
    return this->read_byte(reg, value);
}

bool FocalTechTouchscreen::read_reg16_(uint8_t reg, uint16_t *value)
{
    if (this->is_failed()) return false;
    return this->read_byte_16(reg, value);
}

bool FocalTechTouchscreen::write_reg_(uint8_t reg, uint8_t value)
{
    if (this->is_failed()) return false;
    return this->write_byte(reg, value);
}

void FocalTechTouchscreen::dump_config() {
    ESP_LOGCONFIG(TAG, "FocalTech Touchscreen:");
    LOG_I2C_DEVICE(this);
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

uint8_t FocalTechTouchscreen::getControl(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_CONTROL, &value);
    return value;
}

uint8_t FocalTechTouchscreen::getDeviceMode(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_MODE, &value);
    return (value >> 4) & 0x07;
}

GesTrue_t FocalTechTouchscreen::getGesture(void)
{
    uint8_t value = 0;
    if (!read_reg_(FOCALTECH_REGISTER_GEST, &value)) return FOCALTECH_NO_GESTRUE;
    switch (value) {
    case 0x10:
        return FOCALTECH_MOVE_UP;
    case 0x14:
        return FOCALTECH_MOVE_RIGHT;
    case 0x18:
        return FOCALTECH_MOVE_DOWN;
    case 0x1C:
        return FOCALTECH_MOVE_LEFT;
    case 0x48:
        return FOCALTECH_ZOOM_IN;
    case 0x49:
        return FOCALTECH_ZOOM_OUT;
    default:
        break;
    }
    return FOCALTECH_NO_GESTRUE;
}

void FocalTechTouchscreen::setTheshold(uint8_t value)
{
    write_reg_(FOCALTECH_REGISTER_THRESHHOLD, value);
}

uint8_t FocalTechTouchscreen::getThreshold(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_THRESHHOLD, &value);
    return value;
}

uint8_t FocalTechTouchscreen::getMonitorTime(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_MONITORTIME, &value);
    return value;
}

void FocalTechTouchscreen::setMonitorTime(uint8_t sec)
{
    write_reg_(FOCALTECH_REGISTER_MONITORTIME, sec);
}

uint8_t FocalTechTouchscreen::getActivePeriod(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_ACTIVEPERIOD, &value);
    return value;
}

void FocalTechTouchscreen::setActivePeriod(uint8_t period)
{
    write_reg_(FOCALTECH_REGISTER_ACTIVEPERIOD, period);
}

uint8_t FocalTechTouchscreen::getMonitorPeriod(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_MONITORPERIOD, &value);
    return value;
}

void FocalTechTouchscreen::setMonitorPeriod(uint8_t period)
{
    write_reg_(FOCALTECH_REGISTER_MONITORPERIOD, period);
}

void FocalTechTouchscreen::enableAutoCalibration(void)
{
    write_reg_(FOCALTECH_REGISTER_MONITORTIME, 0x00);
}

void FocalTechTouchscreen::disableAutoCalibration(void)
{
    write_reg_(FOCALTECH_REGISTER_MONITORTIME, 0xFF);
}

void FocalTechTouchscreen::getLibraryVersion(uint16_t &version)
{
    read_reg16_(FOCALTECH_REGISTER_LIB_VERSIONH, version);
}

void FocalTechTouchscreen::enableINT(void)
{
    write_reg_(FOCALTECH_REGISTER_INT_STATUS, 0x01);
}

void FocalTechTouchscreen::disableINT(void)
{
    write_reg_(FOCALTECH_REGISTER_INT_STATUS, 0x00);
}

uint8_t FocalTechTouchscreen::getINTMode(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_INT_STATUS, &value);
    return value;
}

bool FocalTechTouchscreen::getPoint(uint16_t &x, uint16_t &y)
{
    if (this->is_failed()) return false;
    uint8_t buffer[5];
    if (this->read_byts(FOCALTECH_REGISTER_STATUS, buffer, 5)) {
        if (buffer[0] == 0 || buffer[0] > 2) {
            return false;
        }
        event = (EventFlag_t)(buffer[1] & 0xC0);
        x = (buffer[1] & 0x0F) << 8 | buffer[2];
        y =  (buffer[3] & 0x0F) << 8 | buffer[4];

        ESP_LOGV(TAG, "x=%03u y=%03u\n", x, y);
        return true;
    }
    return false;
}

uint8_t FocalTechTouchscreen::getTouched()
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_STATUS, &value);
    return value;
}

void FocalTechTouchscreen::setPowerMode(PowerMode_t m)
{
    write_reg_(FOCALTECH_REGISTER_POWER_MODE, m);
}

PowerMode_t FocalTechTouchscreen::getPowerMode(void)
{
    uint8_t value = 0;
    if (!read_reg_(FOCALTECH_REGISTER_POWER_MODE, &value)) return FOCALTECH_PMODE_DEEPSLEEP;
    return (PowerMode_t)value;
}

uint8_t FocalTechTouchscreen::getVendorID(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_VENDOR_ID, &value);
    return value;
}

uint8_t FocalTechTouchscreen::getVendor1ID(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_VENDOR1_ID, &value);
    return value;
}

uint8_t FocalTechTouchscreen::getErrorCode(void)
{
    uint8_t value = 0;
    read_reg_(FOCALTECH_REGISTER_ERROR_STATUS, &value);
    return value;
}

}  // namespace focaltech
}  // namespace esphome
