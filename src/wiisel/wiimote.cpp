#include "wiimote.h"
#include "math.h"


#define NO_PRINT
#ifdef NO_PRINT
#define printf(...) __nop()
#endif

ir_object calibrated_corners[2];

void enable_wiimote_ir(uint16_t channel)
{   
    printf("Enabling IR\n");
    int ms = 50; // wait time  150 worked. Reducing it to 50 to test <--
    const uint8_t IR_COMMAND_SIZE = 3;
    //Enable the 24MHz pixel clock on pin 7 of the camera
    uint8_t enable_ir_c1[IR_COMMAND_SIZE] = {0xa2, 0x13, 0x04};
    l2cap_send_internal(channel, enable_ir_c1,IR_COMMAND_SIZE); 
    
    //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    //Pull pin 4 low - probably an active-low enable.
    uint8_t enable_ir_c2[IR_COMMAND_SIZE] = {0xa2, 0x1a, 0x04};
    l2cap_send_internal(channel, enable_ir_c2, IR_COMMAND_SIZE); 
    
    //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    const uint8_t WRITE_COMMAND_SIZE = 23; 
    uint8_t w_reg_1[] = {0xa2, 0x16, 0x04,0xb0,0x00,0x30,0x01,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    l2cap_send_internal(channel, w_reg_1, WRITE_COMMAND_SIZE);  
    
     //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    //Write Sensitivity Block 1 = 00 00 aa 00 64 (Wii level 3) to registers at 0xb00004
    uint8_t w_reg_2[] = {0xa2, 0x16, 0x04, 0xb0,0x00,0x04,0x09,0x00,0x00,0xaa,0x00,0x64,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    l2cap_send_internal(channel, w_reg_2, WRITE_COMMAND_SIZE); 
    
    //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    //Write Sensitivity Block 2 = 63 03 (Wii level 3) to registers at 0xb0001a
    uint8_t w_reg_3[] = {0xa2, 0x16, 0x04, 0xb0,0x00,0x1a,0x02,0x63,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    l2cap_send_internal(channel, w_reg_3, WRITE_COMMAND_SIZE);
    
    //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    //Write Mode Number to register 0xb00033: using extended mode 0x03 (compatible with report 0x33)
    uint8_t w_reg_4[] = {0xa2, 0x16, 0x04, 0xb0,0x00,0x33,0x01,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    l2cap_send_internal(channel, w_reg_4, WRITE_COMMAND_SIZE);
    
    //Delay 50 ms to avoid random state
    wait_ms(ms);
    
    //Write 0x08 to register 0xb00030 (again) 
    l2cap_send_internal(channel, w_reg_1, WRITE_COMMAND_SIZE);
    
    //Wait before returning
     wait_ms(ms);  
}

/* Tests if IR Camera is correctly enabled by reading the values written to register.
Note: tester need to verify values manually for now*/
void test_ir_enabled(uint16_t channel)
{
    int ms = 50;
    log_info("Test: Reading all values from registers\n");
    
    uint8_t t_reg_1[8] = {0xa2, 0x17, 0x04, 0xb0,0x00,0x30,0x00,0x10};
    l2cap_send_internal(channel, t_reg_1, 8);
    wait_ms(ms);
    
    uint8_t t_reg_2[8] = {0xa2, 0x17, 0x04, 0xb0,0x00,0x00,0x00,0x10};
    l2cap_send_internal(channel, t_reg_2, 8);
    wait_ms(ms);
    
    uint8_t t_reg_3[8] = {0xa2, 0x17, 0x04, 0xb0,0x00,0x1a,0x00,0x10};
    l2cap_send_internal(channel, t_reg_3, 8);
    wait_ms(ms);
    
    uint8_t t_reg_4[8] = {0xa2, 0x17, 0x04, 0xb0,0x00,0x33,0x00,0x10};
    l2cap_send_internal(channel, t_reg_4, 8);
}

void setup_reporting(uint16_t channel)
{   
    const int SETUP_REPORT_SIZE = 4;
    uint8_t enable_report_33[SETUP_REPORT_SIZE] = {0xa2, 0x12, 0x04,0x33};
   printf("setting up report\n");
    // enable continuous reporting of buttons, accelerometer, IR camera
    l2cap_send_internal(channel, enable_report_33, SETUP_REPORT_SIZE);
}

uint8_t process_report(uint8_t* report, int size, wii_packet *p)
{
    if (report == NULL || size == 0)
        {
            printf("report null or size 0, size = %d \n", size);
            return FAIL;
        }
    if (report[0] == 0xa1 && report[1] != 0x33) 
        {
            printf("Incorrect report type \n");
            return FAIL;
        }
    /* Buttons */
    p -> A = (report[3] & 0x08) ? true : false; // Button A
    p -> B = (report[3] & 0x04) ? true : false; // Button B
    p -> B1 = (report[3] & 0x02) ? true : false; // Button 1
    p -> B2 = (report[3] & 0x01) ? true : false; // Button 2
    /* Acceleration */
    uint16_t t1 = (report[4] << 2); // shift X to left to get 2 LSB from Buttons Byte 0
    t1 |= ((report[2] & 0x60) >> 5);
    p -> X = get_acceleration(t1); 
    uint16_t t2 = (report[5] << 2); // shift Y to left to get 2 LSB from Buttons Byte 1
    t2 |= ((report[3] & 0x20) >> 4); // LSB in Y is always 0
    p -> Y = get_acceleration(t2);
    uint16_t t3 = (report[6] << 2); // shift Z to left to get 2 LSB from Buttons Byte 1
    t3 |= ((report[3] & 0x40) >> 5); // LSB in Z is always 0
    p -> Z = get_acceleration(t3);
    /* IR */
    uint8_t t = 7;
    uint8_t i = 0;
    while (t < size && i < 4)
    {
        p ->IR_dot[i].x = report[t];
        p -> IR_dot[i].x |= ((report[t+2] & 0x30) << 4);
        p -> IR_dot[i].y = report[t+1];
        p -> IR_dot[i].y |= ((report[t+2] & 0xC0) << 2);
        p -> IR_dot[i].s = (report[t+2] & 0x0F);
        i++;
        t+=3;
    } 
    
    return SUCCESS;
}

double get_acceleration(uint16_t a)
{
    return ((a - BIAS)*1.0)/ SENSITIVITY;
}

double get_roll(wii_packet *p)
{
    return atan2(p->X, p->Z);
}


double get_pitch(wii_packet *p, double roll)
{
    return atan2(-p->Y, p->X * sin(roll)+ p->Z * cos(roll));
}

double get_magnitude(wii_packet *p)
{
    return sqrt(p->X * p->X  + p->Y * p->Y + p->Z * p->Z); 
}


uint8_t calibrate_corners1(wii_packet p[], uint16_t p_size)
{   
    printf("Calibrate Corners\n");
    uint16_t count = 0;
    for (uint16_t i = 0; i < p_size; i++)
    {   
        // check that all the IR led's are visible in packet  
        if(p[i].IR_dot[0].x != 1023 && p[i].IR_dot[0].y != 1023 && p[i].IR_dot[0].s != 15 &&
           p[i].IR_dot[1].x != 1023 && p[i].IR_dot[1].y != 1023 && p[i].IR_dot[1].s != 15 //&&
           //p[i].IR_dot[2].x != 1023 && p[i].IR_dot[2].y != 1023 && p[i].IR_dot[2].s != 15 &&
           //p[i].IR_dot[3].x != 1023 && p[i].IR_dot[3].y != 1023 && p[i].IR_dot[3].s != 15)
        )
        {
            //TODO: need to figure out the order of IR LED in packet that wiimote report when it points to middle of screen.
            calibrated_corners[UPPER_LEFT].x += p[i].IR_dot[0].x;
            calibrated_corners[UPPER_LEFT].y += p[i].IR_dot[0].y; 
            calibrated_corners[UPPER_RIGHT].x += p[i].IR_dot[1].x;
            calibrated_corners[UPPER_RIGHT].y += p[i].IR_dot[1].y; 
            //calibrated_corners[BOTTOM_LEFT].x += p[i].IR_dot[2].x;
//            calibrated_corners[BOTTOM_LEFT].y += p[i].IR_dot[2].y; 
//            calibrated_corners[BOTTOM_RIGHT].x += p[i].IR_dot[3].x;
//            calibrated_corners[BOTTOM_RIGHT].y += p[i].IR_dot[3].y; 
            count++;
        }
    }
    if(count != 0)
    {
        calibrated_corners[UPPER_LEFT].x /= count; // average out the values 
        calibrated_corners[UPPER_LEFT].y /= count; 
        calibrated_corners[UPPER_RIGHT].x /= count;
        calibrated_corners[UPPER_RIGHT].y /= count;
        //calibrated_corners[BOTTOM_LEFT].x /= count;
//        calibrated_corners[BOTTOM_LEFT].y /= count;
//        calibrated_corners[BOTTOM_RIGHT].x /= count;
//        calibrated_corners[BOTTOM_RIGHT].y /= count; 
        return SUCCESS;
    }
    return FAIL;
}

uint8_t calibrate_corners2(wii_packet* p, uint16_t calibration_size)
{
    static uint16_t count = 0;
    static uint32_t x1 = 0, y1 = 0;
    static uint32_t x2 = 0, y2 = 0;
    // check that all the IR led's are visible in packet  
    if(p->IR_dot[0].x != 1023 && p->IR_dot[0].y != 1023 && p->IR_dot[0].s != 15 &&
       p->IR_dot[1].x != 1023 && p->IR_dot[1].y != 1023 && p->IR_dot[1].s != 15)
    {
            //TODO: need to figure out the order of IR LED in packet that wiimote report when it points to middle of screen.
            x1 += p->IR_dot[0].x;
            y1 += p->IR_dot[0].y; 
            x2 += p->IR_dot[1].x;
            y2 += p->IR_dot[1].y; 
            count++;
    }
    
    if(count == calibration_size)
    {
        calibrated_corners[UPPER_LEFT].x = (uint16_t) x1/count; // average out the values 
        calibrated_corners[UPPER_LEFT].y = (uint16_t) y1/count;
        calibrated_corners[UPPER_RIGHT].x = (uint16_t) x2/count;
        calibrated_corners[UPPER_RIGHT].y = (uint16_t) y2/count;
        printf("UL(x,y) = (%d,%d)\n",calibrated_corners[UPPER_LEFT].x,calibrated_corners[UPPER_LEFT].y);
        printf("UR(x,y) = (%d,%d)\n",calibrated_corners[UPPER_RIGHT].x,calibrated_corners[UPPER_RIGHT].y);
        return SUCCESS;
    }
    return FAIL;
}

uint8_t map_wii_pixels(wii_packet p,uint8_t* x, uint8_t* y)
{
    // find distance of IR led1 from Wii
    double d1 = sqrt(pow(double(calibrated_corners[UPPER_LEFT].x - p.IR_dot[0].x),2) + pow(double(calibrated_corners[UPPER_LEFT].y - p.IR_dot[0].y),2));
    // find distance of IR led2 from Wii 
    double d2 = sqrt(pow(double(calibrated_corners[UPPER_RIGHT].x - p.IR_dot[1].x),2) + pow(double(calibrated_corners[UPPER_RIGHT].y - p.IR_dot[1].y),2));
    // find distance between IR led1 and IR led2
    // should not need to calculate this every time. it is constant
    double d =  sqrt(pow(double(calibrated_corners[UPPER_LEFT].x - calibrated_corners[UPPER_RIGHT].x),2) + pow(double(calibrated_corners[UPPER_LEFT].y - calibrated_corners[UPPER_RIGHT].y),2));
    
    // find x
    double x_t = (-d2*d2 + d1*d1 + d*d)/(2*d);
    // find y
    double y_t = sqrt(d1*d1 - x_t*x_t);
    if(x_t < 0 || y_t < 0)
    {
        return FAIL;
    }
    *x = uint8_t(x_t);
    *y = uint8_t(y_t);
    return SUCCESS;
}
