@echo off
setlocal
REM run this script from an Admin shell to set up ETW tracing

REM set SDK_MANIFEST_PATH to the SDK install path (e.g. C:\Program Files (x86)\Oculus)
for /f "delims=" %%a in ('reg query "HKLM\System\CurrentControlSet\Services\OVRService" -v "ImagePath"') do set SDK_MANIFEST_PATH=%%a
set SDK_MANIFEST_PATH=%SDK_MANIFEST_PATH:~34,-31%\Tools\ETW

REM Add USERS Read & Execute privileges to the folder
icacls . /grant BUILTIN\Users:(OI)(CI)(RX) >nul
if %errorlevel% equ 0 goto CaclsOk

echo Failed to set cacls, installation may fail

:CaclsOk

set OSTYPE=x64
set RIFTENABLER_SYS=%windir%\System32\drivers\RiftEnabler.sys
set OCUSBVID_SYS=%windir%\System32\drivers\OCUSBVID.sys
set OVRDISPLAYRT_DLL=%windir%\System32\OVRDisplayRT64.dll
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" goto GotOSTYPE
if "%PROCESSOR_ARCHITEW6432%"=="AMD64" goto GotOSTYPE
set OSTYPE=x86
REM XXX is this right?
set RIFTENABLER_SYS=%windir%\System32\drivers\RiftEnabler.sys
set OCUSBVID_SYS=%windir%\System32\drivers\OCUSBVID.sys
set OVRDISPLAYRT_DLL=%windir%\System32\OVRDisplayRT32.dll

:GotOSTYPE

REM disable paging on x64 systems if stack walks are desired
if %OSTYPE% neq x64 goto SkipRegCheck
for /f "delims=" %%a in ('reg query "HKLM\System\CurrentControlSet\Control\Session Manager\Memory Management" -v "DisablePagingExecutive"') do set REG_DPA=%%a

if %REG_DPA:~-3% equ 0x1 goto SkipRegCheck
echo ************************
echo DisablePagingExecutive should be set if you want stack tracing to work on %OSTYPE%
echo To disable paging run the following as Administrator:
echo   reg add "HKLM\System\CurrentControlSet\Control\Session Manager\Memory Management" -v DisablePagingExecutive -d 0x1 -t REG_DWORD -f
echo and reboot
echo ************************

:SkipRegCheck

set RIFTDISPLAYDRIVER_DIR=..\..\..\RiftDisplayDriver
set RIFTCAMERADRIVER_DIR=..\..\..\RiftPTDriver

if exist "%SDK_MANIFEST_PATH%\OVRKernelEvents.man" set KERNEL_EVENTS_MAN=%SDK_MANIFEST_PATH%\OVRKernelEvents.man
if exist "%RIFTDISPLAYDRIVER_DIR%\RiftEnabler\OVRKernelEvents.man" set KERNEL_EVENTS_MAN=%RIFTDISPLAYDRIVER_DIR%\RiftEnabler\OVRKernelEvents.man
if exist ".\OVRKernelEvents.man" set KERNEL_EVENTS_MAN=.\OVRKernelEvents.man

echo Installing %RIFTENABLER_SYS% manifest...
REM uninstall any existing manifest first
wevtutil.exe uninstall-manifest "%KERNEL_EVENTS_MAN%"
if %errorlevel% neq 0 echo WARNING: This step failed.
wevtutil.exe install-manifest "%KERNEL_EVENTS_MAN%" /rf:%RIFTENABLER_SYS% /mf:%RIFTENABLER_SYS%
REM make sure it worked
wevtutil get-publisher OVR-Kernel > nul
if %errorlevel% neq 0 echo WARNING: This step failed.
echo Installed %KERNEL_EVENTS_MAN%

if exist "%SDK_MANIFEST_PATH%\RTFilterEvents.man" set RFILTER_EVENTS_MAN=%SDK_MANIFEST_PATH%\RTFilterEvents.man
if exist "%RIFTDISPLAYDRIVER_DIR%\rt_filter\RTFilterEvents.man" set RFILTER_EVENTS_MAN=%RIFTDISPLAYDRIVER_DIR%\rt_filter\RTFilterEvents.man
if exist ".\RTFilterEvents.man" set RFILTER_EVENTS_MAN=.\RTFilterEvents.man

echo Installing %OVRDISPLAYRT_DLL% manifest...
REM uninstall any existing manifest first
wevtutil.exe uninstall-manifest "%RFILTER_EVENTS_MAN%"
if %errorlevel% neq 0 echo WARNING: This step failed.
wevtutil.exe install-manifest "%RFILTER_EVENTS_MAN%" /rf:%OVRDISPLAYRT_DLL% /mf:%OVRDISPLAYRT_DLL%
REM make sure it worked
wevtutil get-publisher OVR-RTFilter > nul
if %errorlevel% neq 0 echo WARNING: This step failed.
echo Installed %RFILTER_EVENTS_MAN%

if exist "%SDK_MANIFEST_PATH%\OVRUSBVidEvents.man" set USBVID_EVENTS_MAN=%SDK_MANIFEST_PATH%\OVRUSBVidEvents.man
if exist "%RIFTCAMERADRIVER_DIR%\OCUSBVID\OVRUSBVidEvents.man" set USBVID_EVENTS_MAN=%RIFTCAMERADRIVER_DIR%\OCUSBVID\OVRUSBVidEvents.man
if exist ".\OVRUSBVidEvents.man" set USBVID_EVENTS_MAN=.\OVRUSBVidEvents.man

echo Installing %OCUSBVID_SYS% manifest...
REM uninstall any existing manifest first
wevtutil.exe uninstall-manifest "%USBVID_EVENTS_MAN%"
if %errorlevel% neq 0 echo WARNING: This step failed.
wevtutil.exe install-manifest "%USBVID_EVENTS_MAN%" /rf:%OCUSBVID_SYS% /mf:%OCUSBVID_SYS%
REM make sure it worked
wevtutil get-publisher OVR-USBVid > nul
if %errorlevel% neq 0 echo WARNING: This step failed.
echo Installed %USBVID_EVENTS_MAN%

REM XXX eventually add OVR-Compositor here...

if exist "%SDK_MANIFEST_PATH%\LibOVREvents.man" set LIBOVR_EVENTS_MAN=%SDK_MANIFEST_PATH%\LibOVREvents.man
if exist ".\LibOVREvents.man" set LIBOVR_EVENTS_MAN=.\LibOVREvents.man

REM this nightmare command copies the newest version of LibOVRRT*.dll into the current directory without prompting...
forfiles /p:c:\Windows\System32 /m:LibOVRRT*.dll /c "cmd /c xcopy /y /f /d @path %cd% >nul" >nul 2>nul
forfiles /s /p:..\..\..\LibOVR\Lib\Windows /m:LibOVRRT*.dll /c "cmd /c xcopy /y /f /d @path %cd% >nul" >nul 2>nul
for /f "delims=" %%a in ('dir /b /o:d LibOVRRT*.dll') do set LIBOVR_DLL=%%a
set LIBOVR_DLL=%cd%\%LIBOVR_DLL%
echo Installing %LIBOVR_DLL% manifest...
REM uninstall any existing manifest first
wevtutil uninstall-manifest "%LIBOVR_EVENTS_MAN%"
if %errorlevel% neq 0 exit /b 1
wevtutil install-manifest "%LIBOVR_EVENTS_MAN%" /rf:%LIBOVR_DLL% /mf:%LIBOVR_DLL%
REM make sure it worked
wevtutil get-publisher OVR-SDK-LibOVR > nul
if %errorlevel% neq 0 exit /b 1
echo Installed %LIBOVR_EVENTS_MAN%

echo You can now start/stop traces with the GUI:
echo   cd ..\..\..\Tools\TraceScript\ovrtap
echo   .\startovrtap.cmd
echo or (command-line):
echo   cd ..\..\..\Tools\Xperf
echo   log
