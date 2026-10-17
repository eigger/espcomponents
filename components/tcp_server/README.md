# TCP Server

A raw TCP server for exchanging bytes with one client at a time. Supports read
and write callbacks, a write action, and UART forwarding through [uartex](../uartex).

## Minimal configuration

Merge into an ESPHome device configuration with networking enabled:

```yaml
socket:  # Required by tcp_server

external_components:
  - source: github://eigger/espcomponents@latest
    components: [tcp_server]

tcp_server:
  id: tcp_bridge
  port: 8888
  recv_buffer_size: 256
  on_read:
    - lambda: |-
        ESP_LOGI("tcp", "Received %u bytes", static_cast<unsigned>(len));
```

Connect to the device IP and the configured port with a TCP client.

| Option | Description |
| --- | --- |
| `port` | Required listening port |
| `recv_buffer_size` | Receive buffer size in bytes; default `256` |
| `on_read` | Bytes received from the TCP client; lambda arguments `data`, `len` |
| `on_write` | Bytes written to the TCP client; same lambda arguments |

## UART forwarding

Load both external components and replace the minimal `tcp_server` block with
this example. Adjust UART pins and baud rate to the attached device.

```yaml
socket:  # Required by tcp_server

external_components:
  - source: github://eigger/espcomponents@latest
    components: [tcp_server, uartex]

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

uartex:
  id: uart_bridge
  on_read:
    - lambda: id(tcp_bridge).write_array(data, len);

tcp_server:
  id: tcp_bridge
  port: 8888
  on_read:
    - lambda: id(uart_bridge).write_array(data, len);
```

To send fixed bytes from an automation:

```yaml
button:
  - platform: template
    name: "Send TCP Bytes"
    on_press:
      - tcp_server.write:
          id: tcp_bridge
          data: [0x01, 0x02, 0x03]
```

TCP read callbacks contain chunks, not guaranteed complete protocol messages.
`write_array` returns whether the entire buffer was written; there is no queued
retransmission for a partial write. The server provides no authentication or TLS.

[Other components](../README.md)
