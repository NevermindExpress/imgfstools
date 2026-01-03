#include <stdio.h>
#include <windows.h>
#include "..\common\imgfs.h"

static const unsigned char IMGFS_GUID[]={0xF8, 0xAC, 0x2C, 0x9D, 0xE3, 0xD4, 0x2B, 0x4D, 
							0xBD, 0x30, 0x91, 0x6E, 0xD8, 0x4F, 0x31, 0xDC };

char* Base=0;

static char *RamUsage[0x100000];	// Would point to module name, TODO: optimize memory usage

// 
void SetModuleAddrs(char *FileName, DWORD Base, DWORD Size)
{
	Base=(Base/0x1000)*0x1000; Size=((Size+0xFFF)/0x1000)*0x1000;
	if(Size>16*1024*1024)	// overcome buzz' buggy devauth patch
		return;
	char *Fn=_strdup(FileName);
	for(DWORD i=Base; i<Base+Size; i+=0x1000)
	{
		if(RamUsage[i/0x1000] && Base<0xFFFFFFFF && Base && Base!=0x10000 && Base!=0x10000000 && _strcmpi(RamUsage[i/0x1000],Fn))
			printf("Ram Address: %08X (%s) overlaps '%s'\n",i,Fn,RamUsage[i/0x1000]);
		RamUsage[i/0x1000]=Fn;
	}
}

//
char *GetFileName(FS_SHORT_NAME &fsName)
{
	DWORD NameOffs=fsName.dwFullNameOffset;
	DWORD Flags=fsName.wFlags;
	DWORD NameLen=fsName.cchName;
	wchar_t *Name=(wchar_t*)(NameOffs+Base);
	if(fsName.cchName<=4)
		Name=fsName.szShortName;

	if(Flags==2)
		Name+=2;
	static char Buff[1024];
	memset(Buff,0,sizeof(Buff));

	WideCharToMultiByte(CP_ACP,0,Name,NameLen,Buff,1024,0,0);
	return Buff;
}

//
void DumpDataTable(FS_DATA_RUN *Dr)
{
	FS_ALLOC_TABLE_ENTRY *Tb=(FS_ALLOC_TABLE_ENTRY*)(Base+Dr->dwDiskOffset);
	int Num=Dr->cbOnDiskSize/sizeof(FS_ALLOC_TABLE_ENTRY);
	for(int i=0; i<Num; i++)
	{
		if(Tb[i].dwDiskOffset==0)
			break;
		printf("   wCompressedBlockSize:   %04X\n",Tb[i].wCompressedBlockSize);
		printf("   wDecompressedBlockSize: %04X\n",Tb[i].wDecompressedBlockSize);
		printf("   dwDiskOffset:           %08X\n",Tb[i].dwDiskOffset);
	}
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

FNCompressOpen CompressOpen = NULL;
FNCompressConvert CompressConvert = NULL;
FNCompressClose CompressClose = NULL;

static char LastData[10240];

void DumpImageData(FS_DATA_RUN *Dr, char *FileName,FILETIME *Time,DWORD FileAttr)
{
  __try {
	char Buff[1024]="dump\\";

	CreateDirectory(Buff,0);
	strcat(Buff,FileName);
	HANDLE HT=CreateFile(Buff,GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
	if(HT==INVALID_HANDLE_VALUE)
	{
		printf("Cannot create %s\n",FileName);
	}
	if(Time)
		SetFileTime(HT,Time,Time,Time);
	SetFileAttributes(Buff,FileAttr&(255&~FILE_ATTRIBUTE_INROM&~FILE_ATTRIBUTE_COMPRESSED));
	FS_ALLOC_TABLE_ENTRY *Tb=(FS_ALLOC_TABLE_ENTRY*)(Base+Dr->dwDiskOffset);
	int Num=Dr->cbOnDiskSize/sizeof(FS_ALLOC_TABLE_ENTRY);
	DWORD Tmp;
	for(int i=0; i<Num; i++)
	{
		if(Tb[i].dwDiskOffset==0)
			break;
		if(Tb[i].wDecompressedBlockSize==Tb[i].wCompressedBlockSize)
		{
		   	WriteFile(HT,Tb[i].dwDiskOffset+Base,Tb[i].wCompressedBlockSize,&Tmp,0);
			memset(LastData,0,sizeof(LastData));
		   	memcpy(LastData,Tb[i].dwDiskOffset+Base,min(10240,Tb[i].wCompressedBlockSize));
		}
		else
		{
			DWORD stream= CompressOpen(0x10000, 0x10000, Compress_AllocFunc, Compress_FreeFunc, 0);
			BYTE *buf= (BYTE*)LocalAlloc(LPTR, 0x10000);
			CompressConvert(stream, buf, Tb[i].wDecompressedBlockSize, Tb[i].dwDiskOffset+Base, Tb[i].wCompressedBlockSize);
		   	WriteFile(HT,buf,Tb[i].wDecompressedBlockSize,&Tmp,0);
			memset(LastData,0,sizeof(LastData));
		   	memcpy(LastData,buf,min(10240,Tb[i].wDecompressedBlockSize));
			LocalFree(buf);
			CompressClose(stream);
		}
	}
	CloseHandle(HT);
  } __except(EXCEPTION_EXECUTE_HANDLER)
  {
	  puts("Unhandled exception in DumpImageData!!!");
  }
}

//
void DumpXipData(FS_DATA_RUN *Dr, char *FileName, FILETIME* FileTime, DWORD FirstStream,DWORD FileAttr)
{
  __try {
	char Buff[1024]="dump\\";

	CreateDirectory(Buff,0);
	strcat(Buff,FileName);
	strcat(Buff,"\\");
	CreateDirectory(Buff,0);
	strcpy(Buff,FileName);
	strcat(Buff,"\\");
	strcat(Buff,"imageinfo.bin");
	DumpImageData(Dr, Buff,FileTime,FileAttr);
	e32_rom *e32=(e32_rom*)LastData;
	strcpy(Buff,"dump\\");
	strcat(Buff,FileName);
	strcat(Buff,"\\");
	strcat(Buff,"imageinfo.txt");
	FILE *Out=fopen(Buff,"w+");
	if(Out==0)
	{
		printf("Invalid file name: %s\n",Buff);
		return;
	}
	fprintf(Out,"  Module name: %s\n",FileName);
	fprintf(Out,"   e32_objcnt:            %08X\n",e32->e32_objcnt);
	fprintf(Out,"   e32_imageflags:        %08X\n",e32->e32_imageflags);
	fprintf(Out,"   e32_entryrva:          %08X\n",e32->e32_entryrva);
	fprintf(Out,"   e32_vbase:             %08X\n",e32->e32_vbase);
	fprintf(Out,"   e32_subsysmajor:       %08X\n",e32->e32_subsysmajor);
	fprintf(Out,"   e32_subsysminor:       %08X\n",e32->e32_subsysminor);
	fprintf(Out,"   e32_stackmax:          %08X\n",e32->e32_stackmax);
	fprintf(Out,"   e32_vsize:             %08X\n",e32->e32_vsize);
	fprintf(Out,"   e32_sect14rva:         %08X\n",e32->e32_sect14rva);
	fprintf(Out,"   e32_sect14size:        %08X\n",e32->e32_sect14size);
	fprintf(Out,"   e32_timestamp:         %08X\n",e32->e32_timestamp);
	for(int i=0; i<ROM_EXTRA; i++)
	{
		fprintf(Out,"   e32_unit[%d].rva:       %08X\n",i,e32->e32_unit[i].rva);
		fprintf(Out,"   e32_unit[%d].size:      %08X\n",i,e32->e32_unit[i].size);
	}
	fprintf(Out,"   e32_subsys:            %08X\n",e32->e32_subsys);

	if(_strcmpi(FileName,".vm") && _strcmpi(FileName,".rom") && _strcmpi(FileName,"devauthsig.dat")	&& e32->e32_unit[FIX].rva)
		SetModuleAddrs(FileName, e32->e32_vbase, e32->e32_vsize);

	o32_rom *o32=(o32_rom*)(LastData+((sizeof(e32_rom)+3)/4)*4);
	for(int i=0; i<e32->e32_objcnt; i++)
	{
	    fprintf(Out,"\n   o32[%d].o32_vsize:      %08X\n",i,o32[i].o32_vsize);
	    fprintf(Out,"   o32[%d].o32_rva:        %08X\n",i,o32[i].o32_rva);
	    fprintf(Out,"   o32[%d].o32_psize:      %08X\n",i,o32[i].o32_psize);
	    fprintf(Out,"   o32[%d].o32_dataptr:    %08X\n",i,o32[i].o32_dataptr);
	    fprintf(Out,"   o32[%d].o32_realaddr:   %08X\n",i,o32[i].o32_realaddr);
	    fprintf(Out,"   o32[%d].o32_flags:      %08X\n",i,o32[i].o32_flags);

		if(_strcmpi(FileName,".vm") && _strcmpi(FileName,".rom") && _strcmpi(FileName,"devauthsig.dat")	&& e32->e32_unit[FIX].rva)
		    SetModuleAddrs(FileName, o32[i].o32_realaddr, o32[i].o32_vsize);
	}
	fclose(Out);

	FSHEADER *Hdr=(FSHEADER*)(Base+FirstStream);
	while((char*)Hdr>Base)
	{
		FS_STREAM_HEADER &St=Hdr->hdrStream;
		memset(Buff,0,sizeof(Buff));
		strcpy(Buff,FileName);
		strcat(Buff,"\\");
		Buff[strlen(Buff)]=St.szSecName[0];
		Buff[strlen(Buff)]=St.szSecName[1];
		Buff[strlen(Buff)]=St.szSecName[2];
		Buff[strlen(Buff)]=St.szSecName[3];
		DumpImageData(St.dataTable,Buff,0,0);
		Hdr=(FSHEADER*)(Base+St.dwNextStreamHeaderOffset);	
	};
  } __except(EXCEPTION_EXECUTE_HANDLER)
  {
	  puts("Unhandled exception in DumpXipData!!!");
  }
	char Buff[1024]="RecMod.exe dump\\";
	strcat(Buff,FileName);
//	ShellExecute(0,0,"RecMod.exe",Buff,0,SW_HIDE);
	STARTUPINFO si;
	memset(&si,0,sizeof(si));
	si.cb=sizeof(si);
	si.wShowWindow=SW_SHOWDEFAULT;
	PROCESS_INFORMATION pi;
	fflush(stdout); // was: fflush(STDOUT);
	CreateProcess(0,Buff,0,0,0,0,0,0,&si,&pi);
	WaitForSingleObject(pi.hProcess,INFINITE);
	fflush(stdout); // was: fflush(STDOUT);
}


// Main entry point. Usage: viewimgfs <imgfs_raw_data.bin>
int main(int argc, char **argv)
{
	printf("ImgfsToDump 2.1rc2\n");

	if(argc<2)
	{
		printf("Usage: %s <imgfs.bin>\n"
			   "Extracts all files and modules from the given imgfs file\n"
			   "into a 'dump' subdirectory\n", argv[0]);
		return 1;
	}

	// load (de)compression library
   	HMODULE hDll= LoadLibrary("cecompr_nt.dll");
    if (hDll==NULL || hDll==INVALID_HANDLE_VALUE) 
	{
		printf("Could not load cecompr_nt.dll. Aborting.\n");
    	return 1;
	}
    
	// open and map (to RAM) the input raw data file. 'Base' is the pointer to the RAM map of the file; all pointers inside the file are relative to this base!
    HANDLE HF=CreateFile(argv[1],GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    Base=(char*)MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);

    if(!HF || !HM || !Base)
    {
        printf("Cannot map file %s\n",argv[1]);
        return 1;
    }

	// print what we know so far about the raw data image
	DWORD Size=GetFileSize(HF,0);
	FS_BOOT_SECTOR *Boot=(FS_BOOT_SECTOR*)Base;


	printf("IMGFS guidBootSignature: ");
	for(int i=0; i < sizeof(IMGFS_GUID); i++)
		printf("%X ", ((unsigned char*)&Boot->guidBootSignature)[i]);

    for(int i=0; i < sizeof(IMGFS_GUID); i++)
	{
		if(((unsigned char*)&Boot->guidBootSignature)[i] != IMGFS_GUID[i])
		{
			printf("\n\nError: %s is not a valid IMGFS file. Exiting.\n", argv[1]);
			return 1;
		}
	}

	printf("\ndwFSVersion: %08X\n",Boot->dwFSVersion);
	printf("dwSectorsPerHeaderBlock: %08X\n",Boot->dwSectorsPerHeaderBlock);
	printf("dwRunsPerFileHeader: %08X\n",Boot->dwRunsPerFileHeader);
	printf("dwBytesPerHeader: %08X\n",Boot->dwBytesPerHeader);
	printf("dwChunksPerSector: %08X\n",Boot->dwChunksPerSector);
	printf("dwFirstHeaderBlockOffset: %08X\n",Boot->dwFirstHeaderBlockOffset);
	printf("dwDataBlockSize: %08X\n",Boot->dwDataBlockSize);
	printf("szCompressionType: %4s\n",Boot->szCompressionType);
	printf("dwFreeSectorCount: %08X\n",Boot->dwFreeSectorCount);
	printf("dwHiddenSectorCount: %08X\n",Boot->dwHiddenSectorCount);
	printf("dwUpdateModeFlag: %08X\n",Boot->dwUpdateModeFlag);

    // get pointers to the three functions we need from the decompression DLL
	char CO[]="LZX_DecompressOpen";
	char CE[]="LZX_DecompressDecode";
	char CC[]="LZX_DecompressClose";

	memcpy(CO,&Boot->szCompressionType,3);
	memcpy(CE,&Boot->szCompressionType,3);
	memcpy(CC,&Boot->szCompressionType,3);

	CompressOpen= (FNCompressOpen)GetProcAddress(hDll, CO);
    CompressConvert= (FNCompressConvert)GetProcAddress(hDll, CE);
    CompressClose= (FNCompressClose)GetProcAddress(hDll, CC);

	if(CompressOpen == 0)
	{
		printf("Compression DLL does not support compression type '%s'!", &Boot->szCompressionType);
		return 1;
	}

    // loop over all header blocks
	FS_BLOCK_HEADER *Block=(FS_BLOCK_HEADER*)(Boot->dwFirstHeaderBlockOffset+Base);
	do
	{
		printf("\nAddress: %08X, dwBlockSignature: %08X\n",(char*)Block-Base, Block->dwBlockSignature);
		printf("dwNextHeaderBlock: %08X (size: %08X)\n", Block->dwNextHeaderBlock, Block->dwNextHeaderBlock-((char*)Block-Base));

		// loop over all headers inside the header block
		for(int i=0; i < Boot->dwFirstHeaderBlockOffset/Boot->dwBytesPerHeader; i++) // maybe better: NumberOfHeaders = (NextHeaderBlock - Block)/BytesPerHeader
		{
//			DWORD *Hdr=(DWORD*)((char*)Block+sizeof(FS_BLOCK_HEADER)+Boot->dwBytesPerHeader*i);
			FSHEADER *Hdr=(FSHEADER*)((char*)Block + sizeof(FS_BLOCK_HEADER) + Boot->dwBytesPerHeader*i);
			printf("\n Header type: %08X, Addr: %08X\n",Hdr->dwHeaderFlags,(char*)Hdr-Base);
			switch(Hdr->dwHeaderFlags)
			{
			case 0xFFFFFEFB:
			  {
    			FS_LOCAL_NAME &Fn=Hdr->hdrName;
    			printf("  szName: %S\n",Fn.szName);
    			break;
    		  }	
			case 0xFFFFF6FE:
			case 0xFFFFFEFE:
			  {
    			FS_FILE_HEADER &Fh=Hdr->hdrFile;
				printf("  dwNextDataTableOffset:    %08X\n",Fh.dwNextDataTableOffset);
				printf("  dwNextStreamHeaderOffset: %08X\n",Fh.dwNextStreamHeaderOffset);
				printf("  cchName:                  %08X\n",Fh.fsName.cchName);
				printf("  wFlags:                   %08X\n",Fh.fsName.wFlags);
				if(Fh.fsName.wFlags!=2 && Fh.fsName.wFlags!=0)
					printf("   Unknown flag!!!");

				if(Fh.fsName.cchName>4)
				{
					printf("  dwShortName:              %08X (%.4s)\n",Fh.fsName.dwShortName,&Fh.fsName.dwShortName);
					printf("  dwFullNameOffset:         %08X\n",Fh.fsName.dwFullNameOffset);
				} else
				{
					printf("  szShortName:              %.4S\n",Fh.fsName.szShortName);
				}
				printf("  dwStreamSize:             %08X\n",Fh.dwStreamSize);
				printf("  dwFileAttributes:         %08X\n",Fh.dwFileAttributes);
				printf("  fileTime:                 %08X %08X\n",Fh.fileTime.dwLowDateTime,Fh.fileTime.dwHighDateTime);
				printf("  dwReserved:               %08X\n",Fh.dwReserved);
				printf("  dwDiskOffset:             %08X\n",Fh.dataTable[0].dwDiskOffset);
				printf("  cbOnDiskSize:             %08X\n",Fh.dataTable[0].cbOnDiskSize);
				DumpDataTable(Fh.dataTable);
				if(0xFFFFFEFE==Hdr->dwHeaderFlags)
					DumpXipData(Fh.dataTable,GetFileName(Fh.fsName),&Fh.fileTime,Fh.dwNextStreamHeaderOffset,Fh.dwFileAttributes);
				else
					DumpImageData(Fh.dataTable,GetFileName(Fh.fsName),&Fh.fileTime,Fh.dwFileAttributes);

				break;
			  }
			case 0xFFFFF6FD:
			  {
    			FS_STREAM_HEADER &St=Hdr->hdrStream;
				printf("  dwNextDataTableOffset:    %08X\n",St.dwNextDataTableOffset);
				printf("  dwNextStreamHeaderOffset: %08X\n",St.dwNextStreamHeaderOffset);
				printf("  dwSecNameLen:             %08X\n",St.dwSecNameLen);
				printf("  szSecName:                %.4S\n",St.szSecName);
				printf("  dwStreamSize:             %08X\n",St.dwStreamSize);
				printf("  dwDiskOffset:             %08X\n",St.dataTable[0].dwDiskOffset);
				printf("  cbOnDiskSize:             %08X\n",St.dataTable[0].cbOnDiskSize);
				for(int i=0; i<4; i++)
    				printf("  dwUnknown[%d]:             %08X\n",i,St.dwUnknown[i]);
				DumpDataTable(St.dataTable);
				char Buff[5];
				Buff[0]=St.szSecName[0];
				Buff[1]=St.szSecName[1];
				Buff[2]=St.szSecName[2];
				Buff[3]=St.szSecName[3];
				Buff[4]=0;
//				DumpImageData(Fh.dataTable,Buff);
    			break;
			  }
			case 0xFFFFFFFF:
			  {
				printf("  Empty header\n");
				break;
			  }
			default:
				printf("  Unknown header type, FS_DATA_TABLE??\n");
			};
		}

		Block=(FS_BLOCK_HEADER*)(Block->dwNextHeaderBlock+Base);  // advance 'Block' to next block
	} while((char*)Block!=Base && (char*)Block<Base+Size);

	FILE *Out=fopen("dump_MemoryMap.txt","w+");
	for(int i=0; i<0x100000; i++)
	{
		if(RamUsage[i])
		{
			fprintf(Out,"%08X - ",i*0x1000);
			DWORD Last=i;
			while(RamUsage[i+1] && _strcmpi(RamUsage[i+1],RamUsage[i])==0)
				i++;
			fprintf(Out,"%08X (%7d bytes): %s\n",i*0x1000+0xFFF, i*0x1000+0xFFF-Last*0x1000, RamUsage[i]);
		}
	}
	fclose(Out);

	return 0; // success
}
