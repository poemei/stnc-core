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
    src\stnc_directory.c ^
    src\stnc_http.c ^
    src\stnc_network.c ^
    src\stnc_peers.c ^
    src\stnc_stnc.c ^
    src\stnc_stnp.c ^
    platforms\windows\platform_windows.c ^
    /Fe:build\stnc-core.exe ^
    /link ws2_32.lib winhttp.lib

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

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

cl /nologo /W4 /TC /Iincludes tests\test_stnc_peers.c src\stnc_peers.c /Fe:build\test-stnc-peers.exe
if errorlevel 1 (
    echo.
    echo PEER TEST BUILD FAILED
    exit /b 1
)

build\test-stnc-peers.exe
if errorlevel 1 (
    echo.
    echo PEER TEST FAILED
    exit /b 1
)

cl /nologo /W4 /TC /Iincludes tests\test_stnc_directory.c src\stnc_directory.c src\stnc_peers.c /Fe:build\test-stnc-directory.exe
if errorlevel 1 (
    echo.
    echo DIRECTORY TEST BUILD FAILED
    exit /b 1
)

build\test-stnc-directory.exe
if errorlevel 1 (
    echo.
    echo DIRECTORY TEST FAILED
    exit /b 1
)

cl /nologo /W4 /TC /Iincludes tests\test_stnc_http.c src\stnc_http.c /Fe:build\test-stnc-http.exe
if errorlevel 1 (
    echo.
    echo HTTP TEST BUILD FAILED
    exit /b 1
)

build\test-stnc-http.exe
if errorlevel 1 (
    echo.
    echo HTTP TEST FAILED
    exit /b 1
)


echo.
echo BUILD SUCCESSFUL
echo build\stnc-core.exe

endlocal
