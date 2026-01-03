#include <stdio.h>
#include <windows.h>

#include "..\common\imgfs.h"
#include "..\common\fls.h"

// This is the signature of an IMGFS
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


// Start here...
int main(int argc, char **argv)
{
	printf("ImgfsFromNb 2.2\n");

	if (argc < 3)
	{
		printf("Usage: %s <os.nb.payload> <imgfs.bin>\n"
			   "Creates a raw <imgfs.bin> file from <os.nb.payload>\n", argv[0]);
		return 2;
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
		return 1;
	}

    BYTE* Base = (BYTE*)MapViewOfFile(hMem, FILE_MAP_READ, 0, 0, 0);

    if (!Base)
	{
		printf("Internal error 2. Aborting.\n");
		return 1;
	}

    DWORD Size = GetFileSize(hNbFile, 0);

	if(!IS_VALID_BOOTSEC(Base))
	{
		printf("\nInput file '%s' has no valid boot sector! Aborting.\n", argv[1]);
		return 1;
	}

    // find ImgFs partition
	unsigned int i;

	// look into Partition Table at offset 0x1be, find ImgFs partition (i.e., type 4)
	PPARTENTRY part = (PPARTENTRY)(Base + 0x1be);

	for(i=0; i < 4; i++)
	{
		if(part[i].Part_FileSystem == 0x25) // 0x25 is an ImgFS partition
			break;
	}

	if(i >= 4) // no ImgFs partition found
	{
		printf("Input file '%s' does not contain an ImgFs partition. Aborting.\n", argv[1]);
		return 1;
	}

	// get partition start and end (relative to Base)
	unsigned int ImgFsStart = part[i].Part_StartSector * sectorSize(Base, Size);
	unsigned int ImgFsEnd = ImgFsStart + part[i].Part_TotalSectors * sectorSize(Base, Size);

	printf("ImgFs partition starts at 0x%08x and ends at 0x%08x\n", ImgFsStart, ImgFsEnd);

	// find first occurence of IMGFS_GUID between start and end of this partition
	for(i = ImgFsStart; i < ImgFsEnd - sizeof(IMGFS_GUID); i++)
    {
    	if(memcmp(IMGFS_GUID, Base+i, sizeof(IMGFS_GUID))==0 && (memcmp(Base+i+0x2c, "LZX", 3)==0 || memcmp(Base+i+0x2c, "XPR", 3)==0))
			break;
    }

	if(i >= ImgFsEnd - sizeof(IMGFS_GUID))
	{
		printf("No IMGFS signature found. Exiting.\n");
		return 1;
	}

    // write ImgFs
	printf("Dumping IMGFS at offset 0x%08x (size 0x%08x)\n", i, ImgFsEnd - i);

	DWORD writtenBytes;

	HANDLE hRawFile=CreateFile(argv[2], GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	if(hRawFile == INVALID_HANDLE_VALUE)
	{
		printf("Could not create output file %s. Exiting.\n", argv[2]);
		return 1;
	}

	// printf(".");
	WriteFile(hRawFile, Base + i, ImgFsEnd - i, &writtenBytes, 0);

	CloseHandle(hRawFile);

	UnmapViewOfFile(Base);
	CloseHandle(hNbFile);

	printf("\nDone!\n");
	return 0;
}

