param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$projectDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildPath = (Resolve-Path (Join-Path $projectDir $BuildDir)).Path
$distDir = Join-Path $projectDir 'dist'
New-Item -ItemType Directory -Force $distDir | Out-Null
$stage = Join-Path $distDir ('stage-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $stage 'Suitunes'
$binary = Join-Path $stage 'Windows-x64'
New-Item -ItemType Directory -Force $source,$binary,(Join-Path $source 'vendor/signalsmith-stretch'),(Join-Path $source 'evidence'),(Join-Path $binary 'licenses') | Out-Null
foreach ($name in @('CMakeLists.txt','README.md','VALIDATION.md','DEPENDENCIES.md','AGENTS.md','.gitignore','Source','Tests','docs','tools','.github','Presets','Assets','preview')) {
    Copy-Item -LiteralPath (Join-Path $projectDir $name) -Destination $source -Recurse
}
foreach ($name in @('CMakeLists.txt','signalsmith-stretch.h','LICENSE.txt','README.md','include')) {
    Copy-Item -LiteralPath (Join-Path $projectDir "vendor/signalsmith-stretch/$name") -Destination (Join-Path $source 'vendor/signalsmith-stretch') -Recurse
}
foreach ($name in @('build.log','tests.log','expanded-tests.log','pluginval.log','pluginval-error.log','pluginval-10.log','pluginval-10-error.log','benchmark-sweep.log','benchmark-final.log')) {
    Copy-Item -LiteralPath (Join-Path $projectDir $name) -Destination (Join-Path $source 'evidence')
}
Copy-Item -LiteralPath (Join-Path $buildPath 'Suitunes_artefacts/Release/VST3/Suitunes.vst3') -Destination $binary -Recurse
Copy-Item -LiteralPath (Join-Path $buildPath 'Suitunes_artefacts/Release/Standalone/Suitunes.exe') -Destination $binary
Copy-Item -LiteralPath (Join-Path $projectDir 'README.md'),(Join-Path $projectDir 'VALIDATION.md') -Destination $binary
Copy-Item -LiteralPath (Join-Path $projectDir 'preview'),(Join-Path $projectDir 'docs') -Destination $binary -Recurse
Copy-Item -LiteralPath (Join-Path $projectDir 'tools/Install-Windows.ps1') -Destination $binary
Copy-Item -LiteralPath (Join-Path $projectDir 'vendor/signalsmith-stretch/LICENSE.txt') -Destination (Join-Path $binary 'licenses/Signalsmith-Stretch-MIT.txt')
Copy-Item -LiteralPath (Join-Path $buildPath '_deps/signalsmith-linear-src/LICENSE.txt') -Destination (Join-Path $binary 'licenses/Signalsmith-Linear-MIT.txt')
# Read the configured JUCE source path, including an optional local override.
$cache = Get-Content (Join-Path $buildPath 'CMakeCache.txt')
$juceLine = $cache | Where-Object { $_ -match '^JUCE_SOURCE_DIR:STATIC=' } | Select-Object -First 1
if (-not $juceLine) { throw 'JUCE source directory missing in CMake cache' }
$jucePath = $juceLine.Substring($juceLine.IndexOf('=') + 1)
Copy-Item -LiteralPath (Join-Path $jucePath 'LICENSE.md') -Destination (Join-Path $binary 'licenses/JUCE-LICENSE.md')
Copy-Item -LiteralPath (Join-Path $jucePath 'modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt') -Destination (Join-Path $binary 'licenses/VST3-SDK-LICENSE.txt')
Compress-Archive -Path (Join-Path $source '*') -DestinationPath (Join-Path $distDir 'Suitunes-0.2.0-Source.zip') -Force
Compress-Archive -Path (Join-Path $binary '*') -DestinationPath (Join-Path $distDir 'Suitunes-0.2.0-Windows-x64.zip') -Force
Get-FileHash -Algorithm SHA256 (Join-Path $distDir 'Suitunes-0.2.0-*.zip') | ForEach-Object { "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))" } | Set-Content -LiteralPath (Join-Path $distDir 'SHA256SUMS-0.2.0.txt') -Encoding ascii
Get-FileHash -Algorithm SHA256 (Join-Path $distDir '*.zip') | Format-Table Hash,Path -AutoSize
