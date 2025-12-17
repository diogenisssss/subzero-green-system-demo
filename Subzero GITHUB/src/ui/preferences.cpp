#include <ESPmDNS.h>
#include "preferences.h"
#include "../hardware/display.h"
#include "../hardware/wifimanager.h"
#include "calibration_wizard.h"
#include "../algorithms/ai_optimizer.h"
#include "../algorithms/pattern_learner.h"

// External declarations - defined in main.cpp
extern CalibrationWizard calibrationWizard;
extern AIOptimizationEngine aiOptimizer;
extern PatternLearner patternLearner;    

Preferences prefs;
SystemState state;

// Demo build:
// - No real credentials are committed to the repo.
// - Defaults come from `demo_public_build.h` placeholders.
const char DEFAULT_SSID[] = "";
const char DEFAULT_WIFI_PASS[] = "";

// Helper function to update mode and cache simultaneously
void setSystemMode(const char* mode) {
    strncpy(state.currentMode, mode, sizeof(state.currentMode) - 1);
    state.currentMode[sizeof(state.currentMode) - 1] = '\0';
    
    // Update cached enum
    if (strcmp(mode, "AUTO") == 0) {
        state.cachedMode = MODE_AUTO;
    } else if (strcmp(mode, "MANUAL") == 0) {
        state.cachedMode = MODE_MANUAL;
    } else {
        state.cachedMode = MODE_OFF;
    }
}

SystemMode getCachedMode() {
    return state.cachedMode;
}

void loadSettings() {
    prefs.begin("subzero", true);
    
    state.targetTemp = prefs.getFloat("targetTemp", 5.0);
    state.safetyTemp = prefs.getFloat("safetyTemp", 10.0);
    
    // Load strings with defaults - OPTIMIZED: Use static buffer instead of String objects
    static char buffer[64];
    size_t len = prefs.getString("otaPassword", buffer, sizeof(buffer));
    if (len == 0) {
        strncpy(buffer, DEMO_DEFAULT_OTA_PASSWORD, sizeof(buffer));
    }
    strncpy(state.otaPassword, buffer, sizeof(state.otaPassword));
    state.otaPassword[sizeof(state.otaPassword) - 1] = '\0';
    
    const uint8_t mode = prefs.getUChar("mode", 2);
    const char* modes[] = {"AUTO", "MANUAL", "OFF"};
    const uint8_t modeIndex = (mode < 3) ? mode : 2;
    setSystemMode(modes[modeIndex]); // Use helper to update both string and cache
    
    state.lastDisconnectTime = prefs.getULong("lastDiscTime", 0);
    state.reconnectAttempts = prefs.getUChar("reconnAttempts", 0);
    state.manualPumpState = prefs.getBool("manualPump", false);
    
    // Load WiFi credentials - OPTIMIZED: Use static buffer
    len = prefs.getString("password", buffer, sizeof(buffer));
    if (len == 0) {
        strncpy(buffer, DEMO_DEFAULT_ADMIN_PASSWORD, sizeof(buffer));
    }
    strncpy(state.storedPassword, buffer, sizeof(state.storedPassword));
    state.storedPassword[sizeof(state.storedPassword) - 1] = '\0';
    
    len = prefs.getString("wifiSSID", buffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
    strncpy(state.wifiSSID, buffer, sizeof(state.wifiSSID));
    state.wifiSSID[sizeof(state.wifiSSID) - 1] = '\0';
    
    len = prefs.getString("wifiPass", buffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
    strncpy(state.wifiPassword, buffer, sizeof(state.wifiPassword));
    state.wifiPassword[sizeof(state.wifiPassword) - 1] = '\0';
    
    state.wifiConfigured = prefs.getBool("wifiConfigured", false);
    state.wifiMode = prefs.getUChar("wifiMode", 0);
    
    // Load system properties
    state.systemProps.tubeLength = prefs.getFloat("tubeLength", 10.0f);
    state.systemProps.tubeDiameter = prefs.getFloat("tubeDiameter", 15.0f);
    state.systemProps.pumpHead = prefs.getFloat("pumpHead", 3.0f);
    state.systemProps.reservoirVolume = prefs.getFloat("reservoirVol", 50.0f);
    state.systemProps.sunExposure = prefs.getFloat("sunExposure", 0.3f);
    
    // Load original credentials - OPTIMIZED: Use static buffer
    len = prefs.getString("origSSID", buffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
    strncpy(state.originalWifiSSID, buffer, sizeof(state.originalWifiSSID));
    state.originalWifiSSID[sizeof(state.originalWifiSSID) - 1] = '\0';
    
    len = prefs.getString("origPass", buffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
    strncpy(state.originalWifiPassword, buffer, sizeof(state.originalWifiPassword));
    state.originalWifiPassword[sizeof(state.originalWifiPassword) - 1] = '\0';
    
    state.hasOriginalCredentials = prefs.getBool("hasOrigCred", false);
    state.lastRecoveryAttempt = prefs.getULong("lastRecAttempt", 0);
    
    // Reset session states
    state.serialLoggedIn = state.webSerialLoggedIn = false;
    state.serialLoginTime = state.webSerialLoginTime = 0;
    
    for(int i = 0; i < 90; i++) {
        state.tempHistory[i] = -127.0f;  // Use -127 for invalid
        state.extTempHistory[i] = -127.0f;
        state.humidityHistory[i] = -1.0f; // -1 means invalid
        state.timeHistory[i] = 0;
    }

    state.historyIndex = 0;
    state.historyInitialized = false;
    
    prefs.end();

    // Force system to OFF mode on startup
    setSystemMode("OFF");
}

void printMemoryStats() {
    Serial.println("\n=== MEMORY USAGE ===");
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Total Heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("Max Alloc Heap: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
}

void saveSettings() {
    prefs.begin("subzero", false);
    
    prefs.putFloat("targetTemp", state.targetTemp);
    prefs.putFloat("safetyTemp", state.safetyTemp);
    prefs.putString("otaPassword", state.otaPassword);
    
    // Use cached mode for fast comparison
    uint8_t modeValue = 2;
    if (state.cachedMode == MODE_AUTO) {
        modeValue = 0;
    } else if (state.cachedMode == MODE_MANUAL) {
        modeValue = 1;
    }
    
    prefs.putULong("lastDiscTime", state.lastDisconnectTime);
    prefs.putUChar("reconnAttempts", state.reconnectAttempts);
    prefs.putUChar("mode", modeValue);
    prefs.putBool("manualPump", state.manualPumpState);
    prefs.putString("password", state.storedPassword);
    prefs.putString("wifiSSID", state.wifiSSID);
    prefs.putString("wifiPass", state.wifiPassword);
    prefs.putBool("wifiConfigured", state.wifiConfigured);
    prefs.putUChar("wifiMode", state.wifiMode);
    
    prefs.putFloat("tubeLength", state.systemProps.tubeLength);
    prefs.putFloat("tubeDiameter", state.systemProps.tubeDiameter);
    prefs.putFloat("pumpHead", state.systemProps.pumpHead);
    prefs.putFloat("reservoirVol", state.systemProps.reservoirVolume);
    prefs.putFloat("sunExposure", state.systemProps.sunExposure);
    
    prefs.putString("origSSID", state.originalWifiSSID);
    prefs.putString("origPass", state.originalWifiPassword);
    prefs.putBool("hasOrigCred", state.hasOriginalCredentials);
    prefs.putULong("lastRecAttempt", state.lastRecoveryAttempt);
    
    prefs.end();
}

void factoryReset() {
    Serial.println("Starting factory reset...");
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100); // Keep delay - WiFi hardware needs time
    
    prefs.begin("subzero", false);
    prefs.clear();
    prefs.end();
    
    // Reset all state variables
    memset(&state, 0, sizeof(state));
    
    // Set default values
    state.targetTemp = 5.0;
    setSystemMode("OFF"); // Use helper to update both
    strcpy(state.storedPassword, DEMO_DEFAULT_ADMIN_PASSWORD);
    strcpy(state.otaPassword, DEMO_DEFAULT_OTA_PASSWORD);
    
    // Reset system properties
    state.systemProps.tubeLength = 10.0f;
    state.systemProps.tubeDiameter = 15.0f;
    state.systemProps.pumpHead = 3.0f;
    state.systemProps.reservoirVolume = 50.0f;
    state.systemProps.sunExposure = 0.3f;
    
    Serial.println("Factory reset completed. Restarting...");
    delay(1000); // Keep delay - needed before restart
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100); // Keep delay - WiFi hardware needs time
    
    ESP.restart();
}

void handleSerialConfiguration() {
    static String serialInput = "";
    static int configStep = 0;
    static String tempSSID = "", tempPassword = "";
    static int networkCount = 0;
    static String availableNetworks[15];
    
    // Serial access granted by default (no password required)
    if (!state.serialLoggedIn) {
                        state.serialLoggedIn = true;
                        state.serialLoginTime = millis();
    }

    // Process serial commands
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialInput.length() > 0) {
                serialInput.trim();
                Serial.println("");
                
                // Handle calibration input first
                if (calibrationWizard.isActive()) {
                    calibrationWizard.processCalibrationInput(serialInput);
                    serialInput = "";
                    return;
                }
                
                // Command processing
                if (serialInput == "/help") {
                    Serial.println(R"(
=== SUBZERO GREEN SYSTEMS - AVAILABLE COMMANDS ===

STATUS & MONITORING:
  /status       - System status overview
  /patterns     - Show learned business patterns
  /aidebug      - Show AI efficiency data
  /maintenance  - Check maintenance needs
  /memory       - Memory usage statistics

SAFETY SETTINGS:
  /safetytemp   - Set safety temperature limit (8.0 - 20.0C)

SETTINGS & CONFIGURATION:
  /settings     - WiFi configuration wizard
  /scan         - Scan WiFi networks
  /ap           - Start Access Point mode
  /calibrate    - System calibration wizard
  /resetpatterns- Reset learning data
  /ota          - Change OTA password (for web serial)

SYSTEM OPERATIONS:
  /restart      - System restart
  /reset        - Factory reset (CAUTION)
  /logout       - Terminate serial session

Type any command above to execute it.)");
                }
                else if (serialInput == "/logout") {
                    state.serialLoggedIn = false;
                    Serial.println("Serial session terminated");
                }
                else if (serialInput == "/memory") {
                    printMemoryStats();
                }
                else if (serialInput == "/ota") {
                    Serial.println("\n=== OTA PASSWORD CHANGE ===");
                    Serial.print("Current OTA password: "); Serial.println(state.otaPassword);
                    Serial.println("Enter new OTA password (or 'cancel' to abort):");
                    
                    serialInput = "";
                    while (!Serial.available()) yield(); // OPTIMIZED: Non-blocking
                    
                    String newOtaPass = Serial.readString(); newOtaPass.trim();
                    if (newOtaPass == "cancel") {
                        Serial.println("OTA password change cancelled.");
                    } else if (newOtaPass.length() > 0) {
                        strncpy(state.otaPassword, newOtaPass.c_str(), sizeof(state.otaPassword));
                        state.otaPassword[sizeof(state.otaPassword) - 1] = '\0';
                        saveSettings();
                        Serial.println("OTA password updated successfully!");
                        Serial.print("New OTA password: "); Serial.println(state.otaPassword);
                    } else {
                        Serial.println("ERROR: OTA password cannot be empty");
                    }
                }
                else if (serialInput == "/safetytemp") {
                    Serial.println("\n=== SAFETY TEMPERATURE ===");
                    Serial.print("Current safety temperature: "); Serial.print(state.safetyTemp, 1); Serial.println(" C");
                    Serial.println("Enter new safety temperature (8.0 to 20.0) or 'cancel' to abort:");
                    
                    serialInput = "";
                    while (!Serial.available()) yield(); // OPTIMIZED: Non-blocking
                    
                    String tempInput = Serial.readString(); tempInput.trim();
                    if (tempInput == "cancel") {
                        Serial.println("Safety temperature change cancelled.");
                    } else {
                        float newSafetyTemp = tempInput.toFloat();
                        if (newSafetyTemp >= 8.0f && newSafetyTemp <= 40.0f) {
                            float oldSafetyTemp = state.safetyTemp;
                            state.safetyTemp = newSafetyTemp;
                            saveSettings();
                            Serial.println("\n=== SAFETY TEMPERATURE UPDATED ===");
                            Serial.print("Previous: "); Serial.print(oldSafetyTemp, 1); Serial.println(" C");
                            Serial.print("New safety limit: "); Serial.print(state.safetyTemp, 1); Serial.println(" C");
                        } else {
                            Serial.println("ERROR: Safety temperature must be between 8.0 C and 20.0 C");
                            Serial.print("Current safety limit remains: "); Serial.print(state.safetyTemp, 1); Serial.println(" C");
                        }
                    }
                }
                else if (serialInput == "/status") {
                    Serial.println("\n=== SYSTEM STATUS ===");
                    Serial.println("NETWORK:");
                    Serial.print("   WiFi SSID: "); Serial.println(state.wifiSSID);
                    Serial.print("   WiFi Configured: "); Serial.println(state.wifiConfigured ? "Yes" : "No");
                    Serial.print("   WiFi Mode: "); Serial.println(state.wifiMode == 0 ? "Station" : "AP");
                    
                    if (state.hasOriginalCredentials) {
                        Serial.print("   Original WiFi: "); Serial.println(state.originalWifiSSID);
                        Serial.println("   Auto-recovery: Active (every 30 minutes)");
                    }
                    if (WiFi.status() == WL_CONNECTED) {
                        Serial.print("   IP Address: "); Serial.println(WiFi.localIP());
                    } else {
                        Serial.println("   WiFi Status: Not connected");
                    }
                    
                    Serial.println("\nTEMPERATURE:");
                    Serial.print("   Liquid Temp: "); Serial.print(state.SensorTemp, 1); Serial.println("C");
                    Serial.print("   External Temp: "); Serial.print(state.externalTemp, 1); Serial.println("C");
                    Serial.print("   Target Temp: "); Serial.print(state.targetTemp, 1); Serial.println("C");
                    Serial.print("   Safety Temp: "); Serial.print(state.safetyTemp, 1); Serial.println("C");
                    
                    Serial.println("\nSYSTEM:");
                    Serial.print("   Current Mode: "); Serial.println(state.currentMode);
                    Serial.print("   Pump State: "); Serial.println(state.pumpState ? "ON" : "OFF");
                    
                    unsigned long sessionTime = (millis() - state.serialLoginTime) / 1000;
                    Serial.print("   Serial Session: "); Serial.print(sessionTime / 60); Serial.print("m "); Serial.print(sessionTime % 60); Serial.println("s");
                    
                    Serial.println("\nSECURITY:");
                    Serial.print("   OTA Password: "); Serial.println(state.otaPassword);
                    Serial.print("   Web Serial Access: "); Serial.println(state.webSerialLoggedIn ? "ACTIVE" : "Inactive");
                    
                    Serial.println("\nSYSTEM PROPERTIES:");
                    Serial.print("   Tube Length: "); Serial.print(state.systemProps.tubeLength); Serial.println(" m");
                    Serial.print("   Tube Diameter: "); Serial.print(state.systemProps.tubeDiameter); Serial.println(" mm");
                    Serial.print("   Pump Head: "); Serial.print(state.systemProps.pumpHead); Serial.println(" m");
                    Serial.print("   Reservoir Volume: "); Serial.print(state.systemProps.reservoirVolume); Serial.println(" L");
                    Serial.print("   Sun Exposure: "); Serial.println(state.systemProps.sunExposure);
                }
                else if (serialInput == "/patterns") patternLearner.printLearnedPatterns();
                else if (serialInput == "/aidebug") aiOptimizer.printAIDebug();
                else if (serialInput == "/maintenance") {
                    if (!patternLearner.checkMaintenanceNeeds()) {
                        Serial.println("System maintenance: All systems normal");
                    }
                }

                else if (serialInput == "/settings") {
                    Serial.println("\n=== WIFI CONFIGURATION ===");
                    Serial.println("Choose mode:\n  1. Connect to WiFi network\n  2. Start Access Point mode");
                    Serial.print("Enter choice (1 or 2): ");
                    configStep = 10;
                }
                else if (configStep == 10) {
                    // Handle WiFi configuration choice
                    if (serialInput == "1") {
                        configStep = 0;
                        Serial.println("\n=== CONNECT TO WIFI NETWORK ===");
                        Serial.println("Scanning for WiFi networks...");
                        WiFi.scanDelete();
                        networkCount = WiFi.scanNetworks();
                        for (int i = 0; i < min(networkCount, 20); i++) {
                            availableNetworks[i] = WiFi.SSID(i);
                        }
                        scanWiFiNetworks();
                        if (networkCount > 0) configStep = 20;
                    } else if (serialInput == "2") {
                        configStep = 0;
                        Serial.println("\n=== ACCESS POINT MODE ===");
                        Serial.println("Starting Access Point mode...");
                        startAPMode();
                    } else {
                        Serial.println("Invalid choice. Please enter 1 or 2:");
                    }
                }
                else if (configStep == 20) {
                    // Handle network selection
                    int networkChoice = serialInput.toInt();
                    if (networkChoice == 0) {
                        configStep = 0;
                        Serial.println("Network selection cancelled.");
                    } else if (networkChoice >= 1 && networkChoice <= min(networkCount, 20)) {
                        int networkIndex = networkChoice - 1;
                        tempSSID = WiFi.SSID(networkIndex);
                        Serial.print("\nSelected network: ");
                        Serial.println(tempSSID);
                        Serial.print("Enter WiFi password (or 'cancel' to abort): ");
                        configStep = 30; // Next step: password input
                    } else {
                        Serial.print("Invalid network number. Please enter 1-");
                        Serial.print(min(networkCount, 20));
                        Serial.println(" or 0 to cancel:");
                    }
                }
                else if (configStep == 30) {
                    // Handle WiFi password input
                    if (serialInput == "cancel") {
                        configStep = 0;
                        tempSSID = "";
                        Serial.println("WiFi configuration cancelled.");
                    } else {
                        tempPassword = serialInput;
                        Serial.println("\nTesting WiFi connection...");
                        
                        if (testWiFiConnection(tempSSID.c_str(), tempPassword.c_str())) {
                            // Save credentials
                            strncpy(state.wifiSSID, tempSSID.c_str(), sizeof(state.wifiSSID));
                            strncpy(state.wifiPassword, tempPassword.c_str(), sizeof(state.wifiPassword));
                            state.wifiConfigured = true;
                            state.wifiMode = 0; // Station mode
                            saveSettings();
                            
                            Serial.println("\n=== WiFi CONFIGURED SUCCESSFULLY ===");
                            Serial.print("SSID: "); Serial.println(state.wifiSSID);
                            Serial.println("Connecting to WiFi...");
                            
                            // Reset WiFi state before connecting
                            WiFi.disconnect(true);
                            delay(500);
                            WiFi.mode(WIFI_OFF);
                            delay(200);
                            
                            connectToWiFi();
                            configStep = 0;
                            tempSSID = "";
                            tempPassword = "";
                        } else {
                            Serial.println("\n=== CONNECTION FAILED ===");
                            Serial.println("Please check your password and try again.");
                            Serial.print("Enter WiFi password for '");
                            Serial.print(tempSSID);
                            Serial.print("' (or 'cancel' to abort): ");
                            // Stay at configStep 30 to retry
                        }
                    }
                }
                else if (serialInput == "auto" || serialInput == "AUTO") {
                    if (state.cachedMode != MODE_MANUAL && state.manualPumpState) {
                        state.manualPumpState = false;
                        Serial.println("🔄 Manual pump reset for AUTO mode");
                    }
                    setSystemMode("AUTO");
                    Serial.println("✅ Mode: AUTO");
                }
                else if (serialInput == "/scan") {
                    Serial.println("\n=== WIFI NETWORK SCAN ===");
                    Serial.println("Scanning for WiFi networks...");
                    WiFi.scanDelete();
                    networkCount = WiFi.scanNetworks();
                    for (int i = 0; i < min(networkCount, 20); i++) {
                        availableNetworks[i] = WiFi.SSID(i);
                    }
                    scanWiFiNetworks();
                    if (networkCount > 0) configStep = 20;
                }
                else if (serialInput == "/ap") {
                    Serial.println("\n=== ACCESS POINT MODE ===");
                    Serial.println("Starting Access Point mode...");
                    startAPMode();
                }
                else if (serialInput == "/calibrate") {
                    Serial.println("\n=== SYSTEM CALIBRATION ===");
                    calibrationWizard.startCalibration();
                }
                else if (serialInput == "/resetpatterns") {
                    Serial.println("\n=== PATTERN RESET ===");
                    patternLearner.resetDailyData();
                    Serial.println("Pattern learning data reset");
                }
                else if (serialInput == "/restart") {
                    Serial.println("\n=== SYSTEM RESTART ===");
                    Serial.println("System restarting...");
                    delay(500); // Keep delay - needed before restart
                    ESP.restart();
                    return;
                }
                else if (serialInput == "/reset") {
                    Serial.println("\n=== FACTORY RESET ===");
                    Serial.println("WARNING: This will erase all settings!");
                    Serial.println("Type 'CONFIRM' to proceed or anything else to cancel:");
                    
                    serialInput = "";
                    while (!Serial.available()) yield(); // OPTIMIZED: Non-blocking
                    
                    String confirmation = Serial.readString(); confirmation.trim();
                    if (confirmation == "CONFIRM") {
                        Serial.println("Initiating factory reset...");
                        factoryReset();
                    } else {
                        Serial.println("Factory reset cancelled.");
                    }
                }
                else {
                    Serial.println("Unknown command. Type '/help' for available commands.");
                }
                
                serialInput = "";
            }
        } else {
            serialInput += c;
        }
    }
}

void scanWiFiNetworks() {
  Serial.println("Scanning for WiFi networks... (this may take a while)");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("No networks found.");
  } else {
    Serial.print("Found ");
    Serial.print(n);
    Serial.println(" networks:");
    Serial.println("--------------------------------------------");
    Serial.println("No. SSID                     RSSI  ENC");
    Serial.println("--------------------------------------------");
    
    for (int i = 0; i < n; ++i) {
      Serial.printf("%2d. %-24s %4d  ", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:
          Serial.println("OPEN");
          break;
        case WIFI_AUTH_WEP:
          Serial.println("WEP");
          break;
        case WIFI_AUTH_WPA_PSK:
          Serial.println("WPA");
          break;
        case WIFI_AUTH_WPA2_PSK:
          Serial.println("WPA2");
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          Serial.println("WPA/WPA2");
          break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
          Serial.println("WPA2-E");
          break;
        default:
          Serial.println("UNKNOWN");
          break;
      }
      
      if (i >= 19) {
        Serial.println("... (more networks available)");
        break;
      }
    }
    Serial.println("--------------------------------------------");
  }
  
  if (n > 0) {
    Serial.print("Enter network number to connect (1-");
    Serial.print(min(n, 20));
    Serial.println(") or 0 to cancel:");
  }
}