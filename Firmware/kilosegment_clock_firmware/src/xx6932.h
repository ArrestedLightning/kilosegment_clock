#pragma once

#include "Arduino.h"
#include <SPI.h>

#define NUM_DIGITS_6932             16
#define NUM_BRIGHTNESS_LEVELS_6932  8
#define MAX_BRIGHTNESS_LEVEL_6932   (NUM_BRIGHTNESS_LEVELS_6932 - 1)

#define XX6932_CMD_ADDRESS(addr)       (0xC0 | (addr & 0xF)) //The 4-bit address within the XX6932 to write to
#define XX6932_CMD_BRIGHTNESS(b)       (0x88 | (b & 0x7))//The 3-bit brightness setting (note: non-linear)
#define XX6932_CMD_DISPLAY_OFF         (0x80)
#define XX6932_CMD_DISPLAY_MODE_NORMAL (0x40)



//mapping between brightness values and brightness levels
static const uint8_t brightness_levels_6932[] = { 1, 2, 4, 10, 11, 12, 13, 14 };

class xx6932
{
  public:
    //constructors
    xx6932(void);
    xx6932(uint32_t chip_select_pin);
    void set_chip_select_pin(uint32_t chip_select_pin);
    void set_spi_instance(SPIClass *spi);
    void set_spi_settings(SPISettings *spi_settings);
    void set_digit(int digit_num, uint8_t digit_value);
    uint8_t get_digit(int digit_num);
    void init(void);
    void clear(void);
    void update(void);
    void set_brightness(uint8_t brightness_value);

  private:
    void _send_command(uint8_t command);
    uint8_t _disp_buffer[NUM_DIGITS_6932];
    uint32_t _chip_select_pin;
    uint8_t _disp_brightness = MAX_BRIGHTNESS_LEVEL_6932;
    SPIClass *_spi;
    SPISettings *_spi_settings;
};