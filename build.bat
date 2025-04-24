@echo off

call "shell.bat"

:: Quick refresher for cl flags used in this bat file.
:: /I          - Specifies the directory for include files (header files).
:: /D          - Defines a macro with a specific value for use during compilation.
:: /std:c++20  - Enables the use of the C++20 standard for compilation.
:: /EHsc       - Enables synchronous exception handling in C++ code.
:: /LD         - Tells the compiler to produce a dynamic library (DLL).
:: /MD         - Links against the multithreaded dynamic runtime library (DLL CRT).
:: /link       - Indicates that linker options follow this argument.
:: /LIBPATH    - Specifies the directory where library files (.lib) are located for linking.
:: /Od         - Disables optimizations to improve debugging and speed up compilation.
:: /O2         - Enables full optimization for speed, favoring performance over size.
:: /c          - Compiles source files without linking, producing object files (.obj).

set B_UNICODE_FLAGS=/DUNICODE /D_UNICODE

set B_DEBUG_DEPENDANT_ARGS=/Zi /Od /D_DEBUG /MDd
if /i "%1"=="release" set B_DEBUG_DEPENDANT_ARGS=/O2 /DNDEBUG /MD
if /i "%1"=="profile" set B_DEBUG_DEPENDANT_ARGS=/O2 /DNDEBUG /MD /DENABLE_DETAILED_PERF_LOG

set B_COMMON_FLAGS=/std:c++20 /EHsc %B_DEBUG_DEPENDANT_ARGS%
set B_COMMON_INCLUDES=/I"..\src\renderer" /I"..\src\common" /I"..\extern\imgui" /I"..\extern\imgui\backends"

set B_TESTS_INCLUDES=/I"%VCInstallDir%Auxiliary\VS\UnitTest\include"
set B_TESTS_LIBPATH=/LIBPATH:"%VCInstallDir%Auxiliary\VS\UnitTest\lib"

echo ------------------------------------------------
echo Visual Studio environment is initialized from %B_VCVARSALL_PATH%

mkdir build

echo ------------------------------------------------
echo Copy assets...
echo ------------------------------------------------

call robocopy "assets" "build\assets" /MIR
call :FailIfError 8

pushd build

echo ------------------------------------------------
echo Building signature...
echo ------------------------------------------------

cl /I"..\src\signature" /I"..\src\common" /D_CONSOLE %B_COMMON_FLAGS% ../src/signature/signature.cpp
call :FailIfError 1

call "signature.exe"

echo ------------------------------------------------
echo Building renderer...
echo ------------------------------------------------

cl /I"." %B_COMMON_INCLUDES% %B_COMMON_FLAGS% /c ../src/renderer/renderer.cpp
call :FailIfError 1

lib /OUT:renderer.lib renderer.obj
call :FailIfError 1

echo ------------------------------------------------
echo Building application...
echo ------------------------------------------------

cl /I"..\src\application" %B_COMMON_INCLUDES% /D_CONSOLE %B_UNICODE_FLAGS% %B_COMMON_FLAGS% ../src/application/application.cpp /link renderer.lib
call :FailIfError 1

echo ------------------------------------------------
echo Building tests...
echo ------------------------------------------------

cl /I"..\src\tests" %B_COMMON_INCLUDES% %B_TESTS_INCLUDES% %B_COMMON_FLAGS% /LD ../src/tests/tests.cpp /link %B_TESTS_LIBPATH% renderer.lib
call :FailIfError 1

echo ------------------------------------------------
echo Testing...
echo ------------------------------------------------

call "vstest.console.exe" tests.dll /blame
call :FailIfError 1

popd

exit /b 0

:: Tools

:FailIfError
if errorlevel %1 ( exit 1 )
exit /b 0