#pragma once
#include <algorithm>
#include <functional>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
namespace esphome {
template<typename T> using optional=std::optional<T>;
struct Component { virtual ~Component()=default; virtual void setup() {} virtual void loop() {} virtual void dump_config() {} virtual float get_setup_priority() const {return 0;} };
struct PollingComponent : Component {virtual void update() {}};
namespace setup_priority { constexpr float DATA=0, BUS=1; }
struct EntityBase { struct Name {std::string str() const {return {};}}; Name get_name() const {return {};} };
inline unsigned long fake_millis = 0;
inline unsigned long millis() {return fake_millis;} inline void delay(int ms) {fake_millis += ms;}
struct InternalGPIOPin {void setup() {} void digital_write(bool) {}};
template<typename T> class CallbackManager;
template<typename R,typename... A> class CallbackManager<R(A...)> {
 public:
  void add(std::function<R(A...)> &&f) { callbacks_.push_back(std::move(f)); }
  void call(A... args) { for (auto &f : callbacks_) f(args...); }
 private:
  std::vector<std::function<R(A...)>> callbacks_;
};
}
#define VERSION_CODE(a,b,c) 1
#define ESPHOME_VERSION_CODE 0
#define ESP_LOGCONFIG(...) do {} while(0)
#define ESP_LOGD(...) do {} while(0)
#define ESP_LOGI(...) do {} while(0)
#define ESP_LOGV(...) do {} while(0)
#define ESP_LOGW(...) do {} while(0)
#define ESP_LOGE(...) do {} while(0)
#define YESNO(x) ((x) ? "YES" : "NO")
