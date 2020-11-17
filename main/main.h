/**
 * 2020 Jiesoft
 * All Rights Reserved
 *
 * main.h
 */

#ifndef MAIN_H_
#define MAIN_H_

////// Includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nvs_flash.h"
#include "soc/soc.h"

#include "sdkconfig.h"

// C++
#include <string>
using namespace std;

// Firmware version
#define FW_VERSION "1.1"

// Target
#ifdef CONFIG_IDF_TARGET_ESP32
#define CHIP_NAME "ESP32"
#else
#define CHIP_NAME "OTHER"
#endif

// FreeRTOS // TODO: see it!

// Dual or single core ?
#if !CONFIG_FREERTOS_UNICORE
#define TASK_CPU APP_CPU_NUM // Core 1
#else
#define TASK_CPU PRO_CPU_NUM // Core 0
#endif

// Task priorities
#define TASK_PRIOR_HIGHEST  20
#define TASK_PRIOR_HIGH     8
#define TASK_PRIOR_MEDIUM   3
#define TASK_PRIOR_LOW      1

// Stack Depth
#define TASK_STACK_LARGE 	10240
#define TASK_STACK_MEDIUM   5120
#define TASK_STACK_SMALL    1024

/*
 * Macro to check the outputs of TWDT functions and trigger a restart if an
 * incorrect code is returned.
 */
#define CHECK_TWDT_ERROR_CODE(returned, expected) ({                    \
		if (returned != expected) {                                \
			printf("TWDT ERROR\n");                                \
            esp_restart();                                         \
        }                                                          \
})

#define TWDT_TIMEOUT_S       3

// Actions of main_Task - by task notifications

#define MAIN_TASK_ACTION_NONE 			0	// No action
#define MAIN_TASK_ACTION_RESET_TIMER 	1	// To reset the seconds timer (for example, after a app connection)

////// Prototypes of main

extern void appInitialize(bool resetTimerSeconds);
//extern void notifyMainTask(uint32_t action, bool fromISR=false);
//extern void error(const char* message, bool fatal=false);

////// External variables 
// Log 
extern bool mLogActive;
 
// Times
extern uint32_t mTimeSeconds;               // Current time in seconds (for timeouts calculations)
extern uint32_t mLastTimeFeedback;
//extern uint32_t mLastTimeReceivedData;

extern "C" {
    void app_main(void);

    //extern void appInitialize(bool resetTimerSeconds);
}


#endif // MAIN_H_

