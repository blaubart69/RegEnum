@rem
@rem link DYNAMIC against ucrt.lib
@rem 
cl /nologo /std:c++17 /EHsc /Os RegQueryInfoKey.cpp /link /nodefaultlib kernel32.lib ucrt.lib libvcruntime.lib libcmt.lib libcpmt.lib Advapi32.lib