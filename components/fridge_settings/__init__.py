import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC

DEPENDENCIES = ["web_server_base", "preferences", "logger"]

CONF_WARNING_DEFAULT = "warning_default"
CONF_ALARM_DEFAULT = "alarm_default"
CONF_RECOVERY_DEFAULT = "recovery_default"
CONF_WARNING_DELAY_DEFAULT = "warning_delay_default"
CONF_ALARM_DELAY_DEFAULT = "alarm_delay_default"
CONF_RECOVERY_DELAY_DEFAULT = "recovery_delay_default"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_OBJECT_NAME = "object_name"
CONF_LOG_LEVEL = "log_level"

ns = cg.esphome_ns.namespace("fridge_settings")
FridgeSettings = ns.class_("FridgeSettings", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(FridgeSettings),
    cv.Optional(CONF_WARNING_DEFAULT, default=6.0): cv.float_,
    cv.Optional(CONF_ALARM_DEFAULT, default=8.0): cv.float_,
    cv.Optional(CONF_RECOVERY_DEFAULT, default=5.0): cv.float_,
    cv.Optional(CONF_WARNING_DELAY_DEFAULT, default=600): cv.int_range(min=0, max=86400),
    cv.Optional(CONF_ALARM_DELAY_DEFAULT, default=300): cv.int_range(min=0, max=86400),
    cv.Optional(CONF_RECOVERY_DELAY_DEFAULT, default=300): cv.int_range(min=0, max=86400),
    cv.Optional(
        CONF_FIRMWARE_VERSION,
        default={CONF_NAME: "Версия прошивки"},
    ): text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional(
        CONF_OBJECT_NAME,
        default={CONF_NAME: "Настроенный объект"},
    ): text_sensor.text_sensor_schema(),
    cv.Optional(
        CONF_LOG_LEVEL,
        default={CONF_NAME: "Уровень логирования"},
    ): text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_defaults(
        config[CONF_WARNING_DEFAULT],
        config[CONF_ALARM_DEFAULT],
        config[CONF_RECOVERY_DEFAULT],
        config[CONF_WARNING_DELAY_DEFAULT],
        config[CONF_ALARM_DELAY_DEFAULT],
        config[CONF_RECOVERY_DELAY_DEFAULT],
    ))

    version_sensor = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
    cg.add(var.set_firmware_version_sensor(version_sensor))

    object_sensor = await text_sensor.new_text_sensor(config[CONF_OBJECT_NAME])
    cg.add(var.set_object_name_sensor(object_sensor))

    log_level_sensor = await text_sensor.new_text_sensor(config[CONF_LOG_LEVEL])
    cg.add(var.set_log_level_sensor(log_level_sensor))
