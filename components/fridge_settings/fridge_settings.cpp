#include "fridge_settings.h"

#include <cstdio>
#include <cstdlib>

#include "esphome/core/log.h"

namespace esphome {
namespace fridge_settings {

static const char *const TAG = "fridge_settings";
static constexpr uint32_t SETTINGS_MAGIC = 0x46524731;  // FRG1
static constexpr uint16_t SETTINGS_VERSION = 1;

void FridgeSettings::set_defaults(float warning_c, float alarm_c, float recovery_c,
                                  uint32_t warning_delay_s, uint32_t alarm_delay_s,
                                  uint32_t recovery_delay_s) {
  defaults_.warning_c = warning_c;
  defaults_.alarm_c = alarm_c;
  defaults_.recovery_c = recovery_c;
  defaults_.warning_delay_s = warning_delay_s;
  defaults_.alarm_delay_s = alarm_delay_s;
  defaults_.recovery_delay_s = recovery_delay_s;
}

void FridgeSettings::setup() {
  settings_ = defaults_;
  pref_ = global_preferences->make_preference<Persisted>(0xA87311D4);
  load_();

  if (web_server_base::global_web_server_base != nullptr) {
    web_server_base::global_web_server_base->add_handler(this);
  } else {
    ESP_LOGE(TAG, "web_server_base unavailable");
    mark_failed();
  }
}

void FridgeSettings::dump_config() {
  ESP_LOGCONFIG(TAG, "Fridge runtime settings:");
  ESP_LOGCONFIG(TAG, "  Warning: %.1f C / %u s", settings_.warning_c,
                (unsigned) settings_.warning_delay_s);
  ESP_LOGCONFIG(TAG, "  Alarm: %.1f C / %u s", settings_.alarm_c,
                (unsigned) settings_.alarm_delay_s);
  ESP_LOGCONFIG(TAG, "  Recovery: %.1f C / %u s", settings_.recovery_c,
                (unsigned) settings_.recovery_delay_s);
  ESP_LOGCONFIG(TAG, "  Settings page: /settings");
}

void FridgeSettings::save_() {
  Persisted p{};
  p.magic = SETTINGS_MAGIC;
  p.version = SETTINGS_VERSION;
  p.settings = settings_;
  if (!pref_.save(&p)) ESP_LOGW(TAG, "Failed to persist settings");
}

void FridgeSettings::load_() {
  Persisted p{};
  if (pref_.load(&p) && p.magic == SETTINGS_MAGIC && p.version == SETTINGS_VERSION) {
    settings_ = p.settings;
  }
}

bool FridgeSettings::canHandle(AsyncWebServerRequest *request) const {
  const std::string url = request->url();
  if (request->method() == HTTP_GET)
    return url == "/settings" || url == "/settings/" || url == "/settings.json";
  if (request->method() == HTTP_POST)
    return url == "/settings/save" || url == "/settings/reset";
  return false;
}

void FridgeSettings::handleRequest(AsyncWebServerRequest *request) {
  const std::string url = request->url();

  if (request->method() == HTTP_GET) {
    if (url == "/settings.json") {
      const std::string body = settings_json_();
      request->send(200, "application/json; charset=utf-8", body.c_str());
      return;
    }
    const std::string body = settings_page_();
    request->send(200, "text/html; charset=utf-8", body.c_str());
    return;
  }

  if (url == "/settings/reset") {
    settings_ = defaults_;
    save_();
    request->send(200, "application/json; charset=utf-8", "{\"ok\":true}");
    return;
  }

  auto argf = [&](const char *name, float current) {
    if (!request->hasArg(name)) return current;
    return strtof(request->arg(name).c_str(), nullptr);
  };
  auto argu = [&](const char *name, uint32_t current) {
    if (!request->hasArg(name)) return current;
    long value = strtol(request->arg(name).c_str(), nullptr, 10);
    return value < 0 ? current : static_cast<uint32_t>(value);
  };

  RuntimeSettings next = settings_;
  next.warning_c = argf("warning_c", next.warning_c);
  next.alarm_c = argf("alarm_c", next.alarm_c);
  next.recovery_c = argf("recovery_c", next.recovery_c);
  next.warning_delay_s = argu("warning_delay_s", next.warning_delay_s);
  next.alarm_delay_s = argu("alarm_delay_s", next.alarm_delay_s);
  next.recovery_delay_s = argu("recovery_delay_s", next.recovery_delay_s);

  if (!(next.recovery_c < next.warning_c && next.warning_c < next.alarm_c) ||
      next.warning_delay_s > 86400 || next.alarm_delay_s > 86400 ||
      next.recovery_delay_s > 86400) {
    request->send(400, "application/json; charset=utf-8",
                  "{\"ok\":false,\"error\":\"invalid settings\"}");
    return;
  }

  settings_ = next;
  save_();
  ESP_LOGI(TAG, "Settings saved: warning %.1f, alarm %.1f, recovery %.1f",
           settings_.warning_c, settings_.alarm_c, settings_.recovery_c);
  request->send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

std::string FridgeSettings::settings_json_() const {
  char b[320];
  snprintf(b, sizeof(b),
           "{\"warning_c\":%.2f,\"alarm_c\":%.2f,\"recovery_c\":%.2f,"
           "\"warning_delay_s\":%u,\"alarm_delay_s\":%u,\"recovery_delay_s\":%u}",
           settings_.warning_c, settings_.alarm_c, settings_.recovery_c,
           (unsigned) settings_.warning_delay_s, (unsigned) settings_.alarm_delay_s,
           (unsigned) settings_.recovery_delay_s);
  return b;
}

std::string FridgeSettings::settings_page_() const {
  return R"HTML(<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Настройки витрины</title>
<style>body{font-family:system-ui;margin:0;background:#eef2f6;color:#17202a}main{max-width:700px;margin:auto;padding:18px}.card{background:white;padding:18px;border-radius:15px;margin:14px 0;box-shadow:0 2px 14px #0001}label{display:block;margin:12px 0 4px}input{width:100%;box-sizing:border-box;padding:10px;border:1px solid #ccd2d8;border-radius:8px}button{padding:10px 14px;border:0;border-radius:8px;margin:12px 8px 0 0;cursor:pointer}.primary{background:#155eef;color:#fff}.danger{background:#b42318;color:#fff}.msg{margin-top:12px}</style></head>
<body><main><h1>Настройки холодильной витрины</h1><a href="/">Основной портал</a> · <a href="/history">История</a>
<div class="card"><form id="f"><h2>Температура</h2>
<label>Предупреждение, °C</label><input name="warning_c" type="number" step="0.1">
<label>Авария, °C</label><input name="alarm_c" type="number" step="0.1">
<label>Возврат в норму ниже, °C</label><input name="recovery_c" type="number" step="0.1">
<h2>Задержки</h2><label>Предупреждение, секунд</label><input name="warning_delay_s" type="number" min="0">
<label>Авария, секунд</label><input name="alarm_delay_s" type="number" min="0">
<label>Восстановление, секунд</label><input name="recovery_delay_s" type="number" min="0">
<button class="primary" type="submit">Сохранить</button><button class="danger" type="button" onclick="resetCfg()">Заводские значения</button></form><div class="msg" id="msg"></div></div></main>
<script>const f=document.getElementById('f');async function load(){const c=await fetch('/settings.json',{cache:'no-store'}).then(r=>r.json());Object.keys(c).forEach(k=>{if(f.elements[k])f.elements[k].value=c[k]})}f.onsubmit=async e=>{e.preventDefault();const d=new URLSearchParams(new FormData(f));const r=await fetch('/settings/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:d});msg.textContent=r.ok?'Сохранено. Новые пороги применяются без перепрошивки.':'Ошибка: '+await r.text();if(r.ok)load()};async function resetCfg(){if(!confirm('Вернуть заводские значения?'))return;await fetch('/settings/reset',{method:'POST'});await load();msg.textContent='Заводские значения восстановлены.'}load()</script></body></html>)HTML";
}

}  // namespace fridge_settings
}  // namespace esphome
