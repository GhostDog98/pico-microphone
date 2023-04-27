#include <stdio.h>
#include "pico/stdlib.h"
// For ADC input:
#include "hardware/adc.h"
#include "hardware/dma.h"

// For SD Card
#include "f_util.h"
#include "ff.h"
#include "rtc.h"

#include "hw_config.h"


// Channel 2 is GPIO28
#define CAPTURE_CHANNEL 2
#define CAPTURE_DEPTH 100000


#define DEBUG
#ifdef DEBUG
    #define DEBUG_PRINT(...) printf("DEBUG: " __VA_ARGS__)
#else
    #define DEBUG_PRINT(...) do {} while (0)
#endif


uint8_t capture_buf[CAPTURE_DEPTH]; // Buffer for storing our results
uint8_t capture_buf_tmp[CAPTURE_DEPTH]; // Second buffer

volatile bool currentbuf = true;
volatile bool computeComplete = false;

int dma_chan = -1; // Since we can't ever get a -1 from our dma allocation function, this serves as a way to sanity check it is being assigned
                   // as otherwise global variables get assigned to zero, which would be the same as our function can return

FATFS fs;
FIL fil;
char filename[] = "data.txt";




void configureDMA(bool currentbuf);

void dma_irq_handler(){
    DEBUG_PRINT("handler triggered, value of computeComplete is %d and currentbuf is %d\n", computeComplete, currentbuf);
    dma_hw->ints0 = (1u << dma_chan);
    if(computeComplete){
        //iteration++;
        currentbuf = !currentbuf;
        computeComplete = false;
    }
    configureDMA(currentbuf);
}


void configureDMA(bool currentbuf){
    DEBUG_PRINT("configure was called with currentbuf being %d, dma_chan being %d\n", currentbuf, dma_chan);
    
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    
    channel_config_set_dreq(&cfg, DREQ_ADC);
    

    if(currentbuf){
        dma_channel_configure(dma_chan, &cfg,
        capture_buf,    // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        false           // dont start immediately
    );
    }else{
        dma_channel_configure(dma_chan, &cfg,
            capture_buf,    // dst
            &adc_hw->fifo,  // src
            CAPTURE_DEPTH,  // transfer count
            false           // dont start immediately
        );
    }

    //dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start(dma_chan);
    
    DEBUG_PRINT("configureDMA finished!\n");
}


void setup_sd(){
    DEBUG_PRINT("setup_sd() was called\n");

    sd_card_t *pSD = sd_get_by_num(0);
    DEBUG_PRINT("setup_sd(): Got sd card number 0\n");

    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    DEBUG_PRINT("setup_sd(): Attempted mount...\n");

    if (FR_OK != fr) DEBUG_PRINT("setup_sd(): f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    DEBUG_PRINT("setup_sd(): mounted filesys\n");

    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        printf("f_printf failed\n");
    }

    DEBUG_PRINT("setup_sd(): closing file");
    fr = f_close(&fil);
    if (fr != FR_OK) {
        DEBUG_PRINT("ERROR: Could not close file %s(%d)\r\n", FRESULT_str(fr), fr);
        while (true);
    }

    DEBUG_PRINT("setup_sd(): SD card setup is done, good to go!\n");
}


void write_to_sd(bool currentbuf){
    DEBUG_PRINT("write_to_sd was called with currentbuf being %d\n", currentbuf);

    FRESULT fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
            panic("write_to_sd: f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);

    for (unsigned long i = 0; i < CAPTURE_DEPTH; ++i) {
            
            char tmp[32];
            if(currentbuf){
                snprintf(tmp, 32, "%u,", capture_buf[i]);
            }else{
                snprintf(tmp, 32, "%u,", capture_buf_tmp[i]);
            }
            //printf("%s\n", tmp);
            f_printf(&fil, tmp);
    }
    DEBUG_PRINT("write_to_sd(): Finished print loop");
    fr = f_close(&fil);
    if (fr != FR_OK) {
        DEBUG_PRINT("ERROR: Could not close file %s(%d)\r\n", FRESULT_str(fr), fr);
        while (true);
    }
    DEBUG_PRINT("Finished write_to_sd!");
}


void main() {
    stdio_init_all(); 
    time_init();
    sleep_ms(10000);



    DEBUG_PRINT("main(): initializing ADC gpio\n");

    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(28 + CAPTURE_CHANNEL);

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        false,   // We won't see the ERR bit because of 8 bit reads; disable.
        true     // Shift each sample to 8 bits when pushing to FIFO
    );

    // Divisor of 0 -> full speed. 
    adc_set_clkdiv(0);

    setup_sd(); // I gotta put this before the configureDMA so we don't get interrupted, but putting it before seems to lock us up
               // instantly once we call configureDMA() before anything in that function can happen

    DEBUG_PRINT("main(): Arming DMA\n");

    dma_chan = dma_claim_unused_channel(true);
    DEBUG_PRINT("main(): unused channel %d claimed\n", dma_chan);

    DEBUG_PRINT("Calling configureDMA with currentbuf %d\n", currentbuf);
    configureDMA(currentbuf); // Instantly crashes things?


    bool oldBufferState = false;
    DEBUG_PRINT("main(): Loop for collecting data has started!\n");
    while(1){
        if(oldBufferState != currentbuf && !computeComplete){
            write_to_sd(!currentbuf);
            oldBufferState = currentbuf;
            computeComplete = true;
        }
    }
}