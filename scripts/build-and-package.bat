@echo off
REM ===========================================================================
REM  FanTune Build + Package — Windows (MSVC + NSIS)
REM  Builds Release config, generates presets, creates NSIS installer.
REM ===========================================================================
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_DIR%\build
set PRESET_DIR=%PROJECT_DIR%\presets

echo ===== FanTune Build + Package [Windows] =====

REM ── 1. Generate factory presets ──
echo.
echo [1/4] Generating factory presets...
cd /d "%PROJECT_DIR%"
python tools\generate_factory_presets.py --output-dir "%PRESET_DIR%" --verbose
if %ERRORLEVEL% neq 0 (
    echo ERROR: Preset generation failed
    exit /b 1
)

REM ── 2. Configure CMake ──
echo.
echo [2/4] Configuring CMake...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"
cmake .. -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

REM ── 3. Build all targets ──
echo.
echo [3/4] Building all targets...
cmake --build . --config Release --target FanTune_VST3 FanTune_CLAP FanTune_Standalone
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

REM ── 4. Create installer with CPack ──
echo.
echo [4/4] Creating NSIS installer...
cpack -G NSIS -C Release
if %ERRORLEVEL% neq 0 (
    REM CPack/makensis may fail if branding bitmaps aren't accessible
    REM (requires NSIS File command). Copy them alongside the NSI script
    REM and re-run makensis manually.
    echo CPack phase 1 done ^(NSI + install tree^). Re-running makensis with bitmaps...
    copy /Y "%PROJECT_DIR%\resources\installer\header.bmp"   "%BUILD_DIR%\_CPack_Packages\win64\NSIS\" >nul
    copy /Y "%PROJECT_DIR%\resources\installer\welcome.bmp"  "%BUILD_DIR%\_CPack_Packages\win64\NSIS\" >nul
    "%NSIS_MAKENSIS%" "%BUILD_DIR%\_CPack_Packages\win64\NSIS\project.nsi"
    if %ERRORLEVEL% neq 0 (
        echo ERROR: makensis failed even with bitmap files in place.
        exit /b 1
    )
) else (
    echo CPack/NSIS succeeded directly.
    goto :zip_fallback_check
)

echo Installer created: "%BUILD_DIR%\_CPack_Packages\win64\NSIS\fantune-1.0.0-windows-amd64.exe"

:zip_fallback_check
REM Check if the installer was created
if not exist "%BUILD_DIR%\fantune-1.0.0-windows-amd64.exe" (
    if exist "%BUILD_DIR%\_CPack_Packages\win64\NSIS\fantune-1.0.0-windows-amd64.exe" (
        copy /Y "%BUILD_DIR%\_CPack_Packages\win64\NSIS\fantune-1.0.0-windows-amd64.exe" "%BUILD_DIR%\"
    ) else (
        echo WARNING: NSIS installer not found, trying ZIP fallback...
        cpack -G ZIP -C Release
    )
)
if %ERRORLEVEL% neq 0 (
    echo WARNING: CPack/NSIS failed — see above for details
    echo Checking for alternate generators...
    cpack -G ZIP -C Release
    if %ERRORLEVEL% neq 0 (
        echo ERROR: All packaging methods failed
        exit /b 1
    )
    echo Created ZIP package as fallback
)

echo.
echo ===== BUILD + PACKAGE COMPLETE =====
echo Installer package(s) in: %BUILD_DIR%

REM ── 5. Quick local install (optional, admin) ──
echo.
echo To install locally, run as Administrator:
echo   powershell -ExecutionPolicy Bypass "%SCRIPT_DIR%\install-fantune.ps1"

endlocal
