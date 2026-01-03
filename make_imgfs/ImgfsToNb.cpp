#include <stdio.h>
#include <windows.h>

#include "..\common\imgfs.h"
#include "..\common\fls.h"

static const BYTE IMGFS_GUID[]={0xF8, 0xAC, 0x2C, 0x9D, 0xE3, 0xD4, 0x2B, 0x4D, 
						 0xBD, 0x30, 0x91, 0x6E, 0xD8, 0x4F, 0x31, 0xDC };


// returns the size of a sector, in bytes; unfortunately, we have to search for the MSFLSH header here
unsigned int sectorSize(BYTE *Base, unsigned int Size)
{
	static unsigned int bytesPerSector = 0;  // default;

    if(bytesPerSector == 0)
	{
		static const char signature[] = "MSFLSH50";
		unsigned int i;

		// find MSFLSH50 header
		for(i = 0; i < Size - sizeof(signature); i+=0x100)  // assumption: MSFLSH always starts at a sector border, and sectors size is always a multiple of 0x100
		{
			if(memcmp(Base+i, signature, sizeof(signature)) == 0)  
				break;
		}

		if(i < Size - sizeof(signature))
		{
    		PFlashLayoutSector p = (PFlashLayoutSector)(Base + i);
			PFlashRegion pRegion = (PFlashRegion)((BYTE *)(p + 1) + p->cbReservedEntries); // Flash Region entries start right behind reserved entries
			
			// assumption: all region entries have same sector size value, so just read from the first one
			bytesPerSector = pRegion->dwBytesPerBlock/pRegion->dwSectorsPerBlock;

			printf("\nSector size is 0x%x bytes\n", bytesPerSector);  // just to allow some visual control
		}
	}
	return bytesPerSector;
}

	/**
     * Returns the number of (virtual) heads used in the partition table
	 *
	 * The general formula to convert between CHS (a.k.a. track/head/sector) and LBA blocks (a.k.a. sectors) is
	 *
     *     sectors = (track * heads + head) * sectors/track + sector -1
	 *
	 * where
	 *  sectors: LBA sector number
	 *  heads: number of heads. This varies between devices!
	 *  sectors/track: so far, all HTC devics I saw use 1 for this. So:
	 *
	 *  -> heads = (sectors - head - sector +1)/track
	*/
static unsigned int getHeads(PPARTENTRY part)
{
	static unsigned int heads = 0;

	if(!heads)
	{
		for(int i = 0; (i < 4) && !heads; i++)
		{
	        if(part[i].Part_FileSystem && part[i].Part_FirstTrack)
			   heads = (part[i].Part_StartSector - part[i].Part_FirstHead - part[i].Part_FirstSector + 1)/part[i].Part_FirstTrack;
		}
	}

	return heads;
}


/**
 * Updates the partiton table entry to which p points with the given data.
 *
 * Algorithm to determine CHS (a.k.a. track/head/sector):
 *
 * Formula: sectors = (track * heads + head) * sectors/track + sector -1 with sectors/track = 1
 *
 * - FirstHead is always 0, LastHead is always heads-1
 * - FirstSector and LastSector are always 1, plus bits 8 and 9 of track in their bits 6 and 7
 * - track = sectors - head - sector + 1)/heads
 *
 * Note that track has 10 bits. The top two are bits 6 and 7 of the sector fields!
 */
static void patchPartitionEntry(PPARTENTRY p, DWORD start, DWORD sectors, unsigned int heads)
{
	DWORD end = start + sectors -1;     // last used sector

	printf("\n\nPartition entry before:\n");
	printf(" File System:    0x%02x\n", p->Part_FileSystem);
	printf(" Start Sector:   0x%08x\n", p->Part_StartSector);
	printf(" Total Sectors:  0x%08x\n", p->Part_TotalSectors);
	printf(" Boot indicator: 0x%02x\n", p->Part_BootInd);
	printf(" First Head:     0x%02x\n", p->Part_FirstHead);
	printf(" First Sector:   0x%02x\n", p->Part_FirstSector & 0x3F);  // sector has only 6 bits
	printf(" First Track:    0x%02x\n", p->Part_FirstTrack + ((p->Part_FirstSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track
	printf(" Last Head:      0x%02x\n", p->Part_LastHead);
	printf(" Last Sector:    0x%02x\n", p->Part_LastSector & 0x3F);  // sector has only 6 bits
	printf(" Last Track:     0x%02x\n", p->Part_LastTrack + ((p->Part_LastSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track
	

	p->Part_StartSector = start;
	p->Part_TotalSectors = sectors;

	p->Part_FirstHead   = (BYTE)0;
	p->Part_FirstTrack  = (BYTE)(start/heads);
	p->Part_FirstSector = (BYTE)(1 + (((start/heads) >> 2) & 0xC0));

	p->Part_LastHead   = (BYTE)(heads - 1);
	p->Part_LastTrack  = (BYTE)((end - p->Part_LastHead)/heads);
	p->Part_LastSector = (BYTE)(1 + ((((end - p->Part_LastHead)/heads) >> 2) & 0xC0));

	printf("Partition entry after:\n");
	printf(" File System:    0x%02x\n", p->Part_FileSystem);
	printf(" Start Sector:   0x%08x\n", p->Part_StartSector);
	printf(" Total Sectors:  0x%08x\n", p->Part_TotalSectors);
	printf(" Boot indicator: 0x%02x\n", p->Part_BootInd);
	printf(" First Head:     0x%02x\n", p->Part_FirstHead);
	printf(" First Sector:   0x%02x\n", p->Part_FirstSector & 0x3F);  // sector has only 6 bits
	printf(" First Track:    0x%02x\n", p->Part_FirstTrack + ((p->Part_FirstSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track
	printf(" Last Head:      0x%02x\n", p->Part_LastHead);
	printf(" Last Sector:    0x%02x\n", p->Part_LastSector & 0x3F);  // sector has only 6 bits
	printf(" Last Track:     0x%02x\n", p->Part_LastTrack + ((p->Part_LastSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track
}

// Start here...
int main(int argc, char **argv)
{
	enum { CONSERVATIVE, BIGSTORAGE, SUPERSTORAGE } mode = BIGSTORAGE;

	printf("ImgfsToNb 2.2\n");

	if(argc < 4)
	{
		printf("Usage: %s <imgfs.bin> <os-in.nb.payload> <os-out.nb.payload> [-conservative]\n"
			   "Combines <imgfs.bin> and <os-in.nb.payload> into <os-out.nb.payload>\n", argv[0]);
		return 2;
	}

	/**
	 * mode can be one of:
	 * - conservative: ImgFs partition is not resized; IMGFS header inside partition is not moved
	 * - bigstorage (the default):  ImgFs partition reduced to minimum size; IMGFS header moved to start of partition
	 * - superstorage: Like bigstorage, but extends Storage partition to end of ExtROM.
	 */
	if(argc > 4)
	{
		if(!strcmp(argv[4],"-conservative")) 
		{
			mode = CONSERVATIVE;
		}
		else if(!strcmp(argv[4],"-superstorage")) 
		{
			mode = SUPERSTORAGE;
		}
	}

	switch(mode) {
		case CONSERVATIVE: printf("Using conservative mode\n"); break;
		case BIGSTORAGE:   printf("Using bigstorage mode\n"); break;
		case SUPERSTORAGE: printf("Using superstorage mode\n"); break;
		default:           printf("Unknown mode. Aborting\n"); return 1;
	}


	// Idee (bepe): minimale und maximale Größe der resultierenden Datei angeben.
	// (ImgFS? nb vor, oder nb nach dem split?)

	// open both input files
    HANDLE inImgfsFile = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if(inImgfsFile == INVALID_HANDLE_VALUE)
	{
		printf("Input file %s cannot be opened. Exiting.\n", argv[1]);
		return 1;
	}
    DWORD inImgfsSize = GetFileSize(inImgfsFile, 0);

	HANDLE inNbFile = CreateFile(argv[2], GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if(inNbFile == INVALID_HANDLE_VALUE)
	{
		printf("Input file %s cannot be opened. Exiting.\n", argv[2]);
		return 1;
	}
    DWORD inNbSize = GetFileSize(inNbFile, 0);
	
	if(inNbSize < 512)
	{
		printf("Input file %s is too small. Exiting.\n", argv[2]);
		return 1;
	}

	// Set up our workspace memory (for simplicity: combined filesize of both input files, as this is the worst case,
	// and I hope the Windows memory manager is clever enough not to allocate physical memory for all of it :-)
	HANDLE heap = HeapCreate(0, 0, 0);
	BYTE * Base = (BYTE *)HeapAlloc(heap, 0, inImgfsSize + inNbSize + 1024 * 1024);  // 1 MByte for padding 

	if(!heap || !Base)
	{
		printf("Out of memory. Exiting.\n");
		HeapDestroy(heap);
		return 1;
	}

	DWORD read;

	ReadFile(inNbFile, Base, inNbSize, &read, 0);
	CloseHandle(inNbFile);

	if(read != inNbSize)
	{
		printf("Input file %s cannot be read. Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}

	// Look into partition table, verify that there's an IMGFS partition and that it extends until the end of the file
	PPARTENTRY partTable = (PPARTENTRY)(Base + 0x1be);
	int imgfsPartIndex = -1;

    for(unsigned int i = 0; i < 4; i++)
		if(partTable[i].Part_FileSystem == 0x25)
			imgfsPartIndex = i;

    if(imgfsPartIndex == -1)
	{
		printf("No IMGFS partition found in partition table in %s. Exiting.\n", argv[2]);
		HeapDestroy(heap);
		return 1;
	}

	for(unsigned int i = imgfsPartIndex + 1; i < 4; i++)
	{
		if(partTable[i].Part_FileSystem != 0x04 && partTable[i].Part_FileSystem != 0x00 && partTable[i].Part_FileSystem != 0xFF)
		{
			printf("Partition with file system 0x%02x found after IMGFS partition. It's safer to abort here, sorry.\n", partTable[i].Part_FileSystem);
			HeapDestroy(heap);
			return 1;
		}
	}

    unsigned int bytesPerSector = sectorSize(Base, inNbSize);
	unsigned int heads = getHeads(partTable);

	// the following three are offsets relative to Base, in bytes
	unsigned int imgfsStart = partTable[imgfsPartIndex].Part_StartSector * bytesPerSector;
	unsigned int imgfsEnd = (partTable[imgfsPartIndex].Part_StartSector + partTable[imgfsPartIndex].Part_TotalSectors) * bytesPerSector;
	unsigned int imgfsHeaderOffset; // in bytes, from Base

    // find first occurrence of IMGFS GUID in ImgFs partition
	if(mode == CONSERVATIVE)  // this mode does not move the IMGFS header
	{
		for(imgfsHeaderOffset = imgfsStart; imgfsHeaderOffset < imgfsEnd - sizeof(IMGFS_GUID); imgfsHeaderOffset++)
		{
    		if(memcmp(IMGFS_GUID, Base+imgfsHeaderOffset, sizeof(IMGFS_GUID))==0)
				break;
		}

		if(imgfsHeaderOffset >= imgfsEnd - sizeof(IMGFS_GUID))
		{
			printf("Conservative mode: no IMGFS signature found. Exiting.\n");
			return 1;
		}
	}
	else // Move Imgfs Header to start of ImgFs partition
	{
		imgfsHeaderOffset = imgfsStart;
	}
	
	printf("Writing imgfs to offset byte 0x%x, sector 0x%x\n", imgfsHeaderOffset, imgfsHeaderOffset/bytesPerSector);


	// write complete imgfs.bin file to imgfsHeaderOffset
	ReadFile(inImgfsFile, Base + imgfsHeaderOffset, inImgfsSize, &read, 0);
	CloseHandle(inImgfsFile);

	if(read != inImgfsSize)
	{
		printf("Input file %s cannot be read. Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}


	// pad image so that inImgfsSize is a multiple of 'sectors' * 'heads'
    unsigned int oldInImgfsSize = inImgfsSize;
	unsigned int sectors = (inImgfsSize - 1)/bytesPerSector;  // -1 to make special case where inImgfsSize is already a multiple of heads * sectors a normal case :-)
 	sectors = (sectors + heads) & ~(heads-1);

	inImgfsSize = sectors * bytesPerSector;

	printf("Padding ImgFs from 0x%x bytes (0x%x sectors)\n"
		   "                to 0x%x bytes (0x%x sectors)\n", oldInImgfsSize, oldInImgfsSize/bytesPerSector, inImgfsSize, inImgfsSize/bytesPerSector);

    // fill padding area with 0xFF to mark it as empty (so NBMerge can mark these sectors as free)
    BYTE * p = Base + imgfsHeaderOffset; 
	for(unsigned int i = oldInImgfsSize; i < inImgfsSize; i++)
	{
		p[i] = 0xFF;
	}

	// calculate new end of imgfs partition, save in imgfsEnd
	// in conservative mode, don't alter (shrink or enlarge) the imgfs partition!
	if(mode != CONSERVATIVE)
	{
		unsigned int o = imgfsEnd;
		imgfsEnd = imgfsHeaderOffset + inImgfsSize;
		printf("Not conservative mode. Changing imgfsEnd from 0x%x to 0x%x\n", o, imgfsEnd );
	}
	else
	{
        if(imgfsEnd < imgfsHeaderOffset + inImgfsSize)
		{
			printf("Conservative mode: imgfs partition overflow! Aborting!\n" 
				"available sectors: 0x%x, needed sectors: 0x%x", (imgfsEnd - imgfsHeaderOffset)/bytesPerSector, inImgfsSize/bytesPerSector);
			return 1;
		}
		else
		{
			printf("Conservative mode. Not changing imgfsEnd\n" );
		}
	}

	// patch the IMGFS partition table entry
    patchPartitionEntry(partTable + imgfsPartIndex, imgfsStart/bytesPerSector, (imgfsEnd - imgfsStart)/bytesPerSector, heads);

	// patch the Storage partition table entry
    if(partTable[imgfsPartIndex+1].Part_FileSystem == 0x04)
	{
		DWORD storagePartIndex = imgfsPartIndex+1;

   	    // Storage starts right behind IMGFS. Note that we use the new values written by the call to patchPartitionEntry above!
		DWORD storageStart = partTable[imgfsPartIndex].Part_StartSector + partTable[imgfsPartIndex].Part_TotalSectors;
		
		// size is old start + old size - new start; this ensures end is unchanged!
		DWORD storageSize = partTable[storagePartIndex].Part_StartSector + partTable[storagePartIndex].Part_TotalSectors - storageStart;

		// Patch partition table for Superstorage. Hermes only!
		if(mode == SUPERSTORAGE)
		{  /* bepe writes (2007-08-31): 
				some more details on how to do this...

				FLASHDRV.DLL S000 needs to be changed:
				"58 20 43" to "04 20 43" (only once)
				All "58 30 43" to "04 30 43" (3 times)

				OS.nb:
				at 0x000001F5 add 0x48 to the current value
				(e.g. A7 00 + 0x48 = EF 00)
				same at 0x000001FB
				DS: 0x48 are added to the track number, i.e. 0x4800 are added to the sector number.
				Why only 0x4800 and not 0x5550? 0x54 sounds much more reasonable. bepe agrees. */

			// Plausibility check: storage end must be below sector 0x40000, otherwise 
			// superstorage would actually make storage *smaller*
            if(storageStart + storageSize > 0x40000)
			{
				printf("Error: Device doesn't seem to be a Hermes. Not creating superstorage!\n");
			}
			else
			{
				/**
				  Extend storage almost to end of ROM (which is at sector 0x40000). A normal ExtROM has
				  0x5550 sectors, which leaves 0x260 sectors free if ExtRom starts right back to back with Storage.
				  I don't know why there are 0x260 sectors left free, but it seems safer to also leave them
				  alone. Additionally, the patch in FLASHDRV.DLL seems to hint we should leave
				  4 blocks free (58 becomes 04), which is 0x400 sectors.
				*/
				storageSize = 0x40000 - storageStart - 0x400;
				printf("Enlarging storage to 0x%x blocks.\n"
					   "New storage end is now at 0x%08x bytes\n"
					   "WARNING: you also *must* patch FLASHDRV.FLL!", storageSize, (storageStart + storageSize) * 0x200);
			}
		}

        patchPartitionEntry(partTable + storagePartIndex, storageStart, storageSize, heads);
    }
	
	// find and patch MSFLSH50 header
	static const char signature[] = "MSFLSH50";
	unsigned int i;

	// find MSFLSH50 header
	for(i = 0; i < inNbSize - sizeof(signature); i+=0x100)  // assumption: MSFLSH always starts at a sector border, and sectors size is always a multiple of 0x100
	{
		if(memcmp(Base+i, signature, sizeof(signature)) == 0)  
			break;
	}

	if(i < inNbSize - sizeof(signature))
	{
		PFlashLayoutSector p = (PFlashLayoutSector)(Base + i);
		PFlashRegion pRegion = (PFlashRegion)((BYTE *)(p + 1) + p->cbReservedEntries); // Flash Region entries start right behind reserved entries
		
        // find ImgFs Flash Region
		for(i = 0; i < p->cbRegionEntries/sizeof(FlashRegion); i++)
		{
			if(pRegion[i].regionType == READONLY_FILESYS) // this is the ImgFs Flash Region
				break;
		}

		if(i < p->cbRegionEntries/sizeof(FlashRegion))  // we have an ImgFs Flash Region
		{
			unsigned int old_dwNumLogicalBlocks = pRegion[i].dwNumLogicalBlocks;
			pRegion[i].dwNumLogicalBlocks = partTable[imgfsPartIndex].Part_TotalSectors / pRegion[i].dwSectorsPerBlock;
			printf("ImgFs Flash Region log blocks was 0x%x, now is 0x%x\n", old_dwNumLogicalBlocks, pRegion[i].dwNumLogicalBlocks);

			if(++i < p->cbRegionEntries/sizeof(FlashRegion)) // patch next region entry, if exists
			{
     			printf("Storage Flash Region log block was 0x%x,", pRegion[i].dwNumLogicalBlocks);
				if(pRegion[i].dwNumLogicalBlocks != END_OF_FLASH)
					pRegion[i].dwNumLogicalBlocks += old_dwNumLogicalBlocks - pRegion[i-1].dwNumLogicalBlocks; // add as many blocks as preceding Flash Region shrunk
     			printf(" now is 0x%x,", pRegion[i].dwNumLogicalBlocks);
			}
			else
			{
				printf("No Storage Flash Region found!\n");
			}
		}
		else
			printf("Strange, no ImgFs Flash Region found!\n");
	}


    // dump what we built in memory map to file
    HANDLE outFile = CreateFile(argv[3], GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    if(outFile == INVALID_HANDLE_VALUE)
	{
		printf("Output file %s cannot be opened. Exiting.\n", argv[3]);
		HeapDestroy(heap);
		return 1;
	}

	DWORD written;
	WriteFile(outFile, Base, imgfsEnd, &written, 0);

	if(written != imgfsEnd)
	{
		printf("Error writing to output file %s. DO NOT USE IT! Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}

	CloseHandle(outFile);
    HeapDestroy(heap);

	printf("\nDone!\n");
	return 0;
}
