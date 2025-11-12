```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ axp192 ]

i2c:
   - id: bus_a
     sda: GPIO21
     scl: GPIO22
     scan: true
     
axp192:
  address: 0x34
  i2c_id: bus_a
  update_interval: 30s
  battery_level:
    id: "id_batterylevel"
    internal: True
```
