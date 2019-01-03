#include <stdio.h>
#include "main.h"
#include "settings.h"
#include "loader.h"
#include "server_comm.h"

int main()
{
	DrawSplash();

	ServerComm::Initialize();

	int failcount = 0;

	while (failcount < 10) {
		SetColor(ANSI_COLOR_RESET);
		STPRINT("Choose one of these options:\n1. Load the cheat\n2. Open settings console\nEOF. Quit\n\nEnter choice> ");

		int val = 0;

		if (scanf("%d", &val) == EOF)
			break;

		switch(val) {
		  case 1:
			  Load();
			  break;
		  case 2:
			  SettingsConsole();
			  break;
		  default:
			  goto failcount_increase;
		}

		failcount = -1;
	  failcount_increase:
		failcount++;
	}

	ServerComm::Stop();

	return 0;
}
