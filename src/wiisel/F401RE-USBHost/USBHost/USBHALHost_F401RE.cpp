// Simple USBHost for Nucleo F401RE
#if defined(TARGET_NUCLEO_F401RE)
#include "USBHALHost_F401RE.h"

template <bool>struct CtAssert;
template <>struct CtAssert<true> {};
#define CTASSERT(A) CtAssert<A>();


#ifdef _USB_DBG
extern RawSerial pc;
#include "mydebug.h"
#define USB_DBG(...) do{pc.printf("[%s@%d] ",__PRETTY_FUNCTION__,__LINE__);pc.printf(__VA_ARGS__);pc.puts("\n");} while(0);
#define USB_DBG_HEX(A,B) debug_hex<RawSerial>(pc,A,B)

#else
#define USB_DBG(...) while(0)
#define USB_DBG_HEX(A,B) while(0)
#endif

#ifdef _USB_TEST
#define USB_TEST_ASSERT(A) while(!(A)){fprintf(stderr,"\n\n%s@%d %s ASSERT!\n\n",__PRETTY_FUNCTION__,__LINE__,#A);exit(1);};
#define USB_TEST_ASSERT_FALSE(A) USB_TEST_ASSERT(!(A))
#else
#define USB_TEST_ASSERT(A) while(0)
#define USB_TEST_ASSERT_FALSE(A) while(0)
#endif

#ifdef _USB_TRACE
#define USB_TRACE() while(0)
#define USB_TRACE1(A) while(0)
#define USB_TRACE_VIEW() while(0)
#define USB_TRACE_CLEAR() while(0)
#else
#define USB_TRACE() while(0)
#define USB_TRACE1(A) while(0)
#define USB_TRACE_VIEW() while(0)
#define USB_TRACE_CLEAR() while(0)
#endif

#define USB_INFO(...) do{fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");}while(0);

__IO bool attach_done = false;
__IO bool token_done = false;
__IO HCD_URBStateTypeDef g_urb_state = URB_IDLE;

void delay_ms(uint32_t t)
{
    HAL_Delay(t);
}

// from usbh_conf.c
extern HCD_HandleTypeDef hhcd_USB_OTG_FS;

void HAL_HCD_MspInit(HCD_HandleTypeDef* hhcd)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  if(hhcd->Instance==USB_OTG_FS)
  {
    /* Peripheral clock enable */
    __USB_OTG_FS_CLK_ENABLE();
  
    /**USB_OTG_FS GPIO Configuration    
    PA11     ------> USB_OTG_FS_DM
    PA12     ------> USB_OTG_FS_DP 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Peripheral interrupt init*/
    /* Sets the priority grouping field */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  }
}

// from stm32f4xx_it.c
extern "C" {
void HAL_HCD_Connect_Callback(HCD_HandleTypeDef *hhcd)
{
    USB_TRACE();
    attach_done = true;
}

void HAL_HCD_Disconnect_Callback(HCD_HandleTypeDef *hhcd)
{
    USB_TRACE();
}

void HAL_HCD_HC_NotifyURBChange_Callback(HCD_HandleTypeDef *hhcd, uint8_t chnum, HCD_URBStateTypeDef urb_state)
{
    USB_TRACE1(chnum);
    USB_TRACE1(urb_state);
    g_urb_state = urb_state;
    token_done = true;
}

} // extern "C"

const int CH_CTL_IN  = 0;
const int CH_CTL_OUT = 1;
const int CH_INT_IN  = 2;
const int CH_INT_OUT = 3;
const int CH_BLK_IN  = 4;
const int CH_BLK_OUT = 5;
const int CH_ISO_IN  = 6;
const int DIR_IN  = 1;
const int DIR_OUT = 0;



USBHALHost* USBHALHost::instHost;

USBHALHost::USBHALHost() {
    instHost = this;
}

void USBHALHost::init() {
    hhcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hhcd_USB_OTG_FS.Init.Host_channels = 8;
    hhcd_USB_OTG_FS.Init.speed = HCD_SPEED_FULL;
    hhcd_USB_OTG_FS.Init.dma_enable = DISABLE;
    hhcd_USB_OTG_FS.Init.phy_itface = HCD_PHY_EMBEDDED;
    hhcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
    hhcd_USB_OTG_FS.Init.low_power_enable = ENABLE;
    hhcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hhcd_USB_OTG_FS.Init.use_external_vbus = DISABLE;

    HAL_HCD_Init(&hhcd_USB_OTG_FS);
    HAL_HCD_Start(&hhcd_USB_OTG_FS);

    bool lowSpeed = wait_attach();
    delay_ms(200);
    HAL_HCD_ResetPort(&hhcd_USB_OTG_FS);
    delay_ms(100); // Wait for 100 ms after Reset
    addDevice(NULL, 0, lowSpeed);
}

bool USBHALHost::wait_attach() {
    Timer t;
    t.reset();
    t.start();
    while(!attach_done) {
        if (t.read_ms() > 5*1000) {
            t.reset();
            USB_INFO("Please attach USB device.");
        }
    }
    wait_ms(100);
    return HAL_HCD_GetCurrentSpeed(&hhcd_USB_OTG_FS) == USB_OTG_SPEED_LOW;
}

int USBHALHost::token_setup(USBEndpoint* ep, SETUP_PACKET* setup, uint16_t wLength) {
    USBDeviceConnected* dev = ep->getDevice();

    HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_CTL_OUT, 0x00, 
        dev->getAddress(),
        dev->getSpeed() ? HCD_SPEED_LOW : HCD_SPEED_FULL,
        EP_TYPE_CTRL, ep->getSize());

    setup->wLength = wLength;

    HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_CTL_OUT, DIR_OUT, EP_TYPE_CTRL, 0, (uint8_t*)setup, 8, 0);
    while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_OUT) == URB_IDLE);

    switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_OUT)) {
        case URB_DONE:
            LastStatus = ACK;
            break;
        default:
            LastStatus = 0xff;
            break;
    }
    ep->setData01(DATA1);
    return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_CTL_OUT);
}

HAL_StatusTypeDef HAL_HCD_HC_SubmitRequest2(HCD_HandleTypeDef *hhcd,
                                            uint8_t ch_num, 
                                            uint8_t direction ,
                                            uint8_t ep_type,  
                                            uint8_t token, 
                                            uint8_t* pbuff, 
                                            uint16_t length,
                                            uint8_t do_ping) 
{
    HCD_HCTypeDef* hc = &(hhcd->hc[ch_num]);
    hc->ep_is_in = direction;
    hc->ep_type  = ep_type; 

    if (hc->toggle_in == 0) {
        hc->data_pid = HC_PID_DATA0;
    } else {
        hc->data_pid = HC_PID_DATA1;
   }

  hc->xfer_buff = pbuff;
  hc->xfer_len  = length;
  hc->urb_state = URB_IDLE;
  hc->xfer_count = 0 ;
  hc->ch_num = ch_num;
  hc->state = HC_IDLE;

  return USB_HC_StartXfer(hhcd->Instance, hc, hhcd->Init.dma_enable);
}

int USBHALHost::token_in(USBEndpoint* ep, uint8_t* data, int size, int retryLimit) {
    switch(ep->getType()) {
        case CONTROL_ENDPOINT:
            return token_ctl_in(ep, data, size, retryLimit);
        case INTERRUPT_ENDPOINT:
            return token_int_in(ep, data, size);
        case BULK_ENDPOINT:
            return token_blk_in(ep, data, size, retryLimit);
    }
    return -1;
}

int USBHALHost::token_ctl_in(USBEndpoint* ep, uint8_t* data, int size, int retryLimit) {
    USBDeviceConnected* dev = ep->getDevice();
    HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_CTL_IN, 0x80, 
        dev->getAddress(),
        dev->getSpeed() ? HCD_SPEED_LOW : HCD_SPEED_FULL,
        EP_TYPE_CTRL, ep->getSize());
    hhcd_USB_OTG_FS.hc[CH_CTL_IN].toggle_in = (ep->getData01() == DATA0) ? 0 : 1;

    HAL_HCD_HC_SubmitRequest2(&hhcd_USB_OTG_FS, CH_CTL_IN, DIR_IN, EP_TYPE_CTRL, 1, data, size, 0);
    while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_IN) == URB_IDLE);

    switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_IN)) {
        case URB_DONE:
            LastStatus = ACK;
            break;
        default:
            LastStatus = 0xff;
            return -1;
    }
    ep->toggleData01();
    return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_CTL_IN);
}

int USBHALHost::token_int_in(USBEndpoint* ep, uint8_t* data, int size) {
    USBDeviceConnected* dev = ep->getDevice();
    HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_INT_IN,
        ep->getAddress(),
        dev->getAddress(),
        dev->getSpeed() ? HCD_SPEED_LOW : HCD_SPEED_FULL,
        EP_TYPE_INTR, ep->getSize());
    hhcd_USB_OTG_FS.hc[CH_INT_IN].toggle_in = (ep->getData01() == DATA0) ? 0 : 1;

    HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_INT_IN, DIR_IN, EP_TYPE_INTR, 1, data, size, 0);
    while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_INT_IN) == URB_IDLE);
    switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_INT_IN)) {
        case URB_DONE:
            switch(HAL_HCD_HC_GetState(&hhcd_USB_OTG_FS, CH_INT_IN)) {
                case HC_XFRC:
                    LastStatus = ep->getData01();
                    ep->toggleData01();
                    return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_INT_IN);
                case HC_NAK:
                    LastStatus = NAK;
                    return -1;
            }
            break;
    }
    LastStatus = STALL;
    return -1;
}

int USBHALHost::token_blk_in(USBEndpoint* ep, uint8_t* data, int size, int retryLimit) {
    USBDeviceConnected* dev = ep->getDevice();
    int retry = 0;
    do {
        HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_BLK_IN,
            ep->getAddress(),
            dev->getAddress(),
            HCD_SPEED_FULL,
            EP_TYPE_BULK, ep->getSize());
        hhcd_USB_OTG_FS.hc[CH_BLK_IN].toggle_in = (ep->getData01() == DATA0) ? 0 : 1;

        HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_BLK_IN, DIR_IN, EP_TYPE_BULK, 1, data, size, 0);
        while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_BLK_IN) == URB_IDLE);

        switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_BLK_IN)) {
            case URB_DONE:
                switch(HAL_HCD_HC_GetState(&hhcd_USB_OTG_FS, CH_BLK_IN)) {
                    case HC_XFRC:
                        LastStatus = ep->getData01();
                        ep->toggleData01();
                        return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_BLK_IN);
                    case HC_NAK:
                        LastStatus = NAK;
                        if (retryLimit > 0) {
                            delay_ms(1 + 10 * retry);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case URB_STALL:
                LastStatus = STALL;
                return -1;
            default:
                LastStatus = STALL;
                delay_ms(500 + 100 * retry);
                break;
        }
    }while(retry++ < retryLimit);
    return -1;
}

int USBHALHost::token_out(USBEndpoint* ep, const uint8_t* data, int size, int retryLimit) {
    switch(ep->getType()) {
        case CONTROL_ENDPOINT:
            return token_ctl_out(ep, data, size, retryLimit);
        case INTERRUPT_ENDPOINT:
            return token_int_out(ep, data, size);
        case BULK_ENDPOINT:
            return token_blk_out(ep, data, size, retryLimit);
    }
    return -1;
}

int USBHALHost::token_ctl_out(USBEndpoint* ep, const uint8_t* data, int size, int retryLimit) {
    USBDeviceConnected* dev = ep->getDevice();
        HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_CTL_OUT, 0x00, 
            dev->getAddress(),
            dev->getSpeed() ? HCD_SPEED_LOW : HCD_SPEED_FULL,
            EP_TYPE_CTRL, ep->getSize());
            hhcd_USB_OTG_FS.hc[CH_CTL_OUT].toggle_out = (ep->getData01() == DATA0) ? 0 : 1;

    do {
        HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_CTL_OUT, DIR_OUT, EP_TYPE_CTRL, 1, (uint8_t*)data, size, 0);
        while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_OUT) == URB_IDLE);

        switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_CTL_OUT)) {
            case URB_DONE:
                LastStatus = ACK;
                ep->toggleData01();
                return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_CTL_OUT);

            default:
                break;
        }
        delay_ms(1);
    }while(retryLimit-- > 0); 
    return -1;
}

int USBHALHost::token_int_out(USBEndpoint* ep, const uint8_t* data, int size) {
    USBDeviceConnected* dev = ep->getDevice();
    HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_INT_OUT,
        ep->getAddress(),
        dev->getAddress(),
        dev->getSpeed() ? HCD_SPEED_LOW : HCD_SPEED_FULL,
        EP_TYPE_INTR, ep->getSize());
    hhcd_USB_OTG_FS.hc[CH_INT_OUT].toggle_out = (ep->getData01() == DATA0) ? 0 : 1;

    token_done = false;
    HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_INT_OUT, DIR_OUT, EP_TYPE_INTR, 1, (uint8_t*)data, size, 0);
    while(!token_done);

    if (HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_INT_OUT) != URB_DONE) {
        return -1;
    }
    ep->toggleData01();
    return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_INT_OUT);
}

int USBHALHost::token_blk_out(USBEndpoint* ep, const uint8_t* data, int size, int retryLimit) {
    USBDeviceConnected* dev = ep->getDevice();
    int retry = 0;
    do {
        HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_BLK_OUT,
            ep->getAddress(), dev->getAddress(),
            HCD_SPEED_FULL, EP_TYPE_BULK, ep->getSize());
        hhcd_USB_OTG_FS.hc[CH_BLK_OUT].toggle_out = (ep->getData01() == DATA0) ? 0 : 1;

        HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_BLK_OUT, DIR_OUT, EP_TYPE_BULK, 1, (uint8_t*)data, size, 0);
        while(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_BLK_OUT) == URB_IDLE);

        switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_BLK_OUT)) {
            case URB_DONE:
                switch(HAL_HCD_HC_GetState(&hhcd_USB_OTG_FS, CH_BLK_OUT)) {
                    case HC_XFRC: // ACK
                        LastStatus = ep->getData01();
                        ep->toggleData01();
                        return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_BLK_OUT);
                    default:
                        break;
                }
                break;
            case URB_NOTREADY: // HC_NAK
                LastStatus = NAK;
                delay_ms(100 * retry);
                break;
            default:
                LastStatus = STALL;
                return -1;
        }
    } while(retry++ < retryLimit);
    return -1;
}

int USBHALHost::token_iso_in(USBEndpoint* ep, uint8_t* data, int size) {
    USBDeviceConnected* dev = ep->getDevice();
    HAL_HCD_HC_Init(&hhcd_USB_OTG_FS, CH_ISO_IN,
        ep->getAddress(),
        dev->getAddress(),
        HCD_SPEED_FULL,
        EP_TYPE_ISOC, ep->getSize());

    token_done = false;
    HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_FS, CH_ISO_IN, DIR_IN, EP_TYPE_ISOC, 1, data, size, 0);
    while(!token_done);

    switch(HAL_HCD_HC_GetURBState(&hhcd_USB_OTG_FS, CH_ISO_IN)) {
        case URB_DONE:
    return HAL_HCD_HC_GetXferCount(&hhcd_USB_OTG_FS, CH_ISO_IN);

        default:
            return -1;
    }
}

#endif


