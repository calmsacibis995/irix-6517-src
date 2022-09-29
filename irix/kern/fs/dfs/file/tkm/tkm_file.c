/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_file.c,v 65.4 1998/04/01 14:16:27 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */

#include "tkm_internal.h"
#include "tkm_file.h"
#include "tkm_volume.h"
#ifdef SGIMIPS
#include "tkm_tokenlist.h"
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_file.c,v 65.4 1998/04/01 14:16:27 gwehrman Exp $")

#define TKM_FILE_BUCKETS 1024
osi_dlock_t   tkm_fileListLock; /*
				 * lock for the file hash table and
				 * tkm_fileCount
				 */

static tkm_file_t  	tkm_zeroRefcountDummy;

/* the following 4 are protected by the tkm_fileListLock*/
static tkm_file_t	*tkm_fileList[TKM_FILE_BUCKETS];
/*
 * the zeroRefCount list is a singly linked list who's last element
 * is the tkm_zeroRefcountDummy
 */

static tkm_file_t  	*tkm_zeroRefcountFile;
#ifdef KERNEL
static int 		tkm_fileMax = 11000; /*
					      * MUST be more than
					      * tkm_maxTokens + TKM_BULK_TOKENS
					      */
static int	tkm_fileLow = 1000;
#else /*KERNEL*/
static int 	tkm_fileMax = 300;
static int	tkm_fileLow = 30;
#endif /*KERNEL*/
static int 	tkm_fileCount = 0;		/* current count */
static int	tkm_fileCountHiwater = 0;	/* max ever in use */
static int	tkm_fileFreeCount = 0;		/* cumulative frees */
static int	tkm_fileAllocCount = 0;		/* cumulative allocs */
static int	tkm_fileScarcityCount = 0;	/* cumulative scarcity */
static int	tkm_fileRecycleCount = 0;	/* cumulative recycles */

#define FIDHASH(fid) \
    (((AFS_hgetlo((fid)->Volume) * 491) + ((fid)->Vnode *13)) \
     & (TKM_FILE_BUCKETS - 1))

#if TKM_FILE_HASH_STATS
static int tkm_hashFidMaxDepth = 0, tkm_hashFidSearches = 0;
static int tkm_hashFidAvgDepth;

KeepDepthStats(
	       int d)
{
    if (d > tkm_hashFidMaxDepth) tkm_hashFidMaxDepth = d;
    tkm_hashFidAvgDepth = (((tkm_hashFidSearches++) *  tkm_hashFidAvgDepth) +
			   d) / tkm_hashFidSearches;
}
#else
#define KeepDepthStats(d)
#endif


#if TKM_DEBUG

/* keep track of file structure holds and reles */

#if KERNEL
osi_dlock_t tkm_fileHLock;

struct tkm_fileRefH {
    struct tkm_fileRefH *next;
    caddr_t caller;
    int howMany;
} *tkm_FileHolds = NULL;

struct tkm_fileRefH *tkm_FileReles = NULL;


#ifdef SGIMIPS
void
#endif
tkm_FileRecordHold(tkm_file_t *	f, caddr_t caller)
{
   struct tkm_fileRefH *t;

   lock_ObtainWrite(&tkm_fileHLock);
   t = tkm_FileHolds;
   while (t != NULL) {
       if (t->caller == caller) {
	   t->howMany++;
	   break;
       }
       t = t->next;
   }
   if (t == NULL) { /* we need to add */
       t =  (struct tkm_fileRefH *) osi_Alloc(sizeof(struct tkm_fileRefH));
       t->caller = caller;
       t->howMany = 1;
       t->next = tkm_FileHolds;
       tkm_FileHolds = t;
   }
   lock_ReleaseWrite(&tkm_fileHLock);
}

#ifdef SGIMIPS
void
#endif
tkm_FileRecordRele(tkm_file_t *	f, caddr_t caller)
{
   struct tkm_fileRefH *t;

   lock_ObtainWrite(&tkm_fileHLock);
   t = tkm_FileReles;
   while (t != NULL) {
       if (t->caller == caller) {
	   t->howMany++;
	   break;
       }
       t = t->next;
   }
   if (t == NULL) { /* we need to add */
       t =  (struct tkm_fileRefH *) osi_Alloc(sizeof(struct tkm_fileRefH));
       t->caller = caller;
       t->howMany = 1;
       t->next = tkm_FileReles;
       tkm_FileReles = t;
   }
   lock_ReleaseWrite(&tkm_fileHLock);
}

#define	RECORDHOLD(f) tkm_FileRecordHold(f, osi_caller())
#define	RECORDRELE(f) tkm_FileRecordRele(f, osi_caller())

#else /* KERNEL*/
#define	RECORDHOLD(f) tkm_dprintf(("HOLD %x from %x\n", f, osi_caller()));
#define	RECORDRELE(f) tkm_dprintf(("RELE %x from %x\n", f, osi_caller()));
#endif /*KERNEL */

#else /*AFS_DEBUG */
#define	RECORDHOLD(f)
#define	RECORDRELE(f)
#endif /*AFS_DEBUG*/

void tkm_InitFile(void)
{
    lock_Init(&tkm_fileListLock);
    tkm_zeroRefcountFile = &tkm_zeroRefcountDummy;
}

int tkm_SetFileMax(int newMax)
{
    int old;
    tkm_Init();
    lock_ObtainWrite(&tkm_fileListLock);
    old = tkm_fileMax;
    if (newMax > 0)
	tkm_fileMax = newMax;
    lock_ReleaseWrite(&tkm_fileListLock);
    return(old);
}

/*
 * Routine used to get the first fileList table. It obtains and releases the
 * fileListLock. Returns the file held. If it returns NULL the table was
 * empty
 */
tkm_file_t *tkm_GetFirstFile()
{
    tkm_file_t *first;
    int i;

    lock_ObtainWrite(&tkm_fileListLock);
    for (i = 0; i < TKM_FILE_BUCKETS; i++) {
	if ((first = tkm_fileList[i]) != NULL) {
	    tkm_FastHoldFile(first);
	    break;
	}
    }
    lock_ReleaseWrite(&tkm_fileListLock);
    return(first);
}

/*
 * Routine used to traverse the fileList table. It obtains and releases the
 * fileListLock. Returns the file held. If it returns NULL we reached the end
 * of the table.
 */
tkm_file_t *tkm_GetNextFile(tkm_file_t *fileP)
{
    tkm_file_t *next = NULL;

    lock_ObtainWrite(&tkm_fileListLock);
    if (fileP->hashNext != NULL){
	next = fileP->hashNext;
	tkm_FastHoldFile(next);
    } else {
	int pos = FIDHASH((&(fileP->id)));
	while (++pos <  TKM_FILE_BUCKETS) {
	    next = tkm_fileList[pos];
	    if (next != NULL) {
		tkm_FastHoldFile(next);
		break;
	    }
	}
    }
    lock_ReleaseWrite(&tkm_fileListLock);
    return(next);
}



/*
 * Recycle a file entry. Called with tkm_fileListLock held
 */

#ifdef SGIMIPS
static tkm_file_t *tkm_RecycleFile(void)
#else
static tkm_file_t *tkm_RecycleFile()
#endif
{
    tkm_file_t *tmp = NULL, *found = NULL;

    osi_assert(lock_WriteLocked(&tkm_fileListLock));
    while (tkm_zeroRefcountFile != (&tkm_zeroRefcountDummy)) {
	if ((tkm_zeroRefcountFile->refcount) != 0){
	    tmp = tkm_zeroRefcountFile;
	    tkm_zeroRefcountFile = tkm_zeroRefcountFile->gcNext;
	    tmp->gcNext = NULL; /* flag that it's no longer on the list */
	} else {
	    /* we found one with 0 refcount so we can recycle it */
	    tmp = tkm_zeroRefcountFile;
    	    tkm_zeroRefcountFile = tmp->gcNext;
	    tkm_RemoveFileFromVolume(tmp);
	    /* remove it from the global hash table */
	    if (tmp->hashNext != NULL)
	    	    tmp->hashNext->hashPrev = tmp->hashPrev;
	    if (tmp -> hashPrev != NULL)
		tmp->hashPrev->hashNext = tmp->hashNext;
	    else {
		osi_assert(tkm_fileList[FIDHASH(&(tmp->id))] == tmp);
		tkm_fileList[FIDHASH(&(tmp->id))] = tmp->hashNext;
	    }
	    tkm_ReleVol(tmp->vol);
	    tmp->gcNext = NULL; /* take it off the 0 refcount list */
	    osi_assert(tmp->granted.list == NULL);
	    osi_assert(tmp->queued == NULL);
	    if (found == NULL) {
		found = tmp;
	    }

	    if (tkm_fileCount >= tkm_fileLow) 
		if (tmp != found) {
		    osi_Free(tmp->granted.mask, (sizeof(tkm_tokenMask_t)));
		    osi_Free(tmp, sizeof(tkm_file_t));
		    tkm_fileCount--;
		    tkm_fileFreeCount++;
		}

	    if (tkm_fileCount < tkm_fileLow) 
		break;
	}
    }

#if 0
    if (found == NULL) {
	int i;
	/*
	 * this code is just to make sure that the zeroRefCount list code
	 * works correctly.
	 */
	for (i = 0; i < TKM_FILE_BUCKETS; i++) {
	    tmp = tkm_fileList[i];
	    while (tmp != NULL) {
		if (tmp->refcount == 0) {
		    printf("BUT found a 0 refcount file anyway\n");
		    tkm_RemoveFileFromVolume(tmp);
		    /* remove it from the global hash table */
		    if (tmp->hashNext != NULL)
			tmp->hashNext->hashPrev = tmp->hashPrev;
		    if (tmp -> hashPrev != NULL)
			tmp->hashPrev->hashNext = tmp->hashNext;
		    else {
			osi_assert(tkm_fileList[FIDHASH(&(tmp->id))] == tmp);
			tkm_fileList[FIDHASH(&(tmp->id))] = tmp->hashNext;
		    }
		    tkm_ReleVol(tmp->vol);
		    return(tmp);
		} else
		    tmp = tmp -> hashNext;
	    }
	}
	tmp = NULL; /*if we get here we found nothing */
    }
#endif /* NOT_IN_PRODUCTION */
    return(found);
}

/*
 * Add a new file to the table. This routine must be called with
 * tkm_fileListLock held
 */

static tkm_file_t *tkm_AddFile(afsFid	*fid)
{
    tkm_file_t *newFile;
    tkm_vol_t *vol, *oldvol = NULL;
    int filePos = FIDHASH(fid);
    static long tkm_lastFileWarning = 0;

    osi_assert(lock_WriteLocked(&tkm_fileListLock));
    if ((vol = tkm_FindVol(fid, 1)) == NULL)
	    return NULL;
    if (tkm_fileCount < tkm_fileLow){ /* allocate a new structure */
	newFile = (tkm_file_t *) osi_Alloc(sizeof(tkm_file_t));
	bzero((caddr_t) (newFile), sizeof(tkm_file_t));
	tkm_fileAllocCount++;
	tkm_fileCount++;
	if (tkm_fileCount > tkm_fileCountHiwater) 
	    tkm_fileCountHiwater++;
    } else {
	/*
	 * try to recycle first. If you can't get one allocate if
	 * we are below the maximum allowed
	 */
	if ((newFile = tkm_RecycleFile()) == NULL) {
	    if (tkm_fileCount < tkm_fileMax){
		newFile = (tkm_file_t *) osi_Alloc(sizeof(tkm_file_t));
		bzero((caddr_t) (newFile), sizeof(tkm_file_t));
		tkm_fileAllocCount++;
		tkm_fileCount++;
		if (tkm_fileCount > tkm_fileCountHiwater) 
		    tkm_fileCountHiwater++;
	    } else {
		long curTime = osi_Time();

		/* We are in trouble let someone know */

		if ((tkm_lastFileWarning +
		     TKM_TIME_BETWEEN_WARNINGS) < curTime) {
		    printf("TKM: out of file structures\n");
		    tkm_lastFileWarning = curTime;
		}
		tkm_fileScarcityCount++;
		return NULL;
	    }
	} else 
	    tkm_fileRecycleCount++;
    }
    FidCopy((&(newFile->id)), fid);
    lock_Init(&(newFile->lock));
    /* we will put the file at the top of the hash bucket */
    if (tkm_fileList[filePos] != NULL){
	tkm_fileList[filePos]->hashPrev = newFile;
    }
    tkm_TokenListInit(&(newFile->granted));
    newFile->queued = NULL;
    newFile->hashPrev = NULL;
    newFile->hashNext = tkm_fileList[filePos];
    tkm_fileList[filePos] = newFile;
    newFile->tokenGrants = 0;
    newFile->refcount = 1;
    newFile->host = NULL;
    newFile->vol = vol;
    tkm_AddFileToVolume(newFile, vol); /* adjusts the next and prev fields*/
    /*
     * NOTE: do not rele new file's vol from the hold due to tkm_FindVol
     * since it now has an additional file hanging off of it
     */
    return(newFile);
}

/*
 * tkm_FindFile(fid, add) : Find a file structure given an afsFid
 * if add is set add the file if not found.
 *
 * The returned file is held (if it's a new file the refcount is set to 1)
 */


tkm_file_t *tkm_FindFile(afsFid	*fid,
			 int add)
{
    tkm_file_t *thisFile, *newPrev;
    int filePos = FIDHASH(fid);
    int depth = 0;

    lock_ObtainWrite(&tkm_fileListLock);
    thisFile = tkm_fileList[filePos];
    while (thisFile != NULL) {
	if (!FidCmp(&(thisFile->id), fid)){
	    /*
	     * if the file is found deep in the hash slot then move it
	     * up one position. This way frequently used files will rise
	     * to the top. We want to fo this only aftrer certain depth
	     * because if we did it always, if more than one file were
	     * being actively used at the same time we would up constantly
	     * moving them around each other. The depth to start doing this
	     * is set to a pretty small number (5) because we don't expect
	     * too many file to have a lot of token activity.

	     */

	    if (depth > 5) {
		tkm_file_t *newPrev, *newNext;
		/*
		 * keep a most frequently used type queue
		 * by moving the fid one up every time it's
		 * used
		 */
		newPrev = thisFile->hashPrev->hashPrev;
		newNext = thisFile->hashPrev;
		/* move out of the list */
		if (thisFile->hashNext != NULL)
		    thisFile->hashNext->hashPrev = thisFile->hashPrev;
		/* thisFile->hashPrev can't be NULL since depth > 5 */
		thisFile->hashPrev->hashNext = thisFile->hashNext;
		/* put in new position */
		newPrev->hashNext = thisFile;
		thisFile->hashPrev = newPrev;
		newNext->hashPrev = thisFile;
		thisFile->hashNext = newNext;
	    }
	    KeepDepthStats(depth);
	    thisFile->refcount++;
	    RECORDHOLD(thisFile);
	    lock_ReleaseWrite(&tkm_fileListLock);
	    return(thisFile);
	}
	depth++;
	thisFile = thisFile->hashNext;
    }
    if (add) {
	thisFile = tkm_AddFile(fid);
	RECORDHOLD(thisFile);
    }
    else
	thisFile = NULL;
    lock_ReleaseWrite(&tkm_fileListLock);
    return (thisFile);
}

/*
 * The following routines do the file refcount handling
 */


/* the Fast* routines expect their caller to take care of tkm_fileListLock */

void tkm_FastHoldFile(tkm_file_t *file)
{
	osi_assert(lock_WriteLocked(&tkm_fileListLock));
	RECORDHOLD(file);
	file->refcount++;
}

void tkm_FastReleFile(tkm_file_t *file)
{
    osi_assert(lock_WriteLocked(&tkm_fileListLock));
    osi_assert(file->refcount > 0);
    RECORDRELE(file);
    if (--(file->refcount) == 0) {
	osi_assert(lock_Check(&file->lock) == 0);
	/*
	 * put at the front of the files with  0 refcount list if it's
	 * not already in it
	 */
	if (file->gcNext == NULL) {
	    file->gcNext = tkm_zeroRefcountFile;
	    tkm_zeroRefcountFile = file;
	}
    }
}

void tkm_HoldFile(tkm_file_t *file)
{
    lock_ObtainWrite(&tkm_fileListLock);
    tkm_FastHoldFile(file);
    lock_ReleaseWrite(&tkm_fileListLock);
}

void tkm_ReleFile(tkm_file_t *file)
{
    lock_ObtainWrite(&tkm_fileListLock);
    tkm_FastReleFile(file);
    lock_ReleaseWrite(&tkm_fileListLock);
}


