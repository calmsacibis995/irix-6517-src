/* bootInit.c - ROM initialization module */

/* Copyright 1989-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
03h,22aug95,hdn  added support for I80X86.
03g,14mar95,caf  restored mips resident rom support (SPR #3856).
03f,16feb95,jdi  doc format change.
03e,09dec94,caf  undid mod 03a, use sdata for resident roms (SPR #3856).
03d,22jun92,caf  undid 16-byte alignment portion of mod 03c, below.
03c,14jun94,cd   corrected definitions of etext, edata and end.
	   +caf  for R4000 resident ROMs: data starts on 16-byte boundary.
		 for R4000 uncompressed ROMs: added volatile to absEntry type.
03b,25oct93,cd   for R4000, work around copy-back cache.
03a,21oct93,caf  for resident roms: use RAM_DST_ADRS instead of &sdata.
		 fixed "&etext + <offset>" for MIPS resident ROM.
02z,12sep93,caf  for MIPS bootroms: forced uncompress() call to be made by
		 absolute address, defined binArrayStart and binArrayEnd
		 because tools don't prepend "_", stopped clearing parity
		 because romInit() handles it.
02y,02mar93,yao  fixed "&etext + <offset>" for SPARC ROM-resident stack; '93.
02x,20jan93,jdi  documentation cleanup for 5.1.
02w,23oct92,jcf  limited conditional to not use volatile to SPARC only.
02v,23oct92,wmd  conditional to not use volatile for 960.
02u,20oct92,ccc  changed I960 to call sysInitAlt().
02t,22sep92,jcf  added volatile to absEntry type to avoid relative addressing.
02s,22sep92,jcf  cleaned up.
02r,26may92,rrr  the tree shuffle
02q,13apr92,wmd  moved defines of STACK_SAVE, and RESERVED to configAll.h,
                 changed area to be cleared for ROM_RESIDENT case.  Added
		 define of HIGH_RAM_OFFSET for i960, use of start for i960.
02p,30mar92,hdn  fixed a ROM_RESIDENT version of clearing memory.
02o,28oct91,wmd  changed conditional flags CPU=I960CA to CPU_FAMILY==I960.
02n,18oct91,yao  removed bcopyBytes() and bfillBytes().
02m,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
02l,26sep91,yao  defined reserved space for 68302. changed to define
		 RAM_DST_ADRS where bootrom is uncompressed or copied to.
02k,06jul91,yao  modified for I960. cleanup.
02j,21may91,jwt  modified slightly for SPARC - reserved space for reset status.
02i,29apr91,hdn  added TRON specific stuff.
02h,05arp91,jdi  documentation cleanup; doc review by dnw.
02g,26feb91,shl  added support for resident ROM.
	   +tja
02f,25oct90,dnw  documentation.
02e,03oct90,yao  changed INCLUDE_COMPRESSED_ROM to UNCOMPRESS.
02d,26sep90,yao  added uncompressed bootrom scheme.
02c,14sep90,dab  changed value of STACK_SAVE from 20 to 40.
02b,08jul90,gae  fixed calculation of size of memory to clear for
		 boards without base address at zero.
		 startType parameter is tested only for BOOT_CLEAR bit.
02a,16may90,gae  revised initial bootrom uncompression scheme --
		   uncompress routines in separate module.
01a,21apr90,rdc  modified from compress source.
*/

/*
DESCRIPTION
This module provides a generic boot ROM facility.  The target-specific
romInit.s module performs the minimal preliminary board initialization and
then jumps to the C routine romStart().  This routine, still executing out
of ROM, copies the first stage of the startup code to a RAM address and
jumps to it.  The next stage clears memory and then uncompresses the
remainder of ROM into the final VxWorks ROM image in RAM.

A modified version of the Public Domain \f3compress\fP program is used to
uncompress the VxWorks boot ROM executable linked with it.  Compressing
object code typically achieves 40% compression, permitting much
larger systems to be burned into ROM.  The only expense is the added few
seconds delay while the first two stages complete.

ROM AND RAM MEMORY LAYOUT
Example memory layout for a 1-megabyte board:
.bS 22
    --------------  0x00100000 = LOCAL_MEM_SIZE = sysMemTop()
    |            |
    |    RAM     |
    |  0 filled  |
    |            |
    |------------| = (romInit+ROM_COPY_SIZE) or binArrayStart
    | ROM image  |
    |----------- |  0x00090000  = RAM_HIGH_ADRS
    | STACK_SAVE |
    |------------|
    |            |  0x00080000  = 0.5 Megabytes
    |            |
    |            |
    | 0 filled   |
    |            |
    |            |  0x00001000  = RAM_ADRS & RAM_LOW_ADRS
    |            |
    |            |  exc vectors, bp anchor, exc msg, bootline
    |            |
    |            |
    --------------  0x00000000  = LOCAL_MEM_LOCAL_ADRS
.bE
.bS 6
    --------------
    |    ROM     |
    |            |  0xff8xxxxx  = binArrayStart
    |            |
    |            |  0xff800008  = ROM_TEXT_ADRS
    --------------  0xff800000  = ROM_BASE_ADRS
.bE

SEE ALSO:
compress(), romInit()

AUTHOR
The original compress program was written by Spencer W. Thomas, Jim McKie,
Steve Davies, Ken Turkowski, James A. Woods, Joe Orost.
*/

#include "vxworks.h"
#include "syslib.h"
#include "config.h"


/* This is a hack to fix the bootrom build process.  The actual real
   TTYfds is defined in sgi/src/tty.c but that file does not get 
   linked into the bootrom.  Nevertheless, libI80486gnuvx.a gets linked
   in and uses TTYfds.  Therefore I just include a fake one here, but
   only if the bootrom is getting compiled
   */

#ifdef BOOTROM
#include "tty.h"
int TTYfds[FFSC_NUM_TTYS];	/* Serial port TTYs */
#endif /* BOOTROM */


IMPORT void	romInit ();
IMPORT STATUS	uncompress ();
IMPORT void	usrInit ();
IMPORT void	sysInitAlt ();
IMPORT void	start ();

#if	(CPU_FAMILY == MIPS)
#define	binArrayStart	_binArrayStart
#define	binArrayEnd	_binArrayEnd
#define	RESIDENT_DATA	RAM_DST_ADRS
#else
#define	RESIDENT_DATA	(&sdata)
IMPORT char	sdata;			/* defined in romInit.s */
#endif

IMPORT UCHAR	binArrayStart [];	/* compressed binary image */
IMPORT UCHAR	binArrayEnd;		/* end of compressed binary image */
IMPORT char	etext [];		/* defined by the loader */
IMPORT char	edata [];		/* defined by the loader */
IMPORT char	end [];			/* defined by the loader */


#ifndef RAM_DST_ADRS                	/* default uncompress dest. */
#define RAM_DST_ADRS        RAM_HIGH_ADRS
#endif

#if	defined (UNCOMPRESS) || defined (ROM_RESIDENT)
#define	ROM_COPY_SIZE	(ROM_SIZE - (ROM_TEXT_ADRS - ROM_BASE_ADRS))
#endif	/* UNCOMPRESS */

#define ROM_OFFSET(adr)	(((UINT)adr - (UINT)romInit) + ROM_TEXT_ADRS)

/* forward declarations */

LOCAL void bcopyLongs ();
LOCAL void bfillLongs ();


/*******************************************************************************
*
* romStart - generic ROM initialization
*
* This is the first C code executed after reset.
*
* This routine is called by the assembly start-up code in romInit().
* It clears memory, copies ROM to RAM, and possibly invokes the uncompressor.
* It then jumps to the entry point of the uncompressed object code.
*
* RETURNS: N/A
*/

void romStart
    (
    FAST int startType		/* start type */
    )

    {
#if ((CPU_FAMILY==SPARC) || (CPU_FAMILY==MIPS) || (CPU_FAMILY==I80X86))
    volatile			/* to force absolute adressing */
#endif /* (CPU_FAMILY==SPARC) */
    FUNCPTR absEntry;		/* to avoid PC Relative Jump Subroutine */

    /*
     * Copy from ROM to RAM, minus the compressed image
     * if compressed boot ROM which relies on binArray
     * appearing last in DATA segment.
     */

#ifdef ROM_RESIDENT
    /* If ROM resident code, then copy only data segment
     * from ROM to RAM, initialize memory and jump
     * to usrInit.
     */

    
#if  (CPU_FAMILY == SPARC)
     bcopyLongs (((UINT) etext + 8), (UINT) RESIDENT_DATA,
#elif	(CPU_FAMILY == MIPS)
    bcopyLongs (((UINT) etext + 0), (UINT) RESIDENT_DATA,
#else
     bcopyLongs (((UINT) etext + 4), (UINT) RESIDENT_DATA,
#endif
		 ((UINT) edata - (UINT) RESIDENT_DATA) / sizeof (long));

#else	/* ROM_RESIDENT */

#ifdef UNCOMPRESS

#if	(CPU_FAMILY == MIPS)
    /*
     * copy text to uncached locations to avoid problems with
     * copy back caches
     */
    ((FUNCPTR)ROM_OFFSET(bcopyLongs)) (ROM_TEXT_ADRS, (UINT)K0_TO_K1(romInit),
		ROM_COPY_SIZE / sizeof (long));
#else	/* CPU_FAMILY == MIPS */
    ((FUNCPTR)ROM_OFFSET(bcopyLongs)) (ROM_TEXT_ADRS, (UINT)romInit,
		ROM_COPY_SIZE / sizeof (long));
#endif	/* CPU_FAMILY == MIPS */

#else	/* UNCOMPRESS */

#if	(CPU_FAMILY == MIPS)
    /*
     * copy text to uncached locations to avoid problems with
     * copy back caches
     * copy the entire data segment because there is no way to ensure that
     * binArray is the last thing in the data segment because of GP relative
     * addressing
     */
    ((FUNCPTR)ROM_OFFSET(bcopyLongs)) (ROM_TEXT_ADRS, (UINT)K0_TO_K1(romInit),
		((UINT)edata - (UINT)romInit) / sizeof (long));
#else	/* CPU_FAMILY == MIPS */
    ((FUNCPTR)ROM_OFFSET(bcopyLongs)) (ROM_TEXT_ADRS, (UINT)romInit,
		((UINT)binArrayStart - (UINT)romInit) / sizeof (long));
#endif	/* CPU_FAMILY == MIPS */
#endif	/* UNCOMPRESS */
#endif	/* ROM_RESIDENT */


#if	(CPU_FAMILY != MIPS)

    /* clear all memory if cold booting */


    if (startType & BOOT_CLEAR)
	{
#ifdef ROM_RESIDENT
	/* Clear memory not loaded with text & data.
	 *
	 * We are careful about initializing all memory (except
	 * STACK_SAVE bytes) due to parity error generation (on
	 * some hardware) at a later stage.  This is usually
	 * caused by read accesses without initialization.
	 */
	bfillLongs ((UINT *)(LOCAL_MEM_LOCAL_ADRS + RESERVED),
	    ((UINT) RESIDENT_DATA - STACK_SAVE - (UINT)LOCAL_MEM_LOCAL_ADRS -
	     RESERVED) / sizeof(long), 0);
	bfillLongs (((UINT) edata),
	    ((UINT)LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE -
	    ((UINT) edata)) / sizeof(long), 0);

#else	/* ROM_RESIDENT */
	bfillLongs ((UINT *)(LOCAL_MEM_LOCAL_ADRS + RESERVED),
	    ((UINT)romInit-STACK_SAVE-(UINT)LOCAL_MEM_LOCAL_ADRS-RESERVED) /
	    sizeof(long), 0);

#if     defined (UNCOMPRESS)
	bfillLongs (((UINT)romInit + ROM_COPY_SIZE),
	    ((UINT)LOCAL_MEM_LOCAL_ADRS + LOCAL_MEM_SIZE -
	    ((UINT)romInit + ROM_COPY_SIZE)) / sizeof(long), 0);
#else
	bfillLongs ((UINT *)binArrayStart,
	    ((UINT)LOCAL_MEM_LOCAL_ADRS+LOCAL_MEM_SIZE - (UINT)binArrayStart) /
	    sizeof (long), 0);
#endif 	/* UNCOMPRESS */
#endif 	/* ROM_RESIDENT */

	}

#endif	/* CPU_FAMILY != MIPS */

    /* jump to VxWorks entry point (after uncompressing) */

#if	defined (UNCOMPRESS) || defined (ROM_RESIDENT)
#if	(CPU_FAMILY == I960)
    absEntry = (FUNCPTR)sysInitAlt;			/* reinit proc tbl */
#else
    absEntry = (FUNCPTR)usrInit;			/* on to bootConfig */
#endif	/* CPU_FAMILY == I960 */

#else
    {
#if	(CPU_FAMILY == MIPS)
    volatile FUNCPTR absUncompress = (FUNCPTR) uncompress;
    if ((absUncompress) ((UCHAR *)ROM_OFFSET(binArrayStart),
			 (UCHAR *)K0_TO_K1(RAM_DST_ADRS),
			 (int)((UINT)&binArrayEnd - (UINT)binArrayStart)) != OK)
#elif	(CPU_FAMILY == I80X86)
    volatile FUNCPTR absUncompress = (FUNCPTR) uncompress;
    if ((absUncompress) ((UCHAR *)ROM_OFFSET(binArrayStart),
	            (UCHAR *)RAM_DST_ADRS, &binArrayEnd - binArrayStart) != OK)
#else
    if (uncompress ((UCHAR *)ROM_OFFSET(binArrayStart),
	            (UCHAR *)RAM_DST_ADRS, &binArrayEnd - binArrayStart) != OK)
#endif	/* (CPU_FAMILY == MIPS) */
	return;		/* if we return then ROM's will halt */

    absEntry = (FUNCPTR)RAM_DST_ADRS;			/* compressedEntry () */
    }
#endif	/* defined UNCOMPRESS || defined ROM_RESIDENT */

    (absEntry) (startType);
    }
/*******************************************************************************
*
* bcopyLongs - copy one buffer to another a long at a time
*
* This routine copies the first <nlongs> longs from <source> to <destination>.
*/

LOCAL void bcopyLongs (source, destination, nlongs)
    FAST UINT *source;		/* pointer to source buffer      */
    FAST UINT *destination;	/* pointer to destination buffer */
    UINT nlongs;		/* number of longs to copy       */

    {
    FAST UINT *dstend = destination + nlongs;

    while (destination < dstend)
	*destination++ = *source++;
    }
/*******************************************************************************
*
* bfillLongs - fill a buffer with a value a long at a time
*
* This routine fills the first <nlongs> longs of the buffer with <val>.
*/

LOCAL void bfillLongs (buf, nlongs, val)
    FAST UINT *buf;	/* pointer to buffer              */
    UINT nlongs;	/* number of longs to fill        */
    FAST UINT val;	/* char with which to fill buffer */

    {
    FAST UINT *bufend = buf + nlongs;

    while (buf < bufend)
	*buf++ = val;
    }
