### add esphome *.yaml
```
external_components:
  - source: github://eigger/espcomponents
    components: [ divoom ]

display:
  - platform: divoom
    mac_address: B1:21:81:35:C6:CE
    update_interval: 75ms
    lambda: |-
      std::string message = id(text_hass).state;
      if (message.size() > 0)
      {
        if (message == "rabbit")
        {
          it.image(0, 0, id(id_image_rabbit));
        }
        else if (message == "book")
        {
          it.image(0, 0, id(id_image_book));
        }
        else if (message == "coin")
        {
          it.image(0, 0, id(id_image_coin));
        }
        else if (message == "crystal")
        {
          it.image(0, 0, id(id_image_crystal));
        }
        else if (message == "potion")
        {
          it.image(0, 0, id(id_image_potion));
        }
        else if (message == "scroll")
        {
          it.image(0, 0, id(id_image_scroll));
        }
        else 
        {
          it.print(0, 0, id(digit_font_7), message.c_str());
        }
      }
      else
      {
        it.print(0, 0, id(digit_font_7), "");
      }

text_sensor:
  - platform: homeassistant
    id: text_hass
    entity_id: input_text.ditoo
    
font:
  - file: "fonts/Galmuri11.ttf"
    id: digit_font_11
    size: 11
    glyphs: '!%()+,''"-_.:°0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz/안녕하세요자리비움쉬는시간외출근퇴점심저녁감지됨'
  - file: "fonts/Galmuri7.ttf"
    id: digit_font_7
    size: 8
    glyphs: '!%()+,''"-_.:°0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz/안녕하세요자리비움쉬는시간외출근퇴점심저녁감지됨'

image:
  - file: "images/rabbit.png"
    id: id_image_rabbit
    type: RGB24
  - file: "images/book_06g.png"
    id: id_image_book
    type: RGB24
  - file: "images/coin_04c.png"
    id: id_image_coin
    type: RGB24
  - file: "images/crystal_01c.png"
    id: id_image_crystal
    type: RGB24
  - file: "images/potion_03a.png"
    id: id_image_potion
    type: RGB24
  - file: "images/scroll_01h.png"
    id: id_image_scroll
    type: RGB24

```
