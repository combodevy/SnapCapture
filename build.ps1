# CaptureTool C++ Rebuild - Native Compilation Script
# Natively compiles main.cpp to a super-optimized static binary without runtime dll dependencies.

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  CaptureTool C++ Rebuild Compiler" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Check if g++ is available
if (!(Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Error "Error: g++.exe not found in PATH! Make sure the compiler toolchain is active."
    exit 1
}

Write-Host "Compiling main.cpp..." -ForegroundColor Yellow

# Run g++ compiler with high optimization (-O3) and subsystem windows (-mwindows to hide command prompt)
# Static link (-static) to bundle everything in a single standalone .exe
g++ -std=c++11 -O3 -mwindows -static main.cpp -lgdi32 -lgdiplus -lshlwapi -luser32 -lshell32 -lole32 -lcomdlg32 -o CaptureTool.exe

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "  Success! CaptureTool.exe compiled!   " -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host ""
    
    # Keep compiled .exe in the project folder to prevent desktop clutter
    # Copy-Item -Path "CaptureTool.exe" -Destination "C:\Users\22636\Desktop\CaptureTool.exe" -Force
    Write-Host "Executable kept inside capturetool folder. [OK]" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Error "Compilation failed! Check the compiler errors above."
}
