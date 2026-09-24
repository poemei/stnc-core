@echo off
setlocal

if not exist build mkdir build

cl /nologo /W4 /TC /Iincludes ^
    src\main.c ^
    src\stnc_core.c ^
    src\stnc_log.c ^
    src\stnc_config.c ^
    src\stnc_network.c ^
    src\stnc_stnc.c ^
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
echo build\stnc-core.exe

endlocal
