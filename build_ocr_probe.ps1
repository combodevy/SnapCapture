#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$OutputName = 'ocr_probe.exe'
)

$ErrorActionPreference = 'Stop'

Write-Host ''
Write-Host '==========================================' -ForegroundColor Cyan
Write-Host '  SnapCapture OCR Probe Build Script' -ForegroundColor Cyan
Write-Host '==========================================' -ForegroundColor Cyan
Write-Host ''

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
$compiler = if ($gxx) { $gxx.Source } else { $null }

if (-not $compiler) {
    $fallback = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe'
    if (Test-Path -LiteralPath $fallback) {
        $compiler = $fallback
        Write-Host ("[warn ] g++ not in PATH, using fallback: {0}" -f $compiler) -ForegroundColor Yellow
    }
}

if (-not $compiler) {
    Write-Host '[fail ] g++.exe was not found in PATH or WinGet fallback location.' -ForegroundColor Red
    exit 1
}

Write-Host ("[build] Compiler: {0}" -f $compiler) -ForegroundColor Cyan

Push-Location $PSScriptRoot
try {
    if (-not (Test-Path -LiteralPath 'ocr_probe.cpp')) {
        Write-Host '[fail ] ocr_probe.cpp not found.' -ForegroundColor Red
        exit 1
    }

    $args = @(
        '-std=c++11', '-O2', '-municode',
        'ocr_probe.cpp',
        '-o', $OutputName,
        '-lole32', '-lruntimeobject'
    )

    Write-Host '[build] Compiling OCR probe...' -ForegroundColor Cyan
    & $compiler @args

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[fail ] Compilation failed with exit code $LASTEXITCODE." -ForegroundColor Red
        exit $LASTEXITCODE
    }

    $exe = Get-Item -LiteralPath $OutputName
    Write-Host ("[ ok  ] Build succeeded: {0} ({1:N2} KB)" -f $exe.FullName, ($exe.Length / 1KB)) -ForegroundColor Green
}
finally {
    Pop-Location
}
