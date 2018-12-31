#include "windows_loader.h"
#include "windows_headers.h"
#include <string.h>
#include <stdio.h>

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

WinModule::WinModule(const char* buf, size_t size, bool is64)
{
	//Calloc so as to not leak heap to end-users
	moduleBuffer = (char*)calloc(size, 1);

	PIMAGE_DOS_HEADER dHeader = (PIMAGE_DOS_HEADER)buf;
	//PIMAGE_DOS_HEADER moduleHeader = (PIMAGE_DOS_HEADER)moduleBuffer;

	if (is64) {
	} else {
		PIMAGE_NT_HEADERS32 ntHeader = (PIMAGE_NT_HEADERS32)(buf + dHeader->e_lfanew);

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

		PIMAGE_IMPORT_DESCRIPTOR importDirectory = (PIMAGE_IMPORT_DESCRIPTOR)(moduleBuffer + VirtToFile(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, cachedSection));

		while (importDirectory->Characteristics) {
			PIMAGE_THUNK_DATA32 origFirstThunk = (PIMAGE_THUNK_DATA32)(moduleBuffer + VirtToFile(importDirectory->OriginalFirstThunk, cachedSection));
			PIMAGE_THUNK_DATA32 firstThunk = (PIMAGE_THUNK_DATA32)(moduleBuffer + VirtToFile(importDirectory->FirstThunk, cachedSection));
			//TODO: Find the right module inside the to be provided loaded module list
			void* module = nullptr;

			bool moduleLoaded = !!module;
			char* name = moduleBuffer + VirtToFile(importDirectory->Name, cachedSection);
			int len = strlen(name);
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
					importsWH.push_back({module, {nameOffset, FileToVirt((uint32_t)((char*)&firstThunk->u1.Function - moduleBuffer), cachedSection)}});

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
	bufSize = 0;

	for (auto& i : mod.sections)
		bufSize += i.bufSize;

	moduleBuffer = (char*)malloc(bufSize);

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
	relocationOffset = bufSize;
	bufSize += sizeof(uint32_t) + sizeof(WinRelocation) * mod.relocations.size();

	buffer = (char*)malloc(bufSize);

	*(uint32_t*)(buffer + sectionOffset) = mod.sections.size();
	memcpy(buffer + sectionOffset + sizeof(uint32_t), (char*)mod.sections.data(), sizeof(WinSection) * mod.sections.size());

	memcpy(buffer + nameOffset, mod.names.data(), mod.names.size());

	*(uint32_t*)(buffer + importsWHOffset) = mod.importsWH.size();
	memcpy(buffer + importsWHOffset + sizeof(uint32_t), mod.importsWH.data(), mod.importsWH.size() * sizeof(WinImportH));

	*(uint32_t*)(buffer + importsOffset) = mod.thunkedImports.size();
	memcpy(buffer + importsOffset + sizeof(uint32_t), mod.thunkedImports.data(), mod.thunkedImports.size() * sizeof(WinImport));

	*(uint32_t*)(buffer + importThunksOffset) = mod.importThunk.size();
	memcpy(buffer + importThunksOffset + sizeof(uint32_t), mod.importThunk.data(), mod.importThunk.size() * sizeof(WinImportThunk));

	*(uint32_t*)(buffer + relocationOffset) = mod.relocations.size();
	memcpy(buffer + relocationOffset + sizeof(uint32_t), mod.relocations.data(), mod.relocations.size() * sizeof(WinRelocation));
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

#define INLINE_MEMCPY(DEST, SOURCE, SIZE)		\
	for (uint32_t I_COUNTER = 0; I_COUNTER < SIZE; I_COUNTER++)	\
		((char*)(DEST))[I_COUNTER] = ((char*)(SOURCE))[I_COUNTER];

void LoadModule(void* loadData)
{
	WinLoadData* wLoadData = (WinLoadData*)loadData;

	PackedWinModule* packedModule = wLoadData->packedModule;
	char* outBuf = wLoadData->outBuf;
	GetProcAddressFn pGetProcAddress = wLoadData->pGetProcAddress;
	LoadLibraryAFn pLoadLibraryA = wLoadData->pLoadLibraryA;

	char* buffer = packedModule->buffer;

	//Copy sections
	uint32_t* sectionCount = (uint32_t*)(buffer + packedModule->sectionOffset);
	WinSection* sections = (WinSection*)(sectionCount + 1);

	for (uint32_t i = 0; i < *sectionCount; i++)
	    INLINE_MEMCPY(outBuf + sections[i].virtOffset, packedModule->moduleBuffer + sections[i].bufOffset, sections[i].bufSize);

	char* names = buffer + packedModule->nameOffset;

	uint32_t* importHCount = (uint32_t*)(buffer + packedModule->importsWHOffset);
	WinImportH* importsH = (WinImportH*)(importHCount + 1);

	for (uint32_t i = 0; i < *importHCount; i++) {
		if (importsH[i].imp.nameOffset < ~importsH[i].imp.nameOffset)
			*(nptr_t*)(outBuf + importsH[i].imp.virtOffset) = pGetProcAddress(importsH[i].module, names + importsH[i].imp.nameOffset);
		else
			*(nptr_t*)(outBuf + importsH[i].imp.virtOffset) = pGetProcAddress(importsH[i].module, (char*)(uintptr_t)~importsH[i].imp.nameOffset);
	}

	uint32_t* thunkedImportCount = (uint32_t*)(buffer + packedModule->importsOffset);
	WinImport* imports = (WinImport*)(thunkedImportCount + 1);

	uint32_t* importThunkCount = (uint32_t*)(buffer + packedModule->importThunksOffset);
    WinImportThunk* importThunks = (WinImportThunk*)(importThunkCount + 1);

	for (uint32_t i = 0; i < *importThunkCount; i++) {
		char* mod = names + importThunks[i].moduleNameOffset;
		void* module = pLoadLibraryA(mod);

		for (uint32_t o = importThunks[i].importOffset; o < importThunks[i].importOffset + importThunks[i].importCount; o++) {
			if (imports[o].nameOffset < ~imports[o].nameOffset)
				*(nptr_t*)(outBuf + imports[o].virtOffset) = pGetProcAddress(module, names + imports[o].nameOffset);
			else
				*(nptr_t*)(outBuf + imports[o].virtOffset) = pGetProcAddress(module, (char*)(uintptr_t)~imports[o].nameOffset);
		}
	}

	//TODO: Handle TLS data manually
	//TODO: Call LdrpHandleTlsData

	//DLL_PROCESS_ATTACH = 1
	((DllEntryPointFn)(outBuf + packedModule->entryPointOffset))(outBuf, 1, nullptr);

	return;
}
