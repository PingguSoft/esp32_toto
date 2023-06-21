/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 see <http://www.gnu.org/licenses/>
*/

#ifndef _UTILS_H_
#define _UTILS_H_
#include <Arduino.h>
#include "config.h"

/*
*****************************************************************************************
* MACROS
*****************************************************************************************
*/
// Bit vector from bit position
#ifndef BV
#define BV(bit)                         (1 << (bit))
#endif

#ifndef min
#define min(a,b)                        ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)                        ((a) > (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)                   (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef IS_ELAPSED
#define IS_ELAPSED(ts, last, duration)  ((ts) - (last) > (duration))
#endif

#ifdef __DEBUG__
#define LOG(...)                        printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

/*
*****************************************************************************************
* TEMPLATE
*****************************************************************************************
*/

#ifdef __cplusplus
template <typename T>
void PROGMEM_read(const T * sce, T& dest) {
    memcpy_P(&dest, sce, sizeof (T));
}

template <typename T>
T PROGMEM_get(const T * sce) {
    static T temp;
    memcpy_P(&temp, sce, sizeof (T));
    return temp;
}

template<typename T, typename F>
struct alias_cast_t {
    union {
        F raw;
        T data;
    };
};

template<typename T, typename F>
T alias_cast(F raw_data) {
    alias_cast_t<T, F> ac;
    ac.raw = raw_data;
    return ac.data;
}
#endif

/*
*****************************************************************************************
* FUNCTIONS
*****************************************************************************************
*/
void dump(char *name, uint8_t *data, uint16_t cnt);
void bits2Str(char *str, void *bits, size_t const size);

#endif
