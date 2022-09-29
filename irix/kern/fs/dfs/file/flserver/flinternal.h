/* Copyright (C) 1996 Transarc Corporation - All rights reserved. */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/flserver/RCS/flinternal.h,v 65.1 1997/10/20 19:20:07 jdoak Exp $ */

/* declarations internal to the flserver */

#ifndef _TRANSARC_FLSERVER_FLINTERNAL_H_
#define  _TRANSARC_FLSERVER_FLINTERNAL_H_

#include <dcedfs/hyper.h>

extern struct vldstats dynamic_statistics;

#define	VLDBVERSION		3	/* Vldb database version */
#define	HASHSIZE		8191	/* Must be prime */
#define	NULLO			0
#define	VLDBALLOCCOUNT		40

/* Current upper limits limits on certain entries; increase with care! */
#define	MAXSERVERFLAG	0x80
#define	MAXLOCKTIME	(8*60*60) /* time locks out after 8 hours */
#define	VLDB_MAXSERVERS	10	/* Max cooperating vldb servers */
#define	SITESINBLOCK	64	/* allocation unit */

#define SITEBLOCKTAG 0xaedceacd
/*
 * In blocks of type ``siteBlock'':
 * Tag is kept in network order
 * NextPtr, AllocHere, and UsedHere are kept in host order
 * Sites themselves are kept in network order
 */
struct siteBlock {
    u_int32  Tag;		/* SITEBLOCKTAG, not any reclaimDally field */
    u_int32  NextPtr;		/* pointer to next block */
    u_int32  AllocHere;		/* how many siteDesc guys are allocated here */
    u_int32  UsedHere;		/* how many siteDesc guys are in use here */
    siteDesc Sites[SITESINBLOCK];	/* the block of siteDescs itself */
};

#ifndef KERNEL
/*
 * don't need this def'n in kernel, and vital_vlheader isn't provided from
 * afsvlint.xg's set of definitions.
 */
/* vital_vlheader -- an on-disk image of struct from idl generated include. */
struct disk_vlheader {
    u_int32    vldbversion;		/* vldb version number--must be 1st */
    u_int32    headersize;		/* total bytes in header */
    u_int32    freePtr;			/* first (if any) free enry in freelist */
    u_int32    eofPtr;			/* first free byte in file */
    u_int32    allocs;			/* total calls to AllocBlock */
    u_int32    frees;			/* total calls to FreeBlock */
    dfsh_diskHyper_t maxVolumeId;	/* Global volume id holder */
    u_int32    sitesPtr;		/* Pointer to first Sites structure */
    u_int32    numSites;		/* Total # sites allocated in vldb */
    u_int32    totalEntries[MAXVOLTYPES]; /* Entries by voltype in vldb */
    dfsh_diskHyper_t theCellId;		/* hyper CellID for all */
    u_int32    spare0;
    u_int32    spare1;
    u_int32    spare2;
    u_int32    spare3;
    u_int32    spare4;
    u_int32    spare5;
    u_int32    spare6;
    u_int32    spare7;
    u_int32    spare8;
    u_int32    spare9;
    u_int32    spare10;
    u_int32    spare11;
    u_int32    spare12;
};

/*
 * Header struct holding stats, internal pointers and the hash tables
 */
struct vlheader {
    struct disk_vlheader  vital_header;	/* all small critical stuff in here */
    u_int32    VolnameHash[HASHSIZE];	/* hash table for vol names */
    u_int32    VolidHash[MAXTYPES][HASHSIZE]; /* hash table for vol ids */
};
#endif /* KERNEL */

/* Vlentry's flags state */
#define	VLFREE		1		/* If in free list */
/* #define	VLDELETED	2 */		/* Entry is soft deleted */
#define	VLLOCKED	4		/* Advisory lock on entry */

/* Internal representation of vldbentry; trying to save any bytes */
struct vlentry {
    u_int32    reclaimDally;
    dfsh_diskHyper_t volumeId[MAXTYPES]; /* Corresponding volume of each type */
    u_int32    flags;			/* General flags */
    u_int32    LockTimestamp;		/* lock time stamp */
    dfsh_diskHyper_t cloneId;		/* used during cloning */
    u_int32    spares5;
    u_int32    nextIdHash[MAXTYPES];	/* Next id hash table pointer
					 * (or freelist ->[0]) */
    u_int32    nextNameHash;		/* Next name hash table pointer */
    u_int32    maxTotalLatency;
    u_int32    hardMaxTotalLatency;
    u_int32    minimumPounceDally;
    u_int32    defaultMaxReplicaLatency;
    u_int32  serverSite[MAXNSERVERS];	/* Server site ptr for each server
					 * that holds volume */
    u_int32  serverPartition[MAXNSERVERS]; /* Server Partition number */
    u_int32  sitemaxReplicaLatency[MAXNSERVERS]; /* Per-site max latency */
    u_int32    spares1;			/* int32 spares */
    u_int32    spares2;			/* int32 spares */
    char    name[MAXFTNAMELEN];		/* Volume name */
    char    NameOfLocker[MAXLOCKNAMELEN];
    u_char  serverFlags[MAXNSERVERS];	/* Server flags */
    u_char  volumeTIx;			/* Vol. type (RWVOL, ROVOL, BACKVOL) */
    u_char  spares4;
    char    spares3[5];			/* for 32-bit alignment */
/* ALLOCATE the first four bytes for range limits on authentication types */
#define	vlent_min_lcl_authn	spares3[0]
#define	vlent_max_lcl_authn	spares3[1]
#define	vlent_min_rmt_authn	spares3[2]
#define	vlent_max_rmt_authn	spares3[3]
};

typedef struct vlheader vlheader_t;
typedef struct vlentry vlentry_t;

#endif /* _TRANSARC_FLSERVER_FLINTERNAL_H_ */
