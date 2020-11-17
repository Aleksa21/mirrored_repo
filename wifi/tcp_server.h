#pragma once
#include <string>
#include <cstring>

class TcpServerCallbacks;
class TcpServer;

#define RX_BUF_LEN			128

// TcpServer - C++ class wrapper to tcp_server (C)
//TODO: Write a constructor for this class when you get home!
class TcpServer
{
	public:
		//constructor
		TcpServer() : sock(0), len(0) {memset(rx_buffer, 0, RX_BUF_LEN);}
		//public methods
		void initialize(TcpServerCallbacks* mTcpServerCallbacks);
		void finalize();
		void connect();
		void disconnect();
		bool connectionActive();
		void sendMsg(std::string&);
		void talkWithClient();
		void tcp_server_SetCallbackReceiveData(void(*callbackReceived)(char* data, uint16_t size));

		//public data
		int sock;
		std::string tcpMsgInQueue;


	private:
		//private methods
		void processEventConnection();

		//private data
		bool isConnected = false;
		int len;
		char rx_buffer[RX_BUF_LEN];
};

// Callbacks for tcp server -> abstract class
class TcpServerCallbacks
{
	public:
		virtual ~TcpServerCallbacks() {}
		virtual void onConnect() = 0;
		virtual void onDisconnect() = 0;
		virtual void onReceive(const char* message) = 0;
};
