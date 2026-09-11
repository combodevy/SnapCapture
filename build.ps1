#Requires -Version 5.1
<#
    SnapCapture build script.
    Compiles main.cpp into a single, statically linked Windows executable.
#>

[CmdletBinding()]
param(
    [string]$OutputName = 'CaptureTool.exe',

    [ValidateSet('x64', 'x86')]
    [string]$Architecture = 'x64',

    [switch]$Strip,

    [switch]$Run
)

$ErrorActionPreference = 'Stop'

function Write-Step { param([string]$Message) Write-Host "[build] $Message" -ForegroundColor Cyan }
function Write-Ok    { param([string]$Message) Write-Host "[ ok  ] $Message" -ForegroundColor Green }
function Write-Err   { param([string]$Message) Write-Host "[fail ] $Message" -ForegroundColor Red }

Write-Host ''
Write-Host '==========================================' -ForegroundColor Cyan
Write-Host '  SnapCapture Build Script' -ForegroundColor Cyan
Write-Host '==========================================' -ForegroundColor Cyan
Write-Host ''

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) {
    Write-Err 'g++.exe was not found in PATH.'
    Write-Host '       Install MinGW-w64 and make sure its bin directory is on PATH.' -ForegroundColor Yellow
    exit 1
}

Write-Step ("Compiler : {0}" -f $gxx.Source)
Write-Step ("Version  : {0}" -f $(& g++ --version | Select-Object -First 1))
Write-Step ("Target   : {0}" -f $Architecture)

$projectRoot = $PSScriptRoot
Push-Location $projectRoot

try {
    $source = Join-Path $projectRoot 'main.cpp'
    if (-not (Test-Path -LiteralPath $source)) {
        Write-Err "Source file not found: $source"
        exit 1
    }

    $compileArgs = @('-std=c++11', '-O3', '-mwindows', '-static')
    if ($Architecture -eq 'x86') { $compileArgs += '-m32' }
    if ($Strip)                  { $compileArgs += '-s'  }

    $compileArgs += $source
    $compileArgs += @('-o', $OutputName)
    $compileArgs += @(
        '-lgdi32', '-lgdiplus', '-lshlwapi', '-luser32',
        '-lshell32', '-lole32', '-lcomdlg32', '-ldwmapi', '-limm32'
    )

    Write-Step "Compiling $OutputName ..."
    & g++ @compileArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Err "Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    $exe = Get-Item -LiteralPath $OutputName
    Write-Ok ("Build succeeded: {0} ({1:N2} MB)" -f $exe.Name, ($exe.Length / 1MB))
    Write-Step ("Output: {0}" -f $exe.FullName)

    if ($Run) {
        Write-Step 'Launching executable...'
        Start-Process -FilePath $exe.FullName
    }
}
finally {
    Pop-Location
}
