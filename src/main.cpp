#include <Arduino.h>
#include "config.h"
#include "SharedTypes.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "PlotManager.h"
#include "RTClib.h"
#include "Adafruit_SHT4x.h"
// Globals
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display;
RTC_PCF8563 rtc;
Preferences preferences;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
// Define SPI globally so it persists for the lifetime of the program
// This fixes the crash where Display tried to access a destroyed SPI object
SPIClass hspi(HSPI);

DataManager dataManager;
NetworkManager *networkManager;
PlotManager *plotManager;

PetDataMap allPetData;
std::vector<Pet> allPets;

DateRangeInfo dateRangeInfo[] = {
    {LAST_7_DAYS, "Last 7 Days", 7 * 86400L},
    {LAST_30_DAYS, "Last 30 Days", 30 * 86400L},
    {LAST_90_DAYS, "Last 90 Days", 90 * 86400L},
    {LAST_365_DAYS, "Last 365 Days", 365 * 86400L},
};

void initHardware()
{
  Serial.begin(115200);
  psramInit();
  if (psramFound())
    Serial.println("Found and Initialized PSRAM");
  else
    Serial.println("No PSRAM Found");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);

  pinMode(BUTTON_KEY0, INPUT_PULLUP);
  pinMode(BUTTON_KEY1, INPUT_PULLUP);
  pinMode(BUTTON_KEY2, INPUT_PULLUP);

  pinMode(SD_EN_PIN, OUTPUT);
  digitalWrite(SD_EN_PIN, HIGH);
  pinMode(SD_DET_PIN, INPUT_PULLUP);

  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH); // Enable battery monitoring
  Wire.setPins(I2C_SDA, I2C_SCL);

  // Configure ADC
  analogReadResolution(12); // 12-bit resolution
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  if (!sht4.begin())
  {
    Serial.println("Couldn't find SHT4x");
  }

  // Initialize the Global SPI instance
  hspi.begin(EPD_SCK_PIN, SD_MISO_PIN, EPD_MOSI_PIN, -1);

  display = new GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

  // Pass the global hspi to the display
  display->epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display->init(0);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Setup Battery Monitoring
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH);
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  pinMode(BUTTON_KEY0, INPUT_PULLUP);
  pinMode(BUTTON_KEY1, INPUT_PULLUP);
  pinMode(BUTTON_KEY2, INPUT_PULLUP);
}

void setup()
{
  initHardware();
  preferences.begin(NVS_NAMESPACE);

  networkManager = new NetworkManager(preferences);
  plotManager = new PlotManager(display);

  // 1. Load Local Data using the Shared SPI
  // Pass the global 'hspi' to the data manager
  dataManager.begin(hspi);
  dataManager.loadData(allPetData);
  StatusRecord status;
  int rangeIndex = preferences.getInt(NVS_PLOT_RANGE_KEY, 0);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1)
  {
    uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_pins & BUTTON_KEY1_MASK)
    {
      rangeIndex++;
      if (rangeIndex >= (int)Date_Range_Max)
        rangeIndex = 0;
    }
    else if (wakeup_pins & BUTTON_KEY2_MASK)
    {
      rangeIndex--;
      if (rangeIndex < 0)
        rangeIndex = (int)Date_Range_Max - 1;
    }
    preferences.putInt(NVS_PLOT_RANGE_KEY, rangeIndex);
  }

  bool isViewUpdate = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);

  if (!isViewUpdate)
  {
    networkManager->connectOrProvision(display);
    rtc.begin();
    networkManager->syncTime(rtc);

    if (networkManager->initPetKitApi())
    {

      // --- RESTORED LOGIC START ---
      // Calculate how many days we are missing
      int daysToFetch = 30; // Default max

      time_t latestTimestamp = dataManager.getLatestTimestamp(allPetData);

      if (latestTimestamp > 0)
      {
        time_t now = time(NULL);
        long secondsDifference = now - latestTimestamp;
        Serial.printf("Latest timestamp from SD: %lu, %.2f days ago.\r\n", latestTimestamp, (float)secondsDifference / 86400.0);

        int daysDifference = (int)(secondsDifference / 86400) + 2; // +buffer
        daysToFetch = std::min(std::max(daysDifference, 1), 30);
      }
      Serial.printf("Requesting %d days of data from PetKit.\r\n", daysToFetch);
      // --- RESTORED LOGIC END ---

      if (networkManager->getApi()->fetchAllData(daysToFetch))
      {
        allPets = networkManager->getApi()->getPets();

        // Store pets to NVS
        if (!allPets.empty())
        {
          preferences.putBytes(NVS_PETS_KEY, allPets.data(), allPets.size() * sizeof(Pet));
        }

        // Merge data
        for (const auto &pet : allPets)
        {
          auto records = networkManager->getApi()->getLitterboxRecordsByPetId(pet.id);
          dataManager.mergeData(allPetData, pet.id, records);
        }
        dataManager.saveData(allPetData);
        status = networkManager->getApi()->getLatestStatus();
        if (status.device_name.length() > 0)
        {
          dataManager.saveStatus(status);
        }
      }
    }
  }
  else
  {
    // If we are just updating the view (button press), try to load pets from NVS
    // so we have names for the charts without needing WiFi
    size_t len = preferences.getBytesLength(NVS_PETS_KEY);
    if (len > 0)
    {
      allPets.resize(len / sizeof(Pet));
      preferences.getBytes(NVS_PETS_KEY, allPets.data(), len);
    }
    status = dataManager.getStatus();
  }

  // 3. Render
  plotManager->renderDashboard(allPets, allPetData, dateRangeInfo[rangeIndex], status);

  display->display();
  display->hibernate();

  // 4. Sleep
  Serial.println("Sleeping...");
  uint64_t sleepInterval = 1000000ull * 60ull * 60ull * 2ull; // 2hr
  esp_sleep_enable_timer_wakeup(sleepInterval);
  // Wake up on Key 0, 1, or 2 (Low)
  esp_sleep_enable_ext1_wakeup(BUTTON_KEY0_MASK | BUTTON_KEY1_MASK | BUTTON_KEY2_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  esp_deep_sleep_start();
}

void loop() {}
