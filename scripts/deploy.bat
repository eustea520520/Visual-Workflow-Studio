@echo off
REM Deploy Visual Workflow Studio for distribution.
REM Copies Qt DLLs, plugins, and resources next to the executable.
REM Usage: scripts\deploy.bat [build_dir] [output_dir]
REM   build_dir  = path to the build directory containing the .exe
REM   output_dir = where to place the deployed package (default: dist\VisualWorkflowStudio)

setlocal enabledelayedexpansion

set BUILD_DIR=%1
set OUTPUT_DIR=%2

if "%BUILD_DIR%"=="" set BUILD_DIR=build\Desktop_Qt_6_5_3_MinGW_64_bit-Debug
if "%OUTPUT_DIR%"=="" set OUTPUT_DIR=dist\VisualWorkflowStudio

set QT_BIN=C:\Qt\6.5.3\mingw_64\bin
set WINDEPLOYQT=%QT_BIN%\windeployqt.exe

if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt not found at %WINDEPLOYQT%
    exit /b 1
)

echo === Deploying Visual Workflow Studio ===
echo Build:  %BUILD_DIR%
echo Output: %OUTPUT_DIR%

if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"

echo.
echo --- Copying executable ---
copy "%BUILD_DIR%\visual-workflow-studio.exe" "%OUTPUT_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Could not find visual-workflow-studio.exe in %BUILD_DIR%
    exit /b 1
)

echo.
echo --- Running windeployqt ---
"%WINDEPLOYQT%" "%OUTPUT_DIR%\visual-workflow-studio.exe" --no-translation --no-compiler-runtime --no-system-d3d-compiler

echo.
echo --- Copying additional Qt DLLs ---
set MINGW_BIN=C:\Qt\Tools\mingw1120_64\bin
if exist "%MINGW_BIN%\libstdc++-6.dll" (
    copy "%MINGW_BIN%\libstdc++-6.dll" "%OUTPUT_DIR%\" >nul
    copy "%MINGW_BIN%\libgcc_s_seh-1.dll" "%OUTPUT_DIR%\" >nul
    copy "%MINGW_BIN%\libwinpthread-1.dll" "%OUTPUT_DIR%\" >nul
)

echo.
echo --- Copying project resources ---
if exist "python" xcopy /E /I /Y "python" "%OUTPUT_DIR%\python" >nul

echo.
echo === Deployment complete ===
echo Package is ready at: %OUTPUT_DIR%
echo.
echo To run: %OUTPUT_DIR%\visual-workflow-studio.exe
