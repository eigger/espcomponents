### add esphome *.yaml
Protocol : http://docin.divoom-gz.com/web/#/5/146

Wrtie : service_uuid: '49535343-FE7D-4AE5-8FA9-9FAFD205E455' characteristic_uuid: '49535343-8841-43F4-A8D4-ECBE34729BB3'

Read : service_uuid: '49535343-FE7D-4AE5-8FA9-9FAFD205E455' characteristic_uuid: '49535343-6DAA-4D02-ABF6-19569ACA69FE'

```
external_components:
  - source: github://eigger/espcomponents
    components: [ divoom ]

ble_client:
  - mac_address: B1:21:81:35:C6:CE
    id: my_ble_client

font:
  - file: "fonts/Galmuri7.ttf"
    id: id_font
    size: 8
    glyphs: '!"%()+,-_.:°0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz안녕하세요반갑습니다점심시간'
color:
  - id: RED
    red: 100%
    green: 0%
    blue: 0%
  - id: GREEN
    red: 0%
    green: 100%
    blue: 0%
  - id: BLUE
    red: 0%
    green: 0%
    blue: 100%
  - id: YELLOW
    red: 100%
    green: 100%
    blue: 0%
  - id: ORANGE
    red: 100%
    green: 50%
    blue: 0%
  - id: WHITE
    red: 100%
    green: 100%
    blue: 100%


image:
  # - file: "images/rabbit.png"
  #   id: id_image_rabbit
  #   type: RGB24
  - file: "mdi:chip"
    resize: 16X16
    id: chip_icon

text:
  - platform: template
    name: "Display text"
    optimistic: true
    min_length: 0
    max_length: 100
    mode: text
    id: id_text

time:
  - platform: sntp
    id: sntp_time
    timezone: Asia/Seoul
    servers: 
      - 0.pool.ntp.org
      - 1.pool.ntp.org
      - 2.pool.ntp.org

display:
  - platform: divoom
    update_interval: 100ms
    time_id: sntp_time
    lambda: |-
      std::string message = id(id_text).state;
      if (message.size() > 0)
      {
        if (message == "chip")
        {
          it.image(0, 0, id(chip_icon));
        }
        else 
        {
          it.print(0, 0, id(id_font), message.c_str());
        }
      }
      else
      {
        it.print(0, 0, id(id_font), "");
      }



```
