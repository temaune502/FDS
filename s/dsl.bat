@echo off
setlocal

set "ROOT=%~dp0.."
set "GCC=E:\msys\ucrt64\bin\gcc.exe"
if defined FDS_GCC set "GCC=%FDS_GCC%"
set "BUILD=%ROOT%\build"
set "SOURCE=%ROOT%\src\dsl.c"
set "OUTPUT=%BUILD%\dsl.exe"
set "LOG=%BUILD%\dsl.log"

if not exist "%GCC%" (
    echo ERROR: GCC not found: "%GCC%"
    echo Set FDS_GCC to override the compiler path.
    exit /b 2
)
if not exist "%SOURCE%" (
    echo ERROR: DSL test source not found: "%SOURCE%"
    exit /b 2
)
if not exist "%BUILD%" mkdir "%BUILD%"

pushd "%ROOT%"
>"%LOG%" echo [build] %GCC% -std=c11 -Wall -Wextra -pedantic src\dsl.c -o build\dsl.exe
"%GCC%" -std=c11 -Wall -Wextra -pedantic "%SOURCE%" -o "%OUTPUT%" >>"%LOG%" 2>&1
set "BUILD_EXIT=%ERRORLEVEL%"
if not "%BUILD_EXIT%"=="0" goto report

>>"%LOG%" echo.
>>"%LOG%" echo [run] %OUTPUT%
"%OUTPUT%" >>"%LOG%" 2>&1
set "RUN_EXIT=%ERRORLEVEL%"

:report
popd
 type "%LOG%"
if not "%BUILD_EXIT%"=="0" exit /b %BUILD_EXIT%
exit /b %RUN_EXIT%
