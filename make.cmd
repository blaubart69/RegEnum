@echo off
rem
rem link DYNAMIC against ucrt.lib
rem 
set out=.\build\
cl /nologo /std:c++17 /Fo%OUT% /EHsc /Os .\src\RegEnum.cpp ^
/link /out:%OUT%RegEnum.exe /nodefaultlib ^
kernel32.lib ucrt.lib libvcruntime.lib libcmt.lib libcpmt.lib Advapi32.lib user32.lib