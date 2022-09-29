/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_helper_mach.h,v $
 * Revision 65.1  1997/10/24 14:29:55  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:34  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:45:31  dce
 * *** empty log message ***
 *
 * Revision 1.1  1995/08/04  23:51:55  dcebuild
 * Initial revision
 *
 * Revision 1.1.67.2  1994/06/10  20:54:09  devsrc
 * 	cr10871 - fix copyright
 * 	[1994/06/10  14:59:54  devsrc]
 *
 * Revision 1.1.67.1  1994/01/21  22:31:24  cbrooks
 * 	RPC Code Cleanup
 * 	[1994/01/21  20:33:01  cbrooks]
 * 
 * Revision 1.1.4.2  1993/06/10  19:23:28  sommerfeld
 * 	Initial HPUX RP version.
 * 	[1993/06/03  22:04:06  kissel]
 * 
 * 	Initial revision.
 * 	[1993/01/15  21:17:51  toml]
 * 
 * $EndLog$
 */

/*
 * Platform dependent header file for krpc_helper module
 */

#ifndef _KRPC_HELPER_MACH_H
#define _KRPC_HELPER_MACH_H

#ifdef _KERNEL

/* define our alignment requirements for rounding up */
#define ALIGNMENT 4

#define ROUNDUP(x,y)  roundup(x,y)

#define OSI_TRANSACTION_ID (++tid) 

extern struct sel_queue krpch_selq;

#define KRPC_HELPER_MACHINIT()

#define SELECT_NOTIFY() krpc_wakeup()

extern time_t time;
#define TIME time

#else  /* _KERNEL */

#include <fcntl.h>
#define KRPCH_DEV "/dev/krpch"
#define KRPC_OPEN_HELPER() open (KRPCH_DEV, O_RDWR)

#endif /* _KERNEL */

#endif /* _KRPC_HELPER_MACH_H*/
