#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace temperature_history {

struct HistoryPoint {
  uint64_t uptime_ms;
  float value;
};

struct EventPoint {
  uint64_t uptime_ms;
  std::string type;
  std::string message;
};

struct LogPoint {
  uint64_t uptime_ms;
  uint8_t level;
  std::string tag;
  std::string message;
};

class TemperatureHistory : public Component, public AsyncWebHandler {
 public:
  void set_sensor(sensor::Sensor *value) { this->sensor_ = value; }
  void set_capacity(size_t value) { this->capacity_ = value; }
  void set_sample_interval(uint32_t value) { this->sample_interval_ms_ = value; }
  void set_max_events(size_t value) { this->max_events_ = value; }
  void set_max_log_lines(size_t value) { this->max_log_lines_ = value; }
  void set_max_log_line_length(size_t value) { this->max_log_line_length_ = value; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

  void add_event(const std::string &type, const std::string &message);
  void clear_history();
  void clear_events();
  void clear_logs();

 protected:
  void add_sample_(float value);
  void on_log_(uint8_t level, const char *tag, const char *message, size_t message_len);

  std::string history_json_() const;
  std::string events_json_() const;
  std::string logs_json_() const;
  std::string page_html_() const;

  static std::string json_escape_(const std::string &value);
  static const char *level_name_(uint8_t level);

  sensor::Sensor *sensor_{nullptr};

  size_t capacity_{1000};
  uint32_t sample_interval_ms_{30000};
  size_t max_events_{100};
  size_t max_log_lines_{250};
  size_t max_log_line_length_{220};

  std::vector<HistoryPoint> history_;
  size_t history_write_index_{0};
  bool history_full_{false};

  std::vector<EventPoint> events_;

  std::vector<LogPoint> logs_;
  size_t log_write_index_{0};
  bool logs_full_{false};

  float latest_value_{NAN};
  bool latest_valid_{false};
  uint64_t last_sample_ms_{0};
};

}  // namespace temperature_history
}  // namespace esphome
