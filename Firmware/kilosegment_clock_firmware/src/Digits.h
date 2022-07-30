/*---------------------------------------------------
 * 7 Segment array clock
 * define each digit in a 6x6 byte array
 *--------------------------------------------------*/
 
#pragma once


// Segment order _ g f e d c b a
#define _______ B00000000
#define __cd___ B00001100
#define _bcde_g B01011110
#define abcdefg B01111111
#define __cdefg B01111100
#define __c____ B00000100
#define ab__efg B01110011
#define abcd_fg B01101111
#define abcde_g B01011111
#define ab_defg B01111011
#define _bc____ B00000110
#define a__defg B01111001
#define ____ef_ B00110000
#define ___defg B01111000
#define abc__fg B01100111
#define a___ef_ B00110001
#define _b_____ B00000010
#define ab___f_ B00100011
#define _bcd___ B00001110
#define a___ef_ B00110001
#define ab_____ B00000011
#define ____e__ B00010000
#define __cde_g B01011100
#define a____f_ B00100001
#define a_cdefg B01111101
#define ___de__ B00011000
#define _____f_ B00100000
#define ab___fg B01100011
#define a__def_ B00111001
#define __cde__ B00011100
#define a___efg B01110001
#define a__d___ B00001001
#define abc____ B00000111
#define _bc_ef_ B00110110
#define ___def_ B00111000
#define abc_ef_ B00110111
#define _bcdef_ B00111110
#define a__def_ B00111001
#define abcd___ B00001111


//Square Numbers

#define ASCII_NUM_OFFSET 16 //location of 0 in the ascii array
//ASCII Character Set
//Numbers 0 - 9
//Letters A - Z
//Segment order _ g f e d c b a
const uint8_t ascii[] = {
    B00000000, //Space
    B10000010, //!
    B00100010, //"
    B00000000, //#
    B01101101, //$
    B00000000, //%
    B00000000, //&
    B00000010, //'
    B00111001, //(
    B00001111, //)
    B00000000, //*
    B00000000, //+
    B10000000, //,
    B01000000, //-
    B01000000, //.
    B01010010, ///
    B00111111, //0
    B00000110, //1
    B01011011, //2
    B01001111, //3
    B01100110, //4
    B01101101, //5
    B01111101, //6
    B00000111, //7
    B01111111, //8
    B01101111, //9
    B00000000, //:
    B00000000, //;
    B01011000, //<
    B01001000, //=
    B01001100, //>
    B11010011, //?
    B01111011, //@
    B01110111, //A
    B01111100, //B
    B00111001, //C
    B01011110, //D
    B01111001, //E
    B01110001, //F
    B01101111, //G
    B01110110, //H
    B00000110, //I
    B00011110, //J
    B01110110, //K
    B00111000, //L
    B01010100, //M
    B01010100, //N
    B01011100, //O
    B01110011, //P
    B01100111, //Q
    B01010000, //R
    B01101101, //S
    B01111000, //T
    B00011100, //U
    B00111110, //V
    B00011100, //W
    B01110110, //X
    B01101110, //Y
    B01011011, //Z
    B00111001, //[
    B01100100, // \ (backslash)
    B00001111, //]
    B00100011, //^
    B00001000, //_
    B00100000  //`
};

//------------------------------------------------------------
//            Digits using logical coordinates
//------------------------------------------------------------

const uint8_t largeDigits[11][6][6] = 
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

const uint8_t smallDigits[11][5][3] = 
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
