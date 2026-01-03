@echo off

rem All tools must be either in the path or in the current directory!
rem The dump directory is generated under the current directory.

if "%1"=="" goto usage

echo Processing %1

NBSplit -hermes %1

if errorlevel 1 goto error

ImgfsFromNb %1.payload imgfs.bin

if errorlevel 1 goto error

del %1.payload
del %1.extra

ImgfsToDump imgfs.bin

if errorlevel 1 goto error

del imgfs.bin

ren dump %1.dump

echo Done. The files are in %1.dump

goto:eof


:usage
echo %0 filename.nb
echo Dumps the files from the IMGFS partition inside 'filename.nb' 
echo to 'filename.nb.dump'.
echo This is the Hermes version.
goto:eof

:error
echo ---> Error <---
echo An error occured while processing '%1'.
echo Processing was aborted; Any generated files are probably unusable!
goto:eof
