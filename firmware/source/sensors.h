#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>
#include "ens210_temperature_sensor.h"
#include "lps22hh_barometer.h"
#include "stcc4_co2_sensor.h"

ens210_data_t g_ens210_data;
lps22hh_data_t g_lps22hh_data;
stcc4_data_t g_stcc4_data;

void ens210_start_measurement_task(void);
void lps22hh_start_measurement_task(void);
void stcc4_start_measurement_task(void);
void stcc4_push_compensation_task(void);
void ens210_read_results_task(void);
void lps22hh_read_results_task(void);
void stcc4_read_results_task(void);
void sensors_init(void);

#endif