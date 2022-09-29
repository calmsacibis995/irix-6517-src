
/* Copyright (C) 1989-1992 Silicon Graphics, Inc. All rights reserved.  */

#ifndef _SYS_KFCNTL_H
#define _SYS_KFCNTL_H
#ident	"$Revision: 1.4 $"

#include <sys/ktypes.h>

/*
 * Interface structures used for the fcntl system call.
 * Do not use __mips macro here
 * Use ONLY definitions that are constant whether compiled mips2 or 3
 */

	/* Irix5 abi definition */
struct irix5_flock {
	short		l_type;
	short		l_whence;
	app32_long_t	l_start;
	app32_long_t	l_len;		/* len == 0 means until end of file */
        app32_long_t	l_sysid;
        app32_long_t	l_pid;
	app32_long_t	pad[4];		/* reserve area */
};

struct irix5_n32_flock {
	short	l_type;
	short	l_whence;
	irix5_n32_off_t	l_start;
	irix5_n32_off_t	l_len;
        app32_long_t	l_sysid;
        app32_long_t	l_pid;
	app32_long_t	pad[4];
};

#endif	/* _SYS_KFCNTL_H */
