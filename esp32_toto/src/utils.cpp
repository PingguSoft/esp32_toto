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

#include <stdarg.h>
#include "utils.h"


void dump(char *name, uint8_t *data, uint16_t cnt) {
    uint8_t  i;
    uint8_t  b;
    uint16_t addr = 0;

    LOG(PSTR("-- %s str size : %d -- \n"), name, cnt);
    while (cnt) {
        LOG(PSTR("%08x - "), addr);

        for (i = 0; (i < 16) && (i < cnt); i ++) {
            b = *(data + i);
            LOG(PSTR("%02x "), b);
        }

        LOG(PSTR(" : "));
        for (i = 0; (i < 16) && (i < cnt); i ++) {
            b = *(data + i);
            if ((b > 0x1f) && (b < 0x7f))
                LOG(PSTR("%c"), b);
            else
                LOG(PSTR("."));
        }
        LOG(PSTR("\n"));
        data += i;
        addr += i;
        cnt  -= i;
    }
}

void bits2Str(char *str, void *bits, size_t const size) {
    uint8_t *b = (uint8_t*)bits;
    uint8_t byte;
    int     i, j;

    for (i = size - 1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            *str++ = '0' + byte;
        }
    }
    *str = 0;
}
