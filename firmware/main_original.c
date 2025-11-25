#include <msp430.h>
#include <stdbool.h>
#include "uart.h"
#include "watchdog.h"
#include "i2c.h"
#include "util.h"
#include "gpio.h"
#include "ens210_temperature_sensor.h"
#include "lps22hh_barometer.h"
#include "stcc4_co2_sensor.h"
#include "captivate.h"
#include "CAPT_Type.h"
#include "CAPT_UserConfig.h"
#include "CAPT_App.h"
#include "rom_captivate.h"
#include "rom_map_captivate.h"
#include "touch.h"
//#include "buzzer.h"
#include "scheduler.h"
#include "dali_phy_mac.h"
#include "sensors.h"


/* Globális változók (volatile!) */
volatile uint16_t low_event_counter = 0;
volatile uint16_t high_event_counter = 0;

/* HA a header fájlban nincs definiálva az ADCWINC, pótoljuk manuálisan.
Az FR2xx User Guide (SLAU445) szerint az ADCMCTL0 regiszter 5. bitje az. */
#ifndef ADCWINC
#define ADCWINC BIT5
#endif

void init_ADC_WindowComp(void) {
    P1SEL0 |= BIT1;
    P1SEL1 |= BIT1;

    // ADCCTL0: Bekapcsolás, Sample time
    ADCCTL0 = ADCSHT_2 | ADCON; 
    

    // Először nullázzuk ki a felbontás biteket (ADCRES)
    ADCCTL1 &= ~(ADCRES0 | ADCRES1); 
    
    // Majd állítsuk be a Pulse Mode-ot, Szoftver Triggert, SMCLK-t ÉS a 12-bitet (ADCRES_2)
    ADCCTL1 |= ADCSHP | ADCSHS_0 | ADCSSEL_2 | ADCRES_2;
    
    // ADCCTL2: Repeat Single (minden ADCSC-re egyet mér)
    ADCCTL2 = ADCCONSEQ_2; 

    // Memória és Window Comparator (ez marad)
    ADCMCTL0 = ADCINCH_1 | ADCSREF_0 | ADCWINC;
    ADCLO = 910;
    ADCHI = 1213;

    // Interruptok (marad)
    ADCIE = ADCLOIE | ADCHIIE;
    
    // ADC Engedélyezése
    ADCCTL0 |= ADCENC;
}

void init_Timer_Trigger(void) {
    // Timer B0 beállítása 9600 Hz-re
    // 1 MHz / 104 = ~9615 Hz
    TB0CCR0 = 103; 
    
    // Trigger pont (TB0.1 kimenet a belső triggerhez)
    TB0CCR1 = 50;  
    TB0CCTL1 = OUTMOD_3; // Set/Reset mód

    // SMCLK, Up mode, Clear
    TB0CTL = TBSSEL__SMCLK | MC__UP | TBCLR;
}

void test_loop(void) {
    uint16_t prev_low = 0;
    uint16_t prev_high = 0;
    uint8_t timer_1s_counter = 0;
    uint16_t current_adc_val = 0;

    __enable_interrupt();

    while(1) {
        __delay_cycles(100000); // 100ms

        // --- KÉZI TRIGGER ---
        // Ez helyettesíti most a Timert.
        // Megbökjük az ADCSC bitet (Start Conversion)
        ADCCTL0 |= ADCSC; 

        // Várunk picit, hogy biztosan kész legyen (opcionális, de debugnál biztosabb)
        while(ADCCTL1 & ADCBUSY);
        // --------------------

        // Eredmény kiolvasása
        current_adc_val = ADCMEM0;

        uart_send_string("\r\n[DEBUG] ADC: ");
        uart_send_uint_dec(current_adc_val);
        // ... ADC kiírás után ...
        uart_send_string(" [Test: 1000->");
        uart_send_uint_dec(1000); 
        uart_send_string("]");
        // Window események kiírása
        if (low_event_counter != prev_low) {
            uart_send_string(" -> Low IRQ! (Cnt: ");
            uart_send_uint_dec(low_event_counter);
            uart_send_string(")");
            prev_low = low_event_counter;
        }

        if (high_event_counter != prev_high) {
            uart_send_string(" -> High IRQ! (Cnt: ");
            uart_send_uint_dec(high_event_counter);
            uart_send_string(")");
            prev_high = high_event_counter;
        }

        // P1.5 Villogtatás
        timer_1s_counter++;
        if (timer_1s_counter >= 10) {
            timer_1s_counter = 0;
            P1OUT ^= BIT5;
            uart_send_string(" [FLIP P1.5]");
        }
    }
}
/* ADC Interrupt Service Routine */
// A vektor neve ebben a családban ADC_VECTOR (nem ADC12_VECTOR)
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC_VECTOR))) ADC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    // Az interrupt vektor regiszter neve is ADCIV
    switch(__even_in_range(ADCIV, ADCIV_ADCIFG)) {
        case ADCIV_NONE: break;
        case ADCIV_ADCOVIFG: break;   // Overflow
        case ADCIV_ADCTOVIFG: break;  // Time overflow
        case ADCIV_ADCHIIFG:          // Window High ( > 0.8V )
            high_event_counter++;
            break;
        case ADCIV_ADCLOIFG:          // Window Low ( < 0.6V )
            low_event_counter++;
            break;
        case ADCIV_ADCINIFG: break;   // Window Inside
        case ADCIV_ADCIFG: break;     // Conversion complete (nincs engedélyezve)
        default: break;
    }
}



int main(void)
{
    disable_watchdog();
    
    // Power-on stabilization delay - critical for reliable startup
    // This ensures clocks and power domains are stable before initialization
    __delay_cycles(100000);        // Wait ~100ms at ~1MHz for clocks to stabilize
    //touch_init();
    
    gpio_init();
    /*buzzer_beep(1000, 100);*/
    uart_init();

    // P1.5 kimenetként (DALI simulation test)
    P1DIR |= BIT5;
    P1OUT |= BIT5;  // kezdetben HIGH
    
    // ADC window comparator inicializálása
    init_ADC_WindowComp();
    init_Timer_Trigger();   
    test_loop();

    //buzzer_init(0);
    i2c_init(1000000u, 100000u);
    
    sensors_init();
    /*dali_phy_init(dali_frame_callback);
    dali_phy_enable();*/

 
    __bis_SR_register(GIE);        // globál interrupt engedély
    scheduler_init(g_uiApp.ui16ActiveModeScanPeriod, 1000);

    scheduler_add_task(ens210_start_measurement_task, 250);
    scheduler_add_task(ens210_read_results_task, 4000);

    scheduler_add_task(lps22hh_start_measurement_task, 500);
    scheduler_add_task(lps22hh_read_results_task, 4000);

    scheduler_add_task(stcc4_push_compensation_task, 300);
    scheduler_add_task(stcc4_start_measurement_task, 400);
    scheduler_add_task(stcc4_read_results_task, 5000);

    for(;;)
    {
        CAPT_appHandler();        // ez indítja a scanneket és hívja a callbacket
        
        
        if(g_eventTap) {
            //buzzer_beep(2000, 5);
            uart_send_string("Tap\r\n");
            g_eventTap = 0;
        }
        if(g_eventUp) {
            //buzzer_beep(4000, 5);
            uart_send_string("Up\r\n");
            g_eventUp = 0;
        }
        if(g_eventDown) {
            //buzzer_beep(1000, 5);
            uart_send_string("Down\r\n");
            g_eventDown = 0;
        }


        scheduler_tick();        
        CAPT_appSleep();
    }
    
}
