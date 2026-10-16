#include "temperature_history.h"

#include <algorithm>
#include <cstdio>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace temperature_history {

static const char *const TAG = "temperature_history";

void TemperatureHistory::setup() {
  this->history_.reserve(this->capacity_);
  this->events_.reserve(this->max_events_);
  this->logs_.reserve(this->max_log_lines_);

  if (this->sensor_ != nullptr) {
    this->sensor_->add_on_state_callback([this](float value) {
      if (std::isfinite(value)) {
        this->latest_value_ = value;
        this->latest_valid_ = true;
      }
    });
  }

  if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_callback(
        this,
        [](void *self, uint8_t level, const char *tag,
           const char *message, size_t message_len) {
          static_cast<TemperatureHistory *>(self)->on_log_(
              level, tag, message, message_len);
        });
  }

  if (web_server_base::global_web_server_base != nullptr) {
    // Public read-only telemetry. Settings and ESPHome commands retain web_server auth.
    web_server_base::global_web_server_base->add_handler_without_auth(this);
  } else {
    ESP_LOGE(TAG, "web_server_base is unavailable");
    this->mark_failed();
    return;
  }

  this->add_event("boot", "Компонент истории запущен");
}

void TemperatureHistory::dump_config() {
  ESP_LOGCONFIG(TAG, "Temperature history:");
  ESP_LOGCONFIG(TAG, "  Temperature points: %u", static_cast<unsigned>(this->capacity_));
  ESP_LOGCONFIG(TAG, "  Sample interval: %u ms",
                static_cast<unsigned>(this->sample_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Event records: %u", static_cast<unsigned>(this->max_events_));
  ESP_LOGCONFIG(TAG, "  Debug log lines: %u", static_cast<unsigned>(this->max_log_lines_));
  ESP_LOGCONFIG(TAG, "  Web page: /history");
}

void TemperatureHistory::loop() {
  const uint64_t now = millis_64();

  if (!this->latest_valid_)
    return;

  if (this->last_sample_ms_ != 0 &&
      now - this->last_sample_ms_ < this->sample_interval_ms_)
    return;

  this->last_sample_ms_ = now;
  this->add_sample_(this->latest_value_);
}

void TemperatureHistory::add_sample_(float value) {
  const HistoryPoint point{millis_64(), value};

  if (this->history_.size() < this->capacity_) {
    this->history_.push_back(point);
    if (this->history_.size() == this->capacity_) {
      this->history_full_ = true;
      this->history_write_index_ = 0;
    }
    return;
  }

  this->history_[this->history_write_index_] = point;
  this->history_write_index_ =
      (this->history_write_index_ + 1) % this->capacity_;
  this->history_full_ = true;
}

void TemperatureHistory::add_event(const std::string &type,
                                   const std::string &message) {
  if (this->events_.size() >= this->max_events_)
    this->events_.erase(this->events_.begin());

  this->events_.push_back(EventPoint{millis_64(), type, message});
  ESP_LOGI(TAG, "[%s] %s", type.c_str(), message.c_str());
}

void TemperatureHistory::on_log_(uint8_t level, const char *tag,
                                 const char *message, size_t message_len) {
  // Never log from this callback: doing so would recursively call this method.
  if (message == nullptr)
    return;

  size_t length = std::min(message_len, this->max_log_line_length_);
  std::string clean_message(message, length);

  while (!clean_message.empty() &&
         (clean_message.back() == '\r' || clean_message.back() == '\n'))
    clean_message.pop_back();

  LogPoint point{
      millis_64(),
      level,
      tag != nullptr ? std::string(tag) : std::string(),
      std::move(clean_message),
  };

  if (this->logs_.size() < this->max_log_lines_) {
    this->logs_.push_back(std::move(point));
    if (this->logs_.size() == this->max_log_lines_) {
      this->logs_full_ = true;
      this->log_write_index_ = 0;
    }
    return;
  }

  this->logs_[this->log_write_index_] = std::move(point);
  this->log_write_index_ =
      (this->log_write_index_ + 1) % this->max_log_lines_;
  this->logs_full_ = true;
}

void TemperatureHistory::clear_history() {
  this->history_.clear();
  this->history_write_index_ = 0;
  this->history_full_ = false;
  this->last_sample_ms_ = 0;
}

void TemperatureHistory::clear_events() {
  this->events_.clear();
}

void TemperatureHistory::clear_logs() {
  this->logs_.clear();
  this->log_write_index_ = 0;
  this->logs_full_ = false;
}

bool TemperatureHistory::canHandle(AsyncWebServerRequest *request) const {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const auto url = request->url_to(url_buf);

  if (request->method() == HTTP_GET) {
    return url == "/" || url == "/monitor" || url == "/monitor/" ||
           url == "/history" ||
           url == "/history/" ||
           url == "/history.json" ||
           url == "/events.json" ||
           url == "/logs.json";
  }

  // Never expose mutation endpoints from an unauthenticated handler.

  return false;
}

void TemperatureHistory::handleRequest(AsyncWebServerRequest *request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const auto url = request->url_to(url_buf);

  if (url == "/" || url == "/monitor" || url == "/monitor/") {
    const std::string body = R"PAGE(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Датчик температуры</title><style>
:root{font-family:system-ui,sans-serif;color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:#0b1120;color:#e5edf8}main{max-width:1080px;margin:auto;padding:24px}header{display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap}h1{font-size:clamp(24px,4vw,36px);margin:0}p{color:#94a3b8}nav{display:flex;gap:8px;flex-wrap:wrap;margin:24px 0}nav a{color:#e2e8f0;text-decoration:none;background:#1e293b;padding:12px 18px;border-radius:12px}nav a.active{background:#2563eb}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:16px}.card{background:#151f31;border:1px solid #334155;border-radius:18px;padding:24px;min-width:0}.label{color:#94a3b8;font-size:14px}.value{font-size:clamp(40px,8vw,72px);font-weight:750;margin:10px 0}.ok{color:#34d399}.warn{color:#fbbf24}.alarm{color:#fb7185}.events{margin:0;padding-left:20px;line-height:2}.muted{color:#94a3b8}footer{margin-top:25px;color:#94a3b8;font-size:13px}@media(max-width:600px){main{padding:14px}.card{padding:18px}}
</style></head><body><main><header><div><h1>Датчик температуры</h1><p>Мониторинг в реальном времени · без пароля</p></div><span id="online" class="warn">Подключение…</span></header><nav><a class="active" href="/">Мониторинг</a><a href="/history">История</a><a href="/settings">Настройки</a><a href="/info">Информация</a></nav><section class="grid"><article class="card"><div class="label">Текущая температура</div><div id="temp" class="value">—</div><div id="status" class="muted">Ожидание измерений</div></article><article class="card"><div class="label">Записано измерений</div><div id="count" class="value">—</div><div class="muted">Буфер до 1000 точек, до перезагрузки</div></article></section><section class="card" style="margin-top:16px"><h2>Последние события</h2><ul id="events" class="events"><li>Загрузка…</li></ul></section><footer>Автоматическое обновление каждые 15 секунд · Управление и обновление прошивки требуют пароль администратора.</footer></main><script>
const $=id=>document.getElementById(id);
async function refresh(){try{const [h,e]=await Promise.all([fetch('/history.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error(r.status);return r.json()}),fetch('/events.json',{cache:'no-store'}).then(r=>r.json())]);const pts=h.points||[];const p=pts.at(-1);$('temp').textContent=p?Number(p.value).toFixed(2)+' °C':'—';$('count').textContent=pts.length+' / '+h.capacity;$('online').textContent='● На связи';$('online').className='ok';$('status').textContent=p?'Последнее сохранённое измерение':'Ожидание датчика';const ul=$('events');ul.replaceChildren();for(const item of (e.events||[]).slice(-8).reverse()){const li=document.createElement('li');li.textContent=item.type+': '+item.message;ul.append(li)}if(!ul.children.length){const li=document.createElement('li');li.textContent='Событий пока нет';ul.append(li)}}catch(err){$('online').textContent='● Нет связи';$('online').className='alarm';$('status').textContent='Не удалось получить данные'}}refresh();setInterval(refresh,15000);
</script></body></html>)PAGE";
    request->send(200, "text/html; charset=utf-8", body.c_str());
    return;
  }

  if (url == "/history.json") {
    const std::string body = this->history_json_();
    request->send(200, "application/json; charset=utf-8", body.c_str());
    return;
  }

  if (url == "/events.json") {
    const std::string body = this->events_json_();
    request->send(200, "application/json; charset=utf-8", body.c_str());
    return;
  }

  if (url == "/logs.json") {
    const std::string body = this->logs_json_();
    request->send(200, "application/json; charset=utf-8", body.c_str());
    return;
  }

  const std::string body = this->page_html_();
  request->send(200, "text/html; charset=utf-8", body.c_str());
}

std::string TemperatureHistory::json_escape_(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);

  for (const char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20)
          out += c;
        break;
    }
  }
  return out;
}

const char *TemperatureHistory::level_name_(uint8_t level) {
  // ESPHome's logger values are ordered NONE, ERROR, WARN, INFO, DEBUG, VERBOSE...
  switch (level) {
    case 1: return "ERROR";
    case 2: return "WARN";
    case 3: return "INFO";
    case 4: return "DEBUG";
    case 5: return "VERBOSE";
    case 6: return "VERY_VERBOSE";
    default: return "UNKNOWN";
  }
}

std::string TemperatureHistory::history_json_() const {
  std::string out;
  out.reserve(128 + this->history_.size() * 40);
  out += "{\"capacity\":";
  out += std::to_string(this->capacity_);
  out += ",\"count\":";
  out += std::to_string(this->history_.size());
  out += ",\"sample_interval_ms\":";
  out += std::to_string(this->sample_interval_ms_);
  out += ",\"points\":[";

  const size_t count = this->history_.size();
  for (size_t logical = 0; logical < count; logical++) {
    const size_t physical = this->history_full_
                                ? (this->history_write_index_ + logical) % count
                                : logical;
    const auto &point = this->history_[physical];

    if (logical != 0)
      out += ",";

    char buffer[96];
    snprintf(buffer, sizeof(buffer),
             "{\"uptime_ms\":%llu,\"value\":%.3f}",
             static_cast<unsigned long long>(point.uptime_ms),
             point.value);
    out += buffer;
  }

  out += "]}";
  return out;
}

std::string TemperatureHistory::events_json_() const {
  std::string out;
  out.reserve(128 + this->events_.size() * 100);
  out += "{\"capacity\":";
  out += std::to_string(this->max_events_);
  out += ",\"count\":";
  out += std::to_string(this->events_.size());
  out += ",\"events\":[";

  for (size_t i = 0; i < this->events_.size(); i++) {
    const auto &event = this->events_[i];

    if (i != 0)
      out += ",";

    out += "{\"uptime_ms\":";
    out += std::to_string(event.uptime_ms);
    out += ",\"type\":\"";
    out += json_escape_(event.type);
    out += "\",\"message\":\"";
    out += json_escape_(event.message);
    out += "\"}";
  }

  out += "]}";
  return out;
}

std::string TemperatureHistory::logs_json_() const {
  std::string out;
  out.reserve(128 + this->logs_.size() * 160);
  out += "{\"capacity\":";
  out += std::to_string(this->max_log_lines_);
  out += ",\"count\":";
  out += std::to_string(this->logs_.size());
  out += ",\"logs\":[";

  const size_t count = this->logs_.size();
  for (size_t logical = 0; logical < count; logical++) {
    const size_t physical = this->logs_full_
                                ? (this->log_write_index_ + logical) % count
                                : logical;
    const auto &line = this->logs_[physical];

    if (logical != 0)
      out += ",";

    out += "{\"uptime_ms\":";
    out += std::to_string(line.uptime_ms);
    out += ",\"level\":\"";
    out += level_name_(line.level);
    out += "\",\"tag\":\"";
    out += json_escape_(line.tag);
    out += "\",\"message\":\"";
    out += json_escape_(line.message);
    out += "\"}";
  }

  out += "]}";
  return out;
}

std::string TemperatureHistory::page_html_() const {
  return R"HTML(
<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>История холодильной витрины</title>
<style>
:root{font-family:system-ui,-apple-system,sans-serif;color-scheme:light dark}
body{margin:0;background:#eef2f6;color:#17202a}
main{max-width:1100px;margin:auto;padding:18px}
.card{background:#fff;padding:18px;margin:14px 0;border-radius:16px;box-shadow:0 3px 18px #00000012}
header{display:flex;justify-content:space-between;align-items:center;gap:15px;flex-wrap:wrap}
h1,h2{margin:0 0 12px}
a,button{color:#155eef}
.metrics{display:flex;gap:12px;flex-wrap:wrap}
.metric{background:#f5f7fa;border-radius:12px;padding:12px 16px;min-width:125px}
.metric b{display:block;font-size:1.55rem}
canvas{width:100%;height:360px;border-radius:10px;background:#fbfcfd}
.tabs{display:flex;gap:8px;flex-wrap:wrap}
.tabs button,.actions button{border:0;border-radius:9px;padding:10px 14px;cursor:pointer}
.tabs button.active{background:#155eef;color:#fff}
.panel{display:none}.panel.active{display:block}
table{width:100%;border-collapse:collapse}
td,th{text-align:left;padding:9px;border-bottom:1px solid #e5e7eb;vertical-align:top}
.log{font-family:ui-monospace,Consolas,monospace;font-size:.86rem;white-space:pre-wrap;word-break:break-word}
.ERROR{color:#d92d20}.WARN{color:#b54708}.INFO{color:#067647}.DEBUG{color:#175cd3}
.small{color:#667085;font-size:.9rem}
.actions{display:flex;justify-content:flex-end;margin-bottom:10px}
@media(prefers-color-scheme:dark){
 body{background:#101418;color:#edf2f7}.card{background:#1b222a}
 .metric,canvas{background:#111820}td,th{border-color:#34404c}
}
.site-nav{display:flex;gap:8px;flex-wrap:wrap;margin:12px 0 20px}.site-nav a{display:inline-block;padding:11px 16px;border-radius:10px;text-decoration:none;background:#25334a;color:#fff;font-weight:600}.site-nav a:hover{background:#155eef}.site-nav a[href="/history"]{background:#155eef;color:#fff}
:root{color-scheme:dark}*{box-sizing:border-box}body{background:#0b1120;color:#e5edf8}main{max-width:1080px;margin:auto;padding:24px}.card{background:#151f31;border:1px solid #334155;border-radius:18px;padding:24px;margin:16px 0;box-shadow:none}h1{font-size:clamp(24px,4vw,36px);margin:0}h2{font-size:1.25rem}.small{color:#94a3b8}.metric{background:#0f172a;color:#e5edf8}canvas{background:#0f172a}td,th{border-color:#334155}.site-nav{display:flex;gap:8px;flex-wrap:wrap;margin:24px 0}.site-nav a{color:#e2e8f0;text-decoration:none;background:#1e293b;padding:12px 18px;border-radius:12px;font-weight:400}.site-nav a[href="/history"]{background:#2563eb}a{color:#93c5fd}@media(max-width:600px){main{padding:14px}.card{padding:18px}}
</style>
</head>
<body>
<main>
<header>
<div><h1>История температуры</h1>
<div class="small">История хранится в RAM и очищается только после перезагрузки или вручную.</div></div>

</header>
<nav class="site-nav"><a href="/">Мониторинг</a><a href="/history">История</a><a href="/settings">Настройки</a><a href="/info">Информация</a></nav>
<section class="card">
<div class="metrics">
<div class="metric"><span>Последняя</span><b id="last">—</b></div>
<div class="metric"><span>Минимум</span><b id="min">—</b></div>
<div class="metric"><span>Максимум</span><b id="max">—</b></div>
<div class="metric"><span>Точек</span><b id="count">0</b></div>
<div class="metric"><span>Охват</span><b id="range">—</b></div>
<div class="metric"><span>Логов</span><b id="logCount">0</b></div>
</div>
</section>

<section class="card">
<div class="tabs">
<button class="active" data-panel="chartPanel">Температура</button>
<button data-panel="eventsPanel">События</button>
<button data-panel="logsPanel">Debug log</button>
</div>

<div id="chartPanel" class="panel active">
<div class="actions"><button onclick="clearData('temperature')">Очистить график</button></div>
<canvas id="chart"></canvas>
</div>

<div id="eventsPanel" class="panel">
<div class="actions"><button onclick="clearData('events')">Очистить события</button></div>
<table><thead><tr><th>Uptime</th><th>Тип</th><th>Событие</th></tr></thead>
<tbody id="events"></tbody></table>
</div>

<div id="logsPanel" class="panel">
<div class="actions"><button onclick="clearData('logs')">Очистить debug log</button></div>
<table><thead><tr><th>Uptime</th><th>Уровень</th><th>Tag</th><th>Сообщение</th></tr></thead>
<tbody id="logs"></tbody></table>
</div>
</section>
</main>

<script>
const canvas=document.getElementById('chart'),ctx=canvas.getContext('2d');

function duration(ms){
  const s=Math.floor(ms/1000),d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  if(d)return `${d} д ${h} ч`;
  if(h)return `${h} ч ${m} мин`;
  return `${m} мин`;
}
function uptime(ms){
  const s=Math.floor(ms/1000),d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60),sec=s%60;
  return `${d?d+'д ':''}${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
}
function esc(value){
  const node=document.createElement('div');node.textContent=value??'';return node.innerHTML;
}
function draw(points){
  const dpr=window.devicePixelRatio||1,rect=canvas.getBoundingClientRect();
  canvas.width=Math.max(320,Math.floor(rect.width*dpr));canvas.height=Math.floor(360*dpr);
  ctx.setTransform(dpr,0,0,dpr,0,0);
  const w=rect.width,h=360,p={l:55,r:18,t:18,b:35};
  ctx.clearRect(0,0,w,h);
  if(points.length<2){ctx.fillStyle=getComputedStyle(document.body).color;ctx.fillText('Недостаточно данных',20,35);return}
  const vals=points.map(x=>x.value);let lo=Math.min(...vals),hi=Math.max(...vals);
  if(hi-lo<1){lo-=.5;hi+=.5}
  const x=i=>p.l+i*(w-p.l-p.r)/(points.length-1),y=v=>p.t+(hi-v)*(h-p.t-p.b)/(hi-lo);
  ctx.strokeStyle=getComputedStyle(document.body).color+'33';ctx.fillStyle=getComputedStyle(document.body).color;
  ctx.font='12px system-ui';
  for(let i=0;i<=4;i++){const yy=p.t+i*(h-p.t-p.b)/4,value=hi-i*(hi-lo)/4;
    ctx.beginPath();ctx.moveTo(p.l,yy);ctx.lineTo(w-p.r,yy);ctx.stroke();ctx.fillText(value.toFixed(1)+'°',5,yy+4)}
  ctx.strokeStyle='#155eef';ctx.lineWidth=2;ctx.beginPath();
  points.forEach((q,i)=>i?ctx.lineTo(x(i),y(q.value)):ctx.moveTo(x(i),y(q.value)));ctx.stroke();
}
async function clearData(kind){
  alert("Очистка данных доступна только администратору; публичный интерфейс работает в режиме чтения."); return;
  if(!confirm('Очистить данные в памяти?'))return;
  const path={temperature:'/history/clear-temperature',events:'/history/clear-events',logs:'/history/clear-logs'}[kind];
  await fetch(path,{method:'POST'});await update();
}
async function update(){
  const h=await fetch('/history.json',{cache:'no-store'}).then(r=>r.json()),p=h.points||[];
  count.textContent=`${p.length}/${h.capacity}`;
  if(p.length){const v=p.map(x=>x.value);last.textContent=p.at(-1).value.toFixed(1)+' °C';
    min.textContent=Math.min(...v).toFixed(1)+' °C';max.textContent=Math.max(...v).toFixed(1)+' °C';
    range.textContent=duration(p.at(-1).uptime_ms-p[0].uptime_ms)}
  else{last.textContent=min.textContent=max.textContent=range.textContent='—'}
  draw(p);

  const e=await fetch('/events.json',{cache:'no-store'}).then(r=>r.json());
  events.innerHTML=(e.events||[]).slice().reverse().map(x=>
    `<tr><td>${uptime(x.uptime_ms)}</td><td>${esc(x.type)}</td><td>${esc(x.message)}</td></tr>`).join('');

  const l=await fetch('/logs.json',{cache:'no-store'}).then(r=>r.json());
  logCount.textContent=`${l.count}/${l.capacity}`;
  logs.innerHTML=(l.logs||[]).slice().reverse().map(x=>
    `<tr class="log ${esc(x.level)}"><td>${uptime(x.uptime_ms)}</td><td>${esc(x.level)}</td><td>${esc(x.tag)}</td><td>${esc(x.message)}</td></tr>`).join('');
}
document.querySelectorAll('.tabs button').forEach(button=>button.onclick=()=>{
  document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('active'));
  document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));
  button.classList.add('active');document.getElementById(button.dataset.panel).classList.add('active');
});
window.addEventListener('resize',()=>draw([]));
update();setInterval(update,30000);
</script>
</body>
</html>
)HTML";
}

}  // namespace temperature_history
}  // namespace esphome
