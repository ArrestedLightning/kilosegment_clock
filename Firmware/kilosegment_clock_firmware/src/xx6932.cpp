/* Driver for TM6932 and GN6932 LED drivers (possibly others as well) */

#include "xx6932.h"

xx6932::xx6932(void) {
    clear();
}

xx6932::xx6932(uint32_t chip_select_pin) {
    set_chip_select_pin(chip_select_pin);
    clear();
}

void xx6932::set_chip_select_pin(uint32_t chip_select_pin) {
    _chip_select_pin = chip_select_pin;
    pinMode(chip_select_pin, OUTPUT);
    digitalWrite(chip_select_pin, HIGH);
}

void xx6932::set_spi_instance(SPIClass *spi) {
    _spi = spi;
}

void xx6932::set_spi_settings(SPISettings *spi_settings) {
    _spi_settings = spi_settings;
}


void xx6932::set_digit(int digit_num, uint8_t digit_value) {
    if (digit_num < NUM_DIGITS_6932) {
        _disp_buffer[digit_num] = digit_value;
    }
}

uint8_t xx6932::get_digit(int digit_num) {
    if (digit_num < NUM_DIGITS_6932) {
        return _disp_buffer[digit_num];
    }
    return 0;
}

void xx6932::init(void) {
    _send_command(XX6932_CMD_DISPLAY_MODE_NORMAL);
}

void xx6932::clear(void) {
    memset(_disp_buffer, 0, sizeof(_disp_buffer));
}

void xx6932::update(void) {
    _spi->beginTransaction(*_spi_settings);
    digitalWrite(_chip_select_pin, LOW);
    _spi->transfer(XX6932_CMD_ADDRESS(0));
    //send data immediately without toggling chip select
    for (int i = 0; i < NUM_DIGITS_6932; i += 1) {
        _spi->transfer(_disp_buffer[i]);
    }
    digitalWrite(_chip_select_pin, HIGH);
    _spi->endTransaction();
}

void xx6932::set_brightness(uint8_t brightness_value) {
    if (brightness_value > MAX_BRIGHTNESS_LEVEL_6932) {
        brightness_value = MAX_BRIGHTNESS_LEVEL_6932;
    }
    _send_command(XX6932_CMD_BRIGHTNESS(brightness_value));
}

void xx6932::_send_command(uint8_t command) {
    _spi->beginTransaction(*_spi_settings);
    digitalWrite(_chip_select_pin, LOW);
    _spi->transfer(command);
    digitalWrite(_chip_select_pin, HIGH);
    _spi->endTransaction();
}