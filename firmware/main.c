#include <msp430.h>
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
#include "buzzer.h"
#include "scheduler.h"

ens210_data_t g_ens210_data;
lps22hh_data_t g_lps22hh_data;
stcc4_data_t g_stcc4_data;

void ens210_start_measurement_task(void)
{
    ens210_start_single_shot(ENS210_START_T | ENS210_START_H);
}

void lps22hh_start_measurement_task(void)
{
    lps22hh_start_one_shot(LPS22HH_ADDR_SA0_VDD);
}

void stcc4_start_measurement_task(void)
{
    stcc4_start_single_shot(STCC4_ADDR_GND);
}

void stcc4_push_compensation_task(void)
{
    stcc4_push_compensation(STCC4_ADDR_GND, g_ens210_data.temperature_C, g_ens210_data.humidity_RH, g_lps22hh_data.pressure_hPa * 100);
}

void ens210_read_results_task(void)
{
    ens210_read_results(&g_ens210_data);
    uart_send_string("Temperature: ");
    uart_send_string(ftoa(g_ens210_data.temperature_C));
    uart_send_string(" C ");
    uart_send_string("Humidity: ");
    uart_send_string(ftoa(g_ens210_data.humidity_RH));
    uart_send_string("%\r\n");
}

void lps22hh_read_results_task(void)
{
    lps22hh_read_results(LPS22HH_ADDR_SA0_VDD, &g_lps22hh_data);
    uart_send_string("Pressure: ");
    uart_send_string(ftoa(g_lps22hh_data.pressure_hPa));
    uart_send_string(" hPa ");
    uart_send_string("Temperature: ");
    uart_send_string(ftoa(g_lps22hh_data.temperature_C));
    uart_send_string("C\r\n");
}

void stcc4_read_results_task(void)
{
    stcc4_read_measurement(STCC4_ADDR_GND, &g_stcc4_data);
    uart_send_string("CO2: ");
    uart_send_string(ftoa(g_stcc4_data.co2_ppm));
    uart_send_string("ppm, Temperature: ");
    uart_send_string(ftoa(g_stcc4_data.temperature_C));
    uart_send_string(" C, ");
    uart_send_string("Humidity: ");
    uart_send_string(ftoa(g_stcc4_data.humidity_RH));
    uart_send_string("%");
    if(!g_stcc4_data.crc_ok) {
        uart_send_string(", CRC error");
    } else {
        uart_send_string(", CRC ok");
    }
    uart_send_string("\r\n");
}

int main(void)
{
    disable_watchdog();
    
    // Power-on stabilization delay - critical for reliable startup
    // This ensures clocks and power domains are stable before initialization
    __delay_cycles(100000);        // Wait ~100ms at ~1MHz for clocks to stabilize
    i2c_init(1000000u, 100000u);

    gpio_init();
    uart_init();
    touch_init();
    buzzer_init(0);
 
    __bis_SR_register(GIE);        // globál interrupt engedély
    scheduler_init(g_uiApp.ui16ActiveModeScanPeriod, 1000);

    scheduler_add_task(ens210_start_measurement_task, 200);
    scheduler_add_task(ens210_read_results_task, 2000);

    scheduler_add_task(lps22hh_start_measurement_task, 250);
    scheduler_add_task(lps22hh_read_results_task, 2000);

    scheduler_add_task(stcc4_push_compensation_task, 300);
    scheduler_add_task(stcc4_start_measurement_task, 1000);
    scheduler_add_task(stcc4_read_results_task, 5000);

    for(;;)
    {
        CAPT_appHandler();        // ez indítja a scanneket és hívja a callbacket
        
        /*uart_send_string(itoa_padded(g_x, 10));
        uart_send_string(itoa_padded(g_y, 10));
        uart_send_string(itoa_padded(g_sum, 10));
        
        uart_send_string("\r\n");*/
        
        
        if(g_eventTap) {
            buzzer_beep(2000, 10);
            uart_send_string("Tap\r\n");
            g_eventTap = 0;
        }
        if(g_eventUp) {
            buzzer_beep(4000, 10);
            uart_send_string("Up\r\n");
            g_eventUp = 0;
        }
        if(g_eventDown) {
            buzzer_beep(1000, 10);
            uart_send_string("Down\r\n");
            g_eventDown = 0;
        }
        
        scheduler_tick();

        CAPT_appSleep();
    }
    
    while(1)
    {
        uart_send_string("--------------------------------\r\n");
    
        ens210_start_single_shot(ENS210_START_T | ENS210_START_H);
        // tipikus konverziós idő ~130 ms (datasheet); várj annyit, vagy poll-ozd SENS_STAT-ot
        __delay_cycles(130000);
        ens210_data_t e;
        ens210_read_results(&e);
        uart_send_string("Temperature: ");
        uart_send_string(ftoa(e.temperature_C));
        uart_send_string(" C ");
        uart_send_string("Humidity: ");
        uart_send_string(ftoa(e.humidity_RH));
        uart_send_string("%\r\n");

        // --- LPS22HH one-shot ---
        lps22hh_start_one_shot(LPS22HH_ADDR_SA0_VDD);
        // one-shot után a STATUS P_DA/T_DA bitek jelzik a készenlétet
        __delay_cycles(200000); // pár ms
        lps22hh_data_t p;
        lps22hh_read_results(LPS22HH_ADDR_SA0_VDD, &p);
        uart_send_string("Pressure: ");
        uart_send_string(ftoa(p.pressure_hPa));
        uart_send_string(" hPa ");
        uart_send_string("Temperature: ");
        uart_send_string(ftoa(p.temperature_C));
        uart_send_string("C\r\n");
 

        bool success = stcc4_push_compensation(STCC4_ADDR_GND, e.temperature_C, e.humidity_RH, p.pressure_hPa * 100);
        if (!success) { 
            uart_send_string("Failed to push compensation\r\n");
        } else {
            uart_send_string("Compensation pushed\r\n");
        }
        // --- STCC4 single-shot ---
        stcc4_start_single_shot(STCC4_ADDR_GND);
        __delay_cycles(1500000); // ~500 ms
        stcc4_data_t c;
        stcc4_read_measurement(STCC4_ADDR_GND, &c);
        uart_send_string("CO2: ");
        uart_send_string(ftoa(c.co2_ppm));
        uart_send_string("ppm, Temperature: ");
        uart_send_string(ftoa(c.temperature_C));
        uart_send_string(" C, ");
        uart_send_string("Humidity: ");
        uart_send_string(ftoa(c.humidity_RH));
        uart_send_string("%");
        if(!c.crc_ok) {
            uart_send_string(", CRC error");
        } else {
            uart_send_string(", CRC ok");
        }

        uart_send_string("\r\n");

        __delay_cycles(5000000);
    }
}
