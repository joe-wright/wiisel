#include "mbed.h"
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include "hci.h"
#include "wiimote.h"
#include "debug.h"
#include "display.h"
#include "bd_addr.h"  // class bd_addr
#include "WS2811.h"
#include "pictures.h"
Serial pc(USBTX, USBRX);

#define NO_PRINT
#ifdef NO_PRINT
#define printf(...) __nop()
#endif

#if defined(TARGET_NUCLEO_F401RE)
DigitalOut led1(LED1);
int led2 = 0;
#define LED_ON  0
#define LED_OFF 1
#elif defined(TARGET_KL46Z)||defined(TARGET_KL25Z)
DigitalOut led1(LED1), led2(LED2);
static int initial = 1;
#define LED_ON  0
#define LED_OFF 1
#else
#error "target error" 
#endif

#define PITCH_LIMIT 0.8f
#define ROLL_LIMIT 1.57f

static const uint8_t colors[4][4]={{234, 170, 23}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
int clr = 0;
int pic = 0;

bool B2_prsd = false;
bool B1_prsd = false;
bool drawing = true;

DigitalOut ptc13(PTC13);
DigitalOut ptc16(PTC16);
#define INQUIRY_INTERVAL 15
bd_addr addr;

void drawCursor(uint8_t row, uint8_t col)//, uint8_t r, uint8_t g, uint8_t b)
{
    if (row > 0)
        writeLEDUncomp(row - 1, col, 102, 0, 204);
    if (row < 29)
        writeLEDUncomp(row + 1, col, 102, 0, 204);
    if (col > 0)
        writeLEDUncomp(row, col - 1, 102, 0, 204);
    if (col < 29)
        writeLEDUncomp(row, col + 1, 102, 0, 204);
    writeLEDUncomp(row, col, colors[clr][0], colors[clr][1], colors[clr][2]);
}
    

//static bool calibrated = false;
//const uint8_t CALIBRATION_TRIAL = 5;
//const uint16_t CALIBRATION_SIZE = 1000;
static void hid_process_packet(uint8_t* report, int size)
{   
    // blankScreen();
    static int num = 0;
    //calibrated = true;
    //wii_packet* p = new wii_packet();
//    if(num >= 0) // debugging purpose only
//    {
//      process_report(report,size, p);
//      num--;
//      //hexdump(report, size);
//    }
//    free(p);
   
    /*if (!calibrated && report[1] == 0x33)
    {   
        static uint16_t c = 0;
        static uint8_t trial = 1;
        if (c == 0 && trial < CALIBRATION_TRIAL)
        {
            printf("Calibration %d Started!\n", trial);
            centerSquare(0,0,255);
            startDMA();
            wait_ms(500);
        }
        wii_packet* p;// = new wii_packet(); 
        if (process_report(report,size, p) == SUCCESS)
        {
          if (calibrate_corners2(p,CALIBRATION_SIZE) == SUCCESS)
            {
                calibrated = true;
                printf("Calibration Done!\n");
                blankScreen();
                startDMA();
                wait_ms(500);;
                printf("Upper_Left x:%d, y:%d", calibrated_corners[0].x , calibrated_corners[0].y);
                printf("Upper_Right x:%d, y:%d", calibrated_corners[1].x , calibrated_corners[1].y);
            }
        c++;
        }
        free(p);
        if(c == 3*CALIBRATION_SIZE)
        {
            printf("Calibration Failed!\n");
            calibrated = true;
        }
       
    }*/
    if(num >= 10 && !(WS2811::DMABusy()))
    {
        wii_packet* p = new wii_packet();
        //if(num > 0) // debugging purpose only
        //{
          process_report(report,size, p);
          double roll = get_roll(p);
          double pitch = get_pitch(p,roll);
          
          //writeLED(num/30, num%30, 122,255,25);
          //startDMA();
          printf("roll %f, pitch %f\n", roll,pitch);
          int r = (int)(((pitch+PITCH_LIMIT)/(PITCH_LIMIT*2))*30);
          r = 29 - r;
          if(r > 29)
          {
            r = 29;
          }
          if( r < 0)
          {
            r = 0;
          }
          int c = (int)(((roll+ROLL_LIMIT)/(ROLL_LIMIT*2))*30);
          if(c > 29)
          {
            c = 29;
          }
          if( c < 0)
          {
            c = 0;
          }
          
          
          //blankScreen();
//          for(int j = 0; j < 30; j++)
//            {  
//                writeLED(r,j, 234,170,23);    
//            }
//          for (int j = 0; j < 26; j++)
//            {
//                writeLED(j, c, 234, 170, 23);
//            }
            if (p->A && p->B)
            {
                drawing = true;
                pic = 0;
                blankScreen();
                startDMA();
            } else if (p->B && drawing) {
                writeLED(r, c, 0, 0, 0);
                refreshScreen();
                drawCursor(r, c);
                writeLEDUncomp(r, c, 0, 0, 0);
                startDMA();
            } else if (p->B1 and !B1_prsd) {
                drawing = false;
                switch (pic)
                {
                    case 0:
                        displayCal();
                        pic++;
                        break;
                    case 1:
                        displayLee();
                        pic++;
                        break;
                    case 2:
                        displayASV();
                        pic++;
                        break;
                    case 3:
                        displayAntonnio();
                        pic++;
                        break;
                    case 4:
                        refreshScreen();
                        drawing = true;
                        pic = 0;
                        break;
                    default:
                        pic = 0;
                        break;
                }
                startDMA();
            } else if (p->B2 and !B2_prsd) {
                clr = (clr + 1) % 4;
                //blankScreen();
//                startDMA();
//                drawing = false;
            } else if (p->A && drawing) {
                writeLED(r, c, colors[clr][0], colors[clr][1], colors[clr][2]);
                refreshScreen();
                //writeLEDUncomp(r, c, 102, 0, 204);
                drawCursor(r, c);
                startDMA();
            } else if (drawing) {
                refreshScreen();
                //writeLEDUncomp(r, c, 102, 0, 204);
                drawCursor(r, c);
                startDMA();
            }
                
            B2_prsd = p->B2;
            B1_prsd = p->B1;
//            int blockSize = 1;
//                int i = 1;
//                printf("Checking memory with blocksize %d char ...\n", blockSize);
//                while (true) {
//                    char *p2 = (char *) malloc(i * blockSize);
//                    if (p2 == NULL)
//                        break;
//                    free(p2);
//                    ++i;
//                }
//                printf("Ok for %d char\n", (i - 1) * blockSize);
//                while(1);
        free(p);
        num = 0;
    }
    else
    {
        num++;
    }
    
        /*
        if (report[0] == 0xa1 && report[1] == 0x33) { //modify back to 0x33
            led1 = (report[3] & 0x08) ? LED_ON : LED_OFF; // Two
            led2 = (report[3] & 0x04) ? LED_ON : LED_OFF; // One
            //led2 = (report[2] & 0x04) ? LED_ON : LED_OFF; // center
        }
        */
        
    }
    
    static void l2cap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
        //uint32_t retries = 255;
        if (packet_type == HCI_EVENT_PACKET && packet[0] == L2CAP_EVENT_CHANNEL_OPENED){
            if (packet[2]) { //check status
                printf("Connection failed\n");
                return;
            }
            printf("Connected\n");
            
        }
        
        //use this block to handle disconnect
        if (packet_type == HCI_EVENT_PACKET && packet[0] == L2CAP_EVENT_CHANNEL_CLOSED){
                log_info("Connection Disconnected\n");
                return;
        }
        
        if (packet_type == L2CAP_DATA_PACKET){
            // handle input
            //log_info("HID report, size %u\n", size);
            hid_process_packet(packet, size);
            if (initial) {
                enable_wiimote_ir(channel);
                setup_reporting(channel);
                uint8_t led_on_4[3] = {0xa2, 0x11, 0x80};
                l2cap_send_internal(channel, led_on_4, 3);
                initial = 0;
            }
        }
}

static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    //printf("Got packet");
    //led1 = LED_ON;
    char pin[6];
    //HD: packet[0] = event code, packet[1] = #length of all parameters in byte
    if (packet_type == HCI_EVENT_PACKET) {
        switch (packet[0]) {
            case BTSTACK_EVENT_STATE:
                // bt stack activated, get started - set local name
                if (packet[2] == HCI_STATE_WORKING) {
                    hci_send_cmd(&hci_write_authentication_enable, 1);
                }
                break;
                    
            case HCI_EVENT_INQUIRY_RESULT:
                // ignore none mouses: HD: class is 3 bytes. 002580
                //if ((packet[12] & 0x80) != 0x80 || packet[13] != 0x25) break;
                //HD: ignore none wiimote. class is 3 bytes 0x002504. little endian
                if ((packet[12] & 0x04) != 0x04 || packet[13] != 0x25) {
                     printf("try class 0x000508 instead of 0x002504 !\n");
                     break;
                }
                addr.data(&packet[3]); //HD: read bd_addr (6 bytes address). 
                printf("Wiimote addr: %s\n", addr.to_str());
                hci_send_cmd(&hci_inquiry_cancel);
                break;
                    
            case HCI_EVENT_INQUIRY_COMPLETE:
                printf("No wiimote found :(\n");
                break;
                
            case HCI_EVENT_LINK_KEY_REQUEST:
                // deny link key request
                hci_send_cmd(&hci_link_key_request_negative_reply, addr.data());
                break;
                    
            case HCI_EVENT_PIN_CODE_REQUEST:
                // inform about pin code request
                //HD: I think pin is the mac address
                printf("Enter 00:22:AA:5E:A0:C2 backwards \n");
                pin[0] = 0xC2;
                pin[1] = 0xA0;
                pin[2] = 0x5E;
                pin[3] = 0xAA;
                pin[4] = 0x22;
                pin[5] = 0x00;
                hci_send_cmd(&hci_pin_code_request_reply, addr.data(), 6, pin);
                break;
                    
            case HCI_EVENT_COMMAND_COMPLETE:
                if (COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable)){
                    printf("Inquiry\n");
                    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
                }
                if (COMMAND_COMPLETE_EVENT(packet, hci_inquiry_cancel) ) {
                    // inq successfully cancelled
                    printf("Connecting\n");
                    l2cap_create_channel_internal(NULL, l2cap_packet_handler, addr.data(), PSM_HID_INTERRUPT, 150);
                }
                break;
        }
    }
}

int main(void){
    pc.baud(115200);
    //log_info("%s\n", __FILE__);

    // init LEDs
    led1 = LED_OFF; //LED Red
    led2 = LED_OFF;
    display(); 
    ptc13 = LED_OFF;
    ptc16 = LED_OFF;
    blankScreen();
    startDMA(); 
    wait_ms(250);
    uint16_t wait_time = 750;
    float bright = 1.0;
//    triangle(127, 0, 255);
//    startDMA();
//    wait_ms(wait_time);
    //displayCal();
    //startDMA();
    while(0);
    while(0)
    {
        setBrite(bright);
        //lineByLine(127, 127, 127);
        //wait_ms(1000);
        colorScreen(0, 255, 0);
        startDMA();
        wait_ms(wait_time);
        blankScreen();
        colorScreen(255, 0, 0);
        startDMA();
        wait_ms(wait_time);
        blankScreen();
        colorScreen(255, 0, 255);
        startDMA();
        wait_ms(wait_time);
        blankScreen();
        colorScreen(0, 0, 255);
        startDMA();
        wait_ms(wait_time);
        triangle(127, 0, 255);
        startDMA();
        wait_ms(wait_time);
        blankScreen();
        startDMA();
        wait_ms(wait_time);
        centerSquare(255,10,10); //added by HD
        startDMA();
        wait_ms(wait_time);
        //while(bright > 0)
        //{
        //    setBrite(bright);
        //    bright-=0.1;
            colorScreen(200, 200, 0);
            startDMA();
            wait_ms(wait_time);
        //}
        blankScreen();
        startDMA();
        wait_ms(1000);
        bright -= 0.1;
        if (bright <= 0) bright = 1.0;
    }
    // GET STARTED
    run_loop_init(RUN_LOOP_EMBEDDED);

    // init HCI
    hci_transport_t* transport = hci_transport_usb_instance();
    remote_device_db_t* remote_db = (remote_device_db_t *)&remote_device_db_memory;
    hci_init(transport, NULL, NULL, remote_db);

    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
    printf("turn on!, send RESET command");
    // turn on!, send RESET command
    hci_power_control(HCI_POWER_ON);
            
    // go!
    run_loop_execute();    
    
    return 0;
}
