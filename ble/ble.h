/*
 * ble.h
 *
 */

#ifndef MAIN_BLE_H_
#define MAIN_BLE_H_

///// Includes

#include "esp_system.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// C++

#include <string>
using namespace std;

/////// Definitions
#define BLE_DEVICE_NAME CONFIG_BLE_DEVICE_NAME // last two of the mac address is appended to name

////// Prototypes
void bleInitialize();
void bleFinalize();
void bleSendData(const char* data);
void bleSendData(string& data);
bool bleConnected();
const uint8_t* bleMacAddress();

#endif /* MAIN_BLE_H_ */

//////// End
