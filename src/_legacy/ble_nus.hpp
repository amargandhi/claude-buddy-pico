#pragma once

#include <string>

#include "app_state.hpp"

class BleNusService {
public:
  bool init(AppState& app);
  void poll(AppState& app);

  bool connected() const;
  bool read_line(std::string& line);
  void queue_line(const std::string& line);
  void clear_bonds();

private:
  bool initialised_ = false;
  bool connected_ = false;
  std::string pending_rx_line_;
  std::string last_tx_line_;
};
