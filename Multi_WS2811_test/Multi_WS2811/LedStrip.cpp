#include "LedStrip.h"

LedStrip::LedStrip(int n)
{
   // Allocate 3 bytes per pixel:
    numLEDs = n;
    pixels = (uint8_t *)malloc(numPixelBytes());
    if (pixels) {
        memset(pixels, 0x00, numPixelBytes()); // Init to RGB 'off' state
    }
}

LedStrip::~LedStrip()
{
    free(pixels);
}
 
uint32_t LedStrip::total_luminance(void)
{
    uint32_t running_total;
    running_total = 0;
    for (int i=0; i< numPixelBytes(); i++)
        running_total += pixels[i];
    return running_total;
}

// Convert R,G,B to combined 32-bit color
uint32_t LedStrip::Color(uint8_t r, uint8_t g, uint8_t b)
{
    // Take the lowest 7 bits of each value and append them end to end
    // We have the top bit set high (its a 'parity-like' bit in the protocol
    // and must be set!)
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

// store the rgb component in our array
void LedStrip::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{
    if (n >= numLEDs) return; // '>=' because arrays are 0-indexed

    pixels[n*3  ] = g;
    pixels[n*3+1] = r;
    pixels[n*3+2] = b;
}

void LedStrip::setPixelR(uint16_t n, uint8_t r)
{
    if (n >= numLEDs) return; // '>=' because arrays are 0-indexed

    pixels[n*3+1] = r;
}

void LedStrip::setPixelG(uint16_t n, uint8_t g)
{
    if (n >= numLEDs) return; // '>=' because arrays are 0-indexed

    pixels[n*3] = g;
}

void LedStrip::setPixelB(uint16_t n, uint8_t b)
{
    if (n >= numLEDs) return; // '>=' because arrays are 0-indexed

    pixels[n*3+2] = b;
}

void LedStrip::setPackedPixels(uint8_t * buffer, uint32_t n)
{
    if (n >= numLEDs) return;
    memcpy(pixels, buffer, (size_t) (n*3));
}

void LedStrip::setPixelColor(uint16_t n, uint32_t c)
{
    if (n >= numLEDs) return; // '>=' because arrays are 0-indexed

    pixels[n*3  ] = (c >> 16);
    pixels[n*3+1] = (c >>  8);
    pixels[n*3+2] =  c;
}
