#include "mbed.h"
#include "WS2811.h"
#include "Colors.h"
//#include "TSISensor.h"
//#include "MMA8451Q.h"

//#define MMA8451_I2C_ADDRESS (0x1d<<1)

// per LED: 3 * 20 mA = 60mA max
// 60 LEDs: 60 * 60mA = 3600 mA max
// 120 LEDs: 7200 mA max
unsigned const nLEDs = 60;
float brite = 0.15f;

//Serial pc(USBTX, USBRX);
//Timer timeRunning;

// @brief sets different colors in each of the LEDs of a strip
// @param strip the light strip
// @param sat saturation, 0.0 - 1.0
// @param brite brightness, 0.0 - 1.0
// @param hueShift shift, 0.0 - 1.0 is equivalent to 0 - 360 degrees
/*static void showRainbow(WS2811 &strip, float sat, float brite, float hueShift)
{
    unsigned nLEDs = strip.numPixels();
    for (unsigned i = 0; i < nLEDs; i++) {
        uint8_t r, g, b;
        float hue = ((float)i / (float)nLEDs) + hueShift;
        HSBtoRGB(hue, sat, brite, &r, &g, &b);
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}
*/

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
    //printf("Writing LED at (%d, %d)\n", row, col);
    WS2811 *strip = Strips + (row / 2);
    if (row & 0x01) // Odd number strip, second in pair, 60 LEDs per meter
    {
        strip->setPixelColor(59 - col, r, g, b); // Offset by 30 for 2nd in pair, double for 60 LED/m
    } else { // Even number strip, first in pair, 30 LEDs per meter
        strip->setPixelColor(col, r, g, b);
    }
    
    strip->show();
    //WS2811::startDMA();
    //printf("DMA started\n");
}

void writeLEDUncomp(int row, int col, uint8_t r, uint8_t g, uint8_t b)
{
    dim(&r, &g, &b);
    //printf("Writing LED at (%d, %d)\n", row, col);
    WS2811 *strip = Strips + (row / 2);
    if (row & 0x01) // Odd number strip, second in pair, 60 LEDs per meter
    {
        strip->setPixelUncompressed(88 - 2*col, r, g, b); // Offset by 30 for 2nd in pair, double for 60 LED/m
    } else { // Even number strip, first in pair, 30 LEDs per meter
        strip->setPixelUncompressed(col, r, g, b);
    }
    
    //strip->show();
    //WS2811::startDMA();
    //printf("DMA started\n");
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
    //Strips = new WS2811[15];
    //pc.baud(115200);
    for(i = 0; i < STRIPS_NUM-1; i++) {
        //printf("Strip address before %d\n", &(Strips[i]));
        WS2811* w = new WS2811(nLEDs, i);
        Strips[i] = *w;
        //printf("Strip address after %d\n", &(Strips[i]));
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
    
    /*rled = 1.0;
    gled = 1.0;*/

    //float sat = 1.0;

    //timeRunning.start();
/*
    uint8_t r =0;
    uint8_t g =0;
    uint8_t b =0;
    for (;;) {
        if (r < 40)
            r++;
        else if (g < 40)
            g++;
        else if (b < 40)
            b++;
        else {
            //unsigned running = timeRunning.read_us();
            //pc.printf("%u frames in %u usec = %u frames / sec\r\n", frames, running, frames * 1000000 / running);
            break;
        }

        WS2811::startDMA();

        frames++;
    }

    //timeRunning.reset();
    frames = 0;

    float xyz[3];
    acc.getAccAllAxis(xyz);
    pc.printf("x: %f y: %f z: %f\r\n", xyz[0], xyz[1], xyz[2]);

    for (;;) {
        acc.getAccAllAxis(xyz);
        rled = 1.0 - abs(xyz[0]);
        gled = 1.0 - abs(xyz[1]);
        readTouchSensor();
        showRainbow(lightStrip1, sat, brite, abs(xyz[0]));
        showRainbow(lightStrip2, sat, brite, abs(xyz[1]));
        WS2811::startDMA();

        frames ++;
    }*/
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