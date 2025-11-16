#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "PetKitApi.h"
#include "esp_heap_caps.h"

const int limit = 10000;

#if (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#elif (EPD_SELECT == 1001)
#include <GxEPD2_BW.h>
#endif
#include "RTClib.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Wire.h>
#include "esp_adc_cal.h"
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include "preferences.h"
#include "certs.h"
#include "ScatterPlot.h"
#include "histogram.h"
#include <vector>
#include <map>
#include "Adafruit_SHT4x.h"
#include <TzDbLookup.h>
#include "WiFiProvisioner.h"
#include "provisionerConfig.h"

Preferences preferences;
PetKitApi *petkit;
SPIClass hspi(HSPI);
RTC_PCF8563 rtc;
ScatterPlot *myPlot;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display;
// GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(/*CS=*/EPD_CS_PIN, /*DC=*/EPD_DC_PIN,/*RST=*/EPD_RES_PIN, /*BUSY=*/EPD_BUSY_PIN));

// Maps: Pet ID -> (Map of: Timestamp -> Record)
std::map<int, std::map<time_t, LitterboxRecord>> allPetData;

// Define the SD card filename
#define SD_DATA_FILE "/pet_data.json"

String petkitUser;
String petkitPass;
String petkitRegion;
String petkitTZ;

std::vector<uint16_t> petColors = {EPD_RED, EPD_BLUE, EPD_GREEN, EPD_YELLOW, EPD_BLACK};

StatusRecord latest_status;
char time_zone[64];

float battery_voltage = 0;

enum PlotType
{
  Scatterplot_Weight,
  Histogram_Interval,
  Histogram_Duration,
  Histograms,
  ComboPlot,
  Plot_Type_Max
};

struct PlotInfo
{
  enum PlotType type;
  char name[64];
};

struct PlotInfo plotinfo[] =
    {
        {PlotType::Scatterplot_Weight, "Weight Scatterplot"},
        {PlotType::Histogram_Interval, "Interval Histogram"},
        {PlotType::Histogram_Duration, "Duration histogram"},
        {PlotType::Histograms, "Double Histogram"},
        {PlotType::ComboPlot, "Triple Plot"},
};

PlotInfo thisPlot = plotinfo[0];
int plotindex = 0;

bool getTimezoneAndSync();
bool initializeFromRtc();
void saveDataToSD(const std::map<int, std::map<time_t, LitterboxRecord>> &petData);
void loadDataFromSD(std::map<int, std::map<time_t, LitterboxRecord>> &petData);

void button_1_isr()
{
  plotindex++;
  if (plotindex < 0)
    plotindex = PlotType::Plot_Type_Max - 1;
  if (plotindex >= (int)PlotType::Plot_Type_Max)
    plotindex = 0;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);
  Serial.print("Type changed to ");
  Serial.println(thisPlot.name);
}

void button_2_isr()
{
  plotindex--;
  if (plotindex < 0)
    plotindex = PlotType::Plot_Type_Max;
  ;
  if (plotindex > (int)PlotType::Plot_Type_Max)
    plotindex = 0;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);
  Serial.print("Type changed to ");
  Serial.println(thisPlot.name);
}

void setup()
{
  Serial.begin(115200);
  psramInit();
  if (psramFound())
    Serial.println("Found and Initialized PSRAM");
  else
    Serial.println("No PSRAM Found");
  heap_caps_malloc_extmem_enable(limit);
  display = new GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>(GxEPD2_DRIVER_CLASS(/*CS=*/EPD_CS_PIN, /*DC=*/EPD_DC_PIN, /*RST=*/EPD_RES_PIN, /*BUSY=*/EPD_BUSY_PIN));
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);

  pinMode(BUTTON_KEY0, INPUT);
  pinMode(BUTTON_KEY1, INPUT);
  pinMode(BUTTON_KEY2, INPUT);

  preferences.begin(NVS_NAMESPACE);
  // Configure ADC
  analogReadResolution(12); // 12-bit resolution
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  // Initialize SPI
  hspi.begin(EPD_SCK_PIN, SD_MISO_PIN, EPD_MOSI_PIN, -1);
  display->epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  pinMode(SD_EN_PIN, OUTPUT);
  digitalWrite(SD_EN_PIN, HIGH);
  pinMode(SD_DET_PIN, INPUT_PULLUP);
  delay(100);

  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH); // Enable battery monitoring
  // Initialize display
  display->init(0);

  display->setTextSize(1);

  if (!digitalRead(BUTTON_KEY1) && !digitalRead(BUTTON_KEY2)) // both keys pressed at powerup
  {
    uint32_t startPress = millis();
    display->fillScreen(EPD_WHITE);
    display->setCursor(display->width() / 3, 40);
    display->setFont(&FreeSans9pt7b);
    display->setTextSize(2);
    display->setTextColor(EPD_BLACK);
    display->println("Hold Buttons To Clear Settings");
    display->display();
    while (!digitalRead(BUTTON_KEY1) && !digitalRead(BUTTON_KEY2) && (millis() - startPress < 3000))
    {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    if (!digitalRead(BUTTON_KEY1) && !digitalRead(BUTTON_KEY2))
    {
      display->println("Cleared. Rebooting...");
      display->display();
      Serial.println("Wifi and account details cleared");
      preferences.clear();
      preferences.putString(NVS_PETKIT_REGION_KEY, "us");
      preferences.end();
      WiFi.disconnect(true, true);
      delay(4000);
      ESP.restart();
    }
    else
    {
      display->fillScreen(EPD_WHITE);
      display->display();
      digitalWrite(LED_PIN, LOW);
    }
  }
  attachInterrupt(BUTTON_KEY1, button_1_isr, FALLING);
  attachInterrupt(BUTTON_KEY2, button_2_isr, FALLING);

  plotindex = preferences.getInt(NVS_PLOT_TYPE_KEY, 0);
  if (plotindex < 0)
    plotindex = 0;
  if (plotindex >= (int)PlotType::Plot_Type_Max)
    plotindex = (int)PlotType::Plot_Type_Max - 1;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);

  if (digitalRead(SD_DET_PIN))
  {
    Serial.println("No SD card detected. Please insert a card.");
  }
  else
  {
    Serial.println("SD card detected, attempting to mount...");
    if (!SD.begin(SD_CS_PIN, hspi))
    {
      Serial.println("SD Card Mount Failed!");
    }
    else
    {
      Serial.println("SD Card Mounted.");
      loadDataFromSD(allPetData);
    }
  }

  Wire.setPins(I2C_SDA, I2C_SCL);
  if (!sht4.begin())
  {
    Serial.println("Couldn't find SHT4x");
  }

  initializeFromRtc();
  WiFiProvisioner provisioner(provisionerCustom);
  // Set up callbacks
  provisioner.onProvision([]()
                          { Serial.println("Provisioning started."); })

      .onSuccess([](const char *ssid, const char *password, const char *input, const char *pkuser, const char *pkpass)
                 {
                   Serial.printf("Connected to SSID: %s\n", ssid);
                   preferences.putString(NVS_SSID_KEY, ssid);
                   if (password)
                   {
                     Serial.printf("Wifi Password: %s\n", password);
                     preferences.putString(NVS_WIFI_PASS_KEY, password);
                   }
                   if (pkuser)
                   {
                     Serial.printf("Petkit Username: %s\n", pkuser);
                     preferences.putString(NVS_PETKIT_USER_KEY, pkuser);
                   }
                   if (pkpass)
                   {
                     Serial.printf("Petkit Password: %s\n", pkpass);
                     preferences.putString(NVS_PETKIT_PASS_KEY, pkpass);
                   }
                   Serial.println("Provisioning completed successfully! Restarting.");
                   //disp->fillScreen(EPD_WHITE);
                   //disp->setCursor(disp->width()/3, 40);
                   //disp->setFont(&FreeSans9pt7b );
                   //disp->setTextSize(1);
                  // disp->setTextColor(EPD_BLACK);
                  // disp->print("Credentials Stored! Rebooting.");
                   //disp->display();
                   //delay(4000);
                   preferences.end(); })
      .onFactoryReset([]()
                      {
        Serial.println("Factory reset triggered! Clearing preferences...");
        preferences.clear(); // Clear all stored credentials and API key
       preferences.end();
       WiFi.disconnect(true, true);
       ESP.restart(); });

  String WifiSSID = preferences.getString(NVS_SSID_KEY, "");
  if (WifiSSID.length() > 0)
  {
    Serial.print("Recalled Wifi SSID from NVS: ");
    Serial.println(WifiSSID);
  }
  else
    Serial.println("No Wifi SSID found in NVS");
  String WifiPass = preferences.getString(NVS_WIFI_PASS_KEY, "");
  if (WifiPass.length() > 0)
  {
    Serial.print("Recalled Wifi Password from NVS: ");
    Serial.println(WifiPass);
  }
  else
    Serial.println("No Wifi Password found in NVS");

  WiFi.begin(WifiSSID, WifiPass);
  Serial.print("Connecting to WiFi");
  digitalWrite(LED_PIN, HIGH);
  uint32_t startWifiTime = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - startWifiTime < WIFI_TIMEOUT))
  {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(" Connected!");
  }
  else
  {
    Serial.println(" Timed Out. Starting provisioning service.");
    // update epaper with message
    display->fillScreen(EPD_WHITE);
    display->setCursor(10, 40);
    display->setFont(&FreeSans9pt7b);
    display->setTextSize(1);
    display->setTextColor(EPD_BLACK);
    display->println("Wifi Connection Timed Out. Please connect to AP: PetkitDashboard to configure.");
    display->display();
    provisioner.startProvisioning();
    preferences.end();
    WiFi.disconnect();
    ESP.restart();
  }
  digitalWrite(LED_PIN, LOW);

  petkitTZ = preferences.getString(NVS_PETKIT_TIMEZONE_KEY, "");
  if (petkitTZ.length() > 0)
  {
    Serial.print("Recalled Petkit Timezone from NVS: ");
    Serial.println(petkitTZ);
  }
  else
    Serial.println("No Petkit Timezone found in NVS");

  getTimezoneAndSync();

  petkitUser = preferences.getString(NVS_PETKIT_USER_KEY, "");
  if (petkitUser.length() > 0)
  {
    Serial.print("Recalled Petkit Username from NVS: ");
    Serial.println(petkitUser);
  }
  else
    Serial.println("No Petkit Username found in NVS");
  petkitPass = preferences.getString(NVS_PETKIT_PASS_KEY, "");
  if (petkitPass.length() > 0)
  {
    Serial.println("Recalled Petkit Password from NVS.");
  }
  else
    Serial.println("No Petkit Password found in NVS");
  petkitRegion = preferences.getString(NVS_PETKIT_REGION_KEY, "");

  if ((petkitUser.length() > 0) && (petkitPass.length() > 0) && (petkitRegion.length() > 0) && (petkitTZ.length() > 0))
  {

    petkit = new PetKitApi(petkitUser.c_str(), petkitPass.c_str(), petkitRegion.c_str(), petkitTZ.c_str(), LED_PIN);
  }
  else
  {
    display->fillScreen(EPD_WHITE);
    display->setCursor(10, 40);
    display->setFont(&FreeSans9pt7b);
    display->setTextSize(1);
    display->setTextColor(EPD_BLACK);
    if (petkitUser.length() == 0)
    {
      Serial.println("petkit username not found.");
      display->println("petkit username not found.");
    }
    if (petkitPass.length() == 0)
    {
      Serial.println("petkit password not found.");
      display->println("petkit password not found.");
    }
    if (petkitRegion.length() == 0)
    {
      Serial.println("petkit region code not found.");
      display->println("petkit region code not found.");
    }
    if (petkitTZ.length() == 0)
    {
      Serial.println("petkit timezone not found.");
      display->println("petkit timezone not found.");
    }
    Serial.println("please use provisioning portal to load WiFi and Login Details.");
    display->setCursor(10, 80);
    display->println("please Connect to PetkitDashboard WiFI AP to load credentials");
    display->display();
    provisioner.startProvisioning();
    preferences.end();
    WiFi.disconnect();
    ESP.restart();
  }

  if (petkit->login())
  {
    Serial.println("\nLogin successful!");
    // 1. Find the latest timestamp from all loaded SD data
    time_t latestTimestamp = 0;

    // C++11 compatible way
    for (auto const &petPair : allPetData)
    {
      // int petId = petPair.first; // Key (if you need it)
      const std::map<time_t, LitterboxRecord> &recordsMap = petPair.second; // Value

      if (!recordsMap.empty())
      {
        // .rbegin()->first still works fine
        time_t petLatest = recordsMap.rbegin()->first;
        if (petLatest > latestTimestamp)
        {
          latestTimestamp = petLatest;
        }
      }
    }

    // 2. Calculate days to fetch
    int daysToFetch = 30; // Default to 30 days (max)

    if (latestTimestamp > 0)
    {
      time_t now = time(NULL);
      long secondsDifference = now - latestTimestamp;
      Serial.printf("Latest timestamp from SD card: %lu, %f days ago.\r\n", latestTimestamp, (float)secondsDifference / (60.0 * 60.0 * 24.0));
      // Convert seconds to days, add 1 to be safe (to cover the partial day)
      int daysDifference = (int)(secondsDifference / 86400) + 2;

      // Clamp the value: must be at least 2, but no more than 30
      daysToFetch = std::min(std::max(daysDifference, 1), 30);
      Serial.printf("Requesting %d days of data from petkit servers.\r\n", daysToFetch);
    }
    // fetchAllData() gets devices, pets, and historical records in one call
    if (petkit->fetchAllData(daysToFetch))
    {
      // --- Get Pet Information ---
      const auto &pets = petkit->getPets();
      Serial.printf("\nFound %zu pets:\n", pets.size());
      if (pets.empty())
      {
        Serial.println("No pets found on this account. Exiting.");
        // TODO: Display error on e-paper
        return;
      }
      for (const auto &pet : pets)
      {
        Serial.printf(" - Pet ID: %d, Name: %s\n", pet.id, pet.name.c_str());
      }
      // --- Get Latest Litterbox Status ---
      latest_status = petkit->getLatestStatus();
      if (latest_status.device_name != "")
      {
        char timeStr[32];
        // Use localtime() to convert the Unix timestamp to a human-readable format
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&latest_status.timestamp));
        Serial.println("\n--- Latest Status ---");
        Serial.printf("Device: %s\n", latest_status.device_name.c_str());
        Serial.printf("Time: %s\n", timeStr);
        Serial.printf("Litter Level: %d%%\n", latest_status.litter_percent);
        Serial.printf("Waste Box Full: %s\n", latest_status.box_full ? "Yes" : "No");
        Serial.printf("Litter Low: %s\n", latest_status.sand_lack ? "Yes" : "No");
        Serial.println("---------------------");
      }

      // --- Create plot data vectors dynamically based on number of pets, (these will be populated from the map) ---
      size_t numPets = pets.size();
      std::vector<LitterboxRecord> pet_records[numPets];
      std::vector<DataPoint> pet_scatterplot[numPets];
      std::vector<float> weight_hist[numPets];
      std::vector<float> interval_hist[numPets];
      std::vector<float> duration_hist[numPets];

      Serial.println("\n--- Merging API records with local data ---");
      int idx = 0;
      for (const auto &pet : pets)
      {
        // Get records from API
        pet_records[idx] = petkit->getLitterboxRecordsByPetId(pet.id);

        if (pet_records[idx].empty())
        {
          Serial.printf("No new API records for %s.\n", pet.name.c_str());
        }
        else
        {
          Serial.printf("Merging %d new API records for %s.\n", pet_records[idx].size(), pet.name.c_str());
        }

        // *** MERGE API DATA INTO OUR MAP ***
        for (const auto &record : pet_records[idx])
        {
          allPetData[pet.id][record.timestamp] = record;
        }
        idx++;
      }

      // *** SAVE THE MERGED DATA BACK TO SD CARD ***
      saveDataToSD(allPetData);
      
      Serial.println("\n--- Processing all historical records for plotting ---");
      idx = 0;
      for (const auto &pet : pets)
      {
        Serial.printf("\n--- Processing records for %s (ID: %d) ---\n", pet.name.c_str(), pet.id);

        // *** POPULATE PLOT VECTORS FROM THE MAP (allPetData) ***
        if (allPetData.find(pet.id) == allPetData.end() || allPetData[pet.id].empty())
        {
          Serial.println("No historical records found for this pet.");
          idx++;
          continue;
        }

        time_t lastTimestamp = -1;
        // Iterate through the map (which is sorted by timestamp)
        // C++11 compatible way
        for (auto const &recordPair : allPetData[pet.id])
        {
          // time_t timestamp = recordPair.first; // Key (if you need it)
          const LitterboxRecord &record = recordPair.second; // Value

          float weight_lbs = (float)record.weight_grams / GRAMS_PER_POUND;

          pet_scatterplot[idx].push_back({(float)record.timestamp, weight_lbs});
          weight_hist[idx].push_back(weight_lbs);
          duration_hist[idx].push_back((float)record.duration_seconds / 60.0);

          if (lastTimestamp > 0)
            interval_hist[idx].push_back(((float)(record.timestamp - lastTimestamp)) / 3600.0);
          lastTimestamp = record.timestamp;
        }
        idx++;
        lastTimestamp = -1.0;
      }

      // --- START REFACTORED PLOTTING BLOCK ---
      switch (thisPlot.type)
      {
      case Scatterplot_Weight:
      {
        myPlot = new ScatterPlot(display, 0, 0, EPD_WIDTH, EPD_HEIGHT);
        myPlot->setLabels("Weight (lb)", "Date", "Weight(lb)");

        // Loop and add all pets
        for (int i = 0; i < numPets; ++i)
        {
          myPlot->addSeries(pets[i].name.c_str(), pet_scatterplot[i], petColors[i % petColors.size()]);
        }
        myPlot->draw();
      }
      break;
      case Histogram_Duration:
      {
        display->fillScreen(GxEPD_WHITE);
        Histogram histogram(display, 0, 0, display->width(), display->height());
        histogram.setTitle("Duration (Minutes) - Normalized");
        histogram.setBinCount(30);
        histogram.setNormalization(true); // Enable normalization

        // Loop and add all pets
        for (int i = 0; i < numPets; ++i)
        {
          histogram.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram.plot();
      }
      break;
      case Histogram_Interval:
      {
        display->fillScreen(GxEPD_WHITE);
        Histogram histogram(display, 0, 0, display->width(), display->height());
        histogram.setTitle("Interval (Hours) - Normalized");
        histogram.setBinCount(40);
        histogram.setNormalization(true); // Enable normalization

        // Loop and add all pets
        for (int i = 0; i < numPets; ++i)
        {
          histogram.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram.plot();
      }
      break;
      case Histograms:
      {
        display->fillScreen(GxEPD_WHITE);
        Histogram histogram1(display, 0, 0, display->width(), display->height() / 2);
        histogram1.setTitle("Interval (Hours) - Normalized");
        histogram1.setBinCount(30);
        histogram1.setNormalization(true); // Enable normalization

        // Loop and add all pets to histogram 1
        for (int i = 0; i < numPets; ++i)
        {
          histogram1.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram1.plot();

        Histogram histogram2(display, 0, display->height() / 2, display->width(), display->height() / 2);
        histogram2.setTitle("Duration (Minutes) - Normalized");
        histogram2.setBinCount(30);
        histogram2.setNormalization(true); // Enable normalization

        // Loop and add all pets to histogram 2
        for (int i = 0; i < numPets; ++i)
        {
          histogram2.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram2.plot();
      }
      break;
      case ComboPlot:
      {
        display->fillScreen(GxEPD_WHITE);
        Histogram histogram1(display, 0, display->height() * 2 / 3, display->width() / 2, display->height() / 3);
        histogram1.setTitle("Interval (Hours)");
        histogram1.setBinCount(20);
        histogram1.setNormalization(true); // Enable normalization

        // Loop and add all pets to histogram 1
        for (int i = 0; i < numPets; ++i)
        {
          histogram1.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram1.plot();

        Histogram histogram2(display, display->width() / 2, display->height() * 2 / 3, display->width() / 2, display->height() / 3);
        histogram2.setTitle("Duration (Minutes)");
        histogram2.setBinCount(20);
        histogram2.setNormalization(true); // Enable normalization

        // Loop and add all pets to histogram 2
        for (int i = 0; i < numPets; ++i)
        {
          histogram2.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram2.plot();

        myPlot = new ScatterPlot(display, 0, 0, EPD_WIDTH, EPD_HEIGHT * 2 / 3);
        myPlot->setLabels("Weight (lb)", "Date", "Weight(lb)");

        // Loop and add all pets to scatter plot
        for (int i = 0; i < numPets; ++i)
        {
          myPlot->addSeries(pets[i].name.c_str(), pet_scatterplot[i], petColors[i % petColors.size()]);
        }
        myPlot->draw();
      }
      break;

      default:
        break;
      }
      // --- END REFACTORED PLOTTING BLOCK ---
    }
    else
    {
      Serial.println("Failed to fetch data.");
    }
  }
  else
  {
    // Start provisioning
    display->fillScreen(EPD_WHITE);
    display->setCursor(10, 40);
    display->setFont(&FreeSans9pt7b);
    display->setTextSize(1);
    display->setTextColor(EPD_BLACK);
    display->println("please Connect to PetkitDashboard WiFI AP to load credentials");
    display->display();
    provisioner.startProvisioning();
    preferences.end();
    WiFi.disconnect();
    ESP.restart();
    // update epaper with message
    Serial.println("Login failed. Starting Provisioning Service to provide updated credentials.");
  }
  latest_status = petkit->getLatestStatus();
  if (latest_status.device_name != "")
  {
    display->setFont(NULL);
    display->setTextSize(0);
    display->setTextColor(EPD_BLACK);

    char buffer[32];
    int16_t x = EPD_WIDTH * 3 / 4, y = 2, x1, y1;
    uint16_t w, h;
    sprintf(buffer, "Litter: %d%%", latest_status.litter_percent);
    display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
    x = EPD_WIDTH - 20 - w - 120;
    display->setCursor(x, h / 2);
    display->print(buffer);

    display->setCursor(x, 3 * h / 2 + 4);
    if (latest_status.box_full)
    {
      display->print("FULL");
    }
    else
    {
      display->print("Box OK");
    }
  }

  int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
  battery_voltage = (mv / 1000.0) * 2;
  int16_t x = 0, y = 0, x1 = 0, y1 = 0;
  uint16_t w = 0, h = 0;
  if (battery_voltage >= 4.2)
  {
    battery_voltage = 4.2;
  }
  char buffer[32];
  sprintf(buffer, "Battery: %.2fV", battery_voltage);
  display->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
  x = EPD_WIDTH - w - 20;
  y = h * 3 / 2 + 4;
  display->setFont(NULL); // Use the provided font
  display->setTextSize(1);
  display->setTextColor(EPD_BLACK); // Use the provided color
  display->setCursor(x, y);
  display->print(buffer);

  // display plots and go for long nap
  display->display();
  if (battery_voltage < 3.70)
  {
    delay(4000); // display seems to go a bit dark
    display->hibernate();
  }
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);
  Serial.println("enter deep sleep");
  uint64_t sleepInterval = 1000000ull * 60ull * 60ull * 3ull; // 3hr
  esp_sleep_enable_timer_wakeup(sleepInterval);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_KEY0, 0);
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  esp_deep_sleep_start();
}

void loop()
{
  // Nothing to do here
}

bool initializeFromRtc()
{
  if (!rtc.begin())
  {
    Serial.println("[ERROR] Couldn't find RTC! Clock will not keep time without power.");
    return false;
  }

  if (rtc.lostPower())
  {
    Serial.println("[WARN] RTC lost power. Setting to compile time as fallback.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime rtcnow = rtc.now();
  if (rtcnow.year() < 2024)
  {
    Serial.printf("[WARN] RTC has an invalid time (Year: %d). Waiting for WiFi sync.\n", rtcnow.year());
    return false;
  }

  struct timeval tv = {.tv_sec = static_cast<time_t>(rtcnow.unixtime()), .tv_usec = 0};
  settimeofday(&tv, NULL);
  Serial.println("System time initialized from hardware RTC.");

  String tz_string = preferences.getString(NVS_TZ_KEY, "");
  if (tz_string.length() > 0)
  {
    strncpy(time_zone, tz_string.c_str(), sizeof(time_zone) - 1);
    setenv("TZ", tz_string.c_str(), 1);
    tzset();
    Serial.printf("Timezone set from NVS: %s\n", time_zone);
  }
  else
  {
    Serial.println("Timezone not yet known, defaulting to UTC for now.");
  }

  return true;
}

bool getTimezoneAndSync()
{
  WiFiClientSecure client;
  client.setCACert(root_ca_worldtimeapi);
  HTTPClient http;
  http.addHeader("Content-Type", "application/json");

  if (petkitTZ.equals(""))
  {
    bool tz_success = false;
    // --- Retry loop for fetching timezone ---
    for (int i = 0; i < MAX_SYNC_RETRIES; ++i)
    {
      // delay(random(200, 2000));
      Serial.printf("[Time Sync] Fetching timezone, attempt %d/%d...\n", i + 1, MAX_SYNC_RETRIES);
      if (http.begin(client, TIME_API_URL))
      {
        http.setConnectTimeout(8000);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK)
        {
          JsonDocument doc;
          if (deserializeJson(doc, http.getStream()).code() == DeserializationError::Ok)
          {
            const char *tz_iana = doc["timezone"];
            if (tz_iana)
            {
              Serial.print("Found petkit (IANA) timezone: ");
              Serial.println(tz_iana);
              preferences.putString(NVS_PETKIT_TIMEZONE_KEY, tz_iana);
              const char *tz_posix = TzDbLookup::getPosix(tz_iana);
              strncpy(time_zone, tz_posix, sizeof(time_zone) - 1);
              Serial.printf("[Time Sync] Fetched Timezone (POSIX): %s)\n", tz_posix);
              preferences.putString(NVS_TZ_KEY, time_zone);
              tz_success = true;
              // http.end();
              break; // Exit retry loop on success
            }
          }
        }
        http.end();
      }

      Serial.println("[Time Sync] Failed to fetch timezone on this attempt.");
      delay(500);
    }
    if (!tz_success)
    {
      Serial.println("[Time Sync] Failed to fetch timezone after all retries.");
      // return false;
    }
  }
  else
    strncpy(time_zone, petkitTZ.c_str(), min(petkitTZ.length(), sizeof(time_zone)));
  // --- Retry loop for NTP sync ---
  for (int i = 0; i < MAX_SYNC_RETRIES; ++i)
  {
    Serial.printf("[Time Sync] Syncing with NTP server, attempt %d/%d...\n", i + 1, MAX_SYNC_RETRIES);
    configTzTime(time_zone, NTP_SERVER_1, NTP_SERVER_2);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 15000))
    { // 15-second timeout for NTP
      Serial.println("[Time Sync] System time synced via NTP.");

      time_t now_utc;
      time(&now_utc);
      rtc.adjust(DateTime(now_utc));
      Serial.println("[Time Sync] RTC has been updated with correct UTC time.");

      setenv("TZ", time_zone, 1);
      tzset();
      char time_buf[64];
      localtime_r(&now_utc, &timeinfo);
      strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S %Z", &timeinfo);
      Serial.println(time_buf);

      return true; // Return true on success
    }
    Serial.println("[Time Sync] Failed to get local time from NTP server on this attempt.");
    delay(3000);
  }

  Serial.println("[Time Sync] Failed to sync NTP after all retries.");
  return false;
}

void loadDataFromSD(std::map<int, std::map<time_t, LitterboxRecord>> &petData)
{
  if (!SD.exists(SD_DATA_FILE))
  {
    Serial.println("No data file found on SD card. Will create one.");
    return;
  }

  File file = SD.open(SD_DATA_FILE, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open data file for reading.");
    return;
  }

  // Use DynamicJsonDocument for unknown file sizes
  // Adjust size as needed, ESP32-S3 has PSRAM
  JsonDocument doc; // 30KB, adjust as your history grows

  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("Failed to parse JSON, error: ");
    Serial.println(error.c_str());
    return;
  }

  JsonObject root = doc.as<JsonObject>();
  for (JsonPair petPair : root)
  {
    int petId = atoi(petPair.key().c_str());
    JsonArray records = petPair.value().as<JsonArray>();

    for (JsonObject recordJson : records)
    {
      LitterboxRecord rec;
      rec.timestamp = recordJson["ts"];
      rec.weight_grams = recordJson["w_g"];
      rec.duration_seconds = recordJson["dur_s"];
      rec.pet_id = petId; // Assuming LitterboxRecord has a petId field

      // Add to our map. The map structure automatically handles duplicates.
      petData[petId][rec.timestamp] = rec;
    }
  }
  Serial.println("Successfully loaded historical data from SD card.");
}

void saveDataToSD(const std::map<int, std::map<time_t, LitterboxRecord>> &petData)
{
  JsonDocument doc; // 30KB, adjust as your history grows
  JsonObject root = doc.to<JsonObject>();

  // C++11 compatible way to iterate the outer map
  for (auto const &petPair : petData)
  {
    int petId = petPair.first;
    const std::map<time_t, LitterboxRecord> &recordsMap = petPair.second;

    // JsonArray petArray = root.createNestedArray(String(petId));
    JsonArray petArray = root[String(petId)].to<JsonArray>();

    // C++11 compatible way to iterate the inner map
    for (auto const &recordPair : recordsMap)
    {
      // time_t timestamp = recordPair.first; // Key (if you need it)
      const LitterboxRecord &record = recordPair.second; // Value

      // JsonObject recJson = petArray.createNestedObject();
      JsonObject recJson = petArray.add<JsonObject>();
      recJson["ts"] = record.timestamp;
      recJson["w_g"] = record.weight_grams;
      recJson["dur_s"] = record.duration_seconds;
    }
  }
  File file = SD.open(SD_DATA_FILE, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open data file for writing.");
    return;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write to data file.");
  }
  else
  {
    Serial.println("Successfully saved updated data to SD card.");
  }
  file.close();
}