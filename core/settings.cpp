#include "settings.h"
#include "../sdk/framework/features/aimbot_types.h"
#include "../sdk/framework/utils/stackstring.h"
#include "binds.h"
#include "shmfs.h"

//We will use atomic flags and atomic ints to implement a interprocess rwlock
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif


static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

constexpr unsigned int ALLOC_SIZE = 1 << 24;
constexpr unsigned int LOCAL_ALLOC_SIZE = 1 << 25;

std::atomic_int* ipcCounter = nullptr;

uintptr_t Settings::allocBase = 0;
generic_free_list_allocator<Settings::allocBase>* Settings::settingsAlloc = nullptr;

uintptr_t Settings::localAllocBase = 0;
generic_free_list_allocator<Settings::localAllocBase>* Settings::settingsLocalAlloc = nullptr;

decltype(Settings::globalSettingsPtr) Settings::globalSettingsPtr = nullptr;
decltype(Settings::globalSettings) Settings::globalSettings;
decltype(Settings::bindSettingsPtr) Settings::bindSettingsPtr = nullptr;
decltype(Settings::bindSettings) Settings::bindSettings;

AimbotHitbox Settings::aimbotHitboxes[MAX_HITBOXES] = {
	{ HITBOX_HEAD, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_NECK, SCAN_SIMPLE },
	{ HITBOX_PELVIS, SCAN_SIMPLE },
	{ HITBOX_STOMACH, SCAN_SIMPLE },
	{ HITBOX_LOWER_CHEST, SCAN_SIMPLE },
	{ HITBOX_CHEST, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_RIGHT_THIGH, SCAN_SIMPLE },
	{ HITBOX_LEFT_THIGH, SCAN_SIMPLE },
	{ HITBOX_RIGHT_CALF, SCAN_SIMPLE },
	{ HITBOX_LEFT_CALF, SCAN_SIMPLE },
	{ HITBOX_RIGHT_FOOT, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_LEFT_FOOT, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_RIGHT_HAND, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_LEFT_HAND, SCAN_MULTIPOINT, 0.8f },
	{ HITBOX_RIGHT_UPPER_ARM, SCAN_SIMPLE },
	{ HITBOX_LEFT_UPPER_ARM, SCAN_SIMPLE },
};

struct IPCInit
{
	uintptr_t* target;
	size_t size;

	template<typename T>
	IPCInit(T*& in) : target((uintptr_t*)&in), size(sizeof(T)) {}
};

bool MapSharedMemory(fileHandle& fd, void*& addr, size_t msz, const char* name)
{
	bool firstTime = false;

#ifdef _WIN32
	fd = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);
#if defined(M0DULAR_CLIENT) || defined(DEBUG)
	if (!(void*)fd) {
		fd = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, msz, name);
		firstTime = true;
	}
#endif

	if (fd) {
		addr = (void*)MapViewOfFile(fd, FILE_MAP_ALL_ACCESS, 0, 0, msz);
	}

#else
	fd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		firstTime = true;
		fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	}
	//firstTime = true;

	if (fd != -1 && ftruncate(fd, msz) != -2)
		addr = mmap(nullptr, msz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if (addr == (void*)-1)
		addr = nullptr;
#endif

	return firstTime;
}

void UnmapSharedMemory(void* addr, fileHandle& fd, const char* name, size_t msz, bool unlink)
{
#ifdef _WIN32
	UnmapViewOfFile(addr);
	CloseHandle(fd);
#else
	munmap(addr, msz);
	close(fd);
	if (unlink)
		shm_unlink(name);
#endif
}

template<typename T>
static void DestructClass(T*& target)
{
	target->~T();
}

template<typename T, typename... Args>
static void ConstructClass(T*& target, Args... args)
{
#ifdef DEBUG
	printf("Constructing %p\n", target);
	fflush(stdout);
#endif
	target = new(target) T(args...);
}

SettingsInstance::SettingsInstance() {
	Initialize();
}

void SettingsInstance::Initialize()
{

	if (alloc != nullptr)
		return;

	bool firstTime = MapSharedMemory(fd, alloc, ALLOC_SIZE, ST("m0d_settings"));

	Settings::allocBase = (uintptr_t)alloc;

	if (!Settings::allocBase)
		return;

	Settings::localAllocBase = (uintptr_t)malloc(LOCAL_ALLOC_SIZE);

	Settings::settingsLocalAlloc = new std::decay<decltype(*Settings::settingsLocalAlloc)>::type(LOCAL_ALLOC_SIZE, PlacementPolicy::FIND_FIRST, (void**)Settings::localAllocBase);

	uintptr_t finalAddress = Settings::allocBase;
#ifdef DEBUG
	printf("Initializing pointers... BASE %p\n", alloc);
#endif

	IPCInit initializedPointers[] = {
		IPCInit(ipcCounter),
		IPCInit(Settings::settingsAlloc),
		IPCInit(Settings::globalSettingsPtr),
		IPCInit(Settings::bindSettingsPtr),
		IPCInit(BindManager::sharedInstance),
		IPCInit(SHMFS::sharedInstance),
	};

	for (IPCInit& i : initializedPointers) {
		*i.target = finalAddress;
#ifdef DEBUG
		printf("%lx\n", finalAddress - (uintptr_t)alloc);
#endif
		finalAddress += i.size;
	}

	initialized = false;

#ifdef DEBUG
	printf("Constructing classes...\n");
#endif
	if (firstTime) {
#if defined(M0DULAR_CLIENT) || defined(DEBUG)
		ConstructClass(ipcCounter);
		ConstructClass(Settings::settingsAlloc, ALLOC_SIZE - (finalAddress - Settings::allocBase), PlacementPolicy::FIND_FIRST, (void*)finalAddress);
		ConstructClass(Settings::globalSettingsPtr);
		ConstructClass(Settings::bindSettingsPtr);
		ConstructClass(BindManager::sharedInstance);
		ConstructClass(SHMFS::sharedInstance);
		initialized = true;
#endif
	} else
		initialized = true;

	BindManager::sharedInstance->InitializeLocalData();

	(*ipcCounter)++;
}

SettingsInstance::~SettingsInstance()
{
	(*ipcCounter)--;

	bool unlink = false;

	if (!ipcCounter->load()) {
		unlink = true;
		DestructClass(ipcCounter);
		DestructClass(Settings::settingsAlloc);
		DestructClass(Settings::globalSettingsPtr);
		DestructClass(Settings::bindSettingsPtr);
		DestructClass(BindManager::sharedInstance);
	}

	UnmapSharedMemory(alloc, fd, "m0d_settings", ALLOC_SIZE, unlink);
}

//TODO: Ensure this class gets constructed before anything else that depends on it. Currently this is done by changing file ordering, but does not work on GCC
SettingsInstance Settings::sharedInstance;

namespace Settings
{
#define HANDLE_OPTION(type, defaultVal, minVal, maxVal, name, uiName, description, ...) OPTIONDEF(name)(defaultVal);
#include "../bits/option_list.h"
}

//TODO FIXME!!! We seriously have to put this somewhere proper
extern "C" char* strdup(const char* str)
{
	int len = strlen(str);
	char* ret = (char*)malloc(len + 1);
	memcpy(ret, str, len + 1);
	return ret;
}
