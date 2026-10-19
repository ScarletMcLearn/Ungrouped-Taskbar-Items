#include <windows.h>
#include <strsafe.h>
#include "windhawk_utils.h"
#include "MinHook.h"

BOOL Wh_ModInit();
void Wh_ModAfterInit();

void SignalLauncher(bool success) {
    wchar_t name[128];
    StringCchPrintfW(name, ARRAYSIZE(name), L"Local\\TaskbarUngrouped%s-%lu",
                     success ? L"Ready" : L"Failed", GetCurrentProcessId());
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
    if (event) {
        SetEvent(event);
        CloseHandle(event);
    }
}

DWORD WINAPI InitializeTaskbarUngrouped(void*) {
    Wh_Log(L"TaskbarUngrouped initializing");
    if (MH_Initialize() != MH_OK) { Wh_Log(L"MinHook initialization failed"); SignalLauncher(false); return 1; }
    if (!Wh_ModInit()) { Wh_Log(L"Taskbar hooks failed to initialize"); SignalLauncher(false); return 2; }
    Wh_ModAfterInit();
    MH_EnableHook(MH_ALL_HOOKS);
    Wh_Log(L"TaskbarUngrouped enabled");
    SignalLauncher(true);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        SetStandaloneModule(instance);
        HANDLE thread = CreateThread(nullptr, 0, InitializeTaskbarUngrouped, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
