#include "server_comm_handlers.h"
#include "server_comm.h"
#include "main.h"
#include "loader.h"
#include <string.h>

void LoginApproved(const std::string& str)
{
	SetColor(ANSI_COLOR_GREEN);
	STPRINT("Login successful!\n");
	SetColor(ANSI_COLOR_YELLOW);
	fflush(stdout);
	printf("%s\n", str.c_str());
	SetColor(ANSI_COLOR_RESET);
	ServerComm::connected = true;
	ServerComm::mainsem.Post();
}

void LoginRejected(const std::string& str)
{
	SetColor(ANSI_COLOR_RED);
	STPRINT("Login rejected!\n");
	SetColor(ANSI_COLOR_RESET);
	fflush(stdout);
	ServerComm::mainsem.Post();
}

void LoginInvHWID(const std::string& str)
{
	SetColor(ANSI_COLOR_RED);
	STPRINT("Invalid HWID!\n");
	SetColor(ANSI_COLOR_RESET);
	fflush(stdout);
	ServerComm::quit = true;
	ServerComm::mainsem.Post();
}

void LoginInvIP(const std::string& str)
{
	SetColor(ANSI_COLOR_RED);
	STPRINT("Login blocked! Try again later...\n");
	SetColor(ANSI_COLOR_RESET);
	fflush(stdout);
	ServerComm::quit = true;
	ServerComm::mainsem.Post();
}

void ServerMessage(const std::string& str)
{
	SetColor(ANSI_COLOR_YELLOW);
	printf("%s\n", str.c_str());
	SetColor(ANSI_COLOR_RESET);
}

void LibraryStartLoad(const std::string& str)
{
	long pid = strtol(str.c_str(), nullptr, 10);
	ServerStartLoad(pid);
}

void LibraryReceive(const std::string& str)
{
	ServerReceiveModule(str.c_str(), str.size());
}

void LibraryAllocate(const std::string& str)
{
	uint32_t allocSize = 0;
	sscanf(str.c_str(), "%u", &allocSize);
	uint64_t addr = ServerAllocateModule(allocSize);
	char ret[20];
	snprintf(ret, 20, "%llu\n", (unsigned long long)addr);
	ServerComm::Send(ret);
}

void LibraryUnloadID(const std::string& str)
{
	int libID = 0;
	sscanf(str.c_str(), "%d", &libID);
	ServerUnloadModule(libID);
}

void SubscriptionList(const std::string& str)
{
	//STPRINT("Receive subscription list!\n");
    subscriptionList.clear();
    char dstr[256];
	strncpy(dstr, str.c_str(), 255);
	for (const char* s = strtok(dstr, "\n"); s; s = strtok(nullptr, "\n")) {
		char name[128], sub_date[64], int_name[32];
		sscanf(s, "%[^:]:%[^:]:%s", name, sub_date, int_name);
		subscriptionList.push_back(SubscriptionEntry(name, sub_date, int_name));
	}
}
