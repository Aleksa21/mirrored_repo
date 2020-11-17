#pragma once
#include "tcp_server.h"
#include "log.h"
#include <string>

////// Prototypes
void tcpInitialize();

/**
 * @brief Class MyBleServerCalllbacks - based on code of Kolban
 */
class MyTcpServerCallbacks : public TcpServerCallbacks
{
	/**
	 * @brief OnConnect - for connections
	 */
	void onConnect();

	/**
	 * @brief OnDisconnect - for disconnections
	 */
	void onDisconnect();

	/**
	 * @brief OnReceive - for receive data
	 */
	void onReceive(const char *message);
};


