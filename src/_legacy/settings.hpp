#pragma once

#include <string>

#include "app_state.hpp"

class SettingsStore {
public:
  void init(AppState& app);

  const SettingsData& data() const;

  void set_device_name(AppState& app, const std::string& name);
  void set_owner_name(AppState& app, const std::string& name);
  void clear_pairing(AppState& app);

private:
  SettingsData data_{};
};
