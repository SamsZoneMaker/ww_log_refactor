@echo off
REM Build test script for Windows
REM This script helps test the new log system when make is not available

echo ========================================
echo Log System Build Test
echo ========================================
echo.

REM Check if we're in the right directory
if not exist "log_config.json" (
    echo Error: log_config.json not found!
    echo Please run this script from the project root directory.
    pause
    exit /b 1
)

REM Step 1: Generate file ID mappings
echo Step 1: Generating file ID mappings...
if not exist "build" mkdir build
if not exist "include" mkdir include

python tools\gen_file_ids.py log_config.json --makefile > build\file_ids.mk
if errorlevel 1 (
    echo Error: Failed to generate Makefile mappings
    pause
    exit /b 1
)

python tools\gen_file_ids.py log_config.json --header > include\auto_file_ids.h
if errorlevel 1 (
    echo Error: Failed to generate C header
    pause
    exit /b 1
)

echo   [OK] File ID mappings generated
echo.

REM Step 2: Show generated files
echo Step 2: Verifying generated files...
if exist "build\file_ids.mk" (
    echo   [OK] build\file_ids.mk exists
) else (
    echo   [FAIL] build\file_ids.mk not found
)

if exist "include\auto_file_ids.h" (
    echo   [OK] include\auto_file_ids.h exists
) else (
    echo   [FAIL] include\auto_file_ids.h not found
)
echo.

REM Step 3: Show file count
echo Step 3: File ID statistics...
findstr /C:"FILE_ID_" build\file_ids.mk | find /C "=" > nul
if errorlevel 1 (
    echo   No file IDs found
) else (
    for /f %%i in ('findstr /C:"FILE_ID_" build\file_ids.mk ^| find /C "="') do echo   Total files configured: %%i
)
echo.

REM Step 4: Instructions for make
echo Step 4: Next steps...
echo.
echo The file ID mappings have been generated successfully!
echo.
echo To compile the project, you need to run make in an MSYS2 terminal:
echo.
echo   1. Open MSYS2 MinGW 64-bit terminal
echo   2. Navigate to: cd /d/WorkSpace/coding/ww_log
echo   3. Configure mode in include/ww_log.h (uncomment one mode)
echo   4. Run: make clean
echo   5. Run: make
echo   6. Run: make run
echo.
echo Note: Log mode is now configured in include/ww_log.h, not via Makefile
echo Available modes: WW_LOG_MODE_STR, WW_LOG_MODE_ENCODE, WW_LOG_MODE_DISABLED
echo.

pause
