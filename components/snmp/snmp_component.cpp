#include "snmp_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/components/wifi/wifi_component.h"

// Integration test available: https://github.com/aquaticus/esphome_snmp_tests

namespace esphome {
namespace snmp {

#define CUSTOM_OID ".1.3.9999."

static const char *const TAG = "snmp";

// ---- NEW static instance pointer ----
SNMPComponent *SNMPComponent::instance_ = nullptr;

uint32_t SNMPComponent::get_net_uptime() {
#ifdef WIFI_CONNECTED_TIMESTAMP_AVAILABLE
  return (millis() - wifi::global_wifi_component->wifi_connected_timestamp()) / 10;
#else
  return 0; // not available
#endif
}

// ---- NEW static functions for SNMP callbacks ----
int SNMPComponent::get_temperature() {
  if (instance_ == nullptr || instance_->temperature_sensor_ == nullptr) return 0;
  if (!instance_->temperature_sensor_->has_state()) return 0;
  return static_cast<int>(instance_->temperature_sensor_->state * 10.0f);  // tenths
}

int SNMPComponent::get_humidity() {
  if (instance_ == nullptr || instance_->humidity_sensor_ == nullptr) return 0;
  if (!instance_->humidity_sensor_->has_state()) return 0;
  return static_cast<int>(instance_->humidity_sensor_->state * 10.0f);  // tenths
}


void SNMPComponent::setup_system_mib_() {
  const char *desc_fmt = "ESPHome version " ESPHOME_VERSION " compiled %s, Board " ESPHOME_BOARD;
  char description[128];
  snprintf(description, sizeof(description), desc_fmt, App.get_compilation_time().c_str());
  snmp_agent_.addReadOnlyStaticStringHandler(RFC1213_OID_sysDescr, description);
  snmp_agent_.addDynamicReadOnlyStringHandler(RFC1213_OID_sysName, []() -> std::string { return App.get_name(); });
  snmp_agent_.addReadOnlyIntegerHandler(RFC1213_OID_sysServices, 64 /*=2^(7-1) applications*/);
  snmp_agent_.addOIDHandler(RFC1213_OID_sysObjectID,
#ifdef USE_ESP32
                           CUSTOM_OID "32"
#else
                           CUSTOM_OID "8266"
#endif
  );
  snmp_agent_.addReadOnlyStaticStringHandler(RFC1213_OID_sysContact, contact_.c_str());
  snmp_agent_.addReadOnlyStaticStringHandler(RFC1213_OID_sysLocation, location_.c_str());
}

#ifdef USE_ESP32
int SNMPComponent::setup_psram_size(int *used) {
  int total_size = 0;
  *used = 0;
  size_t free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  bool available = free_size > 0;
  if (available) {
    total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (total_size > 0) {
      *used = total_size - free_size;
    }
  }
  return total_size;
}
#endif

void SNMPComponent::setup_storage_mib_() {
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.1.1", 1);
  snmp_agent_.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.25.2.3.1.3.1", "FLASH");
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.4.1", 1);
  snmp_agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.1",
                                      []() -> int { return ESP.getFlashChipSize(); });
  snmp_agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.1",
                                      []() -> int { return ESP.getSketchSize(); });
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.1.2", 2);
  snmp_agent_.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.25.2.3.1.3.2", "SPI RAM");
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.4.2", 1);

#ifdef USE_ESP32
  snmp_agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.2", []() -> int {
    int u;
    return setup_psram_size(&u);
  });
  snmp_agent_.addDynamicIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.2", []() -> int {
    int u;
    setup_psram_size(&u);
    return u;
  });
#endif

#ifdef USE_ESP8266
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.5.2", 0);
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.3.1.6.2", 0);
#endif

#ifdef USE_ESP32
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.2", get_ram_size_kb());
#endif
#ifdef USE_ESP8266
  snmp_agent_.addReadOnlyIntegerHandler(".1.3.6.1.2.1.25.2.2", 160);
#endif
}

std::string SNMPComponent::get_bssid() {
  char buf[30];
  wifi::bssid_t bssid = wifi::global_wifi_component->wifi_bssid();
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  return buf;
}

#ifdef USE_ESP32
void SNMPComponent::setup_esp32_heap_mib_() {
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "32.1.0", []() -> int { return ESP.getHeapSize(); });
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "32.2.0", []() -> int { return ESP.getFreeHeap(); });
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "32.3.0", []() -> int { return ESP.getMinFreeHeap(); });
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "32.4.0", []() -> int { return ESP.getMaxAllocHeap(); });
}
#endif

#ifdef USE_ESP8266
void SNMPComponent::setup_esp8266_heap_mib_() {
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "8266.1.0", []() -> int { return ESP.getFreeHeap(); });
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "8266.2.0", []() -> int { return ESP.getHeapFragmentation(); });
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "8266.3.0", []() -> int { return ESP.getMaxFreeBlockSize(); });
}
#endif

void SNMPComponent::setup_chip_mib_() {
#if ESP32
  snmp_agent_.addReadOnlyIntegerHandler(CUSTOM_OID "2.1.0", 32);
#endif
#if ESP8266
  snmp_agent_.addReadOnlyIntegerHandler(CUSTOM_OID "2.1.0", 8266);
#endif
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "2.2.0", []() -> int { return ESP.getCpuFreqMHz(); });
#if ESP32
  snmp_agent_.addDynamicReadOnlyStringHandler(CUSTOM_OID "2.3.0", []() -> std::string { return ESP.getChipModel(); });
#endif
#ifdef USE_ESP8266
  snmp_agent_.addDynamicReadOnlyStringHandler(CUSTOM_OID "2.3.0",
                                             []() -> std::string { return ESP.getCoreVersion().c_str(); });
#endif
#if USE_ESP32
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "2.4.0", []() -> int { return ESP.getChipCores(); });
#endif
#if USE_ESP8266
  snmp_agent_.addReadOnlyIntegerHandler(CUSTOM_OID "2.4.0", 1);
#endif
#if ESP32
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "2.5.0", []() -> int { return ESP.getChipRevision(); });
#endif
#if ESP8266
  snmp_agent_.addReadOnlyIntegerHandler(CUSTOM_OID "2.5.0", 0 /*no data for ESP8266*/);
#endif
}

void SNMPComponent::setup_wifi_mib_() {
  snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "4.1.0",
                                      []() -> int { return wifi::global_wifi_component->wifi_rssi(); });
  snmp_agent_.addDynamicReadOnlyStringHandler(CUSTOM_OID "4.2.0", get_bssid);
  snmp_agent_.addDynamicReadOnlyStringHandler(CUSTOM_OID "4.3.0",
                                             []() -> std::string { return wifi::global_wifi_component->wifi_ssid(); });
  snmp_agent_.addDynamicReadOnlyStringHandler(
      CUSTOM_OID "4.4.0", []() -> std::string {
        const auto& ip_array = wifi::global_wifi_component->wifi_sta_ip_addresses();
        return ip_array.size() ? wifi::global_wifi_component->wifi_sta_ip_addresses()[0].str() : "";
      });
}

// ---- NEW: add sensor OIDs ----
void SNMPComponent::setup_sensor_mib_() {
  if (temperature_sensor_ != nullptr) {
    snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "10.1.0", get_temperature);
  }
  if (humidity_sensor_ != nullptr) {
    snmp_agent_.addDynamicIntegerHandler(CUSTOM_OID "10.2.0", get_humidity);
  }
}

void SNMPComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SNMP...");
  snmp_agent_ = SNMPAgent(read_community_.c_str(), write_community_.c_str());
  snmp_agent_.setUDP(&udp_);
  snmp_agent_.begin();

  // store singleton instance
  instance_ = this;

  snmp_agent_.addDynamicReadOnlyTimestampHandler(RFC1213_OID_sysUpTime, get_net_uptime);
  snmp_agent_.addDynamicReadOnlyTimestampHandler(".1.3.6.1.2.1.25.1.1.0", get_uptime);

  setup_system_mib_();
  setup_storage_mib_();
#ifdef USE_ESP32
  setup_esp32_heap_mib_();
#endif
#ifdef USE_ESP8266
  setup_esp8266_heap_mib_();
#endif
  setup_chip_mib_();
  setup_wifi_mib_();
  setup_sensor_mib_();   // NEW
  snmp_agent_.sortHandlers();
}

void SNMPComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SNMP Config:");
  ESP_LOGCONFIG(TAG, "  Contact: \"%s\"", contact_.c_str());
  ESP_LOGCONFIG(TAG, "  Location: \"%s\"", location_.c_str());
  ESP_LOGCONFIG(TAG, "  Read Community: \"%s\"", read_community_.c_str());
  ESP_LOGCONFIG(TAG, "  Write Community: \"%s\"", write_community_.c_str());
}

void SNMPComponent::loop() { snmp_agent_.loop(); }

#ifdef USE_ESP32
int SNMPComponent::get_ram_size_kb() {
  const int chip_esp32 = 1;
  const int chip_esp32_s2 = 2;
  const int chip_esp32_s3 = 9;
  const int chip_esp32_c3 = 5;
  const int chip_esp32_h2 = 6;
  const int chip_esp32_c2 = 12;
  const int chip_esp32_c6 = 13;

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  switch ((int) chip_info.model) {
    case chip_esp32: return 520;
    case chip_esp32_s2: return 320;
    case chip_esp32_s3: return 512;
    case chip_esp32_c2:
    case chip_esp32_c3:
    case chip_esp32_c6: return 400;
    case chip_esp32_h2: return 256;
  }
  return 0;
}
#endif

}  // namespace snmp
}  // namespace esphome
