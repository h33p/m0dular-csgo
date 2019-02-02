#include <stdio.h>
#include "main.h"
#include "game_settings_module.h"
#include "loader.h"
#include "server_comm.h"
#include "../sdk/framework/utils/threading.h"

#ifdef __linux__
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct termios oldt;
#else
uint32_t oldMode = 0;
#endif
int echoEnableCount = 0;

int main()
{
	Threading::InitThreads();
	DrawSplash();
	SetColor(ANSI_COLOR_RESET);

	if (!ServerComm::Initialize())
		return 1;

	int failcount = 0;

	int menuModule = 0;
	int cheatModule = 0;

	while (failcount++ < 10) {
		STPRINT("Select the cheat:\n");
		STPRINT("ID\tVALID_UNTIL\tNAME\n");

		for (size_t i = 0; i < subscriptionList.size(); i++)
			printf("%lu\t%s\t%s\n", i + 1, subscriptionList[i].subscription_date, subscriptionList[i].name);

		uint32_t loadID = 0;

		scanf("%u", &loadID);

		if (--loadID < subscriptionList.size()) {
			failcount = 0;

			menuModule = 1;//LoadCheatMenu(loadID);

			if (!menuModule)
				continue;

			while (failcount < 10) {
				SetColor(ANSI_COLOR_RESET);
				STPRINT("Choose one of these options:\n1. Load the cheat\n2. Open cheat menu\n3. Unload the cheat\nQ. Quit\n\nEnter choice> ");

			    char val[256];

				if (scanf("%s", val) == EOF || val[0] == 'q' || val[0] == 'Q')
					break;

				int valInt = strtol(val, nullptr, 10);

				switch(valInt) {
				  case 1:
					  if (!cheatModule)
						  cheatModule = Load(loadID);
					  else
						  STPRINT("The cheat has already been loaded!\n");
					  break;
				  case 2:
					  Menu();
					  break;
				  case 3:
					  if (cheatModule) {
						  UnloadModule(cheatModule);
						  cheatModule = 0;
					  } else
						  STPRINT("The cheat is not loaded at the moment!\n");
					  break;
				  default:
					  goto failcount_increase;
				}

				failcount = -1;
			  failcount_increase:
				failcount++;
			}

			//UnloadModule(menuModule);
			menuModule = 0;
		}
	}

	Threading::EndThreads();
	ServerComm::Stop();

	return 0;
}

void DisableEcho()
{
	if (echoEnableCount++)
		return;

#if defined(__linux__) || defined(__APPLE__)
	struct termios newt;
	if(tcgetattr(0, &oldt))
		fprintf(stderr, "Error getting term attribs\n");
	if(tcgetattr(0, &newt))
		fprintf(stderr, "Error getting term attribs\n");
	cfmakeraw(&newt);

	if(tcsetattr(0, TCSANOW, &newt))
		fprintf(stderr, "Error setting term attribs\n");
#else
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hStdin, (LPDWORD)&oldMode);
	SetConsoleMode(hStdin, oldMode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
#endif
}

void EnableEcho()
{
	if (--echoEnableCount != 0)
		return;

#ifdef __linux__
	if(tcsetattr(0, TCSANOW, &oldt))
		fprintf(stderr, "Error setting term attribs\n");
#else
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hStdin, oldMode);
#endif
}

void EchoInput(char* buf, size_t size, char mask)
{
	DisableEcho();
	int i = 0;
#ifdef __linux__
	while (1) {
	    buf[i] = getc(stdin);
		if (i && buf[i] == 13) {
			buf[i] = '\0';
			break;
		} else if (buf[i] == 8) {
			if (i > 0) {
				write(STDOUT_FILENO, "\b \b", 3);
				fflush(stdout);
				i--;
			}
			continue;
		}

		if (i >= size - 1)
			continue;

		putc(mask ? mask : buf[i], stdout);
		fflush(stdout);
		i++;
	}
#else
	constexpr char BACKSPACE = 8;
	constexpr char RETURN = 13;

	unsigned char ch = 0;

	uint32_t conMode;
	uint32_t dwRead;

	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

	while (ReadConsoleA(hIn, &ch, 1, (LPDWORD)&dwRead, NULL) && (!i || ch != RETURN)) {
		if (ch == BACKSPACE) {
			if (i > 0) {
				printf("\b \b");
				fflush(stdout);
			    buf[i--] = '\0';
			}
		} else if (ch != RETURN && ch != '\n' && i < size - 1){
		    buf[i++] = ch;
		    buf[i] = '\0';
			putc(mask ? mask : ch, stdout);
			fflush(stdout);
		}
	}
#endif
	EnableEcho();
	printf("\n");
}
