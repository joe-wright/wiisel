// 800 KHz WS2811 driver driving potentially many LED strings.
// Uses 3-phase DMA
// 16K SRAM less stack, etc.
//
// Per LED: 3 bytes (malloc'd) for RGB data
//
// Per LED strip / per LED
//          96 bytes (static) for bit data
//        + 96 bytes (static) for ones data
//        = 192 bytes
//
//        40 LEDs max per string = 7680 bytes static
//
//        40 LEDs: 7680 + 40*3 = 7800 bytes
//        80 LEDs: 7680 + 80*3 = 7920 bytes

#include "MKL25Z4.h"
#include "LedStrip.h"
#include "WS2811.h"

//
// Added by Wiisel
//
#define ONES 0xFFFFFFFF
#define TEN_ONES ONES, ONES, ONES, ONES, ONES, ONES, ONES, ONES, ONES, ONES
#define FIFTY_ONES TEN_ONES, TEN_ONES, TEN_ONES, TEN_ONES, TEN_ONES
#define HUND_ONES FIFTY_ONES, FIFTY_ONES
#define FIVE_HUND_ONES HUND_ONES, HUND_ONES, HUND_ONES, HUND_ONES, HUND_ONES
#define THOU_ONES FIVE_HUND_ONES, FIVE_HUND_ONES

//
// Configuration
//

// Define MONITOR_TPM0_PWM as non-zero to monitor PWM timing on PTD0 and PTD1
// PTD0 TPM0/CH0 PWM_1 J2/06
// PTD1 TPM0/CH1 PWM_2 J2/12 (also LED_BLUE)
#define MONITOR_TPM0_PWM 0

// define DEBUG_PIN to identify a pin in PORTD used for debug output
// #define DEBUG_PIN 4 /* PTD4 debugOut */

#ifdef DEBUG_PIN
#define DEBUG 1
#endif

#if DEBUG
#define DEBUG_MASK (1<<DEBUG_PIN)
#define RESET_DEBUG (IO_GPIO->PDOR &= ~DEBUG_MASK)
#define SET_DEBUG (IO_GPIO->PDOR |= DEBUG_MASK)
#else
#define DEBUG_MASK 0
#define RESET_DEBUG (void)0
#define SET_DEBUG (void)0
#endif

//
// Modified by Wiisel
//
static PORT_Type volatile * const IO_PORT = PORTC; // Also change correct port in clock_init();
static GPIO_Type volatile * const IO_GPIO = PTC;

// 48 MHz clock, no prescaling.
#define NSEC_TO_TICKS(nsec) ((nsec)*48/1000)
#define USEC_TO_TICKS(usec) ((usec)*48)
static const uint32_t CLK_NSEC = 1250;
static const uint32_t tpm_period    = NSEC_TO_TICKS(CLK_NSEC);
static const uint32_t tpm_p0_period = NSEC_TO_TICKS(300);
static const uint32_t tpm_p1_period = NSEC_TO_TICKS(800);
static const uint32_t guardtime_period = USEC_TO_TICKS(55);   // guardtime minimum 50 usec.

enum DMA_MUX_SRC {
    DMA_MUX_SRC_TPM0_CH_0     = 24,
    DMA_MUX_SRC_TPM0_CH_1,
    DMA_MUX_SRC_TPM0_Overflow = 54,
};

enum DMA_CHAN {
    DMA_CHAN_START = 0,
    DMA_CHAN_0_LOW = 1,
    DMA_CHAN_1_LOW = 2,
    N_DMA_CHANNELS
};

volatile bool WS2811::dma_done = true;

// class static
bool WS2811::initialized = false;

// class static
uint32_t WS2811::enabledPins = 0;

#define WORD_ALIGNED __attribute__ ((aligned(4)))

#define DMA_LEADING_ZEROS  4
#define BITS_PER_RGB       24
#define DMA_TRAILING_ZEROS 1

static struct {
    uint32_t start_t1_low[ DMA_LEADING_ZEROS ];
    uint32_t dmaWords[ BITS_PER_RGB * MAX_LEDS_PER_STRIP ];
    uint32_t trailing_zeros_1[ DMA_TRAILING_ZEROS ];
} dmaData WORD_ALIGNED;

//
// Heavily modified by Wiisel.
//
const uint32_t dmaOnes[] = {0, 0, 0, 0, THOU_ONES, THOU_ONES, HUND_ONES, FIFTY_ONES, TEN_ONES, 0};

// class static
void WS2811::hw_init()
{
    if (initialized) return;

    dma_data_init();
    clock_init();
    dma_init();
    io_init();
    tpm_init();

    initialized = true;

    SET_DEBUG;
    RESET_DEBUG;
}

//
// Modified by Wiisel
//
// class static
void WS2811::dma_data_init()
{
#if DEBUG
    for (unsigned i = 0; i < BITS_PER_RGB * MAX_LEDS_PER_STRIP; i++)
        dmaData.dmaWords[i] = DEBUG_MASK;
#endif
}

//
// Modified by Wiisel
//
// class static
// Enable PORTC, DMA and TPM0 clocking
void WS2811::clock_init()
{
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX_MASK | SIM_SCGC6_TPM0_MASK; // Enable clock to DMA mux and TPM0
    SIM->SCGC7 |= SIM_SCGC7_DMA_MASK;  // Enable clock to DMA

    SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1); // Clock source: MCGFLLCLK or MCGPLLCLK
}

// class static
// Configure GPIO output pins
void WS2811::io_init()
{
    uint32_t m = 1;
    for (uint32_t i = 0; i < 32; i++) {
        // set up each pin
        if (m & enabledPins) {
            IO_PORT->PCR[i] = PORT_PCR_MUX(1) // GPIO
                              | PORT_PCR_DSE_MASK; // high drive strength
        }
        m <<= 1;
    }

    IO_GPIO->PDDR |= enabledPins;      // set as outputs

#if MONITOR_TPM0_PWM
    // PTD0 CH0 monitor: TPM0, high drive strength
    IO_PORT->PCR[0] = PORT_PCR_MUX(4) | PORT_PCR_DSE_MASK;
    // PTD1 CH1 monitor: TPM0, high drive strength
    IO_PORT->PCR[1] = PORT_PCR_MUX(4) | PORT_PCR_DSE_MASK;
    IO_GPIO->PDDR  |= 3;               // set as outputs
    IO_GPIO->PDOR &= ~(enabledPins | 3);     // initially low
#else
    IO_GPIO->PDOR &= ~enabledPins;     // initially low
#endif

#if DEBUG
    IO_PORT->PCR[DEBUG_PIN] = PORT_PCR_MUX(1) | PORT_PCR_DSE_MASK;
    IO_GPIO->PDDR |= DEBUG_MASK;
    IO_GPIO->PDOR &= ~DEBUG_MASK;
#endif
}

// class static

/// Configure DMA and DMAMUX
void WS2811::dma_init()
{
    // reset DMAMUX
    DMAMUX0->CHCFG[DMA_CHAN_START] = 0;
    DMAMUX0->CHCFG[DMA_CHAN_0_LOW] = 0;
    DMAMUX0->CHCFG[DMA_CHAN_1_LOW] = 0;

    // wire our DMA event sources into the first three DMA channels
    // t=0: all enabled outputs go high on TPM0 overflow
    DMAMUX0->CHCFG[DMA_CHAN_START] = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(DMA_MUX_SRC_TPM0_Overflow);
    // t=tpm_p0_period: all of the 0 bits go low.
    DMAMUX0->CHCFG[DMA_CHAN_0_LOW] = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(DMA_MUX_SRC_TPM0_CH_0);
    // t=tpm_p1_period: all outputs go low.
    DMAMUX0->CHCFG[DMA_CHAN_1_LOW] = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(DMA_MUX_SRC_TPM0_CH_1);

    NVIC_SetVector(DMA0_IRQn, (uint32_t)&DMA0_IRQHandler);
    NVIC_EnableIRQ(DMA0_IRQn);
}

// class static

/// Configure TPM0 to do two different PWM periods at 800kHz rate
void WS2811::tpm_init()
{
    // set up TPM0 for proper period (800 kHz = 1.25 usec ±600nsec)
    TPM_Type volatile *tpm = TPM0;
    tpm->SC = TPM_SC_DMA_MASK          // enable DMA
              | TPM_SC_TOF_MASK        // reset TOF flag if set
              | TPM_SC_CMOD(0)         // disable clocks
              | TPM_SC_PS(0);          // 48MHz / 1 = 48MHz clock
    tpm->MOD = tpm_period - 1;         // 48MHz / 800kHz

    // No Interrupts; High True pulses on Edge Aligned PWM
    tpm->CONTROLS[0].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK | TPM_CnSC_DMA_MASK;
    tpm->CONTROLS[1].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK | TPM_CnSC_DMA_MASK;

    // set TPM0 channel 0 for 0.35 usec (±150nsec) (0 code)
    // 1.25 usec * 1/3 = 417 nsec
    tpm->CONTROLS[0].CnV = tpm_p0_period;

    // set TPM0 channel 1 for 0.7 usec (±150nsec) (1 code)
    // 1.25 usec * 2/3 = 833 nsec
    tpm->CONTROLS[1].CnV = tpm_p1_period;

    NVIC_SetVector(TPM0_IRQn, (uint32_t)&TPM0_IRQHandler);
    NVIC_EnableIRQ(TPM0_IRQn);
}

WS2811::WS2811(unsigned n, unsigned pinNumber)
    : LedStrip(n)
    , pinMask(1U << pinNumber)
{
    enabledPins |= pinMask;
    initialized = false;
}

//
// Added by Wiisel
//
WS2811::WS2811()
    : LedStrip(0)
{
    initialized = false;
}


// class static
void WS2811::startDMA()
{
    hw_init();
    
    wait_for_dma_done();
    dma_done = false;

    DMA_Type volatile * dma   = DMA0;
    TPM_Type volatile *tpm   = TPM0;
    uint32_t nBytes = sizeof(dmaData.start_t1_low)
                      + sizeof(dmaData.dmaWords)
                      + sizeof(dmaData.trailing_zeros_1);

    tpm->SC = TPM_SC_DMA_MASK        // enable DMA
              | TPM_SC_TOF_MASK  // reset TOF flag if set
              | TPM_SC_CMOD(0)   // disable clocks
              | TPM_SC_PS(0);    // 48MHz / 1 = 48MHz clock
    tpm->MOD = tpm_period - 1;       // 48MHz / 800kHz

    tpm->CNT = tpm_p0_period - 2 ;
    tpm->STATUS = 0xFFFFFFFF;

    dma->DMA[DMA_CHAN_START].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status
    dma->DMA[DMA_CHAN_0_LOW].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status
    dma->DMA[DMA_CHAN_1_LOW].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status

    // t=0: all outputs go high
    // triggered by TPM0_Overflow
    // source is one word of 0 then 24 x 0xffffffff, then another 0 word
    dma->DMA[DMA_CHAN_START].SAR     = (uint32_t)(void*)dmaOnes;
    dma->DMA[DMA_CHAN_START].DSR_BCR = DMA_DSR_BCR_BCR_MASK & nBytes; // length of transfer in bytes

    // t=tpm_p0_period: some outputs (the 0 bits) go low.
    // Triggered by TPM0_CH0
    // Start 2 words before the actual data to avoid garbage pulses.
    dma->DMA[DMA_CHAN_0_LOW].SAR     = (uint32_t)(void*)dmaData.start_t1_low; // set source address
    dma->DMA[DMA_CHAN_0_LOW].DSR_BCR = DMA_DSR_BCR_BCR_MASK & nBytes; // length of transfer in bytes

    // t=tpm_p1_period: all outputs go low.
    // Triggered by TPM0_CH1
    // source is constant 0x00000000 (first word of dmaWords)
    dma->DMA[DMA_CHAN_1_LOW].SAR     = (uint32_t)(void*)dmaData.start_t1_low; // set source address
    dma->DMA[DMA_CHAN_1_LOW].DSR_BCR = DMA_DSR_BCR_BCR_MASK & nBytes; // length of transfer in bytes

    dma->DMA[DMA_CHAN_0_LOW].DAR
    = dma->DMA[DMA_CHAN_1_LOW].DAR
      = dma->DMA[DMA_CHAN_START].DAR
        = (uint32_t)(void*)&IO_GPIO->PDOR;

    SET_DEBUG;

    dma->DMA[DMA_CHAN_0_LOW].DCR     = DMA_DCR_EINT_MASK // enable interrupt on end of transfer
                                       | DMA_DCR_ERQ_MASK
                                       | DMA_DCR_D_REQ_MASK // clear ERQ on end of transfer
                                       | DMA_DCR_SINC_MASK // increment source each transfer
                                       | DMA_DCR_CS_MASK
                                       | DMA_DCR_SSIZE(0) // 32-bit source transfers
                                       | DMA_DCR_DSIZE(0); // 32-bit destination transfers

    dma->DMA[DMA_CHAN_1_LOW].DCR     = DMA_DCR_EINT_MASK // enable interrupt on end of transfer
                                       | DMA_DCR_ERQ_MASK
                                       | DMA_DCR_D_REQ_MASK // clear ERQ on end of transfer
                                       | DMA_DCR_CS_MASK
                                       | DMA_DCR_SSIZE(0) // 32-bit source transfers
                                       | DMA_DCR_DSIZE(0); // 32-bit destination transfers

    dma->DMA[DMA_CHAN_START].DCR     = DMA_DCR_EINT_MASK // enable interrupt on end of transfer
                                       | DMA_DCR_ERQ_MASK
                                       | DMA_DCR_D_REQ_MASK // clear ERQ on end of transfer
                                       | DMA_DCR_SINC_MASK // increment source each transfer
                                       | DMA_DCR_CS_MASK
                                       | DMA_DCR_SSIZE(0) // 32-bit source transfers
                                       | DMA_DCR_DSIZE(0);

    tpm->SC |= TPM_SC_CMOD(1);         // enable internal clocking
}

//
// Added by Wiisel
//
void WS2811::setPixelUncompressed(uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *dest = dmaData.dmaWords + n * BITS_PER_RGB;
    writeByte(g, pinMask, dest + 0); // R
    writeByte(r, pinMask, dest + 8); // G
    writeByte(b, pinMask, dest + 16); // B
}

//
// Modified by Wiisel
//
void WS2811::writePixel(unsigned n, uint16_t *p)
{
    uint32_t *dest = dmaData.dmaWords + n * BITS_PER_RGB;
    uint8_t r, g, b;
    LedStrip::uncompress(*p, &r, &g, &b);
    writeByte(g, pinMask, dest + 0); // R
    writeByte(r, pinMask, dest + 8); // G
    writeByte(b, pinMask, dest + 16); // B
}

// class static
void WS2811::writeByte(uint8_t byte, uint32_t mask, uint32_t *dest)
{
    for (uint8_t bm = 0x80; bm; bm >>= 1) {
        // MSBit first
        if (byte & bm)
            *dest |= mask;
        else
            *dest &= ~mask;
        dest++;
    }
}

void WS2811::begin()
{
    blank();
    show();
}

void WS2811::blank()
{
    memset(pixels, 0x00, numPixelBytes());

#if DEBUG
    for (unsigned i = DMA_LEADING_ZEROS; i < DMA_LEADING_ZEROS + BITS_PER_RGB; i++)
        dmaData.dmaWords[i] = DEBUG_MASK;
#else
    memset(dmaData.dmaWords, 0x00, sizeof(dmaData.dmaWords));
#endif
}

//
// Modified by Wiisel
//
void WS2811::show()
{

    uint16_t i, n = numPixels(); // 3 bytes per LED
    uint16_t *p = pixels;

    for (i=0; i<n; i++ ) {
        if (i < 30) {
            writePixel(i, p);
        } else {
            writePixel(30 + 2*(i-30), p);
        }
        p++;
    }
}

extern "C" void DMA0_IRQHandler()
{
    DMA_Type volatile *dma = DMA0;
    TPM_Type volatile *tpm = TPM0;

    uint32_t db;

    db = dma->DMA[DMA_CHAN_0_LOW].DSR_BCR;
    if (db & DMA_DSR_BCR_DONE_MASK) {
        dma->DMA[DMA_CHAN_0_LOW].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status
    }

    db = dma->DMA[DMA_CHAN_1_LOW].DSR_BCR;
    if (db & DMA_DSR_BCR_DONE_MASK) {
        dma->DMA[DMA_CHAN_1_LOW].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status
    }

    db = dma->DMA[DMA_CHAN_START].DSR_BCR;
    if (db & DMA_DSR_BCR_DONE_MASK) {
        dma->DMA[DMA_CHAN_START].DSR_BCR = DMA_DSR_BCR_DONE_MASK; // clear/reset DMA status
    }

    tpm->SC = TPM_SC_TOF_MASK;  // reset TOF flag; disable internal clocking

    SET_DEBUG;

    // set TPM0 to interrrupt after guardtime
    tpm->MOD = guardtime_period - 1; // 48MHz * 55 usec
    tpm->CNT = 0;
    tpm->SC  = TPM_SC_PS(0)        // 48MHz / 1 = 48MHz clock
               | TPM_SC_TOIE_MASK  // enable interrupts
               | TPM_SC_CMOD(1);   // and internal clocking
}

extern "C" void TPM0_IRQHandler()
{
    TPM0->SC = 0; // disable internal clocking
    TPM0->SC = TPM_SC_TOF_MASK;        
    RESET_DEBUG;
    WS2811::dma_done = true;
}
