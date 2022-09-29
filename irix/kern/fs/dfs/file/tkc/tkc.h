/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: tkc.h,v $
 * Revision 65.1  1997/10/20 19:18:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.535.1  1996/10/02  21:01:18  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:48:08  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkc/RCS/tkc.h,v 65.1 1997/10/20 19:18:02 jdoak Exp $ */

#ifndef _TRANSARC_TKC_H_
#define _TRANSARC_TKC_H_ 1

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/common_def.h>
#include <dcedfs/debug.h>
#include <dcedfs/hs.h>
#include <dcedfs/hs_errs.h>
#include <dcedfs/tkm_tokens.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/icl.h>
#include <dcedfs/tkc_trace.h>


/* Locking hierarchy comment:
 * The locking hierarchy is a little tricky in this package.
 *
 * The first lock to be obtained is typically the tkc_lrulock,
 * which controls access to the vcache LRU queue, and the next/prev
 * pointers stored in the vcache entries themselves.
 *
 * The next lock to be obtained is the vcache lock itself.  This
 * lock controls access to all fields in the vcache entry, except
 * for the lruq (handled by the lru queue lock), the vhash chain
 * pointers (handled by its hash table lock), the refCount field
 * (handled by the tkc_rclock), or the gstates field (also handled
 * by the rclock).  The vstates field is handled by the vcache lock.
 * Note that all fields in the tkc_tokenList structure except for
 * the thash chain are also locked by the vcache lock.  The thash (tokenID
 * hash) chain is locked by the thash chain lock.  The only difference
 * between the vstates and gstates fields are the locks that control
 * access to those fields.  The gstates fields are state flags that need
 * to be modified at about the same time as the vcache entry's refCount.
 *
 * The next lock to be obtained is the thash chain lock.  It controls
 * the hash chains in the tokenList structure, as well as the token ID
 * hash table itself.  This lock appears to be unordered with respect
 * to the vhash chain lock, immediately below, but definitely must
 * be locked before the tkc_rclock.
 *
 * The next lock to be obtained is the vhash chain lock.  It controls
 * the hash chains in the vcache entry, as well as the hash table itself.
 *
 * The next lock to be obtained is the tkc_rclock, which controls the
 * vcache entry's refCount and gstates fields.  It also controls access
 * to the revoke racing structures in tkc_hostops.c.
 *
 * Other miscellaneous locks, like the allocation lock used in the file
 * tkc_alloc.c, are at the bottom, and must be locked last in the locking
 * hierarchy.
 */

/* Random constants */
#define TKC_MAXCONCURRENTCALLS	20	/* really RPCs, not just any calls */

/*
 * Module's constants: hash table sizes.
 */
#define	TKC_VHASHSIZE	256			/* For the Vnode cache table */
#define	TKC_THASHSIZE	256			/* For the token cache table */

/* this structure represents one element in a hash table.  The lock
 * generally locks the hash chain for this hash table.
 */
typedef struct tkc_qqueue {
    struct squeue chain;
    osi_dlock_t lock;
} tkc_qqueue_t;


/* represents one token in use by a glue file entry.  Threaded off of
 * a tkc_vcache entry through the vchain entry.
 */
typedef struct tkc_tokenList {
    struct squeue vchain;	/* other tokens for same file */
    struct squeue thash;	/* token hash chain queue */
    struct tkc_vcache *backp;	/* pointer back to vcache entry */
    unsigned long procid;	/* for Sys V locking */
    int reqID;			/* request ID if pending */
    afs_token_t token;	/* the token itself */
    short states;		/* TKC_L status flags */
} tkc_tokenList_t;


/*
 * Cached vnode.  Indicates states that might prevent tokens from being revoked.
 *
 * If readOpens or writeOpens are > 0, then the corresponding read or write open
 * token can't be revoked.  Generally, a vcache entry may
 * hold references to open tokens after the file has been closed; they will,
 * however, be revocable at these times.  If dataHolds is > 0, the data
 * token(s) can't be revoked because their is a system call actively
 * referencing this data.  The locks field is simply an optimization
 * telling us if there are any Sys V locks held; the vcache entry can't
 * be recycled while there are locks outstanding.
 *
 * Lock tokens are maintained on a strict basis; for each active locked byte
 * range, there's a token, labelled with the relevant process ID, representing
 * that type of lock.  Lock tokens are never revocable, since they're returned
 * as soon as the appropriate lock is released.
 *
 * Other tokens (read and write X status and data) are revocable if there
 * are no active vnode operations requiring their presence.  This is indicated
 * in a vcache entry by dataHolds being > 0, meaning vnode operations are active.
 * A revoke of a vcache entry with dataHolds > 0 just sleeps until the vnode
 * operation completes, since the duration of these operations is determined
 * by the vnode layer, not by any user-controllable operation, like opening
 * or locking a file.
 */
typedef struct tkc_vcache {
    struct squeue lruq;		/* LRU queue */
    struct squeue vhash;	/* vnode hash chain queue */
    struct vnode *vp;		/* vnode on which the tokens are desired */
    osi_dlock_t lock;	/* serialize the refcounts and state manipulation */
    u_long heldTokens;		/* bitmask of available tokens */
    struct squeue tokenList;	/* list of tokens used by this file */
    struct afsFid fid;		/* afs fid corresponding to the file */
    short refCount;		/* struct's ref counter */
    short readOpens;		/* opens for reads */
    short writeOpens;		/* opens for writes */
    short execOpens;		/* opens for execs on supporting systems */
    short dataHolds;		/* number of references to vcache for active RWs;
				 * used to ensure that no RWs are active during a
				 * token revoke.
				 */
    short locks;		/* boolean non-zero if have any lock tokens */
    short tokenAddCounter;	/* bumped on each token add */
    short tokenScanValue;	/* value of tokenAddCounter when last
				 * token duplicate scan done.
				 */
    unsigned char gstates;	/* state bits controlled by rclock */
    unsigned char vstates;	/* state bits controlled by vcache lock */
} tkc_vcache_t;


/*
 * vstates values for tkc_vcache entries.
 */
#define TKC_DHWAITING	0x1	/* someone's waiting for dataHolds to go to 0 */
#define TKC_GRANTWAIT	0x2	/* someone's waiting for a grant to complete */
#define TKC_FLUSHONCLOSE 0x4	/* flush from cache at last close and ref */
#define TKC_VMFLUSHING	0x8	/* vm flush in progress, don't create
				 * new pages
				 */
#define TKC_DELTOKENS	0x10	/* there are deleted entries in the token
				 * list that should be removed when the
				 * reference count hits 0.
				 */
#define TKC_PARTLOCK	0x20	/* have lock token, waiting for lock */
#define TKC_PLWAITING	0x40	/* waiting for TKC_PARTLOCK to go to 0 */

/*
 * gstates values for vcache entries
 */
#define TKC_GDELETING	0x1	/* being deleted, leave refCount alone */
#define TKC_GDISCARD	0x2	/* return all tokens and free object when
				 * when refCount goes to 0.  Used for freeing
			         * references to deleted files.
				 */

/* state values for tokenList entries.  A token is either held or queued, and if
 * it is queued, there may be processes waiting (TKC_LWAITING on) for it to
 * go from the LQUEUED state to the LHELD state.  If it is queued, it is not in
 * the token ID hash table, since we don't know its ID at that point.
 */
#define TKC_LHELD	0x1	/* the token was granted (or in tokset) */
#define TKC_LQUEUED	0x2	/* the request is pending with the tkm */
#define TKC_LWAITING	0x4	/* someone is waiting for a token grant */
#define TKC_LDELETED	0x10	/* entry should be deleted when refcount 0 */
#define TKC_LLOCK	0x20	/* represents a token that is a lock.  These
				 * tokens actually represent instances of real
				 * locks, as well as the token backing the lock.
				 */
#define TKC_LBLOCK	0x40	/* lock token cannot be revoked */

struct tkc_stats {
    u_long	spaceAlloced;
    u_long	revrdwr, revpage;
    u_long	hentries, tentries;
    u_long	hhits, hmisses;
    u_long	busyvcache;
    u_long	rettokenF, gettokensF;
    u_long	extras;
};

typedef struct {
    afs_hyper_t beginRange;
    afs_hyper_t endRange;
} tkc_byteRange_t;

struct tkc_sets {
    struct vnode *vp;
    u_long types;
    tkc_byteRange_t *byteRangep;
    struct tkc_vcache *vcp;
};

/* CAUTIONS -- "sets" is a free variable.
 *
 * NOTE -- Byterangep is never used. */

#define tkc_Set(i, Vp, Types, Byterangep) \
  do { \
    sets[(i)].vp = (Vp); \
    sets[(i)].types = (Types); \
    sets[(i)].byteRangep = (Byterangep); \
    sets[(i)].vcp = NULL; \
  } while(0);

/*
 * Interface calls to TKM (we just call tkm_GeToken & tkm_ReturnToken)
 */
#define	TKM_RETTOKEN(fidP, tok, f)	\
  tkm_ReturnToken((fidP), (tkm_tokenID_t *) &(tok)->tokenID, \
		  (tkm_tokenSet_t *) &(tok)->type, (f))


#define	TKC_VHASH(vp)	(((u_long)(vp) >> 2) & (TKC_VHASHSIZE - 1))
#define	TKC_QTOV(e)	((struct tkc_vcache *)(((char *)(e)) - (((char *) \
			(&(((struct tkc_vcache *)(e))->vhash))) - ((char *)(e)))))
#define	TKC_THASH(tid)	((u_long)AFS_hgetlo(*tid) & (TKC_THASHSIZE - 1))
#define	TKC_QTOT(e)	((struct tkc_tokenList *)(((char *)(e)) - (((char *) \
			(&(((struct tkc_tokenList *)(e))->thash))) - ((char *)(e)))))

extern struct tkc_qqueue tkc_vhash[TKC_VHASHSIZE], tkc_thash[TKC_THASHSIZE];
extern struct icl_set *tkc_iclSetp;
extern struct tkc_stats tkc_stats;
extern struct tkm_race tkc_race;	/* race condition structure */

extern struct tkc_tokenList *tkc_AddToken(
    struct tkc_vcache *,
    afs_token_t *
);
extern int tkc_HaveTokens(
  struct tkc_vcache *vcp,
  u_long tokensNeeded,
  tkc_byteRange_t *byteRangep);
extern int tkc_Release(struct tkc_vcache *);
extern int tkc_QuickRelease(struct tkc_vcache *);
extern int tkc_HandleOpen(struct vnode *vp,
			  int rw,
			  struct tkc_vcache **outp);

extern struct tkc_vcache *tkc_HandleClose(struct vnode *, int);
extern long tkc_HandleDelete(struct vnode *, struct volume *);

extern osi_dlock_t tkc_rclock;
extern long tkc_GetToken(
  struct tkc_vcache *vcp,
  u_long tokenType,
  tkc_byteRange_t *byteRangep,
  long block,
  afs_recordLock_t *rblockerp
);

/* Function exported from internal modules */
/* from tkc_alloc.c */
extern void tkc_InitSpace(void);
extern char *tkc_AllocSpace(void);
extern void tkc_FreeSpace(char *);

/* from tkc_cache.c */
extern void tkc_InitCache(void);
extern int tkc_RecycleVcache(struct tkc_vcache *, int);
extern int tkc_flushvfsp(struct osi_vfs *);
extern int tkc_FlushEntry(struct tkc_vcache *);
extern void tkc_FlushVnode(struct vnode *);
extern struct tkc_vcache *tkc_GetVcache(struct vnode *);

extern int tkc_PruneDuplicateTokens(struct tkc_vcache *);

extern int tkc_CheckTokens(struct tkc_vcache *, long);

/*
 * Functions exported by the gluevn code
 */
#ifdef AFS_SUNOS5_ENV

extern void gluevn_InitThreadFlags(void);
extern void gluevn_SetThreadFlags(long);
extern void gluevn_Done(void);
extern long gluevn_GetThreadFlags(void);

#else

#define gluevn_InitThreadFlags()
#define gluevn_SetThreadFlags(flags)
#define gluevn_Done()
#define gluevn_GetThreadFlags()		0

#endif /* AFS_SUNOS5_ENV */

/*
 * Symbols exported by tkc to other kernel modules.
 */
extern void tkc_Init(void);
extern void tkc_Put(struct tkc_vcache *vcp);
extern struct tkc_vcache *tkc_FindVcache(struct vnode *vp);
extern void tkc_FlushVnode(struct vnode *avp);
extern long tkc_GetTokens(struct tkc_sets *setp, long size);
extern void tkc_PutTokens(struct tkc_sets *setsp, long size);
extern struct tkc_vcache *tkc_Get(
  struct vnode *vp,
  u_long tokenType,
  tkc_byteRange_t *byteRangep);
extern long tkc_GetLocks(
  struct vnode *avp,
  long atokenType,
  tkc_byteRange_t *abyteRangep,
  int ablock,
  afs_recordLock_t *rblockerp);
extern long tkc_PutLocks(
  struct vnode *vp,
  tkc_byteRange_t *byteRangep);

extern struct tkc_vcache * tkc_FindByFID _TAKES((struct afsFid *));

/*
 * Declare the linkage table.  If we are inside the core component, we
 * will initialize it at compile-time; outside the core, we initialize
 * at module init time by copying the core version.
 */
#if defined(_KERNEL) && defined(AFS_SUNOS5_ENV)
extern void *tkc_symbol_linkage[];
#endif /* _KERNEL && AFS_SUNOS5_ENV */

#endif /* _TRANSARC_TKC_H_ */
