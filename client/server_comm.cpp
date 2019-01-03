#include "server_comm.h"
#include <stdio.h>
#include "main.h"
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXSocket.h>
#include <sstream>

ix::WebSocket wsocket;

namespace ix
{
void setThreadName(const std::string& name) {}
}

bool printed = false;

void Callback(ix::WebSocketMessageType messageType,
			  const std::string& str,
			  size_t wireSize,
			  const ix::WebSocketErrorInfo& error,
			  const ix::WebSocketCloseInfo& closeInfo,
			  const ix::WebSocketHttpHeaders& headers)
{
	printf("Message %s\n", str.c_str());
	if (messageType == ix::WebSocket_MessageType_Open) {
		printf("Connected!\n");
	}
	else if (messageType == ix::WebSocket_MessageType_Close)
		printf("Disconnect!\n");
}

void ServerComm::Initialize()
{
	ix::Socket::init();
	wsocket.setUrl("wss://127.0.0.1:8083");
	wsocket.setOnMessageCallback(Callback);
	STPRINT("Connecting to the server...");
	wsocket.start();
}

void ServerComm::Stop()
{
	wsocket.stop();
}
