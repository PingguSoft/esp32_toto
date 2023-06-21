/*
  AudioOutputI2S
  Base class for an I2S output port
  
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

#pragma once

#include "AudioOutputI2S.h"

#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>
#include <I2S.h>
#endif

class AudioInOutI2S : public AudioOutputI2S
{
  public:
    AudioInOutI2S(int port=0, int output_mode=EXTERNAL_I2S, int input_mode=EXTERNAL_I2S, int dma_buf_count = 8, int use_apll=APLL_DISABLE);
    enum : int { INTERNAL_ADC = 1 };
    bool SetPinout(int bclkPin, int wclkPin, int doutPin, int dinPin);
    virtual ~AudioInOutI2S() override;

    bool begin(bool txDAC);
    int read(int16_t *samples, int count);

  protected:
    bool SetPinout();
    int input_mode;
    uint8_t dinPin;

    int32_t *raw_samples;
};
