// Simple USBHost for  Nucleo F401RE
#pragma once
#include "mbed.h"
#include "USBHostTypes.h"
#include "USBEndpoint.h"

struct SETUP_PACKET {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

class USBHALHost {
public:
    uint8_t LastStatus;
    uint8_t prev_LastStatus;

protected:
    USBHALHost();
    void init();
    virtual bool addDevice(USBDeviceConnected* parent, int port, bool lowSpeed) = 0;
    void setAddr(int addr, bool lowSpeed = false){}
    void setEndpoint(){}
    void token_transfer_init(){}
    int token_setup(USBEndpoint* ep, SETUP_PACKET* setup, uint16_t wLength = 0);
    int token_in(USBEndpoint* ep, uint8_t* data = NULL, int size = 0, int retryLimit = 10);
    int token_out(USBEndpoint* ep, const uint8_t* data = NULL, int size = 0, int retryLimit = 10);
    int token_iso_in(USBEndpoint* ep, uint8_t* data, int size);
    void token_ready(){}

private:
    int token_ctl_in(USBEndpoint* ep, uint8_t* data, int size, int retryLimit);
    int token_int_in(USBEndpoint* ep, uint8_t* data, int size);
    int token_blk_in(USBEndpoint* ep, uint8_t* data, int size, int retryLimit);
    int token_ctl_out(USBEndpoint* ep, const uint8_t* data, int size, int retryLimit);
    int token_int_out(USBEndpoint* ep, const uint8_t* data, int size);
    int token_blk_out(USBEndpoint* ep, const uint8_t* data, int size, int retryLimit);
    bool wait_attach();
    static USBHALHost * instHost;
};

