@echo off

if "%1" == "" (
    echo [Usage] : %0 {Version}
    echo Example : %0 0921
    goto exit
)

if exist stdout.txt del stdout.txt

set BOOT_PATH=..\Common\Boot

set OUTPUT=6632A-%1
set MAKE_CONFIG=_MODEL_CM6632A_
set CHIP=6632
set VID_PID=0D8C 0315
if exist %OUTPUT% del/q %OUTPUT%*
call make %OUTPUT% %MAKE_CONFIG% 1>>stdout.txt
if exist %OUTPUT% (
    %BOOT_PATH%\AddInfo %OUTPUT%.hex %BOOT_PATH%\CM66xxBoot.hex %CHIP% %1 %VID_PID% 1>>stdout.txt
) else (
    echo Build %OUTPUT% Fail
)

set BOOT_PATH=

:exit

@echo on
