/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_USTAT_H__
#define __SYS_USTAT_H__

/*#ident	"@(#)kern-port:sys/ustat.h	10.1"*/
#ident	"$Revision: 3.6 $"
struct  ustat {
	daddr_t	f_tfree;	/* total free */
	ino_t	f_tinode;	/* total inodes free */
	char	f_fname[6];	/* filsys name */
	char	f_fpack[6];	/* filsys pack name */
};

#ifdef _KERNEL

struct irix5_o32_ustat {
	app32_long_t	f_tfree;
	app32_long_t	f_tinode;
	char		f_fname[6];
	char		f_fpack[6];
};

struct irix5_n32_ustat {
	app32_long_long_t	f_tfree;
	app32_long_long_t	f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};
#endif	/* _KERNEL */

#endif /* __SYS_USTAT_H__ */
