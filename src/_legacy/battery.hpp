#pragma once

#include <cstdint>

#include "app_state.hpp"

class BatteryMonitor {
public:
  void init();
  void poll(AppState& app);

private:
  static uint16_t read_vsys_mv();
  static bool usb_present();
  static uint8_t estimate_percent(uint16_t millivolts);
};
