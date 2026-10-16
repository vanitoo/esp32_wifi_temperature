#include "fridge_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace fridge_settings {

static const char *const TAG = "fridge_settings";
static constexpr uint32_t SETTINGS_MAGIC = 0x46524731;  // FRG1
static constexpr uint16_t SETTINGS_VERSION = 2;

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
  apply_runtime_settings_();
  publish_runtime_state_();

  if (web_server_base::global_web_server_base != nullptr) {
    web_server_base::global_web_server_base->add_handler(this);
  } else {
    ESP_LOGE(TAG, "web_server_base unavailable");
    mark_failed();
  }
}

void FridgeSettings::dump_config() {
  ESP_LOGCONFIG(TAG, "Runtime settings:");
  ESP_LOGCONFIG(TAG, "  Firmware: %s", ESPHOME_PROJECT_VERSION);
  ESP_LOGCONFIG(TAG, "  Object: %s", settings_.object_name);
  ESP_LOGCONFIG(TAG, "  Location: %s", settings_.location);
  ESP_LOGCONFIG(TAG, "  Warning: %.1f C / %u s", settings_.warning_c,
                (unsigned) settings_.warning_delay_s);
  ESP_LOGCONFIG(TAG, "  Alarm: %.1f C / %u s", settings_.alarm_c,
                (unsigned) settings_.alarm_delay_s);
  ESP_LOGCONFIG(TAG, "  Recovery: %.1f C / %u s", settings_.recovery_c,
                (unsigned) settings_.recovery_delay_s);
  ESP_LOGCONFIG(TAG, "  Log level: %s", log_level_name_(settings_.log_level));
  ESP_LOGCONFIG(TAG, "  NarodMon: %s / %u min",
                settings_.narodmon_enabled ? "enabled" : "disabled",
                (unsigned) settings_.narodmon_interval_min);
  ESP_LOGCONFIG(TAG, "  Settings page: /settings");
}

void FridgeSettings::save_() {
  Persisted p{};
  p.magic = SETTINGS_MAGIC;
  p.version = SETTINGS_VERSION;
  p.settings = settings_;
  if (!pref_.save(&p))
    ESP_LOGW(TAG, "Failed to persist settings");
}

void FridgeSettings::load_() {
  Persisted p{};
  if (pref_.load(&p) && p.magic == SETTINGS_MAGIC &&
      p.version == SETTINGS_VERSION) {
    settings_ = p.settings;
  }

  settings_.object_name[sizeof(settings_.object_name) - 1] = '\0';
  settings_.location[sizeof(settings_.location) - 1] = '\0';

  if (settings_.object_name[0] == '\0')
    copy_string_(settings_.object_name, sizeof(settings_.object_name),
                 "Датчик температуры");

  if (settings_.narodmon_interval_min < 5)
    settings_.narodmon_interval_min = 5;
}

void FridgeSettings::apply_runtime_settings_() {
  if (logger::global_logger != nullptr)
    logger::global_logger->set_log_level(settings_.log_level);
}

void FridgeSettings::publish_runtime_state_() {
  if (firmware_version_sensor_ != nullptr)
    firmware_version_sensor_->publish_state(ESPHOME_PROJECT_VERSION);

  if (object_name_sensor_ != nullptr)
    object_name_sensor_->publish_state(settings_.object_name);

  if (log_level_sensor_ != nullptr)
    log_level_sensor_->publish_state(log_level_name_(settings_.log_level));
}

bool FridgeSettings::consume_narodmon_due(uint32_t now_ms) {
  if (!narodmon_enabled())
    return false;

  const uint32_t period_ms =
      static_cast<uint32_t>(settings_.narodmon_interval_min) * 60UL * 1000UL;

  if (last_narodmon_auto_send_ms_ == 0 ||
      static_cast<uint32_t>(now_ms - last_narodmon_auto_send_ms_) >= period_ms) {
    last_narodmon_auto_send_ms_ = now_ms;
    return true;
  }

  return false;
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
    apply_runtime_settings_();
    publish_runtime_state_();
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

  if (request->hasArg("object_name"))
    copy_string_(next.object_name, sizeof(next.object_name), request->arg("object_name"));
  if (request->hasArg("location"))
    copy_string_(next.location, sizeof(next.location), request->arg("location"));

  next.warning_c = argf("warning_c", next.warning_c);
  next.alarm_c = argf("alarm_c", next.alarm_c);
  next.recovery_c = argf("recovery_c", next.recovery_c);
  next.warning_delay_s = argu("warning_delay_s", next.warning_delay_s);
  next.alarm_delay_s = argu("alarm_delay_s", next.alarm_delay_s);
  next.recovery_delay_s = argu("recovery_delay_s", next.recovery_delay_s);

  if (request->hasArg("log_level"))
    next.log_level = parse_log_level_(request->arg("log_level"), next.log_level);

  next.narodmon_enabled = request->hasArg("narodmon_enabled") ? 1 : 0;
  next.narodmon_interval_min = static_cast<uint16_t>(
      argu("narodmon_interval_min", next.narodmon_interval_min));

  const size_t object_len = strlen(next.object_name);
  const size_t location_len = strlen(next.location);

  if (object_len == 0 || object_len >= sizeof(next.object_name) ||
      location_len >= sizeof(next.location) ||
      !(next.recovery_c < next.warning_c && next.warning_c < next.alarm_c) ||
      next.warning_delay_s > 86400 || next.alarm_delay_s > 86400 ||
      next.recovery_delay_s > 86400 ||
      next.narodmon_interval_min < 5 || next.narodmon_interval_min > 1440) {
    request->send(400, "application/json; charset=utf-8",
                  "{\"ok\":false,\"error\":\"invalid settings\"}");
    return;
  }

  settings_ = next;
  save_();
  apply_runtime_settings_();
  publish_runtime_state_();
  last_narodmon_auto_send_ms_ = 0;

  ESP_LOGI(TAG, "Settings saved for '%s'", settings_.object_name);
  request->send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

void FridgeSettings::copy_string_(char *target, size_t target_size,
                                  const std::string &value) {
  if (target_size == 0)
    return;
  snprintf(target, target_size, "%s", value.c_str());
  target[target_size - 1] = '\0';
}

std::string FridgeSettings::json_escape_(const char *value) {
  std::string out;
  if (value == nullptr)
    return out;

  for (const char *p = value; *p != '\0'; ++p) {
    switch (*p) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += *p; break;
    }
  }
  return out;
}

uint8_t FridgeSettings::parse_log_level_(const std::string &value,
                                         uint8_t fallback) {
  if (value == "NONE") return ESPHOME_LOG_LEVEL_NONE;
  if (value == "ERROR") return ESPHOME_LOG_LEVEL_ERROR;
  if (value == "WARN") return ESPHOME_LOG_LEVEL_WARN;
  if (value == "INFO") return ESPHOME_LOG_LEVEL_INFO;
  if (value == "DEBUG") return ESPHOME_LOG_LEVEL_DEBUG;
  return fallback;
}

const char *FridgeSettings::log_level_name_(uint8_t level) {
  switch (level) {
    case ESPHOME_LOG_LEVEL_NONE: return "NONE";
    case ESPHOME_LOG_LEVEL_ERROR: return "ERROR";
    case ESPHOME_LOG_LEVEL_WARN: return "WARN";
    case ESPHOME_LOG_LEVEL_INFO: return "INFO";
    case ESPHOME_LOG_LEVEL_DEBUG: return "DEBUG";
    default: return "DEBUG";
  }
}

std::string FridgeSettings::settings_json_() const {
  std::string out;
  out.reserve(600);
  out += "{\"firmware_version\":\"";
  out += ESPHOME_PROJECT_VERSION;
  out += "\",\"object_name\":\"";
  out += json_escape_(settings_.object_name);
  out += "\",\"location\":\"";
  out += json_escape_(settings_.location);
  out += "\",\"warning_c\":" + std::to_string(settings_.warning_c);
  out += ",\"alarm_c\":" + std::to_string(settings_.alarm_c);
  out += ",\"recovery_c\":" + std::to_string(settings_.recovery_c);
  out += ",\"warning_delay_s\":" + std::to_string(settings_.warning_delay_s);
  out += ",\"alarm_delay_s\":" + std::to_string(settings_.alarm_delay_s);
  out += ",\"recovery_delay_s\":" + std::to_string(settings_.recovery_delay_s);
  out += ",\"log_level\":\"";
  out += log_level_name_(settings_.log_level);
  out += "\",\"narodmon_enabled\":";
  out += settings_.narodmon_enabled ? "true" : "false";
  out += ",\"narodmon_interval_min\":" +
         std::to_string(settings_.narodmon_interval_min);
  out += "}";
  return out;
}

std::string FridgeSettings::settings_page_() const {
  return R"HTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Настройки датчика</title>
<style>
:root{font-family:system-ui,-apple-system,sans-serif;color-scheme:light dark}
body{margin:0;background:#eef2f6;color:#17202a}main{max-width:760px;margin:auto;padding:18px}
.card{background:#fff;padding:18px;border-radius:15px;margin:14px 0;box-shadow:0 2px 14px #0001}
h1,h2{margin:0 0 12px}label{display:block;margin:12px 0 4px;font-weight:600}
input,select{width:100%;box-sizing:border-box;padding:10px;border:1px solid #ccd2d8;border-radius:8px;background:transparent;color:inherit}
.row{display:grid;grid-template-columns:1fr 1fr;gap:14px}.check{display:flex;align-items:center;gap:9px;font-weight:400}.check input{width:auto}
button{padding:10px 14px;border:0;border-radius:8px;margin:12px 8px 0 0;cursor:pointer}.primary{background:#155eef;color:#fff}.danger{background:#b42318;color:#fff}
.msg,.hint{margin-top:12px;color:#667085}.version{font-family:ui-monospace,Consolas,monospace}
a{color:#155eef}@media(max-width:600px){.row{grid-template-columns:1fr}}
@media(prefers-color-scheme:dark){body{background:#101418;color:#edf2f7}.card{background:#1b222a}input,select{border-color:#475467}}
.site-nav{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0 20px}.site-nav a{display:inline-block;padding:11px 16px;border-radius:10px;text-decoration:none;background:#25334a;color:#fff;font-weight:600}.site-nav a:hover{background:#155eef}.site-nav a[href="/settings"]{background:#155eef;color:#fff}</style></head>
<body><main>
<h1>Настройки датчика температуры</h1><p>Администрирование · изменение параметров устройства</p>
<nav class="site-nav"><a href="/">Мониторинг</a><a href="/history">История</a><a href="/settings">⚙ Настройки</a></nav>
<form id="f">
<div class="card"><h2>Объект</h2>
<label>Название объекта</label><input name="object_name" maxlength="63" placeholder="Например: Холодильная витрина №1">
<label>Место установки</label><input name="location" maxlength="63" placeholder="Например: Магазин 12, молочный отдел">
<div class="hint">Название ESPHome/hostname остаётся служебным. Это поле описывает, что именно контролирует датчик.</div></div>

<div class="card"><h2>Температурные аварии</h2>
<div class="row"><div><label>Предупреждение, °C</label><input name="warning_c" type="number" step="0.1"></div>
<div><label>Авария, °C</label><input name="alarm_c" type="number" step="0.1"></div></div>
<label>Возврат в норму ниже, °C</label><input name="recovery_c" type="number" step="0.1">
<div class="row"><div><label>Задержка предупреждения, сек</label><input name="warning_delay_s" type="number" min="0" max="86400"></div>
<div><label>Задержка аварии, сек</label><input name="alarm_delay_s" type="number" min="0" max="86400"></div></div>
<label>Задержка восстановления, сек</label><input name="recovery_delay_s" type="number" min="0" max="86400"></div>

<div class="card"><h2>Передача данных</h2>
<label class="check"><input name="narodmon_enabled" type="checkbox"> Отправлять данные в Народный мониторинг</label>
<label>Интервал отправки</label><select name="narodmon_interval_min">
<option value="5">5 минут</option><option value="10">10 минут</option><option value="15">15 минут</option>
<option value="30">30 минут</option><option value="60">1 час</option></select>
<div class="hint">ID/OWNER пока остаются служебными параметрами прошивки. Их перенесём в runtime-настройки после стабилизации отправки.</div></div>

<div class="card"><h2>Диагностика</h2>
<label>Уровень логирования</label><select name="log_level">
<option>NONE</option><option>ERROR</option><option>WARN</option><option>INFO</option><option>DEBUG</option></select>
<div class="hint">DEBUG — максимум, скомпилированный в эту прошивку. INFO/WARN/ERROR уменьшают объём логов и нагрузку.</div>
<label>Версия прошивки</label><div id="fw" class="version">—</div></div>

<div class="card"><h2>Wi‑Fi</h2>
<div>Сеть Wi‑Fi меняется через fallback portal. Если сохранённая сеть недоступна, устройство поднимет точку <b>Fridge-A87C21</b>.</div>
<div class="hint">Служебные параметры, которые требуют перезагрузки стека или влияют на безопасность (hostname, GPIO, OTA/API ключи, пароль web-портала), пока остаются в прошивке.</div></div>

<button class="primary" type="submit">Сохранить настройки</button>
<button class="danger" type="button" onclick="resetCfg()">Заводские значения</button>
<div class="msg" id="msg"></div>
</form>
<script>
const f=document.getElementById('f'),msg=document.getElementById('msg');
async function load(){
 const c=await fetch('/settings.json',{cache:'no-store'}).then(r=>r.json());
 for(const k of ['object_name','location','warning_c','alarm_c','recovery_c','warning_delay_s','alarm_delay_s','recovery_delay_s','log_level','narodmon_interval_min'])
   if(f.elements[k]) f.elements[k].value=c[k];
 f.elements.narodmon_enabled.checked=!!c.narodmon_enabled;
 fw.textContent=c.firmware_version||'—';
}
f.onsubmit=async e=>{
 e.preventDefault();
 const d=new URLSearchParams(new FormData(f));
 const r=await fetch('/settings/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:d});
 msg.textContent=r.ok?'Сохранено. Настройки уже применены.':'Ошибка: '+await r.text();
 if(r.ok) await load();
};
async function resetCfg(){
 if(!confirm('Вернуть заводские значения?'))return;
 const r=await fetch('/settings/reset',{method:'POST'});
 if(r.ok){await load();msg.textContent='Заводские значения восстановлены.'}
}
load();
</script></main></body></html>)HTML";
}

}  // namespace fridge_settings
}  // namespace esphome
