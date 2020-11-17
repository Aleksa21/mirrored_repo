/**
 * 2020 Jiesoft
 * All Rights Reserved
 *
 * main.cc
 */
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#include "freertos/task.h"


#include "log.h"
#include "esp_util.h"
#include "fields.h"

#include "ble.h"
#include "peripherals.h"
#include "main.h"
#include "wifi_setup.h"
#include "tcp.h"
#include "uart.h"

// Log
static const char *TAG = "main";

// Utility
static Esp_Util& mUtil = Esp_Util::getInstance();

// Times and intervals
uint32_t mTimeSeconds = 0; 			// Current time in seconds (for timeouts calculations)
uint32_t mLastTimeFeedback = 0; 	// Indicates the time of the last feedback message

// Log active (debugging)
bool mLogActive = false;

// Task main
static TaskHandle_t xTaskMainHandler = NULL;


/**
 * @brief Main Task - main processing
 * Is a second timer to process, adquiry data, send responses to mobile App, control timeouts, etc.
 */
static void main_Task(void * pvParameters)
{
	logI ("Starting main Task");
	//ADD TASK TO TWDT
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_add(xTaskMainHandler), ESP_OK);
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_status(xTaskMainHandler), ESP_OK);

	// Initializations
	mTimeSeconds = 0;
	mLastTimeFeedback = 0;

	mLogActive = true;

	////// FreeRTOS
	// Timeout - task - 1 second
	const TickType_t xTicks = (1000u / portTICK_RATE_MS);

	////// Loop
	uint32_t notification; // Notification variable

	while(1)
	{
		CHECK_TWDT_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
		 // FIXME Wait for the time or something in the msgIn queue,
		 // process the msg if any, then put result to msgOut queue
		if (xTaskNotifyWait(0, 0xffffffff, &notification, xTicks) == pdPASS)
		{
			// Action by task notification
			logI ("Notification received -> %u", notification);

			bool reset_timer = false;

			switch (notification) {

				case MAIN_TASK_ACTION_RESET_TIMER: 	// Reset timer
					reset_timer = true;
					break;

				// TODO: see it! If need put here your custom notifications

				default:
					break;
			}

			// Resets the time variables and returns to the beginning of the loop ?
			// Usefull to initialize the time (for example, after App connection)

			if (reset_timer) {

				// Clear variables

				mTimeSeconds = 0;
				mLastTimeFeedback = 0;

				// TODO: see it! put here custom reset timer code

				// Debug
				logD("Reseted time");

				continue; // Returns to loop begin
			}
		}
		else
		{
			// Action by task notification
			//logI ("No notification received -> %u", notification);
		}

		////// Processes every second

		// Time counter
		mTimeSeconds++;

#ifdef PIN_LED_STATUS
		// Blink the led of status (board led of Esp32 or external)
		gpioBlinkLedStatus();
#endif
		// TODO Sensors readings by ADC
		//adcRead();

		// TODO: see it! Put here your custom code to run every second

		// Debug

		if (mLogActive)
		{
			if (mTimeSeconds % 5 == 0)
			{ // Debug each 5 secs
				logD("* Time seconds=%d", mTimeSeconds);
			} else
			{
				// Verbose //TODO: see it! put here that you want see each second
				// If have, please add our variables and uncomment it
				// logV("* Time seconds=%d", mTimeSeconds);
			}
		}
	}
	////// End
	// Delete this task
	vTaskDelete (NULL);
	xTaskMainHandler = NULL;
}


void appInitialize(bool resetTimerSeconds)
{
	mLogActive = true;

	logI("Initializing");

	// Initialize the Esp32
	mUtil.esp32Initialize();

    //Initialize or reinitialize TWDT
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, false), ESP_OK);

    // initialize status LED, pulse, panic alarm, and roof light GPIOs
	peripheralsInitialize();

	// iraw_scan_rsp_datanitialize BLE server
	bleInitialize();

	// Task -> Initialize task_main in core 1
	xTaskCreatePinnedToCore(
			&main_Task,
			"main_Task",
			TASK_STACK_LARGE,
			NULL,
			TASK_PRIOR_HIGH,
			&xTaskMainHandler,
			TASK_CPU
	);

	// FIXME Task -> Initialize task_ble_reply in core 2
	// which takes a message from msgOut queue and send it out


	// initialize wifi connection
	//wifiInitialize();

	// initialize tcp server connection
	//tcpInitialize();

 	// initialize Serial
    uartInitialize();

 	/*
 	vTaskStartScheduler();
	scheduler starts by default, no need to start it. Source:
	https://www.reddit.com/r/esp32/comments/cj7f9p/guru_meditation_error_core_0_paniced/evbypcb?utm_source=share&utm_medium=web2x&context=3
 	*/

	/**
	 * will be implemented later.
	 */
	// FIXME initialize bluetooth
    //initBluetooth();

	// TODO initialize GPS
	//initGps();

	// TODO initialize camera module
	//initCamera();

	// TODO initialize printer serial interface
	//initPrinter();

	// TODO initialize EFPOS
	//initEfpos();

    // TODO test the system components
    //doSystemTest();
}


void app_main(void)
{
    /* Print chip information */
    printf("Staring TAM Controller\n");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("Staring TAM Controller with %s chip: %d CPU cores, WiFi%s%s, %dMB\n",
    		CHIP_NAME,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
            spi_flash_get_chip_size() / (1024 * 1024));

    /* Init application */
    appInitialize(true);

//    while(1);
//
//    // should never reach here
//    printf("Wow it actually reaches here!\n");
//    fflush(stdout);
//    esp_restart();
//    return;
}
