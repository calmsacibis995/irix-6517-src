/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _FS_IOCCOM_H	/* wrapper symbol for kernel use */
#define _FS_IOCCOM_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:fs/ioccom.h	1.3"*/
#ident	"$Revision: 1.2 $"

/*
 *  		PROPRIETARY NOTICE (Combined)
 *  
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *  
 *  
 *  
 *  		Copyright Notice 
 *  
 *  Notice of copyright on this source code product does not indicate 
 *  publication.
 *  
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *  	          All rights reserved.
 */

/*
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 255 bytes.
 */
#define	IOCPARM_MASK	0xff		/* parameters must be < 256 bytes */
#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
/* the 0x20000000 is so we can distinguish new ioctl's from old */
#define	_IOC(f,n,x,y)	((int)((f)|(((n)&IOCPARM_MASK)<<16)|((x)<<8)|(y)))
#define	_IO(x,y)	_IOC(IOC_VOID, 0, x, y)
#define	_IOR(x,y,t)	_IOC(IOC_OUT, sizeof(t), x, y)
#define	_IORN(x,y,n)	_IOC(IOC_OUT, n, x, y)
#define	_IOW(x,y,t)	_IOC(IOC_IN, sizeof(t), x, y)
#define	_IOWN(x,y,n)	_IOC(IOC_IN, n, x, y)
/* this should be _IORW, but stdio got there first */
#define	_IOWR(x,y,t)	_IOC(IOC_INOUT, sizeof(t), x, y)

#endif	/* _FS_IOCCOM_H */
