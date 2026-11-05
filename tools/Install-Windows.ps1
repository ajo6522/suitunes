param([string]$BundlePath = '')
$ErrorActionPreference = 'Stop'
if (-not $BundlePath) {
    $BundlePath = Join-Path $PSScriptRoot 'Suitunes.vst3'
    if (-not (Test-Path -LiteralPath $BundlePath)) { throw 'Pass -BundlePath with the full path to the built Suitunes.vst3 folder.' }
}
$bundle = (Resolve-Path -LiteralPath $BundlePath).Path
if (-not (Test-Path -LiteralPath (Join-Path $bundle 'Contents/x86_64-win/Suitunes.vst3'))) { throw 'This is not the complete Windows x64 Suitunes VST3 bundle.' }
if (Get-Process -Name FL,FL64,Suitunes -ErrorAction SilentlyContinue) { throw 'Save your work and close FL Studio and Suitunes before installing.' }
$commonFiles = if ($env:CommonProgramW6432) { $env:CommonProgramW6432 } else { $env:CommonProgramFiles }
$destination = Join-Path $commonFiles 'VST3/2of3'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item -LiteralPath $bundle -Destination $destination -Recurse -Force
Write-Output "Installed Suitunes to $destination. Open FL Studio > Options > Manage plugins, verify and rescan previously verified plugins."
