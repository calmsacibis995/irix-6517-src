/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: sysconf.h,v $
 * Revision 65.2  1998/04/01 14:16:49  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added definition for uprintf.  The previous definition came from
 * 	dfs, but including dfs header files causes problems in rpc.
 *
 * Revision 65.1  1997/10/24  14:29:57  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:34  jdoak
 * Initial revision
 *
 * Revision 64.4  1997/05/06  21:48:59  bdr
 * Remove define of NEED_MEMSET for SN0 machine which have their own
 * memset routine in the irix kernel code.
 *
 * Revision 64.3  1997/04/09  17:37:23  lord
 * Add INLINE_PACKET define
 *
 * Revision 64.1  1997/02/14  19:45:32  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/15  17:31:15  vrk
 * Old changes
 *
 * Revision 1.1  1995/09/14  21:52:31  dcebuild
 * Initial revision
 *
 * Revision 1.1.60.3  1994/06/10  20:54:14  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  15:00:00  devsrc]
 *
 * Revision 1.1.60.2  1994/02/02  21:48:56  cbrooks
 * 	OT9855 code cleanup breaks KRPC
 * 	[1994/02/02  21:00:04  cbrooks]
 * 
 * Revision 1.1.60.1  1994/01/21  22:31:30  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:33:05  cbrooks]
 * 
 * Revision 1.1.4.2  1993/06/10  19:25:27  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:24:57  kissel]
 * 
 * 	Increase kernel packet pool size (rpc_c_dg_pkt_max) from 64 to 256.
 * 	[1993/03/29  22:44:21  toml]
 * 
 * 	06/16/92   tmm  Created from COSL drop, with some modifications.
 * 	[1992/06/18  18:31:00  tmm]
 * 
 * $EndLog$
 */
#ifndef _SYSCONF_H 
#define _SYSCONF_H 
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**  NAME:
**
**      osc_m68k.h (sysconf.h)
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  This file contains all (well most) definitions specific to the 
**  OSF1 OSC 680x0 platform.  
**
**  The real scoop is that this file is for a *snap3* OSF1 m68k kernel.
**  Subsequent system specific config files should have names like:
**          sysconf_k<OS_NAME>_<HW_PLATFORM>.h
**  E.G
**      sysconf_kosf1_m68k.h, sysconf_kosf1_hppa.h, sysconf_kosf1_mips.h, 
**      sysconf_kaix_rs6000.h, sysconf_ksunos_sparc.h, ...
**
**  For a particular build, a link to the appropriate file is made
**  from "sysconf.h".
**
*/


/*
 * !!!
 * Here are some things that would normally go on the compiler command line.
 * Unfortunately, gcc's cpp has a limit of 20 -D's (which we were exceeding)
 * so we provide some of these here.
 */

#ifdef DEBUG
#define RPC_MUTEX_DEBUG
#define RPC_MUTEX_STATS
/* #define RPC_DG_LOSSY must be def'ed on cc cmd if desired - (for dglossy.c) */
#define MAX_DEBUG

#define DEBUG   1
#endif

/*
 * Do not include the autostart code in rpc_server_use_protseq_ep().
 */
#define NO_AUTOSTART_USE_PROTSEQ

/*
 * Do not allow hostnames in ipnaf.c.  The kernel has no way to translate
 * hostnames to ip addresses.
 */

#define DO_NOT_ALLOW_HOSTNAMES

/* 
 * There is no naming service available in the kernel.
 */

#define NO_NS

/*
 * Tower support is not present in the kernel.
 * protocol towers ?? are not supported in the kernel
 */

#define RPC_NO_TOWER_SUPPORT

/*
 * Specify supported NAF, protseqs, and auth modules
 */
#define AUTH_NONE

/*
 * For time adjustments.
 */

#define UNIX

/*
 * For now, make everything visible to kdb
 */

#ifdef DEBUG
#  define INTERNAL
#endif

/*
 * Inline packet support optimization
 */

#define INLINE_PACKETS

/*
 * Since OS doesn't already have this type (needed by pthread.h)...
 */
#undef PTHREAD_NO_TIMESPEC
#undef USE_KTHREAD

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>         /* should probably be in comsoc_sys.h */
#ifdef	sa_len
#undef	sa_len
#endif
#include <sys/uio.h>            /* should probably be in comsoc_sys.h */
#include <sys/errno.h>
#include <sys/timers.h>
#ifdef PRE64
#include <sys/user.h>
#else
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#endif

/*
extern int errno;
*/

#include <sys/debug.h> 

/* 
 * kernel memory allocation routines for rpcmem_krpc.h (kalloc()/kfree())
 */

#include <sys/kmem.h>
#include <sys/systm.h>

#include <string.h>
#include <dce/ker/pthread_exc.h>

#ifndef UNIX
#  define UNIX
#endif

/*
 * Use some alternate common include files (see common/commonp.h)
 */

#define ALT_COMMON_INCLUDE  <alt_common_krpc.h>

/*
 * We provide the alternate function specific sscanf / sprintf routines.
 */

#define NO_SSCANF
#define NO_SPRINTF

/*
 * Can't use any of the available rpc__printf implementations.
 */

#define NO_RPC_PRINTF
#define rpc__printf printf

/*
 * Can't share definition of uprintf with dfs.
 */

#define uprintf printf

/*
 * Must not call the kernel gettimeofday().
 */

#define gettimeofday    nck_gettimeofday

/*
 * Modify the default number of sockets allowed (64) to something reasonable.
 */

#define RPC_C_SERVER_MAX_SOCKETS 16

/*
 * Modify some of the default dg pkt buffer allocation constants to something
 * reasonable for the kernel.  These may have to be adjusted.
 */

#define RPC_C_DG_PKT_MAX        256

/*
 * No environment to get debug switches from in the kernel :-)
 */

#define NO_GETENV

/*
 * Misc stuff for subr_krpc.c
 */
#define NEED_ISDIGIT
/*
#define NEED_STRCAT
*/
#define NEED_STRNCAT
#define NEED_RINDEX
#define NEED_STRRCHR
#define NEED_MEMCPY
#define NEED_MEMCMP
#ifndef SN0
#define NEED_MEMSET
#endif /* SN0 */
#define NEED_MEMMOVE
#define NEED_ISXDIGIT
#define NEED_TOUPPER

/*
 * Misc. defines for portablility...
 */
#ifdef HPUX

#define	splhigh		spl6
#define uio_rw          uio_fpflags
#define	uio_segflg	uio_seg
#define	UIO_SYSSPACE	UIOSEG_KERNEL
#define POLLNORM        POLLIN
#define pkt_alloc       NCS_pkt_alloc
#define	NO_TSLEEP	1

#ifdef hp9000s300
#define	MICROTIME(tvp)	get_precise_time(tvp)
#elif defined( __hp9000s800)
#define	MICROTIME(tvp)	kernel_gettimeofday(tvp)
#else
#error "neither hp9000s300 nor __hp9000s800 defined!" 
#endif /* hp9000s300 */

#define assert(i)	DO_ASSERT((i),"")

/*
 * Redfine memory allocator functions to use hpux general memory
 * allocator.
 */
#define KALLOC(size)	kmalloc(size, (M_DYNAMIC), (M_WAITOK|M_ALIGN))
#define KFREE(a,s)   	kfree(a, (M_DYNAMIC))
#endif /* HPUX */

#ifdef SGIMIPS

#define assert(i)	ASSERT(i)

#define MICROTIME(tvp) 	microtime(tvp)  /* in kern/ml/timer.c */

#define KALLOC(size)	kern_malloc((size_t) size)
#define KFREE(a,s)	kern_free(a)
 
#endif    /* SGIMIPS */

#endif /* _SYSCONF_H */

