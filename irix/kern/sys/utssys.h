/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SVC_UTSSYS_H	/* wrapper symbol for kernel use */
#define _SVC_UTSSYS_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

/*#ident	"@(#)uts-3b2:svc/utssys.h	1.2"*/
#ident	"$Revision: 1.4 $"

/*
 * Definitions related to the utssys() system call. 
 */
#include <sys/types.h>
#include <sys/fstyp.h>

/*
 * "commands" of utssys
 */
#define UTS_UNAME	0x0
#define UTS_USTAT	0x2	/* 1 was umask */
#define UTS_FUSERS	0x3

/*
 * Flags to UTS_FUSERS
 */
#define F_FILE_ONLY	0x1
#define F_CONTAINED	0x2
#define F_ANONYMOUS	0x8000	/* anonymous file, e.g. socket */

typedef struct f_anonid {
	char		fa_fsid[FSTYPSZ];
	struct fid	*fa_fid;
} f_anonid_t;

/*
 * structure yielded by UTS_FUSERS
 */
typedef struct f_user {
	pid_t	fu_pid;
	int	fu_flags;	/* see below */
	uid_t	fu_uid;	
} f_user_t;

/*
 * fu_flags values
 */
#define F_CDIR		0x1
#define F_RDIR		0x2
#define F_TEXT		0x4
#define F_MAP		0x8
#define F_OPEN		0x10
#define F_TRACE		0x20
#define F_TTY		0x40

#ifndef _KERNEL
extern int utssys();	/* can't prototype except with ..., which is wrong */
#endif

#ifdef _KERNEL
typedef struct irix5_f_anonid {
	char		fa_fsid[FSTYPSZ];
	app32_ptr_t	fa_fid;
} irix5_f_anonid_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SVC_UTSSYS_H */ 
