/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: aggr.h,v $
 * Revision 65.4  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.3  1998/01/20 17:16:25  lmc
 * Lots of changes from longs to 32 bit ints.  Also added explicit
 * casting of longs to ints where it was appropriate (the values would
 * always be less than 32bits.).
 *
 * Revision 65.1  1997/10/20  19:20:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.118.1  1996/10/02  21:11:17  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:23  damon]
 *
 * $EndLog$
 */
/*
 * Copyright (C) 1990, 1996 Transarc Corporation -- All Rights Reserved
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/aggr.h,v 65.4 1999/07/21 19:01:14 gwehrman Exp $
 */

/*
 * Aggregates and Aggregate Registry
 */

#ifndef	TRANSARC_AGGR_H
#define	TRANSARC_AGGR_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/osi.h>
#include <dcedfs/common_def.h>
#include <dcedfs/lock.h>
#ifdef KERNEL
#include <dcedfs/icl.h>
#endif /* KERNEL */
#include <dcedfs/volume.h>

#define	MAX_AGGRNAME	64		/* XXXX */
/*
 * Aggregate's general static status info: the piece that sits in the
 * ``struct aggr''
 */
struct ag_status_st {
#ifdef SGIMIPS
    __int32_t aggrId;                   /* Aggregate Id */
    __int32_t nVolumes;                 /* # of attached volumes */
    __int32_t localMounts;		/* # of locally mounted file sets */
    __int32_t spare1, spare2, spare3;	/* Some spares */
#else
    long aggrId;			/* Aggregate Id */
    long nVolumes;			/* # of attached volumes */
    long localMounts;			/* # of locally mounted file sets */
    long spare1, spare2, spare3;	/* Some spares */
#endif
    dev_t device;                       /* major/minor device number */
    char aggrName[MAX_AGGRNAME];        /* Aggregate name */
    char devName[32];                   /* Aggr device's name */
    char type;                          /* UFS, PFS, AIX3, etc. */
    char spares[16];                    /* More spares */

};

/*
 * The dynamic aggregate status info: the piece that is computed on the fly
 */
struct ag_status_dy {
#ifdef SGIMIPS
    __int32_t blocks;                   /* # blks (of size fragsize) in aggr */
    __int32_t blocksize;                /* block size of aggregate */
    __int32_t totalUsable;              /* total available blocks */
    __int32_t realFree;                 /* free 1K blocks */
    __int32_t minFree;                  /* min free 1K blocks */
    __int32_t fragsize;                 /* fragment size of aggr */
    __int32_t spares[7];                /* Some spares */
#else
    long blocks;			/* # blks (of size fragsize) in aggr */
    long blocksize;			/* block size of aggregate */
    long totalUsable;			/* total available blocks */
    long realFree;			/* free 1K blocks */
    long minFree;			/* min free 1K blocks */
    long fragsize;			/* fragment size of aggr */
    long spares[7];			/* Some spares */
#endif /* SGIMIPS */
};


struct ag_status {
    struct ag_status_st	ag_st;
    struct ag_status_dy ag_dy;
};


/*
 * Valid aggr types (ag_status.type)
 */
#define	AG_TYPE_UNKNOWN	0
#define	AG_TYPE_UFS	1
#define	AG_TYPE_EPI	2
#define	AG_TYPE_AIX3	3
#define	AG_TYPE_VXFS	4
#define	AG_TYPE_DMEPI	5

#define MAX_AG_TYPE	6		/* size of array of agops structures */

/*
 * Macros to make addition of new file system types easier in the future
 */
#define AG_TYPE_SUPPORTS_EFS(ag)	(((ag) == AG_TYPE_EPI) || ((ag) == AG_TYPE_VXFS) || ((ag) == AG_TYPE_DMEPI))

#define AG_TYPE_TO_STR(ag)	((ag) == AG_TYPE_EPI ? "LFS" : (ag) == AG_TYPE_VXFS\
	? "VXFS" : (ag) == AG_TYPE_DMEPI ? "DMEPI" : "Non-LFS")


/*
 * Aggregate structure
 */
struct aggr {
    struct aggr *a_next;		/* next on linked list */
    struct aggrops *a_aggrOpsp;		/* Ops on aggregates */
    long a_states;			/* Aggregate states */
    long a_refCount;			/* Reference counter */
    osi_dlock_t a_lock;			/* synchs the structure */
    struct ag_status_st a_stat_st;	/* static part of aggregate's status */
    struct vnode *devvp;		/* special vnode for device */
    opaque  a_fsDatap;			/* File-system-dependent data */
    opaque  a_dmDatap;			/* Data Management data */
};

/*
 * XXX By defining all aggr's fields as a_* we can freely move them around
 * without changing the code all over XXX
 */
#define	a_aggrId	a_stat_st.aggrId
#define	a_aggrName	a_stat_st.aggrName
#define	a_devName	a_stat_st.devName
#define	a_device	a_stat_st.device
#define	a_type		a_stat_st.type
#define	a_nVolumes	a_stat_st.nVolumes
#define	a_localMounts	a_stat_st.localMounts

#ifdef notdef	/* Obsolete */
#define	a_blocks	a_status.blocks
#define	a_blocksize	a_status.blocksize
#define	a_totalUsable	a_status.totalUsable
#define	a_realFree	a_status.realFree
#define	a_minFree	a_status.minFree
#define	a_volId		a_status.volId
#endif


/*
 * aggregate states
 */
#define AGGR_EXPORTED	1		/* Exported aggregate */
#define AGGR_DELETED	2		/* Aggregate deleted */


/*
 * Flag(s) to volCreate
 */
#define AGGR_CREATE_ROOT (0x1)

#define AGGR_CREATE_VALID_FLAGS (0x1)

/*
 * Flag(s) to ag_attach
 */
#define AGGR_ATTACH_NOEXPORT (0x1)
#define AGGR_ATTACH_FORCE (0x2)

#define AGGR_ATTACH_VALID_FLAGS (0x3)

struct aggrops {
    int (*ag_hold)(struct aggr *);
    int (*ag_rele)(struct aggr *);
    int (*ag_lock)(struct aggr *, int);
    int (*ag_unlock)(struct aggr *, int);
    int (*ag_stat)(struct aggr *, struct ag_status *);
    int (*ag_volCreate)(struct aggr *, afs_hyper_t *, struct vol_status*, int);
    int (*ag_volInfo)(struct aggr *, int, struct volume *);
    int (*ag_detach)(struct aggr *);
    /* NO AG_ATTACH macro for the following (accessed explicitly) */
    int (*ag_attach)(dev_t, struct vnode *, int, caddr_t, opaque *, int *);
    int (*ag_sync)(struct aggr *, int);
    int (*ag_dmhold)(struct aggr *);
    int (*ag_dmrele)(struct aggr *);
};


#ifdef	KERNEL
#define AG_HOLD(AGP) \
        (*(AGP)->a_aggrOpsp->ag_hold)(AGP)
#define AG_RELE(AGP) \
        (*(AGP)->a_aggrOpsp->ag_rele)(AGP)
#define AG_LOCK(AGP, TYPE) \
        (*(AGP)->a_aggrOpsp->ag_lock)(AGP, TYPE)
#define AG_UNLOCK(AGP, TYPE) \
        (*(AGP)->a_aggrOpsp->ag_unlock)(AGP, TYPE)
#define AG_STAT(AGP, SBUF) \
        (*(AGP)->a_aggrOpsp->ag_stat)(AGP, SBUF)
#define AG_VOLCREATE(AGP, ID, STAT, FLAGS) \
        (*(AGP)->a_aggrOpsp->ag_volCreate)(AGP, ID, STAT, FLAGS)
#define AG_VOLINFO(AGP, INDEX, VOLD) \
        (*(AGP)->a_aggrOpsp->ag_volInfo)(AGP, INDEX, VOLD)
#define AG_DETACH(AGP) \
        (*(AGP)->a_aggrOpsp->ag_detach)(AGP)
#define AG_SYNC(AGP, SYNCTYPE) \
        (*(AGP)->a_aggrOpsp->ag_sync)(AGP, SYNCTYPE)
#define AG_DMHOLD(AGP) \
        (*(AGP)->a_aggrOpsp->ag_dmhold)(AGP)
#define AG_DMRELE(AGP) \
        (*(AGP)->a_aggrOpsp->ag_dmrele)(AGP)
#endif	/* KERNEL */

#define AGGR_SYNC_FILESYS	1
#define AGGR_SYNC_COMMITMETA	2
#define AGGR_SYNC_COMMITALL	3


/*
 * Exported globals/functions of ag_registry.c
 */
extern struct aggr *ag_root;
extern osi_dlock_t ag_lock;
extern long ag_attached;

#ifdef KERNEL
extern struct icl_set *xops_iclSetp;
#endif /* KERNEL */

extern void ag_Init(void);
extern int ag_PutAggr(struct aggr *), ag_RemoveAggr();
extern struct aggr *ag_GetAggrByDev(dev_t);
#ifdef SGIMIPS
extern struct aggr *ag_GetAggr(__int32_t);
#else
extern struct aggr *ag_GetAggr(long);
#endif /* SGIMIPS */
extern int ag_NewAggr(
    char *aggrNamep,
#ifdef SGIMIPS
    __int32_t aggrId,
#else
    long aggrId,
#endif
    char *aggrDevnamep,
    char aggrType,
    struct aggrops *aggrOpsp,
    dev_t dev,
    opaque fsdata,
    struct aggr **aggrpp,
    unsigned flags
);
extern int ag_fsHold(struct aggr *);
extern int ag_fsRele(struct aggr *);
extern int ag_fsLock(struct aggr *, int);
extern int ag_fsUnlock(struct aggr *, int);
extern int ag_fsDMHold(struct aggr *);
extern int ag_fsDMRele(struct aggr *);


extern struct aggrops *agOpvec[];

#ifdef KERNEL
/* Macros to control concurrency between volops and vnops */

/* Broad classification of vnode ops types */
#define VNOP_TYPE_NOOP  5		/* noop */
#define VNOP_TYPE_READONLY 10		/* read only vnode op */
#define VNOP_TYPE_READWRITE  15		/* read write vnode op */
#define VNOP_TYPE_INVALID 30

/* Concurrency levels for concurrency field in vol_stat_st */
#define	VOL_CONCUR_ALLOPS   1
#define	VOL_CONCUR_READONLY 10
#define	VOL_CONCUR_NOOPS    20

/*
 * macro to determine if a vnode op can execute through, even if its fileset
 * is busy
 */
#define VOL_VNOP_COMPAT(concurr, type) (((concurr == VOL_CONCUR_ALLOPS) ||   \
					 (type == VNOP_TYPE_NOOP) || \
					 ((concurr == VOL_CONCUR_READONLY) && \
					 (type == VNOP_TYPE_READONLY))) ? \
					1 : 0)
#endif /* KERNEL */
#endif	/* TRANSARC_AGGR_H */
