#include "wifimanager.h"
#include "../web/web.h"
#include "../config/demo_public_build.h"
#include <WiFi.h>
#include <ESPmDNS.h>

unsigned long lastDisconnectTime = 0;
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 3;
const unsigned long RECONNECT_INTERVAL = 300000;
const unsigned long WIFI_CHECK_INTERVAL = 60000; // Increased from 30s to 60s to reduce overhead

void connectToWiFi() {
    Serial.println("Attempting to connect to WiFi...");
    Serial.print("SSID: "); Serial.println(state.wifiSSID);
    
    WiFi.mode(WIFI_STA);
    delay(100);
    
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false); // Disable persistent storage for faster operations
    WiFi.begin(state.wifiSSID, state.wifiPassword);
    
    // Wait for connection with timeout
    int attempts = 0;
    const int maxAttempts = 20; // 10 seconds max wait
    
    Serial.print("Connecting");
    while (attempts < maxAttempts) {
        delay(500); // Keep delay - WiFi connection needs time
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected successfully!");
        const IPAddress localIP = WiFi.localIP();
        Serial.print("IP Address: "); Serial.println(localIP);
        
        if (!MDNS.begin(DEMO_MDNS_HOSTNAME)) {
            Serial.println("Error setting up MDNS responder!");
        } else {
            Serial.print("Host: "); Serial.print(DEMO_MDNS_HOSTNAME); Serial.print(".local (");
            Serial.print(localIP.toString()); 
            Serial.println(")");
        }
            return;
        }
        attempts++;
    }
    
    Serial.println("\nInitial connection attempt timed out. Will retry in main loop.");
    Serial.println("(WiFi auto-reconnect is enabled - connection will be established automatically)");
}

void handleWiFiDisconnection() {
    if (lastDisconnectTime == 0) {
        lastDisconnectTime = millis();
        reconnectAttempts = 0;
        Serial.println("WiFi disconnected. Starting reconnection attempts...");
    }
    attemptWiFiReconnection();
}

void setupWiFi() {
    // OPTIMIZED: Removed duplicate WiFi.disconnect and WiFi.mode calls
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100); // Keep delay - WiFi hardware needs time

    if (state.wifiConfigured && strlen(state.wifiSSID) > 0) {
        if (state.wifiMode == 0) {
            connectToWiFi();
        } else if (state.wifiMode == 1) {
            startAPMode();
        }
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        Serial.println("WiFi not configured. Use serial 'config' command.");
    }
}

void startAPMode() {
    Serial.println("Starting Access Point mode");
    
    // Save original credentials if available
    if (!state.hasOriginalCredentials && state.wifiMode == 0 && strlen(state.wifiSSID) > 0) {
        strcpy(state.originalWifiSSID, state.wifiSSID);
        strcpy(state.originalWifiPassword, state.wifiPassword);
        state.hasOriginalCredentials = true;
        Serial.println("Original WiFi credentials saved for auto-recovery");
    }
    
    WiFi.disconnect(true);
    yield(); // OPTIMIZED: Non-blocking
    WiFi.mode(WIFI_OFF);
    yield(); // OPTIMIZED: Non-blocking
    
    WiFi.mode(WIFI_AP);
    yield(); // OPTIMIZED: Non-blocking
    
    // DEMO NOTE:
    // In this public showcase repo we avoid committing any real SSIDs/passwords.
    // Default AP password is intentionally empty to make it clear this is NOT production-ready security.
    if (strlen(DEMO_AP_PASSWORD) == 0) {
        WiFi.softAP(DEMO_AP_SSID);
    } else {
        WiFi.softAP(DEMO_AP_SSID, DEMO_AP_PASSWORD, 1, 0, 4);
    }
    
    strncpy(state.wifiSSID, DEMO_AP_SSID, sizeof(state.wifiSSID));
    state.wifiSSID[sizeof(state.wifiSSID) - 1] = '\0';
    strncpy(state.wifiPassword, DEMO_AP_PASSWORD, sizeof(state.wifiPassword));
    state.wifiPassword[sizeof(state.wifiPassword) - 1] = '\0';
    state.wifiConfigured = true;
    state.wifiMode = 1;
    
    yield(); // OPTIMIZED: Non-blocking instead of delay
    
    Serial.print("AP IP Address: "); Serial.println(WiFi.softAPIP());
    Serial.print("SSID: "); Serial.println(DEMO_AP_SSID);
    if (strlen(DEMO_AP_PASSWORD) == 0) {
        Serial.println("Password: (OPEN / none)");
    } else {
        Serial.print("Password: "); Serial.println(DEMO_AP_PASSWORD);
    }
    Serial.println("Connect to this WiFi network for direct access");
    Serial.println("System will automatically try to reconnect to original WiFi every 30 minutes");
    
    saveSettings();
}

bool testWiFiConnection(const char* ssid, const char* pass) {
    Serial.print("Testing connection to: "); Serial.println(ssid);
    
    WiFi.disconnect(true);
    delay(1000); // Keep delay here - needed for WiFi hardware reset
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_STA);
    delay(100);
    
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.begin(ssid, pass);
    
    int attempts = 0;
    bool connected = false;
    
    while (attempts < 25) {
        delay(500); // Keep delay - WiFi connection needs time
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
        attempts++;
    }
    
    WiFi.disconnect(true);
    delay(1000); // Keep delay - needed for WiFi hardware reset
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    
    Serial.println(connected ? " Success" : " Failed");
    return connected;
}

void checkAndReconnectWiFi() {
    static unsigned long lastCheck = 0;
    const unsigned long currentTime = millis();
    
    if (currentTime - lastCheck < WIFI_CHECK_INTERVAL) return;
    lastCheck = currentTime;
    
    const bool isStationMode = (state.wifiMode == 0);
    const bool isConfigured = (state.wifiConfigured && strlen(state.wifiSSID) > 0);
    
    if (isStationMode && isConfigured) {
        const wl_status_t wifiStatus = WiFi.status();
        if (wifiStatus != WL_CONNECTED) {
            handleWiFiDisconnection();
        } else {
            lastDisconnectTime = 0;
            reconnectAttempts = 0;
        }
    }
}

void attemptWiFiReconnection() {
    static unsigned long reconnectStartTime = 0;
    const unsigned long currentTime = millis();
    
    if (currentTime - lastDisconnectTime < RECONNECT_INTERVAL) return;
    
    if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        // Start reconnection only once per attempt
        if (reconnectStartTime == 0) {
            Serial.print("Attempting WiFi reconnection ("); 
            Serial.print(reconnectAttempts + 1); 
            Serial.println("/3)...");
            WiFi.disconnect();
            yield(); // OPTIMIZED: Non-blocking
            WiFi.reconnect();
            reconnectStartTime = currentTime;
            reconnectAttempts++;
            lastDisconnectTime = currentTime;
        }
        
        // Non-blocking check - wait max 10 seconds (not 20)
        const unsigned long reconnectElapsed = currentTime - reconnectStartTime;
        if (reconnectElapsed < 10000) {
            // Still waiting for connection
            static unsigned long lastDotTime = 0;
            if (reconnectElapsed - lastDotTime >= 2000) {
                Serial.print(".");
                lastDotTime = reconnectElapsed;
            }
        } else {
            // Timeout reached - check if connected
            const wl_status_t wifiStatus = WiFi.status();
            if (wifiStatus == WL_CONNECTED) {
                Serial.println("WiFi reconnected successfully!");
                reconnectAttempts = 0;
                lastDisconnectTime = 0;
            } else {
                Serial.println("Reconnection attempt failed");
            }
            reconnectStartTime = 0; // Reset for next attempt
        }
    } else {
        Serial.println("All reconnection attempts failed. Switching to AP mode...");
        startAPMode();
        reconnectAttempts = 0;
        lastDisconnectTime = 0;
        reconnectStartTime = 0;
    }
}

void attemptWiFiRecovery() {
    if (state.wifiMode != 1 || !state.hasOriginalCredentials) return;
    
    unsigned long currentTime = millis();
    if (currentTime - state.lastRecoveryAttempt < state.RECOVERY_INTERVAL) return;
    
    Serial.println("Attempting automatic WiFi recovery...");
    Serial.print("Trying to connect to: "); Serial.println(state.originalWifiSSID);
    
    // Non-blocking recovery attempt - just try to connect, don't wait
    WiFi.softAPdisconnect(true);
    yield(); // OPTIMIZED: Non-blocking
    WiFi.mode(WIFI_STA);
    yield(); // OPTIMIZED: Non-blocking
    
    strcpy(state.wifiSSID, state.originalWifiSSID);
    strcpy(state.wifiPassword, state.originalWifiPassword);
    state.wifiMode = 0;
    
    WiFi.begin(state.wifiSSID, state.wifiPassword);
    state.lastRecoveryAttempt = currentTime;
    Serial.println("Recovery attempt initiated. Connection will be checked in next loop.");
    saveSettings();
}