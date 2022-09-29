/* mmuLib.h - mmuLib header for i86. */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,07jan95,hdn  added GDT and GDT_ENTRIES.
01b,01nov94,hdn  added MMU_STATE_CACHEABLE_WT for Pentium.
01a,26jul93,hdn  written based on mc68k's version.
*/

#ifndef __INCmmuI86Libh
#define __INCmmuI86Libh

#ifdef __cplusplus
extern "C" {
#endif


#define PAGE_SIZE	4096
#define PAGE_BLOCK_SIZE	4194304

#define DIRECTORY_BITS	0xffc00000
#define TABLE_BITS	0x003ff000
#define OFFSET_BITS	0x00000fff
#define DIRECTORY_INDEX	22
#define TABLE_INDEX	12
#define PTE_TO_ADDR	0xfffff000
#define ADDR_TO_PAGE	12

typedef struct
    {
    unsigned present:1;
    unsigned rw:1;
    unsigned us:1;
    unsigned pwt:1;
    unsigned pcd:1;
    unsigned access:1;
    unsigned dirty:1;
    unsigned zero:2;
    unsigned avail:3;
    unsigned page:20;
    } PTE_FIELD;

typedef union pte
    {
    PTE_FIELD field;
    unsigned int bits;
    } PTE;

typedef struct mmuTransTblStruct
    {
    PTE *pDirectoryTable;
    } MMU_TRANS_TBL;

typedef struct gdt
    {
    unsigned short	limit00;
    unsigned short	base00;
    unsigned char	base01;
    unsigned char	type;
    unsigned char	limit01;
    unsigned char	base02;
    } GDT;


#define MMU_STATE_MASK_VALID		0x001
#define MMU_STATE_MASK_WRITABLE		0x002
#define MMU_STATE_MASK_CACHEABLE	0x018

#define MMU_STATE_VALID			0x001
#define MMU_STATE_VALID_NOT		0x000
#define MMU_STATE_WRITABLE		0x002
#define MMU_STATE_WRITABLE_NOT		0x000
#define MMU_STATE_CACHEABLE		0x008
#define MMU_STATE_CACHEABLE_NOT		0x018

#define MMU_STATE_CACHEABLE_WT		0x008

#define GDT_ENTRIES			5


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS mmuLibInit (int pageSize);
extern STATUS mmuEnable (BOOL enable);
extern void mmuOn ();
extern void mmuOff ();
extern void mmuTLBFlush ();
extern void mmuPdbrSet (MMU_TRANS_TBL *transTbl);
extern MMU_TRANS_TBL *mmuPdbrGet ();

#else   /* __STDC__ */

extern STATUS mmuLibInit ();
extern STATUS mmuEnable ();
extern void mmuOn ();
extern void mmuOff ();
extern void mmuTLBFlush ();
extern void mmuPdbrSet ();
extern MMU_TRANS_TBL *mmuPdbrGet ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmmuI86Libh */
