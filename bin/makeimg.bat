@echo off

set inpath=%~f1
if "%inpath%"=="" goto error

set infile=%~nx1.img
set img=.\%~nx1.img

if exist "%inpath%\initramfs\hakchi\transfer" rd /s /q "%inpath%\initramfs\hakchi\transfer"
xcopy "%inpath%\..\mod" "%inpath%\initramfs" /h /y /c /r /s /q || cd >nul
if "%2"=="notx" if exist "%inpath%\initramfs\hakchi\transfer" rd /s /q "%inpath%\initramfs\hakchi\transfer"
if errorlevel 1 goto error

upx -qq --best "%inpath%\initramfs\sbin\cryptsetup" || cd >nul

mkbootfs "%inpath%\initramfs" > "%inpath%\initramfs.cpio"
if errorlevel 1 goto error

lzop --best -f -o "%inpath%\%infile%-ramdisk.gz" "%inpath%\initramfs.cpio"
if errorlevel 1 goto error

set /p cmdline=<"%inpath%\%infile%-cmdline"
set /p board=<"%inpath%\%infile%-board"
set /p base=<"%inpath%\%infile%-base"
set /p pagesize=<"%inpath%\%infile%-pagesize"
set /p kerneloff=<"%inpath%\%infile%-kerneloff"
set /p ramdiskoff=<"%inpath%\%infile%-ramdiskoff"
set /p tagsoff=<"%inpath%\%infile%-tagsoff"

mkbootimg --kernel "%inpath%\%infile%-zImage" --ramdisk "%inpath%\%infile%-ramdisk.gz" --cmdline "%cmdline%" --board "%board%" --base "%base%" --pagesize "%pagesize%" --kernel_offset "%kerneloff%" --ramdisk_offset "%ramdiskoff%" --tags_offset "%tagsoff%" -o "%img%"
if errorlevel 1 goto error

exit 0
:error
echo %0 %* -^> failed
exit 1
