/* usrMmuInit.c - memory management unit initialization */

/* Copyright 1992-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
o1j,06nov94,tmk  added MC68LC040 support.
01i,16jun94,tpr  added MC68060 cpu support.
01i,13nov93,hdn  added support for I80X86.
01i,02dec93,pme  added Am29K family support.
01h,30jul93,jwt  changed CPU to CPU_FAMILY for SPARC; include sun4.h to BSP.
01g,27feb93,rdc  removed check for both BASIC and FULL (done in usrDepend.c).
01f,13feb93,jcf  changed SPARC mmu initialization to utilize sysMmuLibInit.
01e,16oct92,jwt  included sun4.h to pull in prototype for mmuSun4LibInit().
01d,08oct92,rdc  ensured that both BASIC and FULL aren't set simultaneously.
01c,29sep92,ccc  renamed mmu1eLibInit() to mmuSun4LibInit().
01b,22sep92,rdc  cleanup, added support for sun1e.
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to initialize the memory management unit.  This file is
included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrMmuInitc
#define  __INCusrMmuInitc 

#if defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL)

/* externals */

IMPORT FUNCPTR sysMmuLibInit;		/* board-specific MMU library */


/*******************************************************************************
*
* usrMmuInit - initialize the memory management unit
*
* NOMANUAL
*/

void usrMmuInit (void)

    {
#if (CPU_FAMILY == SPARC)

    /* BSP selection of cache library */

    if ((sysMmuLibInit == NULL) || (((* sysMmuLibInit) (VM_PAGE_SIZE)) != OK))
	goto usrMmuPanic;
#endif /* CPU_FAMILY == SPARC */

#if (CPU==MC68020)				/* actually 68030 support */
    if (mmu30LibInit (VM_PAGE_SIZE) != OK)
	goto usrMmuPanic;
#endif /* CPU==MC68020 */

#if ((CPU==MC68040) || CPU==MC68060 || CPU==MC68LC040)
    if (mmu40LibInit (VM_PAGE_SIZE) != OK)
	goto usrMmuPanic;
#endif /* CPU==MC68040 || CPU==MC68060  || CPU==MC68LC040 */

#if (CPU_FAMILY==I80X86)
    if (mmuLibInit (VM_PAGE_SIZE) != OK)
        goto usrMmuPanic;
#endif /* CPU==I80X86 */

#if (CPU==AM29030)
    if (mmuAm29kLibInit (VM_PAGE_SIZE) != OK)
	goto usrMmuPanic;
#endif /* CPU==AM29030 */

#ifdef INCLUDE_MMU_BASIC
    if ((vmBaseLibInit (VM_PAGE_SIZE) != OK) ||
        (vmBaseGlobalMapInit (&sysPhysMemDesc[0],
			      sysPhysMemDescNumEnt, TRUE) == NULL))
	goto usrMmuPanic;
#endif /* INCLUDE_MMU_BASIC */

#ifdef INCLUDE_MMU_FULL			/* unbundled mmu product */
    if ((vmLibInit (VM_PAGE_SIZE) != OK) ||
        (vmGlobalMapInit (&sysPhysMemDesc[0],
			  sysPhysMemDescNumEnt, TRUE) == NULL))
	goto usrMmuPanic;

#ifdef INCLUDE_SHOW_ROUTINES 
    vmShowInit ();
#endif /* INCLUDE_SHOW_ROUTINES */

#endif /* INCLUDE_MMU_FULL */
    return;

usrMmuPanic:
    printExc ("usrRoot: MMU configuration failed, errno = %#x", errno, 0,0,0,0);
    reboot (BOOT_WARM_NO_AUTOBOOT);
    }

#endif	/* defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL) */
#endif	/* __INCusrMmuInitc */
