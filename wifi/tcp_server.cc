#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_task_wdt.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "tcp_server.h"
#include "log.h"
#include "main.h"

#define PORT 						CONFIG_TCP_PORT
#define TCP_SERVER_TASK_PRIO		5
#define TCP_SERVER_TASK_CPU			1
#define TCP_CHAR_VAL_LEN_MAX		128

static void (*mCallbackReceivedData) (char *data, uint16_t size); 	// Callback for receive data

static void tcp_server_task(void *pvParameters);
static TaskHandle_t xTaskTcpServerHandle = NULL;

static const char *TAG = "tcp_server";

static TcpServer tcpServer;

// Callbacks
static TcpServerCallbacks* tcpServerCallbacks;

static void receiveDataCallback(char *data, uint16_t size)
{
	tcpServerCallbacks->onReceive(data);
}

static void tcp_server_task(void *pvParameters)
{
	logI ("Starting tcp_server_task");
	//FIXME -> Change this task so it can be added to twdt
	//ADD TASK TO TWDT
	//CHECK_TWDT_ERROR_CODE(esp_task_wdt_add(xTaskTcpServerHandle), ESP_OK);
	//CHECK_TWDT_ERROR_CODE(esp_task_wdt_status(xTaskTcpServerHandle), ESP_OK);

    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    if (addr_family == AF_INET)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
    else if (addr_family == AF_INET6)
    {
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        logE("Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    logI("Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        logE("Socket unable to bind: errno %d", errno);
        logE("IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    logI("Socket bound, port %d", PORT);

    //listen(socket,backlog) -> start listening for incoming connections.
    //backlog -> maximum nb of connections that can be queued for this socket
    err = listen(listen_sock, 1);
    if (err != 0) {
        logE("Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {
    	//CHECK_TWDT_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
        logI("Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        tcpServer.sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (tcpServer.sock < 0)
        {
            logE("Unable to accept connection: errno %d", errno);
            break;
        }
        else
        	logE("Accepted connection!");

        // Convert ip address to string
        if (source_addr.sin6_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        }
        else if (source_addr.sin6_family == PF_INET6)
        {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }

        logI("Socket accepted ip address: %s", addr_str);

        tcpServer.connect();
        tcpServer.talkWithClient();
        tcpServer.disconnect();

        shutdown(tcpServer.sock, 0);
        close(tcpServer.sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

/**
* @brief Set callback to receiving data
*/
void TcpServer::tcp_server_SetCallbackReceiveData(void (*callbackReceived) (char* data, uint16_t size))
{
	// Arrow callback to receive data
	mCallbackReceivedData = callbackReceived;
}


void TcpServer::talkWithClient()
{
    int& len = tcpServer.len;
    int& sock = tcpServer.sock;
    char* rx_buffer = tcpServer.rx_buffer;

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0)
        {
            logE("Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            logV("Connection closed");
        }
        else
        {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            logI("Received %d bytes: %s", len, rx_buffer);
            receiveDataCallback(rx_buffer, len);
        }
    } while (len > 0);
}


void TcpServer::sendMsg(std::string& message)
{
    int& len = tcpServer.len;
    int& sock = tcpServer.sock;
    char* rx_buffer = tcpServer.rx_buffer;

    // send() can return less bytes than supplied length.
    // Walk-around for robust implementation.
    int to_write = len;
    while (to_write > 0)
    {
        int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
        if (written < 0)
        {
            logE("Error occurred during sending: errno %d", errno);
        }
        to_write -= written;
    }
}

void TcpServer::connect()
{
	isConnected = true;
}

void TcpServer::disconnect()
{
	isConnected = true;
}

bool TcpServer::connectionActive()
{
	return isConnected;
}

/**
* @brief Finalize the Tcp server
*/
void TcpServer::finalize()
{
	if(xTaskTcpServerHandle != NULL)
	{
		vTaskDelete(xTaskTcpServerHandle);
		logI("Task for event deleted");
	}
	logI("Finalized Tcp server");
}

/**
* @brief Initialize the Tcp server
*/
void TcpServer::initialize(TcpServerCallbacks* mTcpServerCallbacks)
{
	tcpServerCallbacks = mTcpServerCallbacks;

	xTaskCreatePinnedToCore(
    		&tcp_server_task,
			"tcp_server",
			4096,
			(void*)AF_INET, //pvParameters
			TCP_SERVER_TASK_PRIO,
			&xTaskTcpServerHandle,
			TCP_SERVER_TASK_CPU
	);
}
