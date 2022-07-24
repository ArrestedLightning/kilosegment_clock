/*
Class: Button
Author: John Bradnam (jbrad2089@gmail.com)
Purpose: Arduino library to handle buttons
*/
#pragma once
#include "Arduino.h"

#define DEBOUNCE_DELAY 10

//Repeat speed
#define REPEAT_START_SPEED 500
#define REPEAT_INCREASE_SPEED 50
#define REPEAT_MAX_SPEED 50

class Button
{
	public:
	//Simple constructor
	Button(int pin);
	Button(int name, int pin);
	Button(int name, int pin, int analogLow, int analogHigh, bool activeLow = true);

  //Background function called when in a wait or repeat loop
  void Background(void (*pBackgroundFunction)());
	//Repeat function called when button is pressed
  void Repeat(void (*pRepeatFunction)());
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
		int _name;
		int _pin;
		bool _range;
		int _low;
		int _high;
		bool _activeLow;
		void (*_repeatCallback)(void);
		void (*_backgroundCallback)(void);
};
