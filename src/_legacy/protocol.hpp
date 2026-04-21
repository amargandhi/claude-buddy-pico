#pragma once

#include <string>

#include "app_state.hpp"

class BleNusService;
class SettingsStore;

class ProtocolEngine {
public:
  void poll(BleNusService& ble, AppState& app, SettingsStore& settings);
  std::string make_permission_reply(const PromptState& prompt, bool approve) const;
  std::string make_status_reply(const AppState& app) const;

private:
  void handle_line(const std::string& line, BleNusService& ble, AppState& app, SettingsStore& settings);

  static bool contains_token(const std::string& line, const char* token);
  static int extract_int(const std::string& line, const char* key, int fallback);
  static std::string extract_string(const std::string& line, const char* key);
};
