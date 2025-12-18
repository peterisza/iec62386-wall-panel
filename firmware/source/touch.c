#include "touch.h"
#include "captivate.h"
#include "CAPT_UserConfig.h"
#include "CAPT_App.h"
#include "rom_captivate.h"
#include "rom_map_captivate.h"

volatile int32_t g_sum = 0, g_x = 0, g_y = 0;
volatile int16_t g_pressedCnt = 0;
volatile int16_t g_lastY = 0;
volatile int16_t
    g_eventTap = 0,
    g_eventHold = 0,
    g_eventUp = 0,
    g_eventDown = 0,
    g_eventSwipeUp = 0,
    g_eventSwipeDown = 0;

volatile uint8_t g_upButtonActive = 0, g_downButtonActive = 0, g_sliderActive = 0;

#define PRESSED_CNT_THRESHOLD 15
#define SLIDE_STEP_SIZE 15

#define BIG_BUTTON_ACTIVATE_THRESHOLD -250
#define BIG_BUTTON_DEACTIVATE_THRESHOLD -150

#define SLIDER_ACTIVATE_THRESHOLD -150
#define SLIDER_DEACTIVATE_THRESHOLD -100

static void touch_handler(tSensor *s)
{

    int16_t btn_d0 = (int16_t)(buttons.pCycle[0]->pElements[0]->filterCount.ui16Natural) - 
    (int16_t)(buttons.pCycle[0]->pElements[0]->LTA.ui16Natural);
    int16_t btn_d1 = (int16_t)(buttons.pCycle[0]->pElements[1]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[0]->pElements[1]->LTA.ui16Natural);
    int16_t btn_d2 = (int16_t)(buttons.pCycle[0]->pElements[2]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[0]->pElements[2]->LTA.ui16Natural);
    int16_t btn_d3 = (int16_t)(buttons.pCycle[0]->pElements[3]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[0]->pElements[3]->LTA.ui16Natural);
    // Cycle 1: E00, E03, E05, E06
    int16_t btn_d4 = (int16_t)(buttons.pCycle[1]->pElements[0]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[1]->pElements[0]->LTA.ui16Natural);
    int16_t btn_d5 = (int16_t)(buttons.pCycle[1]->pElements[1]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[1]->pElements[1]->LTA.ui16Natural);
    int16_t btn_d6 = (int16_t)(buttons.pCycle[1]->pElements[2]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[1]->pElements[2]->LTA.ui16Natural);
    int16_t btn_d7 = (int16_t)(buttons.pCycle[1]->pElements[3]->filterCount.ui16Natural) - 
        (int16_t)(buttons.pCycle[1]->pElements[3]->LTA.ui16Natural);

    int32_t sum1 = btn_d2 + btn_d7;
    //int32_t sum2 = btn_d1 + btn_d4;
    int32_t sum3 = btn_d0 + btn_d5;

    int32_t sumup = btn_d1 + btn_d2 + btn_d5;
    int32_t sumdown = btn_d0 + btn_d4 + btn_d7;

    // Sum of all detections in int32
    g_sum = sumup + sumdown;
    g_x = (-127 * sum1 + 127 * sum3) / g_sum;
    g_y = (127 * sumup - 127 * sumdown) / g_sum;

    if(btn_d6 < BIG_BUTTON_ACTIVATE_THRESHOLD && !g_upButtonActive) {
        g_upButtonActive = 1;
        g_eventUp = 1;
    } else if(btn_d6 > BIG_BUTTON_DEACTIVATE_THRESHOLD && g_upButtonActive) {
        g_upButtonActive = 0;
    }
    
    if(btn_d3 < BIG_BUTTON_ACTIVATE_THRESHOLD && !g_downButtonActive) {
        g_downButtonActive = 1;
        g_eventDown = 1;
    } else if(btn_d3 > BIG_BUTTON_DEACTIVATE_THRESHOLD && g_downButtonActive) {
        g_downButtonActive = 0;
    }

    if(g_pressedCnt == 1) {
        //beep(10, 20);
        g_lastY = g_y;
    }
    if(g_pressedCnt > PRESSED_CNT_THRESHOLD) {
        if(g_y > g_lastY + SLIDE_STEP_SIZE) {
            g_eventSwipeUp = 1;
            g_lastY = g_y;
        }
        if(g_y < g_lastY - SLIDE_STEP_SIZE) {
            g_eventSwipeDown = 1;
            g_lastY = g_y;
        }
    }

    if(g_sum < SLIDER_ACTIVATE_THRESHOLD && !g_sliderActive ) {
        g_sliderActive = 1;
        g_eventTap = 1;
    }
    if(g_sum > SLIDER_DEACTIVATE_THRESHOLD && g_sliderActive) {
        g_sliderActive = 0;
    }  

    if(g_sliderActive) {
        g_pressedCnt++;
        if(g_pressedCnt == PRESSED_CNT_THRESHOLD) {
            g_eventHold = 1;
        }
    } else {
        g_pressedCnt = 0;
    }
}

void touch_init(void)
{
    CAPT_appStart();
    MAP_CAPT_registerCallback(&buttons, &touch_handler);
}