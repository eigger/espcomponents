# M5Unit-Scales ESPHome Component

This ESPHome component provides support for the [M5Stack Unit-Mini Scales](https://docs.m5stack.com/en/unit/Unit-Mini%20Scales) (SKU: U177) and the [M5Stack Unit Scales](https://docs.m5stack.com/en/unit/scales). It communicates over I2C, allowing weight measurements, LED control, button monitoring, calibration, and filter settings directly from Home Assistant.

<p align="center">
  <img src="https://static-cdn.m5stack.com/resource/docs/products/unit/Unit-Mini%20Scales/img-e124306a-177f-4385-996d-18f10f928040.webp" alt="Unit-Mini Scales" width="45%" />
  <img src="https://static-cdn.m5stack.com/resource/docs/products/unit/UNIT%20Scales/img-22f5b336-4142-40f9-90ad-a99eaefeea09.webp" alt="Unit Scales" width="45%" />
</p>

## Configuration Example

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ m5unit_scales ]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

# Main Hub Component
m5unit_scales:
  id: my_scale
  model: MINI  # MINI (Unit-Mini Scales) or STANDARD (Unit Scales)
  address: 0x26
  update_interval: 1s

# Exposed Sensors
sensor:
  - platform: m5unit_scales
    weight:
      name: "Scale Weight"
    raw_adc:
      name: "Scale Raw ADC"

# Binary Sensor for physical button
binary_sensor:
  - platform: m5unit_scales
    button:
      name: "Scale Button"

# Tare Trigger
button:
  - platform: m5unit_scales
    tare:
      name: "Scale Tare"

# RGB LED Output
light:
  - platform: m5unit_scales
    name: "Scale LED"

# Filter Controls (Only supported/used on MINI model)
switch:
  - platform: m5unit_scales
    lp_filter:
      name: "Scale LP Filter"

number:
  - platform: m5unit_scales
    avg_filter:
      name: "Scale Avg Filter"
    ema_filter:
      name: "Scale EMA Filter"
    gap:
      name: "Scale Gap Value"
```

## Calibration Procedure (MINI Model)

To calibrate your scale, you can use the GAP calibration formula:
$$\text{GAP} = \frac{\text{RawADC}_{100g} - \text{RawADC}_{0g}}{100}$$

Follow these steps using the Home Assistant UI or ESPHome logs:
1. **Clear Weight & Tare**: Ensure the scale is empty. Trigger the **Tare Button** (`Scale Tare`) to reset the offset.
2. **Read Zero Weight ADC**: Read the value of the `Scale Raw ADC` sensor with no weight on the scale. This is your $\text{RawADC}_{0g}$ value.
3. **Place Calibration Weight**: Put a known weight (e.g. 100g) on the scale, and read the new `Scale Raw ADC` value. This is your $\text{RawADC}_{weight}$ value.
4. **Calculate GAP**:
   $$\text{GAP} = \frac{\text{RawADC}_{weight} - \text{RawADC}_{0g}}{\text{known\_weight}}$$
   For example, if $\text{RawADC}_{0g} = 10500$, and placing a $100g$ weight gives $\text{RawADC}_{100g} = 22500$:
   $$\text{GAP} = \frac{22500 - 10500}{100} = 120.0$$
5. **Set GAP**: Update the **Scale Gap Value** (`Scale Gap Value` number entity) in Home Assistant to the calculated GAP value. The STM32 MCU will save this internally, and the `Scale Weight` sensor will start outputting calibrated weight in grams!
