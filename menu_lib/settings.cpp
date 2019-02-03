#include <stdio.h>
#include <iostream>
#include <sstream>
#include "settings.h"
#include "../modules/keycode/keyid.h"
#include "../client/main.h"
#include "../client/game_settings_module.h"

void OnLoad(ConsoleSetting** sets, size_t* size);

static uintptr_t clAllocBase = 0;
generic_free_list_allocator<clAllocBase, true> clAlloc(10000, PlacementPolicy::FIND_FIRST);

static ConsoleSetting* settingList = nullptr;
static size_t settingsCount = 0;


static void Handle(const char* command, const char* name, const char** cmds, int n);

static void HandleCommand(const char** cmds, int n)
{
	Handle(cmds[0], n >= 2 ? cmds[1] : nullptr, cmds + 2, n - 2);
}

void SettingsConsole()
{
	OnLoad(&settingList, &settingsCount);
	SetColor(ANSI_COLOR_RESET);
	STPRINT("\nSettings Console:\n");

	char buf[512];
	char* cmds[32];

	while (true) {
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

		//Hardcoded exit
		if (i > 0 && !strcmp(cmds[0], ST("exit")))
			break;

		if (i > 0)
			HandleCommand((const char**)cmds, i);

		printf("> ");
	}

	STPRINT("quit\n");
}

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

	if (name) {
		for (auto& i : commandList) {
			if (commandCrc == i.crc) {
				cmd = i.handler;
				usage = i.usage;
				break;
			}
		}
	}

	if (!cmd) {
		STPRINT("Command not found!\n");
		return;
	}

	printf("%s: ", name);
	crcs_t crc = Crc32(name, strlen(name));

	bool found = false;

	for (size_t o = 0; o < settingsCount; o++) {
		auto& i = settingList[o];
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

	for (size_t o = 0; o < settingsCount; o++) {
		auto& i = settingList[o];
		printf("%s: ", i.name);
		i.printFunction(i.settingPtr);
		printf(" - %s\n", i.description);
	}

	return 0;
}

static int Save(const char** cmds, int n)
{
	char filename[512];
	filename[0] = '\0';

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
	for (size_t i = 0; i < sizeof(BindManager::sharedInstance->binds) / sizeof(BindKey); i++) {

		const BindKey& bind = BindManager::sharedInstance->binds[i];

		if (!bind.pointer)
			continue;

		for (size_t i = 0; i < settingsCount; i++) {
			const auto& set = settingList[i];
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

