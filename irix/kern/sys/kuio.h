/* Copyright (C) 1989-1992 Silicon Graphics, Inc. All rights reserved.  */

#ifndef _SYS_KUIO_H
#define _SYS_KUIO_H

/*
 * Interface structures used for the readv and writev system calls.
 * Do not use __mips macro here
 * Use ONLY definitions that are constant whether compiled mips2 or 3
 */

#ident	"$Revision: 1.3 $"

#ifdef _KERNEL

#include <sys/xlate.h>

struct irix5_iovec {
	app32_ptr_t	iov_base;
	app32_int_t	iov_len;
};

struct irix5_64_iovec {
	app64_ptr_t	iov_base;
	app64_long_t	iov_len;
};

extern int irix5_to_iovec(enum xlate_mode, void *, int, xlate_info_t *);

#endif	/* _KERNEL */

#endif	/* _SYS_KUIO_H */
