/*
Class: Button
Author: John Bradnam (jbrad2089@gmail.com)
Purpose: Arduino library to handle buttons
*/
#pragma once
#include "Arduino.h"

#define DEBOUNCE_DELAY 10

//Repeat speed
#define DEFAULT_REPEAT_START_SPEED 500
#define REPEAT_INCREASE_SPEED 50
#define REPEAT_MAX_SPEED 50

class Button
{
  public:
    //Simple constructor
    Button(int pin);
    Button(int name, int pin);
    Button(int name, int pin, unsigned long repeat_time);
    Button(int name, int pin, unsigned long repeat_time, bool activeLow);
    Button(int name, int pin, int analogLow, int analogHigh, bool activeLow = true);

  //Background function called when in a wait or repeat loop
  void Background(void (*pBackgroundFunction)());
  //Repeat function called when button is pressed
  void Repeat(void (*pRepeatFunction)());
  //Held function called once when button is held down
  void Held(void (*pHeldFunction)());
  //Test if button is pressed
  bool IsDown(void);
  //Test whether button is pressed and released
  //Will call repeat function if one is provided
  bool Pressed();
  //Return button state (HIGH or LOW) - LOW = Pressed
  int State();
  //Return button name
  int Name();

  private:
    void _init(int name, int pin, bool range, int analogLow, int analogHigh, bool activeLow, unsigned long repeat_speed);
    int _name;
    int _pin;
    int _low;
    int _high;
    unsigned long _repeat_speed;
    bool _range;
    bool _activeLow;
    bool _heldTriggered;
    void (*_repeatCallback)(void);
    void (*_backgroundCallback)(void);
    void (*_heldCallback)(void);
};
