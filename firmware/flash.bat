@echo off
setlocal EnableDelayedExpansion

rem ---------------------------------------------------------------------------
rem  NUCLEO-G474RE flashing helper. Needs only STM32_Programmer_CLI,
rem  no make and no STM32CubeIDE.
rem
rem    flash.bat            write protask1.hex, verify, reset
rem    flash.bat erase      full chip erase
rem    flash.bat reset      reset the MCU
rem    flash.bat check      read live counters without halting the core
rem    flash.bat list       list connected ST-Link probes
rem
rem  Override the programmer path with the STM32_PROG variable if needed.
rem
rem  NOTE: this file is deliberately kept in plain ASCII. cmd.exe parses .bat
rem  files using the console OEM code page, and cp866 has no Ukrainian
rem  letters, so any Cyrillic text here would break the parser. Explanations
rem  in Ukrainian live in README.md instead.
rem ---------------------------------------------------------------------------

set "HEXFILE=%~dp0build\release\protask1.hex"
set "MAPFILE=%~dp0build\release\protask1.map"

if not defined STM32_PROG (
  set "STM32_PROG=C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe"
)

if not exist "%STM32_PROG%" (
  echo [ERROR] STM32_Programmer_CLI not found:
  echo         %STM32_PROG%
  echo         Set it manually: set STM32_PROG=...\STM32_Programmer_CLI.exe
  exit /b 1
)

set "CMD=%~1"
if "%CMD%"=="" set "CMD=flash"

if /i "%CMD%"=="list"  goto :do_list
if /i "%CMD%"=="erase" goto :do_erase
if /i "%CMD%"=="reset" goto :do_reset
if /i "%CMD%"=="check" goto :do_check
if /i "%CMD%"=="flash" goto :do_flash

echo [ERROR] Unknown command "%CMD%".
echo         Use: flash ^| erase ^| reset ^| check ^| list
exit /b 1

rem ---------------------------------------------------------------------------
:do_list
echo === Connected ST-Link probes ===
"%STM32_PROG%" -l
exit /b %ERRORLEVEL%

rem ---------------------------------------------------------------------------
:do_erase
echo === Mass erase ===
"%STM32_PROG%" -c port=SWD mode=UR -e all
if errorlevel 1 (
  echo [ERROR] Erase failed.
  exit /b 1
)
echo [OK] Flash erased. The board is now blank.
exit /b 0

rem ---------------------------------------------------------------------------
:do_reset
echo === MCU reset ===
"%STM32_PROG%" -c port=SWD mode=HOTPLUG -rst
exit /b %ERRORLEVEL%

rem ---------------------------------------------------------------------------
:do_flash
if not exist "%HEXFILE%" (
  echo [ERROR] Firmware file not found:
  echo         %HEXFILE%
  echo         Build it first:  make
  exit /b 1
)

for %%F in ("%HEXFILE%") do set "HEXSIZE=%%~zF"
echo === Flashing ===
echo File : %HEXFILE%
echo Size : %HEXSIZE% bytes
echo.

"%STM32_PROG%" -c port=SWD mode=UR -w "%HEXFILE%" -v -rst
if errorlevel 1 (
  echo.
  echo [ERROR] Flashing failed. Check the board connection.
  exit /b 1
)

echo.
echo [OK] Written, verified, MCU restarted.
echo      LD2: three short blinks, then steady 1 Hz.
echo      Pipeline status:  flash.bat check
exit /b 0

rem ---------------------------------------------------------------------------
:do_check
if not exist "%MAPFILE%" (
  echo [ERROR] %MAPFILE% not found - cannot resolve counter addresses.
  exit /b 1
)

rem Addresses are taken from the .map file, so they survive a rebuild
set "A_FRAME="
set "A_ERR="
set "A_OVR="
for /f "tokens=1" %%a in ('findstr /r /c:"0x[0-9a-f]*  *frame_cnt$"   "%MAPFILE%"') do set "A_FRAME=%%a"
for /f "tokens=1" %%a in ('findstr /r /c:"0x[0-9a-f]*  *dma_err_cnt$" "%MAPFILE%"') do set "A_ERR=%%a"
for /f "tokens=1" %%a in ('findstr /r /c:"0x[0-9a-f]*  *overrun_cnt$" "%MAPFILE%"') do set "A_OVR=%%a"

if not defined A_FRAME (
  echo [ERROR] frame_cnt not found in the .map file.
  exit /b 1
)

echo === Live board status ===
echo frame_cnt   @ %A_FRAME%
echo dma_err_cnt @ %A_ERR%
echo overrun_cnt @ %A_OVR%
echo.
echo Sample 1:
"%STM32_PROG%" -c port=SWD mode=HOTPLUG -r32 %A_FRAME% 0x10 | findstr /c:"%A_FRAME% :"
echo.
echo Waiting 5 s...
ping -n 6 127.0.0.1 >nul
echo Sample 2:
"%STM32_PROG%" -c port=SWD mode=HOTPLUG -r32 %A_FRAME% 0x10 | findstr /c:"%A_FRAME% :"
echo.
echo Word order: frame_cnt, dma_err_cnt, overrun_cnt, process_half flags.
echo Expected: frame_cnt grows by ~0x43xx (17000+) over 5 s, the next two
echo stay zero. That means one half-frame every 307.2 us - the pipeline runs.
exit /b 0
