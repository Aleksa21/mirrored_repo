/* ***********
 * Project   : Esp-Idf-App-Mobile - Esp-Idf - Firmware on the Esp32 board - Ble
 * Programmer: Joao Lopes
 * Module    : peripherals - Routines to treat peripherals of Esp32, as gpio, adc, etc.
 * Versions  :
 * ------- 	-------- 	------------------------- 
 * 0.1.0 	01/08/18 	First version 
 */

/////// Includes

#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"

// From the project
#include "main.h"

#include "peripherals.h"

// Utilities

#include "log.h"
#include "esp_util.h"

#ifdef MEDIAN_FILTER_READINGS
	#include "median_filter.h"
#endif

/////// Variables

// Log

static const char* TAG = "peripherals";

#ifdef PIN_LED_STATUS
// Led status on?

static bool mLedStatusOn = false; 
#endif

// ADC reading average

#ifdef MEDIAN_FILTER_READINGS
static MedianFilter <uint16_t, MEDIAN_FILTER_READINGS> Filter;
#endif

/// Sensors

#ifdef HAVE_BATTERY
bool mGpioVEXT = false;			// Powered by external voltage (USB or power supply)
bool mGpioChgBattery = false;	// Charging battery ?
int16_t mAdcBattery = 0;		// voltage of battery readed by ADC
#endif

/////// Prototype - Private

static void gpioInitialize();
static void gpioFinalize();

#ifdef INSTALL_ISR_SERVICE
static void IRAM_ATTR gpio_isr_handler (void * arg);
#endif

static void adcInitialize();
static uint16_t adcReadMedian (adc1_channel_t channelADC1);

////// Methods

/////// Routines for all peripherals

/**
 * @brief Initialize all peripherals
 */
void peripheralsInitialize()
{
	// Debug
	logD("Initializing peripherals ...");

	// Gpio
	gpioInitialize();

	// ADC
	adcInitialize();

	// Debug
	logD("Peripherals initialized");
}

/**
 * @brief Finalize all peripherals 
 */
void peripheralsFinalize() {

	logD("Finalizing peripherals ...");

	// Gpio 
	gpioFinalize();

	// ADC
	//adcFinalize(); // ADC not needs finalize

	// Debug
	logD("Peripherals finalized");

}

/////// Routines for GPIO
static xQueueHandle gpio_evt_queue = NULL;

static void gpio_task_example(void* arg)
{
	logI ("Starting main Task");
	//ADD TASK TO TWDT
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
	uint32_t period_ms = 1000u;
	const TickType_t xTicks = (period_ms / portTICK_RATE_MS);

    uint32_t io_num;
    while(1)
    {
    	CHECK_TWDT_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
        if(xQueueReceive(gpio_evt_queue, &io_num, xTicks))
        {
        	logE("GPIO[%d] val: %d", io_num, gpio_get_level((gpio_num_t)io_num));
        }
    }
}

static void gpio_task_toggle_test(void* arg)
{
	logI ("Starting main Task");
	//ADD TASK TO TWDT
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);

	int cnt = 0;
    while(1)
    {
    	CHECK_TWDT_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_TOGGLE_0, cnt++ % 2);
        //logE("Toggled pin 18");
    }
}

/**
 * @brief Initializes GPIOs
 */
static void gpioInitialize()
{
	// Debug
	logD ("Initializing GPIO ...");

	//// Gpio config
	gpio_config_t config;

#ifdef GPIO_OUTPUT_IO_TOGGLE_0
	config.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
    config.mode = GPIO_MODE_OUTPUT;
    config.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    config.pull_down_en = (gpio_pulldown_t)GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = (gpio_pullup_t)GPIO_PULLUP_DISABLE;
    gpio_config(&config);
    xTaskCreate(gpio_task_toggle_test, "gpio_task_toggle_test", 2048, NULL, 3, NULL);
#endif

//install gpio isr service
#ifdef INSTALL_ISR_SERVICE
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
#endif

#ifdef PIN_PULSE
    config.intr_type = (gpio_int_type_t)GPIO_INTR_ANYEDGE;
    config.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    config.mode = GPIO_MODE_INPUT;
    config.pull_down_en = (gpio_pulldown_t)GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = (gpio_pullup_t)GPIO_PULLUP_ENABLE;
    gpio_config(&config);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 3, NULL);

	//hook isr handler for specific gpio pin
	gpio_isr_handler_add(PIN_PULSE, gpio_isr_handler, (void *) PIN_PULSE);
	//remove isr handler for gpio number.
	gpio_isr_handler_remove(PIN_PULSE);
	//hook isr handler for specific gpio pin again
	gpio_isr_handler_add(PIN_PULSE, gpio_isr_handler, (void*) PIN_PULSE);

#endif
#ifdef PIN_PANIC_ALARM
	gpio_isr_handler_add(PIN_PANIC_ALARM, gpio_isr_handler, (void*) PIN_PANIC_ALARM);
#endif


    //// TODO Output pins

#ifdef PIN_LED_STATUS

    // Led of status (can be a board led of ESP32 or external)

	config.pin_bit_mask = (1<<PIN_LED_STATUS);
	config.mode         = GPIO_MODE_OUTPUT;
	config.pull_up_en   = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type    = GPIO_INTR_DISABLE;

	gpio_config(&config);

	gpio_set_level(PIN_LED_STATUS, 1);
	mLedStatusOn = true;

#endif

	// TODO initialize roof light GPIO
	//initRoofLight();

	// Debug
	logD ("GPIO initalized");
}

/**
 * @brief Finish the GPIO
 */
static void gpioFinalize () {

	// Debug

	logD ("GPIO finalizing ...");

#ifdef PIN_LED_STATUS

	// Turn off the led of status

	gpioSetLevel(PIN_LED_STATUS, 0);

	mLedStatusOn = false;
#endif

#ifdef INSTALL_ISR_SERVICE
	// Disable ISRs

	#ifdef PIN_BUTTON_STANDBY
	gpioDisableISR (PIN_BUTTON_STANDBY);
	#endif

	#ifdef PIN_SENSOR_VEXT
	gpioDisableISR (PIN_SENSOR_VEXT);
	#endif

	#ifdef PIN_SENSOR_CHARGING
	gpioDisableISR (PIN_SENSOR_CHARGING);
	#endif

#endif

	// Debug

	logD ("GPIO finished");

}

#ifdef PIN_LED_STATUS
/**
 * @brief Blink the status led
 */
void gpioBlinkLedStatus() {

	mLedStatusOn = !mLedStatusOn;

	gpioSetLevel(PIN_LED_STATUS, mLedStatusOn);

}

#endif

//// Privates

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

#ifndef INSTALL_ISR_SERVICE
/**
 * @brief Interrupt handler of GPIOs
 */
static void IRAM_ATTR gpio_isr_handler (void * arg) {

	uint32_t gpioNum = (uint32_t) arg;
	logE("ISR HANDLER!!");
	// Debounce events 

	static uint32_t lastTime = 0;		// Last event time
	static uint32_t lastGpioNum = 0; 	// Last GPIO 

	bool ignore = false;				// Ignore event ?

	// Treat gpio

	switch (gpioNum) {

#ifdef PIN_PULSE
    xQueueSendFromISR(gpio_evt_queue, &gpioNum, NULL);
#endif
#ifdef PIN_BUTTON_STANDBY
		case PIN_BUTTON_STANDBY: // Standby?

			// Debounce

			if (lastGpioNum == PIN_BUTTON_STANDBY && lastTime > 0) {

				if ((millis() - lastTime) < 50) {
					ignore=true;
				}

			}

			if (!ignore) {

				// Notify main_Task to enter standby - to not do it in ISR
				
				notifyMainTask(MAIN_TASK_ACTION_STANDBY_BTN, true);
			}
			break;
#endif

#ifdef PIN_SENSOR_VEXT
		case PIN_SENSOR_VEXT: // Powered by external voltage (USB or power supply)

			// Not need debounce

			// Powered by VEXT? 

			mGpioVEXT = gpioIsHigh(PIN_SENSOR_VEXT);

			// Notify main_Task that powered by VEXT is changed - to not do it in ISR
			
			notifyMainTask(MAIN_TASK_ACTION_SEN_VEXT, true);

			break;
#endif

#ifdef PIN_SENSOR_CHARGING
		case PIN_SENSOR_CHARGING: // Charging battery

			// Without debounce - due not notification used more - this is done in main_Task
			//                    (due if no battery plugged, the notification not ok)
			// // Debounce - it is important, due if no battery plugged, the value change fast

			// if (lastGpioNum == PIN_SENSOR_CHARGING && lastTime > 0) {

			// 	if ((millis() - lastTime) < 50) {
			// 		ignore=true;
			// 	}

			// }

			// Charging now ? 

			mGpioChgBattery = gpioIsLow(PIN_SENSOR_CHARGING);

			// No notification 
			// if (!ignore) {
			// 
			// 	// Notify main_Task that charging is changed - to not do it in ISR
			//	
			// 	notifyMainTask(MAIN_TASK_ACTION_SEN_CHGR, true);
			// }
			break;
#endif
		default:
			break;
	}

	// Save this

	lastTime = millis();
	lastGpioNum = gpioNum;
}

#endif

/////// Routines for ADC

/**
 * @brief Initializes ADC
 */
static void adcInitialize()
{
	logD ("Initializing ADC ...");

	// ADC input pins (sensors)
#ifdef ADC_SENSOR_VBAT
	
	// Sensor VBAT 
	
	adc1_config_width(ADC_WIDTH_12Bit);

	adc1_config_channel_atten (ADC_SENSOR_VBAT, ADC_ATTEN_11db); // VBAT sensor - to identify the current battery voltage (VBAT)
#endif

	// Debug
	logD ("ADC Initialized");
}

/**
 * @brief Reading the sensors by ADC
 */
void adcRead() {

	// Read sensors

#if defined HAVE_BATTERY && defined ADC_SENSOR_VBAT 

	// VBAT voltage
		
	#ifdef PIN_GROUND_VBAT

		// Pin to ground resistor divider to measure voltage of battery
		// To turn ground only when reading ADC 

		gpioSetLevel (PIN_GROUND_VBAT, GPIO_LEVEL_READ_VBAT_ON); // Ground this 

	#endif

	// Read ADC

	mAdcBattery = adcReadMedian (ADC_SENSOR_VBAT);

	#ifdef PIN_GROUND_VBAT
		gpioSetLevel (PIN_GROUND_VBAT, GPIO_LEVEL_READ_VBAT_OFF); // Not ground this - no consupmition of battery 
	#endif
	
#endif

}

////// Private

/**
 * @brief Average reading - ADC
 */
 
static uint16_t adcReadMedian (adc1_channel_t channelADC1) {

	for (uint8_t i = 0; i <MEDIAN_FILTER_READINGS; i ++) {

		Filter.set(i, ::adc1_get_raw(channelADC1));
	}

	uint16_t median;
	Filter.getMedian(median);
	return median;
}

////////// End
