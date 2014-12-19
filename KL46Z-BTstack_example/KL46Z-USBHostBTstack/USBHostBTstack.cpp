// USBHostBTstack.cpp
#include "USBHostBTstack.h"

//#define BTSTACK_DEBUG

#ifdef BTSTACK_DEBUG
#define BT_DBG(x, ...) std::printf("[%s:%d]"x"\r\n", __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__);
#define BT_DBG_BYTES(STR,BUF,LEN) _debug_bytes(__PRETTY_FUNCTION__,__LINE__,STR,BUF,LEN)
#define USB_INFO(...) do{fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\r\n");}while(0);
#else
#define BT_DBG(...)  while(0);
#define BT_DBG_BYTES(S,BUF,LEN) while(0);
#define USB_INFO(...)
#endif

#define HCI_COMMAND_DATA_PACKET 0x01
#define HCI_ACL_DATA_PACKET     0x02
#define HCI_SCO_DATA_PACKET     0x03
#define HCI_EVENT_PACKET        0x04

USBHostBTstack::USBHostBTstack()
{
    host = USBHost::getHostInst();
    init();
    m_pCb = NULL;
}

void USBHostBTstack::init()
{
    BT_DBG("");
    dev = NULL;
    int_in = NULL;
    bulk_in = NULL;
    bulk_out = NULL;
    btstack_intf = -1;
    btstack_device_found = false;
    dev_connected = false;
    ep_int_in = false;
    ep_bulk_in = false;
    ep_bulk_out = false;
}

bool USBHostBTstack::connected()
{
    return dev_connected;
}

bool USBHostBTstack::connect()
{
    if (dev_connected) {
        return true;
    }
 
    for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {
        if ((dev = host->getDevice(i)) != NULL) {
            
            BT_DBG("Trying to connect BTstack device\r\n");
            
            if(host->enumerate(dev, this)) {
                break;
            }
            if (btstack_device_found) {
                int_in = dev->getEndpoint(btstack_intf, INTERRUPT_ENDPOINT, IN);
                bulk_in = dev->getEndpoint(btstack_intf, BULK_ENDPOINT, IN);
                bulk_out = dev->getEndpoint(btstack_intf, BULK_ENDPOINT, OUT);
                if (!int_in || !bulk_in || !bulk_out) {
                    continue;
                }
                USB_INFO("New BTstack device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, btstack_intf);
                //dev->setName("BTstack", btstack_intf);
                //host->registerDriver(dev, btstack_intf, this, &USBHostBTstack::init);
 
                //int_in->attach(this, &USBHostBTstack::int_rxHandler);
                //host->interruptRead(dev, int_in, int_report, int_in->getSize(), false);
 
                //bulk_in->attach(this, &USBHostBTstack::bulk_rxHandler);
                //host->bulkRead(dev, bulk_in, bulk_report, bulk_in->getSize(), false);
 
                dev_connected = true;
                return true;
            }
        }
    }
    init();
    return false;
}

/*virtual*/ void USBHostBTstack::setVidPid(uint16_t vid, uint16_t pid)
{
    BT_DBG("vid:%04x,pid:%04x", vid, pid);
}
 
/*virtual*/ bool USBHostBTstack::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol) //Must return true if the interface should be parsed
{
    BT_DBG("intf_nb=%d,intf_class=%02X,intf_subclass=%d,intf_protocol=%d", intf_nb, intf_class, intf_subclass, intf_protocol);
    if ((btstack_intf == -1) && intf_class == 0xe0) {
        btstack_intf = intf_nb;
        return true;
    }
    return false;
}
 
/*virtual*/ bool USBHostBTstack::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir) //Must return true if the endpoint will be used
{
    BT_DBG("intf_nb:%d,type:%d,dir:%d",intf_nb, type, dir);
    if (intf_nb == btstack_intf) {
        if (ep_int_in == false && type == INTERRUPT_ENDPOINT && dir == IN) {
            ep_int_in = true;
        } else if (ep_bulk_in == false && type == BULK_ENDPOINT && dir == IN) {
            ep_bulk_in = true;
        } else if (ep_bulk_out == false && type == BULK_ENDPOINT && dir == OUT) {
            ep_bulk_out = true;
        } else {
            return false;
        }
        if (ep_int_in && ep_bulk_in && ep_bulk_out) {
            btstack_device_found = true;
        }
        return true;
    }
    return false;
}
 
int USBHostBTstack::open()
{
    BT_DBG("%p", this);
    if (!connect()) {
        error("Bluetooth not found.\n");
    }
    return 0;
}

int USBHostBTstack::send_packet(uint8_t packet_type, uint8_t* packet, int size)
{
    USB_TYPE res;
    switch(packet_type){
        case HCI_COMMAND_DATA_PACKET:
            BT_DBG_BYTES("HCI_CMD:", packet, size);
            res = host->controlWrite(dev,
                    USB_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_RECIPIENT_DEVICE, 
                    0, 0, 0, packet, size);
            TEST_ASSERT(res == USB_TYPE_OK);
            break;
        case HCI_ACL_DATA_PACKET:
            BT_DBG_BYTES("ACL_SND:", packet, size);
            res = host->bulkWrite(dev, bulk_out, packet, size);
            TEST_ASSERT(res == USB_TYPE_OK);
            break;
        default:
            TEST_ASSERT(0);
    }
    return 0;
}

void USBHostBTstack::register_packet_handler(void (*pMethod)(uint8_t, uint8_t*, uint16_t) )
{
    BT_DBG("pMethod: %p", pMethod);
    m_pCb = pMethod;
}

void USBHostBTstack::poll()
{
    int result = host->interruptReadNB(int_in, int_report, sizeof(int_report));
    if (result >= 0) {
        int len = int_in->getLengthTransferred();
        BT_DBG_BYTES("HCI_EVT:", int_report, len);
        if (m_pCb) {
            m_pCb(HCI_EVENT_PACKET, int_report, len);
        }    
    }
    result = host->bulkReadNB(bulk_in, bulk_report, sizeof(bulk_report));
    if (result >= 0) {
        int len = bulk_in->getLengthTransferred();
        BT_DBG_BYTES("HCI_ACL_RECV:", bulk_report, len);
        if (m_pCb) {
            m_pCb(HCI_ACL_DATA_PACKET, bulk_report, len);
        }    
    }
}

void _debug_bytes(const char* pretty, int line, const char* s, uint8_t* buf, int len)
{
    printf("[%s:%d]\n%s", pretty, line, s);
    for(int i = 0; i < len; i++) {
        printf(" %02x", buf[i]);
    }
    printf("\n");
}
