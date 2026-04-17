#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "app_state.hpp"
#include "battery.hpp"
#include "ble_nus.hpp"
#include "protocol.hpp"
#include "settings.hpp"
#include "ui.hpp"

namespace {

constexpr int64_t kHeartbeatTimeoutUs = 30 * 1000 * 1000;

}  // namespace

int main() {
  stdio_init_all();

  AppState app;
  SettingsStore settings;
  BatteryMonitor battery;
  BleNusService ble;
  ProtocolEngine protocol;
  UiController ui;

  settings.init(app);

  if(cyw43_arch_init() != 0) {
    while(true) {
      sleep_ms(500);
    }
  }

  battery.init();
  ble.init(app);
  ui.init(app);

  while(true) {
    ble.poll(app);
    protocol.poll(ble, app, settings);

    const ButtonState buttons = ui.poll_buttons();
    ui.apply_buttons(buttons, app, ble);

    battery.poll(app);

    if(!is_nil_time(app.snapshot.last_update) &&
       absolute_time_diff_us(app.snapshot.last_update, get_absolute_time()) > kHeartbeatTimeoutUs) {
      app.link.connected = false;
      app.snapshot.prompt = {};
      app.ui.keep_awake = false;
    }

    ui.render(app);
    sleep_ms(16);
  }
}
