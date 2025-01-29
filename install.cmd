@echo off

set INSTALL_DIR=E:\re\x64dbg_dev\release\x64\plugins\

if not exist %INSTALL_DIR% mkdir %INSTALL_DIR%
copy /Y build64\Release\x64dbg-automate.dp64 %INSTALL_DIR%\
copy /Y build64\Release\libzmq-mt-4_3_5.dll %INSTALL_DIR%\