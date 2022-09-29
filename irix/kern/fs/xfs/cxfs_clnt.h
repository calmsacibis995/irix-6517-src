#ifndef __SYS_XFS_CLNT_H__
#define __SYS_XFS_CLNT_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993-1997, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

/*
 * XFS arguments to the mount system call.
 */
struct xfs_args {
	int	version;	/* version of this */
				/* 1, see xfs_args_ver_1 */
				/* 2, see xfs_args_ver_2 */
				/* 3, see xfs_args_ver_3 */
	int	flags;		/* flags, see XFSMNT_... below */
	int	logbufs;	/* Number of log buffers, -1 to default */
	int	logbufsize;	/* Size of log buffers, -1 to default */
	char	*fsname;	/* filesystem name (mount point) */
	/*
	 * Next two are for stripe aligment.
	 * Set 0 for no alignment handling (see XFSMNT_NOALIGN flag)
	 */
	int	sunit;		/* stripe unit (bbs) */
	int	swidth;		/* stripe width (bbs), multiple of sunit */
        /*
	 * The following stuff is for CXFS support
	 */
        char    **servers;      /* Table of hosts which may be servers */
        int     *servlen;       /* Table of hostname lengths. */
        int     scount;         /* Count of hosts which may be servers. */
        int     stimeout;       /* Server timeout in milliseconds */
        int     ctimeout;       /* Client timeout in milliseconds */
        char    *uuid;          /* Dummy uuid for testing */
        
};

#ifdef _KERNEL
struct xfs_args_ver_1 {
	__int32_t	version;
	__int32_t	flags;
	__int32_t	logbufs;
	__int32_t	logbufsize;
	app32_ptr_t	fsname;
};

struct xfs_args_ver_2 {
	__int32_t	version;
	__int32_t	flags;
	__int32_t	logbufs;
	__int32_t	logbufsize;
	app32_ptr_t	fsname;
	__int32_t	sunit;
	__int32_t	swidth;
};

struct xfs_args_ver_3 {
	__int32_t	version;
	__int32_t	flags;
	__int32_t	logbufs;
	__int32_t	logbufsize;
	app32_ptr_t	fsname;
	__int32_t	sunit;
	__int32_t	swidth;
	uint8_t		iosizelog;
	uint8_t		reserved_3_0;
	__int16_t	reserved_3_1;
	__int32_t	reserved_3_2;
	__int32_t	reserved_3_3;
};
struct xfs_args_ver_4 {
	__int32_t	version;
	__int32_t	flags;
	__int32_t	logbufs;
	__int32_t	logbufsize;
	app32_ptr_t	fsname;
	__int32_t	sunit;
	__int32_t	swidth;
	uint8_t		iosizelog;
	uint8_t		reserved_3_0;
	__int16_t	reserved_3_1;
	__int32_t	reserved_3_2;
	__int32_t	reserved_3_3;
        app32_ptr_t     servers;
        app32_ptr_t     servlen;
  	__int32_t	scount;
        __int32_t       stimeout;
        __int32_t       ctimeout;
        app32_ptr_t     uuid;
};

#define XFSARGS_FOR_CXFSARR(ap)		\
	((ap)->servers || (ap)->scount || \
	 (ap)->stimeout >= 0 || (ap)->ctimeout >= 0 || \
	 (ap)->flags & (XFSMNT_CLNTONLY | XFSMNT_UNSHARED))

#endif /* _KERNEL */

/*
 * XFS mount option flags
 */
#define	XFSMNT_CHKLOG		0x0001	/* check log */
#define	XFSMNT_WSYNC		0x0002	/* safe mode nfs mount compatible */
#define	XFSMNT_INO64		0x0004	/* move inode numbers up past 2^32 */
#define XFSMNT_UQUOTA		0x0008	/* user quota accounting */
#define XFSMNT_PQUOTA		0x0010	/* project quota accounting */
#define XFSMNT_UQUOTAENF	0x0020	/* user quota limit enforcement */
#define XFSMNT_PQUOTAENF	0x0040	/* project quota limit enforcement */
#define XFSMNT_QUOTAMAYBE	0x0080  /* don't turn off if SB has quotas on */
#define XFSMNT_NOATIME		0x0100  /* don't modify access times on reads */
#define XFSMNT_NOALIGN		0x0200	/* don't allocate at stripe boundaries*/
#define XFSMNT_RETERR		0x0400	/* return error to user */
#define XFSMNT_NORECOVERY	0x0800	/* no recovery, implies ro mount */
#define XFSMNT_SHARED		0x1000	/* shared XFS mount */
#define XFSMNT_CLNTONLY         0x2000  /* cxfs mount as client only */
#define XFSMNT_UNSHARED         0x4000  /* cxfs filesystem mounted unshared */
#define XFSMNT_TESTUUID         0x8000  /* test uuid specified. */

#endif /* !__SYS_XFS_CLNT_H__ */
