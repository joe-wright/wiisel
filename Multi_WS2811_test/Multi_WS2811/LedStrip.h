// Parent class for all addressable LED strips.
// Partially based on work by and (c) 2011 Jelmer Tiete
// whose library is ported from the Arduino implementation of Adafruit Industries
// found at: http://github.com/adafruit/LPD8806
// and their strips: http://www.adafruit.com/products/306
// Released under the MIT License: http://mbed.org/license/mit

// This is a pure virtual parent class for all LED strips, so that different types
// of strip may be used in a single array or container.

#include "mbed.h"

#ifndef LEDSTRIP_H
#define LEDSTRIP_H

class LedStrip
{
public:
    LedStrip(int n);
    ~LedStrip();

    virtual void begin(void)=0;
    virtual void show(void)=0;
    virtual void blank(void)=0;

    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b);

    uint16_t numPixels(void) { return numLEDs; }
    uint16_t numPixelBytes(void) { return numLEDs * 3; }
    uint32_t total_luminance(void);

    void setPixelB(uint16_t n, uint8_t b);
    void setPixelG(uint16_t n, uint8_t g);
    void setPixelR(uint16_t n, uint8_t r);
    
    void setPixelColor(uint16_t n, uint32_t c);
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
    void setPackedPixels(uint8_t * buffer, uint32_t n);

protected:
    uint8_t *pixels;     // Holds LED color values
    uint16_t numLEDs;     // Number of RGB LEDs in strand
};
#endif
