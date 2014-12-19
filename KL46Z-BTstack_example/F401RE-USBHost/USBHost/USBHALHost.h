#if defined(TARGET_NUCLEO_F401RE)
#include "USBHALHost_F401RE.h"
#elif defined(TARGET_KL46Z)||defined(TARGET_KL25Z)
#include "USBHALHost_KL46Z.h"
#else
#error "target error"
#endif


