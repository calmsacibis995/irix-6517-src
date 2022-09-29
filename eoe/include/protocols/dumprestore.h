#ifndef __PROTOCOLS_DUMPRESTORE_H__
#define __PROTOCOLS_DUMPRESTORE_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.7 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)dumprestore.h	5.3 (Berkeley) 1/28/87
 *	@(#)inode.h 2.28 88/02/08 SMI; from UCB 4.24 83/07/01
 */

/*
 * The I node is the focus of all local file activity in UNIX.
 * There is a unique inode allocated for each active file,
 * each current directory, each mounted-on file, each mapping,
 * and the root.  An inode is `named' by its dev/inumber pair.
 * Data in icommon is read in from permanent inode on volume.
 */

#define	NDADDR	12		/* direct addresses in inode */
#define	NIADDR	3		/* indirect addresses in inode */

typedef int32_t baddr_t;	/* BSD-compatible daddr_t (32 bits) */

struct 	icommon {
	u_short	ic_mode;	/*  0: mode and type of file */
	short	ic_nlink;	/*  2: number of links to file */
	u_short	ic_uid;		/*  4: owner's user id */
	u_short	ic_gid;		/*  6: owner's group id */
	quad	ic_size;	/*  8: number of bytes in file */
	time_t	ic_atime;	/* 16: time last accessed */
	long	ic_atspare;
	time_t	ic_mtime;	/* 24: time last modified */
	long	ic_mtspare;
	time_t	ic_ctime;	/* 32: last time inode changed */
	long	ic_ctspare;
	baddr_t	ic_db[NDADDR];	/* 40: disk block addresses */
	baddr_t	ic_ib[NIADDR];	/* 88: indirect blocks */
	long	ic_flags;	/* 100: status, currently unused */
	long	ic_blocks;	/* 104: blocks actually held */
	long	ic_gen;		/* 108: generation number */
	long	ic_spare[4];	/* 112: reserved, currently unused */
};

struct sun_dinode {
	union {
		struct	icommon di_icom;
		char	di_size[128];
	} di_un;
};

/*
 * TP_BSIZE is the size of file blocks on the dump tapes.
 * Note that TP_BSIZE must be a multiple of DEV_BSIZE.
 *
 * NTREC is the number of TP_BSIZE blocks that are written
 * in each tape record. HIGHDENSITYTREC is the number of
 * TP_BSIZE blocks that are written in each tape record on
 * 6250 BPI or higher density tapes.
 *
 * TP_NINDIR is the number of indirect pointers in a TS_INODE
 * or TS_ADDR record. Note that it must be a power of two.
 */
#define TP_BSIZE	1024
#define NTREC   	10
#define HIGHDENSITYTREC	32
#define TP_NINDIR	(TP_BSIZE/2)
#define LBLSIZE		16
#define NAMELEN		64

/*
 * special record types
 */
#define TS_TAPE 	1	/* dump tape header */
#define TS_INODE	2	/* beginning of file record */
#define TS_BITS 	3	/* map of inodes on tape */
#define TS_ADDR 	4	/* continuation of file record */
#define TS_END  	5	/* end of volume marker */
#define TS_CLRI 	6	/* map of inodes deleted since last dump */

#define OFS_MAGIC   	(int)60011
#define NFS_MAGIC   	(int)60012
#define CHECKSUM	(int)84446

union u_spcl {
	char dummy[TP_BSIZE];
	struct	s_spcl {
		long	c_type;		    /* record type (see below) */
		time_t	c_date;		    /* date of previous dump */
		time_t	c_ddate;	    /* date of this dump */
		long	c_volume;	    /* dump volume number */
		baddr_t	c_tapea;	    /* logical block of this record */
		efs_ino_t c_inumber;	    /* number of inode */
		long	c_magic;	    /* magic number (see above) */
		long	c_checksum;	    /* record checksum */
		struct	sun_dinode c_dinode;   /* ownership and mode of inode */
		long	c_count;	    /* number of valid c_addr entries */
		char	c_addr[TP_NINDIR];  /* 1 => data; 0 => hole in inode */
		char	c_label[LBLSIZE];   /* dump label */
		long	c_level;	    /* level of this dump */
		char	c_filesys[NAMELEN]; /* name of dumpped file system */
		char	c_dev[NAMELEN];	    /* name of dumpped device */
		char	c_host[NAMELEN];    /* name of dumpped host */
		long	c_flags;	    /* additional information */
	} s_spcl;
};
#define spcl u_spcl.s_spcl
/*
 * flag values
 */
#define DR_NEWHEADER	1	/* new format tape header */
#define DR_EFSDIRS      2       /* directories are in EFS format */

#define	DUMPOUTFMT	"%-16s %c %s"		/* for printf */
						/* name, incno, ctime(date) */
#define	DUMPINFMT	"%[^ ] %c %[^\n]\n"	/* inverse for scanf */
#ifdef __cplusplus
}
#endif
#endif /* !__PROTOCOLS_DUMPRESTORE_H__ */
