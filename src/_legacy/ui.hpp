#pragma once

#include "app_state.hpp"

class BleNusService;

class UiController {
public:
  void init(AppState& app);
  ButtonState poll_buttons();
  void apply_buttons(const ButtonState& buttons, AppState& app, BleNusService& ble);
  void render(AppState& app);

private:
  static void advance_page(AppState& app);
  static void update_buddy_state(AppState& app);
  static void update_led_mode(AppState& app);
};
