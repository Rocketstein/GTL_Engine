@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%PacificEngine
set CONFIGURATION=GameRelease
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\%CONFIGURATION%
set GAME_RELEASE_DIR=%SOLUTION_DIR%GameBuild
set PYTHON_EXE=%SOLUTION_DIR%Scripts\python\python.exe

:: VS Developer 환경 로드 (msbuild PATH 등록)
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo Visual Studio를 찾을 수 없습니다.
    pause
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

echo ============================================
echo  Game Standalone Build Script
echo ============================================

:: 1. MSBuild로 GameRelease x64 빌드
echo.
echo [1/4] Building Game %CONFIGURATION% x64...
msbuild "%SOLUTION_DIR%PacificEngine.sln" /p:Configuration=%CONFIGURATION% /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    pause
    exit /b 1
)

:: 2. 셰이더 컴파일
echo.
echo [2/4] Pre-compiling Shaders...
if exist "%PYTHON_EXE%" (
    "%PYTHON_EXE%" "%SOLUTION_DIR%Scripts\CompileShaders.py"
) else (
    python "%SOLUTION_DIR%Scripts\CompileShaders.py"
)

:: 3. 기존 GameBuild 폴더 정리
echo.
echo [3/4] Preparing output directory...
if exist "%GAME_RELEASE_DIR%" rmdir /s /q "%GAME_RELEASE_DIR%"
mkdir "%GAME_RELEASE_DIR%"

:: 4. 파일 복사 (최적화된 패키징)
echo.
echo [4/4] Copying files...

:: 실행 파일
copy "%BUILD_OUTPUT%\PacificEngine.exe" "%GAME_RELEASE_DIR%\" >nul

:: DLL 파일
copy "%PROJECT_DIR%\ThirdParty\FMod\fmod.dll" "%GAME_RELEASE_DIR%\" >nul
copy "%PROJECT_DIR%\ThirdParty\FBX_SDK\lib\release\libfbxsdk.dll" "%GAME_RELEASE_DIR%\" >nul

:: Asset (Content만 포함, Editor 제외 시도 - 여기서는 단순화)
mkdir "%GAME_RELEASE_DIR%\Asset"
xcopy "%PROJECT_DIR%\Asset\Content" "%GAME_RELEASE_DIR%\Asset\Content\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Asset\Scripts" "%GAME_RELEASE_DIR%\Asset\Scripts\" /e /i /q >nul

:: Settings (Game.ini 포함)
xcopy "%PROJECT_DIR%\Settings" "%GAME_RELEASE_DIR%\Settings\" /e /i /q >nul

:: Compiled Shaders (소스 HLSL 대신 CSO 파일만 포함하는 것이 이상적이나, 
:: 현재 엔진 로더 호환성을 위해 일단 둘 다 혹은 소스만 포함할 수도 있음. 
:: 제안에 따라 컴파일된 캐시 폴더 포함)
mkdir "%GAME_RELEASE_DIR%\Saved"
xcopy "%SOLUTION_DIR%Saved\ShaderCache" "%GAME_RELEASE_DIR%\Saved\ShaderCache\" /e /i /q >nul

:: 엔진이 HLSL 소스를 요구할 경우를 대비해 Shaders 폴더도 복사하되, 
:: 향후 엔진 로직 수정 시 제거 가능
xcopy "%PROJECT_DIR%\Shaders" "%GAME_RELEASE_DIR%\Shaders\" /e /i /q >nul

echo.
echo ============================================
echo  Game Build complete: %GAME_RELEASE_DIR%
echo ============================================
echo.
echo  GameBuild/
echo    PacificEngine.exe
echo    Asset/
echo      Content/ (Scenes, Models, Textures)
echo      Scripts/ (Lua)
echo    Settings/ (Game.ini, Resource.ini)
echo    Saved/
echo      ShaderCache/ (Compiled Shaders)
echo    Shaders/ (Optional)
echo.
pause
