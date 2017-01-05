@echo off

set img=%~f1
if "%img%"=="" goto error

set infile=%~nx1
set inpath=.\%~n1

if exist "%inpath%" rd /s /q "%inpath%"
if errorlevel 1 goto error

md "%inpath%"
if errorlevel 1 goto error

unpackbootimg -i "%img%" -o "%inpath%"
if errorlevel 1 goto error

lzop -d "%inpath%\%infile%-ramdisk.gz" -o "%inpath%\initramfs.cpio"
if errorlevel 1 goto error

md "%inpath%\initramfs"
if errorlevel 1 goto error

cd "%inpath%\initramfs"
if errorlevel 1 goto error

cpio -imd --no-preserve-owner --quiet -I "..\initramfs.cpio"
if errorlevel 1 goto error

exit 0
:error
echo %0 %* -^> failed
exit 1
