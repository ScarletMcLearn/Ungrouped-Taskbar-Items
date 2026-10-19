[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$commandDirectory = Join-Path $PSScriptRoot 'release'
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$entries = @($userPath -split ';' | Where-Object { $_ })
if ($entries -notcontains $commandDirectory) {
    [Environment]::SetEnvironmentVariable('Path', (($entries + $commandDirectory) -join ';'), 'User')
}

Write-Host 'Installed command: taskbar-ungrouped on|off'
Write-Host 'Open a new PowerShell window before first use.'
