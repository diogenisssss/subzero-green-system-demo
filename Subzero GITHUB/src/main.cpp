#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "hardware/hardware.h"
#include "ui/preferences.h"
#include "hardware/display.h"
#include "web/web.h"
#include "hardware/wifimanager.h"
#include "ui/calibration_wizard.h"
#include "algorithms/ai_optimizer.h"
#include "algorithms/pattern_learner.h"
#include "algorithms/smart_auto_mode.h"
#include "config/demo_public_build.h"

// Forward declarations
void addSystemNotification(const char* type, const char* title, const char* message);

// Global instances
CalibrationWizard calibrationWizard;
AIOptimizationEngine aiOptimizer;
PatternLearner patternLearner;

// Timer variables
unsigned long lastSensorRead = 0;
unsigned long lastPumpCheck = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastHealthCheck = 0;
unsigned long lastOptimization = 0;
unsigned long lastMaintenanceCheck = 0;
unsigned long lastSSEUpdate = 0;
unsigned long lastHistoryUpdate = 0;

// Sensor recovery tracking
static uint8_t internalSensorErrorCount = 0;
static uint8_t externalSensorErrorCount = 0;
static unsigned long lastInternalSensorRecoveryAttempt = 0;
static unsigned long lastExternalSensorRecoveryAttempt = 0;
const unsigned long SENSOR_RECOVERY_INTERVAL = 30000; // 30 seconds
const uint8_t SENSOR_ERROR_THRESHOLD = 10; // Attempt recovery after 10 consecutive errors

// Smoothing filter for external sensor (prevents spikes from touch/wind)
const uint8_t FILTER_BUFFER_SIZE = 5; // Keep last 5 readings
const float TEMP_CHANGE_THRESHOLD = 3.0f; // Ignore temp changes > 3°C in short time
const float HUMIDITY_CHANGE_THRESHOLD = 15.0f; // Ignore humidity changes > 15% in short time
const unsigned long EXTREME_VALUE_TIMEOUT = 60000; // Accept extreme value if it persists for 60 seconds

// External temp filter
static float extTempBuffer[FILTER_BUFFER_SIZE] = {-127.0f};
static uint8_t extTempBufferIndex = 0;
static bool extTempBufferFilled = false;
static float lastAcceptedExtTemp = -127.0f;
static float extremeExtTemp = -127.0f;
static unsigned long extremeExtTempStartTime = 0;

// External humidity filter
static float extHumidityBuffer[FILTER_BUFFER_SIZE] = {-1.0f};
static uint8_t extHumidityBufferIndex = 0;
static bool extHumidityBufferFilled = false;
static float lastAcceptedExtHumidity = -1.0f;
static float extremeExtHumidity = -1.0f;
static unsigned long extremeExtHumidityStartTime = 0;

// Helper function to calculate average from buffer
float calculateAverage(float* buffer, uint8_t size, bool filled, uint8_t currentIndex) {
    float sum = 0.0f;
    uint8_t count = 0;
    uint8_t validCount = filled ? size : currentIndex;
    
    for (uint8_t i = 0; i < validCount; i++) {
        if (buffer[i] > -126.0f) { // Valid value for temp
            sum += buffer[i];
            count++;
        } else if (buffer[i] >= 0.0f && buffer[i] <= 100.0f) { // Valid value for humidity
            sum += buffer[i];
            count++;
        }
    }
    
    if (count == 0) return -127.0f;
    return sum / count;
}

// Smoothing filter for external temperature
float filterExternalTemp(float newTemp, unsigned long currentTime) {
    // Add to buffer
    extTempBuffer[extTempBufferIndex] = newTemp;
    extTempBufferIndex = (extTempBufferIndex + 1) % FILTER_BUFFER_SIZE;
    if (extTempBufferIndex == 0) extTempBufferFilled = true;
    
    // Calculate moving average
    float avgTemp = calculateAverage(extTempBuffer, FILTER_BUFFER_SIZE, extTempBufferFilled, extTempBufferIndex);
    
    // If no previous accepted value, accept the average
    if (lastAcceptedExtTemp < -126.0f) {
        lastAcceptedExtTemp = avgTemp;
        return avgTemp;
    }
    
    // Check if change is extreme
    float change = fabs(newTemp - lastAcceptedExtTemp);
    
    if (change > TEMP_CHANGE_THRESHOLD) {
        // Extreme change detected
        if (fabs(newTemp - extremeExtTemp) < 0.5f) {
            // Same extreme value continues
            if (extremeExtTempStartTime == 0) {
                extremeExtTempStartTime = currentTime;
            }
            
            // If extreme value persists for long time, accept it
            if (currentTime - extremeExtTempStartTime > EXTREME_VALUE_TIMEOUT) {
                lastAcceptedExtTemp = newTemp;
                extremeExtTemp = -127.0f;
                extremeExtTempStartTime = 0;
                return newTemp;
            }
        } else {
            // New extreme value
            extremeExtTemp = newTemp;
            extremeExtTempStartTime = currentTime;
        }
        
        // Reject extreme change - return last accepted value
        return lastAcceptedExtTemp;
    } else {
        // Normal change - accept it
        lastAcceptedExtTemp = avgTemp;
        extremeExtTemp = -127.0f;
        extremeExtTempStartTime = 0;
        return avgTemp;
    }
}

// Smoothing filter for external humidity
float filterExternalHumidity(float newHumidity, unsigned long currentTime) {
    // Add to buffer
    extHumidityBuffer[extHumidityBufferIndex] = newHumidity;
    extHumidityBufferIndex = (extHumidityBufferIndex + 1) % FILTER_BUFFER_SIZE;
    if (extHumidityBufferIndex == 0) extHumidityBufferFilled = true;
    
    // Calculate moving average
    float avgHumidity = calculateAverage(extHumidityBuffer, FILTER_BUFFER_SIZE, extHumidityBufferFilled, extHumidityBufferIndex);
    
    // If no previous accepted value, accept the average
    if (lastAcceptedExtHumidity < 0.0f) {
        lastAcceptedExtHumidity = avgHumidity;
        return avgHumidity;
    }
    
    // Check if change is extreme
    float change = fabs(newHumidity - lastAcceptedExtHumidity);
    
    if (change > HUMIDITY_CHANGE_THRESHOLD) {
        // Extreme change detected
        if (fabs(newHumidity - extremeExtHumidity) < 2.0f) {
            // Same extreme value continues
            if (extremeExtHumidityStartTime == 0) {
                extremeExtHumidityStartTime = currentTime;
            }
            
            // If extreme value persists for long time, accept it
            if (currentTime - extremeExtHumidityStartTime > EXTREME_VALUE_TIMEOUT) {
                lastAcceptedExtHumidity = newHumidity;
                extremeExtHumidity = -1.0f;
                extremeExtHumidityStartTime = 0;
                return newHumidity;
            }
        } else {
            // New extreme value
            extremeExtHumidity = newHumidity;
            extremeExtHumidityStartTime = currentTime;
        }
        
        // Reject extreme change - return last accepted value
        return lastAcceptedExtHumidity;
    } else {
        // Normal change - accept it
        lastAcceptedExtHumidity = avgHumidity;
        extremeExtHumidity = -1.0f;
        extremeExtHumidityStartTime = 0;
        return avgHumidity;
    }
}

void setupOTA() {
  // Demo build: OTA is disabled by default to keep this repo a SAFE, non-production showcase.
  // If you want OTA locally, set `DEMO_DISABLE_OTA=0` in `src/config/demo_public_build.h`.
  if (DEMO_DISABLE_OTA) return;

  ArduinoOTA.setHostname(DEMO_MDNS_HOSTNAME);
  ArduinoOTA.setPassword(state.otaPassword);
  ArduinoOTA.begin();
}

void setup() {
  // Απενεργοποίηση Bluetooth για να μειώσουμε την κατανάλωση και το ζέσταμα
  btStop();
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);

  esp_task_wdt_init(10, true);
  Serial.begin(115200);
  delay(1000);

  Serial.println("==== SubZero Green Systems ====");
  Serial.println("Advanced technology for sustainable cooling.\n");

  loadSettings();
  setupHardware();
  setupDisplay();
  
  state.startTime = millis();
  patternLearner.initialize();

  setupWebServer();
  setupOTA();

  configTime(7200, 0, "pool.ntp.org", "time.nist.gov");
  delay(2000);

  Serial.println("System initialized successfully!");
}

void loop() {
  esp_task_wdt_reset();

  if (!DEMO_DISABLE_OTA) {
    ArduinoOTA.handle();
  }
  handleSerialConfiguration();

  const unsigned long currentTime = millis();

  // Temperature sensor reading (every 5 seconds)
  if (currentTime - lastSensorRead >= 5000) {
    lastSensorRead = currentTime;
    
    // Internal sensor reading with auto-recovery
    sensors.requestTemperatures();
    const float tempC = sensors.getTempCByIndex(0);
    if (isValidTemperature(tempC)) {
      state.SensorTemp = tempC;
      internalSensorErrorCount = 0; // Reset error count on successful read
    } else {
      internalSensorErrorCount++;
      
      // Auto-recovery: Reinitialize sensor after threshold errors
      if (internalSensorErrorCount >= SENSOR_ERROR_THRESHOLD && 
          (currentTime - lastInternalSensorRecoveryAttempt >= SENSOR_RECOVERY_INTERVAL)) {
        lastInternalSensorRecoveryAttempt = currentTime;
        sensors.begin(); // Reinitialize sensor
        // OPTIMIZED: Non-blocking delay - use yield() instead
        yield(); // Give other tasks time
        internalSensorErrorCount = SENSOR_ERROR_THRESHOLD - 1; // Prevent immediate retry
      }
    }
    
    // External sensor reading with auto-recovery (DHT22)
    const float externalTempC = externalDHT.readTemperature();
    const float externalHumidityC = externalDHT.readHumidity();
    
    if (isValidTemperature(externalTempC)) {
      // Apply smoothing filter to prevent spikes from touch/wind
      state.externalTemp = filterExternalTemp(externalTempC, currentTime);
      externalSensorErrorCount = 0; // Reset error count on successful read
    } else {
      externalSensorErrorCount++;
      
      // Auto-recovery: Reinitialize sensor after threshold errors
      if (externalSensorErrorCount >= SENSOR_ERROR_THRESHOLD && 
          (currentTime - lastExternalSensorRecoveryAttempt >= SENSOR_RECOVERY_INTERVAL)) {
        lastExternalSensorRecoveryAttempt = currentTime;
        externalDHT.begin(); // Reinitialize sensor
        // OPTIMIZED: Non-blocking delay - use yield() instead
        yield(); // Give other tasks time
        externalSensorErrorCount = SENSOR_ERROR_THRESHOLD - 1; // Prevent immediate retry
      }
    }
    
    // Read humidity (valid range: 0-100%) with smoothing filter
    if (!isnan(externalHumidityC) && externalHumidityC >= 0.0f && externalHumidityC <= 100.0f) {
      // Apply smoothing filter to prevent spikes from touch/wind
      state.externalHumidity = filterExternalHumidity(externalHumidityC, currentTime);
    } else {
      state.externalHumidity = -1.0f; // Invalid marker
    }
  }

  // Pump control (every 200ms)
  if (currentTime - lastPumpCheck >= 200) {
    lastPumpCheck = currentTime;
    
    // Check timed operations
    const bool pumpTimeout = (state.pumpRunUntil > 0 && currentTime >= state.pumpRunUntil);
    const bool fanTimeout = (state.fanRunUntil > 0 && currentTime >= state.fanRunUntil);
    
    if (pumpTimeout) {
      state.pumpState = false;
      state.pumpRunUntil = 0;
    }
    if (fanTimeout) {
      state.fanState = false;
      state.fanRunUntil = 0;
    }
    
    // Use cached mode enum for fast comparison (OPTIMIZED: no strcmp needed)
    const bool isAutoMode = (state.cachedMode == MODE_AUTO);
    const bool isManualMode = (state.cachedMode == MODE_MANUAL);
    const bool isOffMode = (state.cachedMode == MODE_OFF);
    
    if (isAutoMode) {
      // Auto mode is handled by SmartAutoMode class
      smartAuto.update();
    } else if (isManualMode) {
      const bool targetPumpState = state.manualPumpState || state.pumpState;
      ledcWrite(PWM_CHANNEL, targetPumpState ? 255 : 0);
      // Only run fan if manual pump is on OR if fan is specifically scheduled
      digitalWrite(FAN_RELAY_PIN, (targetPumpState && state.manualPumpState) || state.fanState ? HIGH : LOW);
      state.pumpState = targetPumpState;
    } else if (isOffMode) {
      // Allow timed operations even in OFF mode
      if (state.pumpState || state.fanState) {
        ledcWrite(PWM_CHANNEL, state.pumpState ? 255 : 0);
        digitalWrite(FAN_RELAY_PIN, state.fanState ? HIGH : LOW);
      } else {
        ledcWrite(PWM_CHANNEL, 0);
        digitalWrite(FAN_RELAY_PIN, LOW);
      }
    }
  }

  // OLED update (every 3 seconds)
  if (currentTime - lastOLEDUpdate >= 3000) {
    lastOLEDUpdate = currentTime;
    updateOLED();
  }

  // WiFi check (every 30 seconds)
  if (currentTime - lastWiFiCheck >= 30000) {
    lastWiFiCheck = currentTime;
    checkAndReconnectWiFi();
    attemptWiFiRecovery();
  }

  // History update (every minute)
  if (currentTime - lastHistoryUpdate >= 60000) {
    lastHistoryUpdate = currentTime;
    
    // Save current values to history
    uint8_t idx = state.historyIndex;
    state.tempHistory[idx] = state.SensorTemp;
    state.extTempHistory[idx] = state.externalTemp;
    state.humidityHistory[idx] = state.externalHumidity;
    state.timeHistory[idx] = currentTime / 1000; // Store as Unix timestamp (seconds)
    
    // Move to next slot (circular buffer)
    state.historyIndex = (state.historyIndex + 1) % 90;
    state.historyInitialized = true;
  }

  // System health check (every minute)
  if (currentTime - lastHealthCheck >= 60000) {
    lastHealthCheck = currentTime;
    
    // Check for sensor error - notify only once when error occurs, and once when restored
    const bool sensorError = (state.SensorTemp < -100.0f);
    if (sensorError) {
      if (!state.sensorErrorNotified) {
        addSystemNotification("error", "Sensor Failure", "Temperature sensor is not responding. Check wiring.");
        state.sensorErrorNotified = true;
      }
    } else {
      // Sensor is working - if we had notified about error before, notify about restoration
      if (state.sensorErrorNotified) {
        addSystemNotification("info", "Sensor Restored", "Temperature sensor is now working correctly.");
        state.sensorErrorNotified = false;
      }
    }
    
    // Check for high temperature - notify only once when it exceeds, and once when normalized
    const bool highTemp = (state.SensorTemp > state.safetyTemp);
    if (highTemp) {
      if (!state.highTempNotified) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Temperature %.1f°C exceeds safety limit %.1f°C", 
                 state.SensorTemp, state.safetyTemp);
        addSystemNotification("warning", "High Temperature Alert", msg);
        state.highTempNotified = true;
      }
    } else {
      // Temperature is normal - if we had notified about high temp before, notify about normalization
      if (state.highTempNotified) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Temperature has returned to safe levels: %.1f°C", state.SensorTemp);
        addSystemNotification("info", "Temperature Normalized", msg);
        state.highTempNotified = false;
      }
    }
    
    // Check memory usage - notify only once when low, and once when recovered
    const uint32_t freeHeap = ESP.getFreeHeap();
    const bool lowMemory = (freeHeap < 20000);
    if (lowMemory) {
      if (!state.lowMemoryNotified) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Free memory: %u bytes", freeHeap);
        addSystemNotification("warning", "Low Memory", msg);
        state.lowMemoryNotified = true;
      }
    } else {
      // Memory is good - if we had notified about low memory before, notify about recovery
      if (state.lowMemoryNotified) {
        char msg[40];
        snprintf(msg, sizeof(msg), "Memory is back to normal: %u bytes", freeHeap);
        addSystemNotification("info", "Memory Recovered", msg);
        state.lowMemoryNotified = false;
      }
    }
  }

  // AI optimization (every minute) - OPTIMIZED: use cached mode enum
  if (currentTime - lastOptimization >= 60000) {
    lastOptimization = currentTime;
    if (state.cachedMode == MODE_AUTO) {
      aiOptimizer.applySystemOptimizations();
    }
  }

  // Maintenance check (every hour)
  if (currentTime - lastMaintenanceCheck >= 3600000) {
    lastMaintenanceCheck = currentTime;
    patternLearner.checkMaintenanceNeeds();
  }

  patternLearner.updateLearning();
  
  // Non-blocking display setup animation
  updateDisplaySetup();
  
  // Send SSE updates (every 2 seconds - OPTIMIZED: reduced frequency for better performance)
  if (currentTime - lastSSEUpdate >= 2000) {
    lastSSEUpdate = currentTime;
    sendSSEUpdate();
  }
  
  // OPTIMIZED: Use yield() instead of delay for better responsiveness
  yield(); // Give WiFi and other tasks time without blocking
}