[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$toolchain = if ($env:TASKBAR_UNGROUPED_LLVM_MINGW) {
    $env:TASKBAR_UNGROUPED_LLVM_MINGW
} else {
    Join-Path $PSScriptRoot 'work\llvm-mingw\llvm-mingw-20260908-ucrt-x86_64'
}
$clang = Join-Path $toolchain 'bin\clang++.exe'
if (-not (Test-Path -LiteralPath $clang)) {
    throw "LLVM-MinGW missing. Set TASKBAR_UNGROUPED_LLVM_MINGW to extracted x86_64 toolchain root. Expected: $clang"
}

$visualStudio = if ($env:TASKBAR_UNGROUPED_VS_PATH) {
    $env:TASKBAR_UNGROUPED_VS_PATH
} else {
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
}
$diaInclude = Join-Path $visualStudio 'DIA SDK\include'
$diaLib = Join-Path $visualStudio 'DIA SDK\lib\amd64\diaguids.lib'
if (-not (Test-Path -LiteralPath $diaInclude) -or -not (Test-Path -LiteralPath $diaLib)) {
    throw "Visual Studio DIA SDK missing. Install Visual Studio 2022 Build Tools with C++ build tools, or set TASKBAR_UNGROUPED_VS_PATH. Expected: $visualStudio"
}

$release = Join-Path $PSScriptRoot 'release'
New-Item -ItemType Directory -Path $release -Force | Out-Null

$common = @('-std=c++20', '-O2', '-static-libstdc++', '-static-libgcc', '-DUNICODE', '-D_UNICODE', '-I', 'src', '-I', 'third_party\minhook\include')
$minhook = @('third_party\minhook\src\buffer.c', 'third_party\minhook\src\hook.c', 'third_party\minhook\src\trampoline.c', 'third_party\minhook\src\hde\hde64.c')

& $clang @common '-I' $diaInclude '-shared' 'src\entry.cpp' 'src\standalone_runtime.cpp' 'src\dia_resolver.cpp' 'src\taskbar_grouping.cpp' @minhook '-o' 'release\TaskbarUngrouped.dll' '-ldbghelp' '-lpsapi' '-lcomctl32' '-lole32' '-loleaut32' '-lshlwapi' '-lversion' $diaLib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $clang @common '-municode' 'src\launcher.cpp' '-o' 'release\TaskbarUngrouped.exe'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item -LiteralPath 'scripts\TaskbarUngrouped.ps1', 'scripts\taskbar-ungrouped.cmd' -Destination $release -Force
exit 0
