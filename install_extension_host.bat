@echo off
echo =============================================================
echo Registering AB Download Manager Native Host with Browsers
echo =============================================================

set MANIFEST_PATH=d:\Download Manager AB\src\extension\native-host-manifest.json

:: 1. Google Chrome
reg add "HKCU\Software\Google\Chrome\NativeMessagingHosts\com.idm.nativehost" /ve /t REG_SZ /d "%MANIFEST_PATH%" /f >nul 2>&1

:: 2. Microsoft Edge
reg add "HKCU\Software\Microsoft\Edge\NativeMessagingHosts\com.idm.nativehost" /ve /t REG_SZ /d "%MANIFEST_PATH%" /f >nul 2>&1

:: 3. Brave Browser
reg add "HKCU\Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\com.idm.nativehost" /ve /t REG_SZ /d "%MANIFEST_PATH%" /f >nul 2>&1

echo.
echo [SUCCESS] AB Download Manager Native Host registered for Chrome, Edge, and Brave!
echo Manifest Location: %MANIFEST_PATH%
echo Target Binary: d:\Download Manager AB\cpp\build\Release\IDMNativeHost.exe
echo =============================================================
pause
