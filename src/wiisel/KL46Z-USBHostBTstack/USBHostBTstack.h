#include "USBHostConf.h"
#include "USBHost.h"
#pragma once

#define TEST_ASSERT(A) while(!(A)){fprintf(stderr,"\n\n%s@%d %s ASSERT!\n\n",__PRETTY_FUNCTION__,__LINE__,#A);exit(1);};

/** 
 * A class to communicate a BTstack
 */
class USBHostBTstack : public IUSBEnumerator {
public:
    /**
    * Constructor
    *
    */
    USBHostBTstack();

    /**
    * Check if a BTstack device is connected
    *
    * @return true if a BTstack device is connected
    */
    bool connected();

    /**
     * Try to connect to a BTstack device
     *
     * @return true if connection was successful
     */
    bool connect();
    
    int open();
    int send_packet(uint8_t packet_type, uint8_t* packet, int size);
    void register_packet_handler( void (*pMethod)(uint8_t, uint8_t*, uint16_t));
    void poll();

protected:
    //From IUSBEnumerator
    virtual void setVidPid(uint16_t vid, uint16_t pid);
    virtual bool parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol); //Must return true if the interface should be parsed
    virtual bool useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir); //Must return true if the endpoint will be used

private:
    USBHost * host;
    USBDeviceConnected * dev;
    bool dev_connected;
    uint8_t int_report[64];
    uint8_t bulk_report[64];
    USBEndpoint * int_in;
    USBEndpoint * bulk_in;
    USBEndpoint * bulk_out;
    bool ep_int_in;
    bool ep_bulk_in;
    bool ep_bulk_out;

    bool btstack_device_found;
    int btstack_intf;
    void (*m_pCb)(uint8_t, uint8_t*, uint16_t);
    void init();
};

void _debug_bytes(const char* pretty, int line, const char* s, uint8_t* buf, int len);
