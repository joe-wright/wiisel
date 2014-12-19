// Mbed library to control WS2801-based RGB LED Strips
// some portions (c) 2011 Jelmer Tiete
// This library is ported from the Arduino implementation of Adafruit Industries
// found at: http://github.com/adafruit/LPD8806
// and their strips: http://www.adafruit.com/products/306
// Released under the MIT License: http://mbed.org/license/mit
//
/*****************************************************************************/

// Heavily modified by Jas Strong, 2012-10-04
// Changed to use a virtual base class and to use software SPI.
//
// Modified by Ned Konz, December 2013.
// Using three-phase DMA ala Paul Stoffegren's version.

#ifndef MBED_WS2811_H
#define MBED_WS2811_H

#include "mbed.h"
#include "LedStrip.h"

#define MAX_LEDS_PER_STRIP 90

extern "C" void DMA0_IRQHandler();
extern "C" void TPM0_IRQHandler();

class WS2811 : public LedStrip
{
public:
    WS2811(unsigned n, unsigned pinNumber);
    WS2811();

    virtual void begin();
    virtual void show();
    virtual void blank();
    void setPixelUncompressed(uint16_t n, uint8_t r, uint8_t g, uint8_t b);

    static void startDMA();
    static bool DMABusy() { return !dma_done;}

private:
    uint32_t pinMask;

    void writePixel(unsigned n, uint16_t *p);

    // Class Static:

    static bool initialized;
    static uint32_t enabledPins;
    static volatile bool dma_done;
    static void wait_for_dma_done() { while (!dma_done) __WFI(); }

    
    static void writeByte(uint8_t byte, uint32_t mask, uint32_t *dest);

    static void hw_init();
        static void io_init();
        static void clock_init();
        static void dma_init();
        static void tpm_init();
        static void dma_data_init();
        
    friend void TPM0_IRQHandler();
};

#endif

