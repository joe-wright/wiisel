#include "mbed.h"
#include "bd_addr.h"
bd_addr::bd_addr()
{

}

bd_addr::bd_addr(char* s)
{
    for(int i = 0; i <= 5; i++) {
        ad[i] = strtol(s+i*3, NULL, 16);
    }
}

uint8_t* bd_addr::data(uint8_t* addr)
{
    if (addr != NULL) {
        ad[5] = addr[0];
        ad[4] = addr[1];
        ad[3] = addr[2];
        ad[2] = addr[3];
        ad[1] = addr[4];
        ad[0] = addr[5];
    }
    return ad;
}

char* bd_addr::to_str()
{
    snprintf(ad_str, sizeof(ad_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
        ad[0], ad[1], ad[2], ad[3], ad[4], ad[5]);
    return ad_str;
}
