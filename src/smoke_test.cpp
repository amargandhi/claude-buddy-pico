#include <cstdio>

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "button.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "pico_display_28.hpp"
#include "rgbled.hpp"

using namespace pimoroni;

namespace {

constexpr float kAdcReferenceVolts = 3.3f;
constexpr float kVoltageDividerScale = 3.0f;

ST7789 st7789(PicoDisplay28::WIDTH, PicoDisplay28::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);

RGBLED led(PicoDisplay28::LED_R, PicoDisplay28::LED_G, PicoDisplay28::LED_B);

Button button_a(PicoDisplay28::A);
Button button_b(PicoDisplay28::B);
Button button_x(PicoDisplay28::X);
Button button_y(PicoDisplay28::Y);

uint16_t read_vsys_mv() {
  adc_select_input(3);
  sleep_us(10);

  const uint16_t raw = adc_read();
  const float volts = (static_cast<float>(raw) * kAdcReferenceVolts * kVoltageDividerScale) / 4095.0f;
  return static_cast<uint16_t>(volts * 1000.0f);
}

const char* on_off(const bool value) {
  return value ? "ON" : "OFF";
}

void draw_button_row(const int y, const char* name, const bool pressed, const uint8_t r, const uint8_t g, const uint8_t b) {
  graphics.set_pen(18, 26, 36);
  graphics.rectangle(Rect(16, y, 288, 26));

  graphics.set_pen(230, 235, 240);
  graphics.text(name, Point(24, y + 7), 60);

  graphics.set_pen(pressed ? r : 60, pressed ? g : 60, pressed ? b : 60);
  graphics.rectangle(Rect(104, y + 5, 18, 18));

  graphics.set_pen(pressed ? 255 : 150, pressed ? 255 : 150, pressed ? 255 : 150);
  graphics.text(pressed ? "PRESSED" : "idle", Point(136, y + 7), 120);
}

}  // namespace

int main() {
  stdio_init_all();

  adc_init();
  adc_gpio_init(29);

  const bool cyw43_ready = cyw43_arch_init() == 0;

  st7789.set_backlight(180);

  while(true) {
    const bool a = button_a.raw();
    const bool b = button_b.raw();
    const bool x = button_x.raw();
    const bool y = button_y.raw();

    uint8_t led_r = 32;
    uint8_t led_g = 16;
    uint8_t led_b = 0;
    const char* led_mode = "idle amber";

    if(a) {
      led_r = 255;
      led_g = 0;
      led_b = 0;
      led_mode = "A -> red";
    } else if(b) {
      led_r = 0;
      led_g = 255;
      led_b = 0;
      led_mode = "B -> green";
    } else if(x) {
      led_r = 0;
      led_g = 0;
      led_b = 255;
      led_mode = "X -> blue";
    } else if(y) {
      led_r = 255;
      led_g = 255;
      led_b = 255;
      led_mode = "Y -> white";
    }

    led.set_rgb(led_r, led_g, led_b);

    const uint16_t vsys_mv = read_vsys_mv();
    const bool usb_present = cyw43_ready ? (cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN) != 0) : false;

    char header_line[64];
    char power_line[96];
    char usb_line[64];
    char uptime_line[64];

    std::snprintf(header_line, sizeof(header_line), "Claude Buddy Pico smoke test");
    std::snprintf(power_line, sizeof(power_line), "VSYS: %u mV", static_cast<unsigned int>(vsys_mv));
    std::snprintf(usb_line, sizeof(usb_line), "USB sense: %s", cyw43_ready ? on_off(usb_present) : "CYW43 init failed");
    std::snprintf(uptime_line, sizeof(uptime_line), "Uptime: %llu ms", to_ms_since_boot(get_absolute_time()));

    graphics.set_pen(6, 10, 18);
    graphics.clear();

    graphics.set_pen(245, 248, 250);
    graphics.text(header_line, Point(16, 16), 288, 2);

    graphics.set_pen(120, 180, 255);
    graphics.text("If you can read this, the display path is alive.", Point(16, 44), 288);

    graphics.set_pen(210, 214, 220);
    graphics.text(power_line, Point(16, 72), 288);
    graphics.text(usb_line, Point(16, 90), 288);
    graphics.text(uptime_line, Point(16, 108), 288);

    draw_button_row(136, "A", a, 255, 80, 80);
    draw_button_row(166, "B", b, 80, 255, 80);
    draw_button_row(196, "X", x, 80, 120, 255);

    graphics.set_pen(18, 26, 36);
    graphics.rectangle(Rect(16, 226, 288, 10));
    graphics.set_pen(255, 230, 160);
    graphics.text(led_mode, Point(18, 226), 140);
    graphics.set_pen(y ? 255 : 90, y ? 255 : 90, y ? 255 : 90);
    graphics.text(y ? "Y PRESSED" : "Y idle", Point(176, 226), 120);

    st7789.update(&graphics);
    sleep_ms(33);
  }
}
