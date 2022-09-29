/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_buf_mach.h,v $
 * Revision 65.1  1997/10/24 14:29:49  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:43  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:50  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:25  bhim
 * No Message Supplied
 *
 * Revision 1.1.19.2  1994/06/09  14:14:58  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:34  annie]
 *
 * Revision 1.1.19.1  1994/02/04  20:23:50  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:22  devsrc]
 * 
 * Revision 1.1.17.1  1993/12/07  17:29:22  jaffe
 * 	New File from Transarc 1.0.3a
 * 	[1993/12/02  21:19:02  jaffe]
 * 
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_buf.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:10:27  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:49:58  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:51:41  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:01  jaffe
 * 	Remove duplicate HEADER and LOG entries
 * 	[1992/09/25  12:27:31  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:16:32  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    new file for machine specific additions for the osi_buf.h file.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:10:27  jaffe]
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1991, 1990 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_OSI_BUF_MACH_H
#define	TRANSARC_OSI_BUF_MACH_H

#include <sys/param.h>
#include <sys/buf.h>
#include <dcedfs/stds.h>

/*
 * Conversion between bytes, FS blocks and disk blocks
 *
 * XXX Name space pollution
 */
#define osi_btoab(d, b) ((b) >> (d)->logBlkSize)
#define osi_abtob(d, b) ((b) << (d)->logBlkSize)
#define dbtoab(d, b) ((b) >> ((d)->logBlkSize - DEV_BSHIFT))
#define abtodb(d, b) ((b) << ((d)->logBlkSize - DEV_BSHIFT))
#define dbroundup(b) roundup(b, DEV_BSIZE)

#ifdef	KERNEL
/* special macro that provides uniform access to bufsize */
#define b_Bufsize(b) ((b)->b_bufsize)
#endif

#if 0	/* This can't be used in SGIMIPS -- modify code in episode/anode/test_vm.c */
#define b_work b_pfcent                 /* use as driver work area */

/* special macro that allows b_work to be accessed as an lvalue */
#define B_Work(bp) (*(struct buf **)(&(bp)->b_work))
#endif

#ifndef KERNEL
#undef iodone				/* get the test_vm procedure */
#endif

#ifdef	KERNEL
#define b_Wanted(bp) (((bp)->b_flags & B_WANTED) \
		      ? ((bp)->b_flags &= ~B_WANTED, 1) : 0)
#define b_Want(bp) ((bp)->b_flags |= B_WANTED)
#else	/* KERNEL */
#define b_Wanted(bp) 1
#define b_Want(bp) 
#endif	/* KERNEL */

#define b_Call(bp) (((bp)->b_iodone) ? ((*(bp)->b_iodone)(bp), 1) : 0)

#ifndef B_PFSTORE
#define B_PFSTORE	0x40000000
#endif

/*
 * Get and set device number.
 */
#define osi_bdev(bp)		((bp)->b_edev)
#define osi_set_bdev(bp, d)	(osi_bdev(bp) = (d))

typedef void osi_iodone_t;

/* Return value of type osi_iodone_t */
#define osi_iodone_return(n)	return

#define osi_set_iodone(bp, func)  ((bp)->b_iodone = (func))


#ifdef KERNEL
#define osi_bio_wakeup(bp)	osi_Wakeup((opaque)(bp))
#define osi_bio_init(bp)	binit()
#define osi_bio_cleanup(bp)	brelse(bp) 
#define osi_biodone(bp) 	biodone(bp) 
#define osi_bio_wait(bp) 	biowait(bp) 

#endif /* KERNEL */

#endif /* TRANSARC_OSI_BUF_MACH_H */
