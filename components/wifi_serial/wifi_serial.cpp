#include "wifi_serial.h"

#ifdef USE_LOGGER
#include <esphome/components/logger/logger.h>
#endif

namespace esphome {
namespace wifi_serial {

static WS_Client* client = NULL;

void WifiSerial::setup() {
  uint32_t mode = UART_NB_BIT_8 | UART_PARITY_NONE;

  ESP_LOGCONFIG(TAG, "Setting up Wifi Serial server...");

  ESP_LOGI(TAG, "Wifi Serial Server Started [%d]", this->port_);
  this->server_ = new AsyncServer(this->port_);

  this->server_->onClient([](void *s, AsyncClient* c){
    if(c == NULL)
      return;
    WifiSerial *p = (WifiSerial *)s;
    ESP_LOGI(TAG, "connecting... [%s]", c->remoteIP().toString().c_str());

    WS_Client *r = new WS_Client((WifiSerial*)s, c);
    if(r == NULL){
      c->close(true);
      c->free();
      delete c;
    }

    client = r;
  }, this->server_);
  this->server_->begin();

  if (this->stop_bits_ == 1)
    mode |= UART_NB_STOP_BIT_1;
  else
    mode |= UART_NB_STOP_BIT_2;
  SerialConfig config = static_cast<SerialConfig>(mode);
  if (this->tx_pin_.value_or(1) == 1 && this->rx_pin_.value_or(3) == 3) {
    this->hw_serial_ = &Serial;
    this->hw_serial_->begin(this->baud_rate_, config);
  } else if (this->tx_pin_.value_or(15) == 15 && this->rx_pin_.value_or(13) == 13) {
    this->hw_serial_ = &Serial;
    this->hw_serial_->begin(this->baud_rate_, config);
    this->hw_serial_->swap();
  } else if (this->tx_pin_.value_or(2) == 2 && this->rx_pin_.value_or(8) == 8) {
    this->hw_serial_ = &Serial1;
    this->hw_serial_->begin(this->baud_rate_, config);
  } else {
    ESP_LOGI(TAG, "Not Config [%d] %d, %d, %d", this->baud_rate_, this->tx_pin_, this->rx_pin_, this->stop_bits_);
  }
}

void WifiSerial::_handleDisconnect(WS_Client *cli) {
  delete cli;
  client = NULL;
}

void WifiSerial::loop() {
  if(client == NULL)
    return;

  client->loop();
}

WS_Client::WS_Client(WifiSerial* s, AsyncClient* c) 
  : _client(c)
  , _parent(s)
{
  this->hw_serial_ = &Serial;

  c->onDisconnect([](void *r, AsyncClient* c){ WS_Client *req = (WS_Client*)r; req->_onDisconnect(); delete c; }, this);
  c->onData([](void *r, AsyncClient* c, void *buf, size_t len){ (void)c; WS_Client *req = (WS_Client*)r; req->_onData(buf, len); }, this);
}

WS_Client::~WS_Client() {
}

void WS_Client::_onData(void *buf, size_t len){
  this->write_array((const uint8_t *)buf, len);
}

void WS_Client::_onDisconnect(){
  _parent->_handleDisconnect(this);
}

void WS_Client::write_byte(uint8_t data) {
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->write(data);
  }
}
void WS_Client::write_array(const uint8_t *data, size_t len) {
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->write(data, len);
  }
}

int WS_Client::available() {
  if (this->hw_serial_ != nullptr) {
    return this->hw_serial_->available();
  }
  return 0;
}

bool WS_Client::check_read_timeout_(size_t len) {
  if (this->available() >= int(len))
    return true;

  uint32_t start_time = millis();
  while (this->available() < int(len)) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %u!", this->available());
      return false;
    }
    yield();
  }
  return true;
}

bool WS_Client::read_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  if (this->hw_serial_ != nullptr) {
    *data = this->hw_serial_->read();
  }
  return true;
}

int WS_Client::read() {
  uint8_t data;
  if (!this->read_byte(&data))
    return -1;
  return data;
}

bool WS_Client::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->readBytes(data, len);
  } else {
    return false;
  }

  return true;
}

void WS_Client::loop() {
  uint8_t data[32];
  int len;
  int tot = 0;
  while((len=available())) {
    if (len+tot>32) {
      int sz = len+tot-32;
      this->hw_serial_->readBytes(&data[tot], sz);
      tot = 32;
      break;
    }

    this->hw_serial_->readBytes(&data[tot], len);
    tot+=len;
  }
  if (tot) {
    _client->add((char *)data, tot);
    _client->send();
  }
}

} // namespace wifi_serial
} // namespace esphome
