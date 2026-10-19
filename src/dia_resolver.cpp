#include "windhawk_utils.h"

#include <dia2.h>
#include <strsafe.h>

#include <unordered_map>

namespace {
template <typename T>
class ComPtr {
  public:
    ~ComPtr() { if (value_) value_->Release(); }
    T* get() const { return value_; }
    T** put() { if (value_) value_->Release(); value_ = nullptr; return &value_; }
  private:
    T* value_ = nullptr;
};

ComPtr<IDiaDataSource> g_source;
ComPtr<IDiaSession> g_session;
ComPtr<IDiaSymbol> g_global;
std::unordered_map<std::wstring, void*> g_symbols;
bool g_initialized = false;

std::wstring ModuleDirectory() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<PCWSTR>(&ResolveDiaSymbol), &self);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(self, path, ARRAYSIZE(path));
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    return path;
}

bool LoadDia() {
    std::wstring root = ModuleDirectory();
    std::wstring cache = root + L"\\symbols";
    CreateDirectoryW(cache.c_str(), nullptr);
    // Explorer is x64. Visual Studio's Common7 copy is x86, so it cannot be
    // loaded here. Debugging Tools ships matching x64 SymSrv and is the
    // supported source of symbol-server functionality.
    const wchar_t* debugTools = L"C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64";
    const wchar_t* visualStudio = L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools";
    std::wstring symSrv = std::wstring(debugTools) + L"\\symsrv.dll";
    std::wstring dia = std::wstring(visualStudio) + L"\\DIA SDK\\bin\\amd64\\msdia140.dll";
    if (!LoadLibraryExW(symSrv.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
        Wh_Log(L"DIA x64 symsrv unavailable: %s (%lu). Install Windows SDK > Debugging Tools for Windows.",
               symSrv.c_str(), GetLastError());
        return false;
    }
    HMODULE module = LoadLibraryExW(dia.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) { Wh_Log(L"DIA load failed: %lu", GetLastError()); return false; }
    auto getClassObject = reinterpret_cast<decltype(&DllGetClassObject)>(GetProcAddress(module, "DllGetClassObject"));
    if (!getClassObject) { Wh_Log(L"DIA class factory missing"); return false; }
    ComPtr<IClassFactory> factory;
    HRESULT hr = getClassObject(CLSID_DiaSource, IID_IClassFactory, reinterpret_cast<void**>(factory.put()));
    if (FAILED(hr)) { Wh_Log(L"DIA class factory failed: 0x%08X", hr); return false; }
    hr = factory.get()->CreateInstance(nullptr, IID_IDiaDataSource, reinterpret_cast<void**>(g_source.put()));
    if (FAILED(hr)) { Wh_Log(L"DIA source create failed: 0x%08X", hr); return false; }
    return true;
}

bool Initialize(HMODULE module) {
    if (g_initialized) return g_global.get() != nullptr;
    g_initialized = true;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (!LoadDia()) return false;
    wchar_t modulePath[MAX_PATH];
    if (!GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath))) return false;
    std::wstring search = L"srv*" + ModuleDirectory() + L"\\symbols*https://msdl.microsoft.com/download/symbols";
    Wh_Log(L"DIA loading symbols for %s", modulePath);
    HRESULT hr = g_source.get()->loadDataForExe(modulePath, search.c_str(), nullptr);
    if (FAILED(hr)) { Wh_Log(L"DIA PDB load failed: 0x%08X", hr); return false; }
    hr = g_source.get()->openSession(g_session.put());
    if (FAILED(hr)) { Wh_Log(L"DIA open session failed: 0x%08X", hr); return false; }
    hr = g_session.get()->get_globalScope(g_global.put());
    if (FAILED(hr)) { Wh_Log(L"DIA global scope failed: 0x%08X", hr); return false; }
    const DWORD tags[] = { SymTagPublicSymbol, SymTagFunction, SymTagData };
    for (DWORD tag : tags) {
        ComPtr<IDiaEnumSymbols> symbols;
        if (FAILED(g_global.get()->findChildren(static_cast<enum SymTagEnum>(tag), nullptr, nsNone, symbols.put()))) continue;
        while (true) {
            ComPtr<IDiaSymbol> symbol;
            ULONG count = 0;
            hr = symbols.get()->Next(1, symbol.put(), &count);
            if (hr != S_OK || count == 0) break;
            DWORD rva = 0;
            if (symbol.get()->get_relativeVirtualAddress(&rva) != S_OK) continue;
            BSTR name = nullptr;
            if (symbol.get()->get_undecoratedName(&name) == S_OK && name) {
                g_symbols.emplace(name, reinterpret_cast<BYTE*>(module) + rva);
                SysFreeString(name);
            }
        }
    }
    Wh_Log(L"DIA loaded %zu symbols", g_symbols.size());
    return !g_symbols.empty();
}
}

void* ResolveDiaSymbol(HMODULE module, PCWSTR signature) {
    if (!Initialize(module)) return nullptr;
    auto it = g_symbols.find(signature);
    return it == g_symbols.end() ? nullptr : it->second;
}
