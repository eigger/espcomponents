#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include <ESPAsyncTCP.h>
#include <HardwareSerial.h>

namespace esphome {
namespace wifi_serial {

static const char *TAG = "wifi_serial";
static const char *SERVER_HOST_NAME = "Wifi Serial";
static std::vector<AsyncClient *> clients_;

class WS_Client;

class WifiSerial : public Component {
 public:
  void setup() override;
  void loop() override;
  void set_port(uint16_t port) { port_ = port; }
  uint16_t get_port() const { return port_; }
  
  void _handleDisconnect(WS_Client *cli);

  void set_baud_rate(uint32_t baud_rate) { baud_rate_ = baud_rate; }
  void set_tx_pin(uint8_t tx_pin) { this->tx_pin_ = tx_pin; }
  void set_rx_pin(uint8_t rx_pin) { this->rx_pin_ = rx_pin; }
  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }

 protected:
  int initialized_{0};
  uint16_t port_{5000};
  AsyncServer* server_{nullptr};
  WS_Client* client_{nullptr};
  optional<uint8_t> tx_pin_;
  optional<uint8_t> rx_pin_;
  uint32_t baud_rate_;
  uint8_t stop_bits_;
  HardwareSerial *hw_serial_{nullptr};
};

class WS_Client {
 private:
  AsyncClient* _client;
  WifiSerial* _parent;
  HardwareSerial *hw_serial_{nullptr};
  void write_byte(uint8_t data);
  void write_array(const uint8_t *data, size_t len);
  int available();
  bool check_read_timeout_(size_t len = 1);
  bool read_byte(uint8_t *data);
  int read();
  bool read_array(uint8_t *data, size_t len);
  
  void _onData(void *buf, size_t len);
  void _onDisconnect();
 public:
  WS_Client(WifiSerial*, AsyncClient*);
  ~WS_Client();
  
  void loop();
};

}  // namespace wifi_serial
}  // namespace esphome
