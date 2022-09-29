/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: volreg.c,v 65.4 1998/03/23 16:47:02 gwehrman Exp $";
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
 * (C) Copyright 1996, 1990 Transarc Corporation
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 *
 * volreg.c
 *	Implementation of the Volume Registry package.
 */

#include <dcedfs/param.h>		/* General system parameters */
#include <dcedfs/stds.h>		/* Coding standards */
#include <dcedfs/osi.h>			/* OS independence */
#include <dcedfs/common_data.h>		/* afsFid */
#include <dcedfs/volume.h>

#include "volreg.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/volreg/RCS/volreg.c,v 65.4 1998/03/23 16:47:02 gwehrman Exp $")

#define	VOLREG_HASHSIZE	128	/* Size of Volume Registry hash table */


/*
 * Since this package now supports lookup by volume name, we maintain
 * a two parallel hash tables.  One is keyed by ID and the other by name.
 * While a given volume will most likely be present in both tables, it
 * is not strictly necessary.  We do allow entries to be in one table but not
 * the other.  Both tables are protected by `volreg_TableLock' to provide
 * atomic entry/delete interfaces when both tables need to be updated.
 */

/*
 * volreg_cache
 *
 *	Volume Registry ID hash table entries.
 */
struct volreg_cache {
  struct volreg_cache *nextP;	/*Next in hash chain*/
  afs_hyper_t volumeId;		/*Volume ID*/
  struct volume *volP;		/*Ptr to volume descriptor*/
};

/*
 * volreg_name_cache
 *
 *	Volume Registry name hash table entries.
 */
struct volreg_name_cache {
  struct volreg_name_cache *nextP;	/*Next in hash chain*/
  afs_hyper_t volumeId;			/*Volume ID*/
  char volumeName[VOLNAMESIZE];		/*Volume name*/
  struct volume *volP;			/*Ptr to volume descriptor*/
};

/*
 * Volume Registry hash function macro.
 */
#define	VOLREG_HASH_VOLID(volid) (AFS_hgetlo(*(volid)) & (VOLREG_HASHSIZE-1))

static struct volreg_cache *volreg_Table[VOLREG_HASHSIZE];
static struct volreg_name_cache *volreg_NameTable[VOLREG_HASHSIZE];
static struct volreg_cache *volreg_FreeList;
static struct volreg_name_cache *volreg_NameFreeList;
static struct lock_data volreg_TableLock;
static int volreg_Initialized = 0;	


/* must be called with volreg_TableLock held at least in shared mode.
 * Shared is OK since no one ever looks at it in read mode, so shared
 * mode will lock out all accesses.
 */
static struct volreg_cache *volreg_AllocCache(void)
{
    struct volreg_cache *tcp;
    if (tcp = volreg_FreeList) {
	volreg_FreeList = tcp->nextP;
    }
    else
	tcp = (struct volreg_cache *) osi_Alloc(sizeof(*tcp));
    bzero((caddr_t)tcp, sizeof(*tcp));
    return tcp;
}


/* must be called with volreg_TableLock held at least in shared mode.
 * Shared is OK since no one ever looks at it in read mode, so shared
 * mode will lock out all accesses.
 */
static struct volreg_name_cache *volreg_AllocNameCache(void)
{
    struct volreg_name_cache *tcp;
    if (tcp = volreg_NameFreeList) {
	volreg_NameFreeList = tcp->nextP;
    }
    else
	tcp = (struct volreg_name_cache *) osi_Alloc(sizeof(*tcp));
    bzero((caddr_t)tcp, sizeof(*tcp));
    return tcp;
}


/* must be called with volreg_TableLock held at least in shared mode.
 * Shared is OK since no one ever looks at it in read mode, so shared
 * mode will lock out all accesses.
 */
static void volreg_FreeCache(struct volreg_cache *tcp)
{
    tcp->nextP = volreg_FreeList;
    volreg_FreeList = tcp;
}


/* must be called with volreg_TableLock held at least in shared mode.
 * Shared is OK since no one ever looks at it in read mode, so shared
 * mode will lock out all accesses.
 */
static void volreg_FreeNameCache(struct volreg_name_cache *tcp)
{
    tcp->nextP = volreg_NameFreeList;
    volreg_NameFreeList = tcp;
}


/*
 * PRIVATE volreg_Find
 *	Look up a volume registry entry by its volume id.
 *
 * Arguments:
 *	afs_hyper_t avolid			  : Volume ID to find.
 *	long ahashIdx			  : Hash chain to search.
 *	struct volreg_cache *aentryFoundP : Ptr to matching entry, or NIL
 *	struct volreg_cache *aprevEntryP  : Ptr to entry before match, or NIL
 *
 * Locks:
 *	Assumes that the caller has share-locked the VR table.
 *
 */
static void volreg_Find(afs_hyper_t *avolid, long ahashIdx,
			struct volreg_cache **aentryFoundPP,
			struct volreg_cache **aprevEntryPP)
{

    /* 
     * Sweep through the hash chain at the given index, breaking out as
     * soon as we find the proper entry.  If we make it to the end, our
     * pointer will properly be set to NIL.
     */
    if (aprevEntryPP)
	*aprevEntryPP = 0;
    *aentryFoundPP =  volreg_Table[ahashIdx];

    while (*aentryFoundPP) {
	if (AFS_hsame(*avolid, (*aentryFoundPP)->volumeId)) {
	    break;
	}
	if (aprevEntryPP)
	    *aprevEntryPP = *aentryFoundPP;
	*aentryFoundPP = (*aentryFoundPP)->nextP;
    }
}	/* volreg_Find */


/*
 * PRIVATE volreg_FindByName
 *	Look up a volume registry entry by its volume name.
 *
 * Arguments:
 *	char *avolname			       : Name of desired volume.
 *	afs_hyper_t *avolidP		       : Desired volid, may be NULL
 *	long ahashIdx			       : Hash chain to search.
 *	struct volreg_name_cache *aentryFoundP : Ptr to matching entry, or NIL
 *	struct volreg_name cache *aprevEntryP  : Ptr to entry before match, or NIL
 *
 * Locks:
 *	Assumes that the caller has share-locked the VR table.
 *
 */
static void volreg_FindByName(char *avolname, afs_hyper_t *avolidP,
			      long ahashIdx,
			      struct volreg_name_cache **aentryFoundPP,
			      struct volreg_name_cache **aprevEntryPP)
{

    /* 
     * Sweep through the hash chain at the given index, breaking out as
     * soon as we find the proper entry.  Callers may specify a non-NULL
     * value for avolidP.  If so, the entry's ID must also match.
     *
     * If we make it to the end, our pointer will properly be set to NIL.
     */
    if (aprevEntryPP)
	*aprevEntryPP = 0;
    *aentryFoundPP =  volreg_NameTable[ahashIdx];

    while (*aentryFoundPP) {
	if (strcmp(avolname, (*aentryFoundPP)->volumeName) == 0 &&
	    (avolidP == NULL ||
	     AFS_hsame((*aentryFoundPP)->volumeId, *avolidP))) {
	    break;
	}
	if (aprevEntryPP)
	    *aprevEntryPP = *aentryFoundPP;
	*aentryFoundPP = (*aentryFoundPP)->nextP;
    }
}	/* volreg_FindByName */


static long volreg_HashVolName(char *avolname)
{
   register char*		p;
   register unsigned long	h = 0;
   register unsigned long	g;

   for (p = avolname; *p != '\0'; ++p) {
       h = (h << 4) + *p;
       if (g = h & 0xf0000000) {
	   h ^= g >> 24;
	   h ^= g;
       }
   }

   return (h & (VOLREG_HASHSIZE - 1));
}	/* volreg_HashVolName */


/*
 * EXPORT volreg_Init
 *	Initialize the VR lock and hash table.
 */
void volreg_Init(void)
{
    /*
     * If we're already initialized, bug out right now.
     */
    if (volreg_Initialized)
	return;

    lock_Init(&volreg_TableLock);

    /*
     * Set up the VR tables.
     */
    bzero((caddr_t)volreg_Table, sizeof volreg_Table);
    bzero((caddr_t)volreg_NameTable, sizeof volreg_NameTable);
    volreg_Initialized = 1;
}	/* volreg_Init */


/*
 * EXPORT volreg_Enter
 *	Enter a volume and its information into the Volume Registry.
 *
 * Locks:
 *	volreg_TableLock is held in shared mode and then bumped up to thread 
 *	the new entry.
 */
long volreg_Enter(afs_hyper_t *avolid, struct volume *avolP, char *avolname)
{
    struct volreg_cache *newEntryP = 0;		/* Ptr to new VR entry */
    struct volreg_name_cache *newNameEntryP = 0;
    register long hashIdx;			/* Hash into VR table */
    register long nameHashIdx;

    osi_assert(avolid != 0 && avolname != 0);

    if (!volreg_Initialized)
	volreg_Init();

    /*
     * Share-lock the VR, then look up the given volume.  We scream bloody
     * murder if it's in either table.
     */
    lock_ObtainShared(&volreg_TableLock);

    /* Compute ID table index */
    hashIdx = VOLREG_HASH_VOLID(avolid);

    /* Make sure volume is not already in the ID hash table */
    volreg_Find(avolid, hashIdx, &newEntryP, NULL);
    if (newEntryP != 0) {
	lock_ReleaseShared(&volreg_TableLock);
	return EEXIST;
    }

    /* Compute name table index */
    nameHashIdx = volreg_HashVolName(avolname);

    /*
     * We allow entries with the same name in the name hash table, when we
     * want to (at delete time) we can tell them apart by their ID.
     */

    /*
     * Allocate a new VR entry, fill it in, and thread it into the tables.
     * Don't forget to hold the volume ptr, since it will now be remembered
     * in the hash tables.
     */
    lock_UpgradeSToW(&volreg_TableLock);

    /* Update the ID hash table */
    newEntryP = volreg_AllocCache();
    newEntryP->volumeId = *avolid;
    VOL_HOLD(avolP);
    newEntryP->volP = avolP;
    newEntryP->nextP = volreg_Table[hashIdx];
    volreg_Table[hashIdx] = newEntryP;

    /* Update the name hash table */
    newNameEntryP = volreg_AllocNameCache();
    newNameEntryP->volumeId = *avolid;
    strcpy(newNameEntryP->volumeName, avolname);
    newNameEntryP->volP = avolP;
    newNameEntryP->nextP = volreg_NameTable[nameHashIdx];
    volreg_NameTable[nameHashIdx] = newNameEntryP;

    lock_ReleaseWrite(&volreg_TableLock);

    return 0;
}	/* volreg_Enter */


/*
 * EXPORT volreg_ChangeIdentity
 *
 * Arguments:
 *
 * Locks:
 *	No locks are held across invocations.  Internally, volreg_TableLock
 *	is held for the entire routine.
 */
long volreg_ChangeIdentity(afs_hyper_t *oldvolid, afs_hyper_t *newvolid,
			   char *oldvolname, char* newvolname)
{
    struct volreg_cache *entryToDeleteP = 0;
    struct volreg_cache *prevEntryP;
    long hashIdx, newHashIdx;
    struct volreg_name_cache *nameEntryToDeleteP = 0;
    struct volreg_name_cache *prevNameEntryP;
    long nameHashIdx, newNameHashIdx;
    int changeId;
    int changeName;

    changeId = (oldvolid != 0 && !AFS_hiszero(*oldvolid));
    changeName = (oldvolname != 0 && oldvolname[0] != '\0');

    if (!changeId && !changeName)
	return 0;			/* No changes necessary */

    /*
     * Assert that if an old value was specified for either the ID or the name,
     * then a new value was also specified.
     */
    osi_assert((!changeId || (newvolid != 0 && !AFS_hiszero(*newvolid))) &&
	       (!changeName || (newvolname != 0 && newvolname[0] != '0')));

    if (!volreg_Initialized)
	volreg_Init();

    /*
     * Lock the hash table and see if the entries are in the tables.  If
     * the volume is in neither table, we simply return.  If the volume is
     * in one table and not in the other, we panic.
     */
    lock_ObtainShared(&volreg_TableLock);

    if (changeId) {
	/* Ensure that the new ID is not in use */
	newHashIdx = VOLREG_HASH_VOLID(newvolid);
	volreg_Find(newvolid, newHashIdx, &entryToDeleteP, NULL);
	if (entryToDeleteP != 0) {
	    lock_ReleaseShared(&volreg_TableLock);
	    return EEXIST;
	}

	hashIdx = VOLREG_HASH_VOLID(oldvolid);
	volreg_Find(oldvolid, hashIdx, &entryToDeleteP, &prevEntryP);
    }

    if (changeName) {
	newNameHashIdx = volreg_HashVolName(newvolname);
	nameHashIdx = volreg_HashVolName(oldvolname);
	volreg_FindByName(oldvolname, NULL, nameHashIdx,
			  &nameEntryToDeleteP, &prevNameEntryP);
    }

    /*
     * Assert that if an old value was specified, then we found it in the
     * hash table.
     */
    osi_assert((!changeId || entryToDeleteP != 0) &&
	       (!changeName || nameEntryToDeleteP != 0));

    lock_UpgradeSToW(&volreg_TableLock);

    if (entryToDeleteP != 0) {
	/*
	 * Pull the entry off of the old ID hash chain.
	 */
	if (prevEntryP == 0) {
	    /* The victim is at the head of the hash chain. */
	    volreg_Table[hashIdx] = entryToDeleteP->nextP;
	} else {
	    /* The victim is NOT at the head of the hash chain. */
	    prevEntryP->nextP = entryToDeleteP->nextP;
	}
	/*
	 * Put the entry on the new ID hash chain.
	 */
	entryToDeleteP->volumeId = *newvolid;
	entryToDeleteP->nextP = volreg_Table[newHashIdx];
	volreg_Table[newHashIdx] = entryToDeleteP;
    }

    if (nameEntryToDeleteP != 0) {
	/*
	 * Pull the victim off of the old name hash chain.
	 */
	if (prevNameEntryP == (struct volreg_name_cache *)0) {
	    /* The victim is at the head of the hash chain. */
	    volreg_NameTable[nameHashIdx] = nameEntryToDeleteP->nextP;
	} else {
	    /* The victim is NOT at the head of the hash chain. */
	    prevNameEntryP->nextP = nameEntryToDeleteP->nextP;
	}
	/*
	 * Put the entry on the new name hash chain.
	 */
	if (changeId)
	    nameEntryToDeleteP->volumeId = *newvolid;
	strcpy(nameEntryToDeleteP->volumeName, newvolname);
	nameEntryToDeleteP->nextP = volreg_NameTable[newNameHashIdx];
	volreg_NameTable[newNameHashIdx] = nameEntryToDeleteP;
    }

    lock_ReleaseWrite(&volreg_TableLock);

    return 0;
}	/* volreg_ChangeIdentity */


/*
 * EXPORT volreg_Delete
 *	Remove an entry from the Volume Registry; used when detaching/deleting
 *	a volume.
 *
 * Arguments:
 *	IN avolid   : Volume ID to delete.
 *	IN avolname : Name of above volume.
 *
 * Locks:
 *	No locks are held across invocations.  Internally, volreg_TableLock
 *	is held for the entire routine.
 */
long volreg_Delete(afs_hyper_t *avolid, register char *avolname)
{
    struct volreg_cache *entryToDeleteP;	/* Entry going bye-bye */
    struct volreg_cache *prevEntryP;		/* Previous entry to above */
    long hashIdx;				/* VR hash results */
    struct volreg_name_cache *nameEntryToDeleteP;
    struct volreg_name_cache *prevNameEntryP;
    long nameHashIdx;

    osi_assert(avolid != 0 && avolname != 0);

    if (!volreg_Initialized)
	volreg_Init();

    /*
     * Lock the hash table and see if the entries are in the tables.  If
     * the volume is in neither table, we simply return.  If the volume is
     * in one table and not in the other, we panic.
     */
    lock_ObtainShared(&volreg_TableLock);

    hashIdx = VOLREG_HASH_VOLID(avolid);
    volreg_Find(avolid, hashIdx, &entryToDeleteP, &prevEntryP);

    nameHashIdx = volreg_HashVolName(avolname);
    volreg_FindByName(avolname, avolid, nameHashIdx,
		      &nameEntryToDeleteP, &prevNameEntryP);

    if (entryToDeleteP == 0) {
	/* The name entry should definitely not exist either */
	osi_assert(nameEntryToDeleteP == 0);

	/* Nothing to delete; just return */
	lock_ReleaseShared(&volreg_TableLock);
	return ENODEV;
    }

    osi_assert(nameEntryToDeleteP != 0);

    /*
     * Bump up to a write lock and do the dirty deed.
     */
    lock_UpgradeSToW(&volreg_TableLock);

    /*
     * Pull the victim off the ID hash chain.
     */
    if (prevEntryP == 0) {
	/* The victim is at the head of the hash chain. */
	volreg_Table[hashIdx] = entryToDeleteP->nextP;
    } else {
	/* The victim is NOT at the head of the hash chain. */
	prevEntryP->nextP = entryToDeleteP->nextP;
    }

    VOL_RELE(entryToDeleteP->volP);
    volreg_FreeCache(entryToDeleteP);

    /*
     * Pull the victim off the name hash chain.
     */
    if (prevNameEntryP == (struct volreg_name_cache *)0) {
	/* The victim is at the head of the hash chain. */
	volreg_NameTable[nameHashIdx] = nameEntryToDeleteP->nextP;
    } else {
	/* The victim is NOT at the head of the hash chain. */
	prevNameEntryP->nextP = nameEntryToDeleteP->nextP;
    }

    volreg_FreeNameCache(nameEntryToDeleteP);

    lock_ReleaseWrite(&volreg_TableLock);

    return 0;
}	/* volreg_Delete */


/*
 * EXPORT volreg_Lookup
 *	Look for an entry in the Volume Registry, given a fid.
 *
 * Arguments:
 *	IN struct afsFid           : Ptr to the FID to look up.
 * 	OUT struct volume **avolPP : Set to the address of the volume desc
 *					associated with the given file.
 *
 * Locks:
 *	No locks are held across invocations.  Internally, volreg_TableLock
 *	is held for the entire routine.
 */
long volreg_Lookup(struct afsFid *afidP, struct volume **avolPP)
{
    struct volreg_cache *entryFoundP;	/* VR entry found */
    long hashIdx;			/* VR table hash index */
#ifdef SGIMIPS
    long code;				/* Return code */
#else
    long code;				/* Return code */
#endif
    afs_hyper_t *volId = &afidP->Volume; /* Volume field in the fid */

    if (!volreg_Initialized)
	volreg_Init();

    /* Clear the output pointer in case of failure return */
    if (avolPP != 0)
	*avolPP = 0;

    /*
     * Grab a read lock (since we don't need to modify anything), then see
     * if we can find an entry for the given volume.
     */
    hashIdx = VOLREG_HASH_VOLID(volId);
    lock_ObtainRead(&volreg_TableLock);
    volreg_Find(&(afidP->Volume), hashIdx, &entryFoundP, NULL);
    if (entryFoundP) {
	/* If a volume pointer is passed in, we return it (held) */
	if (avolPP) {
	    *avolPP = entryFoundP->volP;
	    VOL_HOLD(*avolPP);
	}
	code = 0;
    } else {
	code = ENODEV;
    }
    lock_ReleaseRead(&volreg_TableLock);

    return code;
}	/* volreg_Lookup */


/*
 * EXPORT volreg_LookupByName
 *	Look for an entry in the Volume Registry, given a name.
 *
 * Arguments:
 *	IN char *avolname          : Name to look up.
 * 	OUT struct volume **avolPP : Set to the address of the volume desc
 *					associated with the given file.
 *
 * Locks:
 *	No locks are held across invocations.  Internally, volreg_TableLock
 *	is held for the entire routine.
 */
long volreg_LookupByName(char* avolname, struct volume **avolPP)
{
    struct volreg_name_cache *entryFoundP;	/* VR entry found */
    long hashIdx;				/* VR table hash index */
    long code;					/* Return code */

    if (!volreg_Initialized)
	volreg_Init();

    /* Clear the output pointer in case of failure return */
    if (avolPP != 0)
	*avolPP = 0;

    /*
     * Grab a read lock (since we don't need to modify anything), then see
     * if we can find an entry for the given volume.
     */
    hashIdx = volreg_HashVolName(avolname);
    lock_ObtainRead(&volreg_TableLock);
    volreg_FindByName(avolname, NULL, hashIdx, &entryFoundP, NULL);
    if (entryFoundP) {
	/* If a volume pointer is passed in, we return it (held) */
	if (avolPP) {
	    *avolPP = entryFoundP->volP;
	    VOL_HOLD(*avolPP);
	}
	code = 0;
    } else {
	code = ENODEV;
    }
    lock_ReleaseRead(&volreg_TableLock);

    return code;
}	/* volreg_Lookup */
