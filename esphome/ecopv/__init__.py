"""ESPHome component for MaxPV EcoPV communication protocol."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, binary_sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_SCAN_INTERVAL,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_ENERGY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

CONF_ECOPV = "ecopv"
CONF_VOLTAGE_SENSOR = "voltage_sensor"
CONF_CURRENT_SENSOR = "current_sensor"
CONF_POWER_ACTIVE_SENSOR = "power_active_sensor"
CONF_POWER_APPARENT_SENSOR = "power_apparent_sensor"
CONF_POWER_ROUTED_SENSOR = "power_routed_sensor"
CONF_POWER_EXPORT_SENSOR = "power_export_sensor"
CONF_ENERGY_ROUTED_SENSOR = "energy_routed_sensor"
CONF_ENERGY_IMPORTED_SENSOR = "energy_imported_sensor"
CONF_ENERGY_EXPORTED_SENSOR = "energy_exported_sensor"
CONF_PV_COUNTER_SENSOR = "pv_counter_sensor"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_PV_REFERENCE_POWER_SENSOR = "pv_reference_power_sensor"
CONF_RELAY_TIME_SENSOR = "relay_time_sensor"
CONF_VERSION_TEXT = "version_text_sensor"
CONF_STATUS_TEXT = "status_text_sensor"
CONF_CONTACT_ESTABLISHED = "contact_established_binary"
CONF_ROUTER_RUNNING = "router_running_binary"

ecopv_ns = cg.esphome_ns.namespace("ecopv")
EcoPVComponent = ecopv_ns.class_("EcoPVComponent", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EcoPVComponent),
        cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_SCAN_INTERVAL, default=1000): cv.positive_int,
        cv.Optional(CONF_VOLTAGE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CURRENT_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_POWER_ACTIVE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_POWER_APPARENT_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_POWER_ROUTED_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_POWER_EXPORT_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ENERGY_ROUTED_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ENERGY_IMPORTED_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ENERGY_EXPORTED_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_PV_COUNTER_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_PV_REFERENCE_POWER_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_RELAY_TIME_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_VERSION_TEXT): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_STATUS_TEXT): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_CONTACT_ESTABLISHED): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_ROUTER_RUNNING): cv.use_id(binary_sensor.BinarySensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Build the esphome component."""
    uart_component = await cg.get_variable(config[CONF_UART_ID])

    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    await cg.register_component(var, config)

    cg.add(var.set_scan_interval(config[CONF_SCAN_INTERVAL]))

    # Register sensors if configured
    if CONF_VOLTAGE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_VOLTAGE_SENSOR])
        cg.add(var.set_voltage_sensor(sens))

    if CONF_CURRENT_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_SENSOR])
        cg.add(var.set_current_sensor(sens))

    if CONF_POWER_ACTIVE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_POWER_ACTIVE_SENSOR])
        cg.add(var.set_power_active_sensor(sens))

    if CONF_POWER_APPARENT_SENSOR in config:
        sens = await cg.get_variable(config[CONF_POWER_APPARENT_SENSOR])
        cg.add(var.set_power_apparent_sensor(sens))

    if CONF_POWER_ROUTED_SENSOR in config:
        sens = await cg.get_variable(config[CONF_POWER_ROUTED_SENSOR])
        cg.add(var.set_power_routed_sensor(sens))

    if CONF_POWER_EXPORT_SENSOR in config:
        sens = await cg.get_variable(config[CONF_POWER_EXPORT_SENSOR])
        cg.add(var.set_power_export_sensor(sens))

    if CONF_ENERGY_ROUTED_SENSOR in config:
        sens = await cg.get_variable(config[CONF_ENERGY_ROUTED_SENSOR])
        cg.add(var.set_energy_routed_sensor(sens))

    if CONF_ENERGY_IMPORTED_SENSOR in config:
        sens = await cg.get_variable(config[CONF_ENERGY_IMPORTED_SENSOR])
        cg.add(var.set_energy_imported_sensor(sens))

    if CONF_ENERGY_EXPORTED_SENSOR in config:
        sens = await cg.get_variable(config[CONF_ENERGY_EXPORTED_SENSOR])
        cg.add(var.set_energy_exported_sensor(sens))

    if CONF_PV_COUNTER_SENSOR in config:
        sens = await cg.get_variable(config[CONF_PV_COUNTER_SENSOR])
        cg.add(var.set_pv_counter_sensor(sens))

    if CONF_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_PV_REFERENCE_POWER_SENSOR in config:
        sens = await cg.get_variable(config[CONF_PV_REFERENCE_POWER_SENSOR])
        cg.add(var.set_pv_reference_power_sensor(sens))

    if CONF_RELAY_TIME_SENSOR in config:
        sens = await cg.get_variable(config[CONF_RELAY_TIME_SENSOR])
        cg.add(var.set_relay_time_sensor(sens))

    # Register text sensors
    if CONF_VERSION_TEXT in config:
        sens = await cg.get_variable(config[CONF_VERSION_TEXT])
        cg.add(var.set_version_text(sens))

    if CONF_STATUS_TEXT in config:
        sens = await cg.get_variable(config[CONF_STATUS_TEXT])
        cg.add(var.set_status_text(sens))

    # Register binary sensors
    if CONF_CONTACT_ESTABLISHED in config:
        sens = await cg.get_variable(config[CONF_CONTACT_ESTABLISHED])
        cg.add(var.set_contact_established_binary(sens))

    if CONF_ROUTER_RUNNING in config:
        sens = await cg.get_variable(config[CONF_ROUTER_RUNNING])
        cg.add(var.set_router_running_binary(sens))
