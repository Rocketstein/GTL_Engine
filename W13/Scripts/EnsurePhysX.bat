@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%i in ("%~dp0..") do set "ROOT_DIR=%%~fi"
set "PHYSX_BIN_DIR=%ROOT_DIR%\KraftonEngine\ThirdParty\PhysX\physx\bin"
set "REQUESTED_CONFIG=%~1"
set "PHYSX_CONFIG=release"

if /I "%REQUESTED_CONFIG%"=="Debug" set "PHYSX_CONFIG=debug"

set "PHYSX_ROOTS=win.x86_64.vc143.md win.x86_64.vc142.md"
set "PHYSX_LIBS=PhysX_64.lib PhysXCommon_64.lib PhysXFoundation_64.lib PhysXCooking_64.lib PhysXExtensions_static_64.lib PhysXPvdSDK_static_64.lib"

set "PHYSX_READY=0"
for %%r in (%PHYSX_ROOTS%) do (
    set "CANDIDATE=%PHYSX_BIN_DIR%\%%r\%PHYSX_CONFIG%"
    if exist "!CANDIDATE!\PhysX_64.lib" (
        set "MISSING=0"
        for %%l in (%PHYSX_LIBS%) do (
            if not exist "!CANDIDATE!\%%l" set "MISSING=1"
        )
        if "!MISSING!"=="0" set "PHYSX_READY=1"
    )
)

if "%PHYSX_READY%"=="1" (
    echo PhysX %PHYSX_CONFIG% libs found.
    exit /b 0
)

echo PhysX %PHYSX_CONFIG% libs were not found. Building PhysX...
call "%~dp0BuildPhysX.bat" --no-pause
if errorlevel 1 exit /b %ERRORLEVEL%

set "PHYSX_READY=0"
for %%r in (%PHYSX_ROOTS%) do (
    set "CANDIDATE=%PHYSX_BIN_DIR%\%%r\%PHYSX_CONFIG%"
    if exist "!CANDIDATE!\PhysX_64.lib" (
        set "MISSING=0"
        for %%l in (%PHYSX_LIBS%) do (
            if not exist "!CANDIDATE!\%%l" set "MISSING=1"
        )
        if "!MISSING!"=="0" set "PHYSX_READY=1"
    )
)

if "%PHYSX_READY%"=="1" (
    echo PhysX %PHYSX_CONFIG% libs are ready.
    exit /b 0
)

echo PhysX build finished, but required %PHYSX_CONFIG% libs are still missing.
for %%r in (%PHYSX_ROOTS%) do (
    echo Checked: %PHYSX_BIN_DIR%\%%r\%PHYSX_CONFIG%
)
exit /b 1
