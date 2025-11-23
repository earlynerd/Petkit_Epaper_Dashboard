#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "SharedTypes.h"
#include "config.h" 

class DataManager {
public:
    DataManager();
    
    // Initialize SD card using the shared SPI instance
    bool begin(SPIClass &spi);
    
    // Load historical data from SD into the provided map
    void loadData(PetDataMap &petData);
    
    // Save the provided map to SD, pruning old data
    void saveData(const PetDataMap &petData);
    
    //save latest status for display on plot
    void saveStatus(const StatusRecord &status);

    //fetch sotred status info from SD card
    StatusRecord getStatus();

    // Merge new records from API into the main map
    void mergeData(PetDataMap &mainData, int petId, const std::vector<LitterboxRecord> &newRecords);

    // Helper to find the most recent timestamp in the existing data
    time_t getLatestTimestamp(const PetDataMap &petData);

private:
    const char* _filename = "/pet_data.json";
    const char* _status_filename = "/status.json";
};

#endif