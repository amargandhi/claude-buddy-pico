#include "protocol.hpp"

#include <cctype>
#include <cstdlib>
#include <string>

#include "ble_nus.hpp"
#include "settings.hpp"

#include "pico/time.h"

namespace {

std::string json_bool(const bool value) {
  return value ? "true" : "false";
}

std::string status_ack(const char* command) {
  return std::string("{\"ack\":\"") + command + "\",\"ok\":true,\"n\":0}";
}

}  // namespace

void ProtocolEngine::poll(BleNusService& ble, AppState& app, SettingsStore& settings) {
  std::string line;
  while(ble.read_line(line)) {
    handle_line(line, ble, app, settings);
  }
}

std::string ProtocolEngine::make_permission_reply(const PromptState& prompt, const bool approve) const {
  if(prompt.id.empty()) {
    return {};
  }

  return std::string("{\"cmd\":\"permission\",\"id\":\"") + prompt.id +
         "\",\"decision\":\"" + (approve ? "once" : "deny") + "\"}";
}

std::string ProtocolEngine::make_status_reply(const AppState& app) const {
  return std::string("{\"ack\":\"status\",\"ok\":true,\"data\":{") +
         "\"name\":\"" + app.settings.device_name + "\"," +
         "\"sec\":" + json_bool(app.link.secure) + "," +
         "\"bat\":{\"pct\":" + std::to_string(app.battery.percent) +
         ",\"mV\":" + std::to_string(app.battery.millivolts) +
         ",\"usb\":" + json_bool(app.battery.usb_present) + "}," +
         "\"sys\":{\"up\":" + std::to_string(to_ms_since_boot(get_absolute_time()) / 1000) + "}," +
         "\"stats\":{\"appr\":" + std::to_string(app.stats.approvals) +
         ",\"deny\":" + std::to_string(app.stats.denials) + "}}}";
}

void ProtocolEngine::handle_line(const std::string& line, BleNusService& ble, AppState& app, SettingsStore& settings) {
  if(contains_token(line, "\"cmd\":\"status\"")) {
    ble.queue_line(make_status_reply(app));
    return;
  }

  if(contains_token(line, "\"cmd\":\"name\"")) {
    const std::string name = extract_string(line, "name");
    if(!name.empty()) {
      settings.set_device_name(app, name);
    }
    ble.queue_line(status_ack("name"));
    return;
  }

  if(contains_token(line, "\"cmd\":\"owner\"")) {
    const std::string owner = extract_string(line, "name");
    if(!owner.empty()) {
      settings.set_owner_name(app, owner);
    }
    ble.queue_line(status_ack("owner"));
    return;
  }

  if(contains_token(line, "\"cmd\":\"unpair\"")) {
    settings.clear_pairing(app);
    ble.clear_bonds();
    ble.queue_line(status_ack("unpair"));
    return;
  }

  if(contains_token(line, "\"evt\":\"turn\"")) {
    app.snapshot.entries[0] = "turn event received";
    return;
  }

  if(contains_token(line, "\"total\"")) {
    app.snapshot.total = extract_int(line, "total", app.snapshot.total);
    app.snapshot.running = extract_int(line, "running", app.snapshot.running);
    app.snapshot.waiting = extract_int(line, "waiting", app.snapshot.waiting);
    app.snapshot.tokens = extract_int(line, "tokens", app.snapshot.tokens);
    app.snapshot.tokens_today = extract_int(line, "tokens_today", app.snapshot.tokens_today);

    const std::string msg = extract_string(line, "msg");
    if(!msg.empty()) {
      app.snapshot.msg = msg;
    }

    if(contains_token(line, "\"prompt\"")) {
      app.snapshot.prompt.active = true;
      app.snapshot.prompt.id = extract_string(line, "id");
      app.snapshot.prompt.tool = extract_string(line, "tool");
      app.snapshot.prompt.hint = extract_string(line, "hint");
      app.ui.page = UiPage::Prompt;
      app.ui.keep_awake = true;
    } else {
      app.snapshot.prompt = {};
      app.ui.keep_awake = false;
    }

    app.snapshot.last_update = get_absolute_time();
    app.link.connected = true;
  }
}

bool ProtocolEngine::contains_token(const std::string& line, const char* token) {
  return line.find(token) != std::string::npos;
}

int ProtocolEngine::extract_int(const std::string& line, const char* key, const int fallback) {
  const std::string needle = std::string("\"") + key + "\":";
  const std::size_t start = line.find(needle);
  if(start == std::string::npos) {
    return fallback;
  }

  std::size_t cursor = start + needle.size();
  while(cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor]))) {
    ++cursor;
  }

  std::size_t end = cursor;
  if(end < line.size() && line[end] == '-') {
    ++end;
  }
  while(end < line.size() && std::isdigit(static_cast<unsigned char>(line[end]))) {
    ++end;
  }

  if(end == cursor) {
    return fallback;
  }

  return std::atoi(line.substr(cursor, end - cursor).c_str());
}

std::string ProtocolEngine::extract_string(const std::string& line, const char* key) {
  const std::string needle = std::string("\"") + key + "\":";
  std::size_t start = line.find(needle);
  if(start == std::string::npos) {
    return {};
  }

  start += needle.size();
  while(start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
    ++start;
  }

  if(start >= line.size() || line[start] != '"') {
    return {};
  }

  ++start;
  const std::size_t end = line.find('"', start);
  if(end == std::string::npos) {
    return {};
  }

  return line.substr(start, end - start);
}
