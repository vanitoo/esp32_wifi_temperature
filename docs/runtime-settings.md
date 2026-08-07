# Runtime settings

Версия 0.6.0 расширяет инженерную страницу `/settings`.

После прошивки откройте:

```text
http://esp32-wifi-term.local/settings
```

или IP устройства + `/settings`.

Страница защищается той же Basic Auth, что и основной `web_server`.

## Что меняется без перепрошивки

- название контролируемого объекта;
- место установки;
- warning temperature;
- alarm temperature;
- recovery temperature;
- warning delay;
- alarm delay;
- recovery delay;
- runtime log level: `NONE`, `ERROR`, `WARN`, `INFO`, `DEBUG`;
- включение/выключение отправки в Народный мониторинг;
- интервал Народного мониторинга: от 5 минут.

Значения записываются через ESPHome preferences во Flash только при нажатии `Сохранить`, а не при каждом измерении температуры.

Проверка температур требует:

```text
recovery < warning < alarm
```

Максимальная задержка — 86400 секунд.

## Название устройства и название объекта

Служебное ESPHome-имя/hostname остаётся стабильным:

```text
esp32-wifi-term
```

Это сделано специально, чтобы OTA, mDNS и API не ломались при переименовании витрины.

На странице `/settings` меняется `Название объекта`, например:

```text
Холодильная витрина №3
Холодильник напитков
Морозильный ларь №2
Склад молочной продукции
```

На главной странице этот параметр публикуется как сущность `Настроенный объект`.

## Log level

В бинарник компилируется максимум `DEBUG`. На работающем устройстве можно снизить уровень до `INFO`, `WARN`, `ERROR` или `NONE`, а затем вернуть `DEBUG` без OTA.

`VERBOSE` и `VERY_VERBOSE` намеренно не включены в production-бинарник. Чтобы они стали доступны без перепрошивки, прошивку пришлось бы компилировать с более высоким максимальным уровнем, что увеличивает размер и нагрузку.

## Что пока остаётся compile-time

Эти параметры намеренно не вынесены в runtime settings:

- GPIO и тип физического датчика;
- ESPHome hostname;
- OTA/API encryption keys;
- Basic Auth основного web server;
- fallback AP password;
- NarodMon owner/device ID;
- структура Flash/OTA partitions.

Wi-Fi credentials меняются штатным captive portal при недоступности сохранённой сети.

## Следующие этапы

- MQTT/Zabbix transport;
- параметры доставки аварий;
- runtime NarodMon owner/device ID;
- калибровка датчика;
- безопасная смена web/admin пароля;
- отдельная инженерная главная страница вместо штатного ESPHome UI.
