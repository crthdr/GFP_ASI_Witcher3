
#include <fstream>
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <psapi.h>
#include <vector>
#include <string>


using std::to_string;
using std::string;
using std::wstring;
using std::string_view;
using vec = std::vector<uint8_t>;
using ptr = uint64_t;

HANDLE hProcess = NULL;
ptr baseAddress;
ptr endAddress;

bool NGE = false;

string NOT_SUPPORTED = string("Game version not supported.");


string getLastErrorAsString() {
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0) return "";

    LPSTR messageBuffer = nullptr;

    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorMessageID,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );
    string message(messageBuffer, size);

    LocalFree(messageBuffer);

    return message;
}

string makeJump(ptr addr, vec& jump, int size) {
    if (size <= 0) {
        size = 12;
    }
    if (size < 12) {
        return string("makeJump: size too small");
    }
    jump = vec(size, 0x90);

    jump[0] = 0x48; // mov rax, ?
    jump[1] = 0xB8;

    memcpy(&jump[2], &addr, sizeof(addr));

    jump[10] = 0xff; // jmp rax
    jump[11] = 0xe0;

    return "";
}

string insertJump(ptr targetAddress, ptr myFunction, int size, int& outJumpSize) {
    string err;
    vec jump;
    err = makeJump((ptr) myFunction, jump, size);
    if (err != "") return err;

     
    outJumpSize = static_cast<int>(jump.size());

    DWORD dwOldProtect;
    if (!VirtualProtectEx(hProcess, (LPVOID) targetAddress, size, PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
        return "insertJump/VirtualProtectEx " + getLastErrorAsString();
    }

    if (!WriteProcessMemory(hProcess, (LPVOID) targetAddress, jump.data(), jump.size(), NULL)) {
        return "insertJump/WriteProcessMemory " + getLastErrorAsString();
    }

    return "";
}

bool dataContains(const std::string& s) {
    uint8_t* start = reinterpret_cast<uint8_t*>(baseAddress);
    uint8_t* end = reinterpret_cast<uint8_t*>(endAddress);
    
    uint8_t* it = std::search(start, end, 
                             reinterpret_cast<const uint8_t*>(s.data()), 
                             reinterpret_cast<const uint8_t*>(s.data() + s.size()));
    
    return it != end; 
}

ptr findData(vec& s) {
    uint8_t* start = reinterpret_cast<uint8_t*>(baseAddress);
    uint8_t* end = reinterpret_cast<uint8_t*>(endAddress);
    
    uint8_t* it = std::search(start, end, 
                             reinterpret_cast<const uint8_t*>(s.data()), 
                             reinterpret_cast<const uint8_t*>(s.data() + s.size()));

    if (it == end) return 0;
    return (ptr) it;

}

string searchAndReplace(vec& search, vec& replace) {
    if (search.size() != replace.size()) {
        return string("oops");
    }
    size_t patternSize = search.size();
    ptr pattern = (ptr) search.data();

    ptr addr = findData(search);
    if (!addr) {
        return string("searchAndReplace: not found");
    }

    DWORD dwOldProtect;
    if (!VirtualProtectEx(hProcess, (LPVOID) addr, patternSize, PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
         return "searchAndReplace/VirtualProtectEx: " + getLastErrorAsString();
    }

    BOOL result = WriteProcessMemory(hProcess, (LPVOID) addr, replace.data(), patternSize, nullptr);
    if (!result) {
        return "searchAndReplace/WriteProcessMemory: " + getLastErrorAsString();
    }

    return "";
}

string detour(ptr addr, vec& expected, ptr jumpTo, ptr& jumpBack) {
    string err;

    for (int i = 0; i < expected.size(); i++) {
        uint8_t value = *reinterpret_cast<uint8_t*>(addr + i);
        if (value != expected[i]) {
            return "detour: not expected";
        }
    }

    int jumpSize;
    err = insertJump(addr, jumpTo, static_cast<int>(expected.size()), jumpSize);
    if (err != "") return err; 

    if (jumpSize < expected.size()) {
        vec fill(expected.size()-jumpSize, 0x90);
        if (!WriteProcessMemory(hProcess, (LPVOID) (addr+jumpSize), fill.data(), fill.size(), NULL)) {
            return "detour 0x90/WriteProcessMemory " + getLastErrorAsString();
        }
    }

    jumpBack = addr + expected.size();

    return "";
}

extern "C" void Perfect_OG();
extern "C" ptr after_Perfect_OG;
ptr after_Perfect_OG;


extern "C" void Perfect_NGE();
extern "C" ptr after_Perfect_NGE;
ptr after_Perfect_NGE;


extern "C" void Shadows_OG();
extern "C" ptr after_Shadows_OG;
ptr after_Shadows_OG;

extern "C" void Shadows_NGE();
extern "C" ptr after_Shadows_NGE;
ptr after_Shadows_NGE;

struct TypeDescriptor {
    void* pVFTable;
    void* spare;
    char name[1];
};

struct CompleteObjectLocator {
    unsigned long signature;
    unsigned long offset;
    unsigned long cdOffset;
    int typeDescriptorOffset;
    int classDescriptorOffset;
    int objectLocatorOffset;
};

string_view GetClassNameFromPointer(void* pObject) {
    if (!pObject) return "";

    void** vtable = *(void***)pObject;
    if (!vtable) return "";

    CompleteObjectLocator* pLocator = (CompleteObjectLocator*)(vtable[-1]);
    if (!pLocator) return "";

    DWORD64 moduleBase = (DWORD64)pLocator - pLocator->objectLocatorOffset;

    TypeDescriptor* pTypeDesc = (TypeDescriptor*)(moduleBase + pLocator->typeDescriptorOffset);
    if (!pTypeDesc) return "";

    return string_view(pTypeDesc->name);
}

class RedString {
public:
    wchar_t* m_buf;
    uint32_t m_size;

    inline const wchar_t* AsChar() {
        return m_size > 1 ? reinterpret_cast< const wchar_t* >(m_buf) : reinterpret_cast< const wchar_t* >(L"");
    }

    inline uint32_t GetLength() {
        return m_size ? static_cast<uint32_t>(m_size - 1) : 0;
    }
};


template<class T> 
class ReferencableInternalHandle_ { 
public:
    char raw[12 + sizeof(T*)]; 

    inline T* Get() {
        if (NGE) {
            return *reinterpret_cast<T**>(&raw[8]); 
        } else {
            return *reinterpret_cast<T**>(&raw[16]);
        }
    }
};

template<class T>
class THandle_ {
public:
	ReferencableInternalHandle_<T>* m_handle;

	inline T* Get() {
		if (m_handle) {
			return m_handle->Get();
		}
		return nullptr;
	}
};

class CDiskFile_ {
public:
	char pad1[24];
	RedString m_fileName;
};

class CEntityTemplate_ {
public:
	char pad1[88];
	CDiskFile_*	m_file;	
};

class CEntity_ {
public:
	char pad1[264];
	THandle_<CEntityTemplate_> m_template;
};

class CObject_ {};

class CDrawableComponent_ {
public:
    char pad1[0x30];
    CObject_* m_parent;
    char pad2[0x128];
    // 0x160
    uint32_t m_drawableFlags;

	inline CEntity_* GetEntity() {
		// safe
		return reinterpret_cast<CEntity_*>(m_parent);
	}
};


enum EDrawableFlags : uint32_t {
    DF_IsVisible = 1,
    DF_CastShadows = 2,
    DF_NoLighting = 4,
    DF_LocalWindSimulation = 8,
    DF_UseInAllApperances = 16,
    DF_NoColoring = 32,
    DF_NoDissolves = 64,
    DF_CameraTransformRotate = 128,
    DF_CameraTransformOnlyPosition = 256,
    DF_CastShadowsWhenNotVisible = 512,
    // ...
};


extern "C" void impl_Shadows(CDrawableComponent_ *self) {
#ifdef _DEBUG
    //OutputDebugString("__________impl_Shadows\n");
    OutputDebugString((string("this className = ") + string(GetClassNameFromPointer(self)) + "\n").c_str());
#endif

	CEntity_* entity = self->GetEntity();

	if (!entity) return;

    string_view className = GetClassNameFromPointer(entity);

#ifdef _DEBUG
    OutputDebugString((string("entity className = ") + string(className) + "\n").c_str());
#endif

	CEntityTemplate_ *entityTemplate = entity->m_template.Get();

	if (!entityTemplate) return;

	CDiskFile_* file = entityTemplate->m_file;

	if (!file) return;

	std::wstring_view name(file->m_fileName.AsChar());

	if (className == ".?AVCItemEntity@@" ||
		className == ".?AVCWitcherSword@@" ||
		className == ".?AVCProjectileTrajectory@@" ||
		className == ".?AVRangedWeapon@@" || // without C
		name.find(L"ciri") != std::string::npos) {

		self->m_drawableFlags |= DF_CastShadowsWhenNotVisible;
		self->m_drawableFlags |= DF_NoDissolves;
	}

    if (name == L"torch.w2ent") {
        self->m_drawableFlags |= DF_CastShadows;
    }
}


string ASI_Main() {
#ifdef _DEBUG
    OutputDebugString("Hello ASI\n");
#endif

    DWORD processID = GetCurrentProcessId();
    auto perm = PROCESS_ALL_ACCESS;

    hProcess = OpenProcess(perm, FALSE, processID);
    if (hProcess == NULL) {
        return getLastErrorAsString();
    }

    HMODULE hModule = GetModuleHandle("witcher3.exe");
    if (hModule == NULL) {
        return getLastErrorAsString();
    }

    MODULEINFO modInfo;
    
    if (!GetModuleInformation(hProcess, hModule, &modInfo, sizeof(modInfo))) {
        return getLastErrorAsString();
    }
    baseAddress = (ptr) modInfo.lpBaseOfDll;
    endAddress = baseAddress + modInfo.SizeOfImage;

    string err;

    // 48 8B 01 33 D2 FF 90 A0 00 00 00 48 8B 8E F0 01 00 00 48 8B 01 FF 90 A8 00 00 00
    vec perfectSearch_ = { 0x48, 0x8B, 0x01, 0x33, 0xD2, 0xFF, 0x90, 0xA0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x8E, 0xF0, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00 };

    ptr addr;

    if (dataContains(string("v 1.32")) || dataContains(string("v 1.31"))) {

        addr = findData(perfectSearch_);
        if (!addr) return "perfect not found";

        addr += perfectSearch_.size() + 18;

        vec perfectExpectedOG = { 0x4C, 0x8D, 0x9C, 0x24, 0xB0, 0x02, 0x00, 0x00, 0x49, 0x8b, 0x5b, 0x48, };
        
        err = detour(addr, perfectExpectedOG, (ptr) &Perfect_OG, after_Perfect_OG);
        if (err != "") return err;

        // 48 8b d3 48 8b cf 48 8b 5c 24 30 48 83 c4 20 5f e9 2e 08 00 00
        vec shadowsSearchOG = { 0x48, 0x8b, 0xd3, 0x48, 0x8b, 0xcf, 0x48, 0x8b, 0x5c, 0x24, 0x30, 0x48, 0x83, 0xc4, 0x20, 0x5f, 0xe9, 0x2e, 0x08, 0x0, 0x0 };
        vec shadowsExpectOG = { 0x48, 0x8b, 0xd3, 0x48, 0x8b, 0xcf, 0x48, 0x8b, 0x5c, 0x24, 0x30, 0x48, 0x83, 0xc4, 0x20, 0x5f };
        ptr shadowsAddr = findData(shadowsSearchOG);

        if (!shadowsAddr) return "shadows not found";

        err = detour(shadowsAddr, shadowsExpectOG, (ptr) &Shadows_OG, after_Shadows_OG);
        if (err != "") return err;


        vec fovSearchOG = { 0xF3, 0x0F, 0x59, 0xC0, 0x0F, 0x2F, 0xC1, 0x77, 0x03, 0x0F, 0x28, 0xC1, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
        vec fovReplaceOG = fovSearchOG;
        fovReplaceOG[2] = 0x5e;
        err = searchAndReplace(fovSearchOG, fovReplaceOG);
        if (err != "") return err;


        vec treesSearchOG = { 0x48, 0x89, 0x43, 0x34, 0x48, 0xFF, 0xC7, 0x49 };
        vec treesReplaceOG = { 0x90, 0x90, 0x90, 0x90, 0x48, 0xFF, 0xC7, 0x49 };

        err = searchAndReplace(treesSearchOG, treesReplaceOG);
        if (err != "") return err;

        vec actorsSearchOG = { 0x74, 0x6e, 0x48, 0x8b, 0xd3, 0x48, 0x8b, 0xce };
        vec actorsReplaceOG = { 0xeb, 0x6e, 0x48, 0x8b, 0xd3, 0x48, 0x8b, 0xce };

        err = searchAndReplace(actorsSearchOG, actorsReplaceOG);
        if (err != "") return err;
    } else if (dataContains(string("v 4.04"))) {
        NGE = true;

        addr = findData(perfectSearch_);
        if (!addr) return "perfect not found";

        addr += perfectSearch_.size() - 54;

        vec perfectExpectedNGE = { 0x48, 0x8B, 0x8E, 0xF0, 0x01, 0x00, 0x00, 0x4C, 0x8B, 0xBC, 0x24, 0x88, 0x05, 0x00, 0x00 };

        err = detour(addr, perfectExpectedNGE, (ptr) &Perfect_NGE, after_Perfect_NGE);
        if (err != "") return err;


        // 48 8b d6 48 8b cb 48 8b 5c 24 40 48 83 c4 20 5e e9 d8 f0 ff ff 
        vec shadowsSearchNGE = { 0x48, 0x8b, 0xd6, 0x48, 0x8b, 0xcb, 0x48, 0x8b, 0x5c, 0x24, 0x40, 0x48, 0x83, 0xc4, 0x20, 0x5e, 0xe9, 0xd8, 0xf0, 0xff, 0xff };
        vec shadowsExpectNGE = { 0x48, 0x8b, 0xd6, 0x48, 0x8b, 0xcb, 0x48, 0x8b, 0x5c, 0x24, 0x40, 0x48, 0x83, 0xc4, 0x20, 0x5e };
        ptr shadowsAddr = findData(shadowsSearchNGE);

        if (!shadowsAddr) return "shadows not found";

        err = detour(shadowsAddr, shadowsExpectNGE, (ptr) &Shadows_NGE, after_Shadows_NGE);
        if (err != "") return err;

        vec searchFovNGE = { 0x0C, 0x00, 0x40, 0x40, 0x04, 0x6C, 0x48, 0x40, 0xC3, 0xF5, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x40, 0x25, 0x06, 0x49, 0x40, 0xDB, 0x0F, 0x49, 0x40, 0xDC, 0x0F, 0x49, 0x40 };
        vec replaceFovNGE = { 0x3f, 0x80, 0x0, 0x0, 0x04, 0x6C, 0x48, 0x40, 0xC3, 0xF5, 0x48, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x40, 0x25, 0x06, 0x49, 0x40, 0xDB, 0x0F, 0x49, 0x40, 0xDC, 0x0F, 0x49, 0x40 };

        err = searchAndReplace(searchFovNGE, replaceFovNGE);
        if (err != "") return err;

        vec actorsSearchNGE = { 0xE8, 0x84, 0xB7, 0xFF, 0xFF };
        vec actorsReplaceNGE = { 0x90, 0x90, 0x90, 0x90, 0x90 };

        err = searchAndReplace(actorsSearchNGE, actorsReplaceNGE);
        if (err != "") return err;
    } else {
        return NOT_SUPPORTED;
    }

    return "";
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        string err = ASI_Main();

        if (err != "") {
#ifndef _DEBUG
            err = NOT_SUPPORTED;
#endif
            MessageBox(NULL, err.data(), "GFP.asi", MB_OK | MB_ICONSTOP);
        }
  
        if (hProcess) {
            CloseHandle(hProcess);
        }
    }

    return TRUE;
}