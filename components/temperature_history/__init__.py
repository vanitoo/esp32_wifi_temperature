import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.logger import request_log_listener
from esphome.const import CONF_ID

DEPENDENCIES = ["web_server_base", "logger"]

CONF_SENSOR_ID = "sensor_id"
CONF_CAPACITY = "capacity"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MAX_EVENTS = "max_events"
CONF_MAX_LOG_LINES = "max_log_lines"
CONF_MAX_LOG_LINE_LENGTH = "max_log_line_length"

ns = cg.esphome_ns.namespace("temperature_history")
TemperatureHistory = ns.class_("TemperatureHistory", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TemperatureHistory),
        cv.Required(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CAPACITY, default=1000): cv.int_range(min=10, max=5000),
        cv.Optional(
            CONF_SAMPLE_INTERVAL, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_EVENTS, default=100): cv.int_range(min=10, max=500),
        cv.Optional(CONF_MAX_LOG_LINES, default=250): cv.int_range(min=20, max=1000),
        cv.Optional(CONF_MAX_LOG_LINE_LENGTH, default=220): cv.int_range(min=64, max=512),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # ESPHome 2026.1+ uses a compile-time sized listener list.
    request_log_listener()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    source_sensor = await cg.get_variable(config[CONF_SENSOR_ID])
    cg.add(var.set_sensor(source_sensor))
    cg.add(var.set_capacity(config[CONF_CAPACITY]))
    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL]))
    cg.add(var.set_max_events(config[CONF_MAX_EVENTS]))
    cg.add(var.set_max_log_lines(config[CONF_MAX_LOG_LINES]))
    cg.add(var.set_max_log_line_length(config[CONF_MAX_LOG_LINE_LENGTH]))
