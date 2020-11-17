/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include "uart.h"
#include "log.h"
#include "main.h"
#include <string>

#define UART_PRIO 			5
#define TXD_PIN 			(CONFIG_UART_TX_GPIO)
#define RXD_PIN 			(CONFIG_UART_RX_GPIO)

static const int RX_BUF_SIZE = 1024;

static const char *TAG = "uart";

std::string serialMsgIn;

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* data)
{
	//or sendToMsgInQ(data),
	//and make sendToMsgInQ(const string& data);
	serialMsgIn.append(data);
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    logI("Wrote %d bytes", txBytes);
    return txBytes;
}

static void rx_task(void *arg)
{
	logI ("Starting uart rx_task");
	//ADD TASK TO TWDT
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
	CHECK_TWDT_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);

    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
    	CHECK_TWDT_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {
        	//null terminate received string
            data[rxBytes] = 0;
            logI("Read %d bytes: '%s'", rxBytes, data);
            sendData("UART received msg: ");
            sendData((const char*) data);
        }
    }
    free(data);
}

void uartInitialize(void)
{
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, UART_PRIO, NULL);
}
