/* Copyright (C) 1989-1992 Silicon Graphics, Inc. All rights reserved.  */

#ifndef _SYS_KSTAT_H
#define _SYS_KSTAT_H
#ident	"$Revision: 1.10 $"

/*
 * This header contains internal kernel stat handling data structures
 * Do not use __mips macro here
 * Use ONLY definitions that are constant whether compiled mips2 or 3
 */

#include <sys/time.h>
#include <sys/timers.h>
#include <sys/ktypes.h>

#undef st_atime
#undef st_mtime	
#undef st_ctime
/*
 * stat structure, used by stat(2) and fstat(2)
 * SVID II
 */

/*
 * IRIX5/SVID III/ MIPS ABI
 */
struct	irix5_stat {
	app32_ulong_t	st_dev;
	app32_long_t	st_pad1[_ST_PAD1SZ];
	app32_ulong_t	st_ino;
	app32_ulong_t	st_mode;
	app32_ulong_t	st_nlink;
	app32_long_t 	st_uid;
	app32_long_t 	st_gid;
	app32_ulong_t	st_rdev;
	app32_long_t	st_pad2[_ST_PAD2SZ];	/* dev and off_t expansion */
	app32_long_t	st_size;
	app32_long_t	st_pad3;
	irix5_timespec_t st_atime;
	irix5_timespec_t st_mtime;
	irix5_timespec_t st_ctime;
	app32_long_t	st_blksize;
	app32_long_t	st_blocks;
	char	st_fstype[_ST_FSTYPSZ];
	app32_long_t	st_projid;
	app32_long_t	st_pad4[_ST_PAD4SZ];	/* expansion area */
};

/*
 * IRIX5_N32
 */
struct	irix5_n32_stat {
	app32_ulong_t		st_dev;
	app32_long_t		st_pad1[_ST_PAD1SZ];
	app32_ulong_long_t	st_ino;
	app32_ulong_t		st_mode;
	app32_ulong_t		st_nlink;
	app32_long_t 		st_uid;
	app32_long_t 		st_gid;
	app32_ulong_t		st_rdev;
	app32_long_t		st_pad2[_ST_PAD2SZ];	/* dev and off_t expansion */
	irix5_n32_off_t		st_size;
	app32_long_t		st_pad3;
	irix5_timespec_t 	st_atime;
	irix5_timespec_t 	st_mtime;
	irix5_timespec_t 	st_ctime;
	app32_long_t		st_blksize;
	app32_long_long_t	st_blocks;
	char			st_fstype[_ST_FSTYPSZ];
	app32_long_t		st_projid;
	app32_long_t		st_pad4[_ST_PAD4SZ];	/* expansion area */
};

/*
 * IRIX5 stat64
 */
struct	irix5_stat64 {
	app32_ulong_t	st_dev;
	app32_long_t	st_pad1[_ST_PAD1SZ];
	app32_ulong_long_t	st_ino;
	app32_ulong_t	st_mode;
	app32_ulong_t	st_nlink;
	app32_long_t 	st_uid;
	app32_long_t 	st_gid;
	app32_ulong_t	st_rdev;
	app32_long_t	st_pad2[_ST_PAD2SZ];	/* dev and off_t expansion */
	app32_long_long_t	st_size;
	app32_long_t	st_pad3;
	irix5_timespec_t st_atime;
	irix5_timespec_t st_mtime;
	irix5_timespec_t st_ctime;
	app32_long_t	st_blksize;
	app32_long_long_t	st_blocks;
	char	st_fstype[_ST_FSTYPSZ];
	app32_long_t	st_projid;
	app32_long_t	st_pad4[_ST_PAD4SZ];	/* expansion area */
};

#endif
