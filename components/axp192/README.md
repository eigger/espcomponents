external_components:
  - source:
      type: git
      url: https://github.com/eigger/espcomponents/
      ref: dev
    components: [ axp192 ]
    refresh: always

i2c:
   - id: bus_a
     sda: GPIO21
     scl: GPIO22
     scan: true
     
sensor:
  - platform: axp192
    address: 0x34
    i2c_id: bus_a
    update_interval: 30s
    battery_level:
      id: "id_batterylevel"
      internal: True
