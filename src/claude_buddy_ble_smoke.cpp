#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "btstack.h"
#include "ble/gatt-service/nordic_spp_service_server.h"

#include "button.hpp"
#include "drivers/st7789/st7789.hpp"
#include "hardware/adc.h"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico_display_28.hpp"
#include "rgbled.hpp"

#include "claude_buddy_ble_smoke.h"

using namespace pimoroni;

namespace {

constexpr uint32_t kUiTickMs = 50;
constexpr float kAdcReferenceVolts = 3.3f;
constexpr float kVoltageDividerScale = 3.0f;
constexpr uint32_t kDefaultPasskey = 123456;

ST7789 st7789(PicoDisplay28::WIDTH, PicoDisplay28::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);
RGBLED led(PicoDisplay28::LED_R, PicoDisplay28::LED_G, PicoDisplay28::LED_B);

Button button_a(PicoDisplay28::A);
Button button_b(PicoDisplay28::B);
Button button_x(PicoDisplay28::X);
Button button_y(PicoDisplay28::Y);

// Keep the primary advertisement under the 31-byte BLE legacy limit so
// scanners reliably see the local name during discovery.
const uint8_t adv_data[] = {
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    12, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'C', 'l', 'a', 'u', 'd', 'e', ' ', 'P', 'i', 'c', 'o',
};
const uint8_t adv_data_len = sizeof(adv_data);

const uint8_t scan_response_data[] = {
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,
};
const uint8_t scan_response_data_len = sizeof(scan_response_data);

struct BuddyBleState {
  hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
  bd_addr_t peer_addr{};
  bd_addr_type_t peer_addr_type = BD_ADDR_TYPE_LE_PUBLIC;
  bool advertising = true;
  bool connected = false;
  bool bonded = false;
  bool secure = false;
  bool prompt_active = false;
  bool last_a = false;
  bool last_b = false;
  uint16_t battery_mv = 0;
  bool usb_present = false;
  uint8_t battery_pct = 0;
  uint32_t approvals = 0;
  uint32_t denials = 0;
  int total = 0;
  int running = 0;
  int waiting = 0;
  int tokens_today = 0;
  char status_line[64] = "Advertising";
  char msg[64] = "Waiting for Claude";
  char last_rx[120] = "No RX yet";
  char owner[32] = "";
  char prompt_id[64] = "";
  char prompt_hint[80] = "";
  char passkey[16] = "123456";
  char tx_line[320] = "";
  bool tx_pending = false;
  btstack_context_callback_registration_t send_request{};
  btstack_timer_source ui_timer{};
} state;

btstack_packet_callback_registration_t hci_event_callback_registration;
btstack_packet_callback_registration_t sm_event_callback_registration;

uint16_t read_vsys_mv() {
  adc_select_input(3);
  sleep_us(10);
  const uint16_t raw = adc_read();
  const float volts = (static_cast<float>(raw) * kAdcReferenceVolts * kVoltageDividerScale) / 4095.0f;
  return static_cast<uint16_t>(volts * 1000.0f);
}

uint8_t estimate_percent(const uint16_t millivolts) {
  if(millivolts >= 4200) return 100;
  if(millivolts <= 3300) return 0;
  if(millivolts >= 4000) return static_cast<uint8_t>(80 + ((millivolts - 4000) * 20) / 200);
  if(millivolts >= 3700) return static_cast<uint8_t>(20 + ((millivolts - 3700) * 60) / 300);
  return static_cast<uint8_t>((millivolts - 3300) * 20 / 400);
}

bool contains_token(const char* haystack, const char* needle) {
  return std::strstr(haystack, needle) != nullptr;
}

int extract_int(const char* line, const char* key, const int fallback) {
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* start = std::strstr(line, needle);
  if(start == nullptr) {
    return fallback;
  }
  start += std::strlen(needle);
  return std::atoi(start);
}

void extract_string(const char* line, const char* key, char* out, const size_t out_size) {
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* start = std::strstr(line, needle);
  if(start == nullptr) {
    return;
  }
  start += std::strlen(needle);
  while(*start == ' ') {
    ++start;
  }
  if(*start != '"') {
    return;
  }
  ++start;
  const char* end = std::strchr(start, '"');
  if(end == nullptr) {
    return;
  }
  const size_t count = static_cast<size_t>(end - start);
  const size_t copy = count < (out_size - 1) ? count : (out_size - 1);
  std::memcpy(out, start, copy);
  out[copy] = '\0';
}

void request_can_send();

void queue_tx_line(const char* line_without_newline) {
  if(state.connection_handle == HCI_CON_HANDLE_INVALID) {
    return;
  }
  std::snprintf(state.tx_line, sizeof(state.tx_line), "%s\n", line_without_newline);
  state.tx_pending = true;
  request_can_send();
}

void send_status_reply() {
  char line[320];
  const char* owner_fragment = "";
  char owner_buf[64];
  if(state.owner[0] != '\0') {
    std::snprintf(owner_buf, sizeof(owner_buf), ",\"owner\":\"%s\"", state.owner);
    owner_fragment = owner_buf;
  }
  std::snprintf(
      line,
      sizeof(line),
      "{\"ack\":\"status\",\"ok\":true,\"data\":{\"name\":\"Claude Pico\"%s,\"sec\":%s,\"bat\":{\"pct\":%u,\"mV\":%u,\"mA\":0,\"usb\":%s},\"sys\":{\"up\":%llu,\"heap\":0},\"stats\":{\"appr\":%lu,\"deny\":%lu,\"vel\":0,\"nap\":0,\"lvl\":0}}}",
      owner_fragment,
      state.secure ? "true" : "false",
      static_cast<unsigned int>(state.battery_pct),
      static_cast<unsigned int>(state.battery_mv),
      state.usb_present ? "true" : "false",
      to_ms_since_boot(get_absolute_time()) / 1000,
      static_cast<unsigned long>(state.approvals),
      static_cast<unsigned long>(state.denials));
  queue_tx_line(line);
}

void ack_command(const char* command) {
  char line[96];
  std::snprintf(line, sizeof(line), "{\"ack\":\"%s\",\"ok\":true,\"n\":0}", command);
  queue_tx_line(line);
}

void request_can_send() {
  if(!state.tx_pending || state.connection_handle == HCI_CON_HANDLE_INVALID) {
    return;
  }
  state.send_request.callback = [](void* context) {
    UNUSED(context);
    if(!state.tx_pending || state.connection_handle == HCI_CON_HANDLE_INVALID) {
      return;
    }
    nordic_spp_service_server_send(
        state.connection_handle,
        reinterpret_cast<const uint8_t*>(state.tx_line),
        static_cast<uint16_t>(std::strlen(state.tx_line)));
    state.tx_pending = false;
  };
  nordic_spp_service_server_request_can_send_now(&state.send_request, state.connection_handle);
}

void handle_protocol_line(const char* line) {
  std::snprintf(state.last_rx, sizeof(state.last_rx), "%s", line);

  if(contains_token(line, "\"cmd\":\"status\"")) {
    send_status_reply();
    return;
  }

  if(contains_token(line, "\"cmd\":\"name\"")) {
    ack_command("name");
    return;
  }

  if(contains_token(line, "\"cmd\":\"owner\"")) {
    extract_string(line, "name", state.owner, sizeof(state.owner));
    ack_command("owner");
    return;
  }

  if(contains_token(line, "\"cmd\":\"unpair\"")) {
    if(state.connected) {
      gap_delete_bonding(state.peer_addr_type, state.peer_addr);
    }
    state.bonded = false;
    state.secure = false;
    ack_command("unpair");
    return;
  }

  if(contains_token(line, "\"total\"")) {
    state.total = extract_int(line, "total", state.total);
    state.running = extract_int(line, "running", state.running);
    state.waiting = extract_int(line, "waiting", state.waiting);
    state.tokens_today = extract_int(line, "tokens_today", state.tokens_today);
    extract_string(line, "msg", state.msg, sizeof(state.msg));

    if(contains_token(line, "\"prompt\"")) {
      state.prompt_active = true;
      extract_string(line, "id", state.prompt_id, sizeof(state.prompt_id));
      extract_string(line, "hint", state.prompt_hint, sizeof(state.prompt_hint));
    } else {
      state.prompt_active = false;
      state.prompt_id[0] = '\0';
      state.prompt_hint[0] = '\0';
    }
  }
}

void nordic_spp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);

  static char line_buffer[320];
  static size_t line_length = 0;

  switch(packet_type) {
    case HCI_EVENT_PACKET:
      if(hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) {
        break;
      }
      switch(hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED:
          state.connection_handle = gattservice_subevent_spp_service_connected_get_con_handle(packet);
          state.connected = true;
          state.advertising = false;
          std::snprintf(state.status_line, sizeof(state.status_line), "NUS connected");
          break;
        case GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED:
          state.connection_handle = HCI_CON_HANDLE_INVALID;
          state.connected = false;
          state.prompt_active = false;
          state.advertising = true;
          std::snprintf(state.status_line, sizeof(state.status_line), "Advertising");
          break;
        default:
          break;
      }
      break;
    case RFCOMM_DATA_PACKET:
      for(uint16_t i = 0; i < size; ++i) {
        const char ch = static_cast<char>(packet[i]);
        if(ch == '\r') {
          continue;
        }
        if(ch == '\n') {
          line_buffer[line_length] = '\0';
          if(line_length > 0) {
            handle_protocol_line(line_buffer);
          }
          line_length = 0;
          continue;
        }
        if(line_length < (sizeof(line_buffer) - 1)) {
          line_buffer[line_length++] = ch;
        }
      }
      break;
    default:
      break;
  }
}

void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);
  UNUSED(size);
  if(packet_type != HCI_EVENT_PACKET) {
    return;
  }

  switch(hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
      if(btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        std::snprintf(state.status_line, sizeof(state.status_line), "Advertising");
      }
      break;
    case HCI_EVENT_META_GAP:
      switch(hci_event_gap_meta_get_subevent_code(packet)) {
        case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
          state.connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
          state.peer_addr_type = static_cast<bd_addr_type_t>(gap_subevent_le_connection_complete_get_peer_address_type(packet));
          gap_subevent_le_connection_complete_get_peer_address(packet, state.peer_addr);
          state.connected = true;
          std::snprintf(state.status_line, sizeof(state.status_line), "Connected");
          break;
        default:
          break;
      }
      break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
      state.connection_handle = HCI_CON_HANDLE_INVALID;
      state.connected = false;
      state.secure = false;
      state.prompt_active = false;
      state.advertising = true;
      std::snprintf(state.status_line, sizeof(state.status_line), "Advertising");
      std::snprintf(state.msg, sizeof(state.msg), "Waiting for Claude");
      break;
    default:
      break;
  }
}

void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);
  UNUSED(size);
  if(packet_type != HCI_EVENT_PACKET) {
    return;
  }

  switch(hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
      sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
      break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
      std::snprintf(
          state.passkey,
          sizeof(state.passkey),
          "%06" PRIu32,
          sm_event_passkey_display_number_get_passkey(packet));
      std::snprintf(state.status_line, sizeof(state.status_line), "Enter passkey");
      break;
    case SM_EVENT_PAIRING_COMPLETE:
      if(sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
        state.bonded = true;
        state.secure = true;
        std::snprintf(state.status_line, sizeof(state.status_line), "Paired");
      } else {
        state.secure = false;
        std::snprintf(state.status_line, sizeof(state.status_line), "Pairing failed");
      }
      break;
    case SM_EVENT_REENCRYPTION_COMPLETE:
      if(sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
        state.secure = true;
        std::snprintf(state.status_line, sizeof(state.status_line), "Secure reconnect");
      }
      break;
    default:
      break;
  }
}

void render_screen() {
  graphics.set_pen(6, 10, 18);
  graphics.clear();

  graphics.set_pen(245, 248, 250);
  graphics.text("Claude Buddy BLE smoke", Point(16, 16), 288, 2);

  graphics.set_pen(120, 180, 255);
  graphics.text(state.status_line, Point(16, 44), 288);

  char line[128];

  std::snprintf(line, sizeof(line), "Name: Claude Pico");
  graphics.set_pen(215, 220, 225);
  graphics.text(line, Point(16, 70), 288);

  std::snprintf(line, sizeof(line), "USB: %s  VSYS: %u mV  Bat: %u%%",
                state.usb_present ? "ON" : "OFF",
                static_cast<unsigned int>(state.battery_mv),
                static_cast<unsigned int>(state.battery_pct));
  graphics.text(line, Point(16, 88), 288);

  std::snprintf(line, sizeof(line), "Sessions t/r/w: %d/%d/%d",
                state.total, state.running, state.waiting);
  graphics.text(line, Point(16, 106), 288);

  std::snprintf(line, sizeof(line), "Tokens today: %d", state.tokens_today);
  graphics.text(line, Point(16, 124), 288);

  graphics.set_pen(40, 56, 74);
  graphics.rectangle(Rect(16, 146, 288, 30));
  graphics.set_pen(255, 236, 168);
  if(state.connected && !state.secure) {
    std::snprintf(line, sizeof(line), "Pair passkey: %s", state.passkey);
  } else if(state.secure) {
    std::snprintf(line, sizeof(line), "Secure link ready");
  } else {
    std::snprintf(line, sizeof(line), "Open Claude Desktop -> Developer -> Hardware Buddy");
  }
  graphics.text(line, Point(20, 156), 280);

  graphics.set_pen(32, 44, 58);
  graphics.rectangle(Rect(16, 186, 288, 34));
  graphics.set_pen(220, 226, 232);
  graphics.text(state.msg, Point(20, 196), 280);

  graphics.set_pen(32, 44, 58);
  graphics.rectangle(Rect(16, 226, 288, 14));
  graphics.set_pen(170, 176, 182);
  graphics.text(state.last_rx, Point(18, 228), 284);

  st7789.update(&graphics);
}

void update_led() {
  if(!state.connected) {
    led.set_rgb(255, 0, 255);
    return;
  }
  if(state.prompt_active || state.waiting > 0) {
    led.set_rgb(255, 160, 0);
    return;
  }
  if(state.running > 0) {
    led.set_rgb(0, 0, 255);
    return;
  }
  led.set_rgb(0, 255, 0);
}

void ui_timer_handler(btstack_timer_source* ts) {
  state.battery_mv = read_vsys_mv();
  state.battery_pct = estimate_percent(state.battery_mv);
  state.usb_present = cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN) != 0;

  const bool a = button_a.raw();
  const bool b = button_b.raw();
  const bool x = button_x.raw();
  const bool y = button_y.raw();

  if(state.prompt_active && a && !state.last_a && state.prompt_id[0] != '\0') {
    char line[160];
    std::snprintf(line, sizeof(line), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"once\"}", state.prompt_id);
    queue_tx_line(line);
    state.approvals += 1;
    state.prompt_active = false;
  }

  if(state.prompt_active && b && !state.last_b && state.prompt_id[0] != '\0') {
    char line[160];
    std::snprintf(line, sizeof(line), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"deny\"}", state.prompt_id);
    queue_tx_line(line);
    state.denials += 1;
    state.prompt_active = false;
  }

  if(x || y) {
    std::snprintf(state.msg, sizeof(state.msg), "Buttons X/Y reserved for next stage");
  }

  state.last_a = a;
  state.last_b = b;

  update_led();
  render_screen();

  btstack_run_loop_set_timer(ts, kUiTickMs);
  btstack_run_loop_add_timer(ts);
}

void setup_ble() {
  l2cap_init();
  sm_init();

  sm_set_secure_connections_only_mode(true);
  sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
  sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING);
  sm_use_fixed_passkey_in_display_role(kDefaultPasskey);

  att_server_init(profile_data, nullptr, nullptr);
  nordic_spp_service_server_init(&nordic_spp_packet_handler);

  const uint16_t adv_int_min = 0x0030;
  const uint16_t adv_int_max = 0x0030;
  const uint8_t adv_type = 0;
  bd_addr_t null_addr;
  std::memset(null_addr, 0, sizeof(null_addr));

  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
  gap_advertisements_set_data(adv_data_len, const_cast<uint8_t*>(adv_data));
  gap_scan_response_set_data(scan_response_data_len, const_cast<uint8_t*>(scan_response_data));
  gap_advertisements_enable(1);

  hci_event_callback_registration.callback = &hci_packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  sm_event_callback_registration.callback = &sm_packet_handler;
  sm_add_event_handler(&sm_event_callback_registration);
}

}  // namespace

int main() {
  stdio_init_all();

  adc_init();
  adc_gpio_init(29);
  st7789.set_backlight(180);

  if(cyw43_arch_init()) {
    while(true) {
      sleep_ms(250);
    }
  }

  setup_ble();

  state.ui_timer.process = &ui_timer_handler;
  btstack_run_loop_set_timer(&state.ui_timer, kUiTickMs);
  btstack_run_loop_add_timer(&state.ui_timer);

  render_screen();
  update_led();

  hci_power_control(HCI_POWER_ON);
  btstack_run_loop_execute();
  return 0;
}
