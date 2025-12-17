#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino.h>
#include "../config/demo_public_build.h"

// Mode enum for fast comparisons (replaces strcmp)
enum SystemMode : uint8_t {
  MODE_OFF = 0,
  MODE_AUTO = 1,
  MODE_MANUAL = 2
};

struct SystemProperties {
  float tubeLength = 10.0f;
  float tubeDiameter = 15.0f;
  float pumpHead = 3.0f;
  float reservoirVolume = 50.0f;
  float sunExposure = 0.3f;
};

struct SystemState {
  // DEMO NOTE: placeholder only. Set a real secret out-of-band for real hardware.
  char otaPassword[32] = DEMO_DEFAULT_OTA_PASSWORD;
  char currentMode[8] = "OFF";
  SystemMode cachedMode = MODE_OFF; // Cached enum for fast comparisons
  bool pumpState = false;
  bool manualPumpState = false;
  float SensorTemp = -127.0;
  float externalTemp = -127.0;
  float externalHumidity = -1.0; // -1 means invalid/not read
  float targetTemp = 5.0;
  float safetyTemp = 10.0;
  float systemEfficiency = 0.0; // 0-100%
  unsigned long efficiencyUpdateTime = 0;
  unsigned long totalOnTargetTime = 0;
  unsigned long lastEfficiencyCalc = 0;
  SystemProperties systemProps;

  unsigned long lastLogTime = 0;
  unsigned long startTime = 0;
  unsigned long messageDisplayTime = 0;
  unsigned long lastDisconnectTime = 0;
  unsigned long bypassStartTime = 0;
  const unsigned long BYPASS_DURATION = 1800000;
  int reconnectAttempts = 0;
  bool isLoggedIn = false;
  // DEMO NOTE: placeholder only. Set a real secret out-of-band for real hardware.
  char storedPassword[32] = DEMO_DEFAULT_ADMIN_PASSWORD;
  char wifiSSID[32] = "";
  char wifiPassword[32] = "";
  uint8_t wifiMode = 0;
  bool wifiConfigured = false;
  String httpRequest;
  char uiSuccessMsg[128] = ""; // Changed from String to char array
  bool loginError = false;
  bool factoryResetRequested = false;
  bool serialConfigMode = false;
  bool bypassEnabled = false;
  
  // Timed operations
  bool fanState = false;
  unsigned long pumpRunUntil = 0;
  unsigned long fanRunUntil = 0;

  char originalWifiSSID[32] = "";
  char originalWifiPassword[32] = "";
  bool hasOriginalCredentials = false;
  unsigned long lastRecoveryAttempt = 0;
  const unsigned long RECOVERY_INTERVAL = 1800000;

  bool serialLoggedIn = false;
  unsigned long serialLoginTime = 0;
  const unsigned long SERIAL_SESSION_TIMEOUT = 1800000;
  
  bool webSerialLoggedIn = false;
  unsigned long webSerialLoginTime = 0;
  const unsigned long WEB_SERIAL_SESSION_TIMEOUT = 1800000;

  uint8_t nightMode = 2;

  float tempHistory[90] = {-127.0f};
  float extTempHistory[90] = {-127.0f};
  float humidityHistory[90] = {-1.0f}; // -1 means invalid
  unsigned long timeHistory[90] = {0};
  uint8_t historyIndex = 0;
  bool historyInitialized = false;

  float aiOptimizationFactor = 1.0f;          // Πολλαπλασιαστής απόδοσης (0.8-1.3)
  float learnedEfficiencyBoost = 0.0f;        // Boost από μάθηση (0-0.5)
  float currentCoolingEfficiency = 0.0f;      // Τρέχουσα απόδοση (°C/sec)
  char optimizationStatus[32] = "Optimizing"; // Status μηνύματος
  
  // Notification tracking to prevent duplicate alerts
  bool sensorErrorNotified = false;
  bool highTempNotified = false;
  bool lowMemoryNotified = false;
};

extern Preferences prefs;
extern SystemState state;

// Helper function to update mode and cache
void setSystemMode(const char* mode);
SystemMode getCachedMode(); // Fast mode getter

void loadSettings();
void saveSettings();
void factoryReset();
void handleSerialConfiguration();
void scanWiFiNetworks();
bool testWiFiConnection(const char* ssid, const char* pass);
void startAPMode();

String processWebSerialCommand(String command);