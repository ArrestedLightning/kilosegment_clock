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
 *--------------------------------------------------*/

#include <SPI.h>
#include <STM32RTC.h>
#include <EEPROM.h>
#include <LTR303.h>
#include <Temperature_LM75_Derived.h>
#include <HardwareSerial.h>
#include "Button.h"
#include "Tunes.h"
#include "Digits.h"
#include "mapping.h"
#include "xx6932.h"
#include "kilosegment_clock_firmware.h"
#include "build_info.h"

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

#define debug_init()        Serial1.begin(115200)
#define debug_println(...)  Serial1.println(__VA_ARGS__)

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

//EEPROM handling
#define EEPROM_ADDRESS 0
#define EEPROM_MAGIC 0x0BAD0DAD
typedef struct {
  uint32_t magic;
  bool alarm;
  uint8_t minutes;
  uint8_t hours;
  bool format12hr;
  uint8_t brightness;
  uint8_t tune;
  uint8_t tempMode;
  bool formatDmy;
  bool squareFont;
} EEPROM_DATA;

EEPROM_DATA EepromData;       //Current EEPROM settings

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
#define SETUP_FLASH_RATE 200;
unsigned long setupTimeout;
bool setupDisplayState = false;


enum ClockButtonModesEnum { CLOCK, TIME_SET, DATE_SET, ALARM_SET, TUNE_SET, BRIGHT_SET, FORMAT_SET, TEMP_SET };
ClockButtonModesEnum clockMode = CLOCK;
#define LAST_CLOCK_MODE TEMP_SET

enum TimeSetMenuEnum { TIME_HOUR, TIME_MIN, TIME_FORMAT };
TimeSetMenuEnum timeSetMode = TIME_HOUR;

enum DateSetMenuEnum { DATE_YEAR, DATE_MONTH, DATE_DAY };
DateSetMenuEnum dateSetMode = DATE_YEAR;

enum AlarmSetMenuEnum { ALARM_HOUR, ALARM_MIN, ALARM_STATE };
AlarmSetMenuEnum alarmSetMode = ALARM_HOUR;

enum FormatSetMenuEnum { DAY_MONTH, FONT_STYLE };
FormatSetMenuEnum formatSetMode = DAY_MONTH;

#define TEMPMODE_OFF 0
#define TEMPMODE_F   1
#define TEMPMODE_C   2

int lastSeconds = -1;
bool alarmRinging = false;    //true when alarm is on
bool alarmCancelled = false;  //alarm cancelled by user
bool musicPlaying = false;    //true if playing a song
bool clockColon = false;      //show/hide colon

static const String tempModeStrings[] = {"OFF", "F", "C"};
#define NUM_TEMP_MODES 3

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

void setup()
{
  debug_init();

  //Eprom
  readEepromData();

  //Initialize buttons
  clockButton = new Button(SW_CLOCK);
  enterButton = new Button(SW_ENTER);
  downButton = new Button(SW_DOWN);
  downButton->Repeat(downButtonPressed);
  upButton = new Button(SW_UP);
  upButton->Repeat(upButtonPressed);

  //Initialize sound
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);

  SPI_2.begin();
  // SPI_2.beginTransaction(spi_settings);
  // SPI_2.endTransaction();

  //Initialize display drivers
  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
    display_drivers[i].set_spi_instance(&SPI_2);
    display_drivers[i].set_chip_select_pin(led_chip_select_pins[i]);
    display_drivers[i].set_spi_settings(&spi_settings);
  }

  for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
    display_drivers[i].init();
    display_drivers[i].set_brightness(EepromData.brightness);
  }

  Wire.begin();
  temp_sensor.disableShutdownMode();
  light_sensor.begin();


  pinMode(HB_LED, OUTPUT);
  digitalWrite(HB_LED, LOW);

  //make sure we're using the 32KHz external crystal if we want remotely accurate timekeeping
  rtc.setClockSource(STM32RTC::LSE_CLOCK);
  rtc.begin();

  if(!rtc.isTimeSet())
  {
    debug_println("RTC not set");
    RTC_SetTime(0,0,0,0,HOUR_AM);
    RTC_SetDate(tmYearFromCalendar(2022), 1, 1, RTC_WEEKDAY_SATURDAY);
  }
  rtc.getDate(&newTime.Wday, &newTime.Day, &newTime.Month, &newTime.Year);
  rtc.getTime(&newTime.Hour, &newTime.Minute, &newTime.Second, NULL, NULL);
  debug_println("Time: " + String(rtc.getHours()) + ":" + String(rtc.getMinutes()) + ":" + String(rtc.getSeconds()));
  debug_println("Date: " + String(rtc.getDay()) + "/" + String(rtc.getMonth()) + "/" + String(rtc.getYear()));
  newTime.timeUpdated = false;
  newTime.dateUpdated = false;

  clockMode = CLOCK;
  showTime(true);
}

void loop()
{
  testButtons();

  if (clockMode == CLOCK)
  {
    showTime(false);
    if (EepromData.alarm && EepromData.hours == rtc.getHours() && EepromData.minutes == rtc.getMinutes())
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

  }
  else
  {
    showSetup(false);
  }

  delay(100);
  digitalWrite(HB_LED, !digitalRead(HB_LED));
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
  if (cancelAlarm()) return;

  inSubMenu = false;
  clockMode = (clockMode == LAST_CLOCK_MODE) ? CLOCK : (ClockButtonModesEnum)((int)clockMode + 1);
  if (clockMode == CLOCK)
  {
    debug_println("Saving Time: " + String(newTime.Hour) + ":" + String(newTime.Minute));
    debug_println("       From: " + String(rtc.getHours()) + ":" + String(rtc.getMinutes()));
    if (newTime.Year != rtc.getYear() || newTime.Month != rtc.getMonth() || newTime.Day != rtc.getDay() || newTime.Hour != rtc.getHours() || newTime.Minute != rtc.getMinutes())
    {
      //Update time
      debug_println("Updating RTC");
      //reset seconds when updating time so as to allow for synchronization with an external clock,
      //but only if time was actually updated by the user while in the settings menu.  This avoids accumulating
      //offset while modifying other settings.
      if (newTime.timeUpdated) {
        newTime.Second = 0;
        rtc.setTime(newTime.Hour, newTime.Minute, newTime.Second, 0, rtc.AM);
      }
      newTime.timeUpdated = false;
      if (newTime.dateUpdated) {
        rtc.setDate(newTime.Wday, newTime.Day, newTime.Month, newTime.Year);
      }
      newTime.dateUpdated = false;
      if (!rtc.isTimeSet())
      {
        debug_println("Set Time Failed");
      }
    }
    writeEepromData();
    showTime(true);
  }
  else
  {
    if (clockMode == TIME_SET)
    {
      rtc.getDate(&newTime.Wday, &newTime.Day, &newTime.Month, &newTime.Year);
      rtc.getTime(&newTime.Hour, &newTime.Minute, &newTime.Second, NULL, NULL);
      debug_println("Loading Time: " + String(newTime.Hour) + ":" + String(newTime.Minute));
    }
    showSetup(true);
  }
}

//---------------------------------------------------------------
//Handle ENTER btton
void enterButtonPressed(void)
{
  if (cancelAlarm()) return;

  if (clockMode != CLOCK)
  {
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
      }
    }
    showSetup(true);
  }
  else
  {
    showDate(rtc.getDay(), rtc.getMonth(), rtc.getYear());
    delay(5000);
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
        }
        showSetup(true);
      }
      break;

    case DATE_SET:
      if (inSubMenu)
      {
        switch(dateSetMode)
        {
          case DATE_YEAR: newTime.Year = ((newTime.Year - 30 + 100) - 1) % 100 + 30; newTime.dateUpdated = true; break;
          case DATE_MONTH: newTime.Month = ((newTime.Month - 1 + 12) - 1) % 12 + 1; newTime.dateUpdated = true; break;
          case DATE_DAY:
            uint8_t md = daysInMonth(newTime.Year, newTime.Month);
            newTime.Day = ((newTime.Day - 1 + md) - 1) % md + 1;
            break;
        }
        showSetup(true);
      }
      break;

    case ALARM_SET:
      if (inSubMenu)
      {
        switch(alarmSetMode)
        {
          case ALARM_HOUR: EepromData.hours = (EepromData.hours + 24 - 1) % 24; break;
          case ALARM_MIN: EepromData.minutes = (EepromData.minutes + 60 - 1) % 60; break;
          case ALARM_STATE: EepromData.alarm = !EepromData.alarm; break;
        }
        showSetup(true);
      }
      break;

    case TUNE_SET:
      EepromData.tune = (EepromData.tune + NUM_OF_MELODIES - 1) % NUM_OF_MELODIES;
      showSetup(true);
      break;

    case BRIGHT_SET:
      EepromData.brightness = (EepromData.brightness + NUM_BRIGHTNESS_LEVELS_6932 - 1) % NUM_BRIGHTNESS_LEVELS_6932;
      setDisplayBrightness(EepromData.brightness);
      showSetup(true);
      break;

    case FORMAT_SET:
      if (inSubMenu)
      {
        switch (formatSetMode)
        {
          case DAY_MONTH: EepromData.formatDmy = !EepromData.formatDmy; break;
          case FONT_STYLE: EepromData.squareFont = !EepromData.squareFont; break;
        }
      }
      showSetup(true);
      break;
    case TEMP_SET:
      EepromData.tempMode = (EepromData.tempMode + NUM_TEMP_MODES - 1) % NUM_TEMP_MODES;
      showSetup(true);
      break;
  }
}

//---------------------------------------------------------------
//Handle UP btton
void upButtonPressed(void)
{
  if (cancelAlarm()) return;

  switch (clockMode)
  {
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
        switch(dateSetMode)
        {
          case DATE_YEAR: newTime.Year = ((newTime.Year - 30) + 1) % 100 + 30; newTime.dateUpdated = true;  break;
          case DATE_MONTH: newTime.Month = ((newTime.Month - 1) + 1) % 12 + 1; newTime.dateUpdated = true; break;
          case DATE_DAY:
            uint8_t md = daysInMonth(newTime.Year, newTime.Month);
            newTime.Day = (newTime.Day % md) + 1;
            break;
        }
        showSetup(true);
      }
      break;

    case ALARM_SET:
      if (inSubMenu)
      {
        switch(alarmSetMode)
        {
          case ALARM_HOUR: EepromData.hours = (EepromData.hours + 1) % 24; break;
          case ALARM_MIN: EepromData.minutes = (EepromData.minutes + 1) % 60; break;
          case ALARM_STATE: EepromData.alarm = !EepromData.alarm; break;
        }
        showSetup(true);
      }
      break;

    case TUNE_SET:
      EepromData.tune = (EepromData.tune + 1) % NUM_OF_MELODIES;
      showSetup(true);
      break;

    case BRIGHT_SET:
      EepromData.brightness = (EepromData.brightness + 1) % NUM_BRIGHTNESS_LEVELS_6932;
      setDisplayBrightness(EepromData.brightness);
      showSetup(true);
      break;

    case FORMAT_SET:
      if (inSubMenu)
      {
        switch (formatSetMode)
        {
          case DAY_MONTH: EepromData.formatDmy = !EepromData.formatDmy; break;
          case FONT_STYLE: EepromData.squareFont = !EepromData.squareFont; break;
        }
      }
      showSetup(true);
      break;
    case TEMP_SET:
      EepromData.tempMode = (EepromData.tempMode + 1) % NUM_TEMP_MODES;
      showSetup(true);
      break;
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

    if (clockMode < TEMP_SET) { //Show page 1
      if (on || !(clockMode == TIME_SET && !inSubMenu)) displayString(0,7,"TIME");
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_HOUR)) displayNumber(0,13,newTime.Hour,2,true);
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_MIN)) displayNumber(0,16,newTime.Minute,2,true);
      if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_FORMAT)) displayString(0,19,(EepromData.format12hr) ? "12HR" : "24HR");

      if (on || !(clockMode == DATE_SET && !inSubMenu)) displayString(1,7,"DATE");
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_YEAR)) displayNumber(1,13,tmYearToCalendar(newTime.Year),4,true);
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_MONTH)) displayNumber(1,18,newTime.Month,2,true);
      if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_DAY)) displayNumber(1,21,newTime.Day,2,true);

      if (on || !(clockMode == ALARM_SET && !inSubMenu)) displayString(2,6,"ALARM");
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_HOUR)) displayNumber(2,13,EepromData.hours,2,true);
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_MIN)) displayNumber(2,16,EepromData.minutes,2,true);
      if (on || !(clockMode == ALARM_SET && inSubMenu && alarmSetMode == ALARM_STATE)) displayString(2,19,(EepromData.alarm) ? "ON" : "OFF");

      if (on || !(clockMode == TUNE_SET && !inSubMenu)) displayString(3,7,"TUNE");
      if (on || !(clockMode == TUNE_SET && inSubMenu))
      {
        switch(EepromData.tune)
        {
          case 0: displayString(3,13,"ELISE"); break;
          case 1: displayString(3,13,"RANGE"); break;
          case 2: displayString(3,13,"SUNSHINE"); break;
        }
      }

      if (on || !(clockMode == BRIGHT_SET && !inSubMenu)) displayString(4,1,"BRIGHTNESS");
      if (on || !(clockMode == BRIGHT_SET && inSubMenu)) displayNumber(4,13,EepromData.brightness,0,false);

      if (on || !(clockMode == FORMAT_SET && !inSubMenu)) displayString(5,0,"DATE FORMAT");
      if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == DAY_MONTH)) displayString(5,13,(EepromData.formatDmy) ? "DD-NN" : "NN-DD");
      if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == FONT_STYLE)) displayString(5,19,(EepromData.squareFont) ? "FONT1" : "FONT2");
    } else {//show page 2
      if (on || !(clockMode == TEMP_SET && !inSubMenu)) displayString(0,7,"TEMP");
      if (on || !(clockMode == TEMP_SET && inSubMenu)) {
        if (EepromData.tempMode < NUM_TEMP_MODES) {
          displayString(0,13,tempModeStrings[EepromData.tempMode]);
          }
      }
      displayString(4, 4, "VERSION");
      displayNumber(4, 13, BUILD_NUMBER, 0, 0);
      displayString(5, 3, BUILD_DATE);
    }

    updateDisplay();
  }
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
      if (ce) {
        if (h >= 12) {
          displayString(5, 11, "P");
        } else {
          displayString(5, 11, "A");
        }
      }
      if (h > 12)
      {
        h = h - 12;
      } else if (h == 0) {
        h = 12;
      }
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
  if (ce)
  {
    displayLargeDigit(11, 10);
  }
  if (EepromData.tempMode == TEMPMODE_F) {
    displayNumber(0, 11, temp_sensor.readTemperatureF(), 3, false);
  } else if (EepromData.tempMode == TEMPMODE_C) {
    displayNumber(0, 11, temp_sensor.readTemperatureC(), 3, false);
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
    displayDateDigit(XOFS + 8, 10);  //colon
    displayDateDigit(XOFS + 12, m / 10);
    displayDateDigit(XOFS + 16, m % 10);
  }
  else
  {
    displayDateDigit(XOFS + 0, m / 10);
    displayDateDigit(XOFS + 4, m % 10);
    displayDateDigit(XOFS + 8, 10);  //colon
    displayDateDigit(XOFS + 12, d / 10);
    displayDateDigit(XOFS + 16, d % 10);
  }
  displayNumber(5, 10, tmYearToCalendar(y), 0, false);
  displayNumber(5, 0, (int)temp_sensor.readTemperatureF(), 0, 0);
  // displayNumber(5, 18, (int)lig)
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

//---------------------------------------------------------------
// Write a segment bit mask
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
    writePhysicalDigit(0, col + 0, 0xff, false);
    writePhysicalDigit(0, col + 1, 0xff, false);
    writePhysicalDigit(0, col + 2, 0xff, false);
  }
  if (mask & B00000010)
  {
    //seg B
    writePhysicalDigit(0, col + 2, 0xff, false);
    writePhysicalDigit(1, col + 2, 0xff, false);
    writePhysicalDigit(2, col + 2, 0xff, false);
  }
  if (mask & B00000100)
  {
    //seg C
    writePhysicalDigit(2, col + 2, 0xff, false);
    writePhysicalDigit(3, col + 2, 0xff, false);
    writePhysicalDigit(4, col + 2, 0xff, false);
  }
  if (mask & B00001000)
  {
    //seg D
    writePhysicalDigit(4, col + 0, 0xff, false);
    writePhysicalDigit(4, col + 1, 0xff, false);
    writePhysicalDigit(4, col + 2, 0xff, false);
  }
  if (mask & B00010000)
  {
    //seg E
    writePhysicalDigit(2, col, 0xff, false);
    writePhysicalDigit(3, col, 0xff, false);
    writePhysicalDigit(4, col, 0xff, false);
  }
  if (mask & B00100000)
  {
    //seg F
    writePhysicalDigit(0, col, 0xff, false);
    writePhysicalDigit(1, col, 0xff, false);
    writePhysicalDigit(2, col, 0xff, false);
  }
  if (mask & B01000000)
  {
    //seg G
    writePhysicalDigit(2, col + 0, 0xff, false);
    writePhysicalDigit(2, col + 1, 0xff, false);
    writePhysicalDigit(2, col + 2, 0xff, false);
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
    if (c < (byte)' ' || c > (byte)'`')
    {
      c = (byte)' ';
    }
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

//---------------------------------------------------------------
// Write a number
void displayNumber(uint8_t row, uint8_t col, int number, int padding, bool leadingZeros)
{
  if (padding == 0)
  {
    padding = (number > 0) ? floor(log10(number)) + 1 : 1;
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
  //This function uses EEPROM.update() to perform the write, so does not rewrites the value if it didn't change.
  debug_println("magic:" + String(EepromData.magic, 16) + ", alarm: " + String(EepromData.alarm) + ", time: " + String(EepromData.hours) + ":" + String(EepromData.minutes) + ", 12hr: " +  String(EepromData.format12hr) + ", brightness: " +  String(EepromData.brightness));
  // EEPROM.put(EEPROM_ADDRESS, EepromData);
}

//---------------------------------------------------------------
//Read the EepromData structure from EEPROM, initialise if necessary
void readEepromData(void)
{
  //Eprom
  // EEPROM.get(EEPROM_ADDRESS,EepromData);
  debug_println("magic:" + String(EepromData.magic, 16) + ", alarm: " + String(EepromData.alarm) + ", time: " + String(EepromData.hours) + ":" + String(EepromData.minutes) + ", 12hr: " +  String(EepromData.format12hr) + ", brightness: " +  String(EepromData.brightness));
  if (EepromData.magic != EEPROM_MAGIC)
  {
    debug_println("Initializing EEPROM ...");
    EepromData.magic = EEPROM_MAGIC;
    EepromData.alarm = false;
    EepromData.minutes = 30;
    EepromData.hours = 5;
    EepromData.format12hr = false;
    EepromData.brightness = MAX_BRIGHTNESS_LEVEL_6932;
    EepromData.tune = 0;
    EepromData.formatDmy = false;
    EepromData.squareFont = false;
    writeEepromData();
  }
  debug_println("alarm: " + String(EepromData.alarm) + ", time: " + String(EepromData.hours) + ":" + String(EepromData.minutes) + ", 12hr: " +  String(EepromData.format12hr) + ", brightness: " +  String(EepromData.brightness));
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
    for (int i = 0; i < NUM_DISPLAY_DRIVERS; i += 1) {
      display_drivers[i].set_brightness(dispBrightness);
    }
}

uint16_t tmYearToCalendar(uint8_t year) {
  return (uint16_t) year + 2001;
}

uint8_t tmYearFromCalendar(uint16_t cYear) {
  return (uint8_t) (cYear - 2001);
}