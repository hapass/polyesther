@echo off

if not defined B_IS_ENVIRONMENT_CONFIGURED (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
)

set B_IS_ENVIRONMENT_CONFIGURED=1
set B_SOLUTION_DIR=%CD%\

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

pushd src\tests
cl^
 /I"%VCInstallDir%Auxiliary\VS\UnitTest\include"^
 /I"..\..\extern\imgui"^
 /I"..\..\extern\imgui\backends"^
 /I"..\renderer"^
 "/D_SOLUTIONDIR=R\"(%B_SOLUTION_DIR%)\""^
 /std:c++20^
 /EHsc^
 /LD^
 /MD^
 /Zi^
 tests.cpp^
 /link^
 /LTCG^
 /LIBPATH:"%VCInstallDir%Auxiliary\VS\UnitTest\lib"^
 "..\..\x64\Release\renderer.lib"
popd

call "vstest.console.exe" src\tests\tests.dll