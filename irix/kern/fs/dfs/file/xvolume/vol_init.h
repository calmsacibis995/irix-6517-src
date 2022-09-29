/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: vol_init.h,v $
 * Revision 65.1  1997/10/20 19:21:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.837.1  1996/10/02  19:04:43  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:59  damon]
 *
 * $EndLog$
 */
/*
 * Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved
 *
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/vol_init.h,v 65.1 1997/10/20 19:21:46 jdoak Exp $
 */

#ifndef	TRANSARC_VOL_INIT_H
#define	TRANSARC_VOL_INIT_H

#include <dcedfs/sysincludes.h>
#include <dcedfs/common_data.h>
#include <dcedfs/queue.h>
#include <dcedfs/volume.h>

/*
 * Volume ops system call
 */

#define	VOLOP_OPEN		4		/* Slots 0-3 are reserved */
#define	VOLOP_SEEK		5
#define	VOLOP_TELL		6
#define	VOLOP_SCAN		7
#define	VOLOP_CLOSE		8
#define	VOLOP_DESTROY		9
#define	VOLOP_GETSTATUS		10
#define	VOLOP_SETSTATUS		11
#define	VOLOP_CREATE		12
#define	VOLOP_READ		13
#define	VOLOP_WRITE		14
#define	VOLOP_TRUNCATE		15
#define	VOLOP_DELETE		16
#define	VOLOP_GETATTR		17
#define	VOLOP_SETATTR		18
#define	VOLOP_GETACL		19
#define	VOLOP_SETACL		20
#define	VOLOP_CLONE		21
#define	VOLOP_RECLONE		22
#define	VOLOP_VGET		23
#define	VOLOP_ROOT		24
#define	VOLOP_ISROOT		25
#define	VOLOP_UNCLONE		26
#define	VOLOP_FCLOSE		27		/* XXXX */
#define	VOLOP_SETVV		28
#define	VOLOP_SWAPVOLIDS	29
#define	VOLOP_COPYACL		30
#define	VOLOP_AGOPEN		31
#define	VOLOP_SYNC		32
#define	VOLOP_PUSHSTATUS	33

#define	VOLOP_PROBE		34
#define	VOLOP_LOCK		35
#define	VOLOP_UNLOCK		36

#define	VOLOP_READDIR		37
#define	VOLOP_APPENDDIR		38

#define	VOLOP_BULKSETSTATUS	39
#define	VOLOP_GETZLC		40
#define	VOLOP_GETNEXTHOLES	41
#define VOLOP_DEPLETE		42
#define VOLOP_READHOLE		43

/*
 * Parameter for VOLOP_SETVV
 */
struct vol_SetVV {
    u_int  vv_Flags;			/* for the vol's ``states'' */
    u_int  vv_Mask;			/* bits to use from Flags */
    afsTimeval vv_Curr;
    afsTimeval vv_PingCurr;
    afsTimeval vv_TknExp;
};

/* Parameter for VOLOP_AGOPEN */
struct vol_AgOpen {
    afs_hyper_t volId;
    unsigned int agId;
    unsigned int accStat;
    unsigned int accErr;
};


#ifdef	KERNEL
struct vol_calldesc {
    struct squeue lruq;			/* free pool for structure */
    int proc;				/* processing doing the call */
    struct vol_handle handle;		/* the handler itself */
    int descId;			/* Unique Identifier */
    int lastCall;			/* Last reference to it */
    int refCount;			/* Reference counter */
    short states;			/* states */
};

/*
 * vol_calldesc's states
 */
#define	VOL_DESCDELETED		1	/* The descriptor has been purged */
#define	VOLCD_GRABBED		2	/* we did a Grab for this volume */


/*
 * General volume descriptor related macros
 */
#define	VOL_NODESC		50	/* Size of vol descriptor table */
#define	VOL_DESCALLOC	6	/* # of preallocated descs */
#define	VOL_DESCTIMEOUT	(5*60)	/* GC entry after 5 mins of inactivity */


/*
 * Globals/functions exported by vol_desc.c
 */
extern struct vol_calldesc *vol_descs[];
extern struct vol_calldesc *vol_freeCDList;
extern osi_dlock_t vol_desclock;

/* from vol_desc.c */
extern int vol_GCDesc (int);
extern void vol_PutDesc (struct vol_calldesc *);
extern void vol_SetDeleted (struct vol_calldesc *);
extern void vol_InitDescMod (void);
extern void vol_PurgeDesc (struct vol_calldesc *);
extern int vol_GetDesc (struct vol_calldesc **);
extern int vol_FindDesc (int, struct vol_calldesc **);

/* from vol_attvfs.c */
extern void vol_InitAttVfsMod (void);
extern int vol_FindVfs (struct osi_vfs *, struct volume **);
extern void vol_AddVfs (struct osi_vfs *, struct volume *);
extern void vol_RemoveVfs (struct volume *);

/* from vol_misc.c */
extern void vol_Init (void);
extern int vol_Attach (afs_hyper_t *, struct vol_status *, struct aggr *,
		       struct volumeops	*);
extern int vol_Detach (struct volume *, int);
extern int vol_VolInactive (struct volume *);
extern void vol_SwapIdentities (struct volume *, struct volume *);
extern int vol_SwapIDs (struct volume *, struct volume *);
extern int vol_fsStartBusyOp (struct volume *, int, opaque *);
extern int vol_fsStartVnodeOp (struct volume *, int, int, opaque *);
extern void vol_fsStartInactiveVnodeOp(struct volume *, struct vnode *, opaque *);
extern int vol_fsEndVnodeOp (struct volume *, opaque *);
extern void vol_SetMoveTimeoutTrigger (struct volume *);

extern int vol_open(
    afs_hyper_t *, u_int, int, int, int *, int *, int);
extern void vol_close(struct volume *, vol_vp_elem_p_t *);
extern void vol_ProcessDeferredReles(vol_vp_elem_p_t);
extern int vol_fsHold(struct volume *);
extern int vol_fsRele(struct volume *);
extern int vol_fsLock(struct volume *, int);
extern int vol_fsUnlock(struct volume *, int);
extern int vol_fsDMWait(struct volume *, opaque *);
extern int vol_fsDMFree(struct volume *, opaque *);

/*
 * The following kernel symbols are exported outside the DFS core.
 */
#endif /* KERNEL */

extern void vol_AddVfs(struct osi_vfs *, struct volume *);
extern void vol_RemoveVfs(struct volume *);
extern int vol_Attach(
    afs_hyper_t *,
    struct vol_status *,
    struct aggr *,
    struct volumeops *
);

extern void vol_Init(void);
extern int vol_GCDesc(int lockit);
extern void vol_RCZero(struct volume *volp);
extern int vol_VolInactive(struct volume *volp);

/*
 * Number of exported symbols
 */
#define VOL_SYMBOL_LINKS	7

/*
 * Declare the linkage table.  If we are inside the core component, we
 * will initialize it at compile-time; outside the core, we initialize
 * at module init time by copying the core version.
 */
#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
extern void *vol_symbol_linkage[VOL_SYMBOL_LINKS];
#endif /* _KERNEL && AFS_SUNOS5_ENV */

#endif	/* TRANSARC_VOL_INIT_H */
