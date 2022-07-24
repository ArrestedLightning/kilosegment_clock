/*
Class: Button
Author: John Bradnam (jbrad2089@gmail.com)
Purpose: Arduino library to handle buttons
*/
#include "Button.h"

Button::Button(int pin)
{
  _name = pin;
  _pin = pin;
  _range = false;
  _low = 0;
  _high = 0;
  _backgroundCallback = NULL;
  _repeatCallback = NULL;
  pinMode(_pin, INPUT_PULLUP);
}

Button::Button(int name, int pin)
{
  _name = name;
  _pin = pin;
  _range = false;
  _low = 0;
  _high = 0;
  _backgroundCallback = NULL;
  _repeatCallback = NULL;
  pinMode(_pin, INPUT_PULLUP);
}

Button::Button(int name, int pin, int analogLow, int analogHigh, bool activeLow)
{
  _name = name;
  _pin = pin;
  _range = true;
  _low = analogLow;
  _high = analogHigh;
  _activeLow = activeLow;
  _backgroundCallback = NULL;
  _repeatCallback = NULL;
  pinMode(_pin, INPUT);
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
  	  unsigned long speed = REPEAT_START_SPEED;
  	  unsigned long time = millis() + speed;
      while (IsDown())
      {
        if (_backgroundCallback != NULL)
        {
          _backgroundCallback();
        }
    		if (_repeatCallback != NULL && millis() >= time)
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
      pressed = true;
    }
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
