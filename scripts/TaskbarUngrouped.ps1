[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('on', 'off')]
    [string]$Action
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Action) {
    Write-Host 'Usage: taskbar-ungrouped on|off'
    exit 2
}

function Restart-Explorer {
    Get-Process -Name explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Process explorer.exe
    Start-Sleep -Milliseconds 1500
}

switch ($Action) {
    'off' {
        Restart-Explorer
        Write-Host 'Taskbar Ungrouped: off.'
    }
    'on' {
        Restart-Explorer
        & (Join-Path $PSScriptRoot 'TaskbarUngrouped.exe')
        exit $LASTEXITCODE
    }
}
