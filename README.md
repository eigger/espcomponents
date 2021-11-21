# espcomponents
components for esphome

# add esphome *.yaml
external_components:
  - source:
      type: git
      url: https://github.com/eigger/espcomponents/
      #ref: master
    components: [ uartex ]
    refresh: always
  # use all components from a local folder
  #- source:
  #    type: local
  #    path: components
