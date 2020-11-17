/* ***********
 * Project   : util - Utilities to esp-idf
 * Programmer: Joao Lopes
 * Module    : ble_uart_server - BLE UART server to esp-idf
 * Comments  : Based in pcbreflux samples
 * Versions  :
 * ------- 	-------- 	-------------------------
 * 0.1.0 	01/08/18 	First version
 * 0.3.0  	23/08/18	Adjustments to allow sizes of BLE > 255
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "sdkconfig.h"

#include "ble_uart_server.h"
#include "log.h"

////// Variables
static const char* TAG = "ble_uart_server";							// Log tag
static const char* START_CMD = "START";  //"START\r\n";
static const char* STOP_CMD = "STOP";  //"STOP\r\n";
enum COMMANDS_NOTIFICATIONS
{
	NOTIFY_START_CMD = 1,
	NOTIFY_STOP_CMD
};


static void (*mCallbackConnection)();								// Callback for connection/disconnection
static void (*mCallbackMTU)();										// Callback for MTU change detect
static void (*mCallbackReceivedData) (char *data, uint16_t size); 	// Callback for receive data


static void ble_log_task(void *pvParameters);
static TaskHandle_t xTaskLogHandle = NULL;

static esp_gatt_if_t mGatts_if = ESP_GATT_IF_NONE;					// To save gatts_if
static bool mConnected = false;										// Connected ?
static char mDeviceName[30];										// Device name
static uint16_t mMTU = 20;											// MTU of BLE data
static const uint8_t* mMacAddress;									// Mac address

////// Prototypes

// Private
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void gap_event_handler(esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t * param);
static void gatts_event_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);

static void char1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void char1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr1_read_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);
static void descr1_write_handler(esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t * param);

////// TODO to be cleaned - Routines based on the pbcreflux example
//static uint8_t char1_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x11, 0x22, 0x33 };
static uint8_t char1_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64 };
static uint8_t descr1_str[GATTS_CHAR_VAL_LEN_MAX] = { 0x01, 0x02, 0x03, 0x04 };

static esp_attr_value_t gatts_demo_char1_val =
{
		.attr_max_len = GATTS_CHAR_VAL_LEN_MAX,
		.attr_len = sizeof(char1_str),
		.attr_value = char1_str,
};

static esp_attr_value_t gatts_demo_descr1_val =
{
		.attr_max_len = GATTS_CHAR_VAL_LEN_MAX,
		.attr_len = sizeof(descr1_str),
		.attr_value = descr1_str,
};

#define BLE_SERVICE_UUID_SIZE ESP_UUID_LEN_128

// Add more UUIDs for more then one Service
static uint8_t ble_service_uuid[BLE_SERVICE_UUID_SIZE] =
{
		/* LSB <--------------------------------------------------------------------------------> MSB */
		//first uuid, 16bit, [12],[13] is the value
	    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
};

static uint8_t ble_manufacturer[BLE_MANUFACTURER_DATA_LEN] = {
		0x12, 0x23, 0x45, 0x56
};

static uint32_t ble_add_char_pos;

/// Advertising data content, according to "Supplement to the Bluetooth Core Specification"
static esp_ble_adv_data_t ble_adv_data =
{
		.set_scan_rsp = false,
		.include_name = true,
		.include_txpower = true,
		.min_interval = 0x20,
		.max_interval = 0x40,
		.appearance = 0x00,
		.manufacturer_len = BLE_MANUFACTURER_DATA_LEN,
		.p_manufacturer_data = (uint8_t *) ble_manufacturer,
		.service_data_len = 0,
		.p_service_data = NULL,
		.service_uuid_len = sizeof(ble_service_uuid),
		.p_service_uuid = ble_service_uuid,
		.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t ble_adv_params =
{
		.adv_int_min = 0x20,
		.adv_int_max = 0x40,
		.adv_type = ADV_TYPE_IND,
		.own_addr_type = BLE_ADDR_TYPE_PUBLIC,
		.channel_map = ADV_CHNL_ALL,
		.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};

struct gatts_char_inst {
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t char_perm;
	esp_gatt_char_prop_t char_property;
	esp_attr_value_t *char_val;
	esp_attr_control_t *char_control;
	uint16_t char_handle;
	esp_gatts_cb_t char_read_callback;
	esp_gatts_cb_t char_write_callback;
	esp_bt_uuid_t descr_uuid;
	esp_gatt_perm_t descr_perm;
	esp_attr_value_t *descr_val;
	esp_attr_control_t *descr_control;
	uint16_t descr_handle;
	esp_gatts_cb_t descr_read_callback;
	esp_gatts_cb_t descr_write_callback;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile =
{
		.gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_char_inst gl_char[GATTS_CHAR_NUM] = 
{ 
	{
		.char_uuid.len = ESP_UUID_LEN_16, // RX
		.char_uuid.uuid.uuid16 = 0xFF01,
		.char_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 
		.char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
		.char_val = &gatts_demo_char1_val, 
		.char_control = NULL, 
		.char_handle = 0,
		.char_read_callback = char1_read_handler, 
		.char_write_callback = char1_write_handler, 
		.descr_uuid.len = ESP_UUID_LEN_16, 
		.descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
		.descr_perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 
		.descr_val = &gatts_demo_descr1_val, 
		.descr_control = NULL, 
		.descr_handle = 0,
		.descr_read_callback = descr1_read_handler,
		.descr_write_callback = descr1_write_handler 
	}, 
};

esp_gatt_if_t gatts_if_task;
uint16_t conn_id_task;
uint16_t char_handle_task;
static void ble_log_task(void *pvParameters)
{
	// Initialize
	logI("Initializing ble log Task");

	//ADD TASK TO TWDT
	if(esp_task_wdt_add(xTaskLogHandle) != ESP_OK || esp_task_wdt_status(xTaskLogHandle) != ESP_OK)
	{
		printf("TWDT ERROR\n");
        esp_restart();
	}

	// Notification variable
	uint32_t notification;
	uint32_t period_ms = 1000u;
	const TickType_t xTicks = (period_ms / portTICK_RATE_MS);

	// Task loop
	while(1)
	{
		if(esp_task_wdt_reset() != ESP_OK)
		{
			printf("TWDT ERROR\n");
            esp_restart();
		}

		// Wait something notified (seen in the FreeRTOS example)
		if (xTaskNotifyWait (0, 0xffffffff, &notification, xTicks) == pdPASS)
		{
			// Process event by task notification
			logE ("Event received -> %u", notification);
			if(notification == NOTIFY_START_CMD)
			{
				while(1)
				{
					if(esp_task_wdt_reset() != ESP_OK)
					{
						printf("TWDT ERROR\n");
			            esp_restart();
					}
					logE ("Send indicate!, %u, %u, %u", gatts_if_task, conn_id_task, char_handle_task);
					esp_ble_gatts_send_indicate(gatts_if_task, conn_id_task, char_handle_task, gl_char[0].char_val->attr_len, gl_char[0].char_val->attr_value, false);
					//vTaskDelay(xTicks);
					if(xTaskNotifyWait (0, 0xffffffff, &notification, xTicks) == pdPASS){
						if (notification == NOTIFY_STOP_CMD)
								break;
						//FIXME
						//else if (notification == NOTIFY_START_CMD)
						//		notify BLE that START has already been sent
					}
				}
			}
		}
	}
}

static void char1_read_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	logI("Char 1 read handler!");
	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[0].char_val != NULL)
	{
		rsp.attr_value.len = gl_char[0].char_val->attr_len;
		for (uint32_t pos = 0; (pos < gl_char[0].char_val->attr_len && pos < gl_char[0].char_val->attr_max_len); pos++)
		{
			rsp.attr_value.value[pos] = gl_char[0].char_val->attr_value[pos];
		}
	}
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
}

static void descr1_read_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	logI("Descr 1 read handler!");
	esp_gatt_rsp_t rsp;
	memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
	rsp.attr_value.handle = param->read.handle;
	if (gl_char[0].descr_val != NULL)
	{
		rsp.attr_value.len = gl_char[0].descr_val->attr_len;
		for (uint32_t pos = 0; pos < gl_char[0].descr_val->attr_len && pos < gl_char[0].descr_val->attr_max_len; pos++)
		{
			rsp.attr_value.value[pos] = gl_char[0].descr_val->attr_value[pos];
		}
	}
	esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
}


static void char1_write_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	logI("Char 1 write handler!");

	if(strcmp((const char*)param->write.value, START_CMD) == 0)
	{
		logE("Received Start command!!");
		xTaskNotify (xTaskLogHandle, NOTIFY_START_CMD, eSetValueWithOverwrite);
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
	}
	else if(strcmp((const char*)param->write.value, STOP_CMD) == 0)
	{
		logE("Received Stop command!!");
		xTaskNotify (xTaskLogHandle, NOTIFY_STOP_CMD, eSetValueWithOverwrite);
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
	}
	else
	{
		if (gl_char[0].char_val != NULL)
		{
			gl_char[0].char_val->attr_len = param->write.len;
			for (uint32_t pos = 0; pos < param->write.len; pos++)
			{
				gl_char[0].char_val->attr_value[pos] = param->write.value[pos];
			}
		}
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);

		// Original code changed here !
		// Callback for receive data (after the send_response to works!!)
		if (gl_char[0].char_val != NULL && gl_char[0].char_val->attr_len > 0)
		{ // Read OK ;-)
			// Callback para receive data
			if (mCallbackReceivedData != NULL)
			{
				mCallbackReceivedData((char*) gl_char[0].char_val->attr_value, (uint8_t) gl_char[0].char_val->attr_len);
			}
		}
	}
}

static void descr1_write_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	logI("Descr 1 write handler!");
	if (gl_char[0].descr_val != NULL)
	{
		gl_char[0].descr_val->attr_len = param->write.len;
		for (uint32_t pos = 0; pos < param->write.len; pos++)
		{
			gl_char[0].descr_val->attr_value[pos] = param->write.value[pos];
		}
	}
	esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
}

static void gatts_add_char()
{
	for (uint32_t pos = 0; pos < GATTS_CHAR_NUM; pos++)
	{
		if (gl_char[pos].char_handle == 0)
		{
			//logI("Char property: %u", gl_char[pos].char_property);
			ble_add_char_pos = pos;
			esp_ble_gatts_add_char(gl_profile.service_handle,
					&gl_char[pos].char_uuid, gl_char[pos].char_perm,
					gl_char[pos].char_property, gl_char[pos].char_val,
					gl_char[pos].char_control);
			break;
		}
	}
}

static void gatts_check_add_char(esp_bt_uuid_t char_uuid, uint16_t attr_handle)
{
	if (attr_handle != 0)
	{
		gl_char[ble_add_char_pos].char_handle = attr_handle;

		// is there a descriptor to add ?
		if (gl_char[ble_add_char_pos].descr_uuid.len != 0 && gl_char[ble_add_char_pos].descr_handle == 0)
		{
			esp_ble_gatts_add_char_descr(gl_profile.service_handle,
					&gl_char[ble_add_char_pos].descr_uuid,
					gl_char[ble_add_char_pos].descr_perm,
					gl_char[ble_add_char_pos].descr_val,
					gl_char[ble_add_char_pos].descr_control);
		}
		else
		{
			gatts_add_char();
		}
	}
}

static void gatts_check_add_descr(esp_bt_uuid_t descr_uuid, uint16_t attr_handle)
{
	if (attr_handle != 0)
	{
		gl_char[ble_add_char_pos].descr_handle = attr_handle;
	}
	gatts_add_char();
}

static void gatts_check_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	uint16_t handle = 0;
	uint8_t read = 1;

	switch (event)
	{
	case ESP_GATTS_READ_EVT:
	{
		read = 1;
		handle = param->read.handle;
		break;
	}
	case ESP_GATTS_WRITE_EVT:
	{
		read = 0;
		handle = param->write.handle;
		break;
	}
	default:
		break;
	}

	for (uint32_t pos = 0; pos < GATTS_CHAR_NUM; pos++)
	{
		if (gl_char[pos].char_handle == handle)
		{
			if (read == 1)
			{
				if (gl_char[pos].char_read_callback != NULL)
				{
					gl_char[pos].char_read_callback(event, gatts_if, param);
				}
			}
			else
			{
				if (gl_char[pos].char_write_callback != NULL)
				{
					gl_char[pos].char_write_callback(event, gatts_if, param);
				}
			}
			break;
		}
		if (gl_char[pos].descr_handle == handle)
		{
			if (read == 1)
			{
				if (gl_char[pos].descr_read_callback != NULL)
				{
					gl_char[pos].descr_read_callback(event, gatts_if, param);
				}
			}
			else
			{
				if (gl_char[pos].descr_write_callback != NULL)
				{
					gl_char[pos].descr_write_callback(event, gatts_if, param);
				}
			}
			break;
		}
	}
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	switch (event)
	{
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&ble_adv_params);
		break;
	default:
		break;
	}
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	switch (event)
	{
	//first event to trigger. Here we set some GAP info and start the service
	case ESP_GATTS_REG_EVT:
		logI("ESP_GATTS_REG_EVT");
		gl_profile.service_id.is_primary = true;
		gl_profile.service_id.id.inst_id = 0x00;
		gl_profile.service_id.id.uuid.len = ESP_UUID_LEN_128;
		for(uint8_t pos = 0; pos < ESP_UUID_LEN_128; pos++)
		{
			gl_profile.service_id.id.uuid.uuid.uuid128[pos] = ble_service_uuid[pos];
		}

		// Changed - device name
		esp_ble_gap_set_device_name(mDeviceName);
		esp_ble_gap_config_adv_data(&ble_adv_data);

		esp_ble_gatts_create_service(gatts_if, &gl_profile.service_id, GATTS_NUM_HANDLE);
		break;
	case ESP_GATTS_READ_EVT:
		logI("ESP_GATTS_READ_EVT");
		gatts_check_callback(event, gatts_if, param);
		break;
	//triggers on write events(when a message is received)
	case ESP_GATTS_WRITE_EVT:
		logI("ESP_GATTS_WRITE_EVT");
		//echo received message
		gatts_if_task = gatts_if;
		conn_id_task = param->write.conn_id;
		char_handle_task = gl_profile.char_handle;
		logE("Send indicate!, %u, %u, %u", gatts_if_task, conn_id_task, char_handle_task);
		esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile.char_handle, param->write.len, param->write.value, false);
		gatts_check_callback(event, gatts_if, param);
		break;
	//triggers when MTU is changed
	case ESP_GATTS_MTU_EVT:
		// Original code changed here !
		mMTU = param->mtu.mtu;

		// Callback for MTU changed
		if (mCallbackMTU != NULL) {

			mCallbackMTU();
		}
		break;
	//Create service evt, triggered by esp_ble_gatts_create_service
	case ESP_GATTS_CREATE_EVT:
		gl_profile.service_handle = param->create.service_handle;
		gl_profile.char_uuid.len = gl_char[0].char_uuid.len;
		gl_profile.char_uuid.uuid.uuid16 = gl_char[0].char_uuid.uuid.uuid16;

		esp_ble_gatts_start_service(gl_profile.service_handle);
		gatts_add_char();
		break;
	//Add Char evt. Characteristics are added in ESP_GATTS_CREATE_EVT
	case ESP_GATTS_ADD_CHAR_EVT:
		gl_profile.char_handle = param->add_char.attr_handle;

		if (param->add_char.status == ESP_GATT_OK)
		{
			gatts_check_add_char(param->add_char.char_uuid,
					param->add_char.attr_handle);
		}
		break;
	//Once the descriptor is added, the `ESP_GATTS_ADD_CHAR_DESCR_EVT` event is triggered
	case ESP_GATTS_ADD_CHAR_DESCR_EVT:
		if (param->add_char_descr.status == ESP_GATT_OK)
		{
			gatts_check_add_descr(param->add_char.char_uuid,
					param->add_char.attr_handle);
		}
		break;
	//An `ESP_GATTS_CONNECT_EVT` is triggered when a client has connected to the GATT server
	case ESP_GATTS_CONNECT_EVT:
		logI("ESP_GATTS_CONNECT_EVT");
		gl_profile.conn_id = param->connect.conn_id;

		// Original code changed here !
		/// BLE
		mGatts_if = gatts_if;

		mConnected = true;

		// Callback for connection
		if (mCallbackConnection != NULL)
		{
			mCallbackConnection();
		}

		break;

	case ESP_GATTS_DISCONNECT_EVT:
		logI("ESP_GATTS_DISCONNECT_EVT");
		esp_ble_gap_start_advertising(&ble_adv_params);
		// Original code changed here !
		/// BLE
		xTaskNotify(xTaskLogHandle, NOTIFY_STOP_CMD, eSetValueWithOverwrite);
		mGatts_if = ESP_GATT_IF_NONE;
		mConnected = false;
		mMTU = 20;

		// Callback for connection
		if (mCallbackConnection != NULL)
		{
			mCallbackConnection();
		}
		break;
	case ESP_GATTS_DELETE_EVT:
	//Triggered by esp_ble_gatts_start_service() function -> Start of a service
	case ESP_GATTS_START_EVT:
	case ESP_GATTS_STOP_EVT:
	case ESP_GATTS_CONF_EVT:
	case ESP_GATTS_EXEC_WRITE_EVT:
	case ESP_GATTS_ADD_INCL_SRVC_EVT:
	case ESP_GATTS_UNREG_EVT:
	case ESP_GATTS_OPEN_EVT:
	case ESP_GATTS_CANCEL_OPEN_EVT:
	case ESP_GATTS_CLOSE_EVT:
	case ESP_GATTS_LISTEN_EVT:
	case ESP_GATTS_CONGEST_EVT:
	default:
		break;
	}
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT)
	{
		if (param->reg.status == ESP_GATT_OK)
		{
			gl_profile.gatts_if = gatts_if;
		}
		else
		{
			return;
		}
	}
	gatts_profile_event_handler(event, gatts_if, param);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// End of pbcreflux codes
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Public

/**
* @brief Finalize ble server and ble 
*/
esp_err_t ble_uart_server_Finalize() {

	// TODO: verify if need something more

	esp_err_t ret = esp_bt_controller_deinit();

	// Debug (logI to show allways)

	logI("BLE UART server finalized");

	return ret;
}

/**
* @brief Set callback for conection/disconnection
*/
void ble_uart_server_SetCallbackConnection(void (*callbackConnection)(), void (*callbackMTU)())
{
	// Set the callbacks
	mCallbackConnection = callbackConnection;
	mCallbackMTU = callbackMTU;
}

/**
* @brief Set callback to receiving data
*/
void ble_uart_server_SetCallbackReceiveData(void (*callbackReceived) (char* data, uint16_t size))
{
	// Arrow callback to receive data
	mCallbackReceivedData = callbackReceived;
}

/**
* @brief Is a client connected to UART server?
*/
bool ble_uart_server_ClientConnected ()
{
	// Client connected to UART server?
	return mConnected;
}

/**
* @brief Send data to client (mobile App)
*/
esp_err_t ble_uart_server_SendData(const char* data, uint16_t size)
{
	// Connected?
	if (mConnected == false || mGatts_if == ESP_GATT_IF_NONE)
	{
		return ESP_FAIL;
	}

	// Check the size
	if (size > GATTS_CHAR_VAL_LEN_MAX) {
		size = GATTS_CHAR_VAL_LEN_MAX;
	}

	// Copy the data
	char send [GATTS_CHAR_VAL_LEN_MAX + 1];
	memset (send, 0, GATTS_CHAR_VAL_LEN_MAX + 1);
	strncpy (send, data, (size_t) size);

	// Send data via BLE notification
	return esp_ble_gatts_send_indicate(mGatts_if, 0, gl_char[1].char_handle,
			size, (uint8_t *) send, false);
}

/**
* @brief Get actual MTU of client (mobile app)
*/
uint16_t ble_uart_server_MTU()
{
	// Returns the received MTU or default
	return mMTU;

}

/**
* @brief Get mac address of ESP32
*/
const uint8_t* ble_uart_server_MacAddress()
{
	return mMacAddress;
}


/**
* @brief Initialize ble and ble server
*/
esp_err_t ble_uart_server_Initialize (const char* device_name) {

	// Device Name
	strncpy(mDeviceName, device_name, 30 - 5); // leaves 5 reserved for the end of mac address

	// Initialize the BLE
	esp_err_t ret;

	// Free the memory of the classic BT (seen at https://www.esp32.com/viewtopic.php?f=13&t=3139);
	esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

	// Initialize the Bluetooth
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init (& bt_cfg);
	if (ret) {
        return ESP_FAIL;
	}

	ret = esp_bt_controller_enable (ESP_BT_MODE_BLE); // BLE -> BLE only
	if (ret) {
        return ESP_FAIL;
	}

	// Initialize bluedroid
	ret = esp_bluedroid_init ();
	if (ret) {
        return ESP_FAIL;
	}
	ret = esp_bluedroid_enable ();
	if (ret) {
        return ESP_FAIL;
	}

	// Get the mac adress
	mMacAddress = esp_bt_dev_get_address();

	// Change the name with the last two of the mac address
	// If name ends with '_'
	uint8_t size = strlen(mDeviceName);

	if (size > 0 && mDeviceName[size-1] == '_')
	{
		char aux[7];
		sprintf(aux, "%02X%02X", mMacAddress[4], mMacAddress[5]);

		strcat (mDeviceName, aux);
	}

	// Set the device name
	esp_bt_dev_set_device_name(mDeviceName);

	// Callbacks
	esp_ble_gatts_register_callback (gatts_event_handler);
	esp_ble_gap_register_callback (gap_event_handler);
	esp_ble_gatts_app_register (BLE_PROFILE_APP_ID);

	// Debug (logI to show allways)
	logI("BLE UART server initialized, device name %s", mDeviceName);

	// Create task for event in core 1
	xTaskCreatePinnedToCore(
			&ble_log_task,
			"ble_log_task",
			5120,
			NULL,
			BLE_LOG_TASK_PRIO,
			&xTaskLogHandle,
			0
	);

	return ESP_OK;
}


//////// End
