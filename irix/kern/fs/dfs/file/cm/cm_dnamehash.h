/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_dnamehash.h,v $
 * Revision 65.2  1998/03/02 22:27:11  bdr
 * Initial cache manager changes.
 *
 * Revision 65.1  1997/10/20  19:17:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.13.1  1996/10/02  17:07:45  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:49  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1990, 1994 Transarc Corporation
 *      All rights reserved.
 */


#ifndef	TRANSARC_CM_DNAMEHASH_H
#define	TRANSARC_CM_DNAMEHASH_H

#define NH_HASHSIZE	128	/* size of hash table */
#define	NH_NAMELEN	32	/* max name size to hash */

/*
 * directory name cached entry
 */
struct nh_cache {
  struct nh_cache *next, *prev;	/* hash chain */
  struct squeue lruq;		/* LRU chain */
  struct cm_scache *dvp;	/* vnode for parent of name */
  afsFid fid;		        /* name's unique Fid */
  short states;
  unsigned char namelen;			/* length of name */
  char name[NH_NAMELEN];	/* name */
};

/* flags for states entry */

#define NH_INSTALLEDBY_BULKSTAT 0x1
#define NH_SEEN			0x2


/*
 * directory name hash stats
 */
struct nh_stats {
  int normal;			/* # of normal entries in the cache */
  int missing;			/* # of "missing" entries in the cache */
  int hits;			/* # of name cache hits */
  int misses;			/* # of name cache misses */
  int enter;			/* # of nh_enter calls */
  int removes;			/* # of nh_delete calls */
  int alreadycached;		/* # of nh_enters on cached names */
  int long_look;		/* # of nh_dolookups on long names */
  int long_enter;		/* # of nh_enters on long names */
  int bulkstat_seen;		/* # entered by bulkstat; lookup hit */
  int bulkstat_notseen;		/* # entered by bulkstat; no lookup */
  int bulkstat_entered;		/* # of nh_enters from bulkstat */
};


#define	NH_INSHASH(nhp, hash)	QAdd(hash, nhp)
#define	NH_RMHASH(nhp)		QRemove(nhp)
#define	NH_NULLHASH(nhp)	QInit(nhp)

/*
 * Directory name hash function; at least it's cheap.
 */
#ifdef SGIMIPS
#define	NH_HASH(name, namelen, vp)	\
                        (((name)[0] + (name)[(namelen)-1] + (namelen) + \
			(unsigned long)(vp)) & (NH_HASHSIZE-1))
#else  /* SGIMIPS */
#define	NH_HASH(name, namelen, vp)	\
                        (((name)[0] + (name)[(namelen)-1] + (namelen) + \
			(int)(vp)) & (NH_HASHSIZE-1))
#endif /* SGIMIPS */

#define	NH_QTONH(e)	((struct nh_cache *)(((char *) (e)) - (((char *) \
			(&(((struct nh_cache *)(e))->lruq))) - ((char *)(e)))))


/*
 * Exported by cm_dnamehash.c
 */
extern struct squeue nh_cache[NH_HASHSIZE];
extern struct nh_stats nh_stats;

extern void nh_init _TAKES((int));
extern int nh_locallookup _TAKES((struct cm_scache *, char *, afsFid *));
extern int nh_dolookup _TAKES((struct cm_scache *, char *, afsFid *,
				struct cm_scache **, struct cm_rrequest *));
extern void nh_enter _TAKES((struct cm_scache *, char *, afsFid *, int));
extern int nh_lookup _TAKES((struct cm_scache *, char *, afsFid *));
extern void nh_delete _TAKES((struct cm_scache *, char *));
extern void nh_delete_dvp _TAKES((struct cm_scache *));

#endif	/* TRANSARC_CM_DNAMEHASH_H */
