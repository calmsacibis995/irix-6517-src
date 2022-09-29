/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: hash.h,v $
 * Revision 65.1  1997/10/20 19:19:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  21:13:13  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:02  damon]
 *
 * $EndLog$
 */
/*
*/
/* A general hash table package */

/* Copyright (C) 1995, 1989 Transarc Corporation - All rights reserved */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/util/RCS/hash.h,v 65.1 1997/10/20 19:19:23 jdoak Exp $ */

/* Moved from afs/ktc package to DEcorum. 900321 -ota */
/* Moved from de/episode/anode to de/episode/tools 910515 -wam */
/* Copied from src/file/episode/tools to src/file/util 930330 -jdp */

#ifndef TRANSARC_DFS_HASH_H
#define TRANSARC_DFS_HASH_H

#include <dcedfs/stds.h>

typedef char *dfsh_hashEntry_t;

struct dfsh_hashTable {
    unsigned long (*hashFunction)();	/* Computes hash for an entry */
    int threadOffset;			/* Byte offset of hash thread */
    /* various counters */
    int emptyCnt;			/* times hash table becomes empty */
    int hashInCnt;			/* hash in operations */
    int hashOutCnt;			/* hash out operations */
    int rehashCnt;			/* times hash table was reallocated */
    int entries;			/* number of entries in hash table */
    int length;				/* number of hash table slots */
    int lock;				/* lock counter? */
    int keyType;			/* type of comparison */
    union {
	int varying;			/* offset of length and key data */
	int string;			/* offset of string */
	struct {
	    int off;			/* offset of key */
	    int len;			/* length of key */
	} fixed;			/* fixed size key */
	int (*fun)();			/* key comparison function */
    } cmpData;
    dfsh_hashEntry_t *table;		/* pointer to actual table */
};

typedef struct dfsh_hashTable dfsh_hashTable_t;

int dfsh_HashInit _TAKES((
   struct dfsh_hashTable *ht,
   unsigned long (*hf)(),
   int to));

long dfsh_HashInitKeyFixed _TAKES((
   struct dfsh_hashTable *ht,
   int off,
   int len));

long dfsh_HashInitKeyVarying _TAKES((
   struct dfsh_hashTable *ht,
   int off));

long dfsh_HashInitKeyString _TAKES((
   struct dfsh_hashTable *ht,
   int off));

long dfsh_HashInitKeyFunction _TAKES((
   struct dfsh_hashTable *ht,
   int (*fun)()));

int dfsh_HashIn_r(dfsh_hashTable_t *ht, opaque he);

int dfsh_HashIn _TAKES((
   struct dfsh_hashTable *ht,
   opaque he));

#define DFSH_HASHIN_R(ht, he) \
MACRO_BEGIN \
    if ((ht)->entries >= (ht)->length * 2) { \
	/* expensive case */ \
	(void) dfsh_HashIn_r(ht, he); \
    } else { \
	int hi = (*(ht)->hashFunction)((opaque)he) % (ht)->length; \
	/* cheap, common case */ \
	*(dfsh_hashEntry_t *)(((char *)(he)) + (ht)->threadOffset) = \
	    (ht)->table[hi]; \
	(ht)->table[hi] = (opaque)(he); \
	(ht)->entries++; \
	(ht)->hashInCnt++; \
    } \
MACRO_END

int dfsh_HashOut_r(dfsh_hashTable_t *ht, opaque he);

int dfsh_HashOut _TAKES((
   struct dfsh_hashTable *ht,
   opaque he));

opaque dfsh_HashLookup _TAKES((
   struct dfsh_hashTable *ht,
   opaque key));

/* DFSH_LOOKUP -- is an optimized macro to searching a hash table for a
 *     matching entry.  To avoid extra function calls the match is specified to
 *     the macro as an expression and the loop in coded inline evaluating the
 *     expression on each entry on the correct hash chain.  The working
 *     variable "he", is also used to return the result, or NULL if none is
 *     found. */

#define DFSH_LOOKUP(type, ht, he, hash, comp) \
do { \
    if ((ht)->length) { \
	unsigned hi = ((unsigned)(hash)) % (ht)->length; \
	    for (he = (type)(ht)->table[hi]; he; \
		 he = *(type *)((char *)(he)+(ht)->threadOffset)) \
		     if (comp) break; \
    } else { \
	he = (type)0; \
    } \
} while(0)

/* Normally this routine assumes that the hash table is static while being
 * iterated.  However, the vnode hash table cannot make that assumption.  It
 * uses the rehashCnt to detect reorganizations and restarts the iteration when
 * one occurs. */

opaque dfsh_HashNext _TAKES((
   struct dfsh_hashTable *ht,
   opaque he));

/*
#if (sizeof(long) != sizeof(char *))
#error We require size of long and pointers to be equal
#endif
 */

#endif /* !TRANSARC_DFS_HASH_H */
