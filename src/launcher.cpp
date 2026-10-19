#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {
DWORD FindExplorerProcess() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
    DWORD pid = 0;
    DWORD currentSessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &currentSessionId);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            DWORD sessionId = 0;
            if (_wcsicmp(entry.szExeFile, L"explorer.exe") == 0 &&
                ProcessIdToSessionId(entry.th32ProcessID, &sessionId) &&
                sessionId == currentSessionId) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

bool InjectDll(DWORD pid, const std::wstring& dllPath) {
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
                                 FALSE, pid);
    if (!process) { std::wcerr << L"OpenProcess failed: " << GetLastError() << L'\n'; return false; }
    const size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { std::wcerr << L"VirtualAllocEx failed: " << GetLastError() << L'\n'; CloseHandle(process); return false; }
    bool written = WriteProcessMemory(process, remote, dllPath.c_str(), bytes, nullptr) == TRUE;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel, "LoadLibraryW"));
    HANDLE thread = written ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr) : nullptr;
    if (!thread) { std::wcerr << L"CreateRemoteThread failed: " << GetLastError() << L'\n'; VirtualFreeEx(process, remote, 0, MEM_RELEASE); CloseHandle(process); return false; }
    DWORD waited = WaitForSingleObject(thread, 15000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    if (waited != WAIT_OBJECT_0 || exitCode == 0) {
        std::wcerr << L"Explorer LoadLibraryW failed or timed out. Remote result: " << exitCode << L'\n';
        return false;
    }
    return true;
}
}

int wmain(int argc, wchar_t** argv) {
    wchar_t executable[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable))) return 1;
    auto dllPath = std::filesystem::path(executable).parent_path() / L"TaskbarUngrouped.dll";
    if (!std::filesystem::exists(dllPath)) {
        std::wcerr << L"Missing " << dllPath.c_str() << L'\n';
        return 2;
    }
    DWORD pid = FindExplorerProcess();
    if (!pid) { std::wcerr << L"Active explorer.exe not found.\n"; return 3; }
    if (argc == 2 && wcscmp(argv[1], L"--check") == 0) {
        std::wcout << L"Ready: explorer.exe PID " << pid << L", DLL " << dllPath.c_str() << L'\n';
        return 0;
    }
    wchar_t readyName[128], failedName[128];
    StringCchPrintfW(readyName, ARRAYSIZE(readyName), L"Local\\TaskbarUngroupedReady-%lu", pid);
    StringCchPrintfW(failedName, ARRAYSIZE(failedName), L"Local\\TaskbarUngroupedFailed-%lu", pid);
    HANDLE events[2] = {
        CreateEventW(nullptr, TRUE, FALSE, readyName),
        CreateEventW(nullptr, TRUE, FALSE, failedName),
    };
    if (!events[0] || !events[1]) { std::wcerr << L"Could not create status events.\n"; return 5; }
    ResetEvent(events[0]);
    ResetEvent(events[1]);
    if (!InjectDll(pid, dllPath.wstring())) {
        CloseHandle(events[0]); CloseHandle(events[1]);
        return 4;
    }
    DWORD status = WaitForMultipleObjects(2, events, FALSE, 90000);
    CloseHandle(events[0]); CloseHandle(events[1]);
    if (status == WAIT_OBJECT_0) {
        std::wcout << L"Enabled. Close and reopen Edge windows to create separate taskbar buttons.\n";
        return 0;
    }
    if (status == WAIT_OBJECT_0 + 1) {
        std::wcerr << L"Extension loaded but taskbar hook initialization failed. Read %TEMP%\\TaskbarUngrouped.log.\n";
        return 6;
    }
    std::wcerr << L"Extension loaded, but did not finish initialization within 90 seconds. Read %TEMP%\\TaskbarUngrouped.log.\n";
    return 7;
}
