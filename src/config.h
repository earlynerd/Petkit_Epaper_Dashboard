#ifndef CONFIG_H__
#define CONFIG_H__
#include "WiFiProvisioner.h"

#define EPD_WIDTH 800
#define EPD_HEIGHT 480

// Define ePaper SPI pins
#define EPD_SCK_PIN 7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN 10
#define EPD_DC_PIN 11
#define EPD_RES_PIN 12
#define EPD_BUSY_PIN 13

#define SERIAL_RX 44
#define SERIAL_TX 43
#define LED_PIN 6 // GPIO6 - Onboard LED (inverted logic)

#define SERIAL_RX 44
#define SERIAL_TX 43

#define BUZZER_PIN 45 // GPIO45 - Buzzer

// I2C pins for reTerminal E Series
#define I2C_SDA 19
#define I2C_SCL 20

// Battery monitoring pins
#define BATTERY_ADC_PIN 1     // GPIO1 - Battery voltage ADC
#define BATTERY_ENABLE_PIN 21 // GPIO21 - Battery monitoring enable

// SD Card Pin Definitions
#define SD_EN_PIN 16  // Power enable for the SD card slot
#define SD_DET_PIN 15 // Card detection pin
#define SD_CS_PIN 14  // Chip Select for the SD card
#define SD_MOSI_PIN 9 // Shared with ePaper Display
#define SD_MISO_PIN 8
#define SD_SCK_PIN 7 // Shared with ePaper Display

// Define button pins according to schematic
const int BUTTON_KEY0 = 3; // KEY0 - GPIO3
const int BUTTON_KEY1 = 4; // KEY1 - GPIO4
const int BUTTON_KEY2 = 5; // KEY2 - GPIO5

// Select the ePaper driver to use
// 0: reTerminal E1001 (7.5'' B&W)
// 1: reTerminal E1002 (7.3'' Color)
#define EPD_SELECT 1001

#if (EPD_SELECT == 1001)
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7 // 7.5'' B&W driver
#elif (EPD_SELECT == 1002)
#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01 // 7.3'' Color driver
#endif

#define MAX_DISPLAY_BUFFER_SIZE 48000

#define MAX_HEIGHT(EPD)                                        \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
         ? EPD::HEIGHT                                         \
         : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#define GRAMS_PER_POUND 453.592
#define NVS_NAMESPACE "petkitplotter"
#define NVS_TZ_KEY "timezone"
#define NVS_PLOT_TYPE_KEY "plottype"

#define NVS_SSID_KEY "ssid"
#define NVS_WIFI_PASS_KEY "pass"
#define NVS_PETKIT_USER_KEY "petkitusername"
#define NVS_PETKIT_PASS_KEY "petkitpassword"
#define NVS_PETKIT_REGION_KEY "petkitregion"
#define NVS_PETKIT_TIMEZONE_KEY "petkittimezone"

#define WIFI_TIMEOUT 20000

#define TIME_API_URL "https://worldtimeapi.org/api/ip"
#define MAX_SYNC_RETRIES 50

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

#define EPD_BLACK 0x0000
#define EPD_BLUE 0x001F
#define EPD_GREEN 0x07E0
#define EPD_RED 0xF800
#define EPD_YELLOW 0xFFE0
#define EPD_WHITE 0xFFFF


#endif