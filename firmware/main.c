#include <msp430.h>
#include "uart.h"
#include "watchdog.h"
#include "i2c.h"
#include "util.h"
#include "led.h"
#include "buzzer.h"
#include "touch.h"
#include "clock.h"
#include "scheduler.h"
#include "sensors.h"
#include "captivate.h"
#include "dali_phy_mac.h"
#include "random.h"

#define CLOCK_HZ 8000000u

int main(void)
{
    disable_watchdog();
    PM5CTL0 &= ~LOCKLPM5;
    
    clock_init_8mhz();
    delay_ms_8mhz(200); 
    led_init();

    uart_init();
    buzzer_init(CLOCK_HZ);
    buzzer_beep(1000, 100);
    
    // Generate random seed from ADC noise and initialize RNG
    uint16_t random_seed = generate_random_seed_from_adc();
    random_init(random_seed);
    
    // Print seed to UART for verification
    uart_send_string("Random seed: ");
    uart_send_hex((uint8_t*)&random_seed, 2);
    uart_send_string("\r\n");
    
    dali_phy_init();
    
    i2c_init(CLOCK_HZ, 100000u);

    sensors_init();
    delay_ms_8mhz(100);

    touch_init();
    
    // Configure P1.5 as GPIO Output
    // Stop Timer A1 first (already done in adc_init, but ensure it's stopped)
    TA1CTL = MC__STOP | TACLR;
    TA1CCTL1 = 0;

    __bis_SR_register(GIE);        // Enable global interrupts

    uint16_t loop_counter = 0;

    scheduler_init(g_uiApp.ui16ActiveModeScanPeriod, 1000);

    scheduler_add_task(ens210_start_measurement_task, 250);
    scheduler_add_task(ens210_read_results_task, 4000);

    scheduler_add_task(lps22hh_start_measurement_task, 500);
    scheduler_add_task(lps22hh_read_results_task, 4000);

    scheduler_add_task(stcc4_push_compensation_task, 300);
    scheduler_add_task(stcc4_start_measurement_task, 400);
    scheduler_add_task(stcc4_read_results_task, 5000);

    uart_send_string("==== DALI PHY test ====\r\n");
    while(1) {
        CAPT_appHandler();
        if(g_eventTap) {
            buzzer_beep(2000, 5);
            uart_send_string("Tap\r\n");
            dali_tx_send_frame(0x0040E0, 24);
            g_eventTap = 0;
        }
        if(g_eventUp) {
            buzzer_beep(4000, 5);
            uart_send_string("Up\r\n");
            dali_tx_send_frame(0xE04000, 24);
            g_eventUp = 0;
        }
        if(g_eventDown) {
            buzzer_beep(1000, 5);
            uart_send_string("Down\r\n");
            dali_tx_send_frame(0xFFFFFF, 24);
            g_eventDown = 0;
        }
        /*loop_counter += g_uiApp.ui16ActiveModeScanPeriod;
        if(loop_counter >= 1000) {
            loop_counter = 0;
            uart_send_string("*tick*\r\n");
        }*/
        scheduler_tick();
        CAPT_appSleep();
    }
}

