```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ bmm150 ]
    
i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true
     
sensor:
  - platform: bmm150
    address: 0x10
    update_interval: 30s
    magnetic_field_x:
      name: "MAG X"
    magnetic_field_y:
      name: "MAG Y"
    magnetic_field_z:
      name: "MAG Z"

```
