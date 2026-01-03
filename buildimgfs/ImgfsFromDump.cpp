
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "..\common\imgfs.h"

#define MAX_ROM_SIZE (128*1024*1024)  // was 64 MByte, but I see no reason not to increase that.

#define RoundUp(a,align) ((((a)+(align)-1)/(align))*(align))

// Globals
BYTE* Base=0;
FS_BOOT_SECTOR *Boot=0;
DWORD SectorSize=-1;
FILETIME FileTime;
DWORD Hash=0;

// contains for every chunk of 0x40 bytes if it is in use or not.
bool MemoryMap[MAX_ROM_SIZE/0x40];	// false == free, true == used


//
int HashName(wchar_t *arg_0,int namelen)
{
	char Buff[1024];
	memset(Buff,0,sizeof(Buff));

	for(int i=0; i<namelen; i++)
		Buff[i]=arg_0[i] & 255;

	if(strlen(Buff)>=8 && Buff[strlen(Buff)-4]=='.')
		Buff[strlen(Buff)-4]=0;

	_strlwr(Buff);
	return Buff[0]|(Buff[1]<<8L)|(Buff[strlen(Buff)-2]<<16L)|(Buff[strlen(Buff)-1]<<24L);
}


#if 0

GGURU's version:

/*
 * Allocate a contiguous area of memory of the given size, aligned to the given granularity.
 * Note that independently of the requested alignment we always align on 64 byte boundaries.
 * If such a block can be found, it is marked as 'in use' in the MemoryMap
 * and a pointer is returned.
 * Otherwise, the function exits the whole process.
 */
long Alloc(DWORD Bytes, DWORD Align=0x40)   //GGURU changed to long to support error enum
{
	int NBlocks=RoundUp(Bytes, Align)/0x40;
	int Pos = 0; //GGURU 
	//int Pos = -1;
	bool block_to_small = true; //GGURU added
	//static int FirstFreePos = 0; //GGURU moved to global
	
	// find the currently first free 64 byte block; @todo Protect against overflow!
	while(MemoryMap[FirstFreePos+1] && (FirstFreePos < MAX_ROM_SIZE/0x40))
		FirstFreePos++;

	// Now, find a contiguous(!) chunk that is big enough for this request
	// Initially, i is likely smaller than FirstFreePos, so the first iteration of the outer loop
	// will likely fail. But that's just cosmetic, no danger.
	// i and j are not byte addresses, but 64 byte block numbers, just like MemoryMap and FirstFreePos

	//GGURU - fixed end of memory detection
	//for(int i=((FirstFreePos*0x40/Align)*Align)/0x40; i < MAX_ROM_SIZE/0x40 - NBlocks; i += Align/0x40)
	//{
	//	for(int j = i; j < i + NBlocks; j++)
	//		if(MemoryMap[j])
	//			goto Next;  // continue outer loop
	//	Pos = i;
	//	break;
//Next:;
	//}
	for(Pos=((FirstFreePos*0x40/Align)*Align)/0x40; Pos < MAX_ROM_SIZE/0x40 - NBlocks && block_to_small; Pos += Align/0x40)
	{	block_to_small=false;
		for(int j = Pos; j < Pos + NBlocks; j++)
			if(MemoryMap[j])
			{	block_to_small=true;
				break;
			}
		if(!block_to_small)
			break;
	}

	//GGURU fixed
    //if(Pos == -1)  // no suitable free space found
	if(Pos >= MAX_ROM_SIZE/0x40-NBlocks || block_to_small)
	{
		printf("Maximum ROM size (0x%x) reached, aborting!\n", MAX_ROM_SIZE);
		return OUT_OF_MEMORY;  //GGURU changed to use enum values
	}

	for(int j = Pos; j < Pos + NBlocks; j++)
		MemoryMap[j]=true;

	return Pos*0x40;
}


On IRC (#ppcgurus on IRC-QNET) he also writes:

 only two things to look for in that:
[22:40] <GGuruUSA> the bounds checking when you're looking near end of memory was not correct/preset
[22:40] <GGuruUSA> er, present
[22:41] and the cause of failure on exit wasn't distinguishable (out of memory vs no block large enough)
[22:41] <GGuruUSA> they both end up being out_of_memory errors, but you might want to do something different in the future.
[22:42] <GGuruUSA> ...like add files from largest to smallest.


#endif


/*
 * Allocate a contiguous area of memory of the given size, aligned to the given granularity.
 * Note that independently of the requested alignment we always align on 64 byte boundaries.
 * If such a block can be found, it is marked as 'in use' in the MemoryMap
 * and a pointer is returned.
 * Otherwise, the function exits the whole process.
 */
DWORD Alloc(DWORD Bytes, DWORD Align=0x40)
{
	int NBlocks=RoundUp(Bytes, Align)/0x40;
	int Pos = -1;
	static int FirstFreePos = 0;
	
	// find the currently first free 64 byte block; @todo Protect against overflow!
	while(MemoryMap[FirstFreePos+1] && (FirstFreePos < MAX_ROM_SIZE/0x40))
		FirstFreePos++;

	// Now, find a contiguous(!) chunk that is big enough for this request
	// Initially, i is likely smaller than FirstFreePos, so the first iteration of the outer loop
	// will likely fail. But that's just cosmetic, no danger.
	// i and j are not byte addresses, but 64 byte block numbers, just like MemoryMap and FirstFreePos
	for(int i=((FirstFreePos*0x40/Align)*Align)/0x40; i < MAX_ROM_SIZE/0x40 - NBlocks; i += Align/0x40)
	{
		for(int j = i; j < i + NBlocks; j++)
			if(MemoryMap[j])
				goto Next;  // continue outer loop
		Pos = i;
		break;
Next:;
	}

    if(Pos == -1)  // no suitable free space found
	{
		printf("Maximum ROM size (0x%x) reached, aborting!\n", MAX_ROM_SIZE);
		exit(1);
	}

	for(int j = Pos; j < Pos + NBlocks; j++)
		MemoryMap[j]=true;

	return Pos*0x40;
}

//
DWORD AllocHeader()
{
	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Boot->dwFirstHeaderBlockOffset+Base);
	FS_BLOCK_HEADER *LastBlock;
	FSHEADER *LastHdr=0;
	do
	{
		LastBlock=Block;
		for(unsigned int i=0; i<Boot->dwFirstHeaderBlockOffset/Boot->dwBytesPerHeader; i++) 
		{
			FSHEADER *Hdr=(FSHEADER*)((char*)LastBlock+sizeof(FS_BLOCK_HEADER)+Boot->dwBytesPerHeader*i);
			if(Hdr->dwHeaderFlags==0xFFFFFFFF)
			{
				LastHdr=Hdr;
				return ((BYTE *)LastHdr)-Base;
			}
		}
		Block=(FS_BLOCK_HEADER*)(Block->dwNextHeaderBlock+Base);
	} while((BYTE *)Block!=Base);

	DWORD NewHeaderPos=Alloc(Boot->dwSectorsPerHeaderBlock*SectorSize, SectorSize);
	LastBlock->dwNextHeaderBlock=NewHeaderPos;
	memset(NewHeaderPos+Base,0xff,Boot->dwSectorsPerHeaderBlock*SectorSize);
	Block=(FS_BLOCK_HEADER*)(Base+NewHeaderPos);
	Block->dwBlockSignature=0x2F5314CE;
	Block->dwNextHeaderBlock=0;
	return NewHeaderPos+sizeof(FS_BLOCK_HEADER);
}

//
void InitAlloc()
{
	memset(MemoryMap, 0, sizeof(MemoryMap));  // mark all 64 byte chunks as free

	for(unsigned int i=0; i<Boot->dwFirstHeaderBlockOffset/0x40; i++)	// mark first sector as used
		MemoryMap[i]=true;

	DWORD NewHeaderPos = Alloc(Boot->dwSectorsPerHeaderBlock * SectorSize, SectorSize); // alloc first header sector

	memset(NewHeaderPos + Base, 0xFF, Boot->dwSectorsPerHeaderBlock * SectorSize);

	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Base + NewHeaderPos);

	Block->dwBlockSignature=0x2F5314CE;
	Block->dwNextHeaderBlock=0;
	// okay, and how does 'Block' get exported outside this function, so that other can write header data to the right place?
}

typedef LPVOID (*FNCompressAlloc)(DWORD AllocSize);
typedef VOID (*FNCompressFree)(LPVOID Address);
typedef DWORD (*FNCompressOpen)( DWORD dwParam1, DWORD MaxOrigSize, FNCompressAlloc AllocFn, FNCompressFree FreeFn, DWORD dwUnknown);
typedef DWORD (*FNCompressConvert)( DWORD ConvertStream, LPVOID CompAdr, DWORD CompSize, LPCVOID OrigAdr, DWORD OrigSize); 
typedef VOID (*FNCompressClose)( DWORD ConvertStream);

LPVOID Compress_AllocFunc(DWORD AllocSize)
{
    return LocalAlloc(LPTR, AllocSize);
}

VOID Compress_FreeFunc(LPVOID Address)
{
    LocalFree(Address);
}

FNCompressOpen CompressOpen= NULL;
FNCompressConvert CompressConvert= NULL;
FNCompressClose CompressClose= NULL;

void AddFile(char* Path)
{
	DWORD Hdr=AllocHeader();
	FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
	LastHdr->dwHeaderFlags=0xFFFFF6FE;
	FS_FILE_HEADER *Fh=&LastHdr->hdrFile;
	memset(Fh,0,sizeof(FS_FILE_HEADER));

	char *FileName=strrchr(Path,'\\')+1;

    HANDLE HFa=CreateFile(Path,GENERIC_READ, FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	if(HFa==0)
		ExitProcess(1);

    HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

    char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

    DWORD SizeA=GetFileSize(HFa,0);

	Fh->dwFileAttributes = (GetFileAttributes(Path) & 0xF) | FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_READONLY;
	Fh->fsName.cchName = (WORD)strlen(FileName);
	Fh->fsName.wFlags = 0;
	Fh->fileTime = FileTime;
	if(strlen(FileName)<=4)
	{
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.szShortName),8);
	} 
	else if(strlen(FileName)<=0x30/2)
	{
		Fh->fsName.wFlags=2;
		Fh->fsName.dwFullNameOffset=AllocHeader();
		FSHEADER *NameHdr=(FSHEADER*)(Base+Fh->fsName.dwFullNameOffset);
		NameHdr->dwHeaderFlags=0xFFFFFEFB;
		memset(NameHdr->hdrName.szName,0,0x30);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,NameHdr->hdrName.szName,Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName(NameHdr->hdrName.szName,Fh->fsName.cchName);
	} 
	else
	{
		Fh->fsName.dwFullNameOffset=Alloc(Fh->fsName.cchName*2);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName((LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName);
	}

	Fh->dwStreamSize=SizeA;
	if(SizeA==0)
	{
		Fh->dataTable[0].cbOnDiskSize=0;
		Fh->dataTable[0].dwDiskOffset=0;
	} 
	else
	{
		Fh->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
		Fh->dataTable[0].dwDiskOffset=Alloc(Fh->dataTable[0].cbOnDiskSize);
		memset(Fh->dataTable[0].dwDiskOffset+Base,0,Fh->dataTable[0].cbOnDiskSize);
		FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fh->dataTable[0].dwDiskOffset);
		for(unsigned int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
		{
			int CurrSize=Boot->dwDataBlockSize;
			if(SizeA-i<Boot->dwDataBlockSize)
				CurrSize=SizeA-i;
		
			Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

			DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
			BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
			DWORD ret=CompressConvert(stream, buf, CurrSize, BaseA+i, CurrSize);

			if(ret<CurrSize)
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize = (WORD)ret;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset = Alloc(ret);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
			} else
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
			}

			LocalFree(buf);
			CompressClose(stream);
		}
	}
	UnmapViewOfFile(BaseA);
	CloseHandle(HMa);
	CloseHandle(HFa);
}

// 
void AddModule(char* Path)
{
	DWORD Hdr=AllocHeader();
	FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
	LastHdr->dwHeaderFlags=0xFFFFFEFE;
	FS_FILE_HEADER *Fh=&LastHdr->hdrFile;
	memset(Fh,0,sizeof(FS_FILE_HEADER));

	char *FileName=strrchr(Path,'\\')+1;

	char TmpFileName[1024];
	strcpy(TmpFileName,Path);
	strcat(TmpFileName,"\\imageinfo.bin");

    HANDLE HFa=CreateFile(TmpFileName,GENERIC_READ, FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	if(HFa==0)
		ExitProcess(1);

	HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

	char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

    DWORD SizeA=GetFileSize(HFa,0);

	Fh->dwFileAttributes=(GetFileAttributes(TmpFileName)&0xF)|FILE_ATTRIBUTE_INROM;
	Fh->fsName.cchName = (WORD)strlen(FileName);
	Fh->fsName.wFlags=0;
	Fh->fileTime=FileTime;
	if(strlen(FileName)<=4)
	{
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.szShortName),8);
	} 
	else if(strlen(FileName)<=0x30/2)
	{
		Fh->fsName.wFlags=2;
		Fh->fsName.dwFullNameOffset=AllocHeader();
		FSHEADER *NameHdr=(FSHEADER*)(Base+Fh->fsName.dwFullNameOffset);
		NameHdr->dwHeaderFlags=0xFFFFFEFB;
		memset(NameHdr->hdrName.szName,0,0x30);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,NameHdr->hdrName.szName,Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName(NameHdr->hdrName.szName,Fh->fsName.cchName);
	} else
	{
		Fh->fsName.dwFullNameOffset=Alloc(Fh->fsName.cchName*2);
		MultiByteToWideChar(CP_OEMCP,0,FileName,Fh->fsName.cchName,(LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName*2);
		Fh->fsName.dwShortName=HashName((LPWSTR)(Fh->fsName.dwFullNameOffset+Base),Fh->fsName.cchName);
	}

	Fh->dwStreamSize=SizeA;
	if(SizeA==0)
	{
		Fh->dataTable[0].cbOnDiskSize=0;
		Fh->dataTable[0].dwDiskOffset=0;
	} else
	{
		Fh->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
		Fh->dataTable[0].dwDiskOffset=Alloc(Fh->dataTable[0].cbOnDiskSize);
		memset(Fh->dataTable[0].dwDiskOffset+Base,0,Fh->dataTable[0].cbOnDiskSize);
		FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fh->dataTable[0].dwDiskOffset);
		for(unsigned int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
		{
			int CurrSize=Boot->dwDataBlockSize;
			if(SizeA-i<Boot->dwDataBlockSize)
				CurrSize=SizeA-i;
		
			Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

			DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
			BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
			DWORD ret=CompressConvert(stream, buf, CurrSize, BaseA+i, CurrSize);

			if(ret<CurrSize)
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize = (WORD)ret;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(ret);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
			} else
			{
				Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
				Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
				memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
			}

			LocalFree(buf);
			CompressClose(stream);
		}
	}
	UnmapViewOfFile(BaseA);
	CloseHandle(HMa);
	CloseHandle(HFa);
	FS_STREAM_HEADER *PrevStream=(FS_STREAM_HEADER*)Fh;
	for(int i=0; i<999; i++) // FIXME: Use FindFirst/FindNext to search for "S000" files in subdir
	{
		char FileName[1024];
		sprintf(FileName,"%s\\S%03d",Path,i);
		HANDLE HFa=CreateFile(FileName,GENERIC_READ, FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
		if(HFa==INVALID_HANDLE_VALUE)
			break;
		Fh->dwFileAttributes|=FILE_ATTRIBUTE_ROMMODULE;

		HANDLE HMa=CreateFileMapping(HFa,0,PAGE_READONLY,0,0,0);

		char *BaseA=(char*)MapViewOfFile(HMa,FILE_MAP_READ,0,0,0);

		DWORD SizeA=GetFileSize(HFa,0);
		DWORD Hdr=AllocHeader();
		FSHEADER *LastHdr=(FSHEADER*)(Hdr+Base);
		LastHdr->dwHeaderFlags=0xFFFFF6FD;
		FS_STREAM_HEADER *Fs=&LastHdr->hdrStream;
		memset(Fs,0,sizeof(FS_STREAM_HEADER));
		PrevStream->dwNextStreamHeaderOffset=Hdr;
		PrevStream=Fs;
		
		Fs->dwSecNameLen=4;
		MultiByteToWideChar(CP_OEMCP,0,FileName+strlen(FileName)-4,4,(LPWSTR)(Fs->szSecName),8);

		Fs->dwStreamSize=SizeA;
		if(SizeA==0)
		{
			Fs->dataTable[0].cbOnDiskSize=0;
			Fs->dataTable[0].dwDiskOffset=0;
		} 
		else
		{
			Fs->dataTable[0].cbOnDiskSize=RoundUp(RoundUp(SizeA,Boot->dwDataBlockSize)/Boot->dwDataBlockSize*sizeof(FS_ALLOC_TABLE_ENTRY),0x40);
			Fs->dataTable[0].dwDiskOffset=Alloc(Fs->dataTable[0].cbOnDiskSize);
			memset(Fs->dataTable[0].dwDiskOffset+Base,0,Fs->dataTable[0].cbOnDiskSize);
			FS_ALLOC_TABLE_ENTRY *Te=(FS_ALLOC_TABLE_ENTRY*)(Base+Fs->dataTable[0].dwDiskOffset);
			for(unsigned int i=0; i<SizeA; i+=Boot->dwDataBlockSize)
			{
				int CurrSize=Boot->dwDataBlockSize;
				if(SizeA-i<Boot->dwDataBlockSize)
					CurrSize=SizeA-i;
			
				Te[i/Boot->dwDataBlockSize].wDecompressedBlockSize=CurrSize;

				DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
				BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
				DWORD ret=CompressConvert(stream, buf, Boot->dwDataBlockSize, BaseA+i, CurrSize);
		//		printf("%d\n",ret);

				if(ret<CurrSize)
				{
					Te[i/Boot->dwDataBlockSize].wCompressedBlockSize = (WORD)ret;
					Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(ret);
					memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,buf,ret);
				} else
				{
					Te[i/Boot->dwDataBlockSize].wCompressedBlockSize=CurrSize;
					Te[i/Boot->dwDataBlockSize].dwDiskOffset=Alloc(CurrSize);
					memcpy(Te[i/Boot->dwDataBlockSize].dwDiskOffset+Base,BaseA+i,CurrSize);
				}

				LocalFree(buf);
				CompressClose(stream);
			}
		}
		UnmapViewOfFile(BaseA);
		CloseHandle(HMa);
		CloseHandle(HFa);
	}
}

// Main function
int _tmain(int argc, _TCHAR* argv[])
{
	printf("ImgfsFromDump 2.2\n");

    if(argc < 3)
	{
		printf("Usage: %s <imgfs-in.bin> <imgfs-out.bin>\n"
			"Creates <imgfs-out.bin> from the 'dump' subdirectory,\n"
			"using <imgfs-in.bin> as template (read-only!)\n", argv[0]);
		return 1;
	}

	if(!strcmp(argv[1], argv[2]))
	{
		printf("Input and output file must NOT be the same!");
    	return 1;
	}

   	HMODULE hDll= LoadLibrary("cecompr_nt.dll");
    if (hDll==NULL || hDll==INVALID_HANDLE_VALUE) 
	{
		printf("Unable to load compression DLL!");
    	return 1;
	}
 
    // Set up our workspace memory
	HANDLE heap = HeapCreate(0, 0, 0);
	Base = (BYTE *)HeapAlloc(heap, 0, MAX_ROM_SIZE);
	Boot=(FS_BOOT_SECTOR*)Base;

	if(!heap || !Base)
	{
		printf("Out of memory. Exiting.\n");
		HeapDestroy(heap);
		return 1;
	}

    HANDLE inFile = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if(inFile == INVALID_HANDLE_VALUE)
	{
		printf("Input file %s cannot be opened. Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}

	GetFileTime(inFile,&FileTime,0,0);  // not sure why we use this file's time, but wth.

	DWORD read;

	ReadFile(inFile, Boot, sizeof(FS_BOOT_SECTOR), &read, 0);
	CloseHandle(inFile);

	if(read != sizeof(FS_BOOT_SECTOR))
	{
		printf("Input file %s cannot be read. Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}


	char CO[]="LZX_CompressOpen";
	char CE[]="LZX_CompressEncode";
	char CC[]="LZX_CompressClose";

	memcpy(CO,&Boot->szCompressionType,3);
	memcpy(CE,&Boot->szCompressionType,3);
	memcpy(CC,&Boot->szCompressionType,3);

	CompressOpen= (FNCompressOpen)GetProcAddress(hDll, CO);
    CompressConvert= (FNCompressConvert)GetProcAddress(hDll, CE);
    CompressClose= (FNCompressClose)GetProcAddress(hDll, CC);
	if(CompressOpen==0)
	{
		printf("Compression DLL does not support compression type '%s'!", &Boot->szCompressionType);
		HeapDestroy(heap);
		return 1;
	}
	printf("Using compression type '%s'!\n", &Boot->szCompressionType);

	SectorSize = Boot->dwFirstHeaderBlockOffset;
    printf("Sector size is 0x%x\n", SectorSize);

	InitAlloc();
	
	WIN32_FIND_DATA fd;
	HANDLE Hf=FindFirstFile("dump\\*",&fd);

    if(Hf == INVALID_HANDLE_VALUE)
	{
		printf("Cannot read 'dump' subdirectory. Exiting.\n");
		HeapDestroy(heap);
		return 1;
	}

	do
	{
        // skip '.' and '..'
		if(fd.cFileName[0]=='.')
			if((fd.cFileName[1]=='.' && fd.cFileName[2]==0) || fd.cFileName[1]==0)
				continue;

		printf("Processing \"%s\"",fd.cFileName);
		char FileName[1024]="dump\\";
		strcat(FileName, fd.cFileName);
		if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			puts(" as module");
			AddModule(FileName);
		} 
		else
		{
			puts(" as file");
			AddFile(FileName);
		}
	} 
	while(FindNextFile(Hf,&fd));

	
#if 0 // Maybe these fields should be left alone

	// Update a few ImgFs header fields
	Boot->dwFreeSectorCount = 0;  // No idea what that field contains. The value in the Kaiser is *way* too high for a free sector count!
	Boot->dwUpdateModeFlag = 0;   // not sure what that means, but *if* something !0 means we can update the ROM via Windows Update, we better set it to 0 (as we have no free sectors).
	// Boot->dwHiddenSectorCount = 0;  // I am not sure about this one...
#endif

    // find first free address (in bytes)
	DWORD firstFree = MAX_ROM_SIZE/0x40 - 1;
	while(MemoryMap[firstFree]==false)
		firstFree--;
	firstFree = (firstFree + 1) * 0x40;  // convert to bytes; + 1 as firstFree was the *last used* 64-byte-chunk, and we want the *first unused*

	// pad to next sector boundary with 0xFF
	DWORD paddedSize = RoundUp(firstFree, SectorSize);
	memset(Base + firstFree, 0xFF, paddedSize - firstFree);

    // dump our in-memory image to file
    HANDLE outFile = CreateFile(argv[2], GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    if(outFile == INVALID_HANDLE_VALUE)
	{
		printf("Output file %s cannot be opened. Exiting.\n", argv[2]);
		HeapDestroy(heap);
		return 1;
	}

	DWORD written;
	WriteFile(outFile, Base, paddedSize, &written, 0);

	if(written != paddedSize)
	{
		printf("Error writing to output file %s. DO NOT USE IT! Exiting.\n", argv[1]);
		HeapDestroy(heap);
		return 1;
	}

	CloseHandle(outFile);
    HeapDestroy(heap);
 
	printf("Total Sectors: 0x%04x\n", paddedSize/SectorSize);

	return 0;
}
