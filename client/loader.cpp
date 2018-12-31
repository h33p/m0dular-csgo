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

int PerformLoad(const char* buf, size_t size)
{
	//Server side
	WinModule loader(buf, size);
	PackedWinModule packedLoader(loader);

	//Client side
    char* mbuf = (char*)VirtualAlloc(nullptr, packedLoader.allocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (!mbuf)
	    return 1;

	//Server side
	packedLoader.PerformRelocations((nptr_t)mbuf);

	//Client side
	WinLoadData lData = {&packedLoader, mbuf, (GetProcAddressFn)GetProcAddress, (LoadLibraryAFn)LoadLibraryA};
	LoadModule((void*)&lData);

	return 0;
}
#endif
