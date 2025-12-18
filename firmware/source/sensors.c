#include "sensors.h"
#include "uart.h"
#include "util.h"

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
    g_ens210_data.valid = false;
    if (!ens210_read_results(&g_ens210_data)) {
        uart_send_status_prefix(false);
        uart_send_string("ENS210 read failed\r\n");
        return;
    }

    uart_send_status_prefix(g_ens210_data.t_valid_crc_ok);
    uart_send_string("ENS210 Temperature: ");
    uart_send_string(ftoa(g_ens210_data.temperature_C));
    uart_send_string(" C ");
    uart_send_string(g_ens210_data.t_valid_crc_ok ? "(CRC ok)\r\n" : "(CRC error)\r\n");

    uart_send_status_prefix(g_ens210_data.h_valid_crc_ok);
    uart_send_string("ENS210 Humidity: ");
    uart_send_string(ftoa(g_ens210_data.humidity_RH));
    uart_send_string("% ");
    uart_send_string(g_ens210_data.h_valid_crc_ok ? "(CRC ok)\r\n" : "(CRC error)\r\n");
    
    g_ens210_data.valid = g_ens210_data.t_valid_crc_ok && g_ens210_data.h_valid_crc_ok;
    //buzzer_beep(3000, 10);
}

void lps22hh_read_results_task(void)
{
    g_lps22hh_data.valid = false;
    if (!lps22hh_read_results(LPS22HH_ADDR_SA0_VDD, &g_lps22hh_data)) {
        uart_send_status_prefix(false);
        uart_send_string("LPS22HH read failed\r\n");
        return;
    }

    bool pressure_ok = (g_lps22hh_data.pressure_hPa >= 900.0f) && (g_lps22hh_data.pressure_hPa <= 1100.0f);

    uart_send_status_prefix(pressure_ok);
    uart_send_string("LPS22HH Pressure: ");
    uart_send_string(ftoa(g_lps22hh_data.pressure_hPa));
    uart_send_string(" hPa");
    if (!pressure_ok) {
        uart_send_string(" (out of 900-1100 range)");
    }
    uart_send_string("\r\n");

    bool temp_ok = false;
    if (g_ens210_data.t_valid_crc_ok) {
        float diff = g_lps22hh_data.temperature_C - g_ens210_data.temperature_C;
        if (diff < 0.0f) diff = -diff;
        temp_ok = (diff <= 2.0f);
    }

    uart_send_status_prefix(temp_ok);
    uart_send_string("LPS22HH Temperature: ");
    uart_send_string(ftoa(g_lps22hh_data.temperature_C));
    uart_send_string(" C");
    if (!temp_ok && g_ens210_data.t_valid_crc_ok) {
        uart_send_string(" (delta > 2C)");
    }
    uart_send_string("\r\n");
    g_lps22hh_data.valid = g_ens210_data.t_valid_crc_ok && temp_ok;
    //buzzer_beep(3500, 10);
}

void stcc4_read_results_task(void)
{
    g_stcc4_data.valid = false;
    if (!stcc4_read_measurement(STCC4_ADDR_GND, &g_stcc4_data)) {
        uart_send_status_prefix(false);
        uart_send_string("STCC4 read failed\r\n");
        return;
    }

    uart_send_status_prefix(g_stcc4_data.crc_ok);
    uart_send_string("STCC4 CO2: ");
    uart_send_string(ftoa(g_stcc4_data.co2_ppm));
    uart_send_string(" ppm\r\n");

    uart_send_status_prefix(g_stcc4_data.crc_ok);
    uart_send_string("STCC4 Temperature: ");
    uart_send_string(ftoa(g_stcc4_data.temperature_C));
    uart_send_string(" C\r\n");

    uart_send_status_prefix(g_stcc4_data.crc_ok);
    uart_send_string("STCC4 Humidity: ");
    uart_send_string(ftoa(g_stcc4_data.humidity_RH));
    uart_send_string("% ");
    uart_send_string(g_stcc4_data.crc_ok ? "(CRC ok)\r\n" : "(CRC error)\r\n");
    g_stcc4_data.valid = g_stcc4_data.crc_ok;
    //buzzer_beep(4000, 10);
}

void sensors_init(void)
{
    lps22hh_init(LPS22HH_ADDR_SA0_VDD);
}