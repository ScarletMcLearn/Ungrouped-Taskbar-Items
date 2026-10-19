[CmdletBinding(SupportsShouldProcess)]
param(
    [switch]$Undo,
    [switch]$RestartExplorer
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$advancedKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
$primaryName = 'TaskbarGlomLevel'
$secondaryName = 'MMTaskbarGlomLevel'

if ($Undo) {
    $values = @{
        $primaryName = 0
        $secondaryName = 0
    }
    $action = 'restore Windows default taskbar grouping'
}
else {
    # 2 = Never combine.  Applies to primary and additional monitors.
    $values = @{
        $primaryName = 2
        $secondaryName = 2
    }
    $action = 'show each window as a separate taskbar button'
}

if ($PSCmdlet.ShouldProcess($advancedKey, $action)) {
    foreach ($entry in $values.GetEnumerator()) {
        New-ItemProperty -Path $advancedKey -Name $entry.Key -PropertyType DWord -Value $entry.Value -Force | Out-Null
    }
}

if ($RestartExplorer) {
    if ($PSCmdlet.ShouldProcess('explorer.exe', 'restart Explorer to apply taskbar setting now')) {
        Get-Process -Name explorer -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Process explorer.exe
    }
}
else {
    if ($WhatIfPreference) {
        Write-Host 'No changes made. Sign out/in or use -RestartExplorer after applying.'
    }
    else {
        Write-Host 'Saved. Sign out/in or run with -RestartExplorer to apply now.'
    }
}

if ($WhatIfPreference) {
    Write-Host 'Preview only.'
}
elseif ($Undo) {
    Write-Host 'Taskbar grouping restored.'
}
else {
    Write-Host 'Requested native Never combine. Verify taskbar: some modern Windows taskbars ignore this legacy preference.'
}
