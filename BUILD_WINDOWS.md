This file documents how to build Cemu for Windows and how to use the included GitHub Actions workflow.

Local build (on Windows):

- Install prerequisites: Visual Studio 2022 with `C++ CMake tools for Windows` and Windows 10/11 SDK.
- Open an elevated PowerShell and run:

```powershell
git clone --recursive https://github.com/cemu-project/Cemu
cd Cemu
.\scripts\build-windows.ps1 -Configuration Release
```

This script bootstraps vcpkg (if present as a submodule), configures CMake for x64 using Visual Studio 2022, and builds Release.

CI build (GitHub Actions):

- A workflow file is available at `.github/workflows/windows-build.yml` and will run on `windows-latest` when pushed to `main` or triggered manually.
- The workflow checks out submodules, bootstraps vcpkg if present, configures CMake for x64 and builds with MSVC, then uploads `bin` and `build\bin` as an artifact.

Notes and limitations:
- This repository requires Windows SDK / Visual Studio for a full native build; building a fully-compatible Windows 10 executable in a Linux container without a Windows toolchain is not possible.
- If you prefer an automated hosted build, enable the workflow in GitHub by pushing these changes and trigger the workflow in Actions.
