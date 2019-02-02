#include "windows_loader.h"
#include "windows_headers.h"
#include "../sdk/framework/utils/crc32.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using DllEntryPointFn = int(__stdcall*)(void*, unsigned int, void*);

GetProcAddressFn pGetProcAddress = nullptr;
LoadLibraryAFn pLoadLibraryA = nullptr;

uint32_t WinModule::VirtToFile(uint32_t virtOffset, WinSection*& hint)
{
	if (!hint || hint->virtOffset > virtOffset || hint->virtOffset + hint->virtSize <= virtOffset)
		for (auto& o : sections)
			if (o.virtOffset <= virtOffset && o.virtOffset + o.virtSize > virtOffset) {
			    hint = &o;
				break;
			}


	if (hint)
		return hint->bufOffset + virtOffset - hint->virtOffset;
	else {
		printf("VirtToFile failure!\n");
		return ~0u;
	}
}

uint32_t WinModule::FileToVirt(uint32_t bufOffset, WinSection*& hint)
{
	if (!hint || hint->bufOffset > bufOffset || hint->bufOffset + hint->bufSize <= bufOffset)
		for (auto& o : sections)
			if (o.bufOffset <= bufOffset && o.bufOffset + o.bufSize > bufOffset) {
			    hint = &o;
				break;
			}


	if (hint)
		return hint->virtOffset + bufOffset - hint->bufOffset;
	else {
		printf("FileToVirt failure!\n");
		return ~0u;
	}
}

WinModule::WinModule(const char* buf, size_t size, ModuleList* moduleList, bool is64)
{
	//Calloc so as to not leak heap to end-users
	moduleBuffer = (char*)calloc(size, 1);

	PIMAGE_DOS_HEADER dHeader = (PIMAGE_DOS_HEADER)buf;
	//PIMAGE_DOS_HEADER moduleHeader = (PIMAGE_DOS_HEADER)moduleBuffer;

	if (dHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return;

	if (is64) {
	} else {
		PIMAGE_NT_HEADERS32 ntHeader = (PIMAGE_NT_HEADERS32)(buf + dHeader->e_lfanew);

		if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
			return;

		//TODO: Falsify header information
		sections.push_back({0, 0, ntHeader->OptionalHeader.SizeOfHeaders, ntHeader->OptionalHeader.SizeOfHeaders});
		memcpy(moduleBuffer, buf, ntHeader->OptionalHeader.SizeOfHeaders);

		PIMAGE_NT_HEADERS32 moduleNTHeader = (PIMAGE_NT_HEADERS32)(moduleBuffer + dHeader->e_lfanew);
		PIMAGE_SECTION_HEADER imageSections = (PIMAGE_SECTION_HEADER)(ntHeader + 1);

		for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
			memcpy(moduleBuffer + imageSections[i].PointerToRawData, buf + imageSections[i].PointerToRawData, imageSections[i].SizeOfRawData);
		    sections.push_back({imageSections[i].PointerToRawData, imageSections[i].VirtualAddress, imageSections[i].SizeOfRawData, imageSections[i].SizeOfRawData, ~0u});
		}

		WinSection* cachedSection = nullptr;

		//Remove the image base from relocated entries so that relocation code does not have to worry about this detail
		PIMAGE_BASE_RELOCATION moduleRelocations = (PIMAGE_BASE_RELOCATION)(moduleBuffer + VirtToFile(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress, cachedSection));

		while (moduleRelocations->VirtualAddress) {
			if (moduleRelocations->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION)) {
				uint32_t count = (moduleRelocations->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
				uint16_t* list = (uint16_t*)(moduleRelocations + 1);

				for (uint32_t i = 0; i < count; i++) {
					if (list[i]) {
						size_t pOff = moduleRelocations->VirtualAddress + (list[i] & 0xfff);
						uint32_t fOff = VirtToFile(pOff, cachedSection);
						*(uint32_t*)(moduleBuffer + fOff) -= ntHeader->OptionalHeader.ImageBase;
						relocations.push_back({fOff});
						list[i] = 0;
					}
				}
			}

			moduleRelocations = (PIMAGE_BASE_RELOCATION)((char*)moduleRelocations + moduleRelocations->SizeOfBlock);
		}

		//TODO: Build a fake relocation table in such subtle fashion that many tools would screw up and manual analysis would be rather difficult and confusing

		entryPointOffset = ntHeader->OptionalHeader.AddressOfEntryPoint;
		moduleNTHeader->OptionalHeader.AddressOfEntryPoint = 0;

		moduleSize = moduleNTHeader->OptionalHeader.SizeOfImage;
		moduleNTHeader->OptionalHeader.SizeOfImage *= 10;
		moduleNTHeader->OptionalHeader.SizeOfImage -= 1234;
		moduleNTHeader->OptionalHeader.SizeOfImage &= ~0xffff;


		PIMAGE_EXPORT_DIRECTORY exportDirectory = (PIMAGE_EXPORT_DIRECTORY)(moduleBuffer + VirtToFile(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, cachedSection));

		uint32_t* exnames = (uint32_t*)(moduleBuffer + VirtToFile(exportDirectory->AddressOfNames, cachedSection));
		uint16_t* ordinals = (uint16_t*)(moduleBuffer + VirtToFile(exportDirectory->AddressOfNameOrdinals, cachedSection));
		uint32_t* functions = (uint32_t*)(moduleBuffer + VirtToFile(exportDirectory->AddressOfFunctions, cachedSection));

		//Also push entry point as __DllMain for easy access
		exports.push_back(ModuleExport(CCRC32("__DllMain"), entryPointOffset));

		for (size_t i = 0; i < exportDirectory->NumberOfFunctions; i++) {
			uint32_t crc = Crc32(moduleBuffer + VirtToFile(exnames[i], cachedSection), strlen(moduleBuffer + VirtToFile(exnames[i], cachedSection)));
			exports.push_back(ModuleExport(crc, functions[ordinals[i]]));
			functions[ordinals[i]] += ((rand() % 20000) - 10000) & ~0xff;
		}

		PIMAGE_IMPORT_DESCRIPTOR importDirectory = (PIMAGE_IMPORT_DESCRIPTOR)(moduleBuffer + VirtToFile(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, cachedSection));

		while (importDirectory->Characteristics) {
			PIMAGE_THUNK_DATA32 origFirstThunk = (PIMAGE_THUNK_DATA32)(moduleBuffer + VirtToFile(importDirectory->OriginalFirstThunk, cachedSection));
			PIMAGE_THUNK_DATA32 firstThunk = (PIMAGE_THUNK_DATA32)(moduleBuffer + VirtToFile(importDirectory->FirstThunk, cachedSection));
			//TODO: Find the right module inside the to be provided loaded module list
			void* module = nullptr;
			char* name = moduleBuffer + VirtToFile(importDirectory->Name, cachedSection);
			int len = strlen(name);

			if (moduleList) {
				for (auto& i : moduleList->modules) {
					if (!STRCASECMP(moduleList->names.data() + i.nameOffset, name)) {
						module = (void*)i.handle;
						break;
					}
				}
			}

			bool moduleLoaded = !!module;
			WinImportThunk* thunk = nullptr;

			if (!moduleLoaded) {
				importThunk.resize(importThunk.size() + 1);
				thunk = &importThunk[importThunk.size() - 1];
				thunk->importOffset = thunkedImports.size();

				thunk->moduleNameOffset = names.size();
				names.resize(names.size() + len + 1);
				memcpy(names.data() + thunk->moduleNameOffset, name, len + 1);
			}

			memset(name, 0, len);

			while (origFirstThunk->u1.AddressOfData) {
				uint32_t nameOffset = 0;

				if (origFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
					nameOffset = ~(origFirstThunk->u1.Ordinal & 0xffff);
					origFirstThunk->u1.Ordinal &= ~IMAGE_ORDINAL_FLAG32;
				} else {
					PIMAGE_IMPORT_BY_NAME imp = (PIMAGE_IMPORT_BY_NAME)(moduleBuffer + VirtToFile(origFirstThunk->u1.AddressOfData, cachedSection));
					int slen = strlen((char*)imp->Name);
					nameOffset = names.size();
					names.resize(names.size() + slen + 1);
					memcpy(names.data() + nameOffset, imp->Name, slen + 1);
					origFirstThunk->u1.Ordinal += 34;
					origFirstThunk->u1.Ordinal &= 0xffff;
					origFirstThunk->u1.Ordinal |= IMAGE_ORDINAL_FLAG32;
				}

				if (!moduleLoaded)
					thunkedImports.push_back({nameOffset, FileToVirt((uint32_t)((char*)&firstThunk->u1.Function - moduleBuffer), cachedSection)});
				else
					importsWH.push_back({(uint64_t)module, {nameOffset, FileToVirt((uint32_t)((char*)&firstThunk->u1.Function - moduleBuffer), cachedSection)}});

				origFirstThunk++;
				firstThunk++;
			}

			if (!moduleLoaded)
				thunk->importCount = thunkedImports.size() - thunk->importOffset;

			importDirectory++;
		}
	}
}


PackedWinModule::PackedWinModule(const WinModule& mod)
{
	xorKey = 0;
	bufSize = 0;

	for (auto& i : mod.sections)
		bufSize += i.bufSize;

	char* moduleBuffer = (char*)malloc(bufSize);

	uint32_t cbo = 0;
	for (auto& i : mod.sections) {
		memcpy(moduleBuffer + cbo, mod.moduleBuffer + i.bufOffset, i.bufSize);
		cbo += i.bufSize;
	}

    modBufSize = bufSize;
	bufSize = 0;

	entryPointOffset = mod.entryPointOffset;
	allocSize = mod.moduleSize;

	sectionOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinSection) * mod.sections.size();
	nameOffset = bufSize;
	bufSize += mod.names.size();
	importsWHOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinImportH) * mod.importsWH.size();
	importsOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinImport) * mod.thunkedImports.size();
	importThunksOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinImportThunk) * mod.importThunk.size();
	exportsOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(ModuleExport) * mod.exports.size();
	relocationOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinRelocation) * mod.relocations.size();

	char* buffer = (char*)malloc(bufSize);

	*(uint32_t*)(buffer + sectionOffset) = mod.sections.size();
	memcpy(buffer + sectionOffset + sizeof(uint32_t), (char*)mod.sections.data(), sizeof(WinSection) * mod.sections.size());

	memcpy(buffer + nameOffset, mod.names.data(), mod.names.size());

	*(uint32_t*)(buffer + importsWHOffset) = mod.importsWH.size();
	memcpy(buffer + importsWHOffset + sizeof(uint32_t), mod.importsWH.data(), mod.importsWH.size() * sizeof(WinImportH));

	*(uint32_t*)(buffer + importsOffset) = mod.thunkedImports.size();
	memcpy(buffer + importsOffset + sizeof(uint32_t), mod.thunkedImports.data(), mod.thunkedImports.size() * sizeof(WinImport));

	*(uint32_t*)(buffer + importThunksOffset) = mod.importThunk.size();
	memcpy(buffer + importThunksOffset + sizeof(uint32_t), mod.importThunk.data(), mod.importThunk.size() * sizeof(WinImportThunk));

	*(uint32_t*)(buffer + exportsOffset) = mod.exports.size();
	memcpy(buffer + exportsOffset + sizeof(uint32_t), mod.exports.data(), mod.exports.size() * sizeof(ModuleExport));

	*(uint32_t*)(buffer + relocationOffset) = mod.relocations.size();
	memcpy(buffer + relocationOffset + sizeof(uint32_t), mod.relocations.data(), mod.relocations.size() * sizeof(WinRelocation));

	this->moduleBuffer = moduleBuffer;
	this->buffer = buffer;

	state = 1;
}

PackedWinModule::PackedWinModule(const char* buf)
{
	uint32_t classSize = *(uint32_t*)buf;
	const char* classBuf = buf + sizeof(uint32_t);
	buf += classSize + sizeof(uint32_t);
	uint32_t moduleSize = *(uint32_t*)buf;
	const char* moduleBuf = buf + sizeof(uint32_t);
	buf += moduleSize + sizeof(uint32_t);
	uint32_t dataSize = *(uint32_t*)buf;
	const char* dataBuf = buf + sizeof(uint32_t);

	state = 0;

	buffer = nullptr;
	moduleBuffer = nullptr;

	if (classSize != sizeof(PackedWinModule) - serSizeSub)
		return;

	memcpy(this, classBuf, classSize);

	state = 0;

	if (bufSize != dataSize || modBufSize != moduleSize)
		return;

	state = 2;
	buffer = dataBuf;
	moduleBuffer = moduleBuf;
}

char* PackedWinModule::ToBuffer(uint32_t* outSize)
{
	size_t allocSize = sizeof(PackedWinModule) - serSizeSub + modBufSize + bufSize + sizeof(uint32_t) * 3;
	char* fullCBuf = (char*)malloc(allocSize);
	char* cTempBuf = fullCBuf;

	*(uint32_t*)cTempBuf = sizeof(PackedWinModule) - serSizeSub;
	cTempBuf += sizeof(uint32_t);
	memcpy(cTempBuf, this, sizeof(PackedWinModule) - serSizeSub);
	cTempBuf += sizeof(PackedWinModule) - serSizeSub;
	*(uint32_t*)cTempBuf = modBufSize;
	cTempBuf += sizeof(uint32_t);
	memcpy(cTempBuf, moduleBuffer, modBufSize);
	cTempBuf += modBufSize;
	*(uint32_t*)cTempBuf = bufSize;
	cTempBuf += sizeof(uint32_t);
	memcpy(cTempBuf, buffer, bufSize);

	if (outSize)
		*outSize = allocSize;

	return fullCBuf;
}

void PackedWinModule::PerformRelocations(nptr_t base)
{
	uint32_t* relocationCount = (uint32_t*)(buffer + relocationOffset);
	WinRelocation* relocations = (WinRelocation*)(relocationCount + 1);

	for (uint32_t i = 0; i < *relocationCount; i++)
		*(uint32_t*)(moduleBuffer + relocations[i].bufOffset) += base;

	//Wipe relocation data so that we do not apply relocations twice (or leak the data)
	memset(relocations, 0, sizeof(WinRelocation) * *relocationCount);
	*relocationCount = 0;
	relocationOffset = 0;
}

void PackedWinModule::RunCrypt()
{
	srand(time(nullptr));

	if (!xorKey)
		for (size_t i = 0; i < sizeof(xorKey); i++)
			((char*)&xorKey)[i] = (rand() % 256);

	for (size_t o = 0; o < modBufSize / sizeof(uint32_t); o++)
		((uint32_t*)moduleBuffer)[o] ^= xorKey;
}


#ifdef _WIN32
void PackedWinModule::SetupInPlace(HANDLE processHandle, char* targetModuleAddress, char* targetDataAddress)
{
	PackedWinModule wModule = *this;
    wModule.buffer = targetDataAddress + sizeof(PackedWinModule);
	wModule.moduleBuffer = nullptr;
	WriteProcessMemory(processHandle, targetDataAddress, &wModule, sizeof(PackedWinModule), nullptr);
	WriteProcessMemory(processHandle, (void*)wModule.buffer, (void*)buffer, bufSize, nullptr);
	wModule.buffer = nullptr;

	//Copy buffer into module
	WriteProcessMemory(processHandle, targetModuleAddress, moduleBuffer, wModule.modBufSize, nullptr);
}


#if defined(_MSC_VER)
#pragma comment(linker, "/merge:_text_code=_text")
#pragma comment(linker, "/merge:_text_end=_text")

__declspec(allocate("_text_end")) void* _text_end;

void** loaderStart = (void**)&LoadPackedModule;
void** loaderEnd = &_text_end;
#else
extern void* __start__text;
extern void* __stop__text;
void** loaderStart = &__start__text;
void** loaderEnd = &__stop__text;
#endif

#if defined(_MSC_VER)
__declspec(code_seg("_text_code"))
#else
[[gnu::section("_text")]]
#endif
unsigned long __stdcall LoadPackedModule(void* loadData)
{
	WinLoadData* wLoadData = (WinLoadData*)loadData;

	PackedWinModule* packedModule = wLoadData->packedModule;
	char* outBuf = wLoadData->outBuf;
	GetProcAddressFn pGetProcAddress = wLoadData->pGetProcAddress;
	LoadLibraryAFn pLoadLibraryA = wLoadData->pLoadLibraryA;

	const char* buffer = packedModule->buffer;
	const char* names = buffer + packedModule->nameOffset;

	uint32_t* sectionCount = (uint32_t*)(buffer + packedModule->sectionOffset);
	WinSection* sections = (WinSection*)(sectionCount + 1);

	//TODO: We should actually copy a fake library using WriteProcessMemory section by section and then run this code from the second allocated buffer

	// Decrypt and copy the sections backwards to not overwrite the buffer
	for (size_t o = 0; o < packedModule->modBufSize / sizeof(uint32_t); o++)
		((uint32_t*)outBuf)[o] ^= packedModule->xorKey;

	for (uint32_t i = *sectionCount - 1; i < (~0u) - 2u; i--)
		for (size_t o = sections[i].bufSize / sizeof(uint32_t) - 1; o < (~0u) - 2u; o--)
			((uint32_t*)(outBuf + sections[i].virtOffset))[o] = ((uint32_t*)(outBuf + sections[i].bufOffset))[o];

	uint32_t* importHCount = (uint32_t*)(buffer + packedModule->importsWHOffset);
	WinImportH* importsH = (WinImportH*)(importHCount + 1);

	for (uint32_t i = 0; i < *importHCount; i++) {
		if (importsH[i].imp.nameOffset < ~importsH[i].imp.nameOffset)
			*(nptr_t*)(outBuf + importsH[i].imp.virtOffset) = pGetProcAddress((void*)importsH[i].module, names + importsH[i].imp.nameOffset);
		else
			*(nptr_t*)(outBuf + importsH[i].imp.virtOffset) = pGetProcAddress((void*)importsH[i].module, (char*)(uintptr_t)~importsH[i].imp.nameOffset);
	}

	uint32_t* thunkedImportCount = (uint32_t*)(buffer + packedModule->importsOffset);
	WinImport* imports = (WinImport*)(thunkedImportCount + 1);

	uint32_t* importThunkCount = (uint32_t*)(buffer + packedModule->importThunksOffset);
    WinImportThunk* importThunks = (WinImportThunk*)(importThunkCount + 1);

	for (uint32_t i = 0; i < *importThunkCount; i++) {
		const char* mod = names + importThunks[i].moduleNameOffset;
		void* module = pLoadLibraryA(mod);

		for (uint32_t o = importThunks[i].importOffset; o < importThunks[i].importOffset + importThunks[i].importCount; o++) {
			if (imports[o].nameOffset < ~imports[o].nameOffset)
				*(nptr_t*)(outBuf + imports[o].virtOffset) = pGetProcAddress(module, names + imports[o].nameOffset);
			else
				*(nptr_t*)(outBuf + imports[o].virtOffset) = pGetProcAddress(module, (char*)(uintptr_t)~imports[o].nameOffset);
		}
	}

	//TODO: Handle TLS data manually
	LDR_DATA_TABLE_ENTRY dummyDataTable;

	dummyDataTable.DllBase = outBuf;

	if (wLoadData->pHandleTlsDataSTD)
		wLoadData->pHandleTlsDataSTD(&dummyDataTable);
	else
		wLoadData->pHandleTlsDataThis(&dummyDataTable);

	//DLL_PROCESS_ATTACH = 1
	return ((DllEntryPointFn)(outBuf + packedModule->entryPointOffset))(outBuf, 1, nullptr);
}

#if defined(_MSC_VER)
#pragma comment(linker, "/merge:__text_code=_text")
#pragma comment(linker, "/merge:__text_end=_text")

__declspec(allocate("__text_end")) void* __text_end;

void** unloaderStart = (void**)&UnloadGameModule;
void** unloaderEnd = &__text_end;
#else
extern void* __start___text;
extern void* __stop___text;
void** unloaderStart = &__start___text;
void** unloaderEnd = &__stop___text;
#endif

#if defined(_MSC_VER)
__declspec(code_seg("__text_code"))
#else
[[gnu::section("__text")]]
#endif
unsigned long __stdcall UnloadGameModule(void* unloadData)
{
	WinUnloadData* data = (WinUnloadData*)unloadData;

	//DLL_PROCESS_DETACH = 0
	return ((DllEntryPointFn)data->entryPoint)((void*)data->baseAddress, 0, nullptr);
}

#endif
