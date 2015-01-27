#include "mbed.h"
#include "WS2811.h"
#include "Colors.h"

unsigned const nLEDs = 60;
float brite = 0.15f;


void dim(uint8_t* r, uint8_t* g, uint8_t* b)
{
    float HSBvals[3];
    float *HSB = RGBtoHSB(*r, *g, *b, HSBvals);
    return HSBtoRGB(HSB[0], HSB[1], brite*HSB[2], r, g, b);
}

void setBrite(float brightness)
{
    brite = brightness;
}
    

static void showSolidColor(WS2811 &strip, uint8_t r, uint8_t g, uint8_t b)
{
    unsigned nLEDs = strip.numPixels();
    for (unsigned i = 0; i < nLEDs; i++) {
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}

uint8_t const STRIPS_NUM = 15;
WS2811 Strips[STRIPS_NUM];

void writeLED(int row, int col, uint8_t r, uint8_t g, uint8_t b)
{
    dim(&r, &g, &b);
    WS2811 *strip = Strips + (row / 2);
    if (row & 0x01) // Odd number strip, second in pair, 60 LEDs per meter
    {
        strip->setPixelColor(59 - col, r, g, b); // Offset by 30 for 2nd in pair, double for 60 LED/m
    } else { // Even number strip, first in pair, 30 LEDs per meter
        strip->setPixelColor(col, r, g, b);
    }
    
    strip->show();
}

void writeLEDUncomp(int row, int col, uint8_t r, uint8_t g, uint8_t b)
{
    dim(&r, &g, &b);
    WS2811 *strip = Strips + (row / 2);
    if (row & 0x01) // Odd number strip, second in pair, 60 LEDs per meter
    {
        strip->setPixelUncompressed(88 - 2*col, r, g, b); // Offset by 30 for 2nd in pair, double for 60 LED/m
    } else { // Even number strip, first in pair, 30 LEDs per meter
        strip->setPixelUncompressed(col, r, g, b);
    }
    
}

//
// Optimized function to color the entire screen a solid color
//
void colorScreen(uint8_t r, uint8_t g, uint8_t b)
{
    dim(&r, &g, &b);
    int i,j;
    for (i = 0; i < STRIPS_NUM; i++)
    {
        for (j = 0; j < 30; j++)
        {
            Strips[i].setPixelColor(j, r, g, b);
            Strips[i].setPixelColor(59 - j, r, g, b);
        }
        Strips[i].show();
    }
}

//
// Optimized function to color the entire screen a solid color
//
void colorScreenUncomp(uint8_t r, uint8_t g, uint8_t b)
{
    int i,j;
    for (i = 0; i < 30; i++)
    {
        for (j = 0; j < 30; j++)
        {
            writeLEDUncomp(i, j, r, g, b);
        }
    }
}

//
// Optimized function to blank the entire screen
//
void blankScreen()
{
    int i;
    for (i = 0; i < STRIPS_NUM; i++)
    {
        showSolidColor(Strips[i], 0, 0, 0);
    }
}

//
// Optimized function to print an equilateral right angle triangle on the screen.
//
void triangle(uint8_t r, uint8_t g, uint8_t b)
{
    dim(&r, &g, &b);
    blankScreen();
    int i, j;
    for (i = 0; i < STRIPS_NUM; i++)
    {
        for (j = 0; j < 2*i + 1; j++)
        {
            Strips[i].setPixelColor(j, r, g, b);
            Strips[i].setPixelColor(59 - j, r, g, b);
        }
        Strips[i].setPixelColor(59 - j, r, g, b);
        Strips[i].show();
    }
}

//
// Start the DMA to actually write to the screen.
//
void startDMA()
{
    WS2811::startDMA();
}

//
// Deprecated. Previously used in test. Unoptimized.
//
void lineByLine(uint8_t r, uint8_t g, uint8_t b)
{
    int i, j;
    for (i = 0; i < 30; i++)
    {
        for (j = 0; j < 30; j++)
        {
            writeLED(i, j, r, g, b);
        }
        wait_ms(500);
        startDMA();
    }
}

void display(void)
{
    int i;
    for(i = 0; i < STRIPS_NUM-1; i++) {
        WS2811* w = new WS2811(nLEDs, i);
        Strips[i] = *w;
        Strips[i].begin();
    }
    if(STRIPS_NUM == 15)
    {
        WS2811* w = new WS2811(nLEDs, 16);
        Strips[i] = *w;
        Strips[i].begin();
    }
    else
    {
        WS2811* w = new WS2811(nLEDs, i);
        Strips[i] = *w;
        Strips[i].begin();
    }
}

void centerSquare(uint8_t r, uint8_t g, uint8_t b)
{
    blankScreen();
    int i, j;
    for (i = 12; i < 17; i++)
    {
        for (j = 12; j < 17; j++)
        {
            writeLED(i, j, r, g, b);
        }
    }
}

void refreshScreen()
{
    for (int i = 0; i < STRIPS_NUM; i++)
    {
        Strips[i].show();
    }
}
