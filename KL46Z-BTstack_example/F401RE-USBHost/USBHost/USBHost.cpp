// Simple USBHost for FRDM-KL46Z
#include "USBHost.h"
#include <algorithm>

USBHost* USBHost::inst = NULL;

USBHost* USBHost::getHostInst()
{
    if (inst == NULL) {
        inst = new USBHost();
        inst->init();
    }
    return inst;
}

void USBHost::poll()
{
    if (inst) {
        inst->task();
    }
}

USBHost::USBHost() {
}

/* virtual */ bool USBHost::addDevice(USBDeviceConnected* parent, int port, bool lowSpeed) {
    USBDeviceConnected* dev = new USBDeviceConnected;
    USBEndpoint* ep = new USBEndpoint(dev);
    dev->init(0, port, lowSpeed);
    dev->setAddress(0);
    dev->setEpCtl(ep);
    uint8_t desc[18];
    wait_ms(100);

    int rc = controlRead(dev, 0x80, GET_DESCRIPTOR, 1<<8, 0, desc, 8);
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    if (rc != USB_TYPE_OK) {
        USB_ERR("ADD DEVICE FAILD: %d", rc);
    }
    USB_DBG_HEX(desc, 8);
    DeviceDescriptor* dev_desc = reinterpret_cast<DeviceDescriptor*>(desc);
    ep->setSize(dev_desc->bMaxPacketSize);

    int new_addr = USBDeviceConnected::getNewAddress();
    rc = controlWrite(dev, 0x00, SET_ADDRESS, new_addr, 0, NULL, 0);
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    dev->setAddress(new_addr);
    wait_ms(100);

    rc = controlRead(dev, 0x80, GET_DESCRIPTOR, 1<<8, 0, desc, sizeof(desc));
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    USB_DBG_HEX(desc, sizeof(desc));

    dev->setVid(dev_desc->idVendor);
    dev->setPid(dev_desc->idProduct);
    dev->setClass(dev_desc->bDeviceClass);
    USB_INFO("parent:%p port:%d speed:%s VID:%04x PID:%04x class:%02x addr:%d",
        parent, port, (lowSpeed ? "low " : "full"), dev->getVid(), dev->getPid(), dev->getClass(),
        dev->getAddress());

    DeviceLists.push_back(dev);

    if (dev->getClass() == HUB_CLASS) {
        const int config = 1;
        int rc = controlWrite(dev, 0x00, SET_CONFIGURATION, config, 0, NULL, 0);
        USB_TEST_ASSERT(rc == USB_TYPE_OK);
        wait_ms(100);
        Hub(dev);
    }
    return true;
}

// enumerate a device with the control USBEndpoint
USB_TYPE USBHost::enumerate(USBDeviceConnected * dev, IUSBEnumerator* pEnumerator)
{
    if (dev->getClass() == HUB_CLASS) { // skip hub class
        return USB_TYPE_OK;
    }
    uint8_t desc[18];
    USB_TYPE rc = controlRead(dev, 0x80, GET_DESCRIPTOR, 1<<8, 0, desc, sizeof(desc));
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    USB_DBG_HEX(desc, sizeof(desc));
    if (rc != USB_TYPE_OK) {
        return rc;
    }
    DeviceDescriptor* dev_desc = reinterpret_cast<DeviceDescriptor*>(desc);
    dev->setClass(dev_desc->bDeviceClass);
    pEnumerator->setVidPid(dev->getVid(), dev->getPid());

    rc = controlRead(dev, 0x80, GET_DESCRIPTOR, 2<<8, 0, desc, 4);
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    USB_DBG_HEX(desc, 4);

    int TotalLength = desc[2]|desc[3]<<8;
    uint8_t* buf = new uint8_t[TotalLength];
    rc = controlRead(dev, 0x80, GET_DESCRIPTOR, 2<<8, 0, buf, TotalLength);
    USB_TEST_ASSERT(rc == USB_TYPE_OK);
    //USB_DBG_HEX(buf, TotalLength);

    // Parse the configuration descriptor
    parseConfDescr(dev, buf, TotalLength, pEnumerator);
    delete[] buf;
    // only set configuration if not enumerated before
    if (!dev->isEnumerated()) {
        USB_DBG("Set configuration 1 on dev: %p", dev);
        // sixth step: set configuration (only 1 supported)
        int config = 1;
        USB_TYPE res = controlWrite(dev, 0x00, SET_CONFIGURATION, config, 0, NULL, 0);
        if (res != USB_TYPE_OK) {
            USB_ERR("SET CONF FAILED");
            return res;
        }
        // Some devices may require this delay
        wait_ms(100);
        dev->setEnumerated();
        // Now the device is enumerated!
        USB_DBG("dev %p is enumerated", dev);
    }
    return USB_TYPE_OK;
}

// this method fills the USBDeviceConnected object: class,.... . It also add endpoints found in the descriptor.
void USBHost::parseConfDescr(USBDeviceConnected * dev, uint8_t * conf_descr, uint32_t len, IUSBEnumerator* pEnumerator)
{
    uint32_t index = 0;
    uint32_t len_desc = 0;
    uint8_t id = 0;
    USBEndpoint * ep = NULL;
    uint8_t intf_nb = 0;
    bool parsing_intf = false;
    uint8_t current_intf = 0;
    EndpointDescriptor* ep_desc;

    while (index < len) {
        len_desc = conf_descr[index];
        id = conf_descr[index+1];
        USB_DBG_HEX(conf_descr+index, len_desc);
        switch (id) {
            case CONFIGURATION_DESCRIPTOR:
                USB_DBG("dev: %p has %d intf", dev, conf_descr[4]);
                dev->setNbIntf(conf_descr[4]);
                break;
            case INTERFACE_DESCRIPTOR:
                if(pEnumerator->parseInterface(conf_descr[index + 2], conf_descr[index + 5], conf_descr[index + 6], conf_descr[index + 7])) {
                    intf_nb++;
                    current_intf = conf_descr[index + 2];
                    dev->addInterface(current_intf, conf_descr[index + 5], conf_descr[index + 6], conf_descr[index + 7]);
                    USB_DBG("ADD INTF %d on device %p: class: %d, subclass: %d, proto: %d", current_intf, dev, conf_descr[index + 5],conf_descr[index + 6],conf_descr[index + 7]);
                    parsing_intf = true;
                } else {
                    parsing_intf = false;
                }
                break;
            case ENDPOINT_DESCRIPTOR:
                ep_desc = reinterpret_cast<EndpointDescriptor*>(conf_descr+index);
                if (parsing_intf && (intf_nb <= MAX_INTF) ) {
                    ENDPOINT_TYPE type = (ENDPOINT_TYPE)(ep_desc->bmAttributes & 0x03);
                    ENDPOINT_DIRECTION dir = (ep_desc->bEndpointAddress & 0x80) ? IN : OUT;
                    if(pEnumerator->useEndpoint(current_intf, type, dir)) {
                        ep = new USBEndpoint(dev);
                        ep->init(type, dir, ep_desc->wMaxPacketSize, ep_desc->bEndpointAddress);
                        USB_DBG("ADD USBEndpoint %p, on interf %d on device %p", ep, current_intf, dev);
                        dev->addEndpoint(current_intf, ep);
                    }
                }
                break;
            case HID_DESCRIPTOR:
                //lenReportDescr = conf_descr[index + 7] | (conf_descr[index + 8] << 8);
                break;
            default:
                break;
        }
        index += len_desc;
    }
}

USB_TYPE USBHost::controlRead(USBDeviceConnected* dev, uint8_t requestType, uint8_t request, uint32_t value, uint32_t index, uint8_t * buf, uint32_t len) {
    SETUP_PACKET setup = {requestType, request, value, index};
    int result = ControlRead(dev, &setup, buf, len);
    //USB_DBG2("result=%d %02x", result, LastStatus);
    return (result >= 0) ? USB_TYPE_OK : USB_TYPE_ERROR;
}

USB_TYPE USBHost::controlWrite(USBDeviceConnected* dev, uint8_t requestType, uint8_t request, uint32_t value, uint32_t index, uint8_t * buf, uint32_t len) {
    SETUP_PACKET setup = {requestType, request, value, index};
    int result = ControlWrite(dev, &setup, buf, len);
    if (result >= 0) {
        return USB_TYPE_OK;
    }
    USB_DBG("result=%d %02x", result, LastStatus);
    USB_DBG_HEX(buf, len);
    return USB_TYPE_ERROR;
}

USB_TYPE USBHost::bulkRead(USBDeviceConnected* dev, USBEndpoint* ep, uint8_t* buf, uint32_t len, bool blocking) {
    if (blocking == false) {
        ep->setBuffer(buf, len);
        ep_queue.push(ep);
        return USB_TYPE_PROCESSING;
    }
    int result = bulkReadBLOCK(ep, buf, len, -1);
    if (result >= 0) {
        return USB_TYPE_OK;
    }
    //USB_DBG2("result=%d %02x", result, host->LastStatus);
    return USB_TYPE_ERROR;
}

USB_TYPE USBHost::bulkWrite(USBDeviceConnected* dev, USBEndpoint* ep, uint8_t* buf, uint32_t len, bool blocking) {
    USB_TEST_ASSERT(blocking);
    int result = bulkWriteNB(ep, buf, len);
    if (result >= 0) {
        return USB_TYPE_OK;
    }
    USB_DBG2("result=%d %02x", result, LastStatus);
    return USB_TYPE_ERROR;
}

USB_TYPE USBHost::interruptRead(USBDeviceConnected* dev, USBEndpoint* ep, uint8_t* buf, uint32_t len, bool blocking) {
    if (blocking == false) {
        ep->setBuffer(buf, len);
        ep_queue.push(ep);
        return USB_TYPE_PROCESSING;
    }
    interruptReadNB(ep, buf, len);
    return USB_TYPE_OK;
}

USB_TYPE USBHost::interruptWrite(USBDeviceConnected* dev, USBEndpoint* ep, uint8_t* buf, uint32_t len, bool blocking) {
    USB_TEST_ASSERT(blocking);
    interruptWriteNB(ep, buf, len);
    return USB_TYPE_OK;
}

USB_TYPE USBHost::isochronousRead(USBDeviceConnected* dev, USBEndpoint* ep, uint8_t* buf, uint32_t len, bool blocking) {
    if (blocking == false) {
        ep->setBuffer(buf, len);
        ep_queue.push(ep);
        return USB_TYPE_PROCESSING;
    }
    isochronousReadNB(ep, buf, len);
    return USB_TYPE_OK;
}

int USBHost::ControlRead(USBDeviceConnected* dev, SETUP_PACKET* setup, uint8_t* data, int size) {
    USB_TEST_ASSERT(dev);
    USBEndpoint* ep = dev->getEpCtl();
    USB_TEST_ASSERT(ep);
    setAddr(dev->getAddress(), dev->getSpeed());
    token_setup(ep, setup, size); // setup stage
    if (LastStatus != ACK) {
        USB_DBG("setup %02x", LastStatus);
        return -1;
    }
    int read_len = 0;
    while(read_len < size) {
        int size2 = std::min(size-read_len, ep->getSize());
        int result = token_in(ep, data+read_len, size2);
        //USB_DBG("token_in result=%d %02x", result, LastStatus);
        if (result < 0) {
            USB_DBG("token_in %d/%d %02x", read_len, size, LastStatus);
            return result;
        }
        read_len += result;
        if (result < ep->getSize()) {
            break;
        }
    }    
    ep->setData01(DATA1);
    int result = token_out(ep); // status stage
    if (result < 0) {
        USB_DBG("status token_out %02x", LastStatus);
        if (LastStatus == STALL) {
            ep->setLengthTransferred(read_len);
            return read_len;
        }
        return result;
    }
    ep->setLengthTransferred(read_len);
    return read_len;
}

int USBHost::ControlWrite(USBDeviceConnected* dev, SETUP_PACKET* setup, uint8_t* data, int size) {
    USB_TEST_ASSERT(dev);
    USBEndpoint* ep = dev->getEpCtl();
    USB_TEST_ASSERT(ep);
    setAddr(dev->getAddress(), dev->getSpeed());
    token_setup(ep, setup, size); // setup stage
    if (LastStatus != ACK) {
        USB_DBG("setup %02x", LastStatus);
        return -1;
    }
    int write_len = 0;
    if (data != NULL) {
        write_len = token_out(ep, data, size);
        if (write_len < 0) {
            return -1;
        }
    }
    ep->setData01(DATA1);
    int result = token_in(ep); // status stage
    if (result < 0) {
        USB_DBG("result=%d %02x", result, LastStatus);
        //return result;
    }
    ep->setLengthTransferred(write_len);
    return write_len;
}

int USBHost::interruptReadNB(USBEndpoint* ep, uint8_t* data, int size)
{
    USB_TEST_ASSERT(ep);
    USBDeviceConnected* dev = ep->getDevice();
    USB_TEST_ASSERT(dev);
    setAddr(dev->getAddress(), dev->getSpeed());
    setEndpoint();
    const int retryLimit = 0;
    int read_len = 0;
    for(int n = 0; read_len < size; n++) {
        int size2 = std::min(size-read_len, ep->getSize());
        int result = token_in(ep, data+read_len, size2, retryLimit);
        if (result < 0) {
            if (LastStatus == NAK) {
                if (n == 0) {
                    return -1;
                }
                break;
            }
            //USB_DBG("token_in result=%d %02x", result, LastStatus);
            return result;
        }
        read_len += result;
        if (result < ep->getSize()) {
            break;
        }
    }
    ep->setLengthTransferred(read_len);
    return read_len;
}

int USBHost::interruptWriteNB(USBEndpoint* ep, const uint8_t* data, int size)
{
    USB_TEST_ASSERT(ep);
    USBDeviceConnected* dev = ep->getDevice();
    USB_TEST_ASSERT(dev);
    setAddr(dev->getAddress(), dev->getSpeed());
    setEndpoint();
    const int retryLimit = 0;
    int transferred_len = 0;
    for(int n = 0; transferred_len < size; n++) {
        int size2 = std::min(size-transferred_len, ep->getSize());
        int result = token_out(ep, data+transferred_len, size2, retryLimit);
        if (result < 0) {
            if (LastStatus == NAK) {
                if (n == 0) {
                    return -1;
                }
                break;
            }
            //USB_DBG("token_in result=%d %02x", result, LastStatus);
            return result;
        }
        transferred_len += result;
        if (result < ep->getSize()) {
            break;
        }
    }
    ep->setLengthTransferred(transferred_len);
    return transferred_len;
}

int USBHost::bulkReadNB(USBEndpoint* ep, uint8_t* data, int size)
{
    return bulkReadBLOCK(ep, data, size, 0);
}

int USBHost::bulkReadBLOCK(USBEndpoint* ep, uint8_t* data, int size, int timeout_ms) {
    USB_TEST_ASSERT(ep);
    USBDeviceConnected* dev = ep->getDevice();
    USB_TEST_ASSERT(dev);
    setAddr(dev->getAddress());
    setEndpoint();
    int retryLimit = (timeout_ms == 0) ? 0 : 10;
    int read_len = 0;
    Timer t;
    for(int n = 0; read_len < size; n++) {
        int size2 = std::min(size-read_len, ep->getSize());
        int result = token_in(ep, data+read_len, size2, retryLimit);
        if (result < 0) {
            if (LastStatus == NAK) {
                if (n == 0) {
                    return -1;
                }
                break;
            }
            //USB_DBG("token_in result=%d %02x", result, LastStatus);
            return result;
        }
        read_len += result;
        if (result < ep->getSize()) {
            break;
        }
        if (timeout_ms > 0 && t.read_ms() > timeout_ms) {
            USB_DBG("timeout_ms: %d", timeout_ms);
            break;
        }
    }
    ep->setLengthTransferred(read_len);
    return read_len;
}

int USBHost::bulkWriteNB(USBEndpoint* ep, const uint8_t* data, int size) {
    USB_TEST_ASSERT(ep);
    USBDeviceConnected* dev = ep->getDevice();
    USB_TEST_ASSERT(dev);
    setAddr(dev->getAddress());
    setEndpoint();
    int write_len = 0;
    for(int n = 0; write_len < size; n++) {
        int size2 = std::min(size-write_len, ep->getSize());
        int result = token_out(ep, data+write_len, size2);
        if (result < 0) {
            if (LastStatus == NAK) {
                if (n == 0) {
                    return -1;
                }
                break;
            }
            USB_DBG("token_out result=%d %02x", result, LastStatus);
            return result;
        }
        write_len += result;
        if (result < ep->getSize()) {
            break;
        }
    }
    ep->setLengthTransferred(write_len);
    return write_len;
}

int USBHost::isochronousReadNB(USBEndpoint* ep, uint8_t* data, int size) {
    USBDeviceConnected* dev = ep->getDevice();
    USB_TEST_ASSERT(dev);
    setAddr(dev->getAddress());
    int result = token_iso_in(ep, data, size);
    if (result >= 0) {
         ep->setLengthTransferred(result);
    }
    return result;
}

void USBHost::task()
{
    if (ep_queue.empty()) {
        return;
    }
    USBEndpoint* ep = ep_queue.pop();
    USB_TEST_ASSERT(ep);
    ep->setLengthTransferred(0);
    switch(ep->getType()) {
        case INTERRUPT_ENDPOINT:
            if (ep->getDir() == IN) {
                interruptReadNB(ep, ep->getBufStart(), ep->getBufSize());
            }
            break;
        case BULK_ENDPOINT:
            if (ep->getDir() == IN) {
                bulkReadNB(ep, ep->getBufStart(), ep->getBufSize());
            }
            break;
        case ISOCHRONOUS_ENDPOINT:
            if (ep->getDir() == IN) {
                isochronousReadNB(ep, ep->getBufStart(), ep->getBufSize());
            }
            break;
    }
    ep->call();
}
