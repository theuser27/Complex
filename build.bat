@echo off

set full=1
set debug=1
set release=0
set vst=0
set standalone=0
set clap=0
set data=0
set hotreload=0
set reloadable=0

echo:

where cl > NUL 2> NUL
if %ERRORLEVEL% neq 0 (

  echo Trying to set up Native Tools Command Prompt

  if defined VS2019INSTALLDIR (
    call "%VS2019INSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
  ) else if defined VS2022INSTALLDIR (
    call "%VS2022INSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
  ) else if defined VS2026INSTALLDIR (
    call "%VS2026INSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64
  ) else (
    echo Cannot find Visual Studio install directory
    echo Please locate it and call vcvarsall.bat in command prompt before running the build
    echo The path should be something like C:\Program Files\Microsoft Visual Studio\<Year>\<Version>\VC\Auxiliary\Build\vcvarsall.bat
    goto :EOF
  )
  echo:
)

for %%a in (%*) do set "%%~a=1"
if "%data%"=="1"                                                set vst=0        && set debug=0      && set full=0
if "%standalone%"=="1"           echo [standalone build]     && set vst=0        && set clap=0       && set full=0
if "%clap%"=="1"                 echo [clap build]           && set vst=0        && set standalone=0 && set full=0
if "%vst%"=="1"                  echo [vst build]            && set standalone=0 && set clap=0       && set full=0
if "%hotreload%"=="1"            echo [hotreload build]      && set full=0
if "%reloadable%"=="1"           echo [reloadble version]

if "%full%"=="1" (

  echo [vst build]
  echo [clap build]
  echo [release mode]
  echo:
  set reloadable=0
  set debug=0

  set vst=1
  call :ProjectCompile
  set vst=0
  echo:

  set clap=1
  call :ProjectCompile
  set clap=0
  echo:

) else if "%data%"=="1" (
  call :ProjectCompile
) else (

  if "%release%"=="1" (
    echo [release mode]
    set debug=0
  ) else (
    echo [debug mode]
    set release=0
  )
  echo:

  call :ProjectCompile
)

goto :EOF

::------Project Compilation------
:ProjectCompile

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /format:list') do set datetime=%%I
set datetime=%datetime:~0,8%-%datetime:~8,6%

set build_dir=build
set binary_data_dir=Source\Data
if not exist %build_dir% mkdir %build_dir%

set top_level=%~dp0
set compiled_files= %top_level%\Source\unity_extern.c %top_level%\Source\unity1.cpp %top_level%\Source\unity2.cpp
set compiler_flags= /I%top_level%\Source\ /std:c++20 /nologo /diagnostics:column /FC /permissive- /MP /Zc:preprocessor /W4 /wd"4201" /sdl- /Zc:inline /fp:precise /D "PUGL_STATIC" /D "_CRT_SECURE_NO_WARNINGS" /D "_MBCS" /errorReport:prompt /GR- /Gd
set linker_flags=   /ERRORREPORT:PROMPT /MANIFEST:EMBED /INCREMENTAL:NO /DEBUG /noexp /nocoffgrpinfo /OPT:REF /OPT:ICF Opengl32.lib Dwmapi.lib kernel32.lib user32.lib Gdi32.lib Ole32.lib Shell32.lib

set hotreload_dir=%build_dir%\hotreload
for %%I in ("%hotreload_dir%") do set "full_hotreload_dir=%%~fI"
if not exist %hotreload_dir% mkdir %hotreload_dir%

if "%reloadable%"=="1" set compiler_flags= /DCOMPLEX_HOTRELOAD_DIR=R\"(%full_hotreload_dir%)\" %compiler_flags%


if "%hotreload%"=="1" (
  set build_dir=%hotreload_dir%
  set out_file=Complex_%datetime%.dll
  set compiler_flags= /DLL %compiler_flags%
  set linker_flags= /DLL /PDB:Complex_%datetime%.pdb %linker_flags%
) else if "%vst%"=="1" (
  del /Q %hotreload_dir%\* > NUL 2> NUL
  set build_dir=%build_dir%\vst3
  set out_file=Complex.vst3
  set compiler_flags= /DLL %compiler_flags%
  set linker_flags= /DLL %linker_flags%
) else if "%clap%"=="1" (
  del /Q %hotreload_dir%\* > NUL 2> NUL
  set build_dir=%build_dir%\clap
  set out_file=Complex.clap
  set compiler_flags= /D "COMPLEX_CLAP" /DLL %compiler_flags%
  set linker_flags= /OUT:"Complex.clap" /DLL %linker_flags%
) else if "%standalone%"=="1" (
  del /Q %hotreload_dir%\* > NUL 2> NUL
  set build_dir=%build_dir%\standalone
  set out_file=Complex.exe
  set compiler_flags= /D "COMPLEX_STANDALONE" %compiler_flags%
)

set linker_flags= /OUT:"%out_file%" %linker_flags%

if not exist %build_dir% mkdir %build_dir%

if "%debug%"=="1" (
  if "%hotreload%"=="0" set build_dir=%build_dir%\debug
  set compiler_flags= /MTd /Od /Ob1 /Zi /RTC1 %compiler_flags%
  set linker_flags= /NODEFAULTLIB:libcpmtd.lib %linker_flags%
) else (
  if "%hotreload%"=="0" set build_dir=%build_dir%\release
  set compiler_flags= /MT /Ox /GL /Gy /Oi /Ot %compiler_flags%
  set linker_flags= /NODEFAULTLIB:libcpmt.lib %linker_flags%
)

if "%data%"=="1" (
  call :DataGen
  goto :EOF
)
if not exist .\Source\Data (
  echo Generating missing binary data
  call :DataGen
)

if not exist %build_dir% mkdir %build_dir%

call :StartTimer

if not exist %build_dir% mkdir %build_dir%
pushd %build_dir%

if "%hotreload%"=="0" del /Q * > NUL 2> NUL

call cl %compiler_flags% %compiled_files% /link %linker_flags%

del *.obj > NUL 2> NUL
del vc*0.pdb > NUL 2> NUL
del *.lib > NUL 2> NUL

popd

echo %cd%\%build_dir%
echo:

call :StopTimer
call :DisplayTimerResult

goto :EOF

::--------Data Generation--------

:DataGen
:: Generates Binary Data
echo [serialising data]
pushd helpers
if not exist build mkdir build
pushd build
call cl /nologo /FC /MTd ../resource_generator.c /link /DEBUG kernel32.lib Shell32.lib Shlwapi.lib Ole32.lib > NUL 2> NUL
resource_generator.exe ..\..\Data ..\..\Source\Data
popd
rd /q /s build
popd
echo:
goto :EOF

::----------Timer stuff----------

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
