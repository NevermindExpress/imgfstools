
#include <stdio.h>
#include <windows.h>
#include "..\common\imgfs.h"
#include "..\common\fls.h"



// return the Flash Region Type as a string
static const char * regionName(REGION_TYPE type)
{
	static const char *names[] = {"XIP", "READONLY_FILESYS", "FILESYS"};

	if(type <= 3) return names[type];
	else return "Undefined";
}


static const char * fsName(BYTE fs)
{
	switch(fs)
	{
	case 0x01:
	case 0x04:
	case 0x06:
	case 0x0E:
	case 0x0F:
		return "FAT";

	case 0x05:
		return "extended";

	case 0x0B:
	case 0x0C:
		return "FAT32";

	case 0x18:
		return "hidden";

	case 0x20:
		return "boot";

	case 0x21:
		return "binfs";

	case 0x22:
		return "XIP ROM";

	case 0x23:
		return "XIP RAM";

	case 0x25:
		return "imgfs";

	case 0x26:
		return "raw binary";
	}
	return "unknown";
}


// main entry point
int main(int argc, char *argv[])
{
    printf("NBInfo 2.2\n");

	if(argc < 2)
	{
		printf("Usage: %s <foo.nb.payload>\n"
			   "Outputs information from the given file.\n", argv[0]);
		return 1;
	}

    HANDLE hNbFile = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	if(hNbFile == INVALID_HANDLE_VALUE)
	{
		printf("Could not open input file '%s'. Aborting.\n", argv[1]);
		return 1;
	}

    HANDLE hMem = CreateFileMapping(hNbFile, 0, PAGE_READONLY, 0, 0, 0);

	if(hMem == 0)
	{
		printf("Internal error 1. Aborting.\n");
		CloseHandle(hNbFile);
		return 1;
	}

    BYTE* Base = (BYTE*)MapViewOfFile(hMem, FILE_MAP_READ, 0, 0, 0);

    if (!Base)
	{
		printf("Internal error 2. Aborting.\n");
		CloseHandle(hNbFile);
		return 1;
	}

    DWORD Size = GetFileSize(hNbFile, 0);

    // Essential number, gets properly set when analyzing MSFLSH header
	unsigned int bytesPerSector = 1;

    // Check if boot sector is valid
	if(IS_VALID_BOOTSEC(Base))
		printf("\n'%s' has valid boot sector\n\n", argv[1]);
	else
		printf("\nWarning! '%s' has no valid boot sector! Info below is probably unusable!\n\n", argv[1]);


    // dump partition table
	unsigned int i;

	PPARTENTRY part = (PPARTENTRY)(Base + 0x1be);
	unsigned int heads = 0;

	printf("Partition table:\n");

	for(i=0; i < 4; i++)
	{
    	printf("\nPartition %i\n-----------\n", i);
		printf(" File System:    0x%02x (%s)\n", part[i].Part_FileSystem, fsName(part[i].Part_FileSystem));
		printf(" Start Sector:   0x%08x\n", part[i].Part_StartSector);
		printf(" Total Sectors:  0x%08x\n", part[i].Part_TotalSectors);
		printf(" Boot indicator: 0x%02x\n", part[i].Part_BootInd);
		printf(" First Head:     0x%02x\n", part[i].Part_FirstHead);
		printf(" First Sector:   0x%02x\n", part[i].Part_FirstSector & 0x3F);  // sector has only 6 bits
		printf(" First Track:    0x%02x\n", part[i].Part_FirstTrack + ((part[i].Part_FirstSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track
		printf(" Last Head:      0x%02x\n", part[i].Part_LastHead);
		printf(" Last Sector:    0x%02x\n", part[i].Part_LastSector & 0x3F);  // sector has only 6 bits
		printf(" Last Track:     0x%02x\n", part[i].Part_LastTrack + ((part[i].Part_LastSector & 0xC0) << 2));  // top 2 bits of sector are bits 9 and 10 of track

        if(!heads && part[i].Part_FileSystem && part[i].Part_FirstTrack)
		   heads = (part[i].Part_StartSector - part[i].Part_FirstHead - part[i].Part_FirstSector + 1)/part[i].Part_FirstTrack;
	}
	printf("\nGeometry: flash has %i virtual heads\n", heads);
	
	// find MSFLSH50 header, and show if found
	const char signature[] = "MSFLSH50";

	for(i = 0; i < Size - sizeof(signature); i++)
	{
		if(memcmp(Base+i, signature, sizeof(signature)) == 0)  
			break;
	}

	if(i < Size - sizeof(signature))
	{
        PFlashLayoutSector p = (PFlashLayoutSector)(Base + i);

        printf("\n\n%s header found at offset 0x%x\n", signature, i);
		printf("  (%i Reserved Entries, %i Flash Region Entries)\n", p->cbReservedEntries/sizeof(ReservedEntry), p->cbRegionEntries/sizeof(FlashRegion));

		// dump reserved entries
		PReservedEntry pReserved = (PReservedEntry)(p + 1); // reserved entries start just behind the FlashLayoutSector
		for(unsigned int j = 0; j < p->cbReservedEntries/sizeof(ReservedEntry); j++)
		{
			printf("\nReserved Entry %i:\n", j);
			printf("-----------------\n");
			printf("  Name:                   %s\n", pReserved[j].szName);
			printf("  Start block:            0x%08x\n", pReserved[j].dwNumBlocks);
			printf("  Number of blocks:       0x%08x\n", pReserved[j].dwNumBlocks);
		}

		// dump Flash Region entries
		PFlashRegion pRegion = (PFlashRegion)(pReserved + p->cbReservedEntries/sizeof(ReservedEntry)); // Flash Region entries start right behind reserved entries
		for(unsigned int j = 0; j < p->cbRegionEntries/sizeof(FlashRegion); j++)
		{
			printf("\nFlash Region Entry %i:\n", j);
			printf("---------------------\n");
			printf("  Region type:            %s\n", regionName(pRegion[j].regionType));

			printf("  Start phys. block:      0x%08x\n", pRegion[j].dwStartPhysBlock);
			printf("  Size in phys. blocks:   0x%08x\n", pRegion[j].dwNumPhysBlocks);
			printf("  Size in log. blocks:    0x%08x -> Size in sectors: 0x%08x\n", pRegion[j].dwNumLogicalBlocks, pRegion[j].dwNumLogicalBlocks * pRegion[j].dwSectorsPerBlock);
			printf("  Sectors per block:      0x%08x\n", pRegion[j].dwSectorsPerBlock);
			printf("  Bytes per block:        0x%08x\n", pRegion[j].dwBytesPerBlock);
			printf("  Compact blocks:         0x%08x\n", pRegion[j].dwCompactBlocks);
			printf("  -> Bytes per sector:    0x%08x\n", pRegion[j].dwBytesPerBlock/pRegion[j].dwSectorsPerBlock);
			if(bytesPerSector == 1) bytesPerSector = pRegion[j].dwBytesPerBlock/pRegion[j].dwSectorsPerBlock;
		}
	}
	else
		printf("No %s header found.\n", signature);



	// Now, try to find IMGFS by their signature

    printf("\nSearching for IMGFS signature...\n");

	// This is the signature of an IMGFS
	const unsigned char IMGFS_GUID[]={0xF8, 0xAC, 0x2C, 0x9D, 0xE3, 0xD4, 0x2B, 0x4D, 
								0xBD, 0x30, 0x91, 0x6E, 0xD8, 0x4F, 0x31, 0xDC };

    for(DWORD i=0; i < Size - sizeof(IMGFS_GUID); i++)
    {
    	if(memcmp(IMGFS_GUID, Base+i, sizeof(IMGFS_GUID))==0 && (memcmp(Base+i+0x2c, "LZX", 3)==0 || memcmp(Base+i+0x2c, "XPR", 3)==0))
    	{
  			printf("  Found IMGFS at byte 0x%08x (sector 0x%08x).\n", i, i/bytesPerSector);

			FS_BOOT_SECTOR *Boot=(FS_BOOT_SECTOR*)(Base+i);

			printf("\n  dwFSVersion:              %08X\n",Boot->dwFSVersion);
			printf("  dwSectorsPerHeaderBlock:  %08X\n",Boot->dwSectorsPerHeaderBlock);
			printf("  dwRunsPerFileHeader:      %08X\n",Boot->dwRunsPerFileHeader);
			printf("  dwBytesPerHeader:         %08X\n",Boot->dwBytesPerHeader);
			printf("  dwChunksPerSector:        %08X\n",Boot->dwChunksPerSector);
			printf("  dwFirstHeaderBlockOffset: %08X\n",Boot->dwFirstHeaderBlockOffset);
			printf("  dwDataBlockSize:          %08X\n",Boot->dwDataBlockSize);
			printf("  szCompressionType:        %3s\n",Boot->szCompressionType);
			printf("  dwFreeSectorCount:        %08X\n",Boot->dwFreeSectorCount);
			printf("  dwHiddenSectorCount:      %08X\n",Boot->dwHiddenSectorCount);
			printf("  dwUpdateModeFlag:         %08X\n",Boot->dwUpdateModeFlag);
    	}
    }

	printf("---\n");
	return 0;
}


