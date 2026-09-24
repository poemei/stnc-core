@echo off
setlocal

if /I "%~1"=="clean" (
    if exist build (
        echo Removing build directory...
        rmdir /S /Q build

        if exist build (
            echo.
            echo CLEAN FAILED
            exit /b 1
        )
    )

    echo.
    echo CLEAN SUCCESSFUL
    exit /b 0
)

if not "%~1"=="" (
    echo Usage:
    echo   build
    echo   build clean
    exit /b 1
)

if not exist build mkdir build

cl /nologo /W4 /TC /Iincludes ^
    src\main.c ^
    src\stnc_command.c ^
    src\stnc_core.c ^
    src\stnc_log.c ^
    src\stnc_config.c ^
    src\stnc_network.c ^
    src\stnc_stnc.c ^
    src\stnc_stnp.c ^
    platforms\windows\platform_windows.c ^
    /Fe:build\stnc-core.exe ^
    /link ws2_32.lib

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
cl /nologo /W4 /TC /Iincludes tests\test_stnc_stnp.c src\stnc_stnp.c /Fe:build\test-stnc-stnp.exe
if errorlevel 1 (
    echo.
    echo TEST BUILD FAILED
    exit /b 1
)

build\test-stnc-stnp.exe
if errorlevel 1 (
    echo.
    echo TEST FAILED
    exit /b 1
)

echo build\stnc-core.exe

endlocal
