### add esphome *.yaml



```
external_components:
  - source: github://eigger/espcomponents
    components: [ gicisky_esl ]

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
