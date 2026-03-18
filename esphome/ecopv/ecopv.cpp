#include "ecopv.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ecopv {

static const char *const TAG = "ecopv";

void EcoPVComponent::setup() {
  ESP_LOGI(TAG, "Setting up EcoPV component");
  this->last_read_time_ = millis();
  this->last_contact_time_ = millis();
  
  // Send initial request for parameters
  delay(1000);
  this->request_version();
}

void EcoPVComponent::loop() {
  uint32_t now = millis();
  
  // Check serial data continuously
  this->read_serial_data_();
  
  // Request parameters periodically
  if (now - this->last_read_time_ >= this->scan_interval_) {
    this->last_read_time_ = now;
    this->request_parameters();
  }

  // Check for timeout on EcoPV contact
  if (now - this->last_contact_time_ > 10000) {  // 10 seconds timeout
    if (this->contact_established_) {
      this->contact_established_ = false;
      if (this->contact_established_binary_) {
        this->contact_established_binary_->publish_state(false);
      }
      ESP_LOGW(TAG, "Lost contact with EcoPV");
    }
  }

  // Update status text sensor
  if (this->status_text_sensor_) {
    this->status_text_sensor_->publish_state(
        this->contact_established_ ? "Connected" : "Disconnected");
  }
}

void EcoPVComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EcoPV Component");
  ESP_LOGCONFIG(TAG, "  Scan interval: %u ms", this->scan_interval_);
}

void EcoPVComponent::read_serial_data_() {
  while (this->available()) {
    uint8_t byte = this->read();
    
    if (byte == '#') {
      // End of message
      if (!this->rx_buffer_.empty()) {
        this->process_line_(this->rx_buffer_);
        this->rx_buffer_.clear();
      }
    } else if (byte == '\n' || byte == '\r') {
      // Skip line endings
      continue;
    } else {
      // Add to buffer
      this->rx_buffer_ += static_cast<char>(byte);
      
      // Prevent buffer overflow
      if (this->rx_buffer_.length() >= this->RX_BUFFER_SIZE) {
        this->rx_buffer_.clear();
      }
    }
  }
}

void EcoPVComponent::process_line_(const std::string &line) {
  if (line.empty()) {
    return;
  }

  ESP_LOGV(TAG, "Received: %s", line.c_str());

  // Update last contact time
  this->last_contact_time_ = millis();
  
  // Establish contact
  if (!this->contact_established_) {
    this->contact_established_ = true;
    if (this->contact_established_binary_) {
      this->contact_established_binary_->publish_state(true);
    }
    ESP_LOGI(TAG, "Established contact with EcoPV");
  }

  // Parse message type
  if (line.find("STATS") == 0) {
    this->parse_stats_(line);
  } else if (line.find("PARAM") == 0) {
    this->parse_params_(line);
  } else if (line.find("VERSION") == 0) {
    this->parse_version_(line);
  } else if (line.find("Routeur running") != std::string::npos) {
    if (this->router_running_binary_) {
      this->router_running_binary_->publish_state(true);
    }
  }
}

void EcoPVComponent::parse_stats_(const std::string &data) {
  // Format: STATS,Vrms,Irms,Pact,Papp,Prouted,Pexport[,...]
  auto values = this->split_string_(data, ',');
  
  if (values.size() < 7) {
    ESP_LOGW(TAG, "Invalid STATS message format");
    return;
  }

  // Parse values (skip "STATS" at index 0)
  try {
    if (this->voltage_sensor_) {
      this->voltage_sensor_->publish_state(std::stof(values[1]));
    }
    
    if (this->current_sensor_) {
      this->current_sensor_->publish_state(std::stof(values[2]));
    }
    
    if (this->power_active_sensor_) {
      this->power_active_sensor_->publish_state(std::stof(values[3]));
    }
    
    if (this->power_apparent_sensor_) {
      this->power_apparent_sensor_->publish_state(std::stof(values[4]));
    }
    
    if (this->power_routed_sensor_) {
      this->power_routed_sensor_->publish_state(std::stof(values[5]));
    }
    
    if (this->power_export_sensor_) {
      this->power_export_sensor_->publish_state(std::stof(values[6]));
    }

    // Additional values if present (energy indices, temperature, etc.)
    if (values.size() > 7 && this->pv_reference_power_sensor_) {
      this->pv_reference_power_sensor_->publish_state(std::stof(values[7]));
    }

  } catch (const std::invalid_argument &e) {
    ESP_LOGW(TAG, "Error parsing STATS values: %s", e.what());
  }
}

void EcoPVComponent::parse_params_(const std::string &data) {
  // Format: PARAM,param1,param2,param3,...
  auto values = this->split_string_(data, ',');
  
  if (values.size() < 2) {
    ESP_LOGW(TAG, "Invalid PARAM message format");
    return;
  }

  // Parameters are received as a comma-separated list
  // Indices don't match exactly with sensor assignments without mapping
  ESP_LOGD(TAG, "Received %zu parameters from EcoPV", values.size() - 1);
}

void EcoPVComponent::parse_version_(const std::string &data) {
  // Format: VERSION,version_string
  auto values = this->split_string_(data, ',');
  
  if (values.size() >= 2 && this->version_text_sensor_) {
    this->version_text_sensor_->publish_state(values[1]);
    ESP_LOGI(TAG, "EcoPV Version: %s", values[1].c_str());
  }
}

std::vector<std::string> EcoPVComponent::split_string_(const std::string &str,
                                                      char delimiter) {
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = str.find(delimiter);
  
  while (end != std::string::npos) {
    tokens.emplace_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }
  
  tokens.emplace_back(str.substr(start));
  return tokens;
}

void EcoPVComponent::send_command(const std::string &command) {
  ESP_LOGD(TAG, "Sending command: %s", command.c_str());
  this->write_str(command.c_str());
  this->write_byte('#');
  delay(10);
}

void EcoPVComponent::request_parameters() {
  this->send_command("PARAM,END");
}

void EcoPVComponent::request_version() {
  this->send_command("VERSION,END");
}

void EcoPVComponent::set_ssr_mode(const std::string &mode) {
  std::string cmd = "SETSSR,";
  if (mode == "Stop") {
    cmd += "STOP";
  } else if (mode == "Force") {
    cmd += "FORCE";
  } else if (mode == "Auto") {
    cmd += "AUTO";
  } else {
    ESP_LOGW(TAG, "Invalid SSR mode: %s", mode.c_str());
    return;
  }
  cmd += ",END";
  this->send_command(cmd);
}

void EcoPVComponent::set_relay_mode(const std::string &mode) {
  std::string cmd = "SETRELAY,";
  if (mode == "Stop") {
    cmd += "STOP";
  } else if (mode == "Force") {
    cmd += "FORCE";
  } else if (mode == "Auto") {
    cmd += "AUTO";
  } else {
    ESP_LOGW(TAG, "Invalid Relay mode: %s", mode.c_str());
    return;
  }
  cmd += ",END";
  this->send_command(cmd);
}

void EcoPVComponent::save_config() {
  this->send_command("SAVECFG,END");
}

void EcoPVComponent::reset_energy_indices() {
  this->send_command("INDX0,END");
  this->send_command("SAVEINDX,END");
}

void EcoPVComponent::soft_restart() {
  this->send_command("RESET,END");
}

}  // namespace ecopv
}  // namespace esphome
