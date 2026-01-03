
#include <stdio.h>
#include <windows.h>
#include "..\common\imgfs.h"

#define PAY_EXT ".payload"
#define EXTRA_EXT ".extra"


// returns true if the given sector is free (i.e., all bytes are 0xFF), false otherwise
static bool isFree(BYTE * Base, unsigned int sec, unsigned int payloadChunkSize, unsigned int extraChunkSize)
{
	// not very quick, but simple
	for(unsigned int i = 0; i < payloadChunkSize; i++)
		if(*(Base + sec *(payloadChunkSize + extraChunkSize) + i) != 0xFF)
			return false;

	return true;
}



int main(int argc, char *argv[])
{
	printf("NBMerge 2.1rc2\n");

	if(argc < 3)
	{
		printf("Usage: %s -hermes|-kaiser|-titan|-sp|-wizard|\n"
			"   -athena|-data <number> -extra <number> <filename.nb> [-conservative]\n"
			   "Puts the extra sector data back into <filename.nb.payload>, writing the\n"
			   "result to <filename.nb>. If -conservative, <filename.nb.extra> must exist.\n", argv[0]);
		return 1;
	}

    unsigned int payloadChunkSize = 0, extraChunkSize = 0;  // safe default
	bool checkNAND = false;  // On some devices (Hermes), we can check that every (payloadChunkSize + 5)th byte is 0xFF

	if(!strcmp(argv[1], "-wizard") || !strcmp(argv[1], "-athena"))
	{
		printf("Wizard and Athena ROMs do not contain extra bytes.\n"
			   "You need not run %s.\n", argv[0]);
		return 0;
	}

    // payload and extra chunk sizes explicitly given
	if(argc > 5 && !strcmp(argv[1], "-data") && !strcmp(argv[3], "-extra"))  // needs nbmerge -data xxx -extra yyy <filename.nb>
	{
        payloadChunkSize = atoi(argv[2]);
    	extraChunkSize = atoi(argv[4]);
	}

	if(!strcmp(argv[1], "-hermes") || !strcmp(argv[1], "-sp"))
	{
        payloadChunkSize = 0x200; extraChunkSize = 0x08;
	    checkNAND = true;
     }

	if(!strcmp(argv[1], "-titan") || !strcmp(argv[1], "-kaiser"))
	{
        payloadChunkSize = 0x800; extraChunkSize = 0x08;
	    checkNAND = true;
	}

#if 0  // Does not work with WM6 emulator images
	if(!strcmp(argv[1], "-emu"))
	{
        payloadChunkSize = 0xF000; extraChunkSize = 0x1000;
	}
#endif

	if(payloadChunkSize == 0)  // no matching argument given
	{
		printf("No supported device specified.\n");
		printf("Usage: %s -hermes|-kaiser|-titan|-sp|-wizard|\n"
			"   -athena|-data <number> -extra <number> <filename.nb> [-conservative]\n"
			   "Puts the extra sector data back into <filename.nb.payload>, writing the\n"
			   "result to <filename.nb>. If -conservative, <filename.nb.extra> must exist.\n", argv[0]);
		return 1;
	}

    char * filearg = argv[argc-1];  // filearg is the last or last but one (if -conservative) argument
	if(!strcmp(filearg, "-conservative")) filearg = argv[argc-2];

	char * filename = (char *)malloc(strlen(filearg) + strlen(PAY_EXT) + strlen(EXTRA_EXT));

	if(!filename)
	{
		printf("Error allocating memory\n");
		return 1;
	}

	FILE * nb, *payload, *extra;

	sprintf(filename, "%s%s", filearg, PAY_EXT);

	if(NULL == (payload = fopen(filename, "rb")))
	{
		printf("Could not open input file %s\n", filename);
		free(filename);
		return 1;
	}

	// use filename.nb.extra only in -conservative mode
	if(!strcmp(argv[argc-1], "-conservative"))
	{
		sprintf(filename, "%s%s", filearg, EXTRA_EXT);

		if(NULL == (extra = fopen(filename, "rb")))
		{
			printf("Conservative mode: could not open input file %s. Aborting.\n", filename);
			free(filename);
			return 1;
		}
	}
	else  // not conservative mode
		extra = NULL;

	free(filename);

	if(NULL == (nb = fopen(filearg, "wb")))
	{
		printf("Could not create output file %s. Does it already exist?\n", filearg);
		return 1;
	}

	printf("Executing %s with data chunk size = 0x%x and extra chunk size = 0x%x\n"
		   "on file %s\n", argv[0], payloadChunkSize, extraChunkSize, filearg );

	// handle the case where we can read our extra bytes from a file.
	if(extra)
	{
		BYTE * payloadBuffer = (BYTE *)malloc(payloadChunkSize);
		BYTE * extraBuffer = (BYTE *)malloc(extraChunkSize);

		if(!payloadBuffer || !extraBuffer)
		{
			printf("Out of memory allocating buffers. Aborting.\n");
			fclose(nb);
			return 1;
		}

		do
		{
			size_t bytes = fread(payloadBuffer, 1, payloadChunkSize, payload);
			if(bytes != fwrite(payloadBuffer, 1, bytes, nb))
			{
				printf("Error writing payload to nb file. Generated files are unusable!\n");
				fclose(nb);
				return 1;
			}

			if(bytes != payloadChunkSize) break;  // we read less than a full payload chunk. Seems we reached the end.

			// we read less than the needed number of extra bytes, i.e file too small
			if(extraChunkSize != fread(extraBuffer, 1, extraChunkSize, extra))
			{
				printf("Error reading from extra file, probably too small. Generated files are unusable!\n");
				fclose(nb);
				return 1;
			}

			if(extraChunkSize != fwrite(extraBuffer, 1, extraChunkSize, nb))
			{
				printf("Error writing extra to nb file. Generated files are unusable!\n");
				fclose(nb);
				return 1;
			}
		}
		while(true);
	}

	else if(extraChunkSize != 8) // we currently do not know how to generate extra data of other sizes than 8
	{
		printf("No extra data file, and don't know how to generate extra data. Generated files are unusable!\n");
		fclose(nb);
		return 1;
	}

	else  // generate 8-byte (two words: counter and magic) extra data
	{
		/**
		 * This is non-trivial. Requirements:
		 * - sectors outside a partition use magic 0xfffbfffd
		 * - boot and XIP partitions use magic 0xfffbfffd
		 * - imgfs partition uses magic 0xfffbffff
		 * - free sectors use magic 0xffffffff
		 * - free sectors can only occur at start or end of a partition
		 * - the counter value does not increment for free blocks at the start of a partition(!)
		 *
		 * Algorithm (a bit heavy on RAM, but rather simple):
		 * - allocate memory for payload plus extra bytes
		 * - read payload file into memory, leaving space for extra bytes. Fill extra space with 0xff ("free sector") as default.
		 * - read partition table.
		 * - for each partition: find first and last used block, and store that info. Also store the magic for that partition.
		 * - write magic and sector numbers from start to first partition
		 * - for each partition: write sector numbers and magic from first used to last used block. Start wih sector number read from partition table for first sector of partition, even if that's a free sector (sector mapping!)
		 * - dump complete buffer to <filename.nb>
		 **/

		// determine payload size, allocate memory
		fseek(payload, 0L, SEEK_END);
		unsigned int sectors = (unsigned int)ftell(payload)/payloadChunkSize;  // number of sectors in our payload file
		fseek(payload, 0L, SEEK_SET);

		unsigned int sectorsize = payloadChunkSize + extraChunkSize;  // in bytes
		BYTE * Base = (BYTE *)malloc(sectors * sectorsize);

		if(!Base)
		{
			printf("Out of memory (need 0x%x bytes). Aborting\n", sectors * sectorsize);
			return 1;
		}

		// read payload data to RAM, leaving extra space
		for(unsigned int i = 0; i < sectors; i++)
		{
			if(payloadChunkSize != fread(Base + i * sectorsize, 1, payloadChunkSize, payload))
			{
				printf("Payload file read error. Aborting\n");
				return 1;
			}
		}

		PPARTENTRY partTable = (PPARTENTRY)(Base + 0x1be);

		// fill extra data for all sectors before first partition
		for(unsigned int sec = 0; sec < partTable[0].Part_StartSector; sec++)
		{
			unsigned int * p = (unsigned int *)(Base + sec * sectorsize + payloadChunkSize);
			p[0] = sec;
			p[1] = 0xfffbfffd;
		}

		// fill extra data for all partitions
		for(int i = 0; i < 4; i++)
		{
			unsigned int magic = (0x25 == partTable[i].Part_FileSystem)? 0xfffbffff : 0xfffbfffd;  // imgfs partition not to be touched by FAL (i.e., read-only)
			unsigned int firstUsed, used;  // first used sector and total number of used sectors
			unsigned int sec;

			// skip unused partitions and partitions which lie outside of the payload file
			if(partTable[i].Part_FileSystem == 0 || partTable[i].Part_StartSector >= sectors)
				continue;

			// find first used (non-free) sector; while doing so, set extra bytes of all free sectors to 0xffffffff
			for(sec = partTable[i].Part_StartSector; sec  < partTable[i].Part_StartSector + partTable[i].Part_TotalSectors; sec++)
			{
				if(isFree(Base, sec, payloadChunkSize, extraChunkSize)) memset(Base + sec * sectorsize + payloadChunkSize, 0xFF, extraChunkSize); 
				else break;  // stop searching
			}

			firstUsed = sec;

			// searching backwards from end of partition, find last used sector; while doing so, set extra bytes of all free sectors to 0xffffffff
			for(sec = partTable[i].Part_StartSector + partTable[i].Part_TotalSectors -1; sec >= firstUsed; sec--)
			{
				if(isFree(Base, sec, payloadChunkSize, extraChunkSize)) memset(Base + sec * sectorsize + payloadChunkSize, 0xFF, extraChunkSize); 
				else break; // stop searching
			}

			used = 1 + sec - firstUsed;

			// set magic of all non-free sectors
			for(sec = firstUsed; sec < firstUsed + used; sec++)
			{
				unsigned int * p = (unsigned int *)(Base + sec * sectorsize + payloadChunkSize);
				p[0] = partTable[i].Part_StartSector + sec - firstUsed; // sector numbering *must* skip free sectors at the start of each partition!
				p[1] = magic;
			}

			printf("Partition %i: start sector: 0x%08x, total: 0x%08x\n"
				   "               first used: 0x%08x, used:  0x%08x\n", i, partTable[i].Part_StartSector, partTable[i].Part_TotalSectors, firstUsed, used);
		}

		// dump file
		if(sectors != fwrite(Base, sectorsize, sectors, nb))
			printf("Error writing to %s. File is unusable!\n", filearg);
	}

	fclose(nb);
	fclose(payload);
	if(extra) fclose(extra);

    // Now, go through the generated file again and check that every 5th extra byte is 0xFF!
	// (Otherwise would cause unrecoverable bad NAND blocks when flashing to Hermes)
	if(checkNAND)
	{
		printf("Checking %s for bad NAND block marker\n", filearg);

		if(NULL == (nb = fopen(filearg, "rb")))
		{
			printf("Can't open file %s for reading... strange! Aborting.\n", filearg);
			return 1;
		}
		
    	BYTE * buffer = (BYTE *)malloc(payloadChunkSize + extraChunkSize);
 
		size_t sectorsize = payloadChunkSize + extraChunkSize;
		int sector = 0;

		while( sectorsize == fread(buffer, 1, sectorsize, nb))
		{
			sector++;
			if(buffer[payloadChunkSize + 5] != 0xFF)
			{
				printf("-------> Found BAD BLOCK marker! <------\nDo not flash this file!\n");
				printf("Sector: 0x%x\n", sector);
				printf(" All extra bytes:\n");
                for(unsigned int i = 0; i < extraChunkSize; i++)
					printf("0x%04x\n", buffer[payloadChunkSize + i]);

				return 1;
			}
		}
		free(buffer);

		printf("Checked 0x%x sectors successfully!\n", sector);
	}

	printf("Done.\n");

	return 0;
}