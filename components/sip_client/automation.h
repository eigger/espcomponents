#pragma once
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "sip_client.h"

namespace esphome {
namespace sip_client {

class RegisteredTrigger : public Trigger<> {
 public:
  explicit RegisteredTrigger(SipClient *parent) {
    parent->add_on_registered_callback([this]() { this->trigger(); });
  }
};

class IncomingCallTrigger : public Trigger<std::string> {
 public:
  explicit IncomingCallTrigger(SipClient *parent) {
    parent->add_on_incoming_call_callback([this](std::string caller) { this->trigger(caller); });
  }
};

class CallConnectedTrigger : public Trigger<> {
 public:
  explicit CallConnectedTrigger(SipClient *parent) {
    parent->add_on_call_connected_callback([this]() { this->trigger(); });
  }
};

class CallEndedTrigger : public Trigger<> {
 public:
  explicit CallEndedTrigger(SipClient *parent) {
    parent->add_on_call_ended_callback([this]() { this->trigger(); });
  }
};

class DtmfTrigger : public Trigger<std::string> {
 public:
  explicit DtmfTrigger(SipClient *parent) {
    parent->add_on_dtmf_callback([this](std::string digit) { this->trigger(digit); });
  }
};

template<typename... Ts> class CallAction : public Action<Ts...> {
 public:
  explicit CallAction(SipClient *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, number)
  void play(const Ts &...x) override { this->parent_->call(this->number_.value(x...)); }

 protected:
  SipClient *parent_;
};

template<typename... Ts> class AnswerAction : public Action<Ts...> {
 public:
  explicit AnswerAction(SipClient *parent) : parent_(parent) {}
  void play(const Ts &...) override { this->parent_->answer(); }

 protected:
  SipClient *parent_;
};

template<typename... Ts> class HangupAction : public Action<Ts...> {
 public:
  explicit HangupAction(SipClient *parent) : parent_(parent) {}
  void play(const Ts &...) override { this->parent_->hangup(); }

 protected:
  SipClient *parent_;
};

template<typename... Ts> class SendDtmfAction : public Action<Ts...> {
 public:
  explicit SendDtmfAction(SipClient *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, digits)
  void play(const Ts &...x) override { this->parent_->send_dtmf(this->digits_.value(x...)); }

 protected:
  SipClient *parent_;
};

}  // namespace sip_client
}  // namespace esphome
