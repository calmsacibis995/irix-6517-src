#ifndef _SYS_UADMIN_H
#define _SYS_UADMIN_H
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/uadmin.h	10.2"*/
#ident	"$Revision: 3.12 $"

#ifdef __cplusplus
extern "C" {
#endif

#define	A_REBOOT	1
#define	A_SHUTDOWN	2
#define	A_REMOUNT	4
#define A_KILLALL	8
#define	A_SETFLAG	256	/* used only with AD_POWEROFF to set flag for
	* powering system off after a normal shutdown */

#define	AD_HALT		0
#define	AD_BOOT		1
#define	AD_IBOOT	2
#define AD_EXEC		3
#define	AD_POWEROFF	9	/* power system off immediately with REBOOT or
	SHUTDOWN, or set flag for poweroff after a later HALT cmd. */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#ifndef _KERNEL
extern int uadmin(int, int, int);
#endif /* !_KERNEL */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UADMIN_H */
