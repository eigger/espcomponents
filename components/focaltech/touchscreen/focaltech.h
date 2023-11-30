#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace focaltech {

#define FOCALTECH_REGISTER_MODE         (0x00)
#define FOCALTECH_REGISTER_GEST         (0x01)
#define FOCALTECH_REGISTER_STATUS       (0x02)
#define FOCALTECH_REGISTER_TOUCH1_XH    (0x03)
#define FOCALTECH_REGISTER_TOUCH1_XL    (0x04)
#define FOCALTECH_REGISTER_TOUCH1_YH    (0x05)
#define FOCALTECH_REGISTER_TOUCH1_YL    (0x06)
#define FOCALTECH_REGISTER_THRESHHOLD   (0x80)
#define FOCALTECH_REGISTER_CONTROL      (0x86)
#define FOCALTECH_REGISTER_MONITORTIME  (0x87)
#define FOCALTECH_REGISTER_ACTIVEPERIOD  (0x88)
#define FOCALTECH_REGISTER_MONITORPERIOD  (0x89)

#define FOCALTECH_REGISTER_LIB_VERSIONH (0xA1)
#define FOCALTECH_REGISTER_LIB_VERSIONL (0xA2)
#define FOCALTECH_REGISTER_INT_STATUS   (0xA4)
#define FOCALTECH_REGISTER_POWER_MODE   (0xA5)
#define FOCALTECH_REGISTER_VENDOR_ID    (0xA3)
#define FOCALTECH_REGISTER_VENDOR1_ID   (0xA8)
#define FOCALTECH_REGISTER_ERROR_STATUS (0xA9)

typedef enum {
    FOCALTECH_NO_GESTRUE,
    FOCALTECH_MOVE_UP,
    FOCALTECH_MOVE_LEFT,
    FOCALTECH_MOVE_DOWN,
    FOCALTECH_MOVE_RIGHT,
    FOCALTECH_ZOOM_IN,
    FOCALTECH_ZOOM_OUT,
} GesTrue_t;

typedef enum {
    FOCALTECH_EVENT_PUT_DOWN,
    FOCALTECH_EVENT_PUT_UP,
    FOCALTECH_EVENT_CONTACT,
    FOCALTECH_EVENT_NONE,
} EventFlag_t;

typedef enum {
    FOCALTECH_PMODE_ACTIVE = 0,         // ~4mA
    FOCALTECH_PMODE_MONITOR = 1,        // ~3mA
    FOCALTECH_PMODE_DEEPSLEEP = 3,      // ~100uA  The reset pin must be pulled down to wake up
} PowerMode_t;


using namespace touchscreen;

struct FocalTechTouchscreenStore {
    volatile bool touch;
    ISRInternalGPIOPin pin;

    static void gpio_intr(FocalTechTouchscreenStore *store);
};

class FocalTechButtonListener {
public:
    virtual void update_button(uint8_t index, uint16_t state) = 0;
};

class FocalTechTouchscreen : public Touchscreen, public Component, public i2c::I2CDevice {
public:
    void setup() override;
    void loop() override;
    void dump_config() override;
    float get_setup_priority() const override;

    void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
    void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

    void register_button_listener(FocalTechButtonListener *listener) { this->button_listeners_.push_back(listener); }

protected:
    void reset_();
    bool read_reg_(uint8_t reg, uint8_t *value);
    bool read_reg16_(uint8_t reg, uint16_t *value);
    bool write_reg_(uint8_t reg, uint8_t value);
    void setTheshold(uint8_t value);
    uint8_t getThreshold(void);

    uint8_t getMonitorTime(void);
    void setMonitorTime(uint8_t sec);
    uint8_t getActivePeriod(void);
    void setActivePeriod(uint8_t rate);
    uint8_t getMonitorPeriod(void);
    void setMonitorPeriod(uint8_t rate);
    void enableAutoCalibration(void);
    void disableAutoCalibration(void);
    void getLibraryVersion(uint16_t &version);
    void setPowerMode(PowerMode_t m);
    PowerMode_t getPowerMode(void);
    uint8_t getVendorID(void);
    uint8_t getVendor1ID(void);
    uint8_t getErrorCode(void);
    void enableINT(void);
    void disableINT(void);
    uint8_t getINTMode(void);
    bool getPoint(uint16_t &x, uint16_t &y);
    uint8_t getTouched(void);
    uint8_t getControl(void);
    uint8_t getDeviceMode(void);
    GesTrue_t getGesture(void);

    FocalTechTouchscreenStore store_;

    InternalGPIOPin *interrupt_pin_;
    GPIOPin *reset_pin_{nullptr};

    std::vector<FocalTechButtonListener *> button_listeners_;
};

}  // namespace focaltech
}  // namespace esphome
