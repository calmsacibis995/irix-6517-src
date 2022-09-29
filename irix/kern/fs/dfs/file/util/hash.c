/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: hash.c,v 65.3 1998/03/23 16:32:30 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: hash.c,v $
 * Revision 65.3  1998/03/23 16:32:30  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:59:41  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:19:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.323.1  1996/10/02  21:13:11  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:01  damon]
 *
 * Revision 1.1.318.2  1994/06/09  14:24:49  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:36:54  annie]
 * 
 * Revision 1.1.318.1  1994/02/04  20:35:34  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:20:18  devsrc]
 * 
 * Revision 1.1.316.1  1993/12/07  17:37:46  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  18:08:32  jaffe]
 * 
 * $EndLog$
*/
/* This is a general purpose hash table package.  A hash table typically
 * declared as a static variable by the user of the package.  It must be
 * initialized and a key comparison method must be specified.  Entries are
 * structures which are passed by pointer (coerced to (dfsh_hashEntry_t)).
 * Each entry is required to include a thread pointer (sizeof
 * (dfsh_hashEntry_t)) at a fixed offset.  A function that takes a pointer to
 * an entry an returned a hash value (unsigned long) is also required. */

/* Copyright (C) 1994, 1989 Transarc Corporation - All rights reserved */

/* For an example of the package's usage see ktc/ktc.c */

/* Moved from afs/ktc package to DEcorum. 900321 -ota */
/* Moved from de/episode/anode to de/episode/tools.  910515 -wam */
/* Copied from src/file/episode/tools to src/file/util 930330 -jdp */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/stds.h>

#include "hash.h"

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/util/RCS/hash.c,v 65.3 1998/03/23 16:32:30 gwehrman Exp $")

#define UNSPECIFIED 0
#define FIXED 1
#define VARYING 2
#define STRING 3
#define FUNCTION 4

#if (DFSH_DEBUG>5)
#define osi_DebugMsg(a) printf a
#define HT_DEBUG(name,ht) printf ("HT in %s @%x %x %d %d\n", name, ht, (ht)->hashFunction, (ht)->entries, (ht)->length)
#else
#define osi_DebugMsg(a)
#define HT_DEBUG(in,ht)
#endif

/* Very cheap locking.  0 is unlocked, positive is number of lockers, -1 is
 * write locked.  Note that panic is a macro that turns into abort, which is a
 * void returning function.  This causes problems with the ?: operator on some
 * compilers. */

#if defined(DFSH_HASH_LOCK)
#define InitLock(lockP) (*(lockP) = 0)
#define HeldLockP(lockP) (*(lockP) != 0)

#define ReadLock(lockP) \
    ( (*(lockP) < 0) ? (panic ("write lock held"),0) : (*(lockP))++ )

#define ReadUnlock(lockP) \
    ( (*(lockP) <= 0) ? (panic ("no read lock held"),0) : (*(lockP))-- )

#define WriteLock(lockP) \
    ( (*(lockP) != 0) ? (panic ("lock held"),0) : (*(lockP))-- )

#define WriteUnlock(lockP) \
    ( (*(lockP) != -1) ? (panic ("no write lock held"),0) : (*(lockP))++ )

/* convert a write lock to a read lock */
#define DowngradeLock(lockP) \
    ( (*(lockP) != -1) ? (panic ("no write lock held"),0) : ((*(lockP)) = 1) )
#else
#define InitLock(lockP)
#define HeldLockP(lockP)
#define ReadLock(lockP)
#define ReadUnlock(lockP)
#define WriteLock(lockP)
#define WriteUnlock(lockP)
#define DowngradeLock(lockP)
#endif /* DFSH_HASH_LOCK */

/* dfsh_HashInit - initializes a hash table.  It takes a pointer to the hash
 * table, a function that returns an entry's hash value, and the offset (in
 * bytes) in the entry of the hash thread pointer. */

/* EXPORT */
int dfsh_HashInit (
  IN struct dfsh_hashTable *ht,
  IN unsigned long (*hf)(),
  IN int to)
{
    HT_DEBUG("dfsh_HashInit in",ht);
    if (!ht || !hf) return -1;
    bzero ((char *)ht, sizeof(*ht));
    ht->hashFunction = hf;
    ht->threadOffset = to;
    ht->keyType = UNSPECIFIED;
    InitLock (&ht->lock);
    HT_DEBUG("dfsh_HashInit out",ht);
    return 0;
}

/* dfsh_HashInitKey* - this is a family of functions that initializes the entry
 * comparison function for a hash table.  Each takes a pointer to the hash
 * table and one or more arguments which describe the comparison to make.  This
 * allows the lookup function to find the matching entry. */

/* dfsh_HashInitKeyFixed - the key is described by offset in the entry and
 * length in bytes.  This is best used if there are one or more fixed size
 * fields, such as pointers or integers, to be checked. */

/* EXPORT */
long dfsh_HashInitKeyFixed (
  struct dfsh_hashTable *ht,
  int off,
  int len)
{
    ht->keyType = FIXED;
    ht->cmpData.fixed.off = off;
    ht->cmpData.fixed.len = len;
    return 0;
}

/* dfsh_HashInitKeyVarying - the key is described as an offset to an int which
 * provides the length of an immediately following sequence of bytes. This is
 * used for counted byte strings which are not null terminated.  The comparison
 * string is bcmp. */

/* EXPORT */
long dfsh_HashInitKeyVarying (
  struct dfsh_hashTable *ht,
  int off)
{
    ht->keyType = VARYING;
    ht->cmpData.varying = off;
    return 0;
}

/* dfsh_HashInitKeyString - the key is a string whose location is given by its
 * offset in the entry.  This is for regular null terminated C strings.  The
 * comparison function used is strcmp. */

/* EXPORT */
long dfsh_HashInitKeyString (
  struct dfsh_hashTable *ht,
  int off)
{
    ht->keyType = STRING;
    ht->cmpData.string = off;
    return 0;
}

/* dfsh_HashInitKeyFunction - the key cannot be specified by any of the above
 * methods so this provides a boolean function which takes two entries and
 * compares them for equality. */

/* EXPORT */
long dfsh_HashInitKeyFunction (
  struct dfsh_hashTable *ht,
  int (*fun)())
{
    ht->keyType = FUNCTION;
    ht->cmpData.fun = fun;
    return 0;
}

#define INITIAL_TABLE_SIZE 17

static int primes[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
	53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 0};

static int GetBiggerTable (int old)
{
    int  new;
    int *pl;

    new = old * 3;			/* start about 1/3 full */
    for (new |= 1; ; new += 2) {	/* force it odd, look for primes */
	for (pl = primes; *pl; pl++) {
	    int rem = new / (*pl);
	    if (rem * (*pl) == new) goto not_prime;
	    if (rem <= (*pl)) break;
	}
	return new;
      not_prime:
	;
    }
}

static int GetSmallerTable (int len, int ent)
{
    int  new;
    int *pl;

    if ((ent == 0) && (len < 2*INITIAL_TABLE_SIZE)) return len;
    new = ent * 2;			/* start about half full */
    /* force it odd, look for primes */
    for (new |= 1; new > INITIAL_TABLE_SIZE; new -= 2) {
	for (pl = primes; *pl; pl++) {
	    int rem = new / (*pl);
	    if (rem * (*pl) == new) goto not_prime;
	    if (rem <= (*pl)) break;
	}
	return new;
      not_prime:
	;
    }
    return INITIAL_TABLE_SIZE;
}

static void
Rehash(struct dfsh_hashTable *ht, dfsh_hashEntry_t *otable, int olength)
{
    HT_DEBUG("rehash",ht);
    if (ht->length == olength) {
	return;
    } else if (otable == 0) {
	afsl_Assert(ht->length == 0);
	return;
    } else if (ht->length == 0) {
	afsl_Assert(ht->entries == 0);
	ht->emptyCnt++;			/* hash table empty; being discarded */
	osi_DebugMsg (("freeing %d word hash table\n", olength));
	ht->table = 0;
    } else {
	int i;

	ht->rehashCnt++;
	if (ht->length < olength) {
	    osi_DebugMsg (("Shrinking %d word hash table to %d words\n", olength, ht->length));
	} else {
	    osi_DebugMsg (("Growing %d word hash table to %d words\n", olength, ht->length));
	}
	ht->table = (dfsh_hashEntry_t *)
	    osi_Alloc_r(ht->length * sizeof (dfsh_hashEntry_t));
	for (i = 0; i < ht->length; i++)
	     ht->table[i] = 0;
	ht->entries = 0;
	for (i = 0; i < olength; i++) {
	    dfsh_hashEntry_t e;
	    dfsh_hashEntry_t next;

	    for (e = otable[i]; e; e = next) {
		next = *(dfsh_hashEntry_t *)(e + ht->threadOffset);
		dfsh_HashIn_r(ht, e);
	    }
	}
	ht->hashInCnt -= olength;
    }
    osi_Free_r((char *)otable, olength*sizeof(dfsh_hashEntry_t));
}

/*
 * dfsh_HashIn -- adds an entry to a hash table.  It takes a pointer to the
 *     hash table and a pointer to the entry.  A value of -1 is returned if the
 *     entry is already in the hash table.
 *
 * dfsh_HashIn_r is the preemption-safe version.
 */
int
dfsh_HashIn(struct dfsh_hashTable *ht, opaque ahe)
{
    int code;
    osi_MakePreemptionRight();
    code = dfsh_HashIn_r(ht, ahe);
    osi_UnmakePreemptionRight();
    return code;
}

int
dfsh_HashIn_r(struct dfsh_hashTable *ht, opaque ahe)
{
    unsigned long hash;
    int hi;
    dfsh_hashEntry_t he = ahe;
    dfsh_hashEntry_t e;
    dfsh_hashEntry_t *next;

#if (DFSH_DEBUG>10)
    HT_DEBUG("HashIn",ht);
#endif
    ht->hashInCnt++;
    if (ht->length == 0) {
	int i;
	ht->length = INITIAL_TABLE_SIZE;
	ht->table = (dfsh_hashEntry_t *)
	    osi_Alloc_r(ht->length * sizeof (dfsh_hashEntry_t));
	for (i = 0; i < ht->length; i++)
	    ht->table[i] = 0;
    } else if (ht->entries > 2 * ht->length) {
	dfsh_hashEntry_t *otable = ht->table;
	int olength = ht->length;

	ht->length = GetBiggerTable (ht->entries);
	Rehash(ht, otable, olength);
    }

    hash = (*ht->hashFunction)(he);
    hi = hash % ht->length;

#if (DFSH_DEBUG>5)
    /* check for entry already on list, to avoid loops */
    for (e = ht->table[hi]; e;
	 e = *(dfsh_hashEntry_t *)(e + ht->threadOffset))
	if (e == he) return -1;
#endif					/* DFSH_DEBUG */

    next = (dfsh_hashEntry_t *)(he + ht->threadOffset);
    *next = ht->table[hi];
    ht->table[hi] = he;
    ht->entries++;
    return 0;
}

/*
 * dfsh_HashOut -- removes an entry from a hash table.  It takes a pointer to
 *     the hash table and a pointer to the entry.  A value of -1 is returned if
 *     the entry was not in the hash table.
 *
 * dfsh_HashOut_r is the preemption-safe version.
 */
int
dfsh_HashOut(struct dfsh_hashTable *ht, opaque ahe)
{
    int code;
    osi_MakePreemptionRight();
    code = dfsh_HashOut_r(ht, ahe);
    osi_UnmakePreemptionRight();
    return code;
}

int
dfsh_HashOut_r(struct dfsh_hashTable *ht, opaque ahe)
{
    u_long hash;
    int hi, result = 0;
    dfsh_hashEntry_t he = ahe;
    dfsh_hashEntry_t e;
    dfsh_hashEntry_t *next;

    HT_DEBUG("dfsh_HashOut",ht);
    afsl_Assert(ht != NULL && he != NULL);
    WriteLock (&ht->lock);
    afsl_Assert(ht->table != NULL);

    ht->hashOutCnt++;

    hash = (*ht->hashFunction)(he);
    hi = hash % ht->length;

    /* Find entry on list */
    next = (dfsh_hashEntry_t *)(he + ht->threadOffset);
    if (he == ht->table[hi])
	ht->table[hi] = *next;
    else {
	dfsh_hashEntry_t last_e = 0;
	for (e = ht->table[hi]; e != he && e != NULL;
	    e = *(dfsh_hashEntry_t *)(e + ht->threadOffset)) {
	    last_e = e;
	}
	if (e == NULL) {
	    result = -1;
	    goto done;
	}
	*(dfsh_hashEntry_t *)(last_e + ht->threadOffset) = *next;
    }

    ht->entries--;
    if (ht->entries * 8 < ht->length) {
	dfsh_hashEntry_t *otable = ht->table;
	int olength = ht->length;

	ht->length = GetSmallerTable (ht->length, ht->entries);
	Rehash(ht, otable, olength);
    }
done:
    WriteUnlock (&ht->lock);
    return result;
}

/* dfsh_HashLookup - locates an entry in a hash table and returns a pointer to
 * it.  It takes a hash table pointer and a pointer to a dummy entry which
 * serves as a comparison key.  The pointer is returned to the first entry that
 * matches the key entry.  If no entry is found a zero pointer is returned. */

/* EXPORT */
opaque dfsh_HashLookup (
  struct dfsh_hashTable *ht,
  opaque akey)
{
    unsigned long hash;
    int hi;
    dfsh_hashEntry_t key = akey;
    dfsh_hashEntry_t e;
    int off;				/* offset */
    int len;				/* length of compare */

    HT_DEBUG("dfsh_HashLookup",ht);
    if (!ht || !key) panic ("bad call to HashLookup");
    ReadLock (&ht->lock);
    e = 0;
    if (ht->length == 0) goto done;

    hash = (*ht->hashFunction)(key);
    hi = hash % ht->length;

    switch (ht->keyType) {
      case FIXED:
	off = ht->cmpData.fixed.off;
	len = ht->cmpData.fixed.len;
	for (e = ht->table[hi]; e; e = *(dfsh_hashEntry_t *)(e+ht->threadOffset))
	    if (bcmp (key+off, e+off, len) == 0) goto done;
	break;
      case VARYING:
	off = ht->cmpData.varying;
	len = *(int *)(key+off);
	off += sizeof(int);
	for (e = ht->table[hi]; e; e = *(dfsh_hashEntry_t *)(e+ht->threadOffset))
	    if ((len == *(int *)(e + off-sizeof(int))) &&
		(bcmp (key+off, e+off, len) == 0)) goto done;
	break;
      case STRING:
	off = ht->cmpData.string;
	for (e = ht->table[hi]; e; e = *(dfsh_hashEntry_t *)(e+ht->threadOffset))
	    if (strcmp (key+off, e+off) == 0) goto done;
	break;
      case FUNCTION:
	for (e = ht->table[hi]; e; e = *(dfsh_hashEntry_t *)(e+ht->threadOffset))
	    if ((*ht->cmpData.fun) (key, e)) goto done;
	break;
      default: panic ("no key type");
    }
  done:
    ReadUnlock (&ht->lock);
    return e;
}

/* This procedure can be used to enumerate the contents of a hash table.  On
 * the first call he is zero and the first hash entry is returned.  On
 * subsequent calls the entry returned by the previous call is provided and the
 * next entry is returned.
 *
 * NOTE that the hash table must not be modified during this sequence as
 * modifications may result in the hash table being reorganized.  The can
 * result in duplicate and omitted entries. */

/* EXPORT */
opaque dfsh_HashNext (
  struct dfsh_hashTable *ht,
  opaque ahe)
{
    dfsh_hashEntry_t he = ahe;
    dfsh_hashEntry_t e = 0;
    int hi;
    dfsh_hashEntry_t next;

#if (DFSH_DEBUG>10)
    HT_DEBUG("dfsh_HashNext",ht);
#endif
    if (ht == 0) panic ("bad call to HashNext");
    ReadLock (&ht->lock);
    if (ht->length == 0) goto done;	/* hash table empty */
    if (he == 0) {
	hi = -1;			/* start at beginning of hash table */
	next = 0;
    } else {
	unsigned long hash = (*ht->hashFunction)(he);
	hi = hash % ht->length;
	next = *(dfsh_hashEntry_t *)(he + ht->threadOffset);
    }
    if (next == 0) {			/* find next non-empty bucket */
	for (hi++; hi < ht->length; hi++) {
	    e = ht->table[hi];
	    if (e) goto done;
	}
	e = 0;
	goto done;			/* no more entries */
    }
    /* Make sure next if really in this bucket, to prevent indirecting through
     * garbage parameter. */
    for (e = ht->table[hi]; e; e = *(dfsh_hashEntry_t *)(e + ht->threadOffset))
	if (e == next) goto done;
    /* input parameter was garbage, panic? */
  done:
    ReadUnlock (&ht->lock);
    return e;
}
