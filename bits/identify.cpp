#include "identify.h"
#include <stdint.h>
#include "../sdk/framework/utils/stackstring.h"

const char* moduleName = MODULE_IDENTIFICATION;

extern char* GetModuleName(void* dependency1, void* dependency2)
{
	uintptr_t addr1 = (uintptr_t)dependency1;
	uintptr_t addr2 = ~(uintptr_t)dependency2;
	volatile uintptr_t addr3 = addr1 & addr2;

	auto st = new StackString(MODULE_IDENTIFICATION);

	return (char*)st + (addr3 & addr2);
}
