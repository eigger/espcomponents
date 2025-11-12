```yaml
tcp_server:
  id: id_tcp
  port: 8888
  on_read:
    then:
      - lambda : id(id_uartex).write_array(data, len);
  
uartex:
  ...
  id: id_uartex
  on_read:
    then:
      - lambda : id(id_tcp).write(data, len);
```