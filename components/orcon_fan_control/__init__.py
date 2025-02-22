import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.components.number as number
from esphome.const import CONF_ID

CONF_AIRFLOW_LEVEL = "airflow_level"

orcon_fan_control_ns = cg.esphome_ns.namespace("orcon_fan_control")
OrconFanControl = orcon_fan_control_ns.class_("OrconFanControl", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(OrconFanControl),
            cv.Required(CONF_AIRFLOW_LEVEL): cv.use_id(number.Number),
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    # 🔥 Wichtig: Das registriert das Objekt in ESPHome
    await cg.register_component(var, config)

    airflow_level = await cg.get_variable(config[CONF_AIRFLOW_LEVEL])
    cg.add(var.set_airflow_level(airflow_level))
