param([string]$BundlePath = '')
$ErrorActionPreference = 'Stop'
if (-not $BundlePath) {
    $BundlePath = Join-Path $PSScriptRoot 'Pitch Pop.vst3'
    if (-not (Test-Path -LiteralPath $BundlePath)) { throw 'Pass -BundlePath with the full path to the built Pitch Pop.vst3 folder.' }
}
$bundle = (Resolve-Path -LiteralPath $BundlePath).Path
if (-not (Test-Path -LiteralPath (Join-Path $bundle 'Contents/x86_64-win/Pitch Pop.vst3'))) { throw 'This is not the complete Windows x64 Pitch Pop VST3 bundle.' }
if (Get-Process -Name FL,FL64,Suitunes,'Pitch Pop' -ErrorAction SilentlyContinue) { throw 'Save your work and close FL Studio and Pitch Pop before installing.' }
$commonFiles = if ($env:CommonProgramW6432) { $env:CommonProgramW6432 } else { $env:CommonProgramFiles }
$destination = Join-Path $commonFiles 'VST3/2of3'
# Preserve identity: require the user to archive the old-name bundle outside scanned folders first.
foreach ($taskLegacy in @((Join-Path $commonFiles 'VST3/Suitunes.vst3'),(Join-Path $destination 'Suitunes.vst3'))) {
    if (Test-Path -LiteralPath $taskLegacy) { throw "Archive the old Suitunes bundle outside VST search paths first: $taskLegacy. It shares Pitch Pop's VST3 identity." }
}
New-Item -ItemType Directory -Force $destination | Out-Null
$taskExisting = Join-Path $destination 'Pitch Pop.vst3'
if (Test-Path -LiteralPath $taskExisting) {
    $taskBackupRoot = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('PitchPopBackups/' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Force $taskBackupRoot | Out-Null
    Copy-Item -LiteralPath $taskExisting -Destination $taskBackupRoot -Recurse
    Write-Output "Existing Pitch Pop backed up to $taskBackupRoot"
}
Copy-Item -LiteralPath $bundle -Destination $destination -Recurse -Force
Write-Output "Installed Pitch Pop to $destination. Open FL Studio > Options > Manage plugins, verify and rescan previously verified plugins."
