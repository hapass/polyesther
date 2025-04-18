@echo off

for /f "tokens=*" %%i in ('vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set B_VS_PATH=%%i

set B_VCVARSALL_PATH=%B_VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat

if not defined B_IS_ENVIRONMENT_CONFIGURED ( call "%B_VCVARSALL_PATH%" x64 )

set B_IS_ENVIRONMENT_CONFIGURED=1