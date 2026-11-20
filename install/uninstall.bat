@echo off
setlocal EnableExtensions
net session >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Administrator privileges are required.
  pause
  exit /b 1
)

set "LOG=%TEMP%\OMRFolderScanner-uninstall.log"
echo ==== OMR Folder Scanner uninstall ==== > "%LOG%"
set "FAILED=0"

for %%D in ("%WINDIR%\twain_32\OMRFolderScanner" "%WINDIR%\twain_64\OMRFolderScanner") do (
  if exist "%%~D" (
    echo Removing %%~D>>"%LOG%"
    rmdir /s /q "%%~D" >>"%LOG%" 2>&1
    if exist "%%~D" (
      echo [ERROR] Could not remove %%~D>>"%LOG%"
      set "FAILED=1"
    )
  )
)

if "%FAILED%"=="1" (
  echo [ERROR] Uninstall incomplete. See %LOG%
  pause
  exit /b 1
)
echo [OK] TWAIN driver files removed. User feed images in C:\VirtualScan were preserved.
echo [OK] Uninstall completed.>>"%LOG%"
pause
exit /b 0
