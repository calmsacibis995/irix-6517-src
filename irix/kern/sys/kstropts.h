/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_KSTROPTS_H
#define _SYS_KSTROPTS_H

#ident	"$Revision: 1.2 $"

#ifdef _KERNEL

/*
 * Kernel view of irix5 strioctl structure.
 */
struct irix5_strioctl {
	int 		ic_cmd;		/* command */
	int		ic_timout;	/* timeout value */
	int		ic_len;		/* length of data */
	app32_ptr_t	ic_dp;		/* pointer to data */
};

/*
 * Stream buffer structure for putmsg and getmsg system calls
 */
struct irix5_strbuf {
	int		maxlen;		/* no. of bytes in buffer */
	int		len;		/* no. of bytes returned */
	app32_ptr_t	buf;		/* pointer to data */
};

/* 
 * Stream I_PEEK ioctl format
 */
struct irix5_strpeek {
	struct irix5_strbuf	ctlbuf;
	struct irix5_strbuf	databuf;
	app32_long_t		flags;
};

/*
 * Stream I_FDINSERT ioctl format
 */
struct irix5_strfdinsert {
	struct irix5_strbuf	ctlbuf;
	struct irix5_strbuf	databuf;
	app32_long_t		flags;
	int			fildes;
	int			offset;
};

/*
 * Receive file descriptor structure. For _ABI64 kernels we need the user
 * view of the strrecvfd structure, since that view differs from
 * the kernel.
 */

struct user_e_strrecvfd {
	int fd;
	uid_t uid;
	gid_t gid;
	char fill[8];
};

/*
 * For I_LIST ioctl.
 */

struct irix5_str_list {
	int sl_nmods;
	app32_ptr_t sl_modlist;
};

#endif	/* _KERNEL */

#endif	/* _SYS_KSTROPTS_H */
