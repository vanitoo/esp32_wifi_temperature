# Runtime settings

Версия 0.5.0 добавляет настройки порогов и задержек без перекомпиляции ESPHome.

После прошивки откройте:

```text
http://esp32-wifi-term.local/settings
```

или IP устройства + `/settings`.

Страница защищается той же Basic Auth, что и основной `web_server`.

## Параметры

- Warning temperature, default `6.0 °C`.
- Alarm temperature, default `8.0 °C`.
- Recovery temperature, default `5.0 °C`.
- Warning delay, default `600 s`.
- Alarm delay, default `300 s`.
- Recovery delay, default `300 s`.

Значения записываются через ESPHome preferences во Flash только при нажатии `Сохранить`, а не при каждом измерении температуры.

Проверка значений требует:

```text
recovery < warning < alarm
```

Максимальная задержка — 86400 секунд.

## Следующие этапы

- название устройства и магазина;
- включение/выключение NarodMon;
- MQTT/Zabbix transport;
- параметры доставки аварий;
- отдельная страница администратора;
- смена пароля и Wi-Fi из общего инженерного интерфейса.
