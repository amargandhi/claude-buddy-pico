#include "battery.hpp"

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

namespace {

constexpr uint32_t kSampleIntervalMs = 2000;
constexpr float kAdcReferenceVolts = 3.3f;
constexpr float kVoltageDividerScale = 3.0f;

}  // namespace

void BatteryMonitor::init() {
  adc_init();
  adc_gpio_init(29);
}

void BatteryMonitor::poll(AppState& app) {
  const absolute_time_t now = get_absolute_time();
  if(!is_nil_time(app.battery.last_sample) &&
     absolute_time_diff_us(app.battery.last_sample, now) < static_cast<int64_t>(kSampleIntervalMs) * 1000) {
    return;
  }

  app.battery.usb_present = usb_present();
  app.battery.millivolts = read_vsys_mv();
  app.battery.percent = estimate_percent(app.battery.millivolts);
  app.battery.low = app.battery.millivolts > 0 && app.battery.millivolts < 3500;
  app.battery.last_sample = now;
}

uint16_t BatteryMonitor::read_vsys_mv() {
  adc_select_input(3);
  sleep_us(10);

  const uint16_t raw = adc_read();
  const float volts = (static_cast<float>(raw) * kAdcReferenceVolts * kVoltageDividerScale) / 4095.0f;
  return static_cast<uint16_t>(volts * 1000.0f);
}

bool BatteryMonitor::usb_present() {
  return cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN) != 0;
}

uint8_t BatteryMonitor::estimate_percent(const uint16_t millivolts) {
  if(millivolts >= 4200) {
    return 100;
  }

  if(millivolts <= 3300) {
    return 0;
  }

  if(millivolts >= 4000) {
    return static_cast<uint8_t>(80 + ((millivolts - 4000) * 20) / 200);
  }

  if(millivolts >= 3700) {
    return static_cast<uint8_t>(20 + ((millivolts - 3700) * 60) / 300);
  }

  return static_cast<uint8_t>((millivolts - 3300) * 20 / 400);
}
