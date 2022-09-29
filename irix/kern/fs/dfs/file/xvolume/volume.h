/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: volume.h,v $
 * Revision 65.4  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.3  1998/02/24 15:32:21  lmc
 * The use of afsHyper should have been changed everywhere to afs_hyper_t.
 * afsHyper should only be used in idl files.  local_64bit_xlate.c
 * does the translation from afsHyper to afs_hyper_t.  This changes
 * the use of afsHyper to afs_hyper_t.
 *
 * Revision 65.1  1997/10/20  19:21:47  jdoak
 * *** empty log message ***
 *
 * $EndLog: $
 */

/*
 * Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/volume.h,v 65.4 1999/07/21 19:01:14 gwehrman Exp $
 */

#ifndef	TRANSARC_VOLUME_H
#define	TRANSARC_VOLUME_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */
#include <dcedfs/osi.h>
#include <dcedfs/common_data.h>
#include <dcedfs/lock.h>
#include <dcedfs/icl.h>
#include <dcedfs/queue.h>
#include <dcedfs/osi_param.h>
#include <dcedfs/xvfs_xattr.h>
#include <dcedfs/aclint.h>

#if defined(AFS_SUNOS5_ENV) || defined(AFS_HPUX_ENV)
#include <sys/dirent.h>
#else /* !AFS_SUNOS5_ENV || AFS_HPUX_ENV */
#include <sys/dir.h>
#endif	/* !AFS_SUNOS5_ENV || AFS_HPUX_ENV */

#define	VOLNAMESIZE	112

#ifdef KERNEL
extern struct icl_set *xops_iclSetp;
#endif /* KERNEL */

/* One of the trickier things here is how volume operations are
 * synchronized.  Basically, at most one volume operation can run
 * concurrently with some number (possibly 0) of compatible vnode
 * operations.
 *
 * The VOL_BUSY states flag is set iff there's a volume transaction
 * running.  If this is set, then the accStatus field is a bit mask
 * specifying which volume ops will be performed by this transaction,
 * and the concurrency field maps this information down into one of
 * VOL_CONCUR_ALLOPS / VOL_CONCUR_READONLY / VOL_CONCUR_NOOPS, describing
 * whether all vnode ops, readonly vnode ops or no vnode ops are allowed
 * to run concurrently with this transaction.
 *
 * If VOL_BUSY is set, and a conflicting vnode op tries to start, the
 * accError field tells what error to return.
 *
 * If any vnode ops are active, then the activeVnops count will be
 * non-zero, which will prevent any volume transactions from starting.
 * Note that even if all of the vnode ops are compatible with the vol
 * op that's starting, the system still waits for the vnode ops to complete,
 * since we don't bother tracking what type of vnode ops are running at
 * any given time.
 *
 * The VOL_GRABWAITING flag is set if a volume operation is waiting to
 * start; it will block new vnode ops from starting.
 * The VOL_LOOKUPWAITING flag is set if a vnode operation is waiting to
 * start.
 */

struct vol_stat_st {
    afs_hyper_t volId;
    afs_hyper_t parentId;
    afsTimeval cloneTime;	/* when this fileset was made via clone or reclone */
    afsTimeval vvCurrentTime;/* when lazy replica's VV was known current */
    afsTimeval vvPingCurrentTime;/* when last tried for VV of lazy replica */
#ifdef SGIMIPS
    __int32_t type;
    __uint32_t accStatus;	/* volops we'll be performing in this trans (bit mask) */
    __uint32_t accError;	/* error to return for incompatible vnode ops */
    __uint32_t states;	/* lots of individual VOL_xxxx bits */
    __uint32_t reclaimDally;    /* for the R/W backing some lazy-rep R/O */
    __int32_t tokenTimeout;
    __int32_t activeVnops;	/* count of active vnode ops */
    __int32_t volMoveTimeout;
    __int32_t procID;	/* pid of process making fileset busy */
    __int32_t spare5;
    __int32_t spare6;
    __int32_t spare7;
    __int32_t spare8;
    __int32_t spare9;
#else
    long type;
    u_long accStatus;	/* volops we'll be performing in this trans (bit mask) */
    u_long accError;	/* error to return for incompatible vnode ops */
    u_long states;	/* lots of individual VOL_xxxx bits */
    u_long reclaimDally;    /* for the R/W backing some lazy-rep R/O */
    long tokenTimeout;
    long activeVnops;	/* count of active vnode ops */
    long volMoveTimeout;
    long procID;	/* pid of process making fileset busy */
    long spare5;
    long spare6;
    long spare7;
    long spare8;
    long spare9;
#endif
    char volName[VOLNAMESIZE];
    unsigned char concurrency;	    /* level of concurrency for vnode ops */
    char cspares[15];
};
struct vol_stat_dy {
    afsTimeval creationDate;	/* when this fileset was created */
    afsTimeval updateDate;	/* when any update happened in this fileset */
    afsTimeval accessDate;	/* when any access was made to this fileset */
    afsTimeval backupDate;	/* spare */
    afsTimeval copyDate;	/* when the dump was made that was restored here */
#ifdef SGIMIPS
    afs_hyper_t volversion;
    afs_hyper_t backupId;
    afs_hyper_t cloneId;
    afs_hyper_t llBackId;			/* low-level backing ID */
    afs_hyper_t llFwdId;			/* low-level forward ID */
    afs_hyper_t allocLimit;
    afs_hyper_t allocUsage;
    afs_hyper_t visQuotaLimit;
    afs_hyper_t visQuotaUsage;
#else
    afsHyper volversion;
    afsHyper backupId;
    afsHyper cloneId;
    afsHyper llBackId;			/* low-level backing ID */
    afsHyper llFwdId;			/* low-level forward ID */
    afsHyper allocLimit;
    afsHyper allocUsage;
    afsHyper visQuotaLimit;
    afsHyper visQuotaUsage;
#endif
#ifdef SGIMIPS
    __int32_t fileCount;
    __int32_t minQuota;
    __int32_t owner;
    __int32_t unique;                /*  necessary if ``version'' is afsHyper? */
    __int32_t index;                 /* Redundant */
    __int32_t rwIndex;
    __int32_t backupIndex;
    __int32_t parentIndex;
    __int32_t cloneIndex;
    __int32_t nodeMax;
    __int32_t aggrId;
    __int32_t spare2;
    __int32_t spare3;
    __int32_t spare4;
    __int32_t spare5;
    __int32_t spare6;
    __uint32_t tag;
    __uint32_t msgLen;

#else
    long fileCount;
    long minQuota;
    long owner;
    long unique;		/*  necessary if ``version'' is afsHyper? */
    long index;			/* Redundant */
    long rwIndex;
    long backupIndex;
    long parentIndex;
    long cloneIndex;
    long nodeMax;
    long aggrId;
    long spare2;
    long spare3;
    long spare4;
    long spare5;
    long spare6;
    u_long tag;
    u_long msgLen;
#endif
    char statusMsg[128];
    char cspares[16];
};

struct vol_status {
    struct vol_stat_st vol_st;
    struct vol_stat_dy vol_dy;
};

#define VOL_MAX_BULKSETSTATUS	6

#ifdef SGIMIPS
typedef struct vol_statusDesc {
    union {
	long			volDesc;
	struct volume		*volp;
    }			vsd_volId;
    __uint32_t		vsd_mask;
    __uint32_t		vsd_spare;
    struct vol_status	vsd_status;
} vol_statusDesc_t;
#else
typedef struct vol_statusDesc {
    union {
	int			volDesc;
	struct volume		*volp;
    }			vsd_volId;
    int			vsd_mask;
    int			vsd_spare;
    struct vol_status	vsd_status;
} vol_statusDesc_t;
#endif /* SGIMIPS */
#define vsd_volDesc	vsd_volId.volDesc
#define vsd_volp	vsd_volId.volp

/* macros to figure out if an error is transient or persistent */
#define VOL_ERROR_IS_TRANSIENT(error) (error < VOLERR_TRANS_HIGHEST && \
					 error > VOLERR_TRANS_LOWEST)

/*
 * volume states (vol_stat_st.states)
 */
/* These really belong to v_voltypes */
#define VOL_TYPEBITS	0x00003	/* what low-level type of volume this is */
#define	VOL_RW		0x00001	/* R/W volume */
#define	VOL_READONLY	0x00002	/* ReadOnly Volume */


#define	VOL_BUSY		0x00004 /* Volume is Busy */
/* The following three definitions are obsolete */
#define	VOL_OFFLINE		0x00008 /* Volume is offline */
#define	VOL_DELONSALVAGE	0x00010 /* Delete On Salvage */
#define	VOL_OUTOFSERVICE	0x00020 /* Out Of service */
#define	VOL_DEADMEAT		0x00040 /* About to be deleted */
#define	VOL_LCLMOUNT		0x00080 /* Volume is mounted locally */

/* replication-specific state bits */
#define	VOL_REPFIELD		0x00f00	/* which replication created this? */
#define	VOL_REP_NONE		0x00000	/* none */
#define	VOL_REP_RELEASE		0x00100	/* ``vos release'' */
#define	VOL_REP_LAZY		0x00200	/* lazy replication */
#define	VOL_IS_COMPLETE	0x01000	/* this volume instance is completed */
#define	VOL_HAS_TOKEN	0x02000	/* our VVage should always be zero. */
#if 0
/* no longer used -- can be reclaimed for some other purpose */
#define	VOL_KNOWDALLY	0x04000	/* know reclaimDally value for this volume */
#endif
#define	VOL_NOEXPORT		0x08000	/* Do not export this volume! */

#define	VOL_TYPEFIELD	0xf0000	/* what high-level type of volume this is */
#define	VOL_TYPE_RW		0x10000	/* read-write (ordinary) */
#define	VOL_TYPE_RO		0x20000	/* ``.readonly'' */
#define	VOL_TYPE_BK		0x30000	/* ``.backup'' */
#define	VOL_TYPE_TEMP	0x40000	/* temporary use, for dumps and moves etc. */

#define	VOL_GRABWAITING	0x100000    /* a grab is pending for this volume */
#define	VOL_LOOKUPWAITING	0x200000    /* a lookup is pending for this volume */
#define	VOL_REPSERVER_MGD	0x400000    /* managed by repserver */
#define	VOL_MOVE_TARGET	0x800000    /* this vol is a target for a move op */
#define	VOL_MOVE_SOURCE	0x1000000   /* this vol is a source for a move op */
#define VOL_ZAPME	0x2000000   /* delete volume with extreme prejudice */
#define VOL_CLONEINPROG 0x4000000   /* this vol is partially cloned */
#define	VOL_IS_REPLICATED 0x8000000 /* this vol is replicated */
#define VOL_OPENDONE	0x10000000   /* back from VOL_OPEN procedure */

/* Bits that the kernel maintains alone, not by VOLOP_SETSTATUS */
#define	VOL_BITS_NOSETSTATUS (VOL_BUSY|VOL_DEADMEAT|VOL_GRABWAITING|VOL_LOOKUPWAITING|VOL_DELONSALVAGE|VOL_OPENDONE|VOL_LCLMOUNT)
/* (This used to be VOL_OFFLINE, but VOL_DELONSALVAGE is a better, persistent initializer.) */
#define       VOL_BITS_INITIAL    (0)
/* Don't turn off VOL_DELONSALVAGE when attaching a volume.  It is a */
/* kernel-maintained bit that is also persistent, unlike the other bits */
/* in VOL_BITS_NOSETSTATUS */
#define	VOL_BITS_INITIALMASK (VOL_BITS_INITIAL \
			      | (VOL_BITS_NOSETSTATUS & ~VOL_DELONSALVAGE))
/* During an identity swap, preserve these bits in their original struct
 * volume, since they are logically connected with the fileset's identity. */
#define VOL_BITS_NOSWAP	(VOL_GRABWAITING|VOL_LOOKUPWAITING|VOL_LCLMOUNT)

/*
 * Useful macros in volume pointers
 */
#define VOL_READWRITE(volp)	((volp)->v_stat_st.states & VOL_RW)
#define VOL_EXPORTED(volp)      (!((volp)->v_stat_st.states & VOL_NOEXPORT))

/*
 * Mask bits for the VOL_SETSTATUS function MASK argument
 */
#define	VOL_STAT_VOLNAME	0x00000001
#define	VOL_STAT_VOLID		0x00000002
#define	VOL_STAT_VERSION	0x00000004
#define	VOL_STAT_UNIQUE		0x00000008
#define	VOL_STAT_OWNER		0x00000010
#define	VOL_STAT_TYPE		0x00000020
#define	VOL_STAT_STATES		0x00000040
#define	VOL_STAT_ALLOCLIMIT	0x00000080
#define	VOL_STAT_BACKUPID	0x00000100
#define	VOL_STAT_PARENTID	0x00000200
#define	VOL_STAT_CLONEID	0x00000400
#define	VOL_STAT_CREATEDATE	0x00000800
#define	VOL_STAT_UPDATEDATE	0x00001000
#define	VOL_STAT_ACCESSDATE	0x00002000
#define	VOL_STAT_COPYDATE	0x00004000
#define	VOL_STAT_NODEMAX	0x00008000
#define	VOL_STAT_VISLIMIT	0x00010000
#define	VOL_STAT_MINQUOTA	0x00020000
/* spare	VOL_STAT_SIZE		0x00040000 */
#define	VOL_STAT_INDEX		0x00080000
#define	VOL_STAT_BACKVOLINDEX	0x00100000
#define	VOL_STAT_STATUSMSG	0x00200000
#define	VOL_STAT_CLONETIME	0x00400000
#define	VOL_STAT_VVCURRTIME	0x00800000
#define	VOL_STAT_VVPINGCURRTIME 0x01000000
/* spare	VOL_STAT_ACCERROR	0x02000000 */
#define	VOL_STAT_BACKUPDATE	0x04000000
#define	VOL_STAT_RECLAIMDALLY	0x08000000
#define	VOL_STAT_LLBACKID	0x10000000
#define	VOL_STAT_LLFWDID	0x20000000
#define VOL_STAT_VOLMOVETIMEOUT	0x40000000

struct vol_vp_elem {
    struct vol_vp_elem *next;
    struct vnode *vp;
};

typedef struct vol_vp_elem * vol_vp_elem_p_t;

#define VOL_NUM_ALLOC_FREE_VP 64

/*
 * In-core volume structure
 */
struct volume {
    struct squeue	v_lruq;		/* lruq + free list queue */
#ifdef SGIMIPS
/*
 * XXX-RAG
 * v_next/v_prev defines are used in irix/kern/sys/vnode.h
 * for now just changing v_next to vl_next here.
 */
    struct volume       *vl_next;
#else
    struct volume	*v_next;
#endif
    struct aggr		*v_paggrp;	/* INVARIANT */
    struct volumeops	*v_volOps;
    struct lock_data	v_lock;
    long		v_count;	/* ref count for storage */
    void		*v_tpqRequest;
    opaque		v_fsDatap;
    struct vol_stat_st	v_stat_st;
    vol_vp_elem_p_t     v_vp;          /* list of vnodes to rele */
};

/*
 * XXX By defining all volume's fields as v_* we can freely move them around
 * without changing the code all over XXX
 */
/* stuff in vol_stat_st */
#define	v_volId			v_stat_st.volId
#define	v_parentId		v_stat_st.parentId
#define	v_cloneTime		v_stat_st.cloneTime
#define	v_vvCurrentTime		v_stat_st.vvCurrentTime
#define	v_vvPingCurrentTime	v_stat_st.vvPingCurrentTime
#define	v_voltype		v_stat_st.type
#define	v_accStatus		v_stat_st.accStatus
#define	v_accError		v_stat_st.accError
#define	v_states		v_stat_st.states
#define	v_reclaimDally		v_stat_st.reclaimDally
#define v_tokenTimeout          v_stat_st.tokenTimeout
#define v_activeVnops		v_stat_st.activeVnops
#define v_volMoveTimeout        v_stat_st.volMoveTimeout
#define v_procID		v_stat_st.procID
#define	v_volName		v_stat_st.volName
#define v_concurrency		v_stat_st.concurrency


/*
 * Internal volume handle for all VOP_* calls.
 * In-kernel structure only; a volume descriptor (long) is passed to the user
 * land to associate it with this structure.
 */
struct vol_handle {
    struct volume *volp;		/* pointer to incore volume */
    long opentype;			/* Open volume type */
    struct afsFid fid;			/* Fid for "active" volume anode */
    long spare1;
    long spare2;
};

/*
 * Structure used by VOL_READDIR and VOL_APPENDDIR.
 */
typedef struct vol_dirent {
    int32	offset;			/* Logical directory offset */
    int32	vnodeNum;		/* Canonical vnode number */
    u_int32	codesetTag;		/* Tag for the name's codeset */
    u_short	reclen;			/* Len of this rec, 4-byte boundary */
    u_short	namelen;		/* Length of `name' */
    char	name[OSI_MAXNAMLEN+1];	/* File name, variable sized */
} vol_dirent_t;

#define VOL_FILL_DIRENT(de, off, num, tag, name)				\
MACRO_BEGIN								\
    de.offset = (off);							\
    de.vnodeNum = (num);						\
    de.codesetTag = tag;						\
    de.namelen = strlen(name);						\
    (void)strcpy(de.name, (name));					\
    de.reclen = 3 * sizeof(int32) + 2 * sizeof(u_short) + de.namelen + 1;	\
    de.reclen = (de.reclen + sizeof(int32) - 1) & ~(sizeof(int32) - 1);	\
MACRO_END

/* Parameter for VOLOP_GETNEXTHOLES */
#define	VOLHOLE_MAX_HOLES   10
struct vol_NextHole {
    afs_hyper_t startPoint;		/* Return all holes starting here--zero this to start */
    unsigned long flags;
    unsigned long outCount;		/* count of returned holes */
    unsigned long spare1;
    unsigned long spare2;
    struct vol_holeDesc {
	afs_hyper_t holeStart;	/* byte in file where the hole starts */
	afs_hyper_t holeLen;	/* byte count of the hole */
    } holes[VOLHOLE_MAX_HOLES];
};
/* IN flags: */
    /* none yet */
/* OUT flags: */
#define VOLHOLE_FLAG_LAST	0x1	/* no more holes in file past these */

/* 
 * Definition of the prime data structure used for the VOL_READHOLE volop.
 *
 * VOL_READHOLE is the preferred volop to use to find out about file data and
 * holes as opposed to a combination of VOL_READ and VOL_GETNEXTHOLES as 
 * VOL_READHOLE avoids traversing the file block map twice as would be
 * required with any combination of the defined VOL_READ and VOL_GETNEXTHOLES
 * semantics. 
 *
 * The following comments expand on the semantics of this new interface.
 * Let ioff, ilen represent the range of data to be dumped. These are
 * the IN arguments i.e. ioff, ilen represent the value of "offset"
 * and "length" on input.
 *
 * Let ooff, olen represent the range of data actually dumped into the
 * passed in data buffer, i.e. ooff, olen represent the value of
 * "offset" and "length" on output.
 *
 * Let hoff, hlen represent any holes on output. i.e. hoff, hlen
 * represent the value of "holeOff" and "holeLen" on output.
 * 
 * Let X represent a dontcare value
 *
 * There are basically 3 interesting cases:
 *
 * A. On output olen == 0 && hlen != 0 i.e. the file has a hole at the
 *    beginning of the requested range [ioff, ioff + ilen)
 *    In this case
 * 	ooff = X
 *	hoff == ioff 
 * 	No constraint on value of hlen wrt ilen
 * 
 * B. On output olen != 0 && hlen == 0 i.e the file has allocated region
 *    in the beginning of the requested range [ioff, ioff + ilen)
 *    In this case
 *	ooff == ioff
 *	olen <= ilen
 *	hoff = X
 *	
 * C. On output olen != 0 && hlen != 0. 
 *    In this case, either
 *	ooff == ioff
 *	ooff + olen == hoff
 * 	olen <= ilen
 *	No constraint on value of hlen wrt ilen
 *    or
 *	hoff == ioff
 *	hoff + hlen == ooff
 * 	hlen + olen <= ilen
 * 
 * The above assumes an infinite file length for the sake of simplifying
 * the discussion. But we also assert that ioff < fileLen. If (ioff +
 * ilen) >= fileLen, the filesystem should attempt to set the
 * VOL_READHOLE_EOF flag bit. The filelength will also naturally
 * constrain olen and hlen 
 *
 * Hope this above explanation helps.
 */

#define VOL_READHOLE_EOF 0x1		
struct vol_ReadHole {
    afs_hyper_t offset; 	/* offset to read from on input, 
			 * offset from which buffer contains data on output */
    int length; 	/* length to read on input 
			 * length actually read on output - if no data then
			 * length is zero on output */
    char *buffer;  
    int flags; 	/* VOL_READHOLE_XXX flags defined above */
    struct vol_FileHole {
	/* 
	 * Only an OUT argument. On output, specifies
	 * hole in range [offset, offset + length). If no such hole, 
	 * the possible hole in [offset + length, ...).
	 * If no hole in region, holeLen is set to zero on output.
	 * If on output the data buffer above contains file data, then
	 * the hole indication has to be adjacent to that data. The hole
	 * could be adjacent on either end of the data i.e.
	 *
	 * if (length != 0) 
	 *    assert((holeOff == offset + length) || 
	 * 	      (holeOff + holeLen == offset));
	 */
	afs_hyper_t holeOff;	
	afs_hyper_t holeLen;
    } hole;
};

/*
 * Volume related error codes
 */
/* currently these overlay the errno error space; in fact, they are returned
 * from the afs_syscall interface via errno.
 * They're also returned from the ftserver, so they need to be defined in
 * terms of the errno space.
 */
#define	VOL_ERR_EOF		EPERM /* was 1 */
#define	VOL_ERR_DELETED		ENOENT /* was 2 */
#define	VOL_ERR_EOW		ENOEXEC /* was 8 */

struct volumeops {
    int	(*vol_hold)(struct volume *);
    int	(*vol_rele)(struct volume *);
    int	(*vol_lock)(struct volume *, int);
    int	(*vol_unlock)(struct volume *, int);
    int	(*vol_open)(struct volume *, int, int, struct vol_handle *);
    int	(*vol_seek)(struct volume *, int, struct vol_handle *);
    int	(*vol_tell)(struct volume *, struct vol_handle *, int *);
    int	(*vol_scan)(struct volume *, int, struct vol_handle *);
    int	(*vol_close)(struct volume *, struct vol_handle *, int);
    int	(*vol_destroy)(struct volume *);
    int	(*vol_attach)(struct volume *);
    int	(*vol_detach)(struct volume *, int);		/* XXX */
    int	(*vol_getstatus)(struct volume *, struct vol_status *);
    int	(*vol_setstatus)(struct volume *, int, struct vol_status *);

    /* VOL_CREATE implementations needs to delete existing file if
     * necessary. Refer defects 5449, 5451 */
    int	(*vol_create)(struct volume *, int, struct xvfs_attr *,
		      struct vol_handle *, osi_cred_t *);
    int	(*vol_read)(struct volume *, struct afsFid *, afs_hyper_t, int, char *,
		    osi_cred_t *, int *);
    int	(*vol_write)(struct volume *, struct afsFid *, afs_hyper_t, int, char *,
		     osi_cred_t *);
    int	(*vol_truncate)(struct volume *, struct afsFid *, afs_hyper_t, osi_cred_t *);
    int	(*vol_delete)(struct volume *, struct afsFid *, osi_cred_t *);
    int	(*vol_getattr)(struct volume *, struct afsFid *, struct xvfs_attr *,
		       osi_cred_t *);
    int	(*vol_setattr)(struct volume *, struct afsFid *, struct xvfs_attr *,
		       osi_cred_t *);
    int	(*vol_getacl)(struct volume *, struct afsFid *, struct dfs_acl *, int,
		      osi_cred_t *);
    int	(*vol_setacl)(struct volume *, struct afsFid *, struct dfs_acl *, int,
		      int, osi_cred_t *);
    int	(*vol_clone)(struct volume *, struct volume *, osi_cred_t *);
    int	(*vol_reclone)(struct volume *, struct volume *, osi_cred_t *);
    int	(*vol_unclone)(struct volume *, struct volume *, osi_cred_t *);

    int	(*vol_vget)(struct volume *, struct afsFid *, struct vnode **);
    int	(*vol_root)(struct volume *, struct vnode **);
    int	(*vol_isroot)(struct volume *, struct afsFid *, int *);
    int	(*vol_getvv)(struct volume *, afs_hyper_t *);
    int	(*vol_setdystat)(struct volume *, struct vol_stat_dy *);
    int	(*vol_freedystat)(struct volume *);
    int	(*vol_setnewvid)(struct volume *, afs_hyper_t *);
    int	(*vol_copyacl)(struct volume *, struct afsFid *, int, int, int,
		       osi_cred_t *);
    int	(*vol_concurr)(struct volume *, int, int, unsigned char *);
    int	(*vol_swapids)(struct volume *, struct volume *, osi_cred_t *);
    int (*vol_sync)(struct volume *, int);
    int (*vol_pushstatus)(struct volume *);

    int (*vol_readdir)(struct volume *, struct afsFid *, int, char *,
		       osi_cred_t *, afs_hyper_t *, int *);
    int (*vol_appenddir)(struct volume *, struct afsFid *, u_int, u_int,
			 char *, int, osi_cred_t *);

    int (*vol_bulksetstatus)(u_int, struct vol_statusDesc *);
    int (*vol_getzlc)(struct volume *, u_int *, struct vnode **);
    int	(*vol_getnextholes)(struct volume *, struct afsFid *,
			    struct vol_NextHole *, osi_cred_t *);
    int (*vol_deplete)(struct volume *);
    int (*vol_readhole)(struct volume *, struct afsFid *,
			struct vol_ReadHole *, osi_cred_t *);

    int (*vol_dmwait)(struct volume *, opaque *);	/* wait for a DM blob to signal */
    int (*vol_dmfree)(struct volume *, opaque *);	/* release pointer to DM blob */
    int (*vol_startvnop)(struct volume *, int, int, opaque *);
    int (*vol_endvnop)(struct volume *, opaque *);
    int (*vol_startbzop)(struct volume *, int, opaque *);
    void (*vol_startinactop)(struct volume *, struct vnode *, opaque *);
};

/*
 * VOLOP bit definitions
 */
#define	VOL_OP_SEEK		0x00000001
#define	VOL_OP_TELL		0x00000002
#define	VOL_OP_SCAN		0x00000004
#define	VOL_OP_DESTROY	0x00000008
#define	VOL_OP_GETSTATUS	0x00000010
#define	VOL_OP_SETSTATUS	0x00000020
#define	VOL_OP_CREATE	0x00000040
#define	VOL_OP_READ		0x00000080
#define VOL_OP_READHOLE VOL_OP_READ
#define	VOL_OP_WRITE		0x00000100
#define	VOL_OP_TRUNCATE	0x00000200
#define	VOL_OP_DELETE	0x00000400
#define	VOL_OP_GETATTR	0x00000800
#define	VOL_OP_SETATTR	0x00001000
#define	VOL_OP_GETACL	0x00002000
#define	VOL_OP_SETACL	0x00004000
#define	VOL_OP_CLONE	0x00008000
#define	VOL_OP_RECLONE	0x00010000
#define	VOL_OP_UNCLONE	0x00020000
#define	VOL_OP_SETNEWVID	0x00040000
#define	VOL_OP_COPYACL	0x00080000
#define	VOL_OP_SWAPIDS	0x00100000
/* now, for the piece of SETSTATUS that changes the fileset ID */
#define	VOL_OP_SETSTATUS_ID	0x00200000
#define VOL_OP_SYNC		0x00400000
#define VOL_OP_PUSHSTATUS	0x00800000
#define VOL_OP_READDIR		0x01000000
#define VOL_OP_APPENDDIR	0x02000000
#define VOL_OP_GETZLC		0x04000000
/* Forcibly (architecturally) deny concurrent vnode ops */
#define VOL_OP_NOACCESS		0x08000000
#define	VOL_OP_GETNEXTHOLES	0x10000000
#define VOL_OP_DETACH		0x20000000
#define VOL_OP_DEPLETE		0x40000000

/*
 * VOL syscall to VOLOP translation
 * Translates syscalls to volop collections;
 * Depends on xvolume/vol_init.c implementation.
 */

#define	VOL_SYS_SEEK	    (VOL_OP_SEEK)
#define	VOL_SYS_TELL	    (VOL_OP_TELL)
#define	VOL_SYS_SCAN	    (VOL_OP_SCAN)
#define	VOL_SYS_DESTROY	    (VOL_OP_DESTROY)
#define	VOL_SYS_GETSTATUS   (VOL_OP_GETSTATUS)
#define	VOL_SYS_SETSTATUS   (VOL_OP_SETSTATUS)
#define	VOL_SYS_CREATE	    (VOL_OP_CREATE)
#define	VOL_SYS_READ	    (VOL_OP_READ)
#define VOL_SYS_READHOLE    (VOL_OP_READHOLE)
#define	VOL_SYS_WRITE	    (VOL_OP_WRITE)
#define	VOL_SYS_TRUNCATE    (VOL_OP_TRUNCATE)
#define	VOL_SYS_DELETE	    (VOL_OP_DELETE)
#define	VOL_SYS_GETATTR	    (VOL_OP_GETATTR)
#define	VOL_SYS_SETATTR	    (VOL_OP_SETATTR)
#define	VOL_SYS_GETACL	    (VOL_OP_GETACL)
#define	VOL_SYS_SETACL	    (VOL_OP_SETACL)
#define	VOL_SYS_CLONE       (VOL_OP_CLONE)
#define	VOL_SYS_RECLONE     (VOL_OP_RECLONE)
#define	VOL_SYS_UNCLONE     (VOL_OP_UNCLONE)
#define	VOL_SYS_SETVV	    (VOL_OP_GETSTATUS | VOL_OP_SETSTATUS)
#define	VOL_SYS_SWAPVOLIDS  (VOL_OP_SWAPIDS)
#define	VOL_SYS_COPYACL	    (VOL_OP_COPYACL)
#define VOL_SYS_SYNC	    (VOL_OP_SYNC)
#define VOL_SYS_PUSHSTATUS  (VOL_OP_PUSHSTATUS)
#define VOL_SYS_READDIR     (VOL_OP_READDIR)
#define VOL_SYS_APPENDDIR   (VOL_OP_APPENDDIR)
#define VOL_SYS_GETZLC	    (VOL_OP_GETZLC)
#define VOL_SYS_GETNEXTHOLES	    (VOL_OP_GETNEXTHOLES)
/* Forcibly (architecturally) deny concurrent vnode ops */
#define VOL_SYS_NOACCESS    (VOL_OP_NOACCESS)
#define VOL_SYS_DETACH	    (VOL_OP_DETACH)
#define VOL_SYS_DEPLETE	    (VOL_OP_DEPLETE)

#define VOL_SYNC_COMMITSTATUS	1
#define VOL_SYNC_COMMITMETA	2
#define VOL_SYNC_COMMITALL	3

#ifdef	KERNEL
#define VOL_HOLD(VOLP) \
    (*(VOLP)->v_volOps->vol_hold)(VOLP)
#define VOL_RELE(VOLP) \
    (*(VOLP)->v_volOps->vol_rele)(VOLP)
#define VOL_LOCK(VOLP, TYPE) \
    (*(VOLP)->v_volOps->vol_lock)(VOLP, TYPE)
#define VOL_UNLOCK(VOLP, TYPE) \
    (*(VOLP)->v_volOps->vol_unlock)(VOLP, TYPE)
#define VOL_OPEN(VOLP, TYPE, OPENERR, DESC) \
    (*(VOLP)->v_volOps->vol_open)(VOLP, TYPE, OPENERR, DESC)
#define VOL_SEEK(VOLP, DESC, INDEX) \
    (*(VOLP)->v_volOps->vol_seek)(VOLP, DESC, INDEX)
#define VOL_TELL(VOLP, DESC, INDEX) \
    (*(VOLP)->v_volOps->vol_tell)(VOLP, DESC, INDEX)
#define VOL_SCAN(VOLP, DESC, ADESC) \
    (*(VOLP)->v_volOps->vol_scan)(VOLP, DESC, ADESC)
#define VOL_CLOSE(VOLP, DESC, ISABORT) \
    (*(VOLP)->v_volOps->vol_close)(VOLP, DESC, ISABORT)
#define VOL_DESTROY(VOLP) \
    (*(VOLP)->v_volOps->vol_destroy)(VOLP)
#define VOL_ATTACH(VOLP) \
    (*(VOLP)->v_volOps->vol_attach)(VOLP)
#define VOL_DETACH(VOLP, ANYFORCE) \
    (*(VOLP)->v_volOps->vol_detach)(VOLP, ANYFORCE)
#define VOL_GETSTATUS(VOLP, VOLSTATUS) \
    (*(VOLP)->v_volOps->vol_getstatus)(VOLP, VOLSTATUS)
#define VOL_SETSTATUS(VOLP, MASK, VOLSTATUS) \
    (*(VOLP)->v_volOps->vol_setstatus)(VOLP, MASK, VOLSTATUS)
#define VOL_CREATE(VOLP, INDEX, XVATTR, ADESC, CRED) \
    (*(VOLP)->v_volOps->vol_create)(VOLP, INDEX, XVATTR, ADESC, CRED)
#define VOL_READ(VOLP, ADESC, STRTPOS, LEN, BUFF, CRED, OUTLEN) \
    (*(VOLP)->v_volOps->vol_read)(VOLP, ADESC, STRTPOS, LEN, BUFF, CRED, OUTLEN)
#define VOL_WRITE(VOLP, ADESC, STRTPOS, LEN, BUFF, CRED) \
    (*(VOLP)->v_volOps->vol_write)(VOLP, ADESC, STRTPOS, LEN, BUFF, CRED)
#define VOL_TRUNCATE(VOLP, ADESC, NEWSIZE, CRED) \
    (*(VOLP)->v_volOps->vol_truncate)(VOLP, ADESC, NEWSIZE, CRED)
#define VOL_DELETE(VOLP, ADESC, CRED) \
    (*(VOLP)->v_volOps->vol_delete)(VOLP, ADESC, CRED)
#define VOL_GETATTR(VOLP, ADESC, XVATTR, CRED) \
    (*(VOLP)->v_volOps->vol_getattr)(VOLP, ADESC, XVATTR, CRED)
#define VOL_SETATTR(VOLP, ADESC, XVATTR, CRED) \
    (*(VOLP)->v_volOps->vol_setattr)(VOLP, ADESC, XVATTR, CRED)
#define VOL_GETACL(VOLP, ADESC, ACLP, W, CRED) \
    (*(VOLP)->v_volOps->vol_getacl)(VOLP, ADESC, ACLP, W, CRED)
#define VOL_SETACL(VOLP, ADESC, ACLP, INDEX, W, CRED) \
    (*(VOLP)->v_volOps->vol_setacl)(VOLP, ADESC, ACLP, INDEX, W, CRED)
#define VOL_CLONE(VOLP, VOL2P, CRED) \
    (*(VOLP)->v_volOps->vol_clone)(VOLP, VOL2P, CRED)
#define VOL_RECLONE(VOLP, VOL2P, CRED) \
    (*(VOLP)->v_volOps->vol_reclone)(VOLP, VOL2P, CRED)
#define VOL_UNCLONE(VOLP, VOL2P, CRED) \
    (*(VOLP)->v_volOps->vol_unclone)(VOLP, VOL2P, CRED)
#define VOL_VGET(VOLP, FID, VPP) \
    (*(VOLP)->v_volOps->vol_vget)(VOLP, FID, VPP)
#define VOL_ROOT(VOLP, VPP) \
    (*(VOLP)->v_volOps->vol_root)(VOLP, VPP)
#define VOL_ISROOT(VOLP, FID, ISFLAG) \
    (*(VOLP)->v_volOps->vol_isroot)(VOLP, FID, ISFLAG)
#define VOL_GETVV(VOLP, VVP) \
    (*(VOLP)->v_volOps->vol_getvv)(VOLP, VVP)
#define VOL_SETDYSTAT(VOLP, DYSTATP) \
    (*(VOLP)->v_volOps->vol_setdystat)(VOLP, DYSTATP)
#define VOL_FREEDYSTAT(VOLP) \
    (*(VOLP)->v_volOps->vol_freedystat)(VOLP)
#define VOL_SETNEWVID(VOLP, NEWIDP) \
    (*(VOLP)->v_volOps->vol_setnewvid)(VOLP, NEWIDP)
#define VOL_COPYACL(VOLP, ADESC, DW, INDEX, SW, CRED) \
    (*(VOLP)->v_volOps->vol_copyacl)(VOLP, ADESC, DW, INDEX, SW, CRED)
#define VOL_CONCURR(VOLP, TYPE, OPENERR, CONCURR) \
    (*(VOLP)->v_volOps->vol_concurr)(VOLP, TYPE, OPENERR, CONCURR)
#define VOL_SWAPIDS(VOLP, VOL2P, CRED) \
    (*(VOLP)->v_volOps->vol_swapids)(VOLP, VOL2P, CRED)
#define VOL_SYNC(VOLP, GUARANTEE) \
    (*(VOLP)->v_volOps->vol_sync)(VOLP, GUARANTEE)
#define VOL_PUSHSTATUS(VOLP) \
    (*(VOLP)->v_volOps->vol_pushstatus)(VOLP)
#define VOL_READDIR(VOLP, FIDP, LEN, BUF, CRED, POSP, NENTRIESP) \
    (*(VOLP)->v_volOps->vol_readdir)(VOLP, FIDP, LEN, BUF, CRED, POSP, NENTRIESP)
#define VOL_APPENDDIR(VOLP, FIDP, NENTRIES, LEN, BUF, KEEPOFFSETS, CRED) \
    (*(VOLP)->v_volOps->vol_appenddir)(VOLP, FIDP, NENTRIES, LEN, BUF, KEEPOFFSETS, CRED)
#define VOL_BULKSETSTATUS(VOLP, ARRAYLEN, DESCARRAY) \
    (*(VOLP)->v_volOps->vol_bulksetstatus)(ARRAYLEN, DESCARRAY)
#define VOL_GETZLC(VOLP, ITERP, VPP) \
    (*(VOLP)->v_volOps->vol_getzlc)(VOLP, ITERP, VPP)
#define VOL_GETNEXTHOLES(VOLP, FIDP, ITERP, CRED) \
    (*(VOLP)->v_volOps->vol_getnextholes)(VOLP, FIDP, ITERP, CRED)
#define VOL_DEPLETE(VOLP) \
    (*(VOLP)->v_volOps->vol_deplete)(VOLP)
#define VOL_READHOLE(VOLP, FIDP, READHOLEINFOP, CRED) \
    (*(VOLP)->v_volOps->vol_readhole)(VOLP, FIDP, READHOLEINFOP, CRED)
#define VOL_DMWAIT(VOLP, BLOBPP) \
    (*(VOLP)->v_volOps->vol_dmwait)(VOLP, BLOBPP)
#define VOL_DMFREE(VOLP, BLOBPP) \
    (*(VOLP)->v_volOps->vol_dmfree)(VOLP, BLOBPP)
#define VOL_STARTVNOP(VOLP, AA, BB, BLOBPP) \
    (*(VOLP)->v_volOps->vol_startvnop)(VOLP, AA, BB, BLOBPP)
#define VOL_ENDVNOP(VOLP, BLOBPP) \
    (*(VOLP)->v_volOps->vol_endvnop)(VOLP, BLOBPP)
#define VOL_STARTBZOP(VOLP, FLG, BLOBPP) \
    (*(VOLP)->v_volOps->vol_startbzop)(VOLP, FLG, BLOBPP)
#define VOL_STARTINACTOP(VOLP, VP, BLOBPP) \
    (*(VOLP)->v_volOps->vol_startinactop)(VOLP, VP, BLOBPP)

#define vol_StartVnodeOp(VOLP, AA, BB, BLOBPP) \
	((VOLP)? VOL_STARTVNOP(VOLP, AA, BB, BLOBPP) : 0)
#define vol_EndVnodeOp(VOLP, BLOBPP) \
	((VOLP)? VOL_ENDVNOP(VOLP, BLOBPP) : 0)
#define vol_StartBusyOp(VOLP, AA, BB, BLOBPP) \
	((VOLP)? VOL_STARTBZOP(VOLP, AA, BB, BLOBPP) : 0)
#define vol_StartInactiveVnodeOp(VOLP, VP, BLOBPP) \
	{if (VOLP) VOL_STARTINACTOP(VOLP, VP, BLOBPP);}
#endif	/* KERNEL */

/* volume xcode flags for vol_StartVnodeOp */
#define VOL_XCODE_NOWAIT	1	/* don't wait for transient errors */
#define VOL_XCODE_SOURCEOK	2	/* VOL_MOVE_SOURCE flag ok */
#define VOL_XCODE_TARGETOK	4	/* VOL_MOVE_TARGET flag ok */
#define VOL_XCODE_RECURSIVE	8	/* recursive VOP call: don't yield
					 * to ftserver (would deadlock).
					 */

/*
 * The following represents the special index for the volume's root anode. The
 * special dummy index is required since various physical file systems use
 * different numbers for their root anode (ufs = 2, episode = 15, etc). Do NOT
 * change its definition since code depends on it (i.e. scanning
 * -1, 0, 1 ... maxino is assumed by various programs).
 */
#define	VOL_ROOTINO	-1

/*
 * Define the largest 64-bit constant that is evenly divisible by 64K and
 * whose value will fit in 32 bits when divided by 1024.  This is the
 * maximum value allowed by LFS when the aggregate fragment size is 1K (the
 * smallest frag. size allowed); although larger quota values are possible
 * when the fragment size is increased.  The value must also be evenly
 * divisible by 64K to prevent rounding errors when converting to units of
 * 64K (or less), the largest fragment size allowed by LFS.
 *
 * The following 64 bit number is approximately 4.4 terabytes.
 */
#define VOL_MAX_QUOTA_HIGH	0x3ff
#define VOL_MAX_QUOTA_LOW	0xffff0000

#endif	/* TRANSARC_VOLUME_H */
