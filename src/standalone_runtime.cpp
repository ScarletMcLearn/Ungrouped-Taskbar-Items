#include "windhawk_utils.h"

#include <dbghelp.h>
#include <strsafe.h>

#include <unordered_map>

#include "MinHook.h"

namespace {
bool g_symbolsInitialized = false;
HMODULE g_standaloneModule = nullptr;
std::unordered_map<std::wstring, void*> g_symbolCache;

void EnsureSymbols() {
    if (g_symbolsInitialized) return;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    wchar_t cache[MAX_PATH];
    GetTempPathW(ARRAYSIZE(cache), cache);
    StringCchCatW(cache, ARRAYSIZE(cache), L"TaskbarUngroupedSymbols");
    CreateDirectoryW(cache, nullptr);
    wchar_t path[MAX_PATH * 2];
    StringCchPrintfW(path, ARRAYSIZE(path), L"srv*%s*https://msdl.microsoft.com/download/symbols", cache);
    g_symbolsInitialized = SymInitializeW(GetCurrentProcess(), path, TRUE) == TRUE;
    if (!g_symbolsInitialized) Wh_Log(L"SymInitialize failed: %lu", GetLastError());
}

void LoadModuleSymbols(HMODULE module) {
    SymRefreshModuleList(GetCurrentProcess());
    IMAGEHLP_MODULEW64 info{.SizeOfStruct = sizeof(info)};
    if (SymGetModuleInfoW64(GetCurrentProcess(), reinterpret_cast<DWORD64>(module), &info)) return;
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(module, path, ARRAYSIZE(path))) return;
    if (!SymLoadModuleExW(GetCurrentProcess(), nullptr, path, nullptr,
                          reinterpret_cast<DWORD64>(module), 0, nullptr, 0)) {
        Wh_Log(L"SymLoadModuleEx failed for %s: %lu", path, GetLastError());
    }
}

struct SymbolSearch {
    std::wstring needle;
    void* address = nullptr;
};

BOOL CALLBACK FindMatchingSymbol(PSYMBOL_INFOW info, ULONG, PVOID context) {
    auto* search = static_cast<SymbolSearch*>(context);
    if (wcsstr(info->Name, search->needle.c_str())) {
        search->address = reinterpret_cast<void*>(info->Address);
        return FALSE;
    }
    return TRUE;
}

std::wstring ExtractQualifiedName(PCWSTR signature) {
    PCWSTR start = wcsstr(signature, L"__cdecl ");
    if (!start) return {};
    start += wcslen(L"__cdecl ");
    PCWSTR end = wcschr(start, L'(');
    return end ? std::wstring(start, end) : std::wstring();
}

void* ResolveSymbol(HMODULE module, PCWSTR name) {
    auto cached = g_symbolCache.find(name);
    if (cached != g_symbolCache.end()) return cached->second;
    if (void* diaAddress = ResolveDiaSymbol(module, name)) {
        g_symbolCache.emplace(name, diaAddress);
        return diaAddress;
    }
    EnsureSymbols();
    if (!g_symbolsInitialized) return nullptr;
    LoadModuleSymbols(module);
    alignas(SYMBOL_INFOW) unsigned char bytes[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)]{};
    auto* info = reinterpret_cast<SYMBOL_INFOW*>(bytes);
    info->SizeOfStruct = sizeof(*info);
    info->MaxNameLen = MAX_SYM_NAME;
    if (SymFromNameW(GetCurrentProcess(), name, info)) {
        g_symbolCache.emplace(name, reinterpret_cast<void*>(info->Address));
        return reinterpret_cast<void*>(info->Address);
    }
    auto needle = ExtractQualifiedName(name);
    if (!needle.empty()) {
        SymbolSearch search{.needle = needle};
        SymEnumSymbolsW(GetCurrentProcess(), reinterpret_cast<DWORD64>(module), L"*", FindMatchingSymbol, &search);
        if (search.address) {
            Wh_Log(L"Resolved by enumeration: %s", name);
            g_symbolCache.emplace(name, search.address);
            return search.address;
        }
    }
    Wh_Log(L"Symbol lookup failed: %s (%lu)", name, GetLastError());
    return nullptr;
}
}  // namespace

void SetStandaloneModule(HMODULE module) { g_standaloneModule = module; }

void Wh_Log(PCWSTR format, ...) {
    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);
    OutputDebugStringW(message);
    wchar_t path[MAX_PATH];
    if (!g_standaloneModule || !GetModuleFileNameW(g_standaloneModule, path, ARRAYSIZE(path))) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    StringCchCopyW(slash + 1, ARRAYSIZE(path) - (slash + 1 - path), L"TaskbarUngrouped.log");
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    StringCchCatW(message, ARRAYSIZE(message), L"\r\n");
    DWORD written;
    WriteFile(file, message, static_cast<DWORD>(wcslen(message) * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
}

PCWSTR Wh_GetStringSetting(PCWSTR format, ...) {
    PCWSTR value = L"";
    if (wcscmp(format, L"pinnedItemsMode") == 0) value = L"replace";
    else if (wcscmp(format, L"placeUngroupedItemsTogether") == 0) value = L"0";
    else if (wcscmp(format, L"groupingMode") == 0) value = L"regular";
    size_t size = (wcslen(value) + 1) * sizeof(wchar_t);
    auto* copy = static_cast<wchar_t*>(CoTaskMemAlloc(size));
    if (copy) memcpy(copy, value, size);
    return copy ? copy : L"";
}

void Wh_FreeStringSetting(PCWSTR value) {
    if (value && value[0]) CoTaskMemFree(const_cast<PWSTR>(value));
}

int Wh_GetIntSetting(PCWSTR) { return 0; }

BOOL Wh_SetFunctionHook(void* target, void* detour, void** original) {
    if (MH_CreateHook(target, detour, original) != MH_OK) return FALSE;
    return TRUE;
}

void Wh_ApplyHookOperations() { MH_EnableHook(MH_ALL_HOOKS); }

bool WindhawkUtils::HookSymbols(HMODULE module, const SYMBOL_HOOK* hooks, size_t count) {
    bool succeeded = true;
    for (size_t i = 0; i < count; ++i) {
        void* address = nullptr;
        for (const auto& name : hooks[i].symbols) {
            address = ResolveSymbol(module, name.c_str());
            if (address) break;
        }
        if (!address) {
            Wh_Log(L"Symbol not found: %s", hooks[i].symbols.front().c_str());
            if (!hooks[i].optional) succeeded = false;
            continue;
        }
        if (hooks[i].hookFunction) {
            if (!Wh_SetFunctionHook(address, hooks[i].hookFunction, hooks[i].originalFunction)) succeeded = false;
        } else {
            *hooks[i].originalFunction = address;
        }
    }
    Wh_ApplyHookOperations();
    return succeeded;
}
