#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/smart_display/plugin_api.h"

#include <vector>

namespace esphome::energy_flow {

class FlowTile;

class EnergyFlow : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Every 250 ms: the tiles move their dots.
  void on_interval(uint32_t now_ms) override;

  void set_solar(sensor::Sensor *s) { solar_ = s; }
  void set_inverter(sensor::Sensor *s) { inverter_ = s; }
  void set_grid(sensor::Sensor *s) { grid_ = s; }
  void set_battery(sensor::Sensor *s) { battery_ = s; }
  void set_soc(sensor::Sensor *s) { soc_ = s; }
  void set_home(sensor::Sensor *s) { home_ = s; }
  void set_wallbox(sensor::Sensor *s) { wallbox_ = s; }
  void set_storage2(sensor::Sensor *s) { storage2_ = s; }

  const sensor::Sensor *solar() const { return solar_; }
  const sensor::Sensor *inverter() const { return inverter_; }
  const sensor::Sensor *grid() const { return grid_; }
  const sensor::Sensor *battery() const { return battery_; }
  const sensor::Sensor *soc() const { return soc_; }
  const sensor::Sensor *home() const { return home_; }
  const sensor::Sensor *wallbox() const { return wallbox_; }
  const sensor::Sensor *storage2() const { return storage2_; }

  void attach(FlowTile *tile);
  void detach(FlowTile *tile);

 protected:
  sensor::Sensor *solar_{}, *inverter_{}, *grid_{}, *battery_{}, *soc_{}, *home_{}, *wallbox_{}, *storage2_{};
  std::vector<FlowTile *> tiles_;
};

}  // namespace esphome::energy_flow
