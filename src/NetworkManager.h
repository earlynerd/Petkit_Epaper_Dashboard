#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TzDbLookup.h>
#include "WiFiProvisioner.h"
#include "PetKitApi.h"
#include "config.h"
#include "RTClib.h"
#if (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#elif (EPD_SELECT == 1001)
#include <GxEPD2_BW.h>
#endif
class NetworkManager {
public:
    NetworkManager(Preferences& prefs);
    
    // Connect to WiFi, falling back to provisioning if it fails
    void connectOrProvision(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display);
    
    // Sync time via API or NTP
    bool syncTime(RTC_PCF8563& rtc);
    
    // Initialize API client
    bool initPetKitApi();
    
    // Get the API client instance
    PetKitApi* getApi() { return _petkit; }
    
    // Clear credentials
    void factoryReset();

private:
    Preferences& _prefs;
    PetKitApi* _petkit = nullptr;
    char _time_zone[64];
    
    bool getTimezoneAndSync(RTC_PCF8563& rtc);
};

#endif