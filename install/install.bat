@echo off
setlocal EnableExtensions
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Administrator privileges are required.
  pause
  exit /b 1
)

if not exist ARCH.txt (
  echo [ERROR] ARCH.txt is missing. Extract the complete GitHub Actions ZIP first.
  pause
  exit /b 1
)
set /p ARCH=<ARCH.txt
if /i "%ARCH%"=="x86" (
  set "DEST=%WINDIR%\twain_32\OMRFolderScanner"
) else if /i "%ARCH%"=="x64" (
  set "DEST=%WINDIR%\twain_64\OMRFolderScanner"
) else (
  echo [ERROR] Unsupported ARCH value: %ARCH%
  pause
  exit /b 1
)

set "ROOT=C:\VirtualScan"
set "LOG=%ROOT%\install.log"
if not exist "%ROOT%" mkdir "%ROOT%"
if not exist "%ROOT%" (
  echo [ERROR] Cannot create %ROOT%
  pause
  exit /b 1
)

echo ==== OMR Folder Scanner install ==== > "%LOG%"
echo Architecture=%ARCH%>>"%LOG%"
echo Destination=%DEST%>>"%LOG%"

if not exist OMRFolderScanner.ds goto :missingds
if not exist FreeImage.dll goto :missingdll
if not exist feed.ini goto :missingini

if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%" goto :destfail
copy /y OMRFolderScanner.ds "%DEST%\OMRFolderScanner.ds" >>"%LOG%" 2>&1
if errorlevel 1 goto :copyfail
copy /y FreeImage.dll "%DEST%\FreeImage.dll" >>"%LOG%" 2>&1
if errorlevel 1 goto :copyfail
if not exist "%DEST%\feed.ini" copy /y feed.ini "%DEST%\feed.ini" >>"%LOG%" 2>&1
if not exist "%DEST%\feed.ini" goto :copyfail

for %%D in (in done bad) do (
  if not exist "%ROOT%\%%D" mkdir "%ROOT%\%%D"
  if not exist "%ROOT%\%%D" goto :folderfail
)
icacls "%ROOT%" /grant *S-1-5-32-545:(OI)(CI)M /T >>"%LOG%" 2>&1
if errorlevel 1 echo [WARN] icacls returned an error. See install.log.>>"%LOG%"

if not exist "%DEST%\OMRFolderScanner.ds" goto :copyfail
if not exist "%DEST%\FreeImage.dll" goto :copyfail
for %%D in (in done bad) do if not exist "%ROOT%\%%D" goto :folderfail

echo [OK] Installation completed.
echo [OK] Installation completed.>>"%LOG%"
echo TWAIN folder: %DEST%
echo Feed folder: %ROOT%
echo Diagnostic log: %TEMP%OMRFolderScanner-ds.log
pause
exit /b 0

:missingds
echo [ERROR] OMRFolderScanner.ds is missing.>>"%LOG%"
goto :fail
:missingdll
echo [ERROR] FreeImage.dll is missing.>>"%LOG%"
goto :fail
:missingini
echo [ERROR] feed.ini is missing.>>"%LOG%"
goto :fail
:destfail
echo [ERROR] Cannot create TWAIN destination: %DEST%>>"%LOG%"
goto :fail
:copyfail
echo [ERROR] Driver copy failed.>>"%LOG%"
goto :fail
:folderfail
echo [ERROR] Feed folder creation failed.>>"%LOG%"
goto :fail
:fail
echo [ERROR] Installation failed. Check C:\VirtualScan\install.log
pause
exit /b 1
