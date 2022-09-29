/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xcred.c,v 65.3 1998/03/23 16:42:50 gwehrman Exp $";
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
 * (C) Copyright 1995, 1990 Transarc Corporation
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 *
 *
 * xcred.c
 *	Implementation of the extended credential package.  These
 *	functions allow creation and interpretation of more general
 *	credential structures.  In particular, an xcred structure
 *	containing various forms of authentication information may be
 *	associated with a standard Unix credential.
 *
 * Locks: the xcred_lock locks the LRU queue, free lists and refCount
 *	on the cred structure itself.  All other fields are
 *      locked by the individual credp->lock locks.
 * Hierarchy: lock individual lock first, then global xcred_lock.
 */

#include <xcred.h>			/* Exported interface */
#include <dcedfs/param.h>		/* Should always be first */
#include <dcedfs/basicincludes.h>	/* Basic afs includes */
#include <dcedfs/osi.h>			/* OS-independent tools */
#include <dcedfs/debug.h>		/* Debugging interface */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xcred/RCS/xcred.c,v 65.3 1998/03/23 16:42:50 gwehrman Exp $")

/*
 * Define limits on how many entries we'll stash on the free queues.
 */
#define XCRED_MAXFREEQ_XCRED	20
#define XCRED_MAXFREEQ_PROP	40
#define XCRED_MAXFREEQ_GC	40

/*
 * Given a squeue struct (e) returns the associated xcred structure
 */
#define	XCRED_QTOX(e) \
    ((struct xcred *)((char *)(e) - offsetof(struct xcred, lruq)))

static int initDone = 0;		/* Module initialized? */
static struct squeue xcred_LRU;		/* LRU of active xcred structs */
static osi_dlock_t xcred_lock;		/* lock for xcred_LRU */

/* free lists */
static xcred_t *freeCredListP = 0;
static xcred_PListEntry_t *freePListP = 0;

/* allocate a plist structure, must be called with xcred_lock write-locked */
static xcred_PListEntry_t *xcred_AllocPList(void)
{
    register xcred_PListEntry_t *tcp;

    if (tcp = freePListP) {
	freePListP = tcp->nextP;
    }
    else {
	tcp = (xcred_PListEntry_t *) osi_Alloc(sizeof(xcred_PListEntry_t));
    }
    bzero((char *)tcp, sizeof(*tcp));
    return tcp;
}

/* free a plist structure; must be called with write-locked xcred_lock */
static int
xcred_FreePList(xcred_PListEntry_t *axp)
{
    axp->nextP = freePListP;
    freePListP = axp;
    return 0;
}

/* allocate a cred structure, must be called with xcred_lock write-locked */
static xcred_t *xcred_AllocCred(void)
{
    register xcred_t *tcp;

    if (tcp = freeCredListP) {
	freeCredListP = (xcred_t *) tcp->lruq.next;
    }
    else {
	tcp = (xcred_t *) osi_Alloc(sizeof(xcred_t));
    }
    bzero((char *)tcp, sizeof(*tcp));
    return tcp;
}

/* free a cred structure; must be called with write-locked xcred_lock */
static int
xcred_FreeCred(xcred_t *axp)
{
    axp->lruq.next = (struct squeue *) freeCredListP;
    freeCredListP = axp;
    return 0;
}

/*
 * EXPORT xcred_Init
 *
 * Environment:
 *	Perform the module's inititalization the first time it is called.
 *	It sets up the xcred, property list, and GC free pools, sets initDone.
 */
/* EXPORT */
long xcred_Init(void)
{
    if (initDone)
	return (0);
    initDone = 1;
    lock_Init(&xcred_lock);		/* init the lock for xcred_LRU */
    QInit(&xcred_LRU);
    return (0);
}


/*
 * EXPORT xcred_Create
 *
 * Locks:
 *	No locks are held across invocations.
 *
 */
/* EXPORT */
long xcred_Create(xcred_t **anewXCredPP)
{
    register xcred_t *newXCredP;	/* Direct ptr */

    /* make self-initializing */
    if (!initDone)
	xcred_Init();

    /*
     * Initialize a newly allocated structure (except for the free list header)
     */
    lock_ObtainWrite(&xcred_lock);
    newXCredP = xcred_AllocCred();
    newXCredP->refcount = 1;

    /*
     * Add it to the global xcred linked list
     */
    QAdd(&xcred_LRU, &newXCredP->lruq);
    lock_ReleaseWrite(&xcred_lock);
    *anewXCredPP = newXCredP;
    return (0);
}


/*
 * EXPORT xcred_Hold
 */
/* EXPORT */
long xcred_Hold(xcred_t *axcredP)
{
    lock_ObtainWrite(&xcred_lock);
    axcredP->refcount++;
    lock_ReleaseWrite(&xcred_lock);
    return (0);
}


/*
 * EXPORT xcred_Release
 *
 * Locks:
 *	The xcred is write-locked during the disposal.  In the case where
 *	the xcred is thrown away we don't release it.
 *
 * Side Effects:
 *	The attribute and value fields for each plist item are released.
 *	The xcred's GC handler and plist chains are returned to the free
 *	pool, as is the xcred itself.
 */
/* EXPORT */
long xcred_Release(xcred_t *axcredP)
{
    xcred_PListEntry_t *currPLP, *tmpPP;	/* Current plist entry */

    lock_ObtainWrite(&xcred_lock);
    /* note reference to flags field is OK if reference count is now
     * zero, since we have xcred_lock and no one else has pointer
     * to this dude.
     */
    if ((--axcredP->refcount > 0) || !(axcredP->flags & XCRED_FLAGS_DELETED))  {
	/*
	 * The xcred is still being referenced or not deleted yet; just return.
	 */
	lock_ReleaseWrite(&xcred_lock);
	return (0);
    }

    /*
     * Scrap all the property list structures, and the storage they contain.
     */
    for (currPLP = axcredP->propListP; currPLP; currPLP = tmpPP) {
	tmpPP = currPLP->nextP;
	osi_Free(currPLP->attributeP, currPLP->attrBytes);
	osi_Free(currPLP->valueP, currPLP->valueBytes);
	xcred_FreePList(currPLP);
    }

    /*
     * Remove it from the list of "active" xcreds
     */
    QRemove(&axcredP->lruq);

    /* and free the credential itself */
    xcred_FreeCred(axcredP);
    lock_ReleaseWrite(&xcred_lock);

    return (0);
}


/*
 * The data will be reclaimed when xcred_Release is called
 */
/* EXPORT */
long xcred_Delete(xcred_t *xcredp)
{
    lock_ObtainWrite(&xcredp->lock);
    xcredp->flags |= XCRED_FLAGS_DELETED;
    lock_ReleaseWrite(&xcredp->lock);
    return 0;
}


/*
 * EXPORT xcred_AssociateCreds
 *
 * Locks:
 *	The xcred is write-locked during the function.
 *
 */
long
xcred_AssociateCreds(
  xcred_t *axcredP,
  osi_cred_t *aucredP,
  u_long pag)
{
    long code;

    /*
     * Create a unique number pag (if not passed in) & put it in aucredP's
     * group list.  If all is ok (i.e. there is space in the group list)
     * preserve this pag in axcredP (so that xcred_UCredToXCred below
     * will find it later on.)
     */
    if (pag == 0)
	pag = osi_genpag();
    code = osi_SetPagInCred(aucredP, pag);
    if (code == 0) {
	lock_ObtainWrite(&(axcredP->lock));
	axcredP->assocPag = pag;
	axcredP->assocUCredP = aucredP;
	lock_ReleaseWrite(&axcredP->lock);
    }
    return (code);
}


/* EXPORT */
long xcred_GetUFlags(xcred_t *xcredp)
{
    register long temp;

    lock_ObtainRead(&xcredp->lock);
    temp = xcredp->uflags;
    lock_ReleaseRead(&xcredp->lock);
    return temp;
}


/*
 * Turn off aand bits, and turn on aor bits in xcredp->uflags
 */
/* EXPORT */
long xcred_SetUFlags(
  struct xcred *xcredp,
  long aor,
  long aand)
{
    lock_ObtainWrite(&xcredp->lock);
    xcredp->uflags |= aor;
    xcredp->uflags &= ~aand;
    lock_ReleaseWrite(&xcredp->lock);
    return 0;
}


/* EXPORT */
struct xcred *xcred_FindByPag(long pag)
{
    struct squeue *qp;
    struct xcred *xcredP = (struct xcred *)NULL;
    struct xcred * tmp_xcredP = (struct xcred *)NULL;

    /* make self-initializing */
    if (!initDone)
	xcred_Init();

    lock_ObtainRead(&xcred_lock);
    for (qp = xcred_LRU.prev; qp != &xcred_LRU; qp = QPrev(qp)) {
	if ((tmp_xcredP = XCRED_QTOX(qp))->assocPag == pag) {
	    tmp_xcredP->refcount++;
	    xcredP = tmp_xcredP;
	    break;
	}
    }
    lock_ReleaseRead(&xcred_lock);
    return xcredP;
}


/*
 * xcred_UCredToXCred
 *
 * Locks:
 *	The xcred's is write-locked while checking & bumping the refcount.
 *
 * Side Effects:
 *	The axcredPP carries back the address of the associated xcred.
 */
/* EXPORT */
long xcred_UCredToXCred(
  osi_cred_t *aucredP,
  xcred_t **axcredPP)
{
    long pag;					/* Associated pag */
    register struct squeue *qp, *tqp;		/* Creds queue pointer & tmp */
    xcred_t *xcredP;				/* Ptr to assoc xcred */

    pag = osi_GetPagFromCred(aucredP);
    if ((pag == OSI_NOPAG) || !(xcredP = xcred_FindByPag(pag)))
	return (XCRED_EC_NO_ENTRY);
    *axcredPP = xcredP;
    return (0);
}


/*
 * PRIVATE xcred_LookupPListEntry
 *	Given a pointer to a property list and a ptr to the attribute
 *	string to locate, return the address of the property list entry
 *	that describes the attribute, or a null pointer if it's not found.
 *
 * Must have xcred itself at least read locked.
*
 * Returns:
 *	The address of the matching property list entry if found, else the null ptr.
 *
 */
static xcred_PListEntry_t *
xcred_LookupPListEntry(
  xcred_PListEntry_t *aPLP,
  char *aattrP,
  long aattrLen)
{
    register xcred_PListEntry_t *currPLP;  /* Ptr to entry found */

    for (currPLP = aPLP; currPLP; currPLP = currPLP->nextP) {
	if (currPLP->flags & XCRED_PLISTFLAGS_DELETED)
	    continue;
	if (aattrLen == currPLP->attrBytes &&
	    bcmp(aattrP, currPLP->attributeP, aattrLen) == 0) {
	    break;
	}
    }
    return currPLP;
}


/*
 * Mark a node as deleted; it will be cleared when the reference count
 * actually goes to 0.  Note that this call doesn't lock the xcred; it
 * assumes it is already locked and that you're calling this function
 * from EnumerateProp, the only legal source for xcred_PListEntry objects.
 */
/* EXPORT */
long xcred_DeleteEntry(
  xcred_t *axcredp,
  struct xcred_PListEntry *aplp)
{
    aplp->flags |= XCRED_PLISTFLAGS_DELETED;
    return 0;
}


/*
 * EXPORT xcred_GetProp
 */
/* EXPORT */
long xcred_GetProp(
  xcred_t *axcredP,
  char *aattributeP,
  long aattributeLength,
  char **avaluePP,
  long *arealLengthP)
{
    xcred_PListEntry_t *matchPLP;
    register long code = 0;

    lock_ObtainRead(&(axcredP->lock));
    if (matchPLP = xcred_LookupPListEntry(axcredP->propListP, aattributeP, aattributeLength)) {
	*avaluePP = matchPLP->valueP;
	*arealLengthP = matchPLP->valueBytes;
    } else
	code = XCRED_EC_NO_ENTRY;
    lock_ReleaseRead(&(axcredP->lock));
    return (code);
}


/*
 * EXPORT xcred_PutProp
 *
 * Locks:
 *	The xcred is write-locked while manipulating its properties.
 *
 * Side Effects:
 *	A plist entry is created if needed.  If null params are passed,
 *	we free the plist entry, if one exists.  If abaseattr is not null,
 *	we walk down the list of derived plist entries there, deleting them all.
 *
 * Note that as long as the reference count on the property list is high, we
 * don't actually reclaim the space, but instead set a flag that tells
 * xcred_Release to do the removals.  In this way, we can let people copy
 * out data from a held xcred structure w/o worrying about the data pointers
 * going bad while our caller is running.
 *
 * Also note that the abase* parameters are now ignored, but the parameters
 * haven't been removed, to reduce external interface changes.
 */
/* EXPORT */
long xcred_PutProp(
  xcred_t *axcredP,
  char *aattributeP,
  long aattrLength,
  char *avalueP,
  long alength,
  char *abaseAttrP,
  long abaseAttrLength)
{
    xcred_PListEntry_t *basePLP = 0;	/* Ptr to base entry, if any */
    xcred_PListEntry_t *plp, *tplp;	/* Ptr to corresponding plist entry */

    lock_ObtainShared(&(axcredP->lock));

    /*
     * Look up the entry in xcred's property list to see if it already exists.
     */
    if (plp = xcred_LookupPListEntry(axcredP->propListP, aattributeP,
				     aattrLength)) {
	/*
	 * Property list entry already exists for this attribute/value pair.
	 */

	/*
	 * Boost ourselves to a write lock on the xcred, since it's time to
	 * start changing things.  Delete all derived entries, and also get
	 * rid of the given entry itself if avalueP == 0.  Also, commit to
	 * marking the xcred as changed.
	 */
	lock_UpgradeSToW(&(axcredP->lock));
	axcredP->changeCount++;

	/* see if we're just deleting the property */
	if (!avalueP) {
	    plp->flags |= XCRED_PLISTFLAGS_DELETED;
	    lock_ReleaseWrite(&(axcredP->lock));
	    return (0);		/* XXX Should we return an error here?? XXX */
	}

	/*
	 * Throw away the current contents of the value field, conjure up
	 * a new buffer, and copy in the new contents.
	 */
	osi_Free(plp->valueP, plp->valueBytes);
	plp->valueP = osi_Alloc(alength);
	bcopy(avalueP, plp->valueP, alength);
	plp->valueBytes = alength;
	lock_ReleaseWrite(&(axcredP->lock));
	return (0);
    }

    /*
     * This attribute/value pair doesn't appear in this xcred.  If this isn't just
     * a delete operation, grab a property list entry from the free pool, set it up,
     * and insert it into the plist.
     */
    if (!avalueP) {
	/*
	 * If this was really a delete command, we're golden, since
	 * this attribute wasn't here to begin with.
	 */
	lock_ReleaseShared(&(axcredP->lock));
	return (0);
    }

    /*
     * Allocate a new entry, copy the attributes/value components
     * and fill the rest
     */
    lock_ObtainWrite(&xcred_lock);
    /* need this lock for changes to global free lists */
    plp = xcred_AllocPList();
    lock_ReleaseWrite(&xcred_lock);
    plp->attrBytes = aattrLength;
    plp->attributeP = osi_Alloc(plp->attrBytes);
    bcopy(aattributeP, plp->attributeP, plp->attrBytes);
    plp->valueBytes = alength;
    plp->valueP = osi_Alloc(alength);
    bcopy(avalueP, plp->valueP, alength);

    /*
     * We need to boost our shared lock on the xcred, as we're now
     * threading in the new plist entry.  We also commit to marking
     * this xcred as changed.
     */
    lock_UpgradeSToW(&(axcredP->lock));
    axcredP->changeCount++;
    /* prevP is initialized by bzero in AllocPList */
    plp->nextP = axcredP->propListP;
    if (axcredP->propListP)
	axcredP->propListP->prevP = plp;
    axcredP->propListP = plp;

    lock_ReleaseWrite(&(axcredP->lock));
    return (0);
}


/*
 * EXPORT xcred_EnumerateProp: Sweep through the plist entries, applying the given function
 * until we've either covered the entries or we've hit an error on an invocation.
 */
/* EXPORT */
long xcred_EnumerateProp(
  xcred_t *axcredP,
  long (*aprocP)(char *, xcred_t *, xcred_PListEntry_t *),
  char *arockP)
{
    register long code = 0;			/* Return code */
    xcred_PListEntry_t *currPLP;		/* Ptr to current plist entry */

    lock_ObtainRead(&(axcredP->lock));
    for (currPLP = axcredP->propListP; currPLP; currPLP = currPLP->nextP) {
	if (code = (*aprocP)(arockP, axcredP, currPLP))
	    break;
    }
    lock_ReleaseRead(&(axcredP->lock));
    return (code);
}
