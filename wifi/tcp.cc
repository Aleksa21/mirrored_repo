#include "tcp.h"
#include "esp_util.h"

#include "freertos/FreeRTOS.h"

// Server class
static TcpServer mTcpServer;

// Utility
static Esp_Util& mUtil = Esp_Util::getInstance();

////// Variables
static const char* TAG = "tcp";	// Log tag

//////// MyTcpServerCallbacks class Methods
void MyTcpServerCallbacks::onConnect()
{
	// Client connected
	logD("Tcp connected!");
}

void MyTcpServerCallbacks::onDisconnect()
{
	// Client disconnected
	logD("Tcp disconnected!");
}

void MyTcpServerCallbacks::onReceive(const char *message)
{
	logI("onReceive callback!");
	//Process the message (main.cc)
	mTcpServer.sendMsg((string&)message);
	logV("Received Tcp message: [%s]", mUtil.strExpand(message).c_str());
	// Append the message to msgIn queue
	mTcpServer.tcpMsgInQueue.append(message);
	logV("Tcp message queue: [%s]", mUtil.strExpand(mTcpServer.tcpMsgInQueue).c_str());
}


//general methods
/**
 * @brief Initialize the BLE Server
 */
void tcpInitialize()
{
	//mTcpServer.initialize(BLE_DEVICE_NAME, new MyBleServerCallbacks());
	mTcpServer.initialize(new MyTcpServerCallbacks());
}

void tcpFinalize()
{
	//mTcpServer.initialize(BLE_DEVICE_NAME, new MyBleServerCallbacks());
	mTcpServer.finalize();
}
