@echo off
@echo WICED envirment initialize...

rem 环境定义
rem @set SDK_VERSION=WICED-6.4.0.1002
@set SDK_VERSION1=WICED-Studio-6.4-1113
@set TARGET=20735-B1_Bluetooth
@set SDK_PATH=D:\Cypress\6.4.0.1023\SDK\%SDK_VERSION1%\%TARGET%
@set APP=AndonLight
rem 环境定义结束

@echo SDK: %SDK_VERSION1%
@echo TARGET: %TARGET%
@echo APP: %APP%

rem @set PATH=%PATH%;D:\SDK\%SDK_VERSION%\%SDK_VERSION1%\%TARGET%\
@set PATH=%PATH%;%SDK_PATH%
@pushd %SDK_PATH%
rem @cd D:\SDK\%SDK_VERSION%\%SDK_VERSION1%\%TARGET%\
rem @cd %SDK_PATH%
@echo on
@echo.
@echo Begin Build
@echo.
@make clean
@REM @set ymd=%date:~0,4%%date:~5,2%%date:~8,2%
@REM set mkdir "%ymd%"
@REM rd /s /q D:\Cypress\6.4.0.1014\SDK\%SDK_VERSION1%\%TARGET%\build\%%Light-%d%%%
@REM @make Andon.Light-CYW920735Q60EVB_01 _DEBUG=LOGLEVEL_VERBOSE LIGHT=1 USE_REMOTE_PROVISION=1 UART=com23 download%  CHIP_SCHEME=CHIP_DEVKIT*
@REM @make Andon.Light-CYW920735Q60EVB_01 _DEBUG=LOGLEVEL_DEBUG LIGHT=2 USE_REMOTE_PROVISION=1 PRESS_TEST=1 UART=com9 download%   CHIP_SCHEME=CHIP1424 configLIGHTAIANDONMODE  configLIGHTAIWYZEMODE* 
@make Andon.AndonLight-CYW920735Q60EVB_01 COREDUMP=0 LIGHT=1 USE_REMOTE_PROVISION=0 ANDON_TEST=0 MESH_START_SYNC=1 CHIP_SCHEME=CHIP1424 LIGHTAI=configLIGHTAIANDONMODE UART=com11 download%*
@REM @make Andon.AndonLight-CYW920735Q60EVB_01 LIGHT=1 USE_REMOTE_PROVISION=0 ANDON_TEST=0 MESH_START_SYNC=1 CHIP_SCHEME=CHIP_DEVKIT LIGHTAI=configLIGHTAIANDONMODE UART=com6 download%*
@REM @make Andon.Light-CYW920735Q60EVB_01 _DEBUG=LOGLEVEL_INFO LIGHT=1 USE_REMOTE_PROVISION=1 UART=com4 download%*
@REM @make Andon.Light-CYW920735Q60EVB_01 _DEBUG=LOGLEVEL_WARNNING LIGHT=2 USE_REMOTE_PROVISION=1 UART=com7 download%*
@REM @make Andon.Light-CYW920735Q60EVB_01 LIGHT=2 USE_REMOTE_PROVISION=1 UART=com7 download%*
@REM @cd D:\Cypress\6.4.0.1023\SDK\%SDK_VERSION1%\%TARGET%\build\APP-$(shell echo %date:~0,4%%date:~5,2%%date:~8,2%)
@cd %SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%
@..\..\..\wiced_tools\DfuMeta\Win32\DfuMeta cid=0x0804 pid=0x3130 vid=0x0001 version=6.4.0.26 -k ..\..\..\common\apps\Andon\provisioner_node_light\src_provisioner\ecdsa256_key.pri.bin -i %APP%-%date:~0,4%%date:~5,2%%date:~8,2%.ota.bin -m metadata -o manifest.json

@..\..\hex2bin %APP%-%date:~0,4%%date:~5,2%%date:~8,2%.hex

@popd

@echo off 
if EXIST ".\target" (
    rd /s/q ".\target")
mkdir ".\target"

@copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\manifest.json" ".\target"
@copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\metadata" ".\target"
@copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.*" ".\target"
@REM @copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.dfu.bin" ".\target"
@REM @copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.hex" ".\target"
@REM @copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.bin" ".\target"
@REM @copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.ota.hex" ".\target"
@REM @copy "%SDK_PATH%\build\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%\%APP%-%date:~0,4%%date:~5,2%%date:~8,2%.ota.bin" ".\target"

@echo on

@echo %date%%time%
@REM @make projects.WICED.dev_kit %*




