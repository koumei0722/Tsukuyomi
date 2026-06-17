@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% neq 0 (
    echo [Error] Failed to load Visual Studio environment.
    exit /b %ERRORLEVEL%
)
echo [Build] Starting CMake build...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [Error] Build failed.
    exit /b %ERRORLEVEL%
)
echo [Build] Build succeeded!
