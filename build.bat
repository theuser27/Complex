@echo off

set debug=1
set release=0
set vst=1
set standalone=0
set clap=0

for %%a in (%*) do set "%%~a=1"
if "%standalone%"=="1"           echo [standalone build]     && set vst=0 && set clap=0
if "%clap%"=="1"                 echo [clap build]           && set vst=0 && set standalone=0
if "%vst%"=="1"                  echo [vst build]            && set standalone=0 && set clap=0
if "%release%"=="1"              echo [release mode]         && set debug=0
if "%debug%"=="1"                echo [debug mode]           && set release=0
if "%~1"==""                     echo [assuming `vst` build] && set vst=1
if "%~1"=="release" if "%~2"=="" echo [assuming `vst` build] && set vst=1

echo:

set top_level=..\..\..
set compiler_flags= /I%top_level%\Source\ /std:c++20 /nologo /diagnostics:column /FC /permissive- /MP /Zc:preprocessor /W4 /wd"4201" /sdl- /Zc:inline /fp:precise /D "PUGL_STATIC" /D "_CRT_SECURE_NO_WARNINGS" /D "_MBCS" /errorReport:prompt /GR- /Gd
set linker_flags=   /ERRORREPORT:PROMPT /MANIFEST:EMBED /INCREMENTAL:NO /DEBUG /noexp /nocoffgrpinfo /OPT:REF /OPT:ICF Opengl32.lib Dwmapi.lib kernel32.lib user32.lib Gdi32.lib Ole32.lib Shell32.lib

set build_dir=build_test
if not exist %build_dir% mkdir %build_dir%

if "%vst%"=="1" (
  :: VST3 build
  set build_dir=%build_dir%\vst3
  set compiler_flags= /DLL %compiler_flags%
  set linker_flags= /OUT:"Complex.vst3" /DLL %linker_flags%
) else if "%clap%"=="1" (
  :: Clap build
  set build_dir=%build_dir%\clap
  set compiler_flags= /D "COMPLEX_CLAP" /DLL %compiler_flags%
  set linker_flags= /OUT:"Complex.clap" /DLL %linker_flags%
) else if "%standalone%"=="1" (
  :: Standalone build
  set build_dir=%build_dir%\standalone
  set compiler_flags= /D "COMPLEX_STANDALONE" %compiler_flags%
  set linker_flags= /OUT:"Complex.exe" %linker_flags%
)

if not exist %build_dir% mkdir %build_dir%

if "%debug%"=="1" (
  set build_dir=%build_dir%\Debug
  set compiler_flags= /Od /Ob1 /Zi /MTd /RTC1 %compiler_flags%
) else (
  set build_dir=%build_dir%\Release
  set compiler_flags= /Ox /GL %compiler_flags%
)

if not exist %build_dir% mkdir %build_dir%

where cl > NUL 2> NUL
if %ERRORLEVEL% neq 0 (
  echo cl.exe wasn't found
  echo trying to set up native tools cmd
  call "%VS2022INSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
  echo:
)

call :StartTimer

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%

del /Q * > NUL 2> NUL

call cl %compiler_flags% %top_level%\Source\unity_extern.c %top_level%\Source\unity.cpp %top_level%\Source\unity_data.cpp /link %linker_flags%

del *.obj > NUL 2> NUL
del vc*0.pdb > NUL 2> NUL
del *.lib > NUL 2> NUL

popd

echo %cd%\%build_dir%
echo:

call :StopTimer
call :DisplayTimerResult



::--------Timer stuff--------

:StartTimer
:: Store start time
set StartTIME=%TIME%
for /f "usebackq tokens=1-4 delims=:., " %%f in (`echo %StartTIME: =0%`) do set /a Start100S=1%%f*360000+1%%g*6000+1%%h*100+1%%i-36610100
goto :EOF

:StopTimer
:: Get the end time
set StopTIME=%TIME%
for /f "usebackq tokens=1-4 delims=:., " %%f in (`echo %StopTIME: =0%`) do set /a Stop100S=1%%f*360000+1%%g*6000+1%%h*100+1%%i-36610100
:: Test midnight rollover. If so, add 1 day=8640000 1/100ths secs
if %Stop100S% LSS %Start100S% set /a Stop100S+=8640000
set /a TookTime=%Stop100S%-%Start100S%
set TookTimePadded=0%TookTime%
goto :EOF

:DisplayTimerResult
:: Show timer start/stop/delta
echo Started: %StartTime%
echo Stopped: %StopTime%
echo Elapsed: %TookTime:~0,-2%.%TookTimePadded:~-2% seconds
goto :EOF
