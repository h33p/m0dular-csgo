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
	ServerComm::mainsem.Post();
}

void LoginInvHWID(const std::string& str)
{
	SetColor(ANSI_COLOR_RED);
	STPRINT("Invalid HWID!\n");
	SetColor(ANSI_COLOR_RESET);
	ServerComm::quit = true;
	ServerComm::mainsem.Post();
}

void LoginInvIP(const std::string& str)
{
	SetColor(ANSI_COLOR_RED);
	STPRINT("Login blocked! Try again later...\n");
	SetColor(ANSI_COLOR_RESET);
	ServerComm::quit = true;
	ServerComm::mainsem.Post();
}

void ServerMessage(const std::string& str)
{
	SetColor(ANSI_COLOR_YELLOW);
	printf("%s\n", str.c_str());
	SetColor(ANSI_COLOR_RESET);
}

void CheatLibraryReceive(const std::string& str)
{
    //printf("Receive library payload size %u!\n", (uint32_t)str.size());
	ServerReceiveModule(str.c_str(), str.size());
}

void LibraryAllocate(const std::string& str)
{
	//STPRINT("Receive allocate request!\n");
	uint32_t allocSize = 0;
	sscanf(str.c_str(), "%u", &allocSize);
	uint64_t addr = ServerAllocateModule(allocSize);
	char ret[20];
	snprintf(ret, 20, "%llu\n", (unsigned long long)addr);
	ServerComm::Send(ret);
}

void SettingsLibraryReceive(const std::string& str)
{
//TODO: have a separate game-specific settings library that controls all the configuration
}

void SubscriptionList(const std::string& str)
{
	//STPRINT("Receive subscription list!\n");
    subscriptionList.clear();
    char dstr[256];
	strncpy(dstr, str.c_str(), 255);
	for (const char* s = strtok(dstr, "\n"); s; s = strtok(nullptr, "\n")) {
		char name[128], sub_date[64], game_name[32], int_name[32];
		sscanf(s, "%[^:]:%[^:]:%[^:]:%s", name, sub_date, int_name, game_name);
		subscriptionList.push_back(SubscriptionEntry(name, sub_date, game_name, int_name));
	}
}
