import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

energy_flow_ns = cg.esphome_ns.namespace("energy_flow")
EnergyFlow = energy_flow_ns.class_("EnergyFlow", cg.Component)

SENSORS = ["solar", "inverter", "grid", "battery", "soc", "home", "wallbox", "storage2"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EnergyFlow),
        **{cv.Required(key): cv.use_id(sensor.Sensor) for key in SENSORS},
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    for key in SENSORS:
        sens = await cg.get_variable(config[key])
        cg.add(getattr(var, f"set_{key}")(sens))
