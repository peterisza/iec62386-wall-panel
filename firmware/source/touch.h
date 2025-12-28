#ifndef TOUCH_H__
#define TOUCH_H__

#include "CAPT_Type.h"

extern volatile int32_t g_sum;
extern volatile int32_t g_x;
extern volatile int32_t g_y;
extern volatile int16_t g_pressedCnt;
extern volatile int16_t g_lastY;
extern volatile int16_t g_eventTap;
extern volatile int16_t g_eventTouch;
extern volatile int16_t g_eventHold;
extern volatile int16_t g_eventSwipeUp;
extern volatile int16_t g_eventSwipeDown;
extern volatile int16_t g_eventUp;
extern volatile int16_t g_eventDown;

void touch_handler(tSensor *s);
void touch_init(void);

#endif
