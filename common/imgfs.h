#include <pshpack1.h>
#define FILE_ATTRIBUTE_ROMMODULE            0x00002000
#define FILE_ATTRIBUTE_INROM                0x00000040

struct FS_BOOT_SECTOR {
        _GUID guidBootSignature;
        DWORD dwFSVersion;
        DWORD dwSectorsPerHeaderBlock;
        DWORD dwRunsPerFileHeader;
        DWORD dwBytesPerHeader;
        DWORD dwChunksPerSector;
        DWORD dwFirstHeaderBlockOffset;
        DWORD dwDataBlockSize;
        char szCompressionType[4];
        DWORD dwFreeSectorCount;
        DWORD dwHiddenSectorCount;
        DWORD dwUpdateModeFlag;
};

struct FS_BLOCK_HEADER {
        DWORD dwBlockSignature;
        DWORD dwNextHeaderBlock;
};

struct FS_LONG_NAME {
	DWORD dwHashValue;
	DWORD* pszCachedName;
	DWORD dwNameOffset;
};

struct FS_SHORT_NAME {
	WORD cchName;
	WORD wFlags;	// 2 when dwFullNameOffset points to FS_LOCAL_NAME header, otherwize it points to unicode name in data area
	union // if cchName<=4 it is contaned here
	{
		struct {
			DWORD dwShortName;
			DWORD dwFullNameOffset;
	 	};
	 	wchar_t szShortName[4];
	};	
};

struct FS_DATA_RUN {
	DWORD dwDiskOffset;
	DWORD cbOnDiskSize;
};

// dwType: 0xFFFFF6FD
struct FS_STREAM_HEADER {
	DWORD dwNextDataTableOffset;
	DWORD dwNextStreamHeaderOffset;
	DWORD dwSecNameLen;
	wchar_t szSecName[4];
	DWORD dwStreamSize;
	FS_DATA_RUN dataTable[1];
	DWORD dwUnknown[4];
};

// dwType: 0xFFFFFEFE - modules (dwNextStreamHeaderOffset != 0)
// dwType: 0xFFFFF6FE - data files (dwNextStreamHeaderOffset == 0)
struct FS_FILE_HEADER {
	DWORD dwNextDataTableOffset;
	DWORD dwNextStreamHeaderOffset;
	FS_SHORT_NAME fsName;
	DWORD dwStreamSize;
	DWORD dwFileAttributes;
	FILETIME fileTime;
	DWORD dwReserved;
	FS_DATA_RUN dataTable[1];
};

// ???
struct FS_DATA_TABLE {
	DWORD dwNextDataTableOffset;
	DWORD dwReserved;
	FS_DATA_RUN dataTable[1];
};

// 0xFFFFFEFB
struct FS_LOCAL_NAME {
	wchar_t szName[1];
};

struct FSHEADER {
	DWORD dwHeaderFlags;
	union {
		FS_FILE_HEADER hdrFile;
		FS_STREAM_HEADER hdrStream;
		FS_DATA_TABLE hdrData;
		FS_LOCAL_NAME hdrName;
	};
};

struct FS_ALLOC_TABLE_ENTRY {
	WORD wCompressedBlockSize;
	WORD wDecompressedBlockSize;
	DWORD dwDiskOffset;
};

struct info {                       /* Extra information header block      */
    unsigned long   rva;            /* Virtual relative address of info    */
    unsigned long   size;           /* Size of information block           */
};

#define EXP             0           /* Export table position               */
#define IMP             1           /* Import table position               */
#define RES             2           /* Resource table position             */
#define EXC             3           /* Exception table position            */
#define SEC             4           /* Security table position             */
#define FIX             5           /* Fixup table position                */
#define DEB             6           /* Debug table position                */
#define IMD             7           /* Image description table position    */
#define MSP             8           /* Machine specific table position     */
#define TLS             9           /* Thread Local Storage                */
#define CBK            10           /* Callbacks                           */
#define RS1            11           /* Reserved                            */
#define RS2            12           /* Reserved                            */
#define RS3            13           /* Reserved                            */
#define RS4            14           /* Reserved                            */
#define RS5            15           /* Reserved                            */

#define ROM_EXTRA 9

typedef struct e32_rom {
    unsigned short  e32_objcnt;     /* Number of memory objects            */
    unsigned short  e32_imageflags; /* Image flags                         */
    unsigned long   e32_entryrva;   /* Relative virt. addr. of entry point */
    unsigned long   e32_vbase;      /* Virtual base address of module      */
    unsigned short  e32_subsysmajor;/* The subsystem major version number  */
    unsigned short  e32_subsysminor;/* The subsystem minor version number  */
    unsigned long   e32_stackmax;   /* Maximum stack size                  */
    unsigned long   e32_vsize;      /* Virtual size of the entire image    */
    unsigned long   e32_sect14rva;  /* section 14 rva */
    unsigned long   e32_sect14size; /* section 14 size */
    unsigned long   e32_timestamp;  /* Time EXE/DLL was created/modified   */
    struct info     e32_unit[ROM_EXTRA]; /* Array of extra info units      */
    unsigned short  e32_subsys;     /* The subsystem type                  */
} e32_rom;

typedef struct o32_rom {
    unsigned long       o32_vsize;      /* Virtual memory size              */
    unsigned long       o32_rva;        /* Object relative virtual address  */
    unsigned long       o32_psize;      /* Physical file size of init. data */
    unsigned long       o32_dataptr;    /* Image pages offset               */
    unsigned long       o32_realaddr;   /* pointer to actual                */
    unsigned long       o32_flags;      /* Attribute flags for the object   */
} o32_rom;

typedef struct _ROMINFO {
    DWORD   dwSlot_0_DllBase;
    DWORD   dwSlot_1_DllBase;
    int     nROMs;
    // hi/lo pair follows
    // LARGE_INTEGER    rwInfo[nROMs];
} ROMINFO, *PROMINFO;



// from bootpart.h

// Partition Table
// end of master boot record contains 4 partition entries
typedef struct _PARTENTRY {
        BYTE            Part_BootInd;           // If 80h means this is boot partition
        BYTE            Part_FirstHead;         // Partition starting head based 0
        BYTE            Part_FirstSector;       // Partition starting sector based 1
        BYTE            Part_FirstTrack;        // Partition starting track based 0
        BYTE            Part_FileSystem;        // Partition type signature field
        BYTE            Part_LastHead;          // Partition ending head based 0
        BYTE            Part_LastSector;        // Partition ending sector based 1
        BYTE            Part_LastTrack;         // Partition ending track based 0
        DWORD           Part_StartSector;       // Logical starting sector based 0
        DWORD           Part_TotalSectors;      // Total logical sectors in partition
} PARTENTRY;
typedef PARTENTRY UNALIGNED *PPARTENTRY;

#include <poppack.h>

