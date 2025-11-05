#include "scheduler.h"
#include <stddef.h>

#define SCHEDULER_MAX_TASKS 16

uint16_t g_scheduler_counter;
uint16_t g_scheduler_interval_ms;
uint16_t g_scheduler_task_count;
void (*g_scheduler_tasks[SCHEDULER_MAX_TASKS])(void);
uint16_t g_scheduler_tasks_interval[SCHEDULER_MAX_TASKS];

uint8_t g_scheduler_task_index;

void scheduler_init(uint16_t interval_ms, uint16_t initial_delay_ms)
{
    g_scheduler_interval_ms = interval_ms;
    g_scheduler_counter = initial_delay_ms / interval_ms;
    if(g_scheduler_counter == 0)
    {
        g_scheduler_counter = 1;
    }
    g_scheduler_task_index = 0;
    g_scheduler_task_count = 0;
}

void scheduler_add_task(void (*task)(void), uint16_t wait_after_task_ms)
{
    if(g_scheduler_task_count >= SCHEDULER_MAX_TASKS)
    {
        return;
    }
    g_scheduler_tasks[g_scheduler_task_count] = task;
    uint16_t tick_count = wait_after_task_ms / g_scheduler_interval_ms;
    if(tick_count == 0)
    {
        tick_count = 1;
    }
    g_scheduler_tasks_interval[g_scheduler_task_count] = tick_count;
    g_scheduler_task_count++;
}

void scheduler_tick(void)
{
    if(g_scheduler_counter == 1 && g_scheduler_task_index < g_scheduler_task_count)
    {
        if(g_scheduler_tasks[g_scheduler_task_index] != NULL)
        {
            g_scheduler_tasks[g_scheduler_task_index]();
        }
        g_scheduler_counter = g_scheduler_tasks_interval[g_scheduler_task_index] + 1;
        g_scheduler_task_index++;
        if(g_scheduler_task_index >= g_scheduler_task_count)
        {
            g_scheduler_task_index = 0;
        }
    }
    if(g_scheduler_counter > 0)
    {
        g_scheduler_counter--;
    }
}
