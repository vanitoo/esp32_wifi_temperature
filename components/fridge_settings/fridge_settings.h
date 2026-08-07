#pragma once

#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace fridge_settings {

struct RuntimeSettings {
  char object_name[64]{"Датчик температуры"};
  char location[64]{""};

  float warning_c{6.0f};
  float alarm_c{8.0f};
  float recovery_c{5.0f};
  uint32_t warning_delay_s{600};
  uint32_t alarm_delay_s{300};
  uint32_t recovery_delay_s{300};

  uint8_t log_level{4};  // ESPHOME_LOG_LEVEL_DEBUG
  uint8_t narodmon_enabled{1};
  uint16_t narodmon_interval_min{5};
};

class FridgeSettings : public Component, public AsyncWebHandler {
 public:
  void set_defaults(float warning_c, float alarm_c, float recovery_c,
                    uint32_t warning_delay_s, uint32_t alarm_delay_s,
                    uint32_t recovery_delay_s);

  void set_firmware_version_sensor(text_sensor::TextSensor *sensor) {
    firmware_version_sensor_ = sensor;
  }
  void set_object_name_sensor(text_sensor::TextSensor *sensor) {
    object_name_sensor_ = sensor;
  }
  void set_log_level_sensor(text_sensor::TextSensor *sensor) {
    log_level_sensor_ = sensor;
  }

  void setup() override;
  void dump_config() override;

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

  const char *object_name() const { return settings_.object_name; }
  const char *location() const { return settings_.location; }

  float warning_c() const { return settings_.warning_c; }
  float alarm_c() const { return settings_.alarm_c; }
  float recovery_c() const { return settings_.recovery_c; }
  uint32_t warning_delay_s() const { return settings_.warning_delay_s; }
  uint32_t alarm_delay_s() const { return settings_.alarm_delay_s; }
  uint32_t recovery_delay_s() const { return settings_.recovery_delay_s; }

  uint8_t log_level() const { return settings_.log_level; }
  bool narodmon_enabled() const { return settings_.narodmon_enabled != 0; }
  uint16_t narodmon_interval_min() const { return settings_.narodmon_interval_min; }

  bool consume_narodmon_due(uint32_t now_ms);

 protected:
  struct Persisted {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    RuntimeSettings settings;
  };

  void save_();
  void load_();
  void apply_runtime_settings_();
  void publish_runtime_state_();

  std::string settings_json_() const;
  std::string settings_page_() const;

  static void copy_string_(char *target, size_t target_size, const std::string &value);
  static std::string json_escape_(const char *value);
  static uint8_t parse_log_level_(const std::string &value, uint8_t fallback);
  static const char *log_level_name_(uint8_t level);

  RuntimeSettings settings_;
  RuntimeSettings defaults_;
  ESPPreferenceObject pref_;

  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  text_sensor::TextSensor *object_name_sensor_{nullptr};
  text_sensor::TextSensor *log_level_sensor_{nullptr};

  uint32_t last_narodmon_auto_send_ms_{0};
};

}  // namespace fridge_settings
}  // namespace esphome
