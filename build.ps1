<#
.SYNOPSIS
    Build and (optionally) launch openFrameworks sketches under apps/myApps from the command line,
    without opening Visual Studio.

.PARAMETER Project
    Folder name of the sketch under apps/myApps, e.g. "2025_11_26_musicclip".
    Omit to list available projects.

.PARAMETER Configuration
    Debug or Release. Default: Debug.

.PARAMETER Platform
    Build platform. Default: x64 (this is the 64-bit oF distribution; ARM64/ARM64EC also exist
    in the .sln but are untested here).

.PARAMETER Target
    MSBuild target: build (default), rebuild, or clean.

.PARAMETER Run
    Launch the built executable after a successful build. Note: these sketches typically open an
    audio stream and an OpenGL window, and several start video/audio recording immediately in
    setup() — running one has real side effects (mic/loopback capture, files written under
    bin/data). The process is started detached; use Stop-Process to kill it.

.EXAMPLE
    .\build.ps1 -Project 2025_11_26_musicclip
    .\build.ps1 -Project 2025_11_26_musicclip -Configuration Release -Run
    .\build.ps1 -Project Template_1_reactive_bounce -Target clean
    .\build.ps1                      # lists available projects
#>
param(
    [string]$Project,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('x64', 'ARM64', 'ARM64EC')]
    [string]$Platform = 'x64',
    [ValidateSet('build', 'rebuild', 'clean')]
    [string]$Target = 'build',
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# Accept a path-like -Project (e.g. tab-completed ".\Template_1_reactive_bounce\")
# and normalize it to the bare folder name the rest of the script expects.
if ($Project) {
    $Project = Split-Path -Leaf ($Project.TrimEnd('\', '/'))
}

function Get-Projects {
    Get-ChildItem -Path $repoRoot -Directory | Where-Object {
        Get-ChildItem -Path $_.FullName -Filter '*.sln' -File -ErrorAction SilentlyContinue |
            Where-Object { $_.BaseName -eq (Split-Path $_.Directory -Leaf) }
    } | Select-Object -ExpandProperty Name
}

if (-not $Project) {
    Write-Host "Usage: .\build.ps1 -Project <name> [-Configuration Debug|Release] [-Run]"
    Write-Host "`nAvailable projects:"
    Get-Projects | Sort-Object | ForEach-Object { Write-Host "  $_" }
    exit 0
}

$projectDir = Join-Path $repoRoot $Project
if (-not (Test-Path $projectDir)) {
    Write-Error "No project folder '$Project' under $repoRoot"
    exit 1
}

# Project folders sometimes contain a stray .sln left over from copy-pasting a template
# (e.g. Template_1_reactive_bounce.sln inside a differently-named project). Prefer the
# .sln whose name matches the folder; fall back to the newest .sln if that's not found.
$sln = Get-ChildItem -Path $projectDir -Filter '*.sln' -File |
    Where-Object { $_.BaseName -eq $Project } |
    Select-Object -First 1
if (-not $sln) {
    $sln = Get-ChildItem -Path $projectDir -Filter '*.sln' -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($sln) {
        Write-Warning "No .sln named '$Project.sln'; using '$($sln.Name)' instead (stray/renamed project file?)."
    }
}
if (-not $sln) {
    Write-Error "No .sln file found in $projectDir"
    exit 1
}

# Locate MSBuild via vswhere (works across VS versions/editions without hardcoding a path).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found at '$vswhere'. Is Visual Studio installed?"
    exit 1
}
$vsInstall = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsInstall) {
    Write-Error "vswhere found no Visual Studio install with MSBuild."
    exit 1
}
$msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path $msbuild)) {
    $msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin\MSBuild.exe'
}
if (-not (Test-Path $msbuild)) {
    Write-Error "MSBuild.exe not found under '$vsInstall'."
    exit 1
}

Write-Host "Building $($sln.Name) [$Configuration|$Platform] target=$Target ..."
& $msbuild $sln.FullName /t:$Target /p:Configuration=$Configuration /p:Platform=$Platform /nologo /v:minimal /m
$buildExitCode = $LASTEXITCODE

if ($buildExitCode -ne 0) {
    Write-Error "Build failed (exit code $buildExitCode)."
    exit $buildExitCode
}
Write-Host "Build succeeded."

if ($Target -eq 'clean') {
    exit 0
}

if ($Run) {
    $suffix = if ($Configuration -eq 'Debug') { '_debug' } else { '' }
    $exePath = Join-Path $projectDir "bin\$Project$suffix.exe"
    if (-not (Test-Path $exePath)) {
        Write-Error "Built, but couldn't find expected executable at '$exePath'."
        exit 1
    }
    Write-Host "Launching $exePath (detached) ..."
    Start-Process -FilePath $exePath -WorkingDirectory (Join-Path $projectDir 'bin')
}
