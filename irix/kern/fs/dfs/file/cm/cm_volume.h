/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_volume.h,v $
 * Revision 65.2  1998/05/07 20:59:34  lmc
 * Changed variables from unsigned long back to long so that compares
 * against 0 work.
 * timeBad was changed to match its use by cm_conn.c.
 *
 * Revision 65.1  1997/10/20  19:17:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.94.1  1996/10/02  17:13:39  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:06:13  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_volume.h,v 65.2 1998/05/07 20:59:34 lmc Exp $ */

#ifndef	TRANSARC_CM_VOLUME_H
#define	TRANSARC_CM_VOLUME_H

#include <cm_cell.h>
#include <cm_server.h>
#include <cm_rrequest.h>

/*
 * Volume module related macros
 */
#define	VL_NVOLS		64	/* hash table size for volume table */
#define	VL_NFENTRIES		256	/* hash table size for disk vol tbl */
#define	VL_MINCACHEVOLUMES	10	/* min cached volumes in memory */
#define	VL_MAXCACHEVOLUMES	200	/* max cached volumes in memory */
#define	VL_DEFCACHEVOLUMES	50	/* default cache volumes in memory */

/* 
 * state for volume; leave the user-settable ones down low for convenience.
 */
#define VL_SGIDOK		1	/* volume allows set group & user ID */
#define VL_DEVOK		2	/* volume allows device files */
#define VL_RO			0x10	/* volume is readonly */
#define VL_RECHECK		0x20	/* recheck volume info with server */
#define	VL_BACKUP		0x40	/* is this a backup volume? */
#define	VL_LAZYREP		0x80	/* volume is sched- or release-rep
					 * kind of RO */
#define VL_LOCAL		0x100	/* volume is known to be local */
#define VL_RESTORETOKENS	0x200   /* restoring tokens for this vol */
#define VL_HISTORICAL		0x400	/* volume doesn't exist any more */
#define VL_TSRCRASHMOVE		0x800   /* vol moved when network partitioned*/
#define VL_NEEDRPC		0x1000	/* Asked to refresh R/O data with an RPC */

/*
 * The number of servers currently supported by a cm_volume structure.
 */
#define CM_VL_MAX_HOSTS	16

/*
 * State bits to save across volume structure swapout.  These bits are set
 * by user-space applications and we should remember their state so users
 * don't have to set them over and over again.
 */
#define VL_SAVEBITS		(VL_SGIDOK | VL_DEVOK)

/* 
 * One structure per volume, describing where the volume is located and where
 * its mount points are. 
 */
struct cm_volume {
    struct cm_volume *next;		/* Next volume in hash list. */
    struct cm_cell *cellp;		/* this volume's cell */
    struct lock_data lock;		/* the lock for this structure */
    afs_hyper_t volume;			/* This volume's ID number. */
    char *volnamep;			/* This volume's name; 0 if unknown */
#ifdef SGIMIPS
    long timeBad[CM_VL_MAX_HOSTS];	/* per-vol, per-server badness info */
#else
    u_long timeBad[CM_VL_MAX_HOSTS];	/* per-vol, per-server badness info */
#endif
    struct cm_server 
	*serverHost[CM_VL_MAX_HOSTS];	/* servers serving this vol */
    struct cm_server 
	*repHosts[CM_VL_MAX_HOSTS];	/* rep servers for this vol */
    u_long hostGen;      		/* generation count on server arrays */
    long   lastIdx;                     /* last server (index) contacted */
    u_long lastIdxHostGen;              /* server gen for lastIdx */
    u_long lastIdxAddrGen;              /* address gen for lastIdx */
    u_long lastIdxTimeout;              /* re-eval timeout for lastIdx */
    afsFid dotdot;			/* dir to access as .. */
    afsFid mtpoint;			/* The mount point for this volume. */
    afsFid rootFid;			/* The file ID of the fileset's root */
    afs_hyper_t roVol;			/* RO volume id assoc with volume */
    afs_hyper_t backVol;			/* BACKUP vol id assoc with volume */
    afs_hyper_t rwVol;			/* RW volume id for this volume */
    afs_hyper_t latestVVSeen;		/* highest VV seen for this volume */
    afs_token_t hereToken;			/* HERE token for this volume */
    afs_token_t lameHereToken;		/*lame-duck HERE token being revoked */
    struct cm_server *hereServerp;	/* server that granted HERE token */
    long accessTime;			/* last time we used it */
    long vtix;				/* volume table index */
    u_long maxTotalLatency;		/* max delay from change to visible */
    u_long hardMaxTotalLatency;		/* limit for that delay, else error */
    u_long reclaimDally;	/* how long a KA msg keeps files alive */
    u_long repTryInterval;	/*how often HaveToken can say to do RPC probe*/
    u_long timeWhenVVCurrent;		/*time when latestVVSeen was current */
    u_long lastProbeTime;		/* time of last periodic probe */
    u_long lastRepTry;		/* last time HaveToken said to do RPC probe */
    short refCount;			/* reference count for allocation */
    short states;			/* snuck here for alignment reasons */
    short outstandingRPC;		/* number of outstanding RPCs for this
					 * fileset */
    short onlyOneOutstandingRPC; 	/* are there concurrent outstanding
					 * RPCs for this fileset */
    u_short minFldbAuthn, maxFldbAuthn;	/* remember FLDB authn bounds */
    u_short minRespAuthn, maxRespAuthn;	/* track what we learn of authn bounds */
    /*
     * perSrvReason + VOLERR_PERS_LOWEST is a VOLERR_ code
     * saying what's with the fileset.
     */
    unsigned char 
	perSrvReason[CM_VL_MAX_HOSTS];	/* per-vol,server badness info */
};

/* 
 * format of an entry in volume info file 
 */
struct cm_fvolume {
    afs_hyper_t cellId;			/* cell for this entry */
    afs_hyper_t volume;			/* volume for this entry */
    long next;				/* has index */
    long ustates;			/* devok and other states */
    afsFid dotdot;			/* .. value */
    afsFid mtpoint;			/* mt point's fid */
    afsFid rootFid;			/* root's fid */
};

#define	VL_HASH(avol)		((avol)&(VL_NVOLS-1))
#define	VL_FHASH(acell,avol)	((AFS_hgetlo(avol)+AFS_hgetlo(acell)) \
				 & (VL_NFENTRIES-1))
#define VL_MATCH(a,b)		AFS_hsame(a,b)

/* 
 * Exported by cm_volume.c 
 */
extern struct cm_volume *cm_volumes[VL_NVOLS];
extern struct lock_data cm_volumelock;
extern struct cm_volume *cm_freeVolList, *Initialcm_freeVolList;
extern long cm_volCounter, cm_fvTable[VL_NFENTRIES];

extern struct cm_volume *cm_GetVolume _TAKES((afsFid *, struct cm_rrequest *));
extern struct cm_volume *cm_GetVolumeByName _TAKES((char *, afs_hyper_t *, long,
						    struct cm_rrequest *));
extern struct cm_volume *cm_GetVolumeByFid _TAKES((afsFid *, 
						   struct cm_rrequest *,
						   int, int));
extern void cm_PutVolume _TAKES((struct cm_volume *));
extern int cm_CheckVolumeInfo _TAKES((afsFid *, struct cm_rrequest *, 
				      struct cm_volume *, struct cm_cell *, int *));
extern void cm_HoldVolume _TAKES((struct cm_volume *));
extern void cm_StartVolumeRPC _TAKES((struct cm_volume *));
extern void cm_EndVolumeRPC _TAKES((struct cm_volume *));
extern int cm_NeedRPC _TAKES((struct cm_volume *));
extern int cm_SameHereServer _TAKES((struct cm_server *, struct cm_volume *));

#endif	/* TRANSARC_CM_VOLUME_H */
