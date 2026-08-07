#pragma once

#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace fridge_settings {

struct RuntimeSettings {
  float warning_c{6.0f};
  float alarm_c{8.0f};
  float recovery_c{5.0f};
  uint32_t warning_delay_s{600};
  uint32_t alarm_delay_s{300};
  uint32_t recovery_delay_s{300};
};

class FridgeSettings : public Component, public AsyncWebHandler {
 public:
  void set_defaults(float warning_c, float alarm_c, float recovery_c,
                    uint32_t warning_delay_s, uint32_t alarm_delay_s,
                    uint32_t recovery_delay_s);

  void setup() override;
  void dump_config() override;

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

  float warning_c() const { return settings_.warning_c; }
  float alarm_c() const { return settings_.alarm_c; }
  float recovery_c() const { return settings_.recovery_c; }
  uint32_t warning_delay_s() const { return settings_.warning_delay_s; }
  uint32_t alarm_delay_s() const { return settings_.alarm_delay_s; }
  uint32_t recovery_delay_s() const { return settings_.recovery_delay_s; }

 protected:
  struct Persisted {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    RuntimeSettings settings;
  };

  void save_();
  void load_();
  std::string settings_json_() const;
  std::string settings_page_() const;

  RuntimeSettings settings_;
  RuntimeSettings defaults_;
  ESPPreferenceObject pref_;
};

}  // namespace fridge_settings
}  // namespace esphome
