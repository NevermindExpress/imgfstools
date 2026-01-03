#include <pshpack1.h>

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

#include <poppack.h>

