/*
  AudioInOutI2S
  Base class for I2S interface port

  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#ifdef ESP32
#include "driver/i2s.h"
#elif defined(ARDUINO_ARCH_RP2040) || ARDUINO_ESP8266_MAJOR >= 3
#include <I2S.h>
#elif ARDUINO_ESP8266_MAJOR < 3
#include <i2s.h>
#endif
#include "AudioInOutI2S.h"

#if defined(ESP32) || defined(ESP8266)
AudioInOutI2S::AudioInOutI2S(int port, int output_mode, int input_mode, int dma_buf_count, int use_apll) : AudioOutputI2S(port, output_mode, dma_buf_count, use_apll) {
    this->input_mode = input_mode;
}
#elif defined(ARDUINO_ARCH_RP2040)

#endif

AudioInOutI2S::~AudioInOutI2S() {
}

bool AudioInOutI2S::SetPinout() {
#ifdef ESP32
    if (input_mode == INTERNAL_ADC)
        ;

    i2s_pin_config_t pins = {
        .mck_io_num = 0,  // Unused
        .bck_io_num = bclkPin,
        .ws_io_num = wclkPin,
        .data_out_num = doutPin,
        .data_in_num = dinPin};
    i2s_set_pin((i2s_port_t)portNo, &pins);
    return true;
#else
    (void)bclkPin;
    (void)wclkPin;
    (void)doutPin;
    return false;
#endif
}

bool AudioInOutI2S::SetPinout(int bclk, int wclk, int dout, int din) {
    bclkPin = bclk;
    wclkPin = wclk;
    doutPin = dout;
    dinPin = din;
    if (i2sOn)
        return SetPinout();

    return true;
}

bool AudioInOutI2S::begin(bool txDAC) {
    bool ret = AudioOutputI2S::begin(txDAC);

    return ret;
}

int AudioInOutI2S::read(int16_t *samples, int count) {
    // read from i2s

    size_t bytes_read = 0;

    i2s_read((i2s_port_t)portNo, raw_samples, sizeof(int32_t) * count, &bytes_read, portMAX_DELAY);
    int samples_read = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples_read; i++) {
        samples[i] = (raw_samples[i] & 0xFFFFFFF0) >> 11;
    }
    free(raw_samples);
    return samples_read;
}
