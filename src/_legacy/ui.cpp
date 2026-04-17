#include "ui.hpp"

#include <string>

#include "ble_nus.hpp"

namespace {

std::string make_permission_reply(const PromptState& prompt, const char* decision) {
  return std::string("{\"cmd\":\"permission\",\"id\":\"") + prompt.id +
         "\",\"decision\":\"" + decision + "\"}";
}

}  // namespace

void UiController::init(AppState& app) {
  app.ui.page = UiPage::Home;
}

ButtonState UiController::poll_buttons() {
  // TODO: Replace this stub with the Pimoroni Pico Display 2.8 input wiring:
  // Button button_a(PicoDisplay28::A);
  // Button button_b(PicoDisplay28::B);
  // Button button_x(PicoDisplay28::X);
  // Button button_y(PicoDisplay28::Y);
  return {};
}

void UiController::apply_buttons(const ButtonState& buttons, AppState& app, BleNusService& ble) {
  if(app.snapshot.prompt.active) {
    if(buttons.a_pressed) {
      ble.queue_line(make_permission_reply(app.snapshot.prompt, "once"));
      app.stats.approvals += 1;
      app.snapshot.prompt.active = false;
      app.ui.page = UiPage::Home;
      app.ui.keep_awake = false;
      return;
    }

    if(buttons.b_pressed) {
      ble.queue_line(make_permission_reply(app.snapshot.prompt, "deny"));
      app.stats.denials += 1;
      app.snapshot.prompt.active = false;
      app.ui.page = UiPage::Home;
      app.ui.keep_awake = false;
      return;
    }

    if(buttons.x_pressed) {
      app.ui.prompt_details = !app.ui.prompt_details;
    }

    if(buttons.y_pressed) {
      app.ui.page = UiPage::Home;
    }

    return;
  }

  if(buttons.a_held) {
    app.ui.page = UiPage::Settings;
    return;
  }

  if(buttons.a_pressed) {
    advance_page(app);
  }

  if(buttons.b_pressed) {
    app.ui.activity_scroll += 1;
  }

  if(buttons.x_pressed) {
    app.ui.transcript_expanded = !app.ui.transcript_expanded;
  }

  if(buttons.y_pressed) {
    app.ui.page = UiPage::Home;
  }
}

void UiController::render(AppState& app) {
  if(app.snapshot.prompt.active) {
    app.ui.page = UiPage::Prompt;
    app.ui.keep_awake = true;
  } else if(app.ui.page == UiPage::Prompt) {
    app.ui.page = UiPage::Home;
  }

  update_buddy_state(app);
  update_led_mode(app);

  // TODO: Replace the state-only render path with the Pimoroni display stack:
  // ST7789 st7789(...);
  // PicoGraphics_PenRGB332 graphics(...);
  // RGBLED led(PicoDisplay28::LED_R, PicoDisplay28::LED_G, PicoDisplay28::LED_B);
}

void UiController::advance_page(AppState& app) {
  switch(app.ui.page) {
    case UiPage::Home:
      app.ui.page = UiPage::Activity;
      break;
    case UiPage::Activity:
      app.ui.page = UiPage::Stats;
      break;
    case UiPage::Stats:
      app.ui.page = UiPage::Home;
      break;
    case UiPage::Prompt:
      app.ui.page = UiPage::Home;
      break;
    case UiPage::Settings:
      app.ui.page = UiPage::Home;
      break;
  }
}

void UiController::update_buddy_state(AppState& app) {
  if(!app.link.connected) {
    app.buddy_state = BuddyState::Sleep;
    return;
  }

  if(app.snapshot.waiting > 0 || app.snapshot.prompt.active) {
    app.buddy_state = BuddyState::Attention;
    return;
  }

  if(app.snapshot.running > 0) {
    app.buddy_state = BuddyState::Busy;
    return;
  }

  app.buddy_state = BuddyState::Idle;
}

void UiController::update_led_mode(AppState& app) {
  if(app.battery.low) {
    app.led_mode = LedMode::Red;
    return;
  }

  if(!app.link.connected) {
    app.led_mode = LedMode::Off;
    return;
  }

  if(app.snapshot.prompt.active || app.snapshot.waiting > 0) {
    app.led_mode = LedMode::Amber;
    return;
  }

  if(app.snapshot.running > 0) {
    app.led_mode = LedMode::Blue;
    return;
  }

  app.led_mode = LedMode::Green;
}
