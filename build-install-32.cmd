@echo off

set INSTALL_DIR=E:\re\x64dbg_dev\release\x32\plugins\

if not exist %INSTALL_DIR% mkdir %INSTALL_DIR%
cmake -B build32 -A Win32
cmake --build build32 --config Release
copy /Y build32\Release\x64dbg-automate.dp32 %INSTALL_DIR%\
copy /Y build32\Release\libzmq-mt-4_3_5.dll %INSTALL_DIR%\