/*
Class: Button
Author: John Bradnam (jbrad2089@gmail.com)
Purpose: Arduino library to handle buttons
Modified by ArrestedLightning to handle "button held"
*/
#include "Button.h"

Button::Button(int pin)
{
  _init(pin, pin, false, 0, 0, true, DEFAULT_REPEAT_START_SPEED);
}

Button::Button(int name, int pin)
{
  _init(name, pin, false, 0, 0, true, DEFAULT_REPEAT_START_SPEED);
}

Button::Button(int name, int pin, unsigned long repeat_time)
{
  _init(name, pin, false, 0, 0, true, repeat_time);
}

Button::Button(int name, int pin, unsigned long repeat_time, bool activeLow)
{
  _init(name, pin, false, 0, 0, activeLow, repeat_time);
}

Button::Button(int name, int pin, int analogLow, int analogHigh, bool activeLow)
{
  _init(name, pin, true, analogLow, analogHigh, activeLow, DEFAULT_REPEAT_START_SPEED);
}


void Button::_init(int name, int pin, bool range, int analogLow, int analogHigh, bool activeLow, unsigned long repeat_speed) {
  _name = name;
  _pin = pin;
  _range = range;
  _low = analogLow;
  _high = analogHigh;
  _activeLow = activeLow;
  _heldTriggered = false;
  _backgroundCallback = NULL;
  _repeatCallback = NULL;
  _heldCallback = NULL;
  _repeat_speed = repeat_speed;
  if (!range) {
    //set pull resistors if button is in digital mode
    if (activeLow) {
      pinMode(_pin, INPUT_PULLUP);
    } else {
      pinMode(_pin, INPUT_PULLDOWN);
    }
  } else {
    //assume we will be reading the pin as an analog input
    pinMode(_pin, INPUT);
  }
}

//Set function to invoke in a delay or repeat loop
void Button::Background(void (*pBackgroundFunction)())
{
  _backgroundCallback = pBackgroundFunction;
}

//Set function to invoke if repeat system required
void Button::Repeat(void (*pRepeatFunction)())
{
  _repeatCallback = pRepeatFunction;
}

void Button::Held(void (*pHeldFunction)())
{
  _heldCallback = pHeldFunction;
}

  bool Button::IsDown()
{
	if (_range)
	{
		int value = analogRead(_pin);
		return (value >= _low && value < _high);
	}
	else
	{
		return (digitalRead(_pin) == LOW);
	}
}

//Tests if a button is pressed and released
//  returns true if the button was pressed and released
//	if repeat callback supplied, the callback is called while the key is pressed
bool Button::Pressed()
{
  bool pressed = false;
  if (IsDown())
  {
    unsigned long wait = millis() + DEBOUNCE_DELAY;
    while (millis() < wait)
    {
      if (_backgroundCallback != NULL)
      {
        _backgroundCallback();
      }
    }
    if (IsDown())
    {
  	  //Set up for repeat loop
  	  if (_repeatCallback != NULL)
  	  {
  	    _repeatCallback();
  	  }
  	  unsigned long speed = _repeat_speed;
  	  unsigned long time = millis() + speed;
      while (IsDown())
      {
        if (_backgroundCallback != NULL)
        {
          _backgroundCallback();
        }
        if (millis() >= time) {
          if (_heldCallback != NULL && !_heldTriggered)
          {
            _heldTriggered = true;
            _heldCallback();

          }
          if (_repeatCallback != NULL)
          {
            _repeatCallback();
            unsigned long faster = speed - REPEAT_INCREASE_SPEED;
            if (faster >= REPEAT_MAX_SPEED)
            {
              speed = faster;
            }
            time = millis() + speed;
          }
        }
      }
      //don't report pressed if the button was held
      if (!_heldTriggered) {
        pressed = true;
      }
    }
  }
  else //button is not pressed
  {
    //reset held down flag if button has been released
    _heldTriggered = false;
  }
  return pressed;
}

//Return current button state
int Button::State()
{
	if (_range)
	{
		int value = analogRead(_pin);
		if (_activeLow)
		{
			return (value >= _low && value < _high) ? LOW : HIGH;
		}
		else
		{
			return (value >= _low && value < _high) ? HIGH : LOW;
		}
	}
	else
	{
		return digitalRead(_pin);
	}
}

//Return current button name
int Button::Name()
{
	return _name;
}
