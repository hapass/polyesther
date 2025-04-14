@echo off

for /f "tokens=*" %%i in ('vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set B_VS_PATH=%%i

set B_VCVARSALL_PATH=%B_VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat

if not defined B_IS_ENVIRONMENT_CONFIGURED ( call "%B_VCVARSALL_PATH%" x64 )

set B_IS_ENVIRONMENT_CONFIGURED=1

:: Use platform toolset v143

:: Quick refresher
:: /I          - Specifies the directory for include files (header files).
:: /D          - Defines a macro with a specific value for use during compilation.
:: /std:c++20  - Enables the use of the C++20 standard for compilation.
:: /EHsc       - Enables synchronous exception handling in C++ code.
:: /LD         - Tells the compiler to produce a dynamic library (DLL).
:: /MD         - Links against the multithreaded dynamic runtime library (DLL CRT).
:: /link       - Indicates that linker options follow this argument.
:: /LIBPATH    - Specifies the directory where library files (.lib) are located for linking.
:: /LTCG       - Link-Time Code Generation allows the compiler and linker to work together more closely to optimize the final binary.

echo ------------------------------------------------
echo Visual Studio environment is initialized from %B_VCVARSALL_PATH%

mkdir build
pushd build

echo ------------------------------------------------
echo Building renderer...
echo ------------------------------------------------

cl^
 /I"..\src\renderer"^
 /I"..\extern\imgui"^
 /I"..\extern\imgui\backends"^
 "/DNDEBUG"^
 /std:c++20^
 /EHsc^
 /Zi^
 /MD^
 /c^
 ../src/renderer/renderer.cpp

lib /OUT:renderer.lib renderer.obj

echo ------------------------------------------------
echo Building application...
echo ------------------------------------------------

cl^
 /I"..\src\renderer"^
 /I"..\src\application"^
 /I"..\extern\imgui"^
 /I"..\extern\imgui\backends"^
 /DNDEBUG^
 /D_CONSOLE^
 /DUNICODE^
 /D_UNICODE^
 /std:c++20^
 /EHsc^
 /Zi^
 /MD^
 ../src/application/application.cpp^
 /link^
 renderer.lib

echo ------------------------------------------------
echo Building tests...
echo ------------------------------------------------

cl^
 /I"..\src\renderer"^
 /I"..\src\tests"^
 /I"%VCInstallDir%Auxiliary\VS\UnitTest\include"^
 /I"..\extern\imgui"^
 /I"..\extern\imgui\backends"^
 "/DNDEBUG"^
 /std:c++20^
 /EHsc^
 /LD^
 /MD^
 /Zi^
 ../src/tests/tests.cpp^
 /link^
 /LIBPATH:"%VCInstallDir%Auxiliary\VS\UnitTest\lib"^
 renderer.lib

popd

echo ------------------------------------------------
echo Testing...
echo ------------------------------------------------

call "vstest.console.exe" build\tests.dll /blame