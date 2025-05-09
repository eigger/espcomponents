#ifndef __BMM150_H__
#define __BMM150_H__

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "bmm150_lib.h"

namespace esphome {
namespace bmm150 {

class BMM150Component : public PollingComponent, public i2c::I2CDevice {
public:
    void set_mag_x(sensor::Sensor *mag_x) { mag_x_ = mag_x; }
    void set_mag_y(sensor::Sensor *mag_y) { mag_y_ = mag_y; }
    void set_mag_z(sensor::Sensor *mag_z) { mag_z_ = mag_z; }

    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override;
    void update() override;

protected:
    sensor::Sensor *mag_x_{nullptr};
    sensor::Sensor *mag_y_{nullptr};
    sensor::Sensor *mag_z_{nullptr};

    struct bmm150_dev dev_;
    struct bmm150_mag_data mag_data_;
    bmm150_mag_data mag_offset_;
    bmm150_mag_data mag_max_;
    bmm150_mag_data mag_min_;
    
    int8_t bmm150_initialization();
}; 

}
}
#endif
