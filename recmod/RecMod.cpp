// RecMod.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <imagehlp.h>
#include "myutil.h"
#include "recmod.h"

BYTE MZHeader[]=
{
	0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00,
	0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
	0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72, 0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
	0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E, 0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
	0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int Count=0;
char *Data=0;
DWORD PrevDWord=0;
DWORD CurrDWord=0;

Sasha::TSortedList<DWORD> Relocations;

bool ProcessNextBits(int BitStart)
{
	int NextBits = *Data & 1;
	CurrDWord |= NextBits << BitStart;
	BitStart++;
	NextBits = (*Data >> 1) & 0x3F;
	CurrDWord |= NextBits << BitStart;
	return *Data & 0x80;
}

void ProcessCase0()
{
	if(!(*Data & 0x40))
	{
		puts("Error! ProcessCase0: bit 5 is zero!"); // probably this case would describe absolute reloc address and not delta from previous
		exit(1);
	}
	bool HasNextBlock=*Data & 0x20;
	CurrDWord=*Data & 0x1F;
	int BitStart=5;			

	Data++;
	while(HasNextBlock)
	{
		HasNextBlock=ProcessNextBits(BitStart);
		BitStart+=7;
		Data++;
	}
	PrevDWord+=CurrDWord;
//	printf("%08X\n",PrevDWord);
	Relocations.Add(PrevDWord);
}

void ProcessCase1()
{
	int Add=(*Data>>5) & 3;
	Add = 4 + Add*4;

	int RepeatCount = 1 + (*Data & 0x1F);
	Data++;

	bool HasNextBlock=*Data & 0x80;
	CurrDWord=*Data & 0x7F;
	int BitStart=7;			

	Data++;
	while(HasNextBlock)
	{
		HasNextBlock=ProcessNextBits(BitStart);
		BitStart+=7;
		Data++;
	}
	PrevDWord+=CurrDWord;
	for(int i=0; i<RepeatCount; i++)
	{
//		printf("%08X\n",PrevDWord);
		Relocations.Add(PrevDWord);
		PrevDWord+=Add;
	}
//	printf("%08X\n",PrevDWord);
	Relocations.Add(PrevDWord);
}

char *RelocationsSection=0;
int RelocationsSectionSize=0;

struct RelocBlock
{
	IMAGE_BASE_RELOCATION Rel;
	Sasha::TList<DWORD> Relocs;
	RelocBlock() {Rel.VirtualAddress = 0;}
	~RelocBlock() {}
	RelocBlock(const RelocBlock &R):Relocs(R.Relocs) {Rel=R.Rel; }
	RelocBlock & operator= (const RelocBlock &R) {Rel=R.Rel; Relocs=R.Relocs; return *this;}
};

void ProcessFixups(char *FileName,DWORD ImageBase)
{
    HANDLE HF=CreateFile(FileName,GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

    HANDLE HM=CreateFileMapping(HF,0,PAGE_READWRITE,0,0,0);

    void* Base=MapViewOfFile(HM,FILE_MAP_WRITE,0,0,0);

    if(!HF || !HM || !Base)
    {
		printf("Error! ProcessFixups: cannot map %s\n",FileName);
        exit(1);
    }
	Data=2+(char*)Base;
	Count=*(WORD*)Data;
	Data+=2;
	char *DataEnd = Data + Count;
	PrevDWord=0;
Next:

	while(Data < DataEnd)
	{
		BYTE Type=(*Data>>7)&1;

		switch(Type)
		{
		case 0:
			ProcessCase0();
			break;
		case 1:
			ProcessCase1();
			break;			
		};
	};

	if(!IsBadReadPtr(Data,2) && (*Data || *(Data+1)))
	{
		Data+=2;
		Count=*(WORD*)Data;
		Data+=2;
		DataEnd = Data + Count;
		PrevDWord=0;
		goto Next;
	}

	Sasha::TList<RelocBlock> RelocsSet;
	DWORD TotalSize=0;
	for(int i=0; i<Relocations.Count;)
	{
		RelocBlock R;
		R.Rel.VirtualAddress=(Relocations[i]+ImageBase) & 0xFFFF000;
		R.Rel.SizeOfBlock=10;
		R.Relocs.Add(((Relocations[i]+ImageBase) & 0xFFF) | 0x3000);
		i++;
		while(i<Relocations.Count && R.Rel.VirtualAddress==((Relocations[i]+ImageBase) & 0xFFFF000))
		{
			R.Rel.SizeOfBlock+=2;
			R.Relocs.Add(((Relocations[i]+ImageBase) & 0xFFF) | 0x3000);
			i++;
		}
		if(R.Relocs.Count&1)	// make DWORD pading
		{
			R.Rel.SizeOfBlock+=2;
			R.Relocs.Add(0);
		}
		RelocsSet.Add(R);
		TotalSize+=sizeof(IMAGE_BASE_RELOCATION)+2*R.Relocs.Count;
	}

	RelocationsSectionSize=TotalSize+sizeof(IMAGE_BASE_RELOCATION);
	RelocationsSection = (char *)malloc(RelocationsSectionSize);
	memset(RelocationsSection,0,RelocationsSectionSize);

	char *Ptr=RelocationsSection;

	for(int i=0; i<RelocsSet.Count; i++)
	{
		memcpy(Ptr,&(RelocsSet[i].Rel),sizeof(IMAGE_BASE_RELOCATION));
		Ptr+=sizeof(IMAGE_BASE_RELOCATION);
		for(int j=0; j<RelocsSet[i].Relocs.Count; j++)
		{
			*(WORD*)Ptr=RelocsSet[i].Relocs[j];
			Ptr+=2;
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	DWORD Tmp;
//	argv[1]="U:\\PocketPC\\ROM\\BA_WM5\\dump\\commdlg.dll"; argc=2;

	if(argc<2)
	{
		puts("Reconstruct WM5 Module v 1.0 (c) mamaich, 2006\n");
		puts("Usage:\nRecMod.exe [path to module directory]");
		return 1;
	}

	if(argv[1][strlen(argv[1])-1] == '\\')
		argv[1][strlen(argv[1])-1] = 0;

	char OutFileName[1024];
	strcpy(OutFileName,argv[1]);
	char *FileName=strrchr(argv[1],'\\');
	if(FileName==0)
		FileName=argv[1];
	strcat(OutFileName,"\\");
	strcat(OutFileName,FileName);

    HANDLE HO=CreateFile(OutFileName,GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);

	char ImgInfo[1024];
	strcpy(ImgInfo,argv[1]);
	strcat(ImgInfo,"\\imageinfo.bin");

    HANDLE HF=CreateFile(ImgInfo,GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	HANDLE HM=CreateFileMapping(HF,0,PAGE_READONLY,0,0,0);

	e32_rom *e32=(e32_rom*)MapViewOfFile(HM,FILE_MAP_READ,0,0,0);
	o32_rom *o32=(o32_rom*)((char*)e32+(((sizeof(e32_rom)+3)/4)*4));

    if(!HF || !HM || !e32)
    {
        printf("Error! Cannot map %s\n",ImgInfo);
        return 1;
    }

	char RelocsSect[1024];
	sprintf(RelocsSect,"%s\\s%03d",argv[1],e32->e32_objcnt-1);	// I assume that relocations section is appended to end of file
	bool HasFixups=e32->e32_unit[FIX].size;

	WriteFile(HO,MZHeader,sizeof(MZHeader),&Tmp,0);
	WriteFile(HO,"PE\x0",4,&Tmp,0);

	static IMAGE_FILE_HEADER FH;
	FH.Machine=0x1C2;		// THUMB
	FH.NumberOfSections=e32->e32_objcnt;
	FH.TimeDateStamp=e32->e32_timestamp;
	FH.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER);
	FH.Characteristics=e32->e32_imageflags & ~IMAGE_FILE_RELOCS_STRIPPED;

	WriteFile(HO,&FH,sizeof(FH),&Tmp,0);

	static IMAGE_OPTIONAL_HEADER32 OP;
	OP.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	OP.MajorLinkerVersion=7;
	OP.SizeOfCode=e32->e32_vsize;				// FIXME: Calculate real sizes
	OP.SizeOfInitializedData=e32->e32_vsize;
	OP.SizeOfUninitializedData=0;
	OP.AddressOfEntryPoint=e32->e32_entryrva;
	OP.BaseOfCode=0x1000;						// FIXME: Calculate real offsets
	OP.BaseOfData=0x1000;
	OP.ImageBase=e32->e32_vbase;
	OP.SectionAlignment=0x1000;
	OP.FileAlignment=0x200;
	OP.MajorOperatingSystemVersion=4;
	OP.MinorOperatingSystemVersion=0;
	OP.MajorSubsystemVersion=e32->e32_subsysmajor;
	OP.MinorSubsystemVersion=e32->e32_subsysminor;
	OP.SizeOfHeaders=e32->e32_objcnt*sizeof(IMAGE_SECTION_HEADER) + sizeof(MZHeader) + sizeof(IMAGE_NT_HEADERS);
	OP.SizeOfHeaders=((OP.SizeOfHeaders+OP.FileAlignment-1)/OP.FileAlignment)*OP.FileAlignment;
	OP.SizeOfImage=e32->e32_vsize+OP.SizeOfHeaders;				// FIXME: Calculate real size
	OP.Subsystem=e32->e32_subsys;
	OP.SizeOfStackReserve=e32->e32_stackmax;
	OP.SizeOfStackCommit=0x1000;
	OP.SizeOfHeapReserve=0x100000;
	OP.SizeOfHeapCommit=0x1000;
	OP.NumberOfRvaAndSizes=16;
	for(int i=0; i<ROM_EXTRA; i++)
	{
		memcpy(&OP.DataDirectory[i],&e32->e32_unit[i],sizeof(IMAGE_DATA_DIRECTORY));
	}

	WriteFile(HO,&OP,sizeof(OP),&Tmp,0);

	if(HasFixups)
		ProcessFixups(RelocsSect,0);

	int NextSectionPtr=OP.SizeOfHeaders;
	static IMAGE_SECTION_HEADER SC[16];		// WM5 allows max 16 sections per ROM module
	for(int i=0; i<e32->e32_objcnt; i++)
	{
		if(o32[i].o32_rva==OP.DataDirectory[RES].VirtualAddress)
			strcpy((char*)SC[i].Name,".rsrc");
		else if(o32[i].o32_rva==OP.DataDirectory[IMP].VirtualAddress)
			strcpy((char*)SC[i].Name,".idata");
		else if((o32[i].o32_flags&0xF0000000)==0x60000000)
		{
			strcpy((char*)SC[i].Name,".text\0\0");
			if(i)
				SC[i].Name[5]=i+'0';	
		}
		else if((o32[i].o32_flags&0xF0000000)==0xc0000000 && o32[i].o32_vsize<0x10)
			strcpy((char*)SC[i].Name,".CRT");
		else if((o32[i].o32_flags&0xF0000000)==0xc0000000)
			strcpy((char*)SC[i].Name,".data");
		else if((o32[i].o32_flags&0xFF000000)==0x42000000)
			strcpy((char*)SC[i].Name,".init");
		else if((o32[i].o32_flags&0xF0000000)==0x40000000)
			strcpy((char*)SC[i].Name,".pdata");
		else
		{
			strcpy((char*)SC[i].Name,".sect0");
			SC[i].Name[5]+=i;	
		}
		if(i==e32->e32_objcnt-1 && HasFixups)
			strcpy((char*)SC[i].Name,".reloc");

		SC[i].VirtualAddress=o32[i].o32_rva;
		SC[i].Misc.VirtualSize=o32[i].o32_vsize;
		if(i==e32->e32_objcnt-1 && HasFixups)
			SC[i].Misc.VirtualSize = RelocationsSectionSize;
		SC[i].SizeOfRawData=((o32[i].o32_psize+OP.FileAlignment-1)/OP.FileAlignment)*OP.FileAlignment;
		if(i==e32->e32_objcnt-1 && HasFixups)
			SC[i].SizeOfRawData=((RelocationsSectionSize+OP.FileAlignment-1)/OP.FileAlignment)*OP.FileAlignment;;
		SC[i].PointerToRawData=NextSectionPtr;
		SC[i].Characteristics=o32[i].o32_flags & ~0x2002;
		WriteFile(HO,&SC[i],sizeof(SC[i]),&Tmp,0);
		NextSectionPtr=NextSectionPtr+SC[i].SizeOfRawData;
	}

	for(int i=0; i<e32->e32_objcnt; i++)
	{
		SetFilePointer(HO,SC[i].PointerToRawData,0,FILE_BEGIN);
		if(i==e32->e32_objcnt-1 && HasFixups)
		{
			WriteFile(HO,RelocationsSection,RelocationsSectionSize,&Tmp,0);
			continue;
		}
		char Sect[1024];
		sprintf(Sect,"%s\\s%03d",argv[1],i);
		HANDLE HF=CreateFile(Sect,GENERIC_READ,
			FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
		HANDLE HM=CreateFileMapping(HF,0,PAGE_READONLY,0,0,0);
		void *Base=MapViewOfFile(HM,FILE_MAP_READ,0,0,0);
		DWORD Size=GetFileSize(HF,0);
		WriteFile(HO,Base,Size,&Tmp,0);
	}
	DWORD FSize=SetFilePointer(HO,0,0,FILE_CURRENT);
	SetFilePointer(HO,NextSectionPtr,0,FILE_BEGIN);
	SetEndOfFile(HO);

	CloseHandle(HO);

// Pass 2. Rebase DLL, calc sizes.

    HO=CreateFile(OutFileName,GENERIC_WRITE|GENERIC_READ,
        FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);

	HM=CreateFileMapping(HO,0,PAGE_READWRITE,0,0,0);

	void *Base=MapViewOfFile(HM,FILE_MAP_READ|FILE_MAP_WRITE,0,0,0);

    if(!HO || !HM || !Base)
    {
        printf("Error! Cannot map %s\n",OutFileName);
        return 1;
    }

	IMAGE_NT_HEADERS *PE=(IMAGE_NT_HEADERS*)(sizeof(MZHeader)+(char*)Base);

	PE->OptionalHeader.SizeOfImage=SC[e32->e32_objcnt-1].VirtualAddress+((SC[e32->e32_objcnt-1].Misc.VirtualSize+OP.SectionAlignment-1)/OP.SectionAlignment)*OP.SectionAlignment;
	PE->OptionalHeader.SizeOfCode=((SC[0].Misc.VirtualSize+OP.FileAlignment-1)/OP.FileAlignment)*OP.FileAlignment;
	if(e32->e32_objcnt>1)
	{
		PE->OptionalHeader.SizeOfInitializedData=((SC[1].Misc.VirtualSize+OP.FileAlignment-1)/OP.FileAlignment)*OP.FileAlignment;
		PE->OptionalHeader.BaseOfData=SC[1].VirtualAddress;
	}

	DWORD OldBase=PE->OptionalHeader.ImageBase;
	PE->OptionalHeader.ImageBase = 0x10000000;

	for(int i=0; i<Relocations.Count; i++)
	{
		DWORD *Ptr=(DWORD*)ImageRvaToVa(PE,Base,Relocations[i],0);
		DWORD DestAddr=*Ptr;
// Search for section data points to. We can take it from compressed relocs, but better calculate ourselves

		int DestSect = -1;
		for(int j=0; j<e32->e32_objcnt; j++)
		{
			if(DestAddr>=o32[j].o32_realaddr && DestAddr<=o32[j].o32_realaddr+o32[j].o32_vsize)
				DestSect = j;
		}
		if(DestSect==-1)
		{
			printf("Error! Cannot find dest section for address %08X\n",DestAddr);
			exit(1);
		}
		*Ptr=*Ptr - o32[DestSect].o32_realaddr + o32[DestSect].o32_rva + PE->OptionalHeader.ImageBase;
	}
	if(HasFixups)
	{
		PE->OptionalHeader.DataDirectory[FIX].VirtualAddress=SC[e32->e32_objcnt-1].VirtualAddress;
		PE->OptionalHeader.DataDirectory[FIX].Size=RelocationsSectionSize;
	}

	return 0;
}
