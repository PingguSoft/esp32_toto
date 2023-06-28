#ifndef _CONFIG_H_
#define _CONFIG_H_

/*
*****************************************************************************************
* FEATURES
*****************************************************************************************
*/
#define __DEBUG__           1


/*
*****************************************************************************************
* CONSTANTS
*****************************************************************************************
*/
#define WIFI_SSID           "TJ's House"
#define WIFI_PASSWORD       "cafebabe12"
#define CALIBRATION_FILE    "/touch.cal"


/*
*****************************************************************************************
* H/W CONSTANTS (PINS)
*****************************************************************************************
*/
#if 0
// T-Audio
#define PIN_LED             22

// T-Audio 1.6 WM8978 I2C pins.
#define PIN_I2C_SDA         19
#define PIN_I2C_SCL         18

// T-Audio 1.6 WM8978 MCLK gpio number
#define PIN_I2S_MCLKPIN     0
#endif

// T-Audio 1.6 WM8978 I2S pins.
#define PIN_I2S_BCK         33
#define PIN_I2S_WS          25
#define PIN_I2S_DOUT        26
#define PIN_I2S_DIN         27

// #define PIN_LED             5

#define PIN_SD_PWR          17
#define PIN_SD_CS           5
#define PIN_SD_CLK          18
#define PIN_SD_MOSI         23
#define PIN_SD_MISO         19

#define PIN_SLEEP_TEST       0
#define PIN_REC_SW           4
#define PIN_TOUCH_1         36
#define PIN_TOUCH_2         39
#define PIN_TOUCH_3         32
#define PIN_TOUCH_4         34
#define PIN_TOUCH_5         14
#define PIN_TOUCH_6         12
#define PIN_TOUCH_7         13

/*
*****************************************************************************************
* MACROS & STRUCTURES
*****************************************************************************************
*/

#endif