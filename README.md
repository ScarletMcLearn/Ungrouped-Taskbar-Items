# Ungrouped Taskbar Items

Standalone GPL-3 Windows 11 Explorer extension. Creates one taskbar item per window, including Microsoft Edge windows, so items can be reordered independently. It does **not** require Windhawk.

> **Prefer native Windows first.** Windows 11: **Settings → Personalization → Taskbar → Taskbar behaviors → Combine taskbar buttons and hide labels → Never**. This shows labeled, never-combined taskbar buttons. This project exists for Windows builds where native behavior is insufficient.

## Important limitations

- Experimental: injects a DLL into your own `explorer.exe` and hooks private Microsoft taskbar code.
- Supports **64-bit Windows 11 only**. A Windows update can change private symbols and break it.
- Must have internet access to Microsoft Symbol Server on first run.
- Do not run it elevated. If taskbar becomes unstable, restart Explorer or sign out; extension unloads.
- Windows Smart App Control / WDAC can block locally built unsigned binaries. Only allow code you built and trust.

## Runtime prerequisites

Install these before running a built release:

1. **Windows SDK → Debugging Tools for Windows**. Required x64 file: `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\symsrv.dll`.
2. **Visual Studio 2022 Build Tools**, including C++ build tools / DIA SDK. Required x64 file: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\DIA SDK\bin\amd64\msdia140.dll`.

## Build from a clone

Install [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases) x86_64 UCRT build. Then set its extracted root and build:

```powershell
$env:TASKBAR_UNGROUPED_LLVM_MINGW = 'C:\Tools\llvm-mingw-<version>-ucrt-x86_64'
& .\build.ps1
```

If Build Tools lives elsewhere:

```powershell
$env:TASKBAR_UNGROUPED_VS_PATH = 'D:\Tools\Microsoft Visual Studio\2022\BuildTools'
& .\build.ps1
```

Build creates `release\TaskbarUngrouped.dll` and `release\TaskbarUngrouped.exe`, plus command wrappers.

## Run

From normal desktop PowerShell:

```powershell
& .\Install-TaskbarUngroupedCommand.ps1
```

Open a new PowerShell window. Then:

```powershell
taskbar-ungrouped on
taskbar-ungrouped off
```

Both commands restart Explorer. `on` injects a fresh extension instance. `off` restarts Explorer without injection and removes it from current session. Close/reopen Edge windows after enabling so they get separate taskbar identities.

For a one-off launch:

```powershell
& .\release\TaskbarUngrouped.exe
```

Diagnostic log: `release\TaskbarUngrouped.log`.

## Native setting helper

`Set-TaskbarUngrouped.ps1` writes legacy native `Never combine` preferences. Modern Windows taskbars can ignore it; use Windows Settings above instead.

## License

This project is GPL-3.0-or-later because `src/taskbar_grouping.cpp` derives from Ramen Software's Windhawk Taskbar Grouping mod. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). `third_party/minhook/` is BSD-2-Clause.
