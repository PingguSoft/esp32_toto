#pragma once

#include <stdio.h>
#include "WAVFile.h"

class WAVFileWriter {
private:
    char    *_fname;
    int     _file_size;
    FILE    *_fp;
    wav_header_t _header;

public:
    WAVFileWriter(const char *fname, int sample_rate) {
        _fname = (char*)fname;
        _header.sample_rate = sample_rate;
    }

    void start() {
        _fp = fopen(_fname, "wb");
        if (_fp) {
            // write out the header - we'll fill in some of the blanks later
            fwrite(&_header, sizeof(wav_header_t), 1, _fp);
            _file_size = sizeof(wav_header_t);
        }
    }

    void write(int16_t *samples, int count) {
        // write the samples and keep track of the file size so far
        if (_fp) {
            fwrite(samples, sizeof(int16_t), count, _fp);
            _file_size += sizeof(int16_t) * count;
        }
    }

    void stop() {
        if (_fp) {
            // now fill in the header with the correct information and write it again
            _header.data_bytes = _file_size - sizeof(wav_header_t);
            _header.wav_size = _file_size - 8;
            fseek(_fp, 0, SEEK_SET);
            fwrite(&_header, sizeof(wav_header_t), 1, _fp);
            fclose(_fp);
        }
    }
};
