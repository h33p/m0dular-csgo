#include <stdio.h>
#include <iostream>
#include <sstream>
#include "../core/settings.h"
#include "../core/binds.h"
#include "../modules/keycode/keyid.h"
#include "../sdk/framework/utils/stackstring.h"

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

static uintptr_t clAllocBase = 0;
generic_free_list_allocator<clAllocBase, true> clAlloc(10000, PlacementPolicy::FIND_FIRST);

static void DrawSplash();

static void Handle(const char* command, const char* name, const char** cmds, int n);

#ifdef __linux__
#define SetColor(col) printf(col)
#else
void SetColor(int col)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, col);
}
#endif

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

typedef void(*PrintFn)(void*);
typedef void(*PrintGroupFn)(crcs_t);

struct ConsoleSetting
{
	const int id;
	const crcs_t crc;
	const char* name;
	const crcs_t typeNameCRC;
	void* settingPtr;
	const PrintFn printFunction;
};

typedef int(*CommandFn)(const ConsoleSetting&, const char**, int);
typedef int(*UnnamedCommandFn)(const char**, int);

template<typename T>
struct ConsoleCommand
{
	const crcs_t crc;
	T handler;
	const char* usage;
	const char* info;
};


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

static int HandleGet(const ConsoleSetting& set, const char** cmds, int n);
static int HandleSet(const ConsoleSetting& set, const char** cmds, int n);
static int HandleBind(const ConsoleSetting& set, const char** cmds, int n);

static int Help(const char** cmds, int n);
static int PrintAll(const char** cmds, int n);
static int Binds(const char** cmds, int n);
static int Save(const char** cmds, int n);
static int Load(const char** cmds, int n);

static int slid = 0;

#define STPRINT(text) printf("%s", (const char*)ST(text))
#define STTYPE(name) decltype(StackString(name))
#define STALLOC(name) *(new (clAlloc.allocate<STTYPE(name)>(1)) StackString(name))

ConsoleSetting settingList[] =
{
#define HANDLE_OPTION(type, defaultVal, name, ...) {slid++, CCRC32(#name), STALLOC(#name), CCRC32(#type), &Settings::name, &GetPrintSetting<type, decltype(Settings::name)::Get> },
#include "../core/option_list.h"
};

ConsoleCommand<CommandFn> commandList[] =
{
	{ CCRC32("get"), HandleGet, STALLOC("get option"), STALLOC("Get the value of an option") },
	{ CCRC32("set"), HandleSet, STALLOC("set option value"), STALLOC("Set a value of an option") },
	{ CCRC32("bind"), HandleBind, STALLOC("bind option scancode_name HOLD|TOGGLE value"), STALLOC("Bind a key to a specific option") },
};


ConsoleCommand<UnnamedCommandFn> unnamedCommandList[] =
{
	{ CCRC32("help"), Help, "help", "Print this message" },
	{ CCRC32("print_all"), PrintAll, "print_all", "Print all settings values" },
	{ CCRC32("binds"), Binds, "binds", "Show active binds" },
	{ CCRC32("save"), Save, "save filename", "Save configuration to file" },
	{ CCRC32("load"), Load, "load filename", "Load a configuration file" },
};

static void PrintUsageError(const char* usage)
{
	printf("\n%s: %s", (const char*)ST("Usage"), usage);
}

static void PrintUsage(const char* usage, const char* info)
{
	printf("\n%s\t%s", usage, info);
}

static void Handle(const char* command, const char* name, const char** cmds, int n)
{
	crcs_t commandCrc = Crc32(command, strlen(command));

	const char* usage = "";

	for (auto& i : unnamedCommandList) {
		if (commandCrc == i.crc) {
			i.handler(cmds - 1, n + 1);
		    usage = i.usage;
		    return;
		}
	}

	CommandFn cmd = nullptr;

	for (auto& i : commandList) {
		if (commandCrc == i.crc) {
			cmd = i.handler;
			usage = i.usage;
			break;
		}
	}

	if (!cmd) {
		STPRINT("Command not found!\n");
		return;
	}

	printf("%s: ", name);
	crcs_t crc = Crc32(name, strlen(name));

	bool found = false;

	for (auto& i : settingList) {
		if (i.crc == crc) {
			int ret = cmd(i, cmds, n);

			if (ret == 1)
			    PrintUsageError(usage);

			found = true;
			break;
		}
	}

	if (!found)
	    STPRINT("not found!");

	STPRINT("\n");
}

static int HandleGet(const ConsoleSetting& set, const char** cmds, int n)
{
	set.printFunction(set.settingPtr);

	return 0;
}

static int HandleSet(const ConsoleSetting& set, const char** cmds, int n)
{
	if (n <= 0) {
		STPRINT("Value not changed!\n");
		return 1;
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
		  return 1;
	}

	STPRINT("Value set successfully!");

	return 0;
}

static int HandleBind(const ConsoleSetting& set, const char** cmds, int n)
{
	if (n < 3)
		return 1;

	int key = keyid_code_from_name(cmds[0]);

	if (key < 0) {
		STPRINT("Scancode not found!");
		return 0;
	}

	BindMode mode = BindMode::HOLD;

	if (!strcmp(cmds[1], ST("HOLD")))
		mode = BindMode::HOLD;
	else if (!strcmp(cmds[1], ST("TOGGLE")))
		mode = BindMode::TOGGLE;
	else {
		STPRINT("Invalid mode!");
		return 1;
	}

	BindManager::sharedInstance->binds[key].mode = mode;

	//TODO: store templated function to do the job
	switch(set.typeNameCRC) {
	  case CCRC32("float"):
		  BindManager::sharedInstance->binds[key].BindPointer(BindManager::sharedInstance->bindList[set.id], ParseStr<float>(cmds[2]));
		  break;
	  case CCRC32("int"):
		  BindManager::sharedInstance->binds[key].BindPointer(BindManager::sharedInstance->bindList[set.id], ParseStr<int>(cmds[2]));
		  break;
	  case CCRC32("bool"):
		  BindManager::sharedInstance->binds[key].BindPointer(BindManager::sharedInstance->bindList[set.id], ParseStr<bool>(cmds[2]));
		  break;
	  default:
		  return 1;
	}


	STPRINT("Key bound!");

	return 0;
}

static int PrintAll(const char** cmds, int n)
{
	STPRINT("Printing all setting values:\n");

	for (auto& i : settingList) {
		printf("%s: ", i.name);
		i.printFunction(i.settingPtr);
		STPRINT("\n");
	}

	return 0;
}

static int Save(const char** cmds, int n)
{
	char filename[512];
	filename[0] = '\0';

	int i = 0;

	if (n < 1)
		return 1;

	strcat(filename, cmds[0]);
	for (int i = 1; i < n; i++) {
		strcat(filename, " ");
		strcat(filename, cmds[i]);
	}

	std::vector<unsigned char> res;

	BindManager::SerializeBinds(res);
	Settings::globalSettings->Serialize(res);

	printf("Writing %zu bytes to %s\n", res.size(), filename);

	FILE* fp = fopen(filename, "w");
	fwrite(res.data(), 1, res.size(), fp);
	fclose(fp);

	STPRINT("Done saving!\n");

	return 0;
}

static int Load(const char** cmds, int n)
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

	size_t idx = BindManager::LoadBinds(buf, 0);
	Settings::globalSettings->Initialize(buf, idx);

	return 0;
}

static int Help(const char** cmds, int n)
{

	STPRINT("\nCommand list:\n");

	for (auto& i : unnamedCommandList)
		PrintUsage(i.usage, i.info);

	STPRINT("\n\nSettings commands:\n");

	for (auto& i : commandList)
		PrintUsage(i.usage, i.info);

	STPRINT("\n");

	return 0;
}

template<typename T>
static void PrintBind(int i, const BindKey& bind, const ConsoleSetting& setting)
{
	printf("%s\t%s %s ", keyid_name_from_code(i), setting.name, bind.mode == BindMode::HOLD ? "HOLD" : "TOGGLE");

	auto keyBind = (typename BindImpl<T>::BindPointer)(uintptr_t)&*bind.pointer;

	std::cout << keyBind->value << "\n";
}

static int Binds(const char** cmds, int n)
{

	STPRINT("\nActive keybinds:\n");

	//This is hella inefficient but okay
	for (int i = 0; i < sizeof(BindManager::sharedInstance->binds) / sizeof(BindKey); i++) {

		const BindKey& bind = BindManager::sharedInstance->binds[i];

		if (!bind.pointer)
			continue;

		for (const auto& set : settingList) {
			if (bind.pointer->handler == BindManager::sharedInstance->bindList[set.id]) {
				switch(set.typeNameCRC) {
				  case CCRC32("float"):
					  PrintBind<float>(i, bind, set);
					  break;
				  case CCRC32("int"):
					  PrintBind<bool>(i, bind, set);
					  break;
				  case CCRC32("bool"):
					  PrintBind<bool>(i, bind, set);
					  break;
				}
			}
		}
	}

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

	//TODO: consider not stack stringing the logos due to binary size
	SetColor(ANSI_COLOR_GREEN);
	if (columns < 81) {
	    STPRINT("        _                          \n");
	    STPRINT(" ._ _  / \\  _|     |  _. ._   _  _ \n");
	    STPRINT(" | | | \\_/ (_| |_| | (_| | o (_ (_ \n");
	    STPRINT("                                   \n\n");
	} else {
#ifdef _WIN32
		if (columns > 150)
		    STPRINT("_________________________/\\\\\\\\\\\\\\____________/\\\\\\_________________/\\\\\\\\\\\\___________________________________________________________________        \n"
				" _______________________/\\\\\\/////\\\\\\_________\\/\\\\\\________________\\////\\\\\\___________________________________________________________________       \n"
				"  ______________________/\\\\\\____\\//\\\\\\________\\/\\\\\\___________________\\/\\\\\\___________________________________________________________________      \n"
				"   ____/\\\\\\\\\\__/\\\\\\\\\\___\\/\\\\\\_____\\/\\\\\\________\\/\\\\\\___/\\\\\\____/\\\\\\____\\/\\\\\\_____/\\\\\\\\\\\\\\\\\\_____/\\\\/\\\\\\\\\\\\\\____________/\\\\\\\\\\\\\\\\_____/\\\\\\\\\\\\\\\\_     \n"
				"    __/\\\\\\///\\\\\\\\\\///\\\\\\_\\/\\\\\\_____\\/\\\\\\___/\\\\\\\\\\\\\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\____\\////////\\\\\\___\\/\\\\\\/////\\\\\\_________/\\\\\\//////____/\\\\\\//////__    \n"
				"     _\\/\\\\\\_\\//\\\\\\__\\/\\\\\\_\\/\\\\\\_____\\/\\\\\\__/\\\\\\////\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\______/\\\\\\\\\\\\\\\\\\\\__\\/\\\\\\___\\///_________/\\\\\\__________/\\\\\\_________   \n"
				"      _\\/\\\\\\__\\/\\\\\\__\\/\\\\\\_\\//\\\\\\____/\\\\\\__\\/\\\\\\__\\/\\\\\\__\\/\\\\\\___\\/\\\\\\____\\/\\\\\\_____/\\\\\\/////\\\\\\__\\/\\\\\\_______________\\//\\\\\\________\\//\\\\\\________  \n"
				"       _\\/\\\\\\__\\/\\\\\\__\\/\\\\\\__\\///\\\\\\\\\\\\\\/___\\//\\\\\\\\\\\\\\/\\\\_\\//\\\\\\\\\\\\\\\\\\___/\\\\\\\\\\\\\\\\\\_\\//\\\\\\\\\\\\\\\\/\\\\_\\/\\\\\\__________/\\\\\\__\\///\\\\\\\\\\\\\\\\__\\///\\\\\\\\\\\\\\\\_ \n"
				"        _\\///___\\///___\\///_____\\///////______\\///////\\//___\\/////////___\\/////////___\\////////\\//__\\///__________\\///_____\\////////_____\\////////__\n\n");
		else
			STPRINT("\n"
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
		STPRINT("\n"
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
