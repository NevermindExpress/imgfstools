
                   ImgFS Tools Version 2
                   
================================================================
                 (Current version: 2.1rc2)


What it is
==========

These little programs help you with cooking your own ROM. They allow you to unpack an OS.nb file into individual files as you'd find them on your device, and to re-pack them into a new OS.nb which you can flash to your device. Of course, betweem unpacking and re-packing, you'll want to add/modify/remove some of them. :-)

The tools are enhanced versions of mamaich's imgfs_tools - a huge Thank You goes to mamaich for creating them, and also for releasing the source code!

The most significant enhancement is that the IMGFS inside the generated OS.nb is no longer of fixed size, but is as small as possible. This means: the more of the unpacked files you remove before re-packing, the smaller it'll get and consequently the bigger your Storage memory will become. Also, the tools are no longer device-specific and should also work other devices than the Hermes.

*** WARNING: ***
Before you flash a ROM created with these tools, make sure you know how to recover from a non-booting ROM! For Hermes and Kaiser, you should install a HardSPL before you use these tools! I accept no responsibility for bricked devices!


Supported devices
=================
These tools have been tested on HTC devices only, and are known to work on Hermes, Kaiser, Titan and Artemis. Other devices were not tested and may or may not work.
If you want to try, I will gladly support you. However, I cannot accept responsibility for bricked devices!


How it works
============

On a high level, this is the chain of actions needed to unpack OS.nb:

   RUU-signed.nbh ---(*NBHExtract)---> OS.nb ---(NBSplit)---> OS.nb.payload -->
   --(ImgfsFromNb)---> imgfs.bin ---(ImgfsToDump)---> 'dump' directory

At that stage, you will want to edit the 'dump' directory. I recommend using bepe's excellent *Package Tool for this.

The reverse process is:

   'dump' directory ---(ImgfsFromDump)---> imgfs-new.bin ---(ImgfsToNb)---> 
   OS-new.nb.payload ---(NBMerge)---> OS-new.nb ---(*NBHGen)---> 
   RUU-signed-new.nbh

(The tools marked with '*' are not part of the ImgfsTools, but are also available for free from xda-developers.com. There is also one additional tool, NBInfo, in this package.)


How it works in detail
======================
Note: the given file names are examples. You are free to use any name you like.


NBSplit
-------
Usage: 

 NBSplit -hermes|-kaiser|-titan|-wizard|-athena|-sp <OS.nb>

or

 NBSplit -data <payload-chunk-size> -extra <extra-chunk-size> <OS.nb>

An OS.nb file contains some device-specific additional data. This tool separates the device-specific from the device-UNspecific data. The device-specific part goes to <OS.nb.extra>, the device-unspecific to <OS.nb.payload> (sorry, couldn't think of a better suffix :-))

For Wizard and Athena, the program does nothing, since the ROMs for these devices do not contain extra data. I added them nonetheless because so many people asked me how my tools can be made to work with these devices. Well, for these devices simply copy OS.nb to OS.nb.payload and continue normally. :-)

The -data and -extra options are for future devices for which there is not yet explicit support built into NBSplit: when a new device appears on the market, it's easy to find out the sector size and extra data size, and so NBSplit can be used right away (hopefully).
As an example, for Hermes the payload-chunk-size is 0x200, the extra-chunk-size is 0x08. For Kaiser it is 0x800 and 0x08.


ImgfsFromNb
-----------
Usage: ImgfsFromNb <OS.nb.payload> <imgfs.bin>

<OS.nb.payload> still contains a lot more than just the IMGFS section we are after. This tool extracts the IMGFS part and writes it to <imgfs.bin>.
This used to be prepare_imgfs.exe, and the output file name used to be hard-coded to imgfs_raw_data-bin.


ImgfsToDump
-----------
Usage: ImgfsToDump <imgfs.bin>

Extracts all files and modules from the given imgfs file into a 'dump' subdirectory under the current directory. If RecMod.exe is in the same directory as this tool, then all modules are also reconstructed as working DLL or EXE and placed in the module's directory. Additionally, a file 'dump_MemoryMap.txt' is created containing the address ranges for the modules.
This used to be viewimgfs.exe.


ImgfsFromDump
-------------
Usage: ImgfsFromDump <imgfs.bin> <imgfs-new.bin>

Creates <imgfs-new.bin> from the 'dump' subdirectory. The file <imgfs.bin>, which must exist before calling this tool, is used *only* to read the IMGFS header to be used in the output. It will not get overwritten.
The resulting IMGFS file will be as small as possible. The IMGFS will have no free (i.e., wasted) sectors.
The maximum allowed size for an IMGFS partition was increased to 128 MByte, and the tool now also does extensive error checking.
This used to be BuildImgfs.exe.


ImgfsToNb
---------
Usage: 

   ImgfsToNb <imgfs-new.bin> <os.nb.payload> <os-new.nb.payload>
   
or

   ImgfsToNb <imgfs-new.bin> <os.nb.payload> <os-new.nb.payload> -conservative
   

Combines <imgfs-new.bin> and <os.nb.payload> (both must exist before calling this tool) into <os-new.nb.payload>.
This tool copies all data except the IMGFS partition from <os.nb.payload> to <os-new.nb.payload>, then adds the IMGFS partition from <imgfs-new.bin>.

If -conservative is not given, it also patches the partition table and MSFLSH flash region table to match the new IMGFS size. In other words: the more stuff you removed from the 'dump' directory, the smaller your IMGFS partition and the bigger your Storage partition will be.

In -conservative mode, the partition table and MSFLSH flash region table are left untouched. Your Storage partition will not increase. This mode should only be used in special cases.

<os.nb.payload> should be the file on which you ran ImgfsToDump earlier in the process. It will not get overwritten.
This used to be make_imgfs.exe.


NBMerge
-------
Usage: 

  NBMerge -hermes|-kaiser|-titan|-sp|-wizard|-athena <os-new.nb> or
  NBMerge -hermes|-kaiser|-titan|-sp|-wizard|-athena <os-new.nb> -conservative
  
or  

  NBMerge -data <payload-chunk-size> -extra <extra-chunk-size> <os-new.nb> or
  NBMerge -data <payload-chunk-size> -extra <extra-chunk-size> <os-new.nb> -conservative

IMPORTANT NOTE: 
If you ran ImgfsToNb with -conservative, you also *must* use -conservative with NBMerge. Similarily, if you ran ImgfsToNb without -conservative, you *must not* use -conservative with NBMerge!

Adds back in the device-specific extra data, creating a properly formatted <os-new.nb> file. The names of the input files are derived from the output file name - for example, if you call NBMerge with os-new.nb, then os-new.nb.payload is used as input file.

Just like NBSplit, NBMerge does nothing for Wizard and Athena.

The -data <numer> -extra <number> format is explained above, under NBSplit.

If -conservative is not given, the extra data is generated internally. (This currently only works for devices which have an extra-chunk-size of 8, like Hermes, Kaiser, and Titan.) In this case, <os-new.nb.extra> is not needed.

In -conservative mode, <os-new.nb.extra> must exist.

As the last step of creating <os-new.nb>, the resulting file is checked for bad NAND block markers (only for Hermes, Kaiser, and Titan). If any are found, a warning is printed and an errorlevel > 0 is returned. DO NOT IGNORE THIS WARNING! 



NBInfo
------
Usage: NBInfo <os.nb.payload>

Outputs information taken from the given file. In particular, the partition table and the MSFLSH header is dumped, and the file is searched for IMGFS signatures.

This is actually a very useful tool for plausability checks. If you have problems processing a .nb file, run the .nb.payload through NBInfo, and try to understand the output (or email me the output).

Note that you should run it on a .payload file, not on the original .nb!


AddFile, DelFile
----------------
These tools are no longer supported.



General notes
=============
- all tools now return an errorlevel. This is 0 in case the tool succeeded, and some value > 0 in case of any error. This makes it very easy to use them in batch processing.

- To get a short help, just call any tool without parameters. 

- none of the tools overwrite any of their input files.

- except for the 'dump' directory, no file names or paths are hard-coded into any of the tools. Use whatever file names you like.

- except for NBSplit and NBMerge, all tools are device independent, i.e. they should work not only for Hermes and Kaiser, but for other devices as well.

- source code for these tools is available on request.


Support
=======
If you encounter problems, please email me at imgfs@tadzio.com. Note that I can *only* help with these tools. For general questions about cooking ROMs, please consult the forum and Wiki at www.xda-developers.com.


Version History
===============

2007-10-17  2.1rc2
ROMs generated by 2.1RC1 would not boot if created with a certain combination of parameters. Fixed.


2007-10-14  2.1RC1
Major overhaul:
- All Hermes-specific assumptions (like the sector size being always 0x200) are now gone. 
- "conservative" mode added to ImgfsToNb and NBMerge
- on devices that have only two MSFLSH flash regions, ImgfsToNb overwrote a few bytes in the XIP section, causing unpredictable results. This was fixed.
- Removed "-emu" support from NBSplit and NBMerge, as this didn't work with WM6 emulator images.


2007-03-18  2.0 RC 2
All tools now linked statically to the MS runtime library, no longer needs DLL. No other changes.

2007-03-18  2.0 RC 1 
Initial Release. Starting with version 2 to not collide with mamaich's version numbers.

