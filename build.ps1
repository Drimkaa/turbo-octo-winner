[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$QtDir,

    [Parameter(Mandatory = $true)]
    [string]$LibreCadSource,

    [string]$InstallDir
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$qmake = Join-Path $QtDir 'bin\qmake.exe'
$pluginHeader = Join-Path $LibreCadSource 'librecad\src\plugins\document_interface.h'
$interfaceHeader = Join-Path $LibreCadSource 'librecad\src\plugins\qc_plugininterface.h'

if (-not (Test-Path -LiteralPath $qmake -PathType Leaf)) {
    throw "qmake.exe was not found at $qmake"
}
if (-not (Test-Path -LiteralPath $pluginHeader -PathType Leaf) -or
    -not (Test-Path -LiteralPath $interfaceHeader -PathType Leaf)) {
    throw 'LibreCadSource must point to a LibreCAD 2.2.1 source checkout.'
}

$qtVersion = (& $qmake -query QT_VERSION).Trim()
$qtSpec = (& $qmake -query QMAKE_XSPEC).Trim()
if ($qtVersion -ne '5.15.2') {
    throw "Qt 5.15.2 is required to match the installed LibreCAD. Found $qtVersion."
}
if ($qtSpec -notmatch 'win32-msvc') {
    throw "The installed LibreCAD uses the MSVC ABI. Found qmake spec '$qtSpec'."
}

$nmake = Get-Command nmake.exe -ErrorAction SilentlyContinue
if (-not $nmake) {
    throw 'nmake.exe is not available. Run this script from an x64 MSVC Native Tools command prompt.'
}

$testBuild = Join-Path $projectRoot 'build\tests'
$pluginBuild = Join-Path $projectRoot 'build\qmake'
New-Item -ItemType Directory -Force -Path $testBuild, $pluginBuild | Out-Null

Push-Location $testBuild
try {
    & $qmake (Join-Path $projectRoot 'tests\tests.pro') 'CONFIG+=release'
    if ($LASTEXITCODE -ne 0) { throw 'qmake failed for geometry tests.' }
    & $nmake.Source
    if ($LASTEXITCODE -ne 0) { throw 'Geometry test build failed.' }
    & (Join-Path $testBuild 'release\leather_stitch_geometry_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Geometry tests failed.' }
}
finally {
    Pop-Location
}

Push-Location $pluginBuild
try {
    $sourceArgument = 'LIBRECAD_SOURCE=' + (Resolve-Path -LiteralPath $LibreCadSource).Path.Replace('\', '/')
    & $qmake (Join-Path $projectRoot 'LeatherStitchArray.pro') $sourceArgument 'CONFIG+=release'
    if ($LASTEXITCODE -ne 0) { throw 'qmake failed for the plugin.' }
    & $nmake.Source
    if ($LASTEXITCODE -ne 0) { throw 'Plugin build failed.' }
}
finally {
    Pop-Location
}

$dll = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build\plugin') `
    -Filter 'leatherstitcharray*.dll' -File | Select-Object -First 1
if (-not $dll) {
    throw 'The build completed without producing a plugin DLL.'
}

Write-Host "Built and tested: $($dll.FullName)"
if ($InstallDir) {
    if (-not (Test-Path -LiteralPath $InstallDir -PathType Container)) {
        throw "InstallDir does not exist: $InstallDir"
    }
    Copy-Item -LiteralPath $dll.FullName -Destination $InstallDir -Force
    Write-Host "Installed to: $InstallDir"
}
