#pragma once
#include <Arduino.h>


void testButtons(void);
void clockButtonPressed(void);
void enterButtonPressed(void);
void downButtonPressed(void);
void upButtonPressed(void);
bool cancelAlarm(void);
void showSetup(bool force);
void showTime(bool force);
void showTime(int h, int m, bool he, bool me, bool ce, bool f24);
void showDate(int d, int m, int y);
void displayDateDigit(uint8_t x, uint8_t v);
void displayLargeDigit(uint8_t x, uint8_t v);
void displaySmallDigit(uint8_t x, uint8_t v);
void displaySquareDigit(uint8_t col, uint8_t v);
void displayString(uint8_t row, uint8_t col, String s);
void displayNumber(uint8_t row, uint8_t col, int number, int padding, bool leadingZeros);
void writePhysicalDigit(uint8_t row, uint8_t col, uint8_t v, bool erase);
uint8_t daysInMonth(int y, int m);
void writeEepromData(void);
void readEepromData(void);
void playSong(const uint16_t* melody);
void playNote(uint16_t noteRaw);
void clearDisplay(void);
void updateDisplay(void);
void setDisplayBrightness(uint8_t dispBrightness);
uint16_t tmYearToCalendar(uint8_t year);
uint8_t tmYearFromCalendar(uint16_t cYear);
void readLightSensor(void);
