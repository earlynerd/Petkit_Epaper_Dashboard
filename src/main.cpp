#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "PetKitApi.h"
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
#include "Adafruit_SHT4x.h"
#include <TzDbLookup.h>
#include "WiFiProvisioner.h"


Preferences preferences;
PetKitApi* petkit;
SPIClass hspi(HSPI);
RTC_PCF8563 rtc;
ScatterPlot *myPlot;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();


std::vector<uint16_t> petColors = {EPD_RED, EPD_BLUE, EPD_GREEN, EPD_YELLOW, EPD_BLACK};

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(/*CS=*/EPD_CS_PIN, /*DC=*/EPD_DC_PIN,
                                /*RST=*/EPD_RES_PIN, /*BUSY=*/EPD_BUSY_PIN));

char time_zone[64];


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

void button_1_isr()
{
  plotindex++;
  if(plotindex < 0 )plotindex = PlotType::Plot_Type_Max-1;
  if(plotindex >= (int)PlotType::Plot_Type_Max) plotindex = 0;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);
  Serial.print("Type changed to ");
  Serial.println(thisPlot.name);
}

void button_2_isr()
{
  plotindex--;
  if(plotindex < 0 )plotindex = PlotType::Plot_Type_Max;;
  if(plotindex > (int)PlotType::Plot_Type_Max) plotindex = 0;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);
  Serial.print("Type changed to ");
  Serial.println(thisPlot.name);
}

void setup()
{
  Serial.begin(115200);

  psramInit();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);

  pinMode(BUTTON_KEY0, INPUT);
  pinMode(BUTTON_KEY1, INPUT);
  pinMode(BUTTON_KEY2, INPUT);
  attachInterrupt(BUTTON_KEY1, button_1_isr, FALLING);
  attachInterrupt(BUTTON_KEY2, button_2_isr, FALLING);

  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH); // Enable battery monitoring
  preferences.begin(NVS_NAMESPACE);


  String petkitUser = preferences.getString(NVS_PETKIT_USER_KEY, "");
  String petkitPass = preferences.getString(NVS_PETKIT_PASS_KEY, "");
  String petkitRegion = preferences.getString(NVS_PETKIT_REGION_KEY, "");
  String petkitTZ = preferences.getString(NVS_PETKIT_TIMEZONE_KEY, "");

 

  plotindex= preferences.getInt(NVS_PLOT_TYPE_KEY, 0);
  if(plotindex < 0 )plotindex = 0;
  if(plotindex >= (int)PlotType::Plot_Type_Max) plotindex = (int)PlotType::Plot_Type_Max-1;
  thisPlot = plotinfo[plotindex];
  preferences.putInt(NVS_PLOT_TYPE_KEY, plotindex);

  // Configure ADC
  analogReadResolution(12); // 12-bit resolution
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  // Initialize SPI
  hspi.begin(EPD_SCK_PIN, SD_MISO_PIN, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.setTextSize(1);
  pinMode(SD_EN_PIN, OUTPUT);
  digitalWrite(SD_EN_PIN, HIGH);
  pinMode(SD_DET_PIN, INPUT_PULLUP);
  delay(100);

  // Initialize display
  display.init(0);

  if (digitalRead(SD_DET_PIN) == HIGH) {
    Serial.println("No SD card detected. Please insert a card.");
  }

  Serial.println("SD card detected, attempting to mount...");
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed!");
  }
  else Serial.println("SD Card Mounted.");

  Wire.setPins(I2C_SDA, I2C_SCL);
  if (!sht4.begin())
  {
    Serial.println("Couldn't find SHT4x");
  }

  initializeFromRtc();


  WiFi.begin();
  Serial.print("Connecting to WiFi");
  digitalWrite(LED_PIN, HIGH);
  uint32_t startWifiTime = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - startWifiTime < WIFI_TIMEOUT))
  {
    delay(500);
    Serial.print(".");
  }
  if(WiFi.status() == WL_CONNECTED)
  {
    Serial.println(" Connected!");
  }
  else{
    Serial.println(" Timed Out. Starting provisioning service.");
    //update epaper with message
    //PROVISIONING
  }
  digitalWrite(LED_PIN, LOW);

  getTimezoneAndSync();
  
  if((petkitUser.length() > 0) && (petkitPass.length() > 0) && (petkitRegion.length() > 0) && (petkitTZ.length() > 0))
  {
    petkit = new PetKitApi(petkitUser.c_str(),petkitPass.c_str(),  petkitRegion.c_str(), petkitTZ.c_str(), LED_PIN);
  }
  else
  {
    Serial.println("No credentials found in NVS, please use provisioning portal to load WiFi and Login Details.");
    //update epaper with message
    //PROVISIONING
  }

  if (petkit->login())
  {
    Serial.println("\nLogin successful! Fetching data...");

    // fetchAllData() gets devices, pets, and historical records in one call
    if (petkit->fetchAllData(30))
    { 
      // --- Get Pet Information ---
      const auto &pets = petkit->getPets();
      Serial.printf("\nFound %zu pets:\n", pets.size());
      if (pets.empty()) {
        Serial.println("No pets found on this account. Exiting.");
        // TODO: Display error on e-paper
        return;
      }
      for (const auto &pet : pets)
      {
        Serial.printf(" - Pet ID: %d, Name: %s\n", pet.id, pet.name.c_str());
      }
      // --- Get Latest Litterbox Status ---
      StatusRecord latest_status = petkit->getLatestStatus();
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

      // --- Create plot data vectors dynamically based on number of pets ---
      size_t numPets = pets.size();
      std::vector<LitterboxRecord> pet_records[numPets];
      std::vector<DataPoint> pet_scatterplot[numPets];
      std::vector<float> weight_hist[numPets];
      std::vector<float> interval_hist[numPets];
      std::vector<float> duration_hist[numPets];

      Serial.println("\n--- Historical Records by Pet ---");
      int idx = 0;
      for (const auto &pet : pets)
      {
        Serial.printf("\n--- Records for %s (ID: %d) ---\n", pet.name.c_str(), pet.id);

        // Use the helper function to get a vector of records just for this pet
        pet_records[idx] = petkit->getLitterboxRecordsByPetId(pet.id);

        if (pet_records[idx].empty())
        {
          Serial.println("No records found for this pet in the last 30 days.");
          idx++; // Don't forget to increment index even if no records
          continue;
        }
        time_t lastTimestamp = -1;
        // Now, loop through the filtered list for this specific pet
        for (const auto &record : pet_records[idx])
        {
          float weight_lbs = (float)record.weight_grams / GRAMS_PER_POUND;
          char timeStr[32];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&record.timestamp));

          Serial.printf("[%s] Weight: %.2f lbs, Duration: %d sec\n",
                        timeStr,
                        weight_lbs,
                        record.duration_seconds);
          pet_scatterplot[idx].push_back({(float)record.timestamp, ((float)record.weight_grams / (float)GRAMS_PER_POUND)});
          weight_hist[idx].push_back((float)record.weight_grams / (float)GRAMS_PER_POUND);
          duration_hist[idx].push_back((float)record.duration_seconds/60.0);
          if (lastTimestamp > 0)
          interval_hist[idx].push_back(((float)(lastTimestamp - record.timestamp))/3600.0);
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
        myPlot = new ScatterPlot(&display, 0, 0, EPD_WIDTH, EPD_HEIGHT);
        myPlot->setLabels("Weight (lb)", "Date", "Weight(lb)");
        
        // Loop and add all pets
        for (int i = 0; i < numPets; ++i) {
          myPlot->addSeries(pets[i].name.c_str(), pet_scatterplot[i], petColors[i % petColors.size()]);
        }
        myPlot->draw();
      }
      break;
      case Histogram_Duration:
      {
        display.fillScreen(GxEPD_WHITE);
        Histogram histogram(&display, 0, 0, display.width(), display.height());
        histogram.setTitle("Duration (Minutes) - Normalized");
        histogram.setBinCount(30); 
        histogram.setNormalization(true); // Enable normalization
        
        // Loop and add all pets
        for (int i = 0; i < numPets; ++i) {
          histogram.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram.plot();
      }
      break;
      case Histogram_Interval:
      {
        display.fillScreen(GxEPD_WHITE);
        Histogram histogram(&display, 0, 0, display.width(), display.height());
        histogram.setTitle("Interval (Hours) - Normalized");
        histogram.setBinCount(40);
        histogram.setNormalization(true); // Enable normalization
        
        // Loop and add all pets
        for (int i = 0; i < numPets; ++i) {
          histogram.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram.plot();
      }
      break;
      case Histograms:
      {
        display.fillScreen(GxEPD_WHITE);
        Histogram histogram1(&display, 0, 0, display.width(), display.height()/2);
        histogram1.setTitle("Interval (Hours) - Normalized");
        histogram1.setBinCount(30);
        histogram1.setNormalization(true); // Enable normalization
        
        // Loop and add all pets to histogram 1
        for (int i = 0; i < numPets; ++i) {
          histogram1.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram1.plot();

        Histogram histogram2(&display, 0, display.height()/2, display.width(), display.height()/2);
        histogram2.setTitle("Duration (Minutes) - Normalized");
        histogram2.setBinCount(30);
        histogram2.setNormalization(true); // Enable normalization
        
        // Loop and add all pets to histogram 2
        for (int i = 0; i < numPets; ++i) {
          histogram2.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram2.plot();
      }
      break;
      case ComboPlot:
      {
        display.fillScreen(GxEPD_WHITE);
        Histogram histogram1(&display, 0, display.height()*2 / 3, display.width()/2, display.height()/3);
        histogram1.setTitle("Interval (Hours)"); 
        histogram1.setBinCount(20);
        histogram1.setNormalization(true); // Enable normalization
        
        // Loop and add all pets to histogram 1
        for (int i = 0; i < numPets; ++i) {
          histogram1.addSeries(pets[i].name.c_str(), interval_hist[i], petColors[i % petColors.size()]);
        }
        histogram1.plot();

        Histogram histogram2(&display, display.width()/2, display.height()*2 / 3, display.width()/2, display.height()/3);
        histogram2.setTitle("Duration (Minutes)"); 
        histogram2.setBinCount(20);
        histogram2.setNormalization(true); // Enable normalization
        
        // Loop and add all pets to histogram 2
        for (int i = 0; i < numPets; ++i) {
          histogram2.addSeries(pets[i].name.c_str(), duration_hist[i], petColors[i % petColors.size()]);
        }
        histogram2.plot();

        myPlot = new ScatterPlot(&display, 0, 0, EPD_WIDTH, EPD_HEIGHT*2/3);
        myPlot->setLabels("Weight (lb)", "Date", "Weight(lb)");
        
        // Loop and add all pets to scatter plot
        for (int i = 0; i < numPets; ++i) {
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
    ////provisioning
    //update epaper with message
    Serial.println("Login failed. Starting Provisioning Service to provide updated credentials.");
    
  }
  
  //display plots and go for long nap
  display.display();
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
  bool tz_success = false;

  // --- Retry loop for fetching timezone ---
  for (int i = 0; i < MAX_SYNC_RETRIES; ++i)
  {
    delay(random(200, 2000));
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
            preferences.putString(NVS_PETKIT_TIMEZONE_KEY, tz_iana);
            const char *tz_posix = TzDbLookup::getPosix(tz_iana);
            strncpy(time_zone, tz_posix, sizeof(time_zone) - 1);
            Serial.printf("[Time Sync] Fetched Timezone (POSIX): %s)\n", tz_posix);
            preferences.putString(NVS_TZ_KEY, time_zone);
            tz_success = true;
            http.end();
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
