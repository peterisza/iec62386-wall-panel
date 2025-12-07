#include <msp430.h>
#include "uart.h"
#include "watchdog.h"
#include "i2c.h"
#include "util.h"
#include "gpio.h"
#include "buzzer.h"
#include "touch.h"
#include "clock.h"
#include "scheduler.h"
#include "sensors.h"
#include "captivate.h"
#include "dali_phy_mac.h"


int main(void)
{
    disable_watchdog();

    clock_init_8mhz();

    __delay_cycles(1600000);        // Wait ~200ms at ~8MHz for clocks to stabilize

    gpio_init();

    uart_init();
    buzzer_init(8000000);
    buzzer_beep(1000, 100);

    dali_phy_init();
    i2c_init(8000000u, 100000u);

    sensors_init();

    uart_send_string("Initializing touch...\r\n");      
    uart_wait_for_tx_empty();
    touch_init();
    __delay_cycles(800000);
    uart_send_string("Touch initialized\r\n");
    uart_wait_for_tx_empty();

    // Configure P1.5 as GPIO Output
    // Stop Timer A1 first (already done in adc_init, but ensure it's stopped)
    TA1CTL = MC__STOP | TACLR;
    TA1CCTL1 = 0;

    // Configure P1.7 as output for DALI_TX
    P1DIR |= BIT7;
    P1OUT &= ~BIT7;  // Start with TX inactive (LOW)

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
            g_eventTap = 0;
        }
        if(g_eventUp) {
            buzzer_beep(4000, 5);
            uart_send_string("Up\r\n");
            g_eventUp = 0;
        }
        if(g_eventDown) {
            buzzer_beep(1000, 5);
            uart_send_string("Down\r\n");
            g_eventDown = 0;
        }
        scheduler_tick();
        CAPT_appSleep();
    }
}

