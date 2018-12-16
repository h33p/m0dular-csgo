#include <stdio.h>
#include "../core/settings.h"
#include <iostream>
#include <sstream>

#ifdef __linux__
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#else
#define ANSI_COLOR_RED 4
#define ANSI_COLOR_GREEN 2
#define ANSI_COLOR_YELLOW 6
#define ANSI_COLOR_BLUE 1
#define ANSI_COLOR_MAGENTA 5
#define ANSI_COLOR_CYAN 3
#define ANSI_COLOR_RESET 7
#endif

#ifdef __linux__
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <winternl.h>
#include <io.h>
#endif

#ifdef __linux__
#define SetColor(col) printf(col)
#else
void SetColor(int col)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, col);
}
#endif

typedef void(*PrintFn)(void*);
typedef void(*PrintGroupFn)(crcs_t);

struct ConsoleSetting
{
	const crcs_t crc;
	const crcs_t typeNameCRC;
	void* settingPtr;
	const PrintFn printFunction;
};

typedef void(*CommandFn)(const ConsoleSetting&, const char**, int);
typedef void(*UnnamedCommandFn)(const char**, int);

template<typename T>
struct ConsoleCommand
{
	const crcs_t crc;
    T handler;
};


static void DrawSplash();

template<typename T>
struct ConsoleCommand;

template<auto& Group, typename T>
T GetSetting(crcs_t crc)
{
    return Group->template GetRuntime<T>(crc);
}

template<typename T, auto& Fn>
void GetPrintSetting(void* in)
{
	typedef T(*FnVFn)(void*);
	std::cout << ((FnVFn)Fn)(in);
}

template<typename T, auto& Fn>
void GetPrintSettingGroups(crcs_t crc)
{
	std::cout << GetSetting<Settings::globalSettings, T>(crc) << " " << GetSetting<Settings::bindSettings, T>(crc);
}

template<auto& Group, typename T>
void SetSetting(crcs_t crc, T value)
{
    Group->SetRuntime(value, crc);
}

template<typename T>
T ParseStr(const char* str)
{
	std::istringstream ss(str);
	T ret;
	ss >> ret;
	return ret;
}

static void HandleGet(const ConsoleSetting& set, const char** cmds, int n);
static void HandleSet(const ConsoleSetting& set, const char** cmds, int n);

static void PrintAll(const char** cmds, int n);
static void Save(const char** cmds, int n);
static void Load(const char** cmds, int n);

ConsoleSetting settingList[] =
{
#define HANDLE_OPTION(type, defaultVal, name, ...) {CCRC32(#name), CCRC32(#type), &Settings::name, &GetPrintSetting<type, decltype(Settings::name)::Get> },
#include "../core/option_list.h"
};

ConsoleCommand<CommandFn> commandList[] =
{
	{ CCRC32("get"), HandleGet },
	{ CCRC32("set"), HandleSet },
};


ConsoleCommand<UnnamedCommandFn> unnamedCommandList[] =
{
	{ CCRC32("print_all"), PrintAll },
	{ CCRC32("save"), Save },
	{ CCRC32("load"), Load },
};

static void Handle(const char* command, const char* name, const char** cmds, int n)
{
	crcs_t commandCrc = Crc32(command, strlen(command));

	for (auto& i : unnamedCommandList) {
		if (commandCrc == i.crc) {
			i.handler(cmds - 1, n + 1);
		    return;
		}
	}

	CommandFn cmd = nullptr;

	for (auto& i : commandList) {
		if (commandCrc == i.crc) {
			cmd = i.handler;
			break;
		}
	}

	if (!cmd) {
		printf("Command not found!\n");
		return;
	}

	printf("%s: ", name);
	crcs_t crc = Crc32(name, strlen(name));

	bool found = false;

	for (auto& i : settingList) {
		if (i.crc == crc) {
			cmd(i, cmds, n);
			found = true;
			break;
		}
	}

	if (!found)
		printf("not found!");

	printf("\n");
}

static void HandleCommand(const char** cmds, int n)
{
	Handle(cmds[0], cmds[1], cmds + 2, n - 2);
}

int main()
{
	DrawSplash();
	SetColor(ANSI_COLOR_RESET);
	printf("\nSettings Console:\n");

	char buf[512];
	char* cmds[32];

	while (true) {
		printf("> ");
		char* ret = fgets(buf, 512, stdin);

		for (char* s = buf; *s != '\0'; s++) {
			if (*s == '\n') {
				*s = '\0';
				s--;
			}
		}

		if (!ret)
			break;

		int i = 0;
		for (char* s = strtok(buf, " "); s && i < 32; s = strtok(nullptr, " "))
			cmds[i++] = s;

	    HandleCommand((const char**)cmds, i);
	}

	getchar();
	return 0;
}

static void DrawSplash()
{
	int columns = 0;
#ifdef __linux__
	struct winsize size;
	ioctl(STDOUT_FILENO,TIOCGWINSZ,&size);
	columns = size.ws_col;
#else
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
#endif

	SetColor(ANSI_COLOR_GREEN);
	if (columns < 81) {
	    printf("        _                          \n");
		printf(" ._ _  / \\  _|     |  _. ._   _  _ \n");
		printf(" | | | \\_/ (_| |_| | (_| | o (_ (_ \n");
		printf("                                   \n\n");
	} else {
#ifdef _WIN32
		if (columns > 150)
			printf("_________________________/\\\\\\\\\\\\\\____________/\\\\\\_________________/\\\\\\\\\\\\___________________________________________________________________        \n"
				" _______________________/\\\\\\/////\\\\\\_________\\/\\\\\\________________\\////\\\\\\___________________________________________________________________       \n"
				"  ______________________/\\\\\\____\\//\\\\\\________\\/\\\\\\___________________\\/\\\\\\___________________________________________________________________      \n"
				"   ____/\\\\\\\\\\__/\\\\\\\\\\___\\/\\\\\\_____\\/\\\\\\________\\/\\\\\\___/\\\\\\____/\\\\\\____\\/\\\\\\_____/\\\\\\\\\\\\\\\\\\_____/\\\\/\\\\\\\\\\\\\\____________/\\\\\\\\\\\\\\\\_____/\\\\\\\\\\\\\\\\_     \n"
				"    __/\\\\\\///\\\\\\\\\\///\\\\\\_\\/\\\\\\_____\\/\\\\\\___/\\\\\\\\\\\\\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\____\\////////\\\\\\___\\/\\\\\\/////\\\\\\_________/\\\\\\//////____/\\\\\\//////__    \n"
				"     _\\/\\\\\\_\\//\\\\\\__\\/\\\\\\_\\/\\\\\\_____\\/\\\\\\__/\\\\\\////\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\______/\\\\\\\\\\\\\\\\\\\\__\\/\\\\\\___\\///_________/\\\\\\__________/\\\\\\_________   \n"
				"      _\\/\\\\\\__\\/\\\\\\__\\/\\\\\\_\\//\\\\\\____/\\\\\\__\\/\\\\\\__\\/\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\_____/\\\\\\/////\\\\\\__\\/\\\\\\_______________\\//\\\\\\________\\//\\\\\\________  \n"
				"       _\\/\\\\\\__\\/\\\\\\__\\/\\\\\\__\\///\\\\\\\\\\\\\\/___\\//\\\\\\\\\\\\\\/\\\\_\\//\\\\\\\\\\\\\\\\\\___/\\\\\\\\\\\\\\\\\\_\\//\\\\\\\\\\\\\\\\/\\\\_\\/\\\\\\__________/\\\\\\__\\///\\\\\\\\\\\\\\\\__\\///\\\\\\\\\\\\\\\\_ \n"
				"        _\\///___\\///___\\///_____\\///////______\\///////\\//___\\/////////___\\/////////___\\////////\\//__\\///__________\\///_____\\////////_____\\////////__\n\n");
		else
			printf("\n"
				"               $$$$$$\\        $$\\           $$\\                                         \n"
				"              $$$ __$$\\       $$ |          $$ |                                        \n"
				"$$$$$$\\$$$$\\  $$$$\\ $$ | $$$$$$$ |$$\\   $$\\ $$ | $$$$$$\\   $$$$$$\\   $$$$$$$\\  $$$$$$$\\ \n"
				"$$  _$$  _$$\\ $$\\$$\\$$ |$$  __$$ |$$ |  $$ |$$ | \\____$$\\ $$  __$$\\ $$  _____|$$  _____|\n"
				"$$ / $$ / $$ |$$ \\$$$$ |$$ /  $$ |$$ |  $$ |$$ | $$$$$$$ |$$ |  \\__|$$ /      $$ /      \n"
				"$$ | $$ | $$ |$$ |\\$$$ |$$ |  $$ |$$ |  $$ |$$ |$$  __$$ |$$ |      $$ |      $$ |      \n"
				"$$ | $$ | $$ |\\$$$$$$  /\\$$$$$$$ |\\$$$$$$  |$$ |\\$$$$$$$ |$$ |$$\\   \\$$$$$$$\\ \\$$$$$$$\\ \n"
				"\\__| \\__| \\__| \\______/  \\_______| \\______/ \\__| \\_______|\\__|\\__|   \\_______| \\_______|\n"
				"                                                                                        \n"
				"                                                                                        \n"
				"                                                                                        \n"
				"");
#else
		printf("\n"
			   "███╗   ███╗ ██████╗ ██████╗ ██╗   ██╗██╗      █████╗ ██████╗     ██████╗ ██████╗\n"
			   "████╗ ████║██╔═████╗██╔══██╗██║   ██║██║     ██╔══██╗██╔══██╗   ██╔════╝██╔════╝\n"
			   "██╔████╔██║██║██╔██║██║  ██║██║   ██║██║     ███████║██████╔╝   ██║     ██║     \n"
			   "██║╚██╔╝██║████╔╝██║██║  ██║██║   ██║██║     ██╔══██║██╔══██╗   ██║     ██║     \n"
			   "██║ ╚═╝ ██║╚██████╔╝██████╔╝╚██████╔╝███████╗██║  ██║██║  ██║██╗╚██████╗╚██████╗\n"
			   "╚═╝     ╚═╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═════╝\n"
			   "                                                                                \n"
			   "");
#endif
	}
}

static void HandleGet(const ConsoleSetting& set, const char** cmds, int n)
{
	set.printFunction(set.settingPtr);
}

static void HandleSet(const ConsoleSetting& set, const char** cmds, int n)
{

	if (n < 0) {
		printf("Value not changed!\n");
		return;
	}

	switch(set.typeNameCRC) {
	  case CCRC32("float"):
		  SetSetting<Settings::globalSettings>(set.crc, ParseStr<float>(cmds[0]));
		  break;
	  case CCRC32("int"):
		  SetSetting<Settings::globalSettings>(set.crc, ParseStr<int>(cmds[0]));
		  break;
	  case CCRC32("bool"):
		  SetSetting<Settings::globalSettings>(set.crc, ParseStr<bool>(cmds[0]));
		  break;
	  default:
		  return;
	}

	printf("Value set successfully!");
}

static void PrintAll(const char** cmds, int n)
{
	printf("Printing all setting values:\n");

	for (auto& i : settingList) {
		printf("%x: ", i.crc);
		i.printFunction(i.settingPtr);
		printf("\n");
	}
}

static void Save(const char** cmds, int n)
{
	char filename[512];
	filename[0] = '\0';

	strcat(filename, cmds[0]);

	for (int i = 1; i < n; i++) {
		strcat(filename, " ");
		strcat(filename, cmds[n]);
	}

	auto res = Settings::globalSettings->Serialize();

	printf("Writing %zu bytes to %s\n", res.size(), filename);

	FILE* fp = fopen(filename, "w");
	fwrite(res.data(), 1, res.size(), fp);
	fclose(fp);

	printf("Done saving!\n");
}

static void Load(const char** cmds, int n)
{
	char filename[512];
	filename[0] = '\0';

	strcat(filename, cmds[0]);

	for (int i = 1; i < n; i++) {
		strcat(filename, " ");
		strcat(filename, cmds[n]);
	}

	FILE* fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	size_t sz = ftell(fp);
	rewind(fp);

	printf("Reading %zu bytes from %s\n", sz, filename);

	std::vector<unsigned char> buf(sz);
	fread(buf.data(), 1, sz, fp);

	Settings::globalSettings->Initialize(buf);
}
