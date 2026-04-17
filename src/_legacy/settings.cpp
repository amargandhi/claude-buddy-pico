#include "settings.hpp"

void SettingsStore::init(AppState& app) {
  app.settings = data_;
}

const SettingsData& SettingsStore::data() const {
  return data_;
}

void SettingsStore::set_device_name(AppState& app, const std::string& name) {
  data_.device_name = name;
  app.settings = data_;
}

void SettingsStore::set_owner_name(AppState& app, const std::string& name) {
  data_.owner_name = name;
  app.settings = data_;
}

void SettingsStore::clear_pairing(AppState& app) {
  data_.owner_name.clear();
  app.settings = data_;
  app.link.connected = false;
  app.link.secure = false;
}
