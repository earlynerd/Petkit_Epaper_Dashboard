#include "NetworkManager.h"
#include "provisionerConfig.h"
#include "certs.h"

NetworkManager::NetworkManager(Preferences& prefs) : _prefs(prefs) {}

void NetworkManager::connectOrProvision(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display) {
    WiFiProvisioner provisioner(provisionerCustom);

    // Define Provisioner Callbacks
    provisioner.onSuccess([this](const char *ssid, const char *password, const char *input, const char *pkuser, const char *pkpass) {
        Serial.printf("Connected to SSID: %s\n", ssid);
        _prefs.putString(NVS_SSID_KEY, ssid);
        if (password) _prefs.putString(NVS_WIFI_PASS_KEY, password);
        if (pkuser) _prefs.putString(NVS_PETKIT_USER_KEY, pkuser);
        if (pkpass) _prefs.putString(NVS_PETKIT_PASS_KEY, pkpass);
        Serial.println("Provisioning success! Restarting...");
        _prefs.end();
        ESP.restart(); // Clean restart after provisioning
    });

    String ssid = _prefs.getString(NVS_SSID_KEY, "");
    String pass = _prefs.getString(NVS_WIFI_PASS_KEY, "");

    if (ssid == "") {
        Serial.println("No saved WiFi. Starting provisioning.");
        provisioner.startProvisioning();
        return; 
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.print("Connecting to WiFi");
    
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi Timed Out. Starting provisioning.");
        if (display) {
             display->fillScreen(GxEPD_WHITE);
             display->setCursor(10, 40);
             display->print("WiFi Failed. Connect to AP: PetkitDashboard");
             display->display();
        }
        provisioner.startProvisioning();
        // Code halts here inside provisioner loop usually, or we explicitly stop
    }
    Serial.println("\nWiFi Connected!");
}

bool NetworkManager::syncTime(RTC_PCF8563& rtc) {
    String storedTZ = _prefs.getString(NVS_TZ_KEY, "");
    if (storedTZ.length() > 0) {
        strncpy(_time_zone, storedTZ.c_str(), sizeof(_time_zone) - 1);
        setenv("TZ", storedTZ.c_str(), 1);
        tzset();
    }
    
    // Try to fetch standard timezone and sync NTP
    return getTimezoneAndSync(rtc);
}

bool NetworkManager::getTimezoneAndSync(RTC_PCF8563& rtc) {
    // Logic moved from main.cpp getTimezoneAndSync()
    // Simplified for brevity:
    configTzTime(_time_zone, NTP_SERVER_1, NTP_SERVER_2);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 15000)) {
        time_t now_utc;
        time(&now_utc);
        rtc.adjust(DateTime(now_utc));
        Serial.println("[Network] Time synced and RTC updated.");
        return true;
    }
    return false;
}

bool NetworkManager::initPetKitApi() {
    String user = _prefs.getString(NVS_PETKIT_USER_KEY, "");
    String pass = _prefs.getString(NVS_PETKIT_PASS_KEY, "");
    String region = _prefs.getString(NVS_PETKIT_REGION_KEY, "us"); // Default US
    String tz = _prefs.getString(NVS_PETKIT_TIMEZONE_KEY, "");

    if (user == "" || pass == "") return false;

    _petkit = new PetKitApi(user.c_str(), pass.c_str(), region.c_str(), tz.c_str(), LED_PIN);
    return _petkit->login();
}

void NetworkManager::factoryReset() {
    _prefs.clear();
    _prefs.end();
    Serial.println("Factory Reset Complete.");
    ESP.restart();
}