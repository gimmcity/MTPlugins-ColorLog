@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer's vswhere.exe was not found.
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "COLORLOG_MSBUILD=%%i"
if not defined COLORLOG_MSBUILD (
  echo MSBuild with C++ tools was not found.
  exit /b 1
)
"%COLORLOG_MSBUILD%" ColorLog.sln /m:1 /restore /p:Configuration=Release /p:Platform=x64 /p:TrackFileAccess=false /p:UseMultiToolTask=false
if errorlevel 1 exit /b %errorlevel%
build\Release\ColorLogParserTests.exe
exit /b %errorlevel%
