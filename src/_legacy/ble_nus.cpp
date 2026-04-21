#include "ble_nus.hpp"

bool BleNusService::init(AppState& app) {
  initialised_ = true;
  app.link.connected = false;
  app.link.secure = false;
  return true;
}

void BleNusService::poll(AppState& app) {
  if(!initialised_) {
    return;
  }

  app.link.connected = connected_;
}

bool BleNusService::connected() const {
  return connected_;
}

bool BleNusService::read_line(std::string& line) {
  if(pending_rx_line_.empty()) {
    return false;
  }

  line = pending_rx_line_;
  pending_rx_line_.clear();
  return true;
}

void BleNusService::queue_line(const std::string& line) {
  // The current scaffold keeps the last outbound line around so the
  // protocol and UI flows can be exercised before the BTstack glue lands.
  last_tx_line_ = line;
}

void BleNusService::clear_bonds() {
  connected_ = false;
  pending_rx_line_.clear();
  last_tx_line_.clear();
}
