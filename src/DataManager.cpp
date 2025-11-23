#include "DataManager.h"

DataManager::DataManager() {}

bool DataManager::begin(SPIClass &spi) {
    pinMode(SD_EN_PIN, OUTPUT);
    digitalWrite(SD_EN_PIN, HIGH);
    pinMode(SD_DET_PIN, INPUT_PULLUP);
    delay(100);

    if (digitalRead(SD_DET_PIN)) {
        Serial.println("[DataManager] No SD card detected.");
        return false;
    }

    // We use the shared SPI instance passed from main
    if (!SD.begin(SD_CS_PIN, spi)) {
        Serial.println("[DataManager] SD Mount Failed!");
        return false;
    }
    
    Serial.println("[DataManager] SD Card Mounted.");
    return true;
}

void DataManager::loadData(PetDataMap &petData) {
    if (!SD.exists(_filename)) {
        Serial.println("[DataManager] No data file found. Creating new.");
        return;
    }

    File file = SD.open(_filename, FILE_READ);
    if (!file) return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("[DataManager] JSON Parse Error: ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject root = doc.as<JsonObject>();
    for (JsonPair petPair : root) {
        int petId = atoi(petPair.key().c_str());
        JsonArray records = petPair.value().as<JsonArray>();

        for (JsonObject recordJson : records) {
            LitterboxRecord rec;
            rec.timestamp = recordJson["ts"];
            rec.weight_grams = recordJson["w_g"];
            rec.duration_seconds = recordJson["dur_s"];
            rec.pet_id = petId;
            petData[petId][rec.timestamp] = rec;
        }
    }
    Serial.println("[DataManager] Historical data loaded.");
}

void DataManager::saveData(const PetDataMap &petData) {
    JsonDocument doc; 
    JsonObject root = doc.to<JsonObject>();

    time_t now = time(NULL);
    time_t pruneTimestamp = now - (365 * 86400L);
    
    for (auto const &petPair : petData) {
        int petId = petPair.first;
        JsonArray petArray = root[String(petId)].to<JsonArray>();

        for (auto const &recordPair : petPair.second) {
            const LitterboxRecord &record = recordPair.second;
            if (record.timestamp < pruneTimestamp) continue;

            JsonObject recJson = petArray.add<JsonObject>();
            recJson["ts"] = record.timestamp;
            recJson["w_g"] = record.weight_grams;
            recJson["dur_s"] = record.duration_seconds;
        }
    }

    File file = SD.open(_filename, FILE_WRITE);
    if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("[DataManager] Data saved to SD.");
    }
}

void DataManager::mergeData(PetDataMap &mainData, int petId, const std::vector<LitterboxRecord> &newRecords) {
    for (const auto &record : newRecords) {
        mainData[petId][record.timestamp] = record;
    }
}

time_t DataManager::getLatestTimestamp(const PetDataMap &petData) {
    time_t latest = 0;
    for (auto const &petPair : petData) {
        const std::map<time_t, LitterboxRecord> &recordsMap = petPair.second;
        if (!recordsMap.empty()) {
            time_t petLatest = recordsMap.rbegin()->first;
            if (petLatest > latest) {
                latest = petLatest;
            }
        }
    }
    return latest;
}