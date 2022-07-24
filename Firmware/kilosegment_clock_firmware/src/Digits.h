/*---------------------------------------------------
 * 7 Segment array clock
 * define each digit in a 6x6 byte array
 *--------------------------------------------------*/
 
#pragma once

//---------------------------------------------------
// Standard MAX7219 wiring
// Digit order 0,1,2,3,4,5

/*
// Segment order _ a b c d e f g
#define _______ B00000000
#define __cd___ B00011000
#define _bcde_g B00111101
#define abcdefg B01111111
#define __cdefg B00011111
#define __c____ B00010000
#define ab__efg B01100111
#define abcd_fg B01111011
#define abcde_g B01111101
#define ab_defg B01101111
#define _bc____ B00110000
#define a__defg B01001111
#define ____ef_ B00000110
#define ___defg B00001111
#define abc__fg B01110011
#define a___ef_ B01000110
#define _b_____ B00100000
#define ab___f_ B01100010
#define _bcd___ B00111000
#define a___ef_ B01000110
#define ab_____ B01100000
#define ____e__ B00000100
#define __cde_g B00011101
#define a____f_ B01000010
#define a_cdefg B01011111
#define ___de__ B00001100
#define _____f_ B00000010
#define ab___fg B01100011
#define a__def_ B01001110
#define __cde__ B00011100
#define a___efg B01000111

#define a__d___ B01001000
#define abc____ B01110000
#define _bc_ef_ B00110110
#define ___def_ B00001110
#define abc_ef_ B01110110
#define _bcdef_ B00111110
#define a__def_ B01001110
#define abcd___ B01111000


*/

//Segment order d e f b c a _ g
#define _______ B00000000
#define __cd___ B10001000
#define _bcde_g B11011001
#define abcdefg B11111101
#define __cdefg B11101001
#define __c____ B00001000
#define ab__efg B01110101
#define abcd_fg B10111101
#define abcde_g B11011101
#define ab_defg B11110101
#define _bc____ B00011000
#define a__defg B11100101
#define ____ef_ B01100000
#define ___defg B11100001
#define abc__fg B00111101
#define a___ef_ B01100100
#define _b_____ B00010000
#define ab___f_ B00110100
#define _bcd___ B10011000
#define a___ef_ B01100100
#define ab_____ B00010100
#define ____e__ B01000000
#define __cde_g B11001001
#define a____f_ B00100100
#define a_cdefg B11101101
#define ___de__ B11000000
#define _____f_ B00100000
#define ab___fg B00110101
#define a__def_ B11100100
#define __cde__ B11001000
#define a___efg B01100101

#define a__d___ B10000100
#define abc____ B00011100
#define _bc_ef_ B01111000
#define ___def_ B11100000
#define abc_ef_ B01111100
#define _bcdef_ B11111000
#define a__def_ B11100100
#define abcd___ B10011100


//Square Numbers

//ASCII Character Set
//Numbers 0 - 9
//Letters A - Z
//Segment order d e f b c a _ g
uint8_t ascii[] = {
    B11111100, B00011000, B11010101, B10011101, B00111001, B10101101, B11101101, B00011100, B11111101, B10111101, B00000000, B00000000, B00000000, B00000001, B00000000, B00000000,
    B00000000, B01111101, B11101001, B11100100, B11011001, B11100101, B01100101, B10111101, B01111001, B00011000, B11011000, B00000000, B11100000, B00000000, B01001001, B11001001,
    B01110101, B00000000, B01000001, B10101101, B11100001, B11001000, B00000000, B00000000, B00000000, B10111001, B00000000, B11001100, B01010001, B10011100, B00000000, B10000000
};

//Digit sequence for each device (MAX7219)
uint8_t digitMap[] = {5, 2, 6, 4, 1, 7, 3, 0};

//------------------------------------------------------------
//            Digits using logical coordinates
//------------------------------------------------------------

const int8_t largeDigits[11][6][6] PROGMEM = 
{
  { //0
    { _______, _______, __cd___, _bcde_g, abcdefg, __cdefg },
    { _______, __c____, abcdefg, ab__efg, abcd_fg, abcdefg },
    { _______, abcde_g, ab_defg, _______, _bc____, a__defg },
    { _______, abcdefg, ____ef_, __c____, abcdefg, ____ef_ },
    { _______, abcdefg, __cdefg, abcdefg, a___efg, _______ },
    { _______, abc__fg, abcdefg, ab__efg, _______, _______ }
  },
  { //1
    { _______, _______, _______, __c____, abcdefg, __cdefg },
    { _______, _______, _______, abcdefg, abcdefg, ____ef_ },
    { _______, _______, _bcde_g, abcdefg, a__defg, _______ },
    { _______, _bc____, abcdefg, abcdefg, ____ef_, _______ },
    { _______, abcdefg, abcdefg, a___ef_, _______, _______ },
    { _b_____, abcdefg, abcdefg, _______, _______, _______ }
  },
  { //2
    {  _______, _______, __cd___, _bcde_g, abcdefg, __cdefg },
    {  _______, _bc____, abcdefg, ab___f_, abcd_fg, abcdefg },
    {  _______, _______, _______, _bcd___, abcdefg, a___ef_ },
    {  _______, _______, _bcde_g, abcdefg, a___ef_, _______ },
    {  __c____, abcdefg, abcdefg, a___ef_, _______, _______ },
    {  _b_____, abc__fg, abcdefg, abcdefg, ab__efg, _______ }
  },
  { //3
    { _______, _______, __cd___, abcdefg, abcdefg, __cdefg },
    { _______, _b_____, abcdefg, ab___f_, abcd_fg, abcdefg },
    { _______, _______, __cd___, _bcde_g, abcdefg, a___ef_ },
    { _______, _______, ab_____, abc__fg, abcdefg, ____e__ },
    { _______, __cd___, __cde_g, _bcde_g, abcdefg, ____ef_ },
    { _b_____, abc__fg, abcdefg, abcdefg, a____f_, _______ }
  },
  { //4
    { _______, _______, _bcd___, abcdefg, __c____, abcdefg },
    { _______, __c____, abcdefg, a___ef_, abcde_g, a__defg },
    { _______, abcde_g, abcdefg, _bcd___, abcdefg, ____ef_ },
    { __c____, abcdefg, abcdefg, abcdefg, abcdefg, _______ },
    { _______, _______, __c____, abcdefg, ____ef_, _______ },
    { _______, _______, abcdefg, ab__efg, _______, _______ }
  },
  { //5
    { _______, _______, _bcde_g, abcdefg, abcdefg, abcdefg },
    { _______, _bc____, abcdefg, a_cdefg, ___de__, _______ },
    { _______, _______, abc__fg, abcdefg, abcdefg, ____ef_ },
    { _______, _______, _______, _bc____, abcdefg, ____ef_ },
    { _______, __cde_g, __cde_g, _bcde_g, abcdefg, _____f_ },
    { _b_____, abcdefg, abcdefg, ab__efg, a____f_, _______ }
  },
  { //6
    { _______, _______, _______, __cd___, abcdefg, ____ef_ },
    { _______, _______, _bcde_g, abcdefg, a____f_, _______ },
    { _______, _bcd___, abcdefg, abcdefg, abcdefg, ____e__ },
    { __c____, abcdefg, a___ef_, ab_____, abcdefg, ____ef_ },
    { _bc____, abcdefg, __cdefg, _bcde_g, abcdefg, _______ },
    { _______, abc__fg, abcdefg, ab__efg, _______, _______ }
  },
  { //7
    { _______, _bc____, abcdefg, abcdefg, abcdefg, __cdefg },
    { _______, _b_____, ab___fg, ab___fg, abcdefg, abcdefg },
    { _______, _______, _______, _bcde_g, abcdefg, a___ef_ },
    { _______, _______, _bcde_g, abcdefg, ab__efg, _______ },
    { _______, _bcde_g, abcdefg, a___ef_, _______, _______ },
    { _b_____, abcdefg, abcdefg, _______, _______, _______ }
  },
  { //8
    { _______, _______, __cd___, abcdefg, abcdefg, __cdefg },
    { _______, _bc____, abcdefg, ab___f_, abcd_fg, abcdefg },
    { _______, _b_____, abcdefg, _bcde_g, abcdefg, a___ef_ },
    { _______, _bcde_g, ab__efg, abc__fg, abcdefg, ____e__ },
    { _bc____, abcdefg, __cde_g, _bcde_g, abcdefg, ____ef_ },
    { _______, abc__fg, abcdefg, abcdefg, a____f_, _______ }
  },
  { //9
    { _______, _______, __cde_g, abcdefg, __cdefg, ___de__ },
    { _______, _bcd___, abcdefg, ab___fg, abcd_fg, abcdefg },
    { _______, abcdefg, a_cdefg, __cde__, abcdefg, a__defg },
    { _______, ab_____, abcd_fg, abcdefg, abcdefg, _____f_ },
    { _______, __cd___, abcdefg, ab__efg, _______, _______ },
    { _b_____, abcdefg, a___ef_, _______, _______, _______ }
  },
  { //Colon
    { _______, _______, _______, _______, _______, _______ },
    { _______, __cde_g, _______, _______, _______, _______ },
    { _______, _______, _______, _______, _______, _______ },
    { _______, _______, _______, _______, _______, _______ },
    { __cde_g, _______, _______, _______, _______, _______ },
    { _______, _______, _______, _______, _______, _______ }
  }
};

const int8_t smallDigits[11][5][3] PROGMEM = 
{
  { //0
    { a___ef_, a__d___, abc____ },
    { _bc_ef_, _______, _bc_ef_ },
    { _bc_ef_, _______, _bc_ef_ },
    { _bc_ef_, _______, _bc_ef_ },
    { ___def_, a__d___, _bcd___ }
  },
  { //1
    { _______, abc_ef_, _______ },
    { _______, _bc_ef_, _______ },
    { _______, _bc_ef_, _______ },
    { _______, _bc_ef_, _______ },
    { _______, _bcdef_, _______ }
  },
  { //2
    { a__def_, a__d___, abc____ },
    { _______, _______, _bc_ef_ },
    { a___ef_, a__d___, _bcd___ },
    { _bc_ef_, _______, _______ },
    { ___def_, a__d___, abcd___ }
  },
  { //3
    { a__def_, a__d___, abc____ },
    { _______, _______, _bc_ef_ },
    { _______, a__def_, _bc____ },
    { _______, _______, _bc_ef_ },
    { a__def_, a__d___, _bcd___ }
  },
  { //4
    { abc_ef_, _______, abc_ef_ },
    { _bc_ef_, _______, _bc_ef_ },
    { ___def_, a__d___, _bc____ },
    { _______, _______, _bc_ef_ },
    { _______, _______, _bcdef_ }
  },
  { //5
    { a___ef_, a__d___, abcd___ },
    { _bc_ef_, _______, _______ },
    { ___def_, a__d___, abc____ },
    { _______, _______, _bc_ef_ },
    { a__def_, a__d___, _bcd___ }
  },
  { //6
    { a___ef_, a__d___, abcd___ },
    { _bc_ef_, _______, _______ },
    { ____ef_, a__d___, abc____ },
    { _bc_ef_, _______, _bc_ef_ },
    { ___def_, a__d___, _bcd___ }
  },
  { //7
    { a__def_, a__d___, abc____ },
    { _______, _______, _bc_ef_ },
    { _______, _bc_ef_, _______ },
    { _______, _bc_ef_, _______ },
    { _______, _bcdef_, _______ },
  },
  { //8
    { a___ef_, a__d___, abc____ },
    { _bc_ef_, _______, _bc_ef_ },
    { ____ef_, a__d___, _bc____ },
    { _bc_ef_, _______, _bc_ef_ },
    { ___def_, a__d___, _bcd___ }
  },
  { //9
    { a___ef_, a__d___, abc____ },
    { _bc_ef_, _______, _bc_ef_ },
    { ___def_, a__d___, _bc____ },
    { _______, _______, _bc_ef_ },
    { a__def_, a__d___, _bcd___ }
  },
  { //Hyphen
    { _______, _______, _______ },
    { _______, _______, _______ },
    { a__def_, a__d___, abcd___ },
    { _______, _______, _______ },
    { _______, _______, _______ }
  }
};
