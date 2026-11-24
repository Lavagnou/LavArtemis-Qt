@echo off
setlocal enableDelayedExpansion

rem Builds the testbench client (CONFIG+=lav_test) into build\test-x64 and
rem deploys it to build\test-x64\deploy\LavArtemisTest.exe. Separate from the
rem production build folders (build-x64-release, deploy-x64-release), which it
rem never touches. Run from the root of the repo (any shell).

set SOURCE_ROOT=%~dp0..
pushd %SOURCE_ROOT%
set SOURCE_ROOT=%cd%
popd

set QT_BIN=C:\Qt\6.11.1\msvc2022_64\bin
set BUILD_FOLDER=%SOURCE_ROOT%\build\test-x64
set DEPLOY_FOLDER=%BUILD_FOLDER%\deploy
set JOBS=2
if defined LAV_TEST_JOBS set JOBS=%LAV_TEST_JOBS%

set VSWHERE="%SOURCE_ROOT%\scripts\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do (
    call "%%i\VC\Auxiliary\Build\vcvarsall.bat" x64
)
if !ERRORLEVEL! NEQ 0 goto Error

set PATH=%QT_BIN%;%PATH%

if not exist "%BUILD_FOLDER%" mkdir "%BUILD_FOLDER%"
if exist "%DEPLOY_FOLDER%" rmdir /s /q "%DEPLOY_FOLDER%"
mkdir "%DEPLOY_FOLDER%"

echo Configuring the test build
pushd "%BUILD_FOLDER%"
qmake.exe "%SOURCE_ROOT%\lavartemis.pro" CONFIG+=lav_test
if !ERRORLEVEL! NEQ 0 goto Error

echo Compiling LavArtemisTest
"%SOURCE_ROOT%\scripts\jom.exe" -j %JOBS% release
if !ERRORLEVEL! NEQ 0 goto Error
popd

echo Copying DLL dependencies
copy "%SOURCE_ROOT%\libs\windows\lib\x64\*.dll" "%DEPLOY_FOLDER%" >nul
if !ERRORLEVEL! NEQ 0 goto Error
copy "%BUILD_FOLDER%\AntiHooking\release\AntiHooking.dll" "%DEPLOY_FOLDER%" >nul
if !ERRORLEVEL! NEQ 0 goto Error
copy "%SOURCE_ROOT%\app\SDL_GameControllerDB\gamecontrollerdb.txt" "%DEPLOY_FOLDER%" >nul
if !ERRORLEVEL! NEQ 0 goto Error

set WINDEPLOYQT_ARGS=--no-system-d3d-compiler --no-system-dxc-compiler --skip-plugin-types qmltooling,generic --no-ffmpeg
set WINDEPLOYQT_ARGS=!WINDEPLOYQT_ARGS! --no-quickcontrols2fusion --no-quickcontrols2imagine --no-quickcontrols2universal
set WINDEPLOYQT_ARGS=!WINDEPLOYQT_ARGS! --no-quickcontrols2fusionstyleimpl --no-quickcontrols2imaginestyleimpl --no-quickcontrols2universalstyleimpl --no-quickcontrols2windowsstyleimpl --no-quickcontrols2fluentwinui3styleimpl

echo Deploying Qt dependencies
windeployqt.exe --dir "%DEPLOY_FOLDER%" --release --qmldir "%SOURCE_ROOT%\app\gui" --no-opengl-sw --no-compiler-runtime --no-sql %WINDEPLOYQT_ARGS% "%BUILD_FOLDER%\app\release\LavArtemisTest.exe"
if !ERRORLEVEL! NEQ 0 goto Error

rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\Controls\Fusion" 2>nul
rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\Controls\Imagine" 2>nul
rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\Controls\Universal" 2>nul
rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\Controls\Windows" 2>nul
rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\Controls\FluentWinUI3" 2>nul
rmdir /s /q "%DEPLOY_FOLDER%\qml\QtQuick\NativeStyle" 2>nul

copy "%BUILD_FOLDER%\app\release\LavArtemisTest.exe" "%DEPLOY_FOLDER%" >nul
if !ERRORLEVEL! NEQ 0 goto Error

rem VC runtime next to the exe, so the folder runs on its own
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -find VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT`) do set VC_REDIST_DLL_PATH=%%i
copy "%VC_REDIST_DLL_PATH%\*.dll" "%DEPLOY_FOLDER%" >nul

echo Test build ready: %DEPLOY_FOLDER%\LavArtemisTest.exe
exit /b 0

:Error
echo Test build failed!
exit /b !ERRORLEVEL!
