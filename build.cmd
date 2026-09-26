@echo off
setlocal

if /I "%~1"=="clean" (
    if exist build ( rmdir /S /Q build & if exist build ( echo. & echo CLEAN FAILED & exit /b 1 ) )
    echo. & echo CLEAN SUCCESSFUL & exit /b 0
)
if not "%~1"=="" ( echo Usage: & echo   build & echo   build clean & exit /b 1 )
if not exist build mkdir build
where cl >nul 2>&1
if errorlevel 1 ( for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%I\VC\Auxiliary\Build\vcvars64.bat" >nul )
where cl >nul 2>&1
if errorlevel 1 ( echo MSVC C Build Tools and a Windows SDK are required. & exit /b 1 )

cl /nologo /W4 /TC /Iincludes src\main.c src\stnc_command.c src\stnc_client.c src\stnc_identity.c src\stnc_contract_draft.c src\stnc_contract_action.c src\stnc_contract_create.c platforms\windows\gui_windows.c src\stnc_background_mining.c src\stnc_contract_status.c src\stnc_core.c src\stnc_log.c src\stnc_mining.c src\stnc_mining_service.c src\stnc_stratum.c src\stnc_stratum_client.c src\stnc_config.c src\stnc_directory.c src\stnc_http.c src\stnc_network.c src\stnc_peers.c src\stnc_peer_select.c src\stnc_stnc.c src\stnc_stnp.c src\stnc_transfer.c src\stnc_wallet.c src\stnc_wallet_store.c src\stnc_wallet_status.c platforms\windows\platform_windows.c src\crypto\ed25519_donna\ed25519_provider.c /Fe:build\stnc-core.exe /link ws2_32.lib winhttp.lib bcrypt.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib
if errorlevel 1 ( echo. & echo BUILD FAILED & exit /b 1 )

cl /nologo /W4 /TC /D_CRT_SECURE_NO_WARNINGS /Iincludes tests\test_stnc_log.c src\stnc_log.c /Fe:build\test-stnc-log.exe && build\test-stnc-log.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_stnp.c src\stnc_stnp.c /Fe:build\test-stnc-stnp.exe && build\test-stnc-stnp.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_peers.c src\stnc_peers.c /Fe:build\test-stnc-peers.exe && build\test-stnc-peers.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_directory.c src\stnc_directory.c src\stnc_peers.c /Fe:build\test-stnc-directory.exe && build\test-stnc-directory.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_http.c src\stnc_http.c /Fe:build\test-stnc-http.exe && build\test-stnc-http.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_peer_select.c src\stnc_peer_select.c /Fe:build\test-stnc-peer-select.exe && build\test-stnc-peer-select.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_stnc.c src\stnc_stnc.c /Fe:build\test-stnc-stnc.exe && build\test-stnc-stnc.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_mining.c src\stnc_mining.c /Fe:build\test-stnc-mining.exe && build\test-stnc-mining.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_mining_service.c src\stnc_mining_service.c /Fe:build\test-stnc-mining-service.exe && build\test-stnc-mining-service.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_stratum.c src\stnc_stratum.c /Fe:build\test-stnc-stratum.exe && build\test-stnc-stratum.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_stratum_client.c src\stnc_stratum_client.c src\stnc_stratum.c /Fe:build\test-stnc-stratum-client.exe && build\test-stnc-stratum-client.exe || exit /b 1
cl /nologo /W4 /TC /D_CRT_SECURE_NO_WARNINGS /Iincludes tests\test_stnc_background_mining.c src\stnc_background_mining.c src\stnc_mining_service.c src\stnc_log.c /Fe:build\test-stnc-background-mining.exe && build\test-stnc-background-mining.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_wallet.c src\stnc_wallet.c src\stnc_core.c src\stnc_background_mining.c src\stnc_stratum_client.c src\stnc_stratum.c src\stnc_mining_service.c src\stnc_mining.c src\stnc_wallet_store.c src\stnc_log.c src\stnc_config.c src\stnc_directory.c src\stnc_http.c src\stnc_network.c src\stnc_peers.c src\stnc_peer_select.c src\stnc_stnc.c src\stnc_stnp.c platforms\windows\platform_windows.c src\crypto\ed25519_donna\ed25519_provider.c /Fe:build\test-stnc-wallet.exe /link ws2_32.lib winhttp.lib bcrypt.lib advapi32.lib && build\test-stnc-wallet.exe || exit /b 1
cl /nologo /W4 /TC /D_CRT_SECURE_NO_WARNINGS /Iincludes tests\test_stnc_wallet_store.c src\stnc_wallet_store.c src\stnc_wallet.c platforms\windows\platform_windows.c src\crypto\ed25519_donna\ed25519_provider.c src\stnc_core.c src\stnc_background_mining.c src\stnc_stratum_client.c src\stnc_stratum.c src\stnc_mining_service.c src\stnc_mining.c src\stnc_log.c src\stnc_config.c src\stnc_directory.c src\stnc_http.c src\stnc_network.c src\stnc_peers.c src\stnc_peer_select.c src\stnc_stnc.c src\stnc_stnp.c /Fe:build\test-stnc-wallet-store.exe /link ws2_32.lib winhttp.lib bcrypt.lib advapi32.lib && build\test-stnc-wallet-store.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_wallet_status.c src\stnc_wallet_status.c /Fe:build\test-stnc-wallet-status.exe && build\test-stnc-wallet-status.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_contract_status.c src\stnc_contract_status.c /Fe:build\test-stnc-contract-status.exe && build\test-stnc-contract-status.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_transfer.c src\stnc_transfer.c /Fe:build\test-stnc-transfer.exe && build\test-stnc-transfer.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_identity.c src\stnc_identity.c src\stnc_wallet.c src\stnc_core.c src\stnc_background_mining.c src\stnc_stratum_client.c src\stnc_stratum.c src\stnc_mining_service.c src\stnc_mining.c src\stnc_wallet_store.c src\stnc_log.c src\stnc_config.c src\stnc_directory.c src\stnc_http.c src\stnc_network.c src\stnc_peers.c src\stnc_peer_select.c src\stnc_stnc.c src\stnc_stnp.c platforms\windows\platform_windows.c src\crypto\ed25519_donna\ed25519_provider.c /Fe:build\test-stnc-identity.exe /link ws2_32.lib winhttp.lib bcrypt.lib advapi32.lib && build\test-stnc-identity.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_contract_draft.c src\stnc_contract_draft.c src\stnc_wallet.c src\stnc_core.c src\stnc_background_mining.c src\stnc_stratum_client.c src\stnc_stratum.c src\stnc_mining_service.c src\stnc_mining.c src\stnc_wallet_store.c src\stnc_log.c src\stnc_config.c src\stnc_directory.c src\stnc_http.c src\stnc_network.c src\stnc_peers.c src\stnc_peer_select.c src\stnc_stnc.c src\stnc_stnp.c platforms\windows\platform_windows.c src\crypto\ed25519_donna\ed25519_provider.c /Fe:build\test-stnc-contract_draft.exe /link ws2_32.lib winhttp.lib bcrypt.lib advapi32.lib && build\test-stnc-contract_draft.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_contract_action.c src\stnc_contract_action.c /Fe:build\test-stnc-contract-action.exe && build\test-stnc-contract-action.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_contract_create.c src\stnc_contract_create.c /Fe:build\test-stnc-contract-create.exe && build\test-stnc-contract-create.exe || exit /b 1
cl /nologo /W4 /TC /Iincludes tests\test_stnc_client.c src\stnc_client.c /Fe:build\test-stnc-client.exe && build\test-stnc-client.exe || exit /b 1
cl /nologo /W4 /TC tests\test_stnc_gui.c /Fe:build\test-stnc-gui.exe /link user32.lib gdi32.lib && build\test-stnc-gui.exe || exit /b 1
echo. & echo BUILD SUCCESSFUL & echo build\stnc-core.exe
endlocal
