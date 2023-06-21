#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMOD.h"
#include "AudioGeneratorWAV.h"
#include "AudioInputI2S.h"
#include "AudioOutputI2S.h"
#include "AudioOutputMixer.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SPIFFS.h"
#include "WAVFileWriter.h"
#include "utils.h"

/*
*****************************************************************************************
* CONSTANTS
*****************************************************************************************
*/
extern "C" {
size_t esp_spiram_get_size(void);
void heap_caps_malloc_extmem_enable(size_t limit);
};

enum : int { ST_IDLE = 0,
             ST_PLAYING = 1,
             ST_RECORDING = 2 };

/*
*****************************************************************************************
* VARIABLES
*****************************************************************************************
*/
static SPIClass _spi_sd(VSPI);

static AudioGenerator *_generator = NULL;
static AudioOutputI2S *_i2s_out;
static AudioInputI2S *_i2s_in;
static AudioFileSource *_file_src = new AudioFileSourceSD();
// static AudioOutputMixer *_mixer;

static uint16_t _rec_buf_size;
static int16_t *_rec_buf;
static WAVFileWriter *_wav_writer;

static int _status = ST_IDLE;
static File _dir;
static float _gain = 1.0f;

/*
*****************************************************************************************
*
*****************************************************************************************
*/
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    LOG("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);

    if (!root) {
        LOG("Failed to open directory\n");
        return;
    }
    if (!root.isDirectory()) {
        LOG("Not a directory\n");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            LOG("[%-30s] %12s", file.name(), " ");
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            LOG(" %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            LOG(" %-30s  %12lu", file.name(), file.size());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            LOG(" %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

void check_resource(int status) {
    // recorder
    if (_rec_buf) {
        free(_rec_buf);
        _rec_buf = NULL;
    }
    if (_wav_writer) {
        delete _wav_writer;
        _wav_writer = NULL;
    }
    if (_i2s_in) {
        delete _i2s_in;
        _i2s_in = NULL;
    }

    // player
    if (_generator) {
        delete _generator;
        _generator = NULL;
    }
    // if (_mixer) {
    //     delete _mixer;
    //     _mixer = NULL;
    // }
    if (_i2s_out) {
        delete _i2s_out;
        _i2s_out = NULL;
    }
}

/*
*****************************************************************************************
*
*****************************************************************************************
*/
bool setup_play(String fname) {
    check_resource(ST_PLAYING);

    if (fname.endsWith(".wav")) {
        _generator = new AudioGeneratorWAV();
    } else if (fname.endsWith(".mod")) {
        AudioGeneratorMOD *mod = new AudioGeneratorMOD();
        mod->SetBufferSize(20 * 1024);
        _generator = mod;
    }

    if (_generator) {
        _file_src->close();
        if (_file_src->open(fname.c_str())) {
            if (_i2s_out == NULL)
                _i2s_out = new AudioOutputI2S();

            // AudioOutputMixerStub *stub;
            // _mixer = new AudioOutputMixer(32, _i2s_out);
            // stub = _mixer->NewInput();
            // stub->SetGain(1.5);

            _i2s_out->SetPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
            _i2s_out->begin();
            _i2s_out->SetGain(_gain);

            // mclk disable
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_GPIO0);
            pinMode(PIN_SLEEP_TEST, INPUT_PULLUP);
            _generator->begin(_file_src, _i2s_out);
            return true;
        }
    }
    return false;
}

void setup_rec(String fname) {
    check_resource(ST_RECORDING);

    if (_i2s_in == NULL)
        _i2s_in = new AudioInputI2S();
    _i2s_in->SetPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN);
    _i2s_in->SetRate(16000);
    _i2s_in->SetChannels(1);

    _wav_writer = new WAVFileWriter(fname.c_str(), _i2s_in->GetRate());
    _wav_writer->start();
    _rec_buf_size = (_i2s_in->GetRate() * 1) / 25;  // 40ms buffer
    _rec_buf = (int16_t *)malloc(sizeof(int16_t) * _rec_buf_size);

    _i2s_in->begin();
    // mclk disable
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_GPIO0);
    pinMode(PIN_SLEEP_TEST, INPUT_PULLUP);
}

/*
*****************************************************************************************
*
*****************************************************************************************
*/
RTC_DATA_ATTR int bootCount = 0;
void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            Serial.println("Wakeup caused by ULP program");
            break;
        default:
            Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
    }
}

void print_wakeup_touchpad() {
    touch_pad_t touchPin = esp_sleep_get_touchpad_wakeup_status();;

    switch (touchPin) {
        case 0:
            Serial.println("Touch detected on GPIO 4");
            break;
        case 1:
            Serial.println("Touch detected on GPIO 0");
            break;
        case 2:
            Serial.println("Touch detected on GPIO 2");
            break;
        case 3:
            Serial.println("Touch detected on GPIO 15");
            break;
        case 4:
            Serial.println("Touch detected on GPIO 13");
            break;
        case 5:
            Serial.println("Touch detected on GPIO 12");
            break;
        case 6:
            Serial.println("Touch detected on GPIO 14");
            break;
        case 7:
            Serial.println("Touch detected on GPIO 27");
            break;
        case 8:
            Serial.println("Touch detected on GPIO 33");
            break;
        case 9:
            Serial.println("Touch detected on GPIO 32");
            break;
        default:
            Serial.println("Wakeup not by touchpad");
            break;
    }
}

void callback() {
    // placeholder callback function
}

void deep_sleep(bool enable) {
    pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED, LOW);
        delay(250);
        digitalWrite(PIN_LED, HIGH);
        delay(250);
    }

    pinMode(PIN_WAKE, INPUT_PULLUP);

    if (enable) {
        _spi_sd.end();
        digitalWrite(PIN_SD_PWR, LOW);

        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE, 0);  // 1 = High, 0 = Low
        // touchAttachInterrupt(T9, callback, 40);
        // esp_sleep_enable_touchpad_wakeup();
        // esp_sleep_enable_timer_wakeup(10000000);
        LOG("Going to sleep now !\n");
        Serial.flush();
        delay(500);
        esp_deep_sleep_start();
    } else {
    }
}

/*
*****************************************************************************************
*
*****************************************************************************************
*/
void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    pinMode(PIN_SD_PWR, OUTPUT);
    digitalWrite(PIN_SD_PWR, HIGH);

    pinMode(PIN_SLEEP_TEST, INPUT_PULLUP);

    WiFi.mode(WIFI_OFF);
    setCpuFrequencyMhz(240);
    Serial.begin(115200);
    // heap_caps_malloc_extmem_enable(512);

    LOG("chip:%s, revision:%d, flash:%d, heap:%d, psram:%d\n", ESP.getChipModel(), ESP.getChipRevision(),
        ESP.getFlashChipSize(), ESP.getFreeHeap(), ESP.getPsramSize());

    // Increment boot number and print it every reboot
    LOG("Boot number: %d\n", ++bootCount);

    // Print the wakeup reason for ESP32
    print_wakeup_reason();
    print_wakeup_touchpad();

    heap_caps_dump_all();
    LOG("largest heap size : %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

#if 1
    _spi_sd.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, -1);
    if (SD.begin(PIN_SD_CS, _spi_sd)) {
        uint8_t cardType = SD.cardType();

        if (cardType != CARD_NONE) {
            LOG("SD Card Type: ");
            if (cardType == CARD_MMC) {
                LOG("MMC");
            } else if (cardType == CARD_SD) {
                LOG("SDSC");
            } else if (cardType == CARD_SDHC) {
                LOG("SDHC");
            } else {
                LOG("UNKNOWN");
            }

            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            LOG(", SD Card Size: %lluMB\n", cardSize);
            listDir(SD, "/", 0);
            _dir = SD.open("/");
        } else {
            LOG("No SD card attached\n");
        }
    } else {
        LOG("Card Mount Failed\n");
    }
#endif

    audioLogger = &Serial;
    // deep_sleep(true);
}

void loop() {
    int key;
    int16_t bytes;
    bool ret;

    key = Serial.available() ? Serial.read() : -1;

    if (_status != ST_PLAYING) {
        if (digitalRead(PIN_SLEEP_TEST) == LOW) {
            while (digitalRead(PIN_SLEEP_TEST) == LOW)
                ;
            deep_sleep(true);
        }
    }

    // global key
    switch (key) {
        case ']':
            _gain = (_gain < 2.0) ? (_gain + 0.1) : _gain;
            LOG("Gain : %2.1f\n", _gain);
            if (_i2s_out)
                _i2s_out->SetGain(_gain);
            break;

        case '[':
            _gain = (_gain > 0) ? (_gain - 0.1) : _gain;
            LOG("Gain : %2.1f\n", _gain);
            if (_i2s_out)
                _i2s_out->SetGain(_gain);
            break;
    }

    // key with status
    switch (_status) {
        case ST_IDLE:
            switch (key) {
                case 'r':
                    setup_rec("/sd/rec.wav");
                    LOG("START RECORDING!\n");
                    _status = ST_RECORDING;
                    break;

                case 'p':
                    setup_play("/rec.wav");
                    LOG("START PLAYING!\n");
                    _status = ST_PLAYING;
                    break;

                case 'n':
                    while (true) {
                        File file = _dir.openNextFile();
                        if (file) {
                            if (setup_play("/" + String(file.name()))) {
                                LOG("START PLAYING : %s\n", file.name());
                                _status = ST_PLAYING;
                                break;
                            }
                        } else {
                            _dir.rewindDirectory();
                        }
                    }
                    break;
            }
            break;

        case ST_PLAYING:
            if (_generator) {
                ret = _generator->isRunning();
                if (ret) {
                    ret = _generator->loop();
                    ret = (key == ' ') ? false : ret;
                    if (!ret) {
                        _generator->stop();
                        LOG("STOP PLAYING!\n");
                        _status = ST_IDLE;
                    }
                } else {
                    _status = ST_IDLE;
                }
            }
            break;

        case ST_RECORDING:
            if (key == ' ') {
                _wav_writer->stop();
                _i2s_in->stop();
                LOG("STOP RECORDING!\n");
                _status = ST_IDLE;
            } else {
                bytes = _i2s_in->read(_rec_buf, _rec_buf_size);
                _wav_writer->write(_rec_buf, bytes / sizeof(int16_t));
                LOG(".");
            }
            break;
    }
}
