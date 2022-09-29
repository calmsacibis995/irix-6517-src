/* usrExtra.c - optionally included modules */

/* Copyright 1992-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01r,23jun95,tpr  added #ifdef INCLUDE_WDB
01q,20jun95,srh  make iostreams separately configurable.
01p,14jun95,srh  normalized names of C++ init files.
01o,13jun95,srh  added triggers for optional C++ products.
01n,20may95,jcf  added WDB.
01m,29mar95,kdl  added INCLUDE_GCC_FP; added reference to __fixunssfsi();
		 made MC68060 use same gcc support as MC68040.
01l,22aug94,dzb  added INCLUDE_NETWORK conditional (SPR #1147).
01k,06nov94,tmk  added MC68LC040 support
01j,16jun94,tpr  Added MC68060 cpu support.
01i,25oct93,hdn  added support for floppy and IDE device.
01h,25mar94,jmm  changed C++ to check for either full or min, not both
01g,08apr94,kdl  changed aio_show() reference to aioShow().
01f,28jan94,dvs  changed INCLUDE_POSIX_MEM_MAN to INCLUDE_POSIX_MEM
01e,02dec93,dvs  added POSIX A-I/O show configuration
01d,22nov93,dvs  added INCLUDE_POSIX_SCHED, INCLUDE_POSIX_MMAN, and 
		 INCLUDE_POSIX_FTRUNCATE
01c,23aug93,srh  Added INCLUDE_CPLUS_MIN to the conditions for making
		   sysCplusEnable TRUE.
01b,30oct92,smb  forced include of libfpg.a for 960
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to include extra modules necessary to satisfy the desired
configuration specified by configAll.h and config.h. This file is included
by usrConfig.c.

SEE ALSO: usrDepend.c

NOMANUAL
*/

#ifndef  __INCusrExtrac
#define  __INCusrExtrac 

/*
 * Extra modules to be included in VxWorks.  These modules are
 * not referred in the VxWorks code, so they will not be
 * automatically linked in unless they are defined here explicitly.
 *
 */


extern VOIDFUNCPTR __cmpsf2();
extern BOOL xdr_WDB_CTX();
extern BOOL xdr_WDB_CTX_CREATE_DESC();

VOIDFUNCPTR usrExtraModules [] =
    {
    usrInit,			/* provided in case of null array */

#if     (CPU_FAMILY==I960)
    (VOIDFUNCPTR) __cmpsf2,	/* forces include of GNU's libfpg.a */
#endif  /* CPU_FAMILY==I960 */

#if	CPU_FAMILY==MIPS
#ifdef	INCLUDE_RPC
    (VOIDFUNCPTR) xdr_float,
#endif
#endif	/* CPU_FAMILY==MIPS */

#if CPU_FAMILY==MC680X0 && !(CPU==MC68040) && !(CPU==MC68060)
#ifdef  INCLUDE_GCC_FP
    (VOIDFUNCPTR) gccMathInit,
    (VOIDFUNCPTR) gccUssInit,
    (VOIDFUNCPTR) __fixunssfsi,
#endif
#endif  /* CPU_FAMILY==MC680X0 && !(CPU==MC68040) && !(CPU==MC68060  */

#if     (CPU==MC68040) || (CPU==MC68060)
#ifdef  INCLUDE_GCC_FP
    (VOIDFUNCPTR) gccUss040Init,
    (VOIDFUNCPTR) __fixunssfsi,
#endif
#endif  /* (CPU==MC68040) || (CPU==MC68060) */

#ifdef  INCLUDE_NETWORK
#ifdef  INCLUDE_NET_SHOW
    netShowInit,
#endif
#endif  /* INCLUDE_NETWORK */

#ifdef	INCLUDE_TASK_VARS
    (VOIDFUNCPTR) taskVarInit,
#endif

#ifdef  INCLUDE_LOADER
    (VOIDFUNCPTR) loadModule,
#endif

#ifdef  INCLUDE_UNLOADER
    (VOIDFUNCPTR) unld,
#endif

#ifdef  INCLUDE_POSIX_TIMERS
    (VOIDFUNCPTR) nanosleep,
    (VOIDFUNCPTR) clock_gettime,
#endif

#ifdef  INCLUDE_POSIX_SCHED
    (VOIDFUNCPTR) sched_setparam,
#endif

#ifdef  INCLUDE_POSIX_MEM
    (VOIDFUNCPTR) mlockall,
#endif

#ifdef  INCLUDE_POSIX_FTRUNC
    (VOIDFUNCPTR) ftruncate,
#endif

#if defined(INCLUDE_POSIX_AIO) && defined(INCLUDE_SHOW_ROUTINES)
    (VOIDFUNCPTR) aioShow,
#endif /* defined(INCLUDE_POSIX_AIO) && defined(INCLUDE_SHOW_ROUTINES */

#ifdef  INCLUDE_NETWORK
#ifdef  INCLUDE_TFTP_CLIENT
    (VOIDFUNCPTR) tftpCopy,
#endif
#endif  /* INCLUDE_NETWORK */

#ifdef	INCLUDE_WDB	/* XXX be removed when the DSA will be redesigned */
    (VOIDFUNCPTR) strtol,
    (VOIDFUNCPTR) memcmp,
    (VOIDFUNCPTR) xdr_WDB_CTX ,
    (VOIDFUNCPTR) xdr_WDB_CTX_CREATE_DESC,
#endif	/* INCLUDE_WDB */
    };

#ifdef  INCLUDE_ANSI_ASSERT
#include "../../src/config/assertinit.c"
#endif

#ifdef  INCLUDE_ANSI_CTYPE
#include "../../src/config/ctypeinit.c"
#endif

#ifdef  INCLUDE_ANSI_LOCALE
#include "../../src/config/localeinit.c"
#endif

#ifdef  INCLUDE_ANSI_MATH
#include "../../src/config/mathinit.c"
#endif

#ifdef  INCLUDE_ANSI_STDIO
#include "../../src/config/stdioinit.c"
#endif

#ifdef  INCLUDE_ANSI_STDLIB
#include "../../src/config/stdlibinit.c"
#endif

#ifdef  INCLUDE_ANSI_STRING
#include "../../src/config/stringinit.c"
#endif

#ifdef  INCLUDE_ANSI_TIME
#include "../../src/config/timeinit.c"
#endif

#ifdef  INCLUDE_ANSI_5_0
#include "../../src/config/ansi_5_0.c"
#endif

#ifdef	INCLUDE_FD
#include "../../src/config/usrfd.c"
#endif

#ifdef	INCLUDE_IDE
#include "../../src/config/usride.c"
#endif

#ifdef	INCLUDE_NET_SYM_TBL
#include "../../src/config/usrloadsym.c"
#endif	/* INCLUDE_NET_SYM_TBL */

#if	defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL)
#include "../../src/config/usrmmuinit.c"
#endif	/* defined(INCLUDE_MMU_BASIC) || defined(INCLUDE_MMU_FULL) */

#ifdef	INCLUDE_NETWORK
#include "../../src/config/usrnetwork.c"
#endif

#ifdef	INCLUDE_STARTUP_SCRIPT
#include "../../src/config/usrscript.c"
#endif	/* INCLUDE_STARTUP_SCRIPT */

#ifdef	INCLUDE_SCSI
#include "../../src/config/usrscsi.c"
#endif

#ifdef	INCLUDE_SM_OBJ
#include "../../src/config/usrsmobj.c"
#endif

#ifdef	INCLUDE_WDB
#include "../../src/config/usrwdb.c"
#endif

#ifdef	INCLUDE_ADA
BOOL 		sysAdaEnable = TRUE;	/* TRUE = enable Ada support options */
#else
BOOL 		sysAdaEnable = FALSE;
#endif

#if	defined (INCLUDE_CPLUS) || defined (INCLUDE_CPLUS_MIN)
BOOL 		sysCplusEnable = TRUE;	/* TRUE = enable C++ support */
#else
BOOL 		sysCplusEnable = FALSE;
#endif

#if	defined (INCLUDE_CPLUS_IOSTREAMS)
#include "../../src/config/cplusios.c"
#endif

#if	defined (INCLUDE_CPLUS_VXW)
#include "../../src/config/cplusvxw.c"
#endif

#if	defined (INCLUDE_CPLUS_TOOLS)
#include "../../src/config/cplustools.c"
#endif

#if	defined (INCLUDE_CPLUS_HEAP)
#include "../../src/config/cplusheap.c"
#endif

#if	defined (INCLUDE_CPLUS_BOOCH)
#include "../../src/config/cplusbooch.c"
#endif

#ifdef  INCLUDE_DELETE_5_0

/*******************************************************************************
*
* delete - delete a file (obsolete routine)
*
* This routine is provided for binary compatibility with VxWorks 5.0
* and earlier versions which define this function.  This interface
* has been removed from VxWorks because "delete" is a reserved word
* in C++.
*
* Source code which uses delete() may be recompiled using a special
* macro in ioLib.h which will translate delete() calls into remove()
* calls.  To enable the macro, define __DELETE_FUNC as TRUE in ioLib.h.
*
* If code is recompiled in this way, it is not necessary to include
* the delete() routine defined here.
*
* RETURNS: OK, or ERROR.
*
* SEE ALSO: remove(), unlink(), ioLib.h
*
* NOMANUAL
*/

STATUS delete 
    (
    const char *filename		/* filename to delete */
    )
    {
    return (remove (filename));
    }

#endif  /* INCLUDE_DELETE_5_0 */

#if	(defined (INCLUDE_FD) || defined (INCLUDE_IDE))
/******************************************************************************
*
* devSplit - split the device name from a full path name
*
* This routine returns the device name from a valid UNIX-style path name
* by copying until two slashes ("/") are detected.  The device name is
* copied into <devName>.
*
* RETURNS: N/A
*
* NOMANUAL
*/

void devSplit
    (
    FAST char *fullFileName,    /* full file name being parsed */
    FAST char *devName          /* result device name */
    )
    {
    FAST int nChars = 0;

    if (fullFileName != NULL)
	{
        char *p0 = fullFileName;
        char *p1 = devName;

        while ((nChars < 2) && (*p0 != EOS))
	    {
            if (*p0 == '/')
                nChars++;

	    *p1++ = *p0++;
            }
        *p1 = EOS;
        }
    else
	{
        (void) strcpy (devName, "");
	}
    }

#endif	/* (defined (INCLUDE_FD) || defined (INCLUDE_IDE)) */
#endif	/* __INCusrExtrac */
