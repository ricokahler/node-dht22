"""Platform for light integration."""
from __future__ import annotations

from homeassistant.components.climate import (
    HVACMode, HVACAction, ClimateEntity, ClimateEntityFeature)
from homeassistant.const import (
    TEMP_FAHRENHEIT,
    ATTR_TEMPERATURE,
    ATTR_TARGET_TEMP_LOW,
    ATTR_TARGET_TEMP_HIGH
)

import requests

local_server_host = 'http://localhost:4201'

mode_map = {
    "off": HVACMode.OFF,
    "fan-only": HVACMode.FAN_ONLY,
    "idle": HVACMode.IDLE,
    "heat": HVACMode.HEAT,
    "cool": HVACMode.COOL,
    "heat-cool": HVACMode.HEAT_COOL
}

action_map = {
    "off": HVACAction.OFF,
    "fan": HVACAction.FAN,
    "idle": HVACMode.IDLE,
    "heating": HVACAction.HEATING,
    "cooling": HVACAction.COOLING,
}


def setup_platform(
    hass,
    config,
    add_entities,
    discovery_info
):
    add_entities([DiyClimate()])


class DiyClimate(ClimateEntity):
    def __init__(self):
        self._current_temperature = 50.0
        self._target_temperature = 70.0
        self._hvac_mode = HVACMode.OFF
        self._hvac_action = HVACAction.OFF

    @property
    def name(self):
        return "Thermostat"

    @property
    def temperature_unit(self):
        return TEMP_FAHRENHEIT

    @property
    def current_temperature(self):
        return self._current_temperature

    # @property
    # def current_humidity(self):
    #     return self._current_humidity

    @property
    def target_temperature(self):
        return None if type(self._target) is list else self._target

    @property
    def target_temperature_high(self):
        return self._target[1] if type(self._target) is list else None

    @property
    def target_temperature_low(self):
        return self._target[0] if type(self._target) is list else None

    @property
    def target_temperature_step(self):
        return 0.1

    @property
    def max_temp(self):
        return 80.0  # 80F is the hottest i'll let it go

    @property
    def min_temp(self):
        return 60.0  # same story here lol

    @property
    def hvac_mode(self):
        return self._hvac_mode

    @property
    def hvac_action(self):
        return self._hvac_action

    @property
    def hvac_modes(self):
        return [
            HVACMode.OFF,
            HVACMode.HEAT,
            HVACMode.COOL,
            HVACMode.HEAT_COOL,
            HVACMode.FAN_ONLY,
        ]

    @property
    def supported_features(self):
        return ClimateEntityFeature.TARGET_TEMPERATURE

    @property
    def fan_modes(self):
        return []

    def set_hvac_mode(self, hvac_mode):
        self._hvac_mode = hvac_mode

    def set_temperature(self, **kwargs):
        target_low = kwargs.get(ATTR_TARGET_TEMP_LOW, None)
        target_high = kwargs.get(ATTR_TARGET_TEMP_HIGH, None)
        target_general = kwargs.get(ATTR_TEMPERATURE, None)

        if target_low is not None and target_high is not None:
            self._target = [target_low, target_high]
        elif target_general is not None:
            self._target = target_general

        requests.put(f"{local_server_host}/thermostat", )

    def update(self) -> None:
        response = requests.get(f"{local_server_host}/thermostat")
        state = response.json()

        if 'mode' in state:
            self._hvac_mode = mode_map[state["mode"]]

        if 'action' in state:
            self._hvac_action = action_map[state["action"]]

        if 'target' in state:
            if type(state["target"]) is list:
                state["target"][0]
