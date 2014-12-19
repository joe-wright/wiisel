/*
Wiisel
Fall 2014
EECS 149 - University of California - Berkeley
*/
#include "l2cap.h"
#include "debug.h"
#include "mbed.h"

#define BIAS 486
#define SENSITIVITY 102

#define UPPER_LEFT 0
#define UPPER_RIGHT 1
#define BOTTOM_LEFT 2
#define BOTTOM_RIGHT 3

#define SUCCESS 0
#define FAIL 1

typedef struct ir_object{
    // x, y are 10 bits each
    uint16_t x;  // range: 0-1023
    uint16_t y;  // range: 0-767
    // size is 4 bits
    uint8_t s; 
    ir_object()
    {
        x = 0;
        y = 0;
        s = 0;
    }
    ir_object(uint16_t xa, uint16_t ya, uint8_t sa = 0)
    {
        x = xa;
        y = ya;
        s = sa;
    }
}ir_object;

extern ir_object calibrated_corners[2];

// represent data reported from Wiimote in mode 33
typedef struct wii_packet{
    // acceleration. 10 bits each
    double X;
    double Y;
    double Z;
    //core buttons
    bool A; //button A : True if button A
    bool B; // button B
    bool B1; // button 1
    bool B2; // button 2
    ir_object IR_dot[4];
    wii_packet()
    {
        X = 0;
        Y = 0;
        Z = 0;
        A = false;
        B = false;
        B1 = false;
        B2 = false;
    }
   } wii_packet;
/**
enable_wiimote_ir : Initializes the IR Camera.
@Input:
    channel: channel Id
**/    
void enable_wiimote_ir(uint16_t channel);

/**
setup_reporting : Set up the report mode of Wiimote to report ID = 0x33
@input:
    channel: channel Id
**/ 
void setup_reporting(uint16_t channel);

/**
process_report: processes report bits to initialize a wii_packet object
@pre_condtion: report ID has to be 0x33
@input:
    report: report send from wiimote to host
    size: size of report
    wii_packet: will have a valid pointer as a result of the function on success
@return: 1 if report ID is not 0x33 or if report is null or if size is 0
         0 on success
**/
uint8_t process_report(uint8_t* report, int size, wii_packet *p);

/**
Using Affine model for the Wiimote: f(x) = ax + b
Assuming all axis have same bias, sensitivity:
bias = 486 units
sensitivity = 102 units/g
@input:
    a : value of acceleration on x,y,or z measured by wiimote
@return: value of acceleration in g using affine function
**/
double get_acceleration(uint16_t a);

/**
calculate roll based on current acceleration values 
@input:
    p : wii_packet which contains the acceleration values
@return: value of roll
**/
double get_roll(wii_packet *p);

/**
calculate pitch based on current acceleration values 
@input:
    p : wii_packet which contains the acceleration values
    roll: roll calculated using get_roll
@return: value of pitch
**/
double get_pitch(wii_packet *p, double roll);

/**
calculate magnitude of acceleration based on current acceleration values 
@input:
    p : wii_packet which contains the acceleration values
@return: value of magnitude
**/
double get_magnitude(wii_packet *p);

/**
calibrate IR leds in the 4 corners. Assumes the packets in array p were collected while the wiimote is pointing to center of screen.
@input:
    p[] : array of wii_packets which will be used to avarage the x and y
    p_size: size of array p
@return: 1 if calibration has failed
         0 on success
**/
uint8_t calibrate_corners1(wii_packet p[], uint16_t p_size);
uint8_t calibrate_corners2(wii_packet* p, uint16_t calibration_size);

/**
Assumes calibration was done
maps the wii to pixels
@input:
    p : wii_packet captured by wiimote
    x : output, x pixel
    y : output, y pixel
@return: 1 if mapping has failed (if x or y is negative <=> out of screen)
         0 on success
**/
uint8_t map_wii_pixels(wii_packet p,uint8_t* x, uint8_t* y);