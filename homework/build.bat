cl -Zs buf.c -nologo
REM cl buf.c -Dtest_buf=main -Z7 -nologo && buf.exe

cl -Fe:day2.exe day2.c -nologo -Z7 && day2.exe 5 + 2*7 - 11/3

