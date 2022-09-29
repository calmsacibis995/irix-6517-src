/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1989, 1990, 1996 Transarc Corporation - All rights reserved */

/*
 * HISTORY
 * $Log: ioctl.h,v $
 * Revision 65.2  1997/12/16 17:05:36  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:20:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.20.1  1996/10/02  17:53:08  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:28  damon]
 *
 * $EndLog$
*/

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/ioctl.h,v 65.2 1997/12/16 17:05:36 lmc Exp $ */

#ifndef	_IOCTLH_
#define	_IOCTLH_

#define	AFSIO_BUFSIZE	8192	/* It was used to be 2K */

#ifndef	_IOW
#include <sys/ioctl.h>
#endif

struct afs_ioctl {
	char  *in, *out;	/* Data to be transferred in, or out */
	short in_size;		/* Size of in buffer (<= AFSIO_BUFSIZE) */
	short out_size;		/* Max size of out buffer (<= AFSIO_BUFSIZE) */
};

#if defined(AFS_HPUX_ENV) || defined(hpux) || defined(__hpux) || defined(AFS_AIX31_ENV) || defined(_AIX) || defined(AFS_OSF_ENV) || defined(AFS_SUNOS5_ENV) || defined (AFS_IRIX_ENV)
#define _AFSIOCTL(id)  ((unsigned int ) _IOW('V', id, struct afs_ioctl))
#else /* defined(AFS_HPUX_ENV) || defined(hpux) || defined(__hpux) || defined(AFS_AIX31_ENV) || defined(_AIX) */
#define _AFSIOCTL(id)  ((unsigned int ) _IOW(V, id, struct afs_ioctl))
#endif /* defined(AFS_HPUX_ENV) || defined(hpux) || defined(__hpux) || defined(AFS_AIX31_ENV) || defined(_AIX) */

#define _VALIDAFSIOCTL(com) (com >= _AFSIOCTL(0) && com <= _AFSIOCTL(255))

/*
 * Below the current supported list of PIOCTLs to the AFS cache manager
 */

/* 
 * PIOCTLs to the cache manager.  Apply these to path names with pioctl. 
*/
#define	VIOCGETVOLSTAT		_AFSIOCTL(1)	/* Get volume status */
#define	VIOCSETVOLSTAT		_AFSIOCTL(2)	/* Set volume status */
#define	VIOCFLUSH		_AFSIOCTL(3)	/* Invalidate cache entry */
#define	VIOCCKSERV		_AFSIOCTL(4)	/* Check that servers are up */
#define	VIOCCKBACK		_AFSIOCTL(5)	/* Check backup volume mappings */
#define	VIOCWHEREIS		_AFSIOCTL(6)	/* Find out where a volume is located */
#define	VIOCPREFETCH		_AFSIOCTL(7)	/* Prefetch a file */
#define	VIOCACCESS		_AFSIOCTL(8)	/* Access using PRS_FS bits */
#define	VIOCGETFID		_AFSIOCTL(9)	/* Get file ID quickly */
#define VIOCGETCACHEPARMS	_AFSIOCTL(10)	/* Get cache stats */
#define	VIOCSETCACHESIZE	_AFSIOCTL(11)	/* Set cache size in 1k units */
#define VIOCGETCELL		_AFSIOCTL(12)	/* Get cell info */
#define	VIOC_AFS_DELETE_MT_PT	_AFSIOCTL(13)	/* Delete mount point */
#define VIOC_AFS_STAT_MT_PT	_AFSIOCTL(14)	/* Stat mount point */
#define	VIOC_FILE_CELL_NAME	_AFSIOCTL(15)	/* Get cell in which file lives */
#define	VIOC_FLUSHVOLUME	_AFSIOCTL(16)	/* flush whole volume's data */
#define	VIOC_AFS_SYSNAME	_AFSIOCTL(17)	/* Change @sys value */
#define VIOCRESETSTORES		_AFSIOCTL(18)	/* reset store retries */
#define VIOCLISTSTORES		_AFSIOCTL(19)	/* list store retries */
#define	VIOC_CLOCK_MGMT		_AFSIOCTL(20)	/* read or set clock mgmt */
#define	VIOC_SETSPREFS		_AFSIOCTL(21)	/* set server prefs */
#define	VIOC_GETSPREFS		_AFSIOCTL(22)	/* get server prefs */
#define	VIOC_GETPROTBNDS	_AFSIOCTL(23)	/* get authn bounds */
#define	VIOC_SETPROTBNDS	_AFSIOCTL(24)	/* set authn bounds */
#define	VIOC_AFS_CREATE_MT_PT	_AFSIOCTL(25)	/* Create mount point */

/* some useful constants for the above calls */
#define VIOC_DEF_SGIDOK		1	/* must match VL_SGIDOK in cm/cm_volume.h */
#define VIOC_DEF_DEVOK		2	/* must match VL_DEVOK in cm/cm_volume.h */

#endif	/* _IOCTLH_ */
