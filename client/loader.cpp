#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "loader.h"
#include "main.h"

int PerformLoad(const char* buf, size_t size);

void Load()
{
	srand(time(nullptr));
#ifdef _WIN32
	FILE* fp = fopen("libm0dular.dll", "rb");
#else
	FILE* fp = fopen("./build/libm0dular.so", "rb");
#endif
	fseek(fp, 0L, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	char* buf = (char*)malloc(size);

	fread(buf, size, 1, fp);

	int ret = PerformLoad(buf, size);
	free(buf);

	if (ret)
		STPRINT("Failed to load!\n");
	else
		STPRINT("Loaded successfully!\n");
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

pid_t ProcFind()
{
	unsigned int pid = 0;

	char* ret = exec("pidof -x "
		 GAME_NAME);

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
int PerformLoad(const char* buf, size_t size)
{
	char temp[30];
	int ret = 0;

	temp[0] = '\0';

	for (int i = 0; i < 10; i++)
		temp[i] = 'a' + rand() % ('z' - 'a');

	temp[10] = '\0';

	int fd = shm_open(temp, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

	pid_t pid = ProcFind();

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

	return ret;
}

#elif defined(__APPLE__)
int PerformLoad(const char* buf, size_t size)
{

}
#else

#include "windows_loader.h"
#include <tlhelp32.h>
#include <string.h>

extern void** loaderStart;
extern void** loaderEnd;

static uint32_t ProcFind(const char* name)
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

	return 0;
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

int PerformLoad(const char* buf, size_t size)
{
	EnableDebugPrivilege(GetCurrentProcess());

	int pid = ProcFind("csgo.exe");
	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

	//Server side
	WinModule loader(buf, size);
	PackedWinModule packedLoader(loader);

	//Client side
    char* mbuf = (char*)VirtualAllocEx(processHandle, nullptr, packedLoader.allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (!mbuf)
	    return 1;

	//Server side
	packedLoader.PerformRelocations((nptr_t)mbuf);

	//Client side
	uintptr_t loaderDataSize = sizeof(PackedWinModule) + packedLoader.bufSize;
	uintptr_t loaderArgsSize = sizeof(WinLoadData);
	uintptr_t loaderSize = (uintptr_t)loaderEnd - (uintptr_t)loaderStart;
	uintptr_t fullLoaderSize = loaderSize + loaderArgsSize + loaderDataSize + 16;

	char* loaderBuf = (char*)VirtualAllocEx(processHandle, nullptr, fullLoaderSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	uintptr_t loaderAddress = (uintptr_t)loaderBuf + loaderDataSize + loaderArgsSize;

	if (loaderAddress % 16)
		loaderAddress += 16 - loaderAddress % 16;

	packedLoader.SetupInPlace(processHandle, mbuf, loaderBuf);

	WinLoadData lData = {(PackedWinModule*)loaderBuf, mbuf, (GetProcAddressFn)GetProcAddress, (LoadLibraryAFn)LoadLibraryA};


	WriteProcessMemory(processHandle, loaderBuf + loaderDataSize, &lData, sizeof(WinLoadData), nullptr);

    WriteProcessMemory(processHandle, (void*)loaderAddress, loaderStart, loaderSize, nullptr);

	HANDLE thread = CreateRemoteThread(processHandle, nullptr, 0, (unsigned long(__stdcall*)(void*))loaderAddress, loaderBuf + loaderDataSize, 0, nullptr);

	WaitForSingleObject(thread, INFINITE);

	VirtualFreeEx(processHandle, loaderBuf, fullLoaderSize, MEM_RELEASE);

	return 0;
}
#endif
