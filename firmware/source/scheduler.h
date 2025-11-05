#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdint.h>

void scheduler_init(uint16_t interval_ms, uint16_t initial_delay_ms);
void scheduler_add_task(void (*task)(void), uint16_t wait_after_task_ms);
void scheduler_tick(void);

#endif