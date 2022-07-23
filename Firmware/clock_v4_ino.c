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
 * - Added Alarm function with selectable alarm usic
 * - Added Brightness setting
 * - Added Setup screen to set time, date, alarm, music and brightness
 * - Added Date screen (displays for two seconds when ENTER button is pressed
 * Update 2020-06-16
 * - Added date format option on configuration screen
 * - Increased date screen timeout from 2 to 5 seconds
 * - Added a configuarble choice of font for date display
 *--------------------------------------------------*/
 
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <TimeLib.h>
#include <DS1302RTC.h>
#include <EEPROM.h>
#include "button.h"
#include "Tunes.h"
#include "Digits.h"

#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define MAX_DEVICES 18

#define LED_CLK   13  // or SCK  (WHI)
#define LED_DATA  11  // or MOSI (BRN)
#define LED_CS    10  // or SS   (YEL)

#define SPEAKER   2
#define SW_UP     3
#define SW_DOWN   4
#define RTC_CLK   5
#define RTC_IO    6
#define SW_ENTER  7
#define RTC_CE    8
#define SW_SELECT 9

DS1302RTC rtc(RTC_CE, RTC_IO, RTC_CLK);

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, LED_CS, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, LED_DATA, LED_CLK, LED_CS, MAX_DEVICES);

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
  bool formatDmy;
  bool squareFont;
} EEPROM_DATA;

EEPROM_DATA EepromData;       //Current EEPROM settings

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
#define SETUP_FLASH_RATE 200;
unsigned long setupTimeout;
bool setupDisplayState = false;


enum ClockButtonModesEnum { CLOCK, TIME_SET, DATE_SET, ALARM_SET, TUNE_SET, BRIGHT_SET, FORMAT_SET };
ClockButtonModesEnum clockMode = CLOCK;

enum TimeSetMenuEnum { TIME_HOUR, TIME_MIN, TIME_FORMAT };
TimeSetMenuEnum timeSetMode = TIME_HOUR;

enum DateSetMenuEnum { DATE_YEAR, DATE_MONTH, DATE_DAY };
DateSetMenuEnum dateSetMode = DATE_YEAR;

enum AlarmSetMenuEnum { ALARM_HOUR, ALARM_MIN, ALARM_STATE };
AlarmSetMenuEnum alarmSetMode = ALARM_HOUR;

enum FormatSetMenuEnum { DAY_MONTH, FONT_STYLE };
FormatSetMenuEnum formatSetMode = DAY_MONTH;

int lastSeconds = -1;
bool alarmRinging = false;    //true when alarm is on
bool alarmCancelled = false;  //alarm cancelled by user
bool musicPlaying = false;    //true if playing a song
bool clockColon = false;      //show/hide colon

int8_t dom[] = {31,28,31,30,31,30,31,31,30,31,30,31};
tmElements_t newTime;           //Used to store new time

void setup()
{
  Serial.begin(115200);

  //Eprom
  readEepromData();

  //Initialise buttons
  clockButton = new Button(SW_SELECT);
  enterButton = new Button(SW_ENTER);
  downButton = new Button(SW_DOWN);
  downButton->Repeat(downButtonPressed);
  upButton = new Button(SW_UP);
  upButton->Repeat(upButtonPressed);

  //Initialise sound
  pinMode(SPEAKER,OUTPUT);

  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, EepromData.brightness); //0..MAX_INTENSITY
  mx.update(MD_MAX72XX::OFF) ;       //No Auto updating

  setSyncProvider(rtc.get);          // the function to get the time from the RTC
  if(timeStatus() != timeSet)
  {
    Serial.println("RTC Sync Bad");
    newTime.Year = CalendarYrToTm(2020);
    newTime.Month = 5;
    newTime.Day = 30;
    newTime.Hour = 14;
    newTime.Minute = 53;
    newTime.Second = 0;
    time_t t = makeTime(newTime);
    setTime(t);
    rtc.set(t);
    
    if (rtc.set(t) != 0) 
    {
      Serial.println("Set Time Failed");
    }
  }
  newTime.Year = CalendarYrToTm(year());
  newTime.Month = month();
  newTime.Day = day();
  newTime.Hour = hour();
  newTime.Minute = minute();
  newTime.Second = second();
  Serial.println("Time: " + String(newTime.Hour) + ":" + String(newTime.Minute) + ":" + String(newTime.Second));
  Serial.println("Date: " + String(newTime.Day) + "/" + String(newTime.Month) + "/" + String(tmYearToCalendar(newTime.Year)));

  clockMode = CLOCK;
  showTime(true);
}

void loop()
{
  testButtons();

  if (clockMode == CLOCK)
  {
    showTime(false);
    if (EepromData.alarm && EepromData.hours == hour() && EepromData.minutes == minute())
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
}

//---------------------------------------------------------------
//Test if any buttons have been pressed
void testButtons()
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
void clockButtonPressed()
{
  if (cancelAlarm()) return;

  inSubMenu = false;
  clockMode = (clockMode == FORMAT_SET) ? CLOCK : (ClockButtonModesEnum)((int)clockMode + 1);
  if (clockMode == CLOCK)
  {
    Serial.println("Saving Time: " + String(newTime.Hour) + ":" + String(newTime.Minute));
    Serial.println("       From: " + String(hour()) + ":" + String(minute()));
    if (newTime.Year != CalendarYrToTm(year()) || newTime.Month != month() || newTime.Day != day() || newTime.Hour != hour() || newTime.Minute != minute())
    {
      //Update time
      Serial.println("Updating RTC");
      newTime.Second = second();
      time_t t = makeTime(newTime);
      setTime(t);
      rtc.set(t);
      if (rtc.set(t) != 0) 
      {
        Serial.println("Set Time Failed");
      }
    }
    writeEepromData(); 
    showTime(true);
  }
  else
  {
    if (clockMode == TIME_SET)
    {
      newTime.Year = CalendarYrToTm(year());
      newTime.Month = month();
      newTime.Day = day();
      newTime.Hour = hour();
      newTime.Minute = minute();
      newTime.Second = second();
      Serial.println("Loading Time: " + String(hour()) + ":" + String(minute()));
    }
    showSetup(true);
  }
}

//---------------------------------------------------------------
//Handle ENTER btton
void enterButtonPressed()
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
    showDate(day(), month(), year());
    delay(5000);
  }
}

//---------------------------------------------------------------
//Handle DOWN btton
void downButtonPressed()
{
  if (cancelAlarm()) return;

  switch (clockMode)
  {
    case TIME_SET:
      if (inSubMenu)
      {
        switch(timeSetMode)
        {
          case TIME_HOUR: newTime.Hour = (newTime.Hour + 24 - 1) % 24; break;
          case TIME_MIN: newTime.Minute = (newTime.Minute + 60 - 1) % 60; break;
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
          case DATE_YEAR: newTime.Year = ((newTime.Year - 30 + 100) - 1) % 100 + 30; break;
          case DATE_MONTH: newTime.Month = ((newTime.Month - 1 + 12) - 1) % 12 + 1; break;
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
      EepromData.brightness = (EepromData.brightness + MAX_INTENSITY - 1) % MAX_INTENSITY; 
      mx.control(MD_MAX72XX::INTENSITY, EepromData.brightness); //0..MAX_INTENSITY
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
     
  }
}

//---------------------------------------------------------------
//Handle UP btton
void upButtonPressed()
{
  if (cancelAlarm()) return;

  switch (clockMode)
  {
    case TIME_SET:
      if (inSubMenu)
      {
        switch(timeSetMode)
        {
          case TIME_HOUR: newTime.Hour = (newTime.Hour + 1) % 24; break;
          case TIME_MIN: newTime.Minute = (newTime.Minute + 1) % 60; break;
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
          case DATE_YEAR: newTime.Year = ((newTime.Year - 30) + 1) % 100 + 30; break;
          case DATE_MONTH: newTime.Month = ((newTime.Month - 1) + 1) % 12 + 1; break;
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
      EepromData.brightness = (EepromData.brightness + 1) % MAX_INTENSITY; 
      mx.control(MD_MAX72XX::INTENSITY, EepromData.brightness); //0..MAX_INTENSITY
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
     
  }
}

//---------------------------------------------------------------
//Turn off the alarm if it is playing a tune
bool cancelAlarm()
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
    
    mx.clear();
    
    if (on || !(clockMode == TIME_SET && !inSubMenu)) displayString(0,7,"TINE");
    if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_HOUR)) displayNumber(0,13,newTime.Hour,2,true);
    if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_MIN)) displayNumber(0,16,newTime.Minute,2,true);
    if (on || !(clockMode == TIME_SET && inSubMenu && timeSetMode == TIME_FORMAT)) displayString(0,19,(EepromData.format12hr) ? "12HR" : "24HR");

    if (on || !(clockMode == DATE_SET && !inSubMenu)) displayString(1,7,"DATE");
    if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_YEAR)) displayNumber(1,13,tmYearToCalendar(newTime.Year),4,true);
    if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_MONTH)) displayNumber(1,18,newTime.Month,2,true);
    if (on || !(clockMode == DATE_SET && inSubMenu && dateSetMode == DATE_DAY)) displayNumber(1,21,newTime.Day,2,true);
    
    if (on || !(clockMode == ALARM_SET && !inSubMenu)) displayString(2,6,"ALARN");
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

    if (on || !(clockMode == FORMAT_SET && !inSubMenu)) displayString(5,0,"DATE FORNAT");
    if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == DAY_MONTH)) displayString(5,13,(EepromData.formatDmy) ? "DD-NN" : "NN-DD");
    if (on || !(clockMode == FORMAT_SET && inSubMenu && formatSetMode == FONT_STYLE)) displayString(5,19,(EepromData.squareFont) ? "FONT1" : "FONT2");
    
    mx.update();
  }
}

//---------------------------------------------------------------
//Display the current time on the display if changed
//  force - always show time even if not changed
void showTime(bool force)
{
  force = force || (lastSeconds != second());
  if (force) 
  {
    lastSeconds = second();
    showTime(hour(), minute(), true, true, (second() & 0x01), (clockMode != CLOCK || !EepromData.format12hr));
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
  mx.clear();
  if (he)
  {
    if (!f24 && h > 12)
    {
      h = h - 12;
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
  mx.update();
}

//---------------------------------------------------------------

void showDate(int d, int m, int y)
{
  #define XOFS 3
  mx.clear();
  if (EepromData.formatDmy)
  {
    displayDateDigit(XOFS+0, d / 10);
    displayDateDigit(XOFS+4, d % 10);
    displayDateDigit(XOFS+8, 10);  //colon
    displayDateDigit(XOFS+12, m / 10);
    displayDateDigit(XOFS+16, m % 10);
  }
  else
  {
    displayDateDigit(XOFS+0, m / 10);
    displayDateDigit(XOFS+4, m % 10);
    displayDateDigit(XOFS+8, 10);  //colon
    displayDateDigit(XOFS+12, d / 10);
    displayDateDigit(XOFS+16, d % 10);
  }
  displayNumber(5, 10, y, 0, false);
  mx.update();
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
  //Segment order d e f b c a _ g
  uint8_t mask = ascii[v];
  if (mask == B00000000)
  {
    //Hyphen
    mask = B00000001;
  }
  if (mask & B00000100)
  {
    //seg A
    writePhysicalDigit(0, col + 0, 0xff, false);
    writePhysicalDigit(0, col + 1, 0xff, false);
    writePhysicalDigit(0, col + 2, 0xff, false);
  }
  if (mask & B00010000)
  {
    //seg B
    writePhysicalDigit(0, col + 2, 0xff, false);
    writePhysicalDigit(1, col + 2, 0xff, false);
    writePhysicalDigit(2, col + 2, 0xff, false);
  }
  if (mask & B00001000)
  {
    //seg C
    writePhysicalDigit(2, col + 2, 0xff, false);
    writePhysicalDigit(3, col + 2, 0xff, false);
    writePhysicalDigit(4, col + 2, 0xff, false);
  }
  if (mask & B10000000)
  {
    //seg D
    writePhysicalDigit(4, col + 0, 0xff, false);
    writePhysicalDigit(4, col + 1, 0xff, false);
    writePhysicalDigit(4, col + 2, 0xff, false);
  }
  if (mask & B01000000)
  {
    //seg C
    writePhysicalDigit(2, col, 0xff, false);
    writePhysicalDigit(3, col, 0xff, false);
    writePhysicalDigit(4, col, 0xff, false);
  }
  if (mask & B00100000)
  {
    //seg E
    writePhysicalDigit(0, col, 0xff, false);
    writePhysicalDigit(1, col, 0xff, false);
    writePhysicalDigit(2, col, 0xff, false);
  }
  if (mask & B00000001)
  {
    //seg D
    writePhysicalDigit(2, col + 0, 0xff, false);
    writePhysicalDigit(2, col + 1, 0xff, false);
    writePhysicalDigit(2, col + 2, 0xff, false);
  }
}

//---------------------------------------------------------------
// Write string of characters
uint8_t displayString(uint8_t row, uint8_t col, String s)
{
  for (int i = 0; i < s.length(); i++)
  {
    byte c = (byte)s.charAt(i);
    if (c == 0x2D)
    {
      c = 0x3D;
    }
    if (c < 0x30 || c > 0x5F)
    {
      c = 0x3F;   //Used as a space character
    }
    c = c - 0x30;
    writePhysicalDigit(row, col, ascii[c], true);
    col = col + 1;
  }
}

//---------------------------------------------------------------
// Write a number
uint8_t displayNumber(uint8_t row, uint8_t col, int number, int padding, bool leadingZeros)
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
      writePhysicalDigit(row, col, ascii[number % 10], true);
      number = number / 10;
      first = false;
    }
    else
    {
      writePhysicalDigit(row, col, ascii[(leadingZeros) ? 0 : 0x0F], true);
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
    uint8_t dig = (col & 0x03) + ((row & 0x01) ? 4 : 0);
    uint8_t dev = (col >> 2) * 3 + (row >> 1);
    uint16_t c = ((uint16_t)dev << 3) | digitMap[dig];
    if (!erase)
    {
      v = v | mx.getColumn(c); 
    }
    mx.setColumn(c, v);
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
  //This function uses EEPROM.update() to perform the write, so does not rewrites the value if it didn't change.
  EEPROM.put(EEPROM_ADDRESS,EepromData);
}

//---------------------------------------------------------------
//Read the EepromData structure from EEPROM, initialise if necessary
void readEepromData()
{
  //Eprom
  EEPROM.get(EEPROM_ADDRESS,EepromData);
  //Serial.println("magic:" + String(EepromData.magic, 16) + ", alarm: " + String(EepromData.alarm) + ", time: " + String(EepromData.hours) + ":" + String(EepromData.minutes) + ", 12hr: " +  String(EepromData.format12hr) + ", brightness: " +  String(EepromData.brightness));
  if (EepromData.magic != EEPROM_MAGIC)
  {
    Serial.println("Initialising EEPROM ...");
    EepromData.magic = EEPROM_MAGIC;
    EepromData.alarm = false;
    EepromData.minutes = 30;
    EepromData.hours = 5;
    EepromData.format12hr = false;
    EepromData.brightness = 8;
    EepromData.tune = 0;
    EepromData.formatDmy = false;
    writeEepromData();
  }
  Serial.println("alarm: " + String(EepromData.alarm) + ", time: " + String(EepromData.hours) + ":" + String(EepromData.minutes) + ", 12hr: " +  String(EepromData.format12hr) + ", brightness: " +  String(EepromData.brightness));
}

//---------------------------------------------------------------
//Play a melody
int playSong(const uint16_t* melody)
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
