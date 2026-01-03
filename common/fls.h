//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#ifndef _FLS_H_
#define _FLS_H_ 

// Boot sector verification
#define BOOT_SIZE               512
#define BS2BYTJMP               0xEB
#define BS3BYTJMP               0xE9
#define BOOTSECTRAILSIGH        0xAA55

#define IS_VALID_BOOTSEC(buffer) ((*(WORD *)((buffer) + BOOT_SIZE - 2) == BOOTSECTRAILSIGH) && ((*(buffer) == BS2BYTJMP) || (*(buffer) == BS3BYTJMP)))

#define FLASH_LAYOUT_SIG "MSFLSH50"
#define FLASH_LAYOUT_SIG_SIZE 8
#define IS_VALID_FLS(buffer) (memcmp ((buffer), FLASH_LAYOUT_SIG, FLASH_LAYOUT_SIG_SIZE) == 0)


//--------------------------- Structure Definitions -----------------------------

typedef enum  _REGION_TYPE { XIP, READONLY_FILESYS, FILESYS } REGION_TYPE;

// For dwNumLogicalBlocks, indicating flash region extends to end of flash
#define END_OF_FLASH (DWORD)-1

// Either dwStartPhysBlock/dwNumPhysicalBlocks or dwNumLogicalBlocks can be specified,
// not both.  The field(s) not being used should be marked FIELD_NOT_IN_USE
#define FIELD_NOT_IN_USE 0

typedef struct _FlashRegion
{
    REGION_TYPE regionType;
    DWORD       dwStartPhysBlock;
    DWORD       dwNumPhysBlocks;
    DWORD       dwNumLogicalBlocks;
    DWORD       dwSectorsPerBlock;
    DWORD       dwBytesPerBlock;
    DWORD       dwCompactBlocks;

}FlashRegion, *PFlashRegion;

#define RESERVED_NAME_LEN 8

typedef struct _ReservedEntry {
    CHAR szName[RESERVED_NAME_LEN];
    DWORD dwStartBlock;
    DWORD dwNumBlocks;
} ReservedEntry, *PReservedEntry;

typedef struct _FlashLayoutSector {
    // Signiture to identify this sector
    BYTE abFLSSig[FLASH_LAYOUT_SIG_SIZE];		
    // Size in bytes of reserved entries array
    DWORD cbReservedEntries; 
    // Size in bytes of region array
    DWORD cbRegionEntries;
    // ReservedEntry[] and FlashRegion[] are located
    // immediately after this struct.
} FlashLayoutSector, *PFlashLayoutSector;



#endif //_FLS_H_

