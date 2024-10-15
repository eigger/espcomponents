```
external_components:
  - source: github://eigger/espcomponents/releases/latest
    components: [ bmm150 ]
    refresh: always

i2c:
   - id: bus_a
     sda: GPIO21
     scl: GPIO22
     scan: true
     
bmm150:
  i2c_id: bus_a
  update_interval: 30s
```
