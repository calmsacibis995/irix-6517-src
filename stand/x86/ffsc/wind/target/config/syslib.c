/* syslib.c - PC 386 system dependent library */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
02d,28sep95,dat  new BSP revision id
02c,27sep95,hdn  fixed a typo by changing IO_ADRS_ULTRA to IO_ADRS_ELC.
02b,14jun95,hdn  added a global variable sysCodeSelector.
		 added a local function sysIntVecSetEnt(), sysIntVecSetExit().
		 renamed pSysEndOfInt to intEOI.
		 moved global function declarations to sysLib.h.
02a,14jun95,myz  moved serial configuration to sysSerial.c
01z,07jan95,hdn  added an accurate memory size checking.
01y,31oct94,hdn  changed sysMemTop() to find out a memory size.
		 deleted sysGDT and used sysGdt in sysALib.s.
		 added the Intel EtherExpress32 driver.
		 deleted a conditional macro for INCLUDE_LPT.
		 swapped 1st and 2nd parameter of fdDevCreate().
		 imported globals to timeout IDE and LPT.
01x,12oct94,hdn  deleted sysBootType.
		 added a conditional macro for INCLUDE_LPT.
01w,29may94,hdn  moved sysCpuProbe() to cacheArchLib.c.
		 added open and read bootrom.dat in sysToMonitor().
01v,22apr94,hdn  moved sysVectorIRQ0 from i8259Pic.c.
		 made new globals sysFdBuf and sysFdBufSize.
		 supported the warm start from the EPROM.
01u,06apr94,hdn  added sysCpuProbe().
01t,17feb94,hdn  deleted memAddToPool() in sysHwInit2().
		 added a conditional statement in sysMemTop().
		 changed sysWarmType 0 to 1.
01s,03feb94,hdn  added MMU conditional macro for the limit in the GDT.
01r,29nov93,hdn  added sysBspRev () routine.
01q,22nov93,hdn  added xxdetach () routine for warm start.
01p,16nov93,hdn  added sysWarmType which controls warm start device.
01o,09nov93,hdn  added warm start (control X).
01n,08nov93,vin  added support pc console drivers.
01m,27oct93,hdn  added memAddToPool stuff to sysHwInit2().
01l,12oct93,hdn  changed PIT_INT_VEC_NUM to PIT_INT_VEC.
01k,05sep93,hdn  moved PIC related functions to intrCtl/i8259Pic.c.
		 added sysDelay ().
01j,12aug93,hdn  changed a global descriptor table sysGDT.
		 deleted sysGDTSet.
01i,11aug93,hdn  added a global sysVectorIRQ0.
01h,03aug93,hdn  changed a mapping IRQ to Vector Table.
01g,26jul93,hdn  added a memory descriptor table sysPhysMemDesc[].
01f,25jun93,hdn  changed sysToMonitor() to call sysReboot.
01e,24jun93,hdn  changed the initialization of PIC.
01d,17jun93,hdn  updated to 5.1.
01c,08apr93,jdi  doc cleanup.
01d,07apr93,hdn  renamed compaq to pc.
01c,26mar93,hdn  added the global descriptor table, memAddToPool.
		 moved enabling A20 to romInit.s. added cacheClear for 486.
01b,18nov92,hdn  supported nested interrupt.
01a,15may92,hdn  written based on frc386 version.
*/

/*
DESCRIPTION
This library provides board-specific routines.  The chip drivers included are:
    i8253Timer.c - i8253 Timer library
    nullNvRam.c  - null NvRam library
    nullVme.c	 - null VME library
    i8259Pic.c	 - i8259 Interrupt Controller library

INCLUDE FILES: sysLib.h

SEE ALSO:
.pG "Configuration"
*/

/* includes */

#include "vxworks.h"
#include "vme.h"
#include "memlib.h"
#include "syslib.h"
#include "string.h"
#include "intlib.h"
#include "config.h"
#include "loglib.h"
#include "tasklib.h"
#include "vxlib.h"
#include "errnolib.h"
#include "dosfslib.h"
#include "stdio.h"
#include "cachelib.h"
#include "private/vmlibp.h"
#include "arch/i86/mmui86lib.h"


/* imports */

IMPORT char end;		  /* end of system, created by ld */
IMPORT GDT sysGdt[];		  /* the global descriptor table */
IMPORT VOIDFUNCPTR intEOI;	  /* pointer to a function sysIntEOI() */
IMPORT VOIDFUNCPTR intVecSetEnt;  /* entry hook for intVecSet() */
IMPORT VOIDFUNCPTR intVecSetExit; /* exit  hook for intVecSet() */

#ifdef INCLUDE_ELC
IMPORT void elcdetach (int unit);
#endif	/* INCLUDE_ELC */

#ifdef INCLUDE_ULTRA
IMPORT void ultradetach (int unit);
#endif	/* INCLUDE_ULTRA */


/* globals */

PHYS_MEM_DESC sysPhysMemDesc [] =
    {
    /* adrs and length parameters must be page-aligned (multiples of 0x1000) */

    /* lower memory */
    {
    (void *) LOCAL_MEM_LOCAL_ADRS,
    (void *) LOCAL_MEM_LOCAL_ADRS,
    0xa0000,
    VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | VM_STATE_MASK_CACHEABLE,
    VM_STATE_VALID      | VM_STATE_WRITABLE      | VM_STATE_CACHEABLE
    },

    /* video ram, etc */
    {
    (void *) 0xa0000,
    (void *) 0xa0000,
    0x60000,
    VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | VM_STATE_MASK_CACHEABLE,
    VM_STATE_VALID      | VM_STATE_WRITABLE      | VM_STATE_CACHEABLE_NOT
    },

    /* upper memory */
    {
    (void *) 0x100000,
    (void *) 0x100000,
    LOCAL_MEM_SIZE - 0x100000,	/* it is changed in sysMemTop() */
    VM_STATE_MASK_VALID | VM_STATE_MASK_WRITABLE | VM_STATE_MASK_CACHEABLE,
    VM_STATE_VALID      | VM_STATE_WRITABLE      | VM_STATE_CACHEABLE
    }
    };

int sysPhysMemDescNumEnt = NELEMENTS (sysPhysMemDesc);


#ifdef	INCLUDE_PC_CONSOLE

PC_CON_DEV	pcConDv [N_VIRTUAL_CONSOLES] = 
    {
	{{{{NULL}}}, FALSE, NULL, NULL}, 
	{{{{NULL}}}, FALSE, NULL, NULL}
    };

#endif	/* INCLUDE_PC_CONSOLE */

#ifdef INCLUDE_FD

FD_TYPE fdTypes[] =
    {
    {2880,18,2,80,2,0x1b,0x54,0x00,0x0c,0x0f,0x02,1,1,"1.44M"},
    {2400,15,2,80,2,0x24,0x50,0x00,0x0d,0x0f,0x02,1,1,"1.2M"},
    };
UINT    sysFdBuf = FD_DMA_BUF;		/* floppy disk DMA buffer address */
UINT    sysFdBufSize = FD_DMA_BUF_SIZE;	/* floppy disk DMA buffer size */

#endif	/* INCLUDE_FD */

#ifdef INCLUDE_IDE

IDE_TYPE ideTypes[] =
    {
    {761, 8, 39, 512, 0xff},		/* drive 0 */
    {0,   0,  0,   0,    0}		/* drive 1 */
    };

#endif	/* INCLUDE_IDE */


int	sysBus		= BUS;		/* system bus type (VME_BUS, etc) */
int	sysCpu		= CPU;		/* system cpu type (MC680x0) */
char	*sysBootLine	= BOOT_LINE_ADRS; /* address of boot line */
char	*sysExcMsg	= EXC_MSG_ADRS;	/* catastrophic message area */
int	sysProcNum;			/* processor number of this cpu */
int	sysFlags;			/* boot flags */
char	sysBootHost [BOOT_FIELD_LEN];	/* name of host from which we booted */
char	sysBootFile [BOOT_FIELD_LEN];	/* name of file from which we booted */
UINT	sysIntIdtType	= 0x0000ef00;	/* trap gate, 0x0000ee00=int gate */
UINT	sysProcessor	= 0;		/* 0=386, 1=486, 2=Pentium */
UINT	sysCoprocessor	= 0;		/* 0=non, 1=387, 2=487 */
int 	sysWarmType	= 1;		/* 0=BIOS, 1=Floppy */
int	sysWarmFdType	= 0;		/* 3.5" 2HD */
int	sysWarmFdDrive	= 0;		/* drive a: */
UINT    sysVectorIRQ0	= INT_VEC_IRQ0;	/* vector for IRQ0 */
UINT	sysStrayIntCount = 0;		/* Stray interrupt count */
char	*memTopPhys	= NULL;		/* top of memory */
GDT	*pSysGdt	= (GDT *)(LOCAL_MEM_LOCAL_ADRS + GDT_BASE_OFFSET);
int	sysCodeSelector	= CODE_SELECTOR;/* code selector for context switch */


/* locals */

LOCAL short *sysRomBase[] = 
    {
    (short *)0xce000, (short *)0xce800, (short *)0xcf000, (short *)0xcf800
    };
#define ROM_SIGNATURE_SIZE	16
LOCAL char sysRomSignature[ROM_SIGNATURE_SIZE] = 
    {
    0x55,0xaa,0x01,0x90,0x90,0x90,0x90,0x90,
    0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90
    };


/* forward declarations */

LOCAL void sysStrayInt		(void);
#if	FALSE
LOCAL void sysIntVecSetEnt	(FUNCPTR *vector, FUNCPTR function);
LOCAL void sysIntVecSetExit	(FUNCPTR *vector, FUNCPTR function);
#endif	/* FALSE */


#include "intrctl/i8259pic.c"
#include "timer/i8253timer.c"
#include "mem/nullnvram.c"
#include "vme/nullvme.c"
#include "sysserial.c"

#ifdef INCLUDE_EEX32
#include "if_eex32.c"
#endif /* INCLUDE_EEX32 */


/*******************************************************************************
*
* sysModel - return the model name of the CPU board
*
* This routine returns the model name of the CPU board.
*
* RETURNS: A pointer to the string "PC 386".
*/

char *sysModel (void)

    {
#if	CPU==I80486
    return ("PC 486");
#else
    return ("PC 386");
#endif	/* CPU==I80486 */
    }

/*******************************************************************************
*
* sysBspRev - return the bsp version with the revision eg 1.1/<x>
*
* This function returns a pointer to a bsp version with the revision.
* for eg. 1.1/<x>. BSP_REV is concatanated to BSP_VERSION to form the
* BSP identification string.
*
* RETURNS: A pointer to the BSP version/revision string.
*/

char * sysBspRev (void)
    {
    return (BSP_VERSION BSP_REV);
    }

/*******************************************************************************
*
* sysHwInit - initialize the CPU board hardware
*
* This routine initializes various features of the 386.
* Normally, it is called from usrInit() in usrConfig.c.
*
* NOTE: This routine should not be called directly by the user.
*
* RETURNS: N/A
*/

void sysHwInit (void)

    {
#ifdef	INCLUDE_IDE
    {
    IMPORT int ideWdSec;
    IMPORT int ideSemSec;

    ideWdSec	= 5;		/* 5 seconds to timeout initialize */
    ideSemSec	= 5;		/* 5 seconds to timeout each IDE command */
    }
#endif	/* INCLUDE_IDE */

#ifdef	INCLUDE_LPT
    {
    IMPORT int lptTimeout;

    lptTimeout	= 300;		/* 300 seconds to timeout write () */
    }
#endif	/* INCLUDE_LPT */

    /* initialize the PIC (Programmable Interrupt Controller) */

    sysIntInitPIC ();

    /* set the global function pointer to sysIntEOI() */

    intEOI		= sysIntEOI;

#if	FALSE
    intVecSetEnt	= sysIntVecSetEnt;
    intVecSetExit	= sysIntVecSetExit;
#endif	/* FALSE */

    /* initializes the serial devices */

    sysSerialHwInit ();      /* initialize serial data structure */
    }
/*******************************************************************************
*
* sysHwInit2 - additional system configuration and initialization
*
* This routine connects system interrupts and does any additiona
* configuration necessary.
*
* RETURNS: N/A
*
* NOMANUAL
*/

void sysHwInit2 (void)

    {

    /* connect sys clock interrupt and auxiliary clock interrupt*/

    (void)intConnect (INUM_TO_IVEC (PIT_INT_VEC), sysClkInt, 0);
    (void)intConnect (INUM_TO_IVEC (RTC_INT_VEC), sysAuxClkInt, 0);

    /* connect serial interrupt */  

    sysSerialHwInit2();

    /* connect stray interrupt XXX */  

    (void)intConnect (INUM_TO_IVEC(LPT_INT_VEC), sysStrayInt, 0);

#ifdef	INCLUDE_PC_CONSOLE

    /* connect keyboard Controller 8042 chip interrupt */

    (void) intConnect (INUM_TO_IVEC (KBD_INT_VEC), kbdIntr, 0);

#endif	/* INCLUDE_PC_CONSOLE */

    }

/*******************************************************************************
*
* sysMemTop - get the address of the top of memory
*
* This routine returns the address of the first missing byte of memory,
* which indicates the top of memory.
*
* Memory probing begins at the end of BSS; at every 4K boundary a byte
* is read until it finds one that cannot be read, or
* 4 Mbytes have been probed, whichever is first.
*
* RETURNS: The address of the top of memory.
*
* INTERNAL
* This routine is used by sysHwInit() to differentiate between models.
* It is highly questionable whether vxMemProbe() can be used during the
* initial stage of booting.
*/

char *sysMemTop (void)

    {
#define TEST_PATTERN	0x12345678
#define SYS_PAGE_SIZE	0x10000
#define N_TIMES		3

    static char *memTop = NULL;			/* top of memory */
    PHYS_MEM_DESC *pMmu = &sysPhysMemDesc[2];	/* upper memory */
    int delta		= SYS_PAGE_SIZE;
    BOOL found		= FALSE;
    GDT *pGdt		= pSysGdt;
    int temp[N_TIMES];
    char gdtr[6];
    int limit;
    char *p;
    int ix;

    if (memTop != NULL)
	return (memTop);

    /* if (&end) is in upper memory, we assume it is VxWorks image.
     * if not, it is Boot image */

    if ((int)&end > 0x100000)
        p = (char *)(((int)&end + (delta - 1)) & (~ (delta - 1)));
    else
	p = (char *)0x100000;

    /* find out the actual size of the memory (max 1GB) */

    for (; (int)p < 0x40000000; p += delta)
	{
	for (ix=0; ix<N_TIMES; ix++)			/* save and write */
	    {
	    temp[ix] = *((int *)p + ix);
	    *((int *)p + ix) = TEST_PATTERN;
	    }
	cacheFlush (DATA_CACHE, p, 4 * sizeof(int));	/* for 486, Pentium */

	if (*(int *)p != TEST_PATTERN)			/* compare */
	    {
	    p -= delta;
	    if (delta == VM_PAGE_SIZE)
		{
                memTop = p;
	        found = TRUE;
	        break;
		}
	    delta = VM_PAGE_SIZE;
	    }

	for (ix=0; ix<N_TIMES; ix++)			/* restore */
	    *((int *)p + ix) = temp[ix];
	}
    
    if (!found)		/* we are fooled by write-back external cache */
        memTop = (char *)(LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE);

    /* copy the global descriptor table from RAM/ROM to RAM */

    bcopy ((char *)sysGdt, (char *)pSysGdt, GDT_ENTRIES * sizeof(GDT));
    *(short *)&gdtr[0]	= GDT_ENTRIES * sizeof(GDT) - 1;
    *(int *)&gdtr[2]	= (int)pSysGdt;

    limit = (((int)memTop) / 0x1000 - 1);
    for (ix=1; ix < GDT_ENTRIES; ix++)
	{
	pGdt++;
	pGdt->limit00 = limit & 0x0ffff;
	pGdt->limit01 = ((limit & 0xf0000) >> 16) | (pGdt->limit01 & 0xf0);
	}

    /* load the global descriptor table. set the MMU table */

    sysLoadGdt (gdtr);
    pMmu->len = (UINT)memTop - (UINT)pMmu->physicalAddr;

    memTopPhys = memTop;		/* set the real memory size */

    if ((int)&end < 0x100000)		/* this is for bootrom */
        memTop = (char *)0xa0000;
    
    return (memTop);
    }

/*******************************************************************************
*
* sysToMonitor - transfer control to the ROM monitor
*
* This routine transfers control to the ROM monitor.  It is usually called
* only by reboot() -- which services ^X -- and bus errors at interrupt
* level.  However, in some circumstances, the user may wish to introduce a
* new <startType> to enable special boot ROM facilities.
*
* RETURNS: Does not return.
*/

STATUS sysToMonitor
    (
    int startType   /* passed to ROM to tell it how to boot */
    )
    {
    FUNCPTR pEntry;
    int ix;
    int iy;
    int iz;
    char buf[ROM_SIGNATURE_SIZE];
    short *pSrc;
    short *pDst;

    VM_ENABLE (FALSE);			/* disbale MMU */

    /* decide a destination RAM address and the entry point */

    if ((int)&end > 0x100000)
	{
	pDst = (short *)RAM_HIGH_ADRS;	/* we are in vxWorks image */
	pEntry = (FUNCPTR)(RAM_HIGH_ADRS + 0x4);
	}
    else
	{
	pDst = (short *)RAM_LOW_ADRS;	/* we are in bootrom image */
	pEntry = (FUNCPTR)(RAM_LOW_ADRS + 0x14);
	}

    /* disable 16-bit memory access */

#ifdef  INCLUDE_ULTRA
    sysOutByte (IO_ADRS_ULTRA + 4, sysInByte (IO_ADRS_ULTRA + 4) | 0x80);
    sysOutByte (IO_ADRS_ULTRA + 5, sysInByte (IO_ADRS_ULTRA + 5) & ~0x80);
#endif  /* INCLUDE_ULTRA */

#ifdef  INCLUDE_ELC
    sysOutByte (IO_ADRS_ELC + 5, sysInByte (IO_ADRS_ELC + 5) & ~0x80);
#endif  /* INCLUDE_ELC */

    /* copy EPROM to RAM and jump, if there is a VxWorks EPROM */

    for (ix = 0; ix < NELEMENTS(sysRomBase); ix++)
	{
	bcopyBytes ((char *)sysRomBase[ix], buf, ROM_SIGNATURE_SIZE);
	if (strncmp (sysRomSignature, buf, ROM_SIGNATURE_SIZE) == 0)
	    {
	    for (iy = 0; iy < 1024; iy++)
		{
		*sysRomBase[ix] = iy;		/* map the moveable window */
		pSrc = (short *)((int)sysRomBase[ix] + 0x200);
	        for (iz = 0; iz < 256; iz++)
		    *pDst++ = *pSrc++;
		}
	    (*pEntry) (startType);
	    }
	}

#ifdef INCLUDE_FD
    if (sysWarmType == 1)
	{
        BLK_DEV *pBlkDev;
        int fd;

	/* initialize floppy driver if we are in boot image */

	if ((int)&end < 0x100000)
	    {
            fdDrv (FD_INT_VEC, FD_INT_LVL);     /* initialize floppy disk */
    	    dosFsInit (NUM_DOSFS_FILES);        /* initialize DOS-FS */
	    }

        /* create a floppy device, initialize it as a raw device */

        if ((pBlkDev = fdDevCreate (sysWarmFdDrive, sysWarmFdType, 0, 0)) == 
	   NULL)
	    {
	    printErr ("Error during fdDevCreate: %x\n", errnoGet ());
	    return (ERROR);
	    }

        if ((dosFsDevInit ("/vxboot/", pBlkDev, NULL)) == NULL)
	    {
	    printErr ("Error during dosFsDevInit: %x\n", errnoGet ());
	    return (ERROR);
	    }

        if ((fd = open ("/vxboot/bootrom.sys", O_RDWR, 0644)) == ERROR)
	    {
            if ((fd = open ("/vxboot/bootrom.dat", O_RDWR, 0644)) == ERROR)
		{
                printErr ("Can't open \"%s\"\n", "bootrom.{sys,dat}");
	        return (ERROR);
		}
            if (read (fd, (char *)pDst, 0x20) == ERROR) /* a.out header */
	        {
	        printErr ("Error during read file: %x\n", errnoGet ());
	        return (ERROR);
                }
	    }

        /* read text and data, write them to the memory */

        if (read (fd, (char *)pDst, 0x98000) == ERROR)
	    {
	    printErr ("Error during read file: %x\n", errnoGet ());
	    return (ERROR);
            }

        close (fd);

        (*pEntry) (startType);
	}
#endif	/* INCLUDE_FD */

    intLock ();

#ifdef INCLUDE_ELC
    elcdetach (0);
#endif /* INCLUDE_ELC */

#ifdef INCLUDE_ULTRA
    ultradetach (0);
#endif /* INCLUDE_ULTRA */

    sysClkDisable ();

    sysWait ();
    sysOutByte (COMMAND_8042, 0xfe);	/* assert SYSRESET */
    sysWait ();
    sysOutByte (COMMAND_8042, 0xff);	/* NULL command */

    sysReboot ();			/* crash the global descriptor table */

    return (OK);	/* in case we ever continue from ROM monitor */
    }

/*******************************************************************************
*
* sysIntDisable - disable a bus interrupt level
*
* This routine disables a specified bus interrupt level.
*
* RETURNS: ERROR.
*
* ARGSUSED0
*/

STATUS sysIntDisable
    (
    int intLevel	/* interrupt level to disable */
    )
    {
    return (ERROR);
    }
/*******************************************************************************
*
* sysIntEnable - enable a bus interrupt level
*
* This routine enables a specified bus interrupt level.
*
* RETURNS: ERROR.
*
* ARGSUSED0
*/

STATUS sysIntEnable
    (
    int intLevel	/* interrupt level to enable */
    )
    {
    return (ERROR);
    }

/****************************************************************************
*
* sysProcNumGet - get the processor number
*
* This routine returns the processor number for the CPU board, which is
* set with sysProcNumSet().
*
* RETURNS: The processor number for the CPU board.
*
* SEE ALSO: sysProcNumSet()
*/

int sysProcNumGet (void)

    {
    return (sysProcNum);
    }
/****************************************************************************
*
* sysProcNumSet - set the processor number
*
* Set the processor number for the CPU board.  Processor numbers should be
* unique on a single backplane.
*
* NOTE
* By convention, only processor 0 should dual-port its memory.
*
* RETURNS: N/A
*
* SEE ALSO: sysProcNumGet()
*/

void sysProcNumSet
    (
    int procNum		/* processor number */
    )
    {
    sysProcNum = procNum;
    }

/*******************************************************************************
*
* sysDelay - allow recovery time for port accesses.
*
* Allow recovery time for port accesses.
*/

void sysDelay (void)
    {
    char ix;

    ix = sysInByte (0x84);	/* it takes 720ns */
    }

/*******************************************************************************
*
* sysStrayInt - Do nothing for stray interrupts.
*
* Do nothing for stray interrupts.
*/

LOCAL void sysStrayInt (void)
    {

    sysStrayIntCount++;
    }


#if	FALSE

/*******************************************************************************
*
* sysIntVecSetEnt - entry hook routine for intVecSet()
*
* This is an entry hook routine for intVecSet()
*
* RETURNS: N/A
*/

LOCAL void sysIntVecSetEnt
    (
    FUNCPTR *vector,		/* vector offset	      */
    FUNCPTR function		/* address to place in vector */
    )

    {
    }

/*******************************************************************************
*
* sysIntVecSetExit - exit hook routine for intVecSet()
*
* This is an exit hook routine for intVecSet()
*
* RETURNS: N/A
*/

LOCAL void sysIntVecSetExit
    (
    FUNCPTR *vector,		/* vector offset	      */
    FUNCPTR function		/* address to place in vector */
    )

    {
    }

#endif	/* FALSE */
