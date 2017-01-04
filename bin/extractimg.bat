@echo off

set img=%~f1
if "%img%"=="" goto error

set infile=%~nx1
set inpath=.\%~n1

if exist "%inpath%" rd /s /q "%inpath%"
if %errorlevel% neq 0 goto error

md "%inpath%"
if %errorlevel% neq 0 goto error

unpackbootimg -i "%img%" -o "%inpath%"
if %errorlevel% neq 0 goto error

lzop -d "%inpath%\%infile%-ramdisk.gz" -o "%inpath%\initramfs.cpio" <nul
if %errorlevel% neq 0 goto error

md "%inpath%\initramfs"
if %errorlevel% neq 0 goto error

cd "%inpath%\initramfs"
if %errorlevel% neq 0 goto error

cpio -imd --no-preserve-owner --quiet -I "..\initramfs.cpio"
if %errorlevel% neq 0 goto error

exit /b %errorlevel%

:error
if %errorlevel% equ 0 set errorlevel=7
echo %0 %1 -^> exit %errorlevel% ?!?
exit /b %errorlevel%
