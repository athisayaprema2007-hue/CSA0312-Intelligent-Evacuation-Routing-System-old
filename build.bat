@echo off
rem build.bat - Windows build script (no make required).
rem Uses gcc from PATH if available, otherwise falls back to the
rem Dev-C++ compiler location. GCC 3.4.x does not understand -std=c11
rem or -Wpedantic, so the portable flag set below is used; with a
rem modern GCC you can replace -std=c99 -pedantic by -std=c11 -Wpedantic.

setlocal
set CC=gcc
where gcc >nul 2>nul
if errorlevel 1 (
    if exist "C:\Dev-Cpp\bin\gcc.exe" set "CC=C:\Dev-Cpp\bin\gcc.exe"
)

set CFLAGS=-std=c99 -O2 -Wall -Wextra -pedantic
set CORE=src\graph.c src\hashmap.c src\hashset.c src\minheap.c src\dijkstra.c src\route_cache.c

if not exist bin mkdir bin

echo Compiling evacsim...
"%CC%" %CFLAGS% -o bin\evacsim.exe src\main.c %CORE%
if errorlevel 1 goto :oldlink

echo Compiling test_runner...
"%CC%" %CFLAGS% -o bin\test_runner.exe tests\test_runner.c %CORE%
if errorlevel 1 goto :oldlink

echo Build OK: bin\evacsim.exe, bin\test_runner.exe
exit /b 0

:oldlink
rem Fallback for the Dev-C++ GCC 3.4.2 toolchain, whose collect2 link
rem wrapper crashes on some modern Windows systems. The objects compile
rem fine, so compile them separately and call ld directly with the same
rem startup files and libraries collect2 would have used.
echo Standard link failed; trying compile-then-direct-ld fallback...
set DEVROOT=C:\Dev-Cpp
if not exist "%DEVROOT%\bin\ld.exe" goto :error

for %%f in (graph hashmap hashset minheap dijkstra route_cache) do (
    "%CC%" %CFLAGS% -c src\%%f.c -o bin\%%f.o
    if errorlevel 1 goto :error
)
"%CC%" %CFLAGS% -c src\main.c -o bin\main.o
if errorlevel 1 goto :error
"%CC%" %CFLAGS% -c tests\test_runner.c -o bin\test_runner.o
if errorlevel 1 goto :error

set LDLIBS=-lmingw32 -lgcc -lmoldname -lmingwex -lmsvcrt -luser32 -lkernel32 -ladvapi32 -lshell32 -lmingw32 -lgcc -lmoldname -lmingwex -lmsvcrt
set CRT0=%DEVROOT%\lib\crt2.o %DEVROOT%\lib\gcc\mingw32\3.4.2\crtbegin.o
set CRT1=%DEVROOT%\lib\gcc\mingw32\3.4.2\crtend.o
set OBJS=bin\graph.o bin\hashmap.o bin\hashset.o bin\minheap.o bin\dijkstra.o bin\route_cache.o

"%DEVROOT%\bin\ld.exe" -Bdynamic -o bin\evacsim.exe %CRT0% -L%DEVROOT%\lib\gcc\mingw32\3.4.2 -L%DEVROOT%\lib bin\main.o %OBJS% %LDLIBS% %CRT1%
if errorlevel 1 goto :error
"%DEVROOT%\bin\ld.exe" -Bdynamic -o bin\test_runner.exe %CRT0% -L%DEVROOT%\lib\gcc\mingw32\3.4.2 -L%DEVROOT%\lib bin\test_runner.o %OBJS% %LDLIBS% %CRT1%
if errorlevel 1 goto :error

echo Build OK (direct-ld fallback): bin\evacsim.exe, bin\test_runner.exe
exit /b 0

:error
echo Build FAILED.
exit /b 1
