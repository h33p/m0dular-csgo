#ifndef SERVER_COMM_H
#define SERVER_COMM_H

#include <string>
#include "../sdk/framework/utils/semaphores.h"

namespace ServerComm
{
	extern bool connected;
	extern bool quit;
	extern Semaphore mainsem;

	bool Initialize();
	void Stop();
	void LoginCredentials();
	void Send(const std::string& buf);
}

#endif
