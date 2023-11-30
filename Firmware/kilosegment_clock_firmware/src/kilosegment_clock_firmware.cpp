/*---------------------------------------------------
 * 7 Segment array clock
 *
 * Based on clock by Frugha (https://hackaday.io/project/169632-7-segment-display-array-clock)
 *
 * Modifications by John Bradnam (jbrad2089@gmail.com)
 * - Changed hardware to use 0.28" 7 segment displays.
 * - MAX7219 device, digit and segments order has been changed to simply an otherwise
 *   complex PCB board routing.
 * - Hardware uses Arduino Mini Pro, DS1302 RTC and 4 push buttons
 * - Added Alarm function with selectable alarm music
 * - Added Brightness setting
 * - Added Setup screen to set time, date, alarm, music and brightness
 * - Added Date screen (displays for two seconds when ENTER button is pressed
 * Update 2020-06-16
 * - Added date format option on configuration screen
 * - Increased date screen timeout from 2 to 5 seconds
 * - Added a configuarble choice of font for date display
 *
 * Modifications by ArrestedLightning
 * 2022-07-24
 * - Modified to suport Kilosegment clock board hardware
 * -- GN6932 display drivers, onboard RTC
 * - Support temperature sensor
 * - Expand ASCII table
 * - Add second page of settings.
 * - Add built-in version number and build date
 * 2023-03-19
 * - Added function to auto-show date at configurable interval
 * 2023-10-12
 * - Added basic command-line interface
 *--------------------------------------------------*/

#include <SPI.h>
#include <STM32RTC.h>
#include <FlashStorage_STM32F1.h>
#include <LTR303.h>
#include <Temperature_LM75_Derived.h>
#include <HardwareSerial.h>
#include <IWatchdog.h>
#include "Button.h"
#include "Tunes.h"
#include "Digits.h"
#include "mapping.h"
#include "xx6932.h"
#include "kilosegment_clock_firmware.h"
#include "kilosegment_clock_bootloader.h"
#include "build_info.h"
extern "C" {
  #include "cli.h"
}


/* Pin definitions */
#define LED_CLK   PB13
#define LED_DIN   PB14 //not used
#define LED_DOUT  PB15


#define SPEAKER   PB9
#define SW_UP     PB0
#define SW_DOWN   PB1
#define SW_ENTER  PB10
#define SW_CLOCK  PB3

#define HB_LED    PA15

#define NUM_DISPLAY_DRIVERS 9

//time to press the clock button to exit the menu
#define CLOCK_BUTTON_HOLD_TIME  1000

//I really wanted to support more than this, but this is all ST will support with their libraries,
//and right now I can't bring myself to fork and maintain all the libraries that would need to change
//to support dates farther in the future.
//In all likelyhood, it will never even matter.  If someone is still using one of these in the year
//2100, you have my sincere apologies.
#define MAX_YEAR 100

#ifdef WANT_DEBUG
#define debug_println(...)  Serial1.println(__VA_ARGS__)
#else
#define debug_println(...)
#endif

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

//EEPROM handling
#define EEPROM_ADDRESS 0
#define EEPROM_MAGIC 0x0BAD0DAD

#define WANT_CLI 1

//boolean values seem like they can have problems when saved to and loaded from flash
//if a value other than 0 or 1 is loaded, so use uint8_t for all boolean values.
typedef struct {
  uint32_t magic;
  uint8_t alarmEnable;
  uint8_t alarmMinute;
  uint8_t alarmHour;
  uint8_t format12hr;
  uint8_t brightness;
  uint8_t tune;
  uint8_t tempMode;
  uint8_t formatDmy;
  uint8_t squareFont;
  uint8_t autoBrightness;
  uint8_t colonMode;
  uint8_t amPmIndMode;
  uint8_t auto_show_date_interval_index;
} EEPROM_DATA;

EEPROM_DATA EepromData; //Current EEPROM settings

//pulled from time.h
typedef struct {
  uint8_t Second;
  uint8_t Minute;
  uint8_t Hour;
  uint8_t Wday;   // day of week, sunday is day 1
  uint8_t Day;
  uint8_t Month;
  uint8_t Year;   // offset from 2001;
  bool timeUpdated;
  bool dateUpdated;
} TimeElements;

void clockButtonPressed(void);
void enterButtonPressed(void);
void downButtonPressed(void);
void upButtonPressed(void);

Button* clockButton;
Button* enterButton;
Button* downButton;
Button* upButton;

//Operating modes
bool inSubMenu = false;
//blink rate on setup menus
#define SETUP_FLASH_RATE 200
#define DATE_SCREEN_TIMEOUT 5000//ms
unsigned long setupTimeout;
bool setupDisplayState = false;
bool dateShownFlag = false;
bool commandProcessedFlag = false; //just for LED status indicator


enum ClockButtonModesEnum { CLOCK, DATE, TIME_SET, DATE_SET, ALARM_SET, TUNE_SET, BRIGHT_SET, FORMAT_SET, TEMP_SET, COLON_SET, AMPMIND_SET, AUTO_DATE_INTERVAL_SET };
ClockButtonModesEnum clockMode = CLOCK;
#define MENU_PAGE_1_END FORMAT_SET
#define FIRST_CLOCK_MENU_MODE TIME_SET
#define LAST_CLOCK_MENU_MODE AUTO_DATE_INTERVAL_SET

enum TimeSetMenuEnum { TIME_HOUR, TIME_MIN, TIME_FORMAT };
TimeSetMenuEnum timeSetMode = TIME_HOUR;

enum DateSetMenuEnum { DATE_YEAR, DATE_MONTH, DATE_DAY };
DateSetMenuEnum dateSetMode = DATE_YEAR;

enum AlarmSetMenuEnum { ALARM_HOUR, ALARM_MIN, ALARM_STATE };
AlarmSetMenuEnum alarmSetMode = ALARM_HOUR;

enum FormatSetMenuEnum { DAY_MONTH, FONT_STYLE };
FormatSetMenuEnum formatSetMode = DAY_MONTH;

enum BrightnessSetMenuEnum { BRIGHT_VALUE, BRIGHT_AUTO };
BrightnessSetMenuEnum brightSetMode = BRIGHT_VALUE;

#define TEMPMODE_OFF    0
#define TEMPMODE_F      1
#define TEMPMODE_C      2

#define COLONMODE_OFF   0
#define COLONMODE_ON    1
#define COLONMODE_FLASH 2

#define AMPMIND_OFF     0
#define AMPMIND_ON      1
#define AMPMIND_FLASH   2


int lastSeconds = -1;
bool alarmRinging = false;    //true when alarm is on
bool alarmCancelled = false;  //alarm cancelled by user
bool musicPlaying = false;    //true if playing a song
bool clockColon = false;      //show/hide colon
bool debugMode = false;     //show sensor values on date screen, plus other debug options

unsigned int light_sensor_ch0 = 0;
unsigned int light_sensor_ch1 = 0;
bool light_sensor_valid = false;
uint8_t active_brightness_level = 0;

uint32_t date_screen_entry_time = 0;

static const String tempModeStrings[] = {"OFF", "F", "C"};
#define NUM_TEMP_MODES 3

static const String colonModeStrings[] = {"OFF", "ON", "FLASH"};
#define NUM_COLON_MODES 3

#define ampmIndModeStrings   colonModeStrings
#define NUM_AMPMIND_MODES 3

static const uint16_t auto_date_intervals[] = {0, 10, 15, 30, 60, 300, 600, 900, 1800, 3600, 7200, 14400, 21600, 43200}; //in seconds;
#define NUM_AUTO_DATE_INTERVALS (sizeof(auto_date_intervals) / sizeof(auto_date_intervals[0]))

/* Number of days in each month */
const uint8_t dom[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

//Temporary time structure
TimeElements newTime = { 0 };

const uint32_t led_chip_select_pins[NUM_DISPLAY_DRIVERS] = { PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8 };
//display driver objects
xx6932 display_drivers[NUM_DISPLAY_DRIVERS];
//LED_DIN is not used, but provided here to keep the library happy
SPIClass SPI_2(LED_DOUT, LED_DIN, LED_CLK);
//TRANSMITRECEIVE is necessary so that it waits for transmission to complete
SPISettings spi_settings(500000, LSBFIRST, SPI_MODE3, SPI_TRANSMITRECEIVE);

Generic_LM75_11Bit temp_sensor;

LTR303 light_sensor;

HardwareSerial Serial1(PA10, PA9);

static void cmd_print_help       (char *args, char *response, size_t response_len);
static void cmd_print_version    (char *args, char *response, size_t response_len);
static void cmd_dump_config      (char *args, char *response, size_t response_len);
static void cmd_set_time         (char *args, char *response, size_t response_len);
static void cmd_set_date         (char *args, char *response, size_t response_len);
static void cmd_set_time_format  (char *args, char *response, size_t response_len);
static void cmd_set_brightness   (char *args, char *response, size_t response_len);
static void cmd_set_alarm        (char *args, char *response, size_t response_len);
static void cmd_save_settings    (char *args, char *response, size_t response_len);
static void cmd_fact_reset       (char *args, char *response, size_t response_len);
static void cmd_show_date        (char *args, char *response, size_t response_len);
static void cmd_get_date_time    (char *args, char *response, size_t response_len);
static void cmd_handle_reboot    (char *args, char *response, size_t response_len);
static void cmd_handle_bootloader(char *args, char *response, size_t response_len);

//if you have one command that's named a subset of another one, be sure to list the longer one first
extern "C" const cli_command cli_commands[] = {
        cmd_def(cmd_print_help,                 "?",            "prints out the list of available commands"),
        cmd_def(cmd_print_help,                 "help",         "prints out the list of available commands"),
        cmd_def(cmd_print_version,              "ver?",         "Prints firmware version."                 ),
        cmd_def(cmd_dump_config,                "config?",      "Prints the saved system config."          ),
        cmd_def(cmd_get_date_time,              "date_time?",   "Prints the current date & time."          ),
        cmd_def(cmd_set_time,                   "set_time",     "Set time hour minute second"              ),
        cmd_def(cmd_set_date,                   "set_date",     "Set date year month day"                  ),
        cmd_def(cmd_set_time_format,            "set_format",   "Set display format 12hr=0/1"              ),
        cmd_def(cmd_set_brightness,             "set_bright",   "Set brightness 0-7 [auto=0/1]"            ),
        cmd_def(cmd_set_alarm,                  "set_alarm",    "Set alarm hour minute [on=0/1]"           ),
        cmd_def(cmd_show_date,                  "show_date",    "Shows the date screen"                    ),
        cmd_def(cmd_save_settings,              "save_config",  "Saves the active configuration to EEPROM" ),
        cmd_def(cmd_fact_reset,                 "fact_reset",   "Resets all settings to factory defaults." ),
        cmd_def(cmd_handle_reboot,              "reboot",       "Reboots the device."                      ),
        cmd_def(cmd_handle_bootloader,          "bootload",     "Reboots the device into bootloader mode"  ),
        cmd_def(NULL, NULL, NULL),
};


void setup()
{
  bool config_reset = false;

#if defined(WANT_DEBUG) || defined(WANT_CLI)
  Serial1.begin(115200);
#endif

  EEPROM.init();

  config_reset = readEepromData();

  //Initialize buttons
  clockButton = new Button(SW_CLOCK, SW_CLOCK, CLOCK_BUTTON_HOLD_TIME);
  clockButton->Held(clockButtonHeld);
  enterButton = new Button(SW_ENTER);
  downButton = new Button(SW_DOWN);
  downButton->Repeat(downButtonPressed);
  upButton = new Button(SW_UP);
  upButton->Repeat(upButtonPressed);

  //Initialize sound
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);

  SPI_2.begin();

  //Initialize display drivers
  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
    display_drivers[i].set_spi_instance(&SPI_2);
    display_drivers[i].set_chip_select_pin(led_chip_select_pins[i]);
    display_drivers[i].set_spi_settings(&spi_settings);
  }

  if (EepromData.autoBrightness) {
    //start at minimum brightness and allow auto-brightness to ramp us up if necessary
    active_brightness_level = 0;
  } else {
    active_brightness_level = EepromData.brightness;
  }
  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
    display_drivers[i].init();
    display_drivers[i].set_brightness(active_brightness_level);
  }

  Wire.begin();
  temp_sensor.disableShutdownMode();
  light_sensor.begin();
  delay(100);
  //set the integration time to 50, sample interval of 50
  light_sensor.setMeasurementRate(0, 0);
  //gain is set to 4 empirically
  light_sensor.setControl(2, false, true);
  delay(15);

  //reset if down button is held on boot
  if (digitalRead(SW_DOWN) == LOW) {
    loadDefaults();
    writeEepromData();
    config_reset = true;
  }

  pinMode(HB_LED, OUTPUT);
  digitalWrite(HB_LED, HIGH);//LED is active low, default off

  //make sure we're using the 32KHz external crystal if we want remotely accurate timekeeping
  rtc.setClockSource(STM32RTC::LSE_CLOCK);
  rtc.begin(STM32RTC::HOUR_24);

  if(!rtc.isTimeSet())
  {
    debug_println("RTC not set");
    rtc.setTime(0, 0, 0);
    rtc.setDate(1, 1, tmYearFromCalendar(2023));
  }
  rtc.getTime(&newTime.Hour, &newTime.Minute, &newTime.Second, NULL, NULL);
  rtc.getDate(&newTime.Wday, &newTime.Day, &newTime.Month, &newTime.Year);
  debug_println("Time: " + String(rtc.getHours()) + ":" + String(rtc.getMinutes()) + ":" + String(rtc.getSeconds()));
  debug_println("Date: " + String(rtc.getDay()) + "/" + String(rtc.getMonth()) + "/" + String(rtc.getYear()));
  newTime.timeUpdated = false;
  newTime.dateUpdated = false;

  clockMode = CLOCK;

  if (config_reset) {
    displayString(0, 0, "DEFAULT SETTINGS LOADED");
    updateDisplay();
    delay(5000);
  }

  showTime(true);
#ifdef WANT_CLI
  SerialUSB.begin();
#endif
  IWatchdog.begin(1000000);
}

void loop()
{
  #ifdef WANT_CLI
  handleCLIUSB();
  handleCLISerial();
  #endif

  if (millis() % 100 == 0) {
    testButtons();

    readLightSensor();

    if (clockMode == CLOCK)
    {
      if (EepromData.auto_show_date_interval_index <  NUM_AUTO_DATE_INTERVALS &&
          auto_date_intervals[EepromData.auto_show_date_interval_index] != 0) {
        if (getSecondOfDay() % auto_date_intervals[EepromData.auto_show_date_interval_index] == 0) {
          if (dateShownFlag == false) {
            dateShownFlag = true;
            switchToDateScreen();
            return; //avoid showing new time for 100ms before switching to date screen
          }
        } else {
          dateShownFlag = false;
        }
      }
      showTime(false);
      if (EepromData.alarmEnable && EepromData.alarmHour == rtc.getHours() && EepromData.alarmMinute == rtc.getMinutes())
      {
        if (!alarmCancelled)
        {
          alarmRinging = true;
          playSong(melodies[EepromData.tune]);
        }
      }
      else
      {
        alarmCancelled = false;
        alarmRinging = false;
      }

    } else if (clockMode == DATE) {
      if (millis() - date_screen_entry_time > DATE_SCREEN_TIMEOUT) {
        clockMode = CLOCK;
      } else {
        showDate(rtc.getDay(), rtc.getMonth(), rtc.getYear());
      }
    }
    else
    {
      showSetup(false);
    }

    // delay(100);
    if (debugMode) {
      digitalWrite(HB_LED, !digitalRead(HB_LED));
    } else {
      //blink LED for one cycle if command received
      if (commandProcessedFlag) {
        commandProcessedFlag = false;
        digitalWrite(HB_LED, LOW);
      } else {
        digitalWrite(HB_LED, HIGH);
      }
    }
  }
  IWatchdog.reload();
}

#ifdef WANT_CLI
void handleCLIUSB(void) {
    bool valid_command;
    if (SerialUSB.available()) {
      char line[128];
      char response[1024];
      memset(line, 0, sizeof(line));
      memset(response, 0, sizeof(response));
      size_t lineLength = SerialUSB.readBytesUntil('\n', line, sizeof(line) - 1);
      line[sizeof(line) - 1] = '\0';
      valid_command = parse_command(line, response, sizeof(response) - 1);
      if (!valid_command) {
        SerialUSB.println("Unrecognized command.");
      } else {
        commandProcessedFlag = true;
        SerialUSB.println(response);
      }
    }
}
void handleCLISerial(void) {
    bool valid_command;
    if (Serial1.available()) {
      char line[128];
      char response[1024];
      memset(line, 0, sizeof(line));
      memset(response, 0, sizeof(response));
      size_t lineLength = Serial1.readBytesUntil('\n', line, sizeof(line) - 1);
      line[sizeof(line) - 1] = '\0';
      valid_command = parse_command(line, response, sizeof(response) - 1);
      if (!valid_command) {
        Serial1.println("Unrecognized command.");
      } else {
        commandProcessedFlag = true;
        Serial1.println(response);
      }
    }
}
#endif


void updateRTCandSettings(void) {
    //Update time
    //reset seconds when updating time so as to allow for synchronization with an external clock,
    //but only if time was actually updated by the user while in the settings menu.  This avoids accumulating
    //offset while modifying other settings.
    if (newTime.timeUpdated) {
      debug_println("Saving Time: " + String(newTime.Hour) + ":" + String(newTime.Minute));
      debug_println("       From: " + String(rtc.getHours()) + ":" + String(rtc.getMinutes()));
      newTime.Second = 0;
      rtc.setTime(newTime.Hour, newTime.Minute, newTime.Second, 0, rtc.AM);
    }
    newTime.timeUpdated = false;

    if (newTime.dateUpdated) {
      debug_println("Saving Date: " + String(tmYearToCalendar(newTime.Year)) + "-" + String(newTime.Month) + "-" + String(newTime.Day));
      debug_println("       From: " + String(tmYearToCalendar(rtc.getYear())) + "-" + String(rtc.getMonth()) + "-" + String(rtc.getDay()));
      rtc.setDate(newTime.Wday, newTime.Day, newTime.Month, newTime.Year);
    }
    newTime.dateUpdated = false;

    if (!rtc.isTimeSet())
    {
      debug_println("Set Time Failed");
    }
    writeEepromData();
    showTime(true);
}
//---------------------------------------------------------------
//Test if any buttons have been pressed
void testButtons(void)
{
  //Single press buttons
  if (clockButton->Pressed())
  {
    clockButtonPressed();
  }
  if (enterButton->Pressed())
  {
    enterButtonPressed();
  }

  //Don't need to check result of pressed since the button handler will invoke its repeat function
  upButton->Pressed();
  downButton->Pressed();
}

//---------------------------------------------------------------
//Handle CLOCK btton
void clockButtonPressed(void)
{
  bool menuEnd = false;
  if (cancelAlarm()) return;

  inSubMenu = false;
  if (clockMode == CLOCK) {
    clockMode = FIRST_CLOCK_MENU_MODE;//jump from clock to menu
  } else if (clockMode == DATE) {
    clockMode = CLOCK;//jump from date to clock
  } else if (clockMode == LAST_CLOCK_MENU_MODE) {
    clockMode = CLOCK;//jump from last menu back to clock
    menuEnd = true;
  } else {
    clockMode = (ClockButtonModesEnum)((int)clockMode + 1);//jump to next menu item
  }
  //update RTC only if we've just left the menu
  if (menuEnd)
  {
    updateRTCandSettings();
  }
  else
  {
    if (clockMode == FIRST_CLOCK_MENU_MODE)
    {
      rtc.getDate(&newTime.Wday, &newTime.Day, &newTime.Month, &newTime.Year);
      rtc.getTime(&newTime.Hour, &newTime.Minute, &newTime.Second, NULL, NULL);
      debug_println("Loading Time: " + String(newTime.Hour) + ":" + String(newTime.Minute));
    }
    showSetup(true);
  }
}

void clockButtonHeld(void)
{
  if (clockMode != CLOCK && clockMode != DATE) {
    clockMode = CLOCK;//jump from last menu back to clock
    updateRTCandSettings();
  }
}

//---------------------------------------------------------------
//Handle ENTER btton
void enterButtonPressed(void)
{
  if (cancelAlarm()) return;

  if (clockMode == CLOCK) {
    switchToDateScreen();
  } else if (clockMode == DATE) {
    clockMode = CLOCK;
  } else {
    if (!inSubMenu)
    {
      timeSetMode = TIME_HOUR;
      dateSetMode = DATE_YEAR;
      alarmSetMode = ALARM_HOUR;
      formatSetMode = DAY_MONTH;
      inSubMenu = true;
    }
    else
    {
      switch (clockMode)
      {
        case TIME_SET: timeSetMode = (timeSetMode == TIME_FORMAT) ? TIME_HOUR : (TimeSetMenuEnum)((int)timeSetMode + 1); break;
        case DATE_SET: dateSetMode = (dateSetMode == DATE_DAY) ? DATE_YEAR : (DateSetMenuEnum)((int)dateSetMode + 1); break;
        case ALARM_SET: alarmSetMode = (alarmSetMode == ALARM_STATE) ? ALARM_HOUR : (AlarmSetMenuEnum)((int)alarmSetMode + 1); break;
        case FORMAT_SET: formatSetMode = (formatSetMode == FONT_STYLE) ? DAY_MONTH : (FormatSetMenuEnum)((int)formatSetMode + 1); break;
        case BRIGHT_SET: brightSetMode = (brightSetMode == BRIGHT_AUTO) ? BRIGHT_VALUE : (BrightnessSetMenuEnum)((int)brightSetMode + 1); break;
        default: break;
      }
    }
    showSetup(true);
  }
}

//---------------------------------------------------------------
//Handle DOWN btton
void downButtonPressed(void)
{
  if (cancelAlarm()) return;

  switch (clockMode)
  {
    case TIME_SET:
      if (inSubMenu)
      {
        switch(timeSetMode)
        {
          case TIME_HOUR: newTime.Hour = (newTime.Hour + 24 - 1) % 24; newTime.timeUpdated = true;  break;
          case TIME_MIN: newTime.Minute = (newTime.Minute + 60 - 1) % 60; newTime.timeUpdated = true; break;
          case TIME_FORMAT: EepromData.format12hr = !EepromData.format12hr; break;
          default: break;
        }
        showSetup(true);
      }
      break;

    case DATE_SET:
      if (inSubMenu)
      {
        uint8_t md;
        switch(dateSetMode)
        {
          case DATE_YEAR: newTime.Year = (newTime.Year + MAX_YEAR - 1) % MAX_YEAR; newTime.dateUpdated = true; break;
          case DATE_MONTH:
            newTime.Month = ((newTime.Month - 1 + 12) - 1) % 12 + 1;
            md = daysInMonth(newTime.Year, newTime.Month);
            if (newTime.Day > md) { //limit day to possible days in selected month
              newTime.Day = md;
            }
            newTime.dateUpdated = true;
            break;
          case DATE_DAY:
            md = daysInMonth(newTime.Year, newTime.Month);
            newTime.Day = ((newTime.Day - 1 + md) - 1) % md + 1;
            newTime.dateUpdated = true;
            break;
            default: break;
        }
        showSetup(true);
      }
      break;

    case ALARM_SET:
      if (inSubMenu)
      {
        switch(alarmSetMode)
        {
          case ALARM_HOUR: EepromData.alarmHour = (EepromData.alarmHour + 24 - 1) % 24; break;
          case ALARM_MIN: EepromData.alarmMinute = (EepromData.alarmMinute + 60 - 1) % 60; break;
          case ALARM_STATE: EepromData.alarmEnable = !EepromData.alarmEnable; break;
          default: break;
        }
        showSetup(true);
      }
      break;

    case TUNE_SET:
      EepromData.tune = (EepromData.tune + NUM_OF_MELODIES - 1) % NUM_OF_MELODIES;
      showSetup(true);
      break;

    case BRIGHT_SET:
      if (inSubMenu)
      {
        switch (brightSetMode)
        {
          case BRIGHT_VALUE:
            EepromData.brightness = (EepromData.brightness + NUM_BRIGHTNESS_LEVELS_6932 - 1) % NUM_BRIGHTNESS_LEVELS_6932;
            if (!EepromData.autoBrightness) {
              setDisplayBrightness(EepromData.brightness);
            }
            break;
          case BRIGHT_AUTO:
            EepromData.autoBrightness = !EepromData.autoBrightness;
            if (!EepromData.autoBrightness) {
              setDisplayBrightness(EepromData.brightness);
            }
            break;
          default: break;
        }
      }
      showSetup(true);
      break;

    case FORMAT_SET:
      if (inSubMenu)
      {
        switch (formatSetMode)
        {
          case DAY_MONTH: EepromData.formatDmy = !EepromData.formatDmy; break;
          case FONT_STYLE: EepromData.squareFont = !EepromData.squareFont; break;
          default: break;
        }
      }
      showSetup(true);
      break;

    case TEMP_SET:
      EepromData.tempMode = (EepromData.tempMode + NUM_TEMP_MODES - 1) % NUM_TEMP_MODES;
      showSetup(true);
      break;

    case COLON_SET:
      EepromData.colonMode = (EepromData.colonMode + NUM_COLON_MODES - 1) % NUM_COLON_MODES;
      showSetup(true);
      break;

    case AMPMIND_SET:
      EepromData.amPmIndMode = (EepromData.amPmIndMode + NUM_AMPMIND_MODES - 1) % NUM_AMPMIND_MODES;
      showSetup(true);
      break;
    case AUTO_DATE_INTERVAL_SET:
      EepromData.auto_show_date_interval_index = (EepromData.auto_show_date_interval_index + NUM_AUTO_DATE_INTERVALS - 1) % NUM_AUTO_DATE_INTERVALS;
      showSetup(true);
      break;
    default: break;
  }
}

//---------------------------------------------------------------
//Handle UP btton
void upButtonPressed(void)
{
  if (cancelAlarm()) return;

  switch (clockMode)
  {
    case DATE:
      debugMode = !debugMode;
      break;
    case TIME_SET:
      if (inSubMenu)
      {
        switch(timeSetMode)
        {
          case TIME_HOUR: newTime.Hour = (newTime.Hour + 1) % 24; newTime.timeUpdated = true; break;
          case TIME_MIN: newTime.Minute = (newTime.Minute + 1) % 60; newTime.timeUpdated = true; break;
          case TIME_FORMAT: EepromData.format12hr = !EepromData.format12hr; break;
        }
        showSetup(true);
      }
      break;

    case DATE_SET:
      if (inSubMenu)
      {
        uint8_t md;
        switch(dateSetMode)
        {
          case DATE_YEAR: newTime.Year = (newTime.Year + 1) % MAX_YEAR; newTime.dateUpdated = true;  break;
          case DATE_MONTH:
            newTime.Month = ((newTime.Month - 1) + 1) % 12 + 1;
            md = daysInMonth(newTime.Year, newTime.Month);
            if (newTime.Day > md) { //limit day to possible days in selected month
              newTime.Day = md;
            }
            newTime.dateUpdated = true;
            break;
          case DATE_DAY:
            md = daysInMonth(newTime.Year, newTime.Month);
            newTime.Day = (newTime.Day % md) + 1;
            newTime.dateUpdated = true;
            break;
          default: break;
        }
        showSetup(true);
      }
      break;

    case ALARM_SET:
      if (inSubMenu)
      {
        switch(alarmSetMode)
        {
          case ALARM_HOUR: EepromData.alarmHour = (EepromData.alarmHour + 1) % 24; break;
          case ALARM_MIN: EepromData.alarmMinute = (EepromData.alarmMinute + 1) % 60; break;
          case ALARM_STATE: EepromData.alarmEnable = !EepromData.alarmEnable; break;
          default: break;
        }
        showSetup(true);
      }
      break;

    case TUNE_SET:
      EepromData.tune = (EepromData.tune + 1) % NUM_OF_MELODIES;
      showSetup(true);
      break;

    case BRIGHT_SET:
      if (inSubMenu)
      {
        switch (brightSetMode)
        {
          case BRIGHT_VALUE:
            EepromData.brightness = (EepromData.brightness + 1) % NUM_BRIGHTNESS_LEVELS_6932;
            if (!EepromData.autoBrightness) {
              setDisplayBrightness(EepromData.brightness);
            }
            break;
          case BRIGHT_AUTO:
            EepromData.autoBrightness = !EepromData.autoBrightness;
            if (!EepromData.autoBrightness) {
              setDisplayBrightness(EepromData.brightness);
            }
            break;
          default: break;
        }
      }
      showSetup(true);
      break;

    case FORMAT_SET:
      if (inSubMenu)
      {
        switch (formatSetMode)
        {
          case DAY_MONTH: EepromData.formatDmy = !EepromData.formatDmy; break;
          case FONT_STYLE: EepromData.squareFont = !EepromData.squareFont; break;
          default: break;
        }
      }
      showSetup(true);
      break;

    case TEMP_SET:
      EepromData.tempMode = (EepromData.tempMode + 1) % NUM_TEMP_MODES;
      showSetup(true);
      break;

    case COLON_SET:
      EepromData.colonMode = (EepromData.colonMode + 1) % NUM_COLON_MODES;
      showSetup(true);
      break;

    case AMPMIND_SET:
      EepromData.amPmIndMode = (EepromData.amPmIndMode + 1) % NUM_AMPMIND_MODES;
      showSetup(true);
      break;

    case AUTO_DATE_INTERVAL_SET:
      EepromData.auto_show_date_interval_index = (EepromData.auto_show_date_interval_index + 1) % NUM_AUTO_DATE_INTERVALS;
      showSetup(true);
      break;
    default: break;
  }
}

//---------------------------------------------------------------
//Turn off the alarm if it is playing a tune
bool cancelAlarm(void)
{
  if (musicPlaying)
  {
    musicPlaying = false;
    alarmCancelled = alarmRinging;
    return true;
  }
  yield();
  return false;
}

void switchToDateScreen(void) {
    clockMode = DATE;
    date_screen_entry_time = millis();
}

//---------------------------------------------------------------
//Show the Setup menu and flash current item selected
void showSetup(bool force)
{
  setupDisplayState = setupDisplayState | force;
  force = force || (millis() > setupTimeout);
  if (force)
  {
    setupTimeout = millis() + SETUP_FLASH_RATE;
    bool on = setupDisplayState;
    setupDisplayState = !setupDisplayState;

    clearDisplay();

    if (clockMode <= MENU_PAGE_1_END) { //Show page 1
      uint8_t hour;
      if (on || !(clockMode == TIME_SET && !inSubMenu)) displayString(0,7,"TIME");
      if (EepromData.format12hr) {
        if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_HOUR)) displayString(0,18,newTime.Hour >= 12 ? "P" : "A");
      }
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_HOUR)) displayNumber(0,13,hour_24_to_12(newTime.Hour, EepromData.format12hr),2,true);
                                                                                   displayString(0,15,".");
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_MIN)) displayNumber(0,15,newTime.Minute,2,true);
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_FORMAT)) displayString(0,20,(EepromData.format12hr) ? "12HR" : "24HR");
      // if (newTime.timeUpdated) writePhysicalDigit(0, 23, dp_____, false);//show indicator if update pending
      if (newTime.timeUpdated) displayString(0,0 ,"SET");//show indicator if update pending

      if (on || !(clockMode == DATE_SET && !inSubMenu)) displayString(1,7,"DATE");
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_YEAR)) displayNumber(1,13,tmYearToCalendar(newTime.Year),4,true);
                                                                                   displayString(1,17,"-");
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_MONTH)) displayNumber(1,18,newTime.Month,2,true);
                                                                                    displayString(1,20,"-");
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_DAY)) displayNumber(1,21,newTime.Day,2,true);
      //if (newTime.dateUpdated) writePhysicalDigit(1, 23, dp_____, false);//show indicator if update pending
      if (newTime.dateUpdated) displayString(1, 0,"SET");//show indicator if update pending

      if (on || !(clockMode == ALARM_SET && !inSubMenu)) displayString(2,6,"ALARM");
      if (EepromData.format12hr) {
        if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_HOUR)) displayString(2,18,EepromData.alarmHour >= 12 ? "P" : "A");
      }
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_HOUR)) displayNumber(2,13,hour_24_to_12(EepromData.alarmHour, EepromData.format12hr),2,true);
                                                                                       displayString(2,15,".");
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_MIN)) displayNumber(2,15,EepromData.alarmMinute,2,true);
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_STATE)) displayString(2,20,(EepromData.alarmEnable) ? "ON" : "OFF");

      if (on || !(clockMode == TUNE_SET && !inSubMenu)) displayString(3,7,"TUNE");
      if (on || !(clockMode == TUNE_SET && inSubMenu))
      {
        switch(EepromData.tune)
        {
          case 0: displayString(3,13,"ELISE"); break;
          case 1: displayString(3,13,"RANGE"); break;
          case 2: displayString(3,13,"SUNSHINE"); break;
          case 3: displayString(3,13,"BEEP 1"); break;
          case 4: displayString(3,13,"BEEP 2"); break;
          default: break;
        }
      }

      if (on || !(clockMode == BRIGHT_SET && !inSubMenu)) displayString(4,1,"BRIGHTNESS");
      if (on || !(clockMode == BRIGHT_SET && inSubMenu && brightSetMode == BRIGHT_VALUE)) displayNumber(4,13,EepromData.brightness,0,false);
      if (on || !(clockMode == BRIGHT_SET && inSubMenu && brightSetMode == BRIGHT_AUTO)) {
        displayString(4, 15, "AUTO");
        displayString(4, 20, (EepromData.autoBrightness) ? "ON" : "OFF");
      }

      if (on || !(clockMode == FORMAT_SET && !inSubMenu)) displayString(5,0,"DATE FORMAT");
      if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == DAY_MONTH)) displayString(5,13,(EepromData.formatDmy) ? "DD-MM" : "MM-DD");
      if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == FONT_STYLE)) displayString(5,19,(EepromData.squareFont) ? "FONT1" : "FONT2");

    } else {//show page 2 -------------------------------------------------------------------------------------------------------------------------------

      if (on || !(clockMode == TEMP_SET && !inSubMenu)) displayString(0,7,"TEMP");
      if (on || !(clockMode == TEMP_SET && inSubMenu)) {
        if (EepromData.tempMode < NUM_TEMP_MODES) {
          displayString(0, 13, tempModeStrings[EepromData.tempMode]);
          }
      }
      if (on || !(clockMode == COLON_SET && !inSubMenu)) displayString(1,6,"COLON");
      if (on || !(clockMode == COLON_SET && inSubMenu)) {
        if (EepromData.colonMode < NUM_COLON_MODES) {
          displayString(1, 13, colonModeStrings[EepromData.colonMode]);
          }
      }
      if (on || !(clockMode == AMPMIND_SET && !inSubMenu)) displayString(2,3,"12HR IND");
      if (on || !(clockMode == AMPMIND_SET && inSubMenu)) {
        if (EepromData.amPmIndMode < NUM_AMPMIND_MODES) {
          displayString(2, 13, ampmIndModeStrings[EepromData.amPmIndMode]);
          }
      }

      if (on || !(clockMode == AUTO_DATE_INTERVAL_SET && !inSubMenu)) displayString(3,2,"AUTO DATE");
      if (on || !(clockMode == AUTO_DATE_INTERVAL_SET && inSubMenu)) {
        if (EepromData.auto_show_date_interval_index < NUM_AUTO_DATE_INTERVALS) {
          //Show reasonable time units based on selected time interval
          uint16_t interval_sec = auto_date_intervals[EepromData.auto_show_date_interval_index];
          if (interval_sec == 0) {
            displayString(3, 13, "OFF");
          } else if (interval_sec >= 3600) {
            displayNumber(3, 13, interval_sec / 3600, 2, false);
            displayString(3, 16, "HR");
          } else if (interval_sec >= 60) {
            displayNumber(3, 13, interval_sec / 60, 2, false);
            displayString(3, 16, "MIN");
          } else {
            displayNumber(3, 13, interval_sec, 2, false);
            displayString(3, 16, "SEC");
          }
        }
      }
      displayString(5, 0, "V. " + String(BUILD_NUMBER) + " " + BUILD_DATE);
    }

    updateDisplay();
  }
}

uint8_t hour_24_to_12(uint8_t hour, bool format_12hr) {
    if (format_12hr) {
        if (hour > 12) {
          hour = hour - 12;
        } else if (hour == 0) {
          hour = 12;
        }
    }
    return hour;
}

//---------------------------------------------------------------
//Display the current time on the display if changed
//  force - always show time even if not changed
void showTime(bool force)
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  rtc.getTime(&hours, &minutes, &seconds, NULL, NULL);
  force = force || (lastSeconds != seconds);
  if (force)
  {
    lastSeconds = seconds;
    showTime(hours, minutes, true, true, (seconds & 0x01), (clockMode != CLOCK || !EepromData.format12hr));
  }
}

//---------------------------------------------------------------
//Display the time on the display
//  h - hour
//  m - minute
//  he - hour enable
//  me - minute enable
//  ce - colon enable
void showTime(int h, int m, bool he, bool me, bool ce, bool f24)
{
  clearDisplay();
  if (he)
  {
    if (!f24) {
      //display AM/PM Indicator if forced on, or requested on and not forced off
      if ((EepromData.amPmIndMode == AMPMIND_ON) || (ce && (EepromData.amPmIndMode != AMPMIND_OFF))) {
        if (h >= 12) {
          displayString(5, 11, "P");
        } else {
          displayString(5, 11, "A");
        }
      }
      h = hour_24_to_12(h, !f24);
    }
    if (h > 9 || f24)
    {
      displayLargeDigit(0, h / 10);
    }
    displayLargeDigit(5, h % 10);
  }
  if (me)
  {
    displayLargeDigit(13, m / 10);
    displayLargeDigit(18, m % 10);
  }
  //display colon if forced on, or requested on and not forced off
  if ((EepromData.colonMode == COLONMODE_ON) || (ce && (EepromData.colonMode != COLONMODE_OFF)))
  {
    displayLargeDigit(11, 10);
  }
  if (EepromData.tempMode == TEMPMODE_F) {
    displayNumber(0, 11, temp_sensor.readTemperatureF(), 3, false);
  } else if (EepromData.tempMode == TEMPMODE_C) {
    displayNumber(0, 11, temp_sensor.readTemperatureC(), 3, false);
  }

  //show dot in lower right corner if alarm is enabled
  if (EepromData.alarmEnable) {
    writePhysicalDigit(5, 23, dp_____, false);
  }

  updateDisplay();
}

//---------------------------------------------------------------

void showDate(int d, int m, int y)
{
  #define XOFS 3
  clearDisplay();
  if (EepromData.formatDmy)
  {
    displayDateDigit(XOFS + 0, d / 10);
    displayDateDigit(XOFS + 4, d % 10);
    displayDateDigit(XOFS + 8, 10);  //dash
    displayDateDigit(XOFS + 12, m / 10);
    displayDateDigit(XOFS + 16, m % 10);
  }
  else
  {
    displayDateDigit(XOFS + 0, m / 10);
    displayDateDigit(XOFS + 4, m % 10);
    displayDateDigit(XOFS + 8, 10);  //dash
    displayDateDigit(XOFS + 12, d / 10);
    displayDateDigit(XOFS + 16, d % 10);
  }
  displayNumber(5, 10, tmYearToCalendar(y), 0, false);

  if (debugMode) {
    displayNumber(5, 0, (int)temp_sensor.readTemperatureF(), 0, 0);
    if (light_sensor_valid) {
      displayNumber(5, 18, active_brightness_level, 0, false);
      displayString(5, 19, ".");
      displayNumber(5, 19, light_sensor_ch1, 5, false);
    } else {
      displayString(5, 19, "Error");
    }
  }

  updateDisplay();
}

//---------------------------------------------------------------
// Write a square or small digit based of configuration setting
// x = 0 to 23
// v = large or square digit to display (0..10)
void displayDateDigit(uint8_t x, uint8_t v)
{
  if (EepromData.squareFont)
  {
    displaySquareDigit(x, v);
  }
  else
  {
    displaySmallDigit(x, v);
  }
}

//---------------------------------------------------------------
// Write a segment bit mask
// x = 0 to 23
// v = large digit to display (0..10)
void displayLargeDigit(uint8_t x, uint8_t v)
{
  if (x < 24 && v < 11)
  {
    for (uint8_t row = 0; row < 6; row++)
    {
      for (uint8_t col = 0; col < 6; col++)
      {
        writePhysicalDigit(row, col + x, pgm_read_byte(&largeDigits[v][row][col]), false);
      }
    }
  }
}

//---------------------------------------------------------------
// Write a segment bit mask
// x = 0 to 23
// v = large digit to display (0..10)
void displaySmallDigit(uint8_t x, uint8_t v)
{
  if (x < 24 && v < 11)
  {
    for (uint8_t row = 0; row < 5; row++)
    {
      for (uint8_t col = 0; col < 3; col++)
      {
        writePhysicalDigit(row, col + x, pgm_read_byte(&smallDigits[v][row][col]), false);
      }
    }
  }
}

//#define PHYSICAL_DIGIT_MASK 0xFF //include decimal point
#define PHYSICAL_DIGIT_MASK 0x7F //exclude decimal point

//---------------------------------------------------------------
// Write a segment bit mask (Font 1)
// x = 0 to 23
// v = large digit to display (0..10)
void displaySquareDigit(uint8_t col, uint8_t v)
{
  //Segment order _ g f e d c b a
  uint8_t mask = ascii[v + ASCII_NUM_OFFSET];
  if (mask == B00000000)
  {
    //Hyphen
    mask = B01000000;
  }
  if (mask & B00000001)
  {
    //seg A
    writePhysicalDigit(0, col + 0, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(0, col + 1, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(0, col + 2, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B00000010)
  {
    //seg B
    writePhysicalDigit(0, col + 2, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(1, col + 2, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(2, col + 2, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B00000100)
  {
    //seg C
    writePhysicalDigit(2, col + 2, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(3, col + 2, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(4, col + 2, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B00001000)
  {
    //seg D
    writePhysicalDigit(4, col + 0, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(4, col + 1, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(4, col + 2, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B00010000)
  {
    //seg E
    writePhysicalDigit(2, col, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(3, col, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(4, col, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B00100000)
  {
    //seg F
    writePhysicalDigit(0, col, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(1, col, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(2, col, PHYSICAL_DIGIT_MASK, false);
  }
  if (mask & B01000000)
  {
    //seg G
    writePhysicalDigit(2, col + 0, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(2, col + 1, PHYSICAL_DIGIT_MASK, false);
    writePhysicalDigit(2, col + 2, PHYSICAL_DIGIT_MASK, false);
  }
}

//---------------------------------------------------------------
// Write string of characters
void displayString(uint8_t row, uint8_t col, String s)
{
  for (int i = 0; i < s.length(); i++)
  {
    byte c = (byte)s.charAt(i);
    byte cb = c;
    //characters outside of our supported range get replaced with spaces
    if (c < (byte)' ' || c > (byte)'`')
    {
      c = (byte)' ';
    }
    //offset the input character by the space character to get the index in our font array
    c = c - (byte)' ';

    if (cb == (byte)'.') {
      //special case period to avoid extra spaces
      //this logic definitely may be suboptimal under some circumstances,
      //but should work for things we care about right now
      if (col > 0) {
        col -= 1;//associate with previous digit
      }
      writePhysicalDigit(row, col, ascii[c], false);
    } else {
      writePhysicalDigit(row, col, ascii[c], true);
    }
      col = col + 1;
  }
}

//from https://stackoverflow.com/questions/25892665/performance-of-log10-function-returning-an-int
//this saves a fair amount of flash vs. using log10 (which uses floating point math)
unsigned int baseTwoDigits(unsigned int x) {
    return 32 - __builtin_clz(x);
}

static unsigned int baseTenDigits(unsigned int x) {
    static const unsigned char guess[33] = {
        0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
        6, 6, 6, 6, 7, 7, 7, 8, 8, 8,
        9, 9, 9
    };
    static const unsigned int tenToThe[] = {
        0, 10, 100, 1000, 10000, 100000,
        1000000, 10000000, 100000000, 1000000000,
    };
    unsigned int digits = guess[baseTwoDigits(x)];
    return digits + (x >= tenToThe[digits]);
}

//---------------------------------------------------------------
// Write a number
void displayNumber(uint8_t row, uint8_t col, int number, int padding, bool leadingZeros)
{
  if (padding == 0)
  {
    padding = (number > 0) ? baseTenDigits(number) : 1;
  }
  col = col + padding;
  bool first = true;
  for (int i = 0; i < padding; i++)
  {
    col--;
    if (number != 0 || first)
    {
      writePhysicalDigit(row, col, ascii[(number % 10) + ASCII_NUM_OFFSET], true);
      number = number / 10;
      first = false;
    }
    else
    {
      writePhysicalDigit(row, col, ascii[(leadingZeros) ? ASCII_NUM_OFFSET : 0], true);
    }
  }
}

//---------------------------------------------------------------
// Write a segment bit mask
// row = 0 to 5
// col = 0 to 23
// v = segment mask
// erase = true will replace all old pattern, false will OR with existing
void writePhysicalDigit(uint8_t row, uint8_t col, uint8_t v, bool erase)
{
  if (row < 6 && col < 24)
  {
    uint8_t dev = device_mapping[row][col];
    uint8_t dig = digit_mapping[row][col];

    if (!erase)
    {
      v = v | display_drivers[dev].get_digit(dig);
    }
    display_drivers[dev].set_digit(dig, v);
  }
}

//---------------------------------------------------------------
//Return the days in a given year and month
//Feb has 28 unless leap year or the turn of a century
uint8_t daysInMonth(int y, int m)
{
  return dom[m - 1] + ((m == 2 && (y % 4) == 0 && (y % 100) != 0) ? 1 : 0);
}

//---------------------------------------------------------------
//Write the EepromData structure to EEPROM
void writeEepromData()
{
  debug_println("Writing EEPROM data");
  //This function uses EEPROM.update() to perform the write, so does not rewrite the value if it didn't change.
  //We're a the FlashStorage library to emulate EEPROM, which uses a single page of flash for storage
  //Using the flash in this way is not really best practice, as no wear leveling is done
  //however, the GD32F103 is rated to 100,000 cycles, and the STM32F103 is rated to at least 10,000 cycles, so it seems
  //unlikely to be a problem with reasonable usage patterns in this system.
  EEPROM.setCommitASAP(false);
  EEPROM.put(EEPROM_ADDRESS, EepromData);
  EEPROM.commit();
}

//---------------------------------------------------------------
//Read the EepromData structure from EEPROM, initialise if necessary
//returns true if default settings were loaded
bool readEepromData(void)
{
  char settingsString[512] = {0};
  bool defaults_loaded = false;
  EEPROM.get(EEPROM_ADDRESS, EepromData);

  if (EepromData.magic != EEPROM_MAGIC)
  {
    loadDefaults();
    writeEepromData();
    defaults_loaded = true;
    debug_println("Defaults loaded");

  }

  //sanity check enumerated values
  if (EepromData.tempMode >= NUM_TEMP_MODES) {
    EepromData.tempMode = TEMPMODE_OFF;
  }
  if (EepromData.tune >= NUM_OF_MELODIES) {
    EepromData.tune = 0;
  }
  if (EepromData.colonMode >= NUM_COLON_MODES) {
    EepromData.colonMode = COLONMODE_FLASH;
  }
  if (EepromData.amPmIndMode >= NUM_AMPMIND_MODES) {
    EepromData.amPmIndMode = AMPMIND_FLASH;
  }
  makeSettingsString(settingsString, sizeof(settingsString) - 1);
  debug_println(settingsString);
  return defaults_loaded;
}

void makeSettingsString(char *str, size_t length) {
  snprintf(str, length, "magic=%08x\nalarm_en=%d\nalarm_time=%d:%d\n12h_format=%d\nauto_brightness=%d\nbrightness=%d\nformat=%d,%d\nauto=%d\n",
          EepromData.magic,
          EepromData.alarmEnable,
          EepromData.alarmHour,
          EepromData.alarmMinute,
          EepromData.format12hr,
          EepromData.autoBrightness,
          EepromData.brightness,
          EepromData.formatDmy,
          EepromData.squareFont,
          EepromData.auto_show_date_interval_index
          );
}
//---------------------------------------------------------------
//Play a melody
void playSong(const uint16_t* melody)
{
  //Play each note in the melody until the END_OF_TUNE note is encountered
  musicPlaying = true;
  int thisNote = 0;
  uint16_t noteRaw = pgm_read_word(&melody[thisNote++]);
  while (musicPlaying && noteRaw != END_OF_TUNE)
  {
    testButtons();
    yield();
    playNote(noteRaw);
    noteRaw = pgm_read_word(&melody[thisNote++]);
  } //while
  delay(50);
}

//---------------------------------------------------------------
//Play a single note
void playNote(uint16_t noteRaw)
{
  // to calculate the note duration, take one second divided by the note type.
  // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
  uint16_t frequency = noteRaw & 0x1FFF;
  uint16_t duration = (noteRaw & 0xE000) >> 13;
  if (duration == 7)
    duration = 8;
  uint16_t noteDuration = 1800 / duration;

  if (frequency != REST)
  {
    tone(SPEAKER, frequency, noteDuration);
  }

  // to distinguish the notes, set a minimum time between them.
  // the note's duration + 30% seems to work well:
  uint16_t pauseBetweenNotes = (noteDuration * 13) / 10;
  delay(pauseBetweenNotes);

  if (frequency != REST)
  {
    // stop the tone playing:
    noTone(SPEAKER);
  }
}

void clearDisplay(void) {
  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
    display_drivers[i].clear();
  }
}

void updateDisplay(void) {
  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
      display_drivers[i].update();
    }
}

void setDisplayBrightness(uint8_t dispBrightness) {
    active_brightness_level = dispBrightness;
    for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
      display_drivers[i].set_brightness(dispBrightness);
    }
}

uint16_t tmYearToCalendar(uint8_t year) {
  return (uint16_t) year + 2000;
}

uint8_t tmYearFromCalendar(uint16_t cYear) {
  return (uint8_t) (cYear - 2000);
}

uint32_t getSecondOfDay(void) {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  rtc.getTime(&hours, &minutes, &seconds, NULL);
  return ((uint32_t) hours) * 3600UL + ((uint32_t) minutes) * 60UL + ((uint32_t) seconds);
}

void readLightSensor(void) {
    long auto_brightness_value;
    bool ls_valid = false;
    byte ls_gain;
    bool ls_intrstatus;
    bool ls_newdata = false;
    unsigned int light_sensor_ch0_raw;
    unsigned int light_sensor_ch1_raw;
    static unsigned int prev_light_sensor0_value = 0;
    static unsigned int prev_light_sensor1_value = 0;
    light_sensor.getStatus(ls_valid, ls_gain, ls_intrstatus, ls_newdata);
    light_sensor_valid = light_sensor.getData(light_sensor_ch0_raw, light_sensor_ch1_raw);
    if (ls_newdata) { //sensor has new data
      //effectively an exponential average; last value is weighted 15x more heavily than the present value to slow down response time.
      //When the light sensor saturates (e.g. is exposed to direct sunlight), it may temporarily report 0; this averaging helps
      //to prevent that from showing up as blips in the display brightness
      light_sensor_ch0 = (light_sensor_ch0_raw + 15 * prev_light_sensor0_value) / 16;
      light_sensor_ch1 = (light_sensor_ch1_raw + 15 * prev_light_sensor1_value) / 16;
      prev_light_sensor0_value = light_sensor_ch0;
      prev_light_sensor1_value = light_sensor_ch1;
      // debug_println(String(ls_valid) + " " + String(ls_newdata) + " " + String(light_sensor_ch0) + " " + String(light_sensor_ch1));
      //map ambient light level to brightness values.  Channel 0 is the "mainly-visible-light" channel.
      //the full-scale value of 200 was chosen empirically.
      // auto_brightness_value = map(light_sensor_ch0, 0, 200, 0, MAX_BRIGHTNESS_LEVEL_6932);
      auto_brightness_value = map(light_sensor_ch0, 0, 200, 0, EepromData.brightness);//map to max brightness
      if (auto_brightness_value < 0 ) {
        auto_brightness_value = 0;
      // } else if (auto_brightness_value > MAX_BRIGHTNESS_LEVEL_6932) {
      } else if (auto_brightness_value > EepromData.brightness) {
        // auto_brightness_value = MAX_BRIGHTNESS_LEVEL_6932;
        auto_brightness_value = EepromData.brightness;
      }
      if (EepromData.autoBrightness) {
        active_brightness_level = auto_brightness_value;
      }
    }
    /* The 6932 LED drivers, at least as implemented on this hardware, seem to have a strange bug.
     * After running for a while (typically days-weeks), one or more of them will blank out.
     * Sending a brightness update restores them to functionality, so we do it on every
     * refresh cycle, regardless of whether or not brightness has changed, or auto-brightness
     * is even enabled. */
    setDisplayBrightness(active_brightness_level);
}

void loadDefaults(void) {
    debug_println("Loading default settings...");
    EepromData.magic = EEPROM_MAGIC;
    EepromData.alarmEnable = false;
    EepromData.alarmMinute = 30;
    EepromData.alarmHour = 5;
    EepromData.format12hr = false;
    EepromData.brightness = MAX_BRIGHTNESS_LEVEL_6932;
    EepromData.tune = 0;
    EepromData.formatDmy = false;
    EepromData.squareFont = false;
    EepromData.autoBrightness = true;
    EepromData.tempMode = TEMPMODE_OFF;
    EepromData.colonMode = COLONMODE_FLASH;
    EepromData.amPmIndMode = AMPMIND_FLASH;
    EepromData.auto_show_date_interval_index = 0;
}

static void cmd_print_help(char *args, char *response, size_t response_len) {
  const cli_command *current_command = &cli_commands[0];
  int len;
  while (current_command->cmd_name) {
    len = snprintf(response, response_len, "%-15s: %s\n", current_command->cmd_name, current_command->cmd_help);
    //decrement remaining bytes and move string pointer forward while building up response
    if (len >= 0) {
      if (len <= response_len) {
        response_len -= len;
        response += len;
      } else {
        //no more space left in response buffer
        response_len = 0;
        break;
      }
    }
    current_command += 1;
  }
}

static void cmd_print_version(char *args, char *response, size_t response_len) {
   snprintf(response, response_len, "Version %d, built on %s", BUILD_NUMBER, BUILD_DATE);
}
static void cmd_dump_config(char *args, char *response, size_t response_len) {
  makeSettingsString(response, response_len);
}

static void cmd_set_time(char *args, char *response, size_t response_len) {
  unsigned int hour;
  unsigned int minute;
  unsigned int second;
  int result;
  bool valid;
  result = sscanf(args, "%*s %u %u %u", &hour, &minute, &second);
  if (result == 3 &&
      hour < 24 &&
      minute < 60 &&
      second < 60) {
        valid = true;
      } else {
        valid = false;
      }
  if (valid) {
    snprintf(response, response_len, "Setting time to %02u:%02u:%02u", hour, minute, second);
    rtc.setTime(hour, minute, second, 0, rtc.AM);
  } else {
    strncpy(response, "Invalid argument", response_len);
  }
}

static void cmd_set_date(char *args, char *response, size_t response_len) {
  unsigned int year;
  unsigned int month;
  unsigned int day;
  int result;
  bool valid;
  result = sscanf(args, "%*s %u %u %u", &year, &month, &day);
  if (result == 3 &&
      year >= tmYearToCalendar(0) && year <= tmYearToCalendar(MAX_YEAR) &&
      month > 0 && month <= 12 &&
      day > 0 && day <= daysInMonth(year, month))
      {
        valid = true;
      } else {
        valid = false;
      }
  if (valid) {
    snprintf(response, response_len, "Setting date to %u-%02u-%02u", year, month, day);
    rtc.setDate(day, month, tmYearFromCalendar(year));
  } else {
    strncpy(response, "Invalid argument", response_len);
  }
}

static void cmd_fact_reset(char *args, char *response, size_t response_len) {
  loadDefaults();
  writeEepromData();
  NVIC_SystemReset();
}
static void cmd_handle_reboot(char *args, char *response, size_t response_len) {
  clearDisplay();
  NVIC_SystemReset();
}
static void cmd_handle_bootloader(char *args, char *response, size_t response_len) {
  //blank display before leaving
  clearDisplay();
  //Set flag in backup RAM so that the HID bootloader stops rather than continuing to main application
  setBackupRegister(LL_RTC_BKP_DR4, 0x424C);
  NVIC_SystemReset();
}

static void cmd_set_brightness(char *args, char *response, size_t response_len) {
  unsigned int brightness;
  unsigned int auto_brightness;
  int result;
  result = sscanf(args, "%*s %u %u", &brightness, &auto_brightness);
  if (result > 0 && result <= 2) {
    if (brightness > MAX_BRIGHTNESS_LEVEL_6932) {
      brightness = MAX_BRIGHTNESS_LEVEL_6932;
    }
    EepromData.brightness = brightness;
    //only set auto-brightness if second argument is provided
    if (result == 2) {
      EepromData.autoBrightness = (auto_brightness == 0 ? 0 : 1);
    }
    if (!EepromData.autoBrightness) {
      //force an update if auto-brightness is not enabled
        setDisplayBrightness(EepromData.brightness);
      }
    snprintf(response, response_len, "Setting brightness to %u (auto = %u)", brightness, EepromData.autoBrightness);
  } else {
    strncpy(response, "Invalid argument", response_len);
  }
}

static void cmd_set_alarm(char *args, char *response, size_t response_len) {
  unsigned int hour;
  unsigned int minute;
  unsigned int enable;
  int result;
  bool valid;
  result = sscanf(args, "%*s %u %u %u", &hour, &minute, &enable);
  if ((result == 2 || result == 3) &&
      hour < 24 &&
      minute < 60) {
        valid = true;
      } else {
        valid = false;
      }
  if (valid) {
    EepromData.alarmHour = hour;
    EepromData.alarmMinute = minute;
    if (result == 3) {
      EepromData.alarmEnable = enable == 0 ? 0 : 1;
    }
    snprintf(response, response_len, "Setting alarm to %02u:%02u (enabled = %u)", hour, minute, EepromData.alarmEnable);
  } else {
    strncpy(response, "Invalid argument", response_len);
  }
}

static void cmd_save_settings(char *args, char *response, size_t response_len) {
   writeEepromData();
   strncpy(response, "Settings saved", response_len);
}

static void cmd_show_date(char *args, char *response, size_t response_len) {
  switchToDateScreen();
}

static void cmd_get_date_time(char *args, char *response, size_t response_len) {
  uint8_t day;
  uint8_t month;
  uint8_t year;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  rtc.getDate(NULL, &day, &month, &year);
  rtc.getTime(&hour, &minute, &second, NULL, NULL);
  snprintf(response, response_len, "Current time is %02u:%02u:%02u\nCurrent date is %u-%02u-%02u\n",
          (unsigned int)hour, (unsigned int)minute, (unsigned int)second,
          (unsigned int)tmYearToCalendar(year), (unsigned int)month, (unsigned int)day);
}

static void cmd_set_time_format(char *args, char *response, size_t response_len) {
  unsigned int time_format;
  int result;
  result = sscanf(args, "%*s %u", &time_format);
  if (result == 1) {
    EepromData.format12hr = time_format == 0 ? 0 : 1;
    strncpy(response, "OK", response_len);
  } else {
    strncpy(response, "Invalid argument", response_len);
  }
}