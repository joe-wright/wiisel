#include "mbed.h"
#include "WS2811.h"
#include "Colors.h"
#include "TSISensor.h"
#include "MMA8451Q.h"

#define MMA8451_I2C_ADDRESS (0x1d<<1)

// per LED: 3 * 20 mA = 60mA max
// 60 LEDs: 60 * 60mA = 3600 mA max
// 120 LEDs: 7200 mA max
unsigned const nLEDs = MAX_LEDS_PER_STRIP;

// I/O pin usage
// PTD0 TPM0 CH0 monitor
// PTD1 TPM0 CH1 monitor
// PTD2 data output
// PTD3 debug output

unsigned const DATA_OUT_PIN1 = 2; // PTD2
unsigned const DATA_OUT_PIN2 = 3; // PTD3

Serial pc(USBTX, USBRX);
Timer timeRunning;

TSISensor touchSensor;
MMA8451Q acc(PTE25, PTE24, MMA8451_I2C_ADDRESS);
PwmOut rled(LED_RED);
PwmOut gled(LED_GREEN);
// LED_BLUE is on PTD1

float touchPercentage;
unsigned frames;

float const minBrite = 0.2;
float const maxBrite = 0.5;
float brite;

void readTouchSensor()
{
    touchPercentage *= 0.9;
    touchPercentage += touchSensor.readPercentage() * 0.1;
    brite = minBrite + (maxBrite - minBrite) * touchPercentage;
}

// @brief sets different colors in each of the LEDs of a strip
// @param strip the light strip
// @param sat saturation, 0.0 - 1.0
// @param brite brightness, 0.0 - 1.0
// @param hueShift shift, 0.0 - 1.0 is equivalent to 0 - 360 degrees
static void showRainbow(WS2811 &strip, float sat, float brite, float hueShift)
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

static void showSolidColor(WS2811 &strip, uint8_t r, uint8_t g, uint8_t b)
{
    unsigned nLEDs = strip.numPixels();
    for (unsigned i = 0; i < nLEDs; i++) {
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}

int main(void)
{
    pc.baud(115200);
    WS2811 lightStrip1(nLEDs, DATA_OUT_PIN1);
    WS2811 lightStrip2(nLEDs, DATA_OUT_PIN2);

    lightStrip1.begin();
    lightStrip2.begin();
    rled = 1.0;
    gled = 1.0;

    float sat = 1.0;

    timeRunning.start();

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
            unsigned running = timeRunning.read_us();
            //pc.printf("%u frames in %u usec = %u frames / sec\r\n", frames, running, frames * 1000000 / running);
            break;
        }

        showSolidColor(lightStrip1, r, g, b);
        showSolidColor(lightStrip2, r, g, b);
        WS2811::startDMA();

        frames++;
    }

    timeRunning.reset();
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
    }
}

