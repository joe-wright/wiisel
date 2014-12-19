#ifndef BD_ADDR_H
#define BD_ADDR_H
class bd_addr {
public:
    bd_addr();
    bd_addr(char* s);
    uint8_t* data(uint8_t* addr = NULL);
    char* to_str();
private:
    char ad_str[18];
    uint8_t ad[6];
};
#endif //BD_ADDR_H
