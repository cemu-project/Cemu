<#
Automated build script for Windows (PowerShell)

Prerequisites:
 - Visual Studio 2022 with "Desktop development with C++" and "C++ CMake tools for Windows"
 - Windows 10/11 SDK

Usage:
  .\scripts\build-windows.ps1 [-Configuration Release]
#>

param(
  [string]$Configuration = "Release"
)

Set-StrictMode -Version Latest

Write-Host "Updating submodules..."
git submodule update --init --recursive

if (Test-Path dependencies\vcpkg) {
  Push-Location dependencies\vcpkg
  Write-Host "Bootstrapping vcpkg..."
  .\bootstrap-vcpkg.bat
  Pop-Location
}

$toolchain = ""
if (Test-Path dependencies\vcpkg\scripts\buildsystems\vcpkg.cmake) { $toolchain = "-DCMAKE_TOOLCHAIN_FILE=$PWD\\dependencies\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows" }

Write-Host "Configuring CMake (x64, $Configuration)..."
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=$Configuration $toolchain -G "Visual Studio 17 2022"

Write-Host "Building..."
cmake --build build --config $Configuration -- /m

Write-Host "Build finished. Artifacts may be in bin/ or build/bin/."
