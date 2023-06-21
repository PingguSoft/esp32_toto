#include <Arduino.h>
#include <Wire.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "utils.h"
#include "LowPower.h"

/*
*****************************************************************************************
* Class
*****************************************************************************************
*/
class PortCfg {
public:
    PortCfg(uint8_t idx, uint8_t mask) {
        switch (idx) {
            case 0:
                _dir_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(DDRB);
                _pin_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(PINB);
                _port_reg   = (volatile uint8_t*)_SFR_MEM_ADDR(PORTB);
                _pcmask_reg = (volatile uint8_t*)_SFR_MEM_ADDR(PCMSK0);
                break;

            case 1:
                _dir_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(DDRC);
                _pin_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(PINC);
                _port_reg   = (volatile uint8_t*)_SFR_MEM_ADDR(PORTC);
                _pcmask_reg = (volatile uint8_t*)_SFR_MEM_ADDR(PCMSK1);
                break;

            case 2:
                _dir_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(DDRD);
                _pin_reg    = (volatile uint8_t*)_SFR_MEM_ADDR(PIND);
                _port_reg   = (volatile uint8_t*)_SFR_MEM_ADDR(PORTD);
                _pcmask_reg = (volatile uint8_t*)_SFR_MEM_ADDR(PCMSK2);
                break;
        }
        _idx     = idx;
        _pc_mask = mask;
        find_nz_bit_counts(_pc_mask, &_nz_bit, &_nz_len);
    }

    void interrupt() {
        uint8_t old_sreg = SREG;

        cli();
        _value = *_pin_reg & _pc_mask;
        if (digitalRead(PIN_INT_REQ) == LOW)
            digitalWrite(PIN_INT_REQ, HIGH);
        SREG = old_sreg;
    }

    void setup() {
        // port configuration : inputs for PC pins
        *_dir_reg    &= ~_pc_mask;
        *_port_reg   |= _pc_mask;
        *_pcmask_reg |= _pc_mask;
        _value        = *_pin_reg;
        LOG("PCMASK:%d %2X=> %d, %d V:%2x\n", _idx, _pc_mask, _nz_bit, _nz_len, get());

        // enable port change interrupt
        PCICR |= _BV(_idx);
    }

    uint8_t get() {
        return (_value >> _nz_bit) & (_pc_mask >> _nz_bit);
    }

    uint8_t get_bits_len() {
        return _nz_len;
    }

private:
    uint8_t bitCount(uint8_t value) {
        //  parallel adding in a register SWAG algorithm
        uint8_t v = value;
        v = v - ((v >> 1) & 0x55);
        v = (v & 0x33) + ((v >> 2) & 0x33);
        v = (v + (v >> 4)) & 0x0F;
        return v;
    }

    void find_nz_bit_counts(uint8_t value, int8_t *nz, int8_t *count) {
        uint8_t v = value;
        int8_t  cnt = 0;
        int8_t  pos = 0;
        int8_t  nz_pos = -1;

        while (v > 0) {
            if (v & 1) {
                cnt++;
                if (nz_pos < 0)
                    nz_pos = pos;
            }
            v >>= 1;
            pos++;
        }
        *nz = nz_pos;
        *count = cnt;
    }

    uint8_t _idx;
    uint8_t _value;
    uint8_t _pc_mask;
    int8_t  _nz_bit;
    int8_t  _nz_len;
    volatile uint8_t *_dir_reg;
    volatile uint8_t *_port_reg;
    volatile uint8_t *_pin_reg;
    volatile uint8_t *_pcmask_reg;
};


/*
*****************************************************************************************
* VARIABLES
*****************************************************************************************
*/
static uint8_t _i2c_cmd;

// pin number order : D-B-C (D0-D7, D8-D13, A0-A5)
static PortCfg _port_cfg[3] = {
    PortCfg(2, PORTD_PC_MASK),
    PortCfg(0, PORTB_PC_MASK),
    PortCfg(1, PORTC_PC_MASK)
};


/*
*****************************************************************************************
* I2C slave events
*****************************************************************************************
*/
void i2c_receiveEvent(int len) {
    uint8_t cmd;
    uint8_t param;

    cmd = Wire.read();
    if (len > 1)
        param = Wire.read();

    //LOG(F("cmd : %10ld, %x len:%d\n"), millis(), cmd, len);
    switch (cmd) {
        case I2C_REG_SET_POWER_DOWN:
            LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
            break;

        default:
            _i2c_cmd = cmd;
            break;
    }
}

uint16_t get_port_status() {
    uint16_t    d16 = 0;
    uint8_t     shl = 0;
    uint8_t     old_sreg = SREG;

    cli();
    for (uint8_t i = 0; i < ARRAY_SIZE(_port_cfg); i++) {
        d16 |= ((uint16_t)_port_cfg[i].get() << shl);
        shl += _port_cfg[i].get_bits_len();
    }
    SREG = old_sreg;

    return d16;
}

void i2c_requestEvent() {
    uint16_t    d16;
    uint8_t     cmd = _i2c_cmd;

    _i2c_cmd = I2C_REG_NOP;
    switch (cmd) {
        case I2C_REG_GET_PIN_STATUS:
            d16 = get_port_status();
            Wire.write((uint8_t*)&d16, 2);
            digitalWrite(PIN_INT_REQ, LOW);
            break;
    }
}

/*
*****************************************************************************************
* SETUP
*****************************************************************************************
*/

// PortB (D8-D13)
ISR(PCINT0_vect) {
    _port_cfg[1].interrupt();
}

// PortC (A0-A5)
ISR(PCINT1_vect) {
    _port_cfg[2].interrupt();
}

// PortD (D0-D7)
ISR(PCINT2_vect) {
    _port_cfg[0].interrupt();
}

#if defined(ARDUINO_ARCH_AVR)
static int serialPutc(char c, FILE *) {
    Serial.write(c);
    return c;
}
#endif

void setup() {
#if defined(ARDUINO_ARCH_AVR)
    fdevopen(&serialPutc, 0);
#endif

    Serial.begin(115200);
    pinMode(PIN_INT_REQ, OUTPUT);
    digitalWrite(PIN_INT_REQ, LOW);

    // i2c slave
    Wire.begin(I2C_SLAVE_ADDR);
    Wire.onReceive(i2c_receiveEvent);
    Wire.onRequest(i2c_requestEvent);

    for (uint8_t i = 0; i < ARRAY_SIZE(_port_cfg); i++) {
        _port_cfg[i].setup();
    }
    LOG("Start !!\n");
    delay(100);
}

/*
*****************************************************************************************
* LOOP
*****************************************************************************************
*/
static uint16_t _d16 = 0;

static void bits2Str(char *buf, void const * const ptr, size_t const size) {
    uint8_t *b = (uint8_t*)ptr;
    uint8_t byte;
    int     i, j;

    for (i = size - 1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            *buf++ = '0' + byte;
        }
    }
    *buf = 0;
}

void loop() {
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

    // if (d16 != _d16) {
    if (true) {
        char     buf[20];
        uint16_t d16 = get_port_status();

        bits2Str(buf, &d16, sizeof(d16));
        LOG("%8ld PIN:%4X %s\n", millis(), d16, buf);
        _d16 = d16;
        delay(100);
        digitalWrite(PIN_INT_REQ, LOW);
    }
}
