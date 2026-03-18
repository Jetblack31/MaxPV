#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/button/button.h"
#include <vector>

namespace esphome {
namespace ecopv {

class EcoPVComponent : public Component, public uart::UARTDevice {
 public:
  EcoPVComponent(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_scan_interval(uint32_t scan_interval) {
    this->scan_interval_ = scan_interval;
  }

  // Sensor registration methods
  void set_voltage_sensor(sensor::Sensor *sensor) {
    this->voltage_sensor_ = sensor;
  }
  void set_current_sensor(sensor::Sensor *sensor) {
    this->current_sensor_ = sensor;
  }
  void set_power_active_sensor(sensor::Sensor *sensor) {
    this->power_active_sensor_ = sensor;
  }
  void set_power_apparent_sensor(sensor::Sensor *sensor) {
    this->power_apparent_sensor_ = sensor;
  }
  void set_power_routed_sensor(sensor::Sensor *sensor) {
    this->power_routed_sensor_ = sensor;
  }
  void set_power_export_sensor(sensor::Sensor *sensor) {
    this->power_export_sensor_ = sensor;
  }
  void set_energy_routed_sensor(sensor::Sensor *sensor) {
    this->energy_routed_sensor_ = sensor;
  }
  void set_energy_imported_sensor(sensor::Sensor *sensor) {
    this->energy_imported_sensor_ = sensor;
  }
  void set_energy_exported_sensor(sensor::Sensor *sensor) {
    this->energy_exported_sensor_ = sensor;
  }
  void set_pv_counter_sensor(sensor::Sensor *sensor) {
    this->pv_counter_sensor_ = sensor;
  }
  void set_temperature_sensor(sensor::Sensor *sensor) {
    this->temperature_sensor_ = sensor;
  }
  void set_pv_reference_power_sensor(sensor::Sensor *sensor) {
    this->pv_reference_power_sensor_ = sensor;
  }
  void set_relay_time_sensor(sensor::Sensor *sensor) {
    this->relay_time_sensor_ = sensor;
  }

  // Binary sensor methods
  void set_contact_established_binary(binary_sensor::BinarySensor *sensor) {
    this->contact_established_binary_ = sensor;
  }
  void set_router_running_binary(binary_sensor::BinarySensor *sensor) {
    this->router_running_binary_ = sensor;
  }

  // Text sensor methods
  void set_version_text(text_sensor::TextSensor *sensor) {
    this->version_text_sensor_ = sensor;
  }
  void set_status_text(text_sensor::TextSensor *sensor) {
    this->status_text_sensor_ = sensor;
  }

  // Methods to send commands to EcoPV
  void send_command(const std::string &command);
  void request_parameters();
  void request_version();
  void set_ssr_mode(const std::string &mode);
  void set_relay_mode(const std::string &mode);
  void save_config();
  void reset_energy_indices();
  void soft_restart();

 private:
  uint32_t scan_interval_{1000};
  uint32_t last_read_time_{0};
  uint32_t last_contact_time_{0};
  bool contact_established_{false};

  // Buffers
  std::string rx_buffer_;
  const uint8_t RX_BUFFER_SIZE = 256;

  // Sensors
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_active_sensor_{nullptr};
  sensor::Sensor *power_apparent_sensor_{nullptr};
  sensor::Sensor *power_routed_sensor_{nullptr};
  sensor::Sensor *power_export_sensor_{nullptr};
  sensor::Sensor *energy_routed_sensor_{nullptr};
  sensor::Sensor *energy_imported_sensor_{nullptr};
  sensor::Sensor *energy_exported_sensor_{nullptr};
  sensor::Sensor *pv_counter_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pv_reference_power_sensor_{nullptr};
  sensor::Sensor *relay_time_sensor_{nullptr};

  // Binary sensors
  binary_sensor::BinarySensor *contact_established_binary_{nullptr};
  binary_sensor::BinarySensor *router_running_binary_{nullptr};

  // Text sensors
  text_sensor::TextSensor *version_text_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};

  // Methods
  void read_serial_data_();
  void process_line_(const std::string &line);
  void parse_stats_(const std::string &data);
  void parse_params_(const std::string &data);
  void parse_version_(const std::string &data);
  std::vector<std::string> split_string_(const std::string &str, char delimiter);
};

}  // namespace ecopv
}  // namespace esphome
