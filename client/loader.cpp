#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "loader.h"
#include "main.h"
#include "server_comm.h"
#include "../sdk/framework/utils/pattern_scan.h"
#include "../sdk/framework/utils/handles.h"
#include "../sdk/framework/utils/semaphores.h"
#include "../sdk/framework/utils/threading.h"

void StartLoad(long pid);
int PerformLoad(void*);
long long ProcFind(const char* name);
static std::vector<ProcessEntry> GetProcessList();

Semaphore loaderSem;

//These are set by the loader during the loading process. Not thread safe, obviously
int loadRet = 0;

static int lastModuleID = 0;
std::vector<SubscriptionEntry> subscriptionList;
std::vector<ModuleEntry> loadedModules;

int Load(int loadID)
{
	loadRet = 0;

	//Send updated process list to the server
	auto list = GetProcessList();

	char* procBuf = (char*)malloc(list.size() * 128);

	procBuf[0] = '\0';

	strcat(procBuf, ST("pl"));

	for (const ProcessEntry& i : list) {
		char procEntry[128];
		snprintf(procEntry, 127, "\n%lld:%s", i.pid, i.name);
		strcat(procBuf, procEntry);
	}

	ServerComm::Send(std::string(procBuf));

	free(procBuf);

	char loadBuf[256];

	snprintf(loadBuf, 255, "%s %s", (char*)ST("ldm"), subscriptionList[loadID].int_name);

	ServerComm::Send(std::string(loadBuf));

	//The lock will be unlocked by the loader
	loaderSem.TimedWait(60000);

	if (loadRet) {
		STPRINT("Failed to load! Error code ");
		printf("%d\n", loadRet);
		return 0;
	} else
		STPRINT("Loaded successfully!\n");

	return lastModuleID;
}

int LoadCheatMenu(int loadID)
{
	loadRet = 0;

	char loadBuf[256];

	snprintf(loadBuf, 255, "%s %s", (char*)ST("lds"), subscriptionList[loadID].int_name);

	ServerComm::Send(std::string(loadBuf));

	//The lock will be unlocked by the loader
	loaderSem.TimedWait(60000);

	if (loadRet) {
		STPRINT("Failed to initialize menu! Error code ");
		printf("%d\n", loadRet);
		return 0;
	}

	return lastModuleID;
}

#if defined(__linux__)
#define GAME_NAME "csgo_linux64"

#include <sys/mman.h>
#include <sys/stat.h>
#include "pmparser.h"

[[nodiscard]] char* exec(const char* cmd) {
	char* outBuf = (char*)malloc(128);
	char* buf = outBuf;
	size_t bufSize = 128;
	FILE* pipe = popen(cmd, "r");
	try {
		while (!feof(pipe)) {
			char* ret = fgets(buf, bufSize - (size_t)(buf - outBuf), pipe);
			if (ret) {
				if (bufSize == (size_t)(ret - outBuf)) {
					outBuf = (char*)realloc((void*)outBuf, bufSize * 2);
					buf = outBuf + bufSize;
					bufSize *= 2;
				} else
					buf = ret;
			}
		}
	} catch (...) {
		pclose(pipe);
		throw;
	}
	pclose(pipe);
	return outBuf;
}

long long ProcFind(const char* gameName)
{
	unsigned int pid = 0;

	char buf[128];
	snprintf(buf, 128, "%s %s", (char*)ST("pidof -x"), gameName);
	char* ret = exec(buf);

	if (ret) {
		sscanf(ret, "%d", &pid);
		free(ret);
	}

	return pid;
}

void printfree(const char* buf)
{
	printf("%s", buf);
	free((void*)buf);
}


//We are too lazy to perform manual ELF rellocations so just let dlopen do all the work and then remap it so that the mapping is anonymous
int PerformLoad(const char* subname, long long pidIn)
{
	/*
	char temp[30];
	int ret = 0;

	temp[0] = '\0';

	for (int i = 0; i < 10; i++)
		temp[i] = 'a' + rand() % ('z' - 'a');

	temp[10] = '\0';

	int fd = shm_open(temp, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

	pid_t pid = pidIn;

	if (fd ^ -1) {
		if (ftruncate(fd, size) ^ -1) {
			write(fd, buf, size);
			char path[40];
			path[0] = 0;
			strcat(path, (const char*)ST("/dev/shm/"));
			strcat(path, temp);

			char strbuf[512];
			char strbuf2[2048];

			snprintf(strbuf, 512, ST("sudo gdb -n -q -batch"
									 " -ex \"set logging file /dev/stdout\""
									 " -ex \"set logging redirect on\""
									 " -ex \"set logging on\""
									 //" -ex \"set verbose off\""
									 " -ex \"attach %d\""
									 " -ex \"set \\$dlopen = (void*(*)(char*, int)) dlopen\""
									 " -ex \"call \\$dlopen(\\\"%s\\\", 1)\""
									 " -ex \"detach\""
									 " -ex \"quit\""), (int)pid, path);

			free(exec(ST("sudo kill -STOP $(pidof steam)")));

			free(exec(strbuf));

			shm_unlink(temp);
			close(fd);

			procmaps_struct* maps = pmparser_parse(pid);
			procmaps_struct* firstMaps = nullptr;
			procmaps_struct* tempMaps = nullptr;
			procmaps_struct* lastMaps = nullptr;

			while((tempMaps = pmparser_next())) {
				if (!firstMaps && !strcmp(tempMaps->pathname, path))
					firstMaps = lastMaps = tempMaps;
				else if (firstMaps && strcmp(tempMaps->pathname, path))
					break;
				else if (firstMaps)
					lastMaps = tempMaps;
			}

			if (firstMaps) {
				unsigned long mapsStart = (unsigned long)firstMaps->addr_start;
				unsigned long mapsSize = (unsigned long)lastMaps->addr_end - mapsStart;

				snprintf(strbuf2, 2048, ST("sudo gdb -n -q -batch"
										   " -ex \"set logging file /dev/stdout\""
										   " -ex \"set logging redirect on\""
										   " -ex \"set logging on\""
										   " -ex \"set verbose off\""
										   " -ex \"attach %d\""
										   " -ex \"set scheduler-locking on\""
										   " -ex \"set \\$mmap = (void*(*)(void*, size_t, int, int, int, long)) mmap\""
										   " -ex \"set \\$munmap = (int(*)(void*, size_t)) munmap\""
										   " -ex \"set \\$memcpy = (void*(*)(void*, void*, size_t))__memmove_sse2_unaligned_erms\""
										   " -ex \"set \\$maddr = (void*)%lu\""
										   " -ex \"set \\$msize = (size_t)%lu\""
										   " -ex \"set \\$addr = \\$mmap(0, \\$msize, 7, 0x22, -1, 0)\""
										   " -ex \"call \\$memcpy(\\$addr, \\$maddr, \\$msize)\""
										   " -ex \"call \\$munmap(\\$maddr, \\$msize)\""
										   " -ex \"call \\$mmap(\\$maddr, \\$msize, 7, 0x32, -1, 0)\""
										   " -ex \"call \\$memcpy(\\$maddr, \\$addr, \\$msize)\""
										   " -ex \"call \\$munmap(\\$addr, \\$msize)\""
										   " -ex \"set *(void**)\\$maddr = (void*)0\""
										   " -ex \"set scheduler-locking off\""
										   " -ex \"detach\""
										   " -ex \"quit\""), (int)pid, mapsStart, mapsSize);

				free(exec(strbuf2));
			} else
				ret = 1;

			pmparser_free(maps);

			free(exec(ST("sudo kill -CONT $(pidof steam)")));
		} else {
			shm_unlink(temp);
			close(fd);
			ret = 1;
		}
	} else
		ret = 1;

		return ret;*/
	return 0;
}

void ServerReceiveModule(const char* dataIn, uint32_t dataSize)
{

}

uint64_t ServerAllocateModule(uint32_t allocSize)
{
	return 0;
}


#elif defined(__APPLE__)
int PerformLoad(const char* buf, size_t size)
{

}
#else

#include "windows_loader.h"
#include <tlhelp32.h>
#include <string.h>

struct WindowsSignatureStorage
{
	uint32_t version;
	std::vector<char*> signatures;
	bool original;

	template<typename T>
	void FillSignatures(const T& arg)
	{
		signatures.push_back(_strdup((const char*)arg));
	}

	template<typename T, typename... Args>
	void FillSignatures(const T& arg, const Args&... args)
	{
		signatures.push_back(_strdup((const char*)arg));
		FillSignatures(args...);
	}

	template<typename... Args>
	WindowsSignatureStorage(uint32_t ver, const Args&... args)
	{
		version = ver;
		FillSignatures(args...);
		original = true;
	}

	~WindowsSignatureStorage()
	{
		if (original)
			for (auto& i : signatures)
				free(i);
	}

	WindowsSignatureStorage(const WindowsSignatureStorage& o)
	{
		version = o.version;
		signatures = o.signatures;
		original = false;
	}
};

WindowsSignatureStorage tlsSignatures[] =
{
	{10, ST("75 0B [E8 *? ? ? ?] 8B")},
};

WindowsSignatureStorage freeTlsSignatures[] =
{
	{10, ST("[E8 *? ? ? ?] 80 7D C7 00 0F 84 9F D1 FA FF")},
};

extern void** loaderStart;
extern void** loaderEnd;

extern void** unloaderStart;
extern void** unloaderEnd;

ModuleList::ModuleList(int64_t pid)
{
	HANDLE moduleList = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

	if (moduleList == INVALID_HANDLE_VALUE)
		return;

	MODULEENTRY32 entry;
	entry.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(moduleList, &entry))
		while (Module32Next(moduleList, &entry)) {
			uint32_t nameOffset = names.size();
			int len = strlen(entry.szModule);
			names.resize(names.size() + len + 1);
		    memcpy(names.data() + nameOffset, entry.szModule, len + 1);
			modules.push_back({(uint64_t)entry.hModule, (uint64_t)entry.modBaseAddr, (uint64_t)entry.modBaseAddr + (uint64_t)entry.modBaseSize, nameOffset});
		}
}

static long long ProcFind(const char* name)
{
	HANDLE processList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (processList == INVALID_HANDLE_VALUE)
		return 0;

	if (WaitForSingleObject(processList, 0) == WAIT_TIMEOUT)
		return 0;

	PROCESSENTRY32 proc;
	proc.dwSize = sizeof(PROCESSENTRY32);

	while (Process32Next(processList, &proc)) {
		if (!strcmp(proc.szExeFile, name)) {
			CloseHandle(processList);
			return proc.th32ProcessID;
		}
	}

	CloseHandle(processList);

	return 0;
}

static std::vector<ProcessEntry> GetProcessList()
{
	std::vector<ProcessEntry> ret;

	HANDLE processList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (processList == INVALID_HANDLE_VALUE)
		return ret;

	if (WaitForSingleObject(processList, 0) == WAIT_TIMEOUT)
		return ret;

	PROCESSENTRY32 proc;
	proc.dwSize = sizeof(PROCESSENTRY32);

	while (Process32Next(processList, &proc))
		ret.push_back(ProcessEntry(proc.th32ProcessID, proc.szExeFile));

	CloseHandle(processList);

	return ret;
}

static bool EnableDebugPrivilege(HANDLE process)
{
	LUID luid;
	HANDLE token;
	TOKEN_PRIVILEGES newPrivileges;

	if (!OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES, &token))
		return false;

	if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
		return false;

	newPrivileges.PrivilegeCount = 1;
	newPrivileges.Privileges[0].Luid = luid;
	newPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	return AdjustTokenPrivileges(token, false, &newPrivileges, sizeof(newPrivileges), nullptr, nullptr);
}

LdrpHandleTlsDataSTDFn handleTlsDataSTD = nullptr;
LdrpHandleTlsDataThisFn handleTlsDataThis = nullptr;
HANDLE processHandle = nullptr;

uintptr_t localReleaseTlsInfoAddress = 0;
uintptr_t releaseTlsInfoOffset = 0;

char* mbuf = nullptr;
char* sbuf = nullptr;
bool localLoad = false;

void StartLoad(long pid)
{
	loadRet = 0;
	mbuf = nullptr;
	sbuf = nullptr;

	EnableDebugPrivilege(GetCurrentProcess());

	//Load to self
	if (pid == -1)
		processHandle = GetCurrentProcess();
	else
		processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

	localLoad = pid == -1;

	if (!processHandle) {
		loadRet = 1;
		return;
	}

	ModuleList moduleList(pid);

	uintptr_t handleTlsInfoAddress = 0;
	uintptr_t releaseTlsInfoAddress = 0;

	auto ntdllString = StackString("ntdll.dll");
	ModuleInfo ntdllInfo = Handles::GetModuleInfo(ntdllString);

	void (WINAPI* RtlGetVersionFunc)(PRTL_OSVERSIONINFOW) = nullptr;
	RtlGetVersionFunc = (decltype(RtlGetVersionFunc))GetProcAddress(ntdllInfo.handle, ST("RtlGetVersion"));

	RTL_OSVERSIONINFOW info;
	memset(&info, 0, sizeof(info));
	info.dwOSVersionInfoSize = sizeof(info);
	RtlGetVersionFunc(&info);

	for (auto& i : tlsSignatures) {
		if (i.version == info.dwMajorVersion) {
			for (auto& o : i.signatures) {
				handleTlsInfoAddress = PatternScan::FindPattern(o, ntdllString);
				if (handleTlsInfoAddress)
					break;
			}
			break;
		}
	}

	for (auto& i : freeTlsSignatures) {
		if (i.version == info.dwMajorVersion) {
			for (auto& o : i.signatures) {
				releaseTlsInfoAddress = PatternScan::FindPattern(o, ntdllString);
				if (handleTlsInfoAddress)
					break;
			}
			break;
		}
	}

	if (!handleTlsInfoAddress || !releaseTlsInfoAddress) {
	    loadRet = 2;
		return;
	}

	if (!localLoad) {
		handleTlsInfoAddress -= ntdllInfo.address;
		releaseTlsInfoAddress -= ntdllInfo.address;
		releaseTlsInfoOffset = releaseTlsInfoAddress;

		for (auto& i : moduleList.modules) {
			if (!STRCASECMP(ntdllString, moduleList.names.data() + i.nameOffset)) {
				handleTlsInfoAddress += i.startAddress;
				break;
			}
		}
	} else
		localReleaseTlsInfoAddress = releaseTlsInfoAddress;

	handleTlsDataSTD = nullptr;
	handleTlsDataThis = nullptr;

	if (info.dwMajorVersion > 8 || (info.dwMajorVersion == 8 && info.dwMinorVersion > 0))
		handleTlsDataThis = (LdrpHandleTlsDataThisFn)handleTlsInfoAddress;
	else
		handleTlsDataSTD = (LdrpHandleTlsDataSTDFn)handleTlsInfoAddress;

	sbuf = mbuf = nullptr;

	char* requestBuf = (char*)malloc(100 + moduleList.names.size() + moduleList.modules.size() * sizeof(RemoteModuleInfo));
	requestBuf[0] = '\0';
	strcat(requestBuf, ST("lml\n"));
	int curidx = strlen(requestBuf);
	*(uint32_t*)(requestBuf + curidx) = moduleList.names.size();
	curidx += sizeof(uint32_t);
	memcpy(requestBuf + curidx, moduleList.names.data(), moduleList.names.size());
	curidx += moduleList.names.size();
	*(uint32_t*)(requestBuf + curidx) = moduleList.modules.size() * sizeof(RemoteModuleInfo);
	curidx += sizeof(uint32_t);
	memcpy(requestBuf + curidx, moduleList.modules.data(), moduleList.modules.size() * sizeof(RemoteModuleInfo));
	curidx += moduleList.modules.size() * sizeof(RemoteModuleInfo);
	ServerComm::Send(std::string(requestBuf, curidx));
	free(requestBuf);
}

void ServerStartLoad(long pid)
{
	Threading::QueueJobRef(StartLoad, (void*)pid);
}

static int PerformLoad(void*)
{
	if (!sbuf)
		return 4;

	PackedWinModule packedLoader(sbuf);

	if (!packedLoader.state)
		loadRet = 5;
	else {

		loadedModules.push_back(ModuleEntry((uint64_t)processHandle, (uint64_t)mbuf, packedLoader.allocSize, ++lastModuleID, (ModuleExport*)(packedLoader.buffer + packedLoader.exportsOffset + sizeof(uint32_t)), *(uint32_t*)(packedLoader.buffer + packedLoader.exportsOffset)));

		if (localLoad) {
			WinLoadData lData = {(PackedWinModule*)&packedLoader, mbuf, (GetProcAddressFn)GetProcAddress, (LoadLibraryAFn)LoadLibraryA, handleTlsDataSTD, handleTlsDataThis};
			memcpy(mbuf, packedLoader.moduleBuffer, packedLoader.modBufSize);
		    LoadPackedModule((void*)&lData);
		} else {
			uintptr_t loaderDataSize = sizeof(PackedWinModule) + packedLoader.bufSize;
			uintptr_t loaderArgsSize = sizeof(WinLoadData);
			uintptr_t loaderSize = (uintptr_t)loaderEnd - (uintptr_t)loaderStart;
			uintptr_t fullLoaderSize = loaderSize + loaderArgsSize + loaderDataSize + 16;

			char* loaderBuf = (char*)VirtualAllocEx(processHandle, nullptr, fullLoaderSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

			if (!loaderBuf)
				loadRet = 6;
			else {
				uintptr_t loaderAddress = (uintptr_t)loaderBuf + loaderDataSize + loaderArgsSize;

				if (loaderAddress % 16)
					loaderAddress += 16 - loaderAddress % 16;

				packedLoader.SetupInPlace(processHandle, mbuf, loaderBuf);

				WinLoadData lData = {(PackedWinModule*)loaderBuf, mbuf, (GetProcAddressFn)GetProcAddress, (LoadLibraryAFn)LoadLibraryA, handleTlsDataSTD, handleTlsDataThis};

				WriteProcessMemory(processHandle, loaderBuf + loaderDataSize, &lData, sizeof(WinLoadData), nullptr);

				WriteProcessMemory(processHandle, (void*)loaderAddress, loaderStart, loaderSize, nullptr);

				HANDLE thread = CreateRemoteThread(processHandle, nullptr, 0, (unsigned long(__stdcall*)(void*))loaderAddress, loaderBuf + loaderDataSize, 0, nullptr);

				WaitForSingleObject(thread, INFINITE);

				VirtualFreeEx(processHandle, loaderBuf, 0, MEM_RELEASE);
			}
		}

		//We send this after load so that the server can time the load time for various purposes
		char buf[32];
		snprintf(buf, 31, "%d", lastModuleID);
		ServerComm::Send(std::string(buf));
	}

	free(sbuf);

	loaderSem.Post();

	return loadRet;
}

void UnloadModule(long libID)
{
	//Find the module in question

	ModuleEntry* entryPtr = nullptr;

	for (ModuleEntry& i : loadedModules) {
		if (i.moduleID == libID) {
			entryPtr = &i;
			break;
		}
	}

	//Failure to unload. TODO: Inform the server about this?
	if (!entryPtr)
		return;

	ModuleEntry entry = *entryPtr;

	loadedModules.erase(loadedModules.begin() + (entryPtr - loadedModules.data()));

	WinUnloadData unloadData = {(void*)(entry.baseAddress), (void*)(entry.baseAddress + entry.FindExport(CCRC32("__DllMain"))->baseOffset), handleTlsDataSTD, handleTlsDataThis, (LdrpReleaseTlsEntryThisFn)localReleaseTlsInfoAddress};

	HANDLE pHandle = (HANDLE)entry.handle;

	if (pHandle == GetCurrentProcess())
		UnloadGameModule((void*)&unloadData);
	else {

		uintptr_t unloaderOffset = sizeof(WinUnloadData);
		uintptr_t unloaderSize = (uintptr_t)unloaderEnd - (uintptr_t)unloaderStart;

		if (unloaderOffset % 16)
		    unloaderOffset += 16 - unloaderOffset % 16;

		char* unloaderBuf = (char*)VirtualAllocEx(pHandle, nullptr, unloaderOffset + unloaderSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		WriteProcessMemory(pHandle, unloaderBuf, &unloadData, sizeof(WinUnloadData), nullptr);
		WriteProcessMemory(pHandle, unloaderBuf + unloaderOffset, unloaderStart, unloaderSize, nullptr);

		HANDLE thread = CreateRemoteThread(pHandle, nullptr, 0, (unsigned long(__stdcall*)(void*))(unloaderBuf + unloaderOffset), unloaderBuf, 0, nullptr);

		WaitForSingleObject(thread, INFINITE);

		VirtualFreeEx(pHandle, unloaderBuf, 0, MEM_RELEASE);
	}

	VirtualFreeEx(pHandle, (void*)entry.baseAddress, 0, MEM_RELEASE);

	CloseHandle(pHandle);

	char buf[64];
	snprintf(buf, 63, "%s %d", (char*)ST("um"), (int)libID);
	ServerComm::Send(std::string(buf));
}

void ServerReceiveModule(const char* dataIn, uint32_t size)
{
	sbuf = (char*)malloc(size);
	memcpy(sbuf, dataIn, size);
	Threading::QueueJobRef(PerformLoad, (void*)0);
}

uint64_t ServerAllocateModule(uint32_t allocSize)
{
	uint64_t ret = (uint64_t)VirtualAllocEx(processHandle, nullptr, allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	mbuf = (char*)ret;

	if (!ret) {
		loadRet = 3;
		loaderSem.Post();
	}

	return ret;
}

void ServerUnloadModule(int libID)
{
	Threading::QueueJobRef(UnloadModule, (void*)(long)libID);
}

#endif
