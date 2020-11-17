/*
 * ble_uart_server.h
 */

#ifndef UTIL_BLE_UART_SERVER_H_
#define UTIL_BLE_UART_SERVER_H_

#ifdef __cplusplus
	extern "C" {
#endif

#include "stdbool.h"
#include "esp_err.h"
#include "esp_bt.h"

#include "log.h"

#define GATTS_CHAR_NUM				1
#define GATTS_NUM_HANDLE     		1+(3*GATTS_CHAR_NUM)

#define BLE_MANUFACTURER_DATA_LEN  	4

#define GATTS_CHAR_VAL_LEN_MAX		185 - 5 // Changed maximum to avoid split of messages ( - 5 is for safe)

#define BLE_PROFILE_APP_ID 			0

#define BLE_LOG_TASK_PRIO 			6

// Prototypes added to esp_uart_server code - public
esp_err_t ble_uart_server_Initialize(const char* device_name);
esp_err_t ble_uart_server_Finalize();
void ble_uart_server_SetCallbackConnection(void (*callbackConnection)(), void (*callbackMTU)());
bool ble_uart_server_ClientConnected();
void ble_uart_server_SetCallbackReceiveData(
		void (*callbackReceived)(char* data, uint16_t size));
esp_err_t ble_uart_server_SendData(const char* data, uint16_t size);
uint16_t ble_uart_server_MTU();
const uint8_t* ble_uart_server_MacAddress();

#ifdef __cplusplus
}
#endif

#endif /* UTIL_BLE_UART_SERVER_H_ */

//////// End
