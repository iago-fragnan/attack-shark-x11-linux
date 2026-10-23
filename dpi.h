#ifndef DPI_H
#define DPI_H

#include <cstdint>

struct DpiEncode {
    uint8_t x = 0;
    uint8_t y = 0;
    bool isDouble = false;
};

extern const uint8_t DPI_3311[220];

DpiEncode convertDpiToBytes(int dpi);

void buildDpiReport(const int dpi[6], int activeStage,
                    bool angleSnap, bool rippleControl,
                    uint8_t out[56]);

#endif // DPI_H
