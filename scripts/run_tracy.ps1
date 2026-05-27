#!/usr/bin/env pwsh

param(
    [ValidateSet("profiler", "capture", "csvexport", "import-chrome", "import-fuchsia", "update")]
    [string]$Tool = "profiler",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ToolArgs
)

$ErrorActionPreference = "Stop"

$tracyRoot = Join-Path $PSScriptRoot "..\tools\Tracy"
$exeMap = @{
    "profiler"       = "tracy-profiler.exe"
    "capture"        = "tracy-capture.exe"
    "csvexport"      = "tracy-csvexport.exe"
    "import-chrome"  = "tracy-import-chrome.exe"
    "import-fuchsia" = "tracy-import-fuchsia.exe"
    "update"         = "tracy-update.exe"
}

$exeName = $exeMap[$Tool]
$exePath = Get-ChildItem -Path $tracyRoot -Recurse -Filter $exeName -File |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $exePath) {
    throw "Could not find $exeName under '$tracyRoot'."
}

Write-Host "Launching ${Tool}: $exePath" -ForegroundColor Cyan
& $exePath @ToolArgs
