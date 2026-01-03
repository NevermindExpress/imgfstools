@echo off

rem All tools must be either in the path or in the current directory!
rem The dump directory is generated under the current directory.

if "%1"=="" goto usage

echo Processing %1

rem We need our templates again

NBSplit -kaiser %1
del %1.extra

if errorlevel 1 goto error

ImgfsFromNb %1.payload imgfs.old.bin

if errorlevel 1 goto error

rename %1.payload %1.old.payload

rem Okay, rock'n'roll

rename %1.dump dump

ImgfsFromDump imgfs.old.bin imgfs.bin

if errorlevel 1 goto error

rename dump %1.dump

del imgfs.old.bin

ImgfsToNb imgfs.bin %1.old.payload %1.payload

if errorlevel 1 goto error

del %1.old.payload
del imgfs.bin

if exist %1 rename %1 %1.old

NBMerge -kaiser %1

if errorlevel 1 goto error

del %1.payload

echo Done.

goto:eof


:usage
echo %0 filename.nb
echo Creates 'filename.nb' from the dump directory 'filename.nb.dump'
echo 'filename.nb' must exist and will be renamed to 'filename.nb.old'
echo This is the Kaiser version.
goto:eof

:error
echo ---> Error <---
echo An error occured while processing '%1'.
echo Processing was aborted; Any generated files are probably unusable!
goto:eof