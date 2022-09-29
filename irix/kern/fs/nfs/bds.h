/*
 * bds.h
 * $Revision: 1.20 $
 * Defines the ports used by BDS.
 * Defines the BDS protocol and associated structures.
 * Defines the user level library API
 *
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * See the file COPYING for the licensing terms.
 */
#ifndef	_BDS_H
#define	_BDS_H

#ifndef	_KERNEL
#include        <stdio.h>
#include	<sys/types.h>
#include        <sys/fcntl.h>
#endif	/* _KERNEL */

/* don't change this */
#define	BDS_PORT	2050

/*
 * This may need to be ifdefed - it works on IRIX and Linux.
 */
typedef	long long		int64;
typedef	unsigned long long	uint64;

#if	defined(__sgi)
#define	flip64(a)	((uint64)a)
#endif

#if	defined(linux)
#define	flip64(a)	intel64((uint64)a)
extern	uint64	intel64(uint64);
#endif

#ifndef	flip64
#error "You need to fix the net XDR stuff"
#endif


#ifdef	_KERNEL
#define F_BDS_FSOPERATIONS	4096
#endif

/*
 * bdsattr and the flags are used with fcntl's F_GETBDSATTR and F_SETBDSATTR
 */

#define BDSATTR_BUFSIZE_DONTCARE	(0xffffffffffffffffLL)
#define BDSATTR_PRIORITY_DONTCARE	(0)

#define BDSATTRF_BUFFERS_SINGLE		0x01
#define BDSATTRF_BUFFERS_RENEW		0x02
#define BDSATTRF_BUFFERS_RENEWRR	0x04
#define BDSATTRF_WRITEBEHIND		0x08
#define BDSATTRF_BUFFERS_PREALLOC	0x10
#define BDSATTRF_BUFFERS_KEEP		0x20

typedef struct bdsattr {
	uint64	bdsattr_flags;
	uint64	bdsattr_bufsize;
	u_char	bdsattr_priority;
} bdsattr_t;


/* A struct flock with all fields of fixed size */

typedef struct bds_flock64 {
	short	l_type;
	short	l_whence;
	u_char	l_pad1[4];	/* explicitly set size of gap */
	uint64	l_start;
	uint64	l_len;		/* len == 0 means until end of file */
        uint64	l_sysid;
        uint64	l_pid;
	uint64	l_pad[4];	/* reserve area */
} bds_flock64_t;	

/*
 * All commands are 32 bytes in size.  This size is fixed.
 * BDS v2.0 defines the following commmands: O, o, f, F, R, W, S, A, N.
 * The structures below are cast in stone when mapped to those commands.
 * If you need a new command, pick a different value, define a structure
 * that goes with it, and have at it.
 *
 * The 2.0 version below gives you everything you need
 * to do open/close/read/write/seek, and extra stuff will just get nak'd
 * if you talk to an old server.
 *
 * Most commands are pretty self explanatory.  
 * Open is a little weird because it does 5 different things:
 *	- with a null pathlen, it is the nullproc() interface for probing.
 *	- with a 'O' command, it is a userlevel open, with a pathname.
 *	- with a 'o' command, it changes semantics on the already-opened file.
 *	- with a 'f' command, it is a NFS V2 file handle instead of a pathname.
 *	- with a 'F' command, it is a NFS V3 file handle instead of a pathname.
 *
 * Protocol version 0 is for BDS 1.0 and 1.1
 * Protocol version 1 is for BDS 2.0
 * Protocol version 2 is for BDS 2.1
 */

#define	BDS_PROTOCOL_VERSION	2

#define	BDS_FILEOPEN	'O'		/* open a pathname */
#define	BDS_REOPEN	'o'		/* set buflen/flags on the open file */
#define	BDS_FH2OPEN	'f'		/* open a V2 file handle */
#define	BDS_FH3OPEN	'F'		/* open a V3 file handle */
#define	BDS_CLOSE	'C'		/* close the open file */
#define	BDS_READ	'R'		/* read */
#define	BDS_WRITE	'W'		/* write */
#define	BDS_SYNC	'S'		/* sync */
#define BDS_FCNTL	'L'		/* fcntl pass-through */
#define	BDS_ACK		'A'		/* Positive ack */
#define	BDS_NAK		'N'		/* nak w/ errno */

struct	_BDS_open {
	uint64	pathlen;	/* if 0, this is the nullproc interface. */
	uint64  buflen;		/* size for R/W buffers: ~0=="don't care" */
#define	BDS_OPEN_UNALIGNED    1	/* use unaligned semantics unless appending */
#define BDS_OPEN_WRITEBEHIND  2	/* loosen write semantics, fsync required */
#define	BDS_OPEN_WRITEBUFFER  4	/* deprecated -- do not use */
#define	BDS_OPEN_GETCONFIG    8	/* get configuration structure */
#define	BDS_OPEN_BUF_SINGLE  16	/* tell BDS to single buffer */
#define	BDS_OPEN_BUF_RENEW   32	/* tell BDS to use renew buffer semantics */
#define	BDS_OPEN_BUF_RENEWRR 64	/* tell BDS to use renew round-robin semantics */
#define BDS_OPEN_BUF_PREALLOC 128 /* tell BDS to try to pre-alloc all its buffers */
#define BDS_OPEN_BUF_KEEP   256	/* tell BDS to use keep semantics */
	uint64	flags;		/* reads are buffered to rbuflen bytes */
				/* the pathname follows */
};	/* 24 bytes */

struct	_BDS_rdwr {		/* command field is 'r' or 'w' */
	uint64	offset;
	uint64	length;
};	/* 16 bytes */

struct	_BDS_fcntl {		/* command field is 'l' */
	int	cmd;
	int	size;
	uint64	offset;
};	/* 16 bytes -- arg is passed after the bds_msg struct */

struct	_BDS_ack {		/* command field is 'a' */
	uint64	bytes;
	uint64	val1;		/* request-specific result */
	uint64	val2;
	uint64	val3;
};	/* 32 bytes */


/* BDS_OPEN_GETCONFIG -- protocol version 1 and greater */
/* If flag is set, a _BDS_config *must* be sent with an ACK */

struct	_BDS_config {
	uint64	cfg_bytes;	/* size of this structure in bytes */
	uint64	cfg_maxiosz;	/* struct dioattr's d_maxiosz */
	uint64	cfg_blksize;	/* struct stat's st_blksize */
};

typedef	struct {		/* BDS version 2.0 message */
	uint64	bds_cmd;
	uint64	bds_xid;
	uint64	bds_uid;
	uint64	bds_gid;
	uint64	bds_priority;
	union	{
		struct 	_BDS_open	bds_open;
		struct	_BDS_rdwr	bds_rdwr;
		struct	_BDS_fcntl	bds_fcntl;
		struct	_BDS_ack	bds_ack;
		uint64			bds_errno;
		char			min[32];	/* union is 32 bytes */
	}	bds_u;
} bds_msg;

#define	bds_pathlen	bds_u.bds_open.pathlen
#define bds_buflen	bds_u.bds_open.buflen
#define bds_oflags	bds_u.bds_open.flags
#define	bds_offset	bds_u.bds_rdwr.offset
#define	bds_length	bds_u.bds_rdwr.length
#define	bds_bytes	bds_u.bds_ack.bytes
#define	bds_ack1	bds_u.bds_ack.val1
#define	bds_error	bds_u.bds_errno


/*
 * _BDS_fcntl.cmd values
 * The BDS protocol uses a BDS_FCNTL command to convey assorted requests
 * via an opaque (well, translucent) data field. These are the legal values
 * for _BDS_fcntl.cmd, often related to standards fcntls found elsewhere.
 */

#define BDSFCNTL_XFS_ALLOCSP		 1	/* IRIX F_ALLOCSP */
#define BDSFCNTL_XFS_FREESP		 2	/* IRIX F_FREESP */
#define BDSFCNTL_XFS_RESVSP		 3	/* IRIX F_RESVSP */
#define BDSFCNTL_XFS_UNRESVSP		 4	/* IRIX F_UNRESVSP */
#define	BDSFCNTL_XFS_ALLOCSP64		 5	/* IRIX F_ALLOCSP64 */
#define BDSFCNTL_XFS_FREESP64		 6	/* IRIX F_FREESP64 */
#define BDSFCNTL_XFS_RESVSP64		 7	/* IRIX F_RESVSP64 */
#define BDSFCNTL_XFS_UNRESVSP64		 8	/* IRIX F_UNRESVSP64 */
#define BDSFCNTL_XFS_FSSETXATTR		 9	/* IRIX F_FSSETXATTR */
#define BDSFCNTL_XFS_FSGETXATTR		10	/* IRIX F_FSGETXATTR */
#define BDSFCNTL_XFS_FSGETXATTRA	11	/* IRIX F_FSGETXATTRA */
#define BDSFCNTL_BDS_GETBDSATTR		12	/* IRIX F_GETBDSATTR */
#define BDSFCNTL_XFS_FSOPERATIONS	13	/* IRIX SGI_XFS_FSOPERATIONS */


#ifndef	_KERNEL
/*
 * User level library representation of a remote file.
 */
typedef	struct	_BDS_NFS {
	char	*bnfs_localpath;	/* malloced local pathname */
	char	*bnfs_remotehost;	/* hostname of NFS server */
	char	*bnfs_remotepath;	/* pathname on remote server */
	int	bnfs_sock;		/* socket to server */
	uint64	bnfs_seekp;		/* current file offset */
	mode_t	bnfs_mode;		/* open stuff */
	int	bnfs_oflags;		/* open stuff */
} bnfs;

#define	BDSFD_CHUNK	16		/* allocate this many at a crack */

uint64	hton_errno(int error);
int	ntoh_errno(uint64 error);
uint64	next_xid();
int	BDS(int);
int	bds_close(int);
int	bds_open(int, const char *, int, mode_t);
int	bds_read(int, void *, unsigned);
bds_msg *bds_awrite(int, const void *, unsigned);
int	bds_adone(int, bds_msg *);
int	bds_write(int, const void *, unsigned);
int	bds_sync(int);
off_t	bds_seek(int, off_t, int);
uint64	bds_seek64(int, uint64, int);
char	*p64(uint64);
char	*p64sz(uint64);
void	decode(bds_msg *m);

#else	/* _KERNEL */

#ifdef	IO_BULK
#define	BDS_IOFLAG_MASK		(IO_DIRECT | IO_BULK)
#define BDS_BULK_AUTO		(64*1024) /* O_BULK: bdsauto = BDS_BULK_AUTO */
#else
#define	BDS_IOFLAG_MASK		(IO_DIRECT)
#endif

int	bds_open(struct rnode *rp, int ioflag, struct cred *cred);
int	bds_close(struct rnode *rp, struct cred *cred);
int	bds_write(struct rnode *rp, struct uio *uiop,
		  int ioflag, struct cred *cr);
int	bds_read(struct rnode *rp, struct uio *uiop,
		 int ioflag, struct cred *cr);
int	bds_sync(struct rnode *rp, struct cred *cr);
int	bds_fcntl(struct rnode *rp, int cmd, void *arg, off_t offset, struct cred *cr);
void	bds_unmount(mntinfo_t *mi);
long	bds_blksize(struct rnode *rp);
int	bds_vers(struct rnode *rp, int nfs_vers);
uint64	hton_errno(int error);
int	ntoh_errno(uint64 error);

#endif	/* _KERNEL */

#endif	/* _BDS_H */
