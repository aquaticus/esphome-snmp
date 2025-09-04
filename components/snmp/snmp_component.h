#pragma once

#include <string>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "SNMP_Agent.h"
#ifdef USE_ESP32
#include <WiFi.h>
#include <esp32/himem.h>
#include "esp_chip_info.h"
#endif
#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>
#include "esphome/components/sensor/sensor.h"   // NEW

namespace esphome {
namespace snmp {

class SNMPComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void loop() override;

  void set_read_community(const std::string &read_community) { read_community_ = read_community; }
  void set_write_community(const std::string &write_community) { write_community_ = write_community; }
  void set_contact(const std::string &contact) { contact_ = contact; }
  void set_location(const std::string &location) { location_ = location; }

  // NEW setters for sensor IDs
  void set_temperature_sensor(sensor::Sensor *s) { temperature_sensor_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { humidity_sensor_ = s; }

 protected:
  WiFiUDP udp_;
  SNMPAgent snmp_agent_;

  std::string read_community_ = "public";
  std::string write_community_ = "private";
  std::string contact_;
  std::string location_;

  // NEW sensor pointers
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  void setup_system_mib_();
  void setup_storage_mib_();
#ifdef USE_ESP32
  void setup_esp32_heap_mib_();
#endif
#ifdef USE_ESP8266
  void setup_esp8266_heap_mib_();
#endif
  void setup_chip_mib_();
  void setup_wifi_mib_();
  void setup_sensor_mib_();   // NEW

#ifdef USE_ESP32
  static int setup_psram_size(int *used);
#endif

  static uint32_t get_uptime() { return millis() / 10; }
  static uint32_t get_net_uptime();
  static std::string get_bssid();
#ifdef USE_ESP32
  static int get_ram_size_kb();
#endif

  // ---- NEW static helpers for SNMP integer callbacks ----
 private:
  static SNMPComponent *instance_;   // singleton pointer
  static int get_temperature();      // static callback
  static int get_humidity();         // static callback
};

}  // namespace snmp
}  // namespace esphome
