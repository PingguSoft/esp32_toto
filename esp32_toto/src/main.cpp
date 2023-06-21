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
#include "DeepSleep.h"

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

static const int kMAX_MIX = 3;

/*
*****************************************************************************************
* VARIABLES
*****************************************************************************************
*/
static SPIClass _spi_sd(VSPI);

static AudioOutputI2S *_i2s_out = new AudioOutputI2S();
static AudioGenerator *_gen[kMAX_MIX];
static AudioFileSource *_file_src[kMAX_MIX];
static AudioOutputMixer *_mixer = new AudioOutputMixer(32, _i2s_out);
static AudioOutputMixerStub *_stub[kMAX_MIX];

static AudioInputI2S *_i2s_in = new AudioInputI2S();
static uint16_t _rec_buf_size = 0;
static int16_t *_rec_buf = NULL;
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


/*
*****************************************************************************************
*
*****************************************************************************************
*/
int get_free_slot() {
    for (int i = 0; i < kMAX_MIX; i++) {
        if (!_gen[i]->isRunning()) {
            return i;
        }
    }
    _gen[0]->stop();
    return 0;
}

bool setup_play(String fname) {
    // if (fname.endsWith(".wav")) {
    //     _gen = new AudioGeneratorWAV();
    // } else if (fname.endsWith(".mod")) {
    //     AudioGeneratorMOD *mod = new AudioGeneratorMOD();
    //     mod->SetBufferSize(20 * 1024);
    //     _gen = mod;
    // }

    int slot = get_free_slot();

    LOG("PLAYING %s  slot:%d\n", fname.c_str(), slot);
    _file_src[slot]->close();
    if (_file_src[slot]->open(fname.c_str())) {
        _stub[slot] = _mixer->NewInput();
        _stub[slot]->SetGain(1.0);

        if (_status != ST_PLAYING) {
            LOG("I2S OUTPUT SETUP\n");
            _i2s_out->SetPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
            _i2s_out->begin();
            _i2s_out->SetGain(_gain);

            // mclk disable
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_GPIO0);
            pinMode(PIN_SLEEP_TEST, INPUT_PULLUP);
        }
        _gen[slot]->begin(_file_src[slot], _stub[slot]);

        return true;
    }
    return false;
}

void setup_rec(String fname) {
    if (_status != ST_RECORDING) {
        LOG("I2S INPUT SETUP\n");
        _i2s_in->SetPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN);
        _i2s_in->SetRate(22050);
        _i2s_in->SetChannels(1);
    }

    if (_wav_writer)
        delete _wav_writer;

    _wav_writer = new WAVFileWriter(fname.c_str(), _i2s_in->GetRate());
    _wav_writer->start();
    _rec_buf_size = (_i2s_in->GetRate() * 1) / 25;  // 40ms buffer
    if (!_rec_buf)
        _rec_buf = (int16_t *)malloc(sizeof(int16_t) * _rec_buf_size);

    if (_status != ST_RECORDING) {
        _i2s_in->begin();
        // mclk disable
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_GPIO0);
        pinMode(PIN_SLEEP_TEST, INPUT_PULLUP);
    }
}

/*
*****************************************************************************************
*
*****************************************************************************************
*/
void deep_sleep() {
    pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED, LOW);
        delay(250);
        digitalWrite(PIN_LED, HIGH);
        delay(250);
    }
    pinMode(PIN_WAKE, INPUT_PULLDOWN);

    _spi_sd.end();
    digitalWrite(PIN_SD_PWR, LOW);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE, 1);  // 1 = High, 0 = Low
    // touchAttachInterrupt(T9, callback, 40);
    // esp_sleep_enable_touchpad_wakeup();
    // esp_sleep_enable_timer_wakeup(10000000);
    LOG("Going to sleep now !\n");
    Serial.flush();
    delay(500);
    esp_deep_sleep_start();
}

/*
*****************************************************************************************
*
*****************************************************************************************
*/
void setup() {
    for (int i = 0; i < kMAX_MIX; i++) {
        _gen[i] = new AudioGeneratorWAV();
        _file_src[i] = new AudioFileSourceSD();
    }

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

    // Print the wakeup reason for ESP32
    print_wakeup_reason();
    print_wakeup_touchpad();

    // heap_caps_dump_all();
    // LOG("largest heap size : %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    _spi_sd.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, -1);
    if (SD.begin(PIN_SD_CS, _spi_sd)) {
        uint8_t cardType = SD.cardType();

        if (cardType != CARD_NONE) {
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            LOG(", SD Card Size: %lluMB\n", cardSize);
            listDir(SD, "/words", 0);
            _dir = SD.open("/words");
        } else {
            LOG("No SD card attached\n");
        }
    } else {
        LOG("Card Mount Failed\n");
    }

    audioLogger = &Serial;
    // deep_sleep(true);
}

void loop() {
    int key;
    int16_t bytes;
    bool ret;

    key = Serial.available() ? Serial.read() : -1;

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

        case 'p':
            if (_status != ST_RECORDING) {
                while (true) {
                    File file = _dir.openNextFile();
                    if (file) {
                        if (setup_play("/words/" + String(file.name()))) {
                            _status = ST_PLAYING;
                            break;
                        }
                    } else {
                        _dir.rewindDirectory();
                    }
                }
            }
            break;

        case 'r':
            if (_status == ST_RECORDING) {
                _wav_writer->stop();
                _i2s_in->stop();
                LOG("STOP RECORDING!\n");
                _status = ST_IDLE;
            } else {
                // stop playing
                for (int i = 0; i < kMAX_MIX; i++) {
                    if (_gen[i]->isRunning()) {
                        _gen[i]->stop();
                        _stub[i]->stop();
                        delete _stub[i];
                    }
                }
                _i2s_out->stop();

                // start recording
                setup_rec("/sd/words/rec.wav");
                LOG("START RECORDING!\n");
                _status = ST_RECORDING;                
            } 
            break;
    }

    switch (_status) {
        case ST_PLAYING:
            if (true) {
                bool idle = true;

                for (int i = 0; i < kMAX_MIX; i++) {
                    if (_gen[i]->isRunning()) {
                        idle = false;
                        if (!_gen[i]->loop()) {
                            _gen[i]->stop();
                            _stub[i]->stop();
                            LOG("STOP PLAYING slot:%d\n", i);
                            delete _stub[i];
                        }
                    }
                }

                if (idle) {
                    if (digitalRead(PIN_SLEEP_TEST) == LOW) {
                        while (digitalRead(PIN_SLEEP_TEST) == LOW);
                        deep_sleep();
                    }
                }
            }
            break;

        case ST_RECORDING:
            bytes = _i2s_in->read(_rec_buf, _rec_buf_size);
            _wav_writer->write(_rec_buf, bytes / sizeof(int16_t));
            LOG(".");
            break;
    }
}
