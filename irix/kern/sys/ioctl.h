/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)kern-port:sys/ioctl.h	10.2"*/
#ident	"$Revision: 3.15 $"
/*
 *	Ioctl commands
 */
#define	IOCTYPE	0xff00

#define	LIOC	('l'<<8)
#define	LIOCGETP	(LIOC|1)
#define	LIOCSETP	(LIOC|2)
#define	LIOCGETS	(LIOC|5)
#define	LIOCSETS	(LIOC|6)

#define	DIOC	('d'<<8)
#define	DIOCGETC	(DIOC|1)
#define	DIOCGETB	(DIOC|2)
#define	DIOCSETE	(DIOC|3)

/* The following are used for setting GBR registers on xbow & bridge */
#define GIOC	('g'<<8)
#define GIOCSETBW	(GIOC|1)
#define GIOCRELEASEBW	(GIOC|2)

/*
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 128 bytes.
 */
#include <sys/ioccom.h>

#define	FIONREAD _IOR('f', 127, int)	/* get # bytes to read */
#define	FIONBIO	 _IOW('f', 126, int)	/* set/clear non-blocking i/o */
#define	FIOASYNC _IOW('f', 125, int)	/* set/clear async i/o */
#define	FIOSETOWN _IOW('f', 124, int)	/* set owner */
#define	FIOGETOWN _IOR('f', 123, int)	/* get owner */
/*
 * An attempt at compatibility with 4.3BSD's <sys/ioctl.h>.
 */
#ifndef _KERNEL
#include <net/soioctl.h>
#include <sys/termio.h>

extern int ioctl(int, int, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_IOCTL_H */
