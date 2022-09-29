/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_volume.c,v 65.4 1998/04/01 14:16:36 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/* Copyright (C) 1996, 1994 Transarc Corporation - All rights reserved. */

/*
 * Routines to manipulate the volume hash tables
 */

#include <dcedfs/volume.h>
#include "tkm_internal.h"
#include "tkm_volume.h"
#ifdef SGIMIPS
#include <dcedfs/volreg.h>
#include "tkm_tokenlist.h"
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_volume.c,v 65.4 1998/04/01 14:16:36 gwehrman Exp $")

#define TKM_VOL_BUCKETS 256

static osi_dlock_t		tkm_volumeListLock;

/* the following 4 are protected by the tkm_volumeListLock*/
static tkm_vol_t	*tkm_volumeList[TKM_VOL_BUCKETS];
static tkm_vol_t  	*tkm_zeroRefcountVol;
static int 		tkm_volMax = 5000;
static int 		tkm_volCount = 0;

#define VOLHASH(id) (AFS_hgetlo(*id) & (TKM_VOL_BUCKETS - 1))

#ifdef SGIMIPS
static tkm_SetVolFlags(tkm_vol_t *volP, afsFid *fidP);
#endif

void tkm_InitVol()
{
    lock_Init(&tkm_volumeListLock);
    lock_ObtainWrite(&tkm_volumeListLock);
    bzero((caddr_t) tkm_volumeList, (TKM_VOL_BUCKETS * sizeof(tkm_vol_t *)));
    lock_ReleaseWrite(&tkm_volumeListLock);
}

int tkm_SetVolMax(int newMax)
{
    int old;

    tkm_Init();
    lock_ObtainWrite(&tkm_volumeListLock);
    old = tkm_volMax;
    if (newMax > 0)
	tkm_volMax = newMax;
    lock_ReleaseWrite(&tkm_volumeListLock);
    return(old);
}

tkm_vol_t *tkm_GetFirstVol()
{
    tkm_vol_t *firstP;
    int i;

    lock_ObtainWrite(&tkm_volumeListLock);
    for (i = 0; i < TKM_VOL_BUCKETS; i++) {
	if ((firstP = tkm_volumeList[i]) != NULL) {
	    tkm_FastHoldVol(firstP);
	    break;
	}
    }
    lock_ReleaseWrite(&tkm_volumeListLock);
    return(firstP);
}

tkm_vol_t *tkm_GetNextVol(tkm_vol_t *volP)
{
    tkm_vol_t *next = NULL;

    lock_ObtainWrite(&tkm_volumeListLock);
    if (volP->next != NULL){
	next = volP->next;
	tkm_FastHoldVol(next);
    } else {
	int pos = VOLHASH((&(volP->id)));
	while (++pos <  TKM_VOL_BUCKETS) {
	    next = tkm_volumeList[pos];
	    if (next != NULL) {
		tkm_FastHoldVol(next);
		break;
	    }
	}
    }
    lock_ReleaseWrite(&tkm_volumeListLock);
    return(next);
}



/* must be called with tkm_volumeListLock held */

static tkm_vol_t *tkm_AddVol(afsFid *fidP)
{
    tkm_vol_t *newVol;
    int volPos;
    static long tkm_lastVolWarning = 0;
    int code;
    
    osi_assert(lock_WriteLocked(&tkm_volumeListLock));
    if (tkm_volCount < tkm_volMax) {
	newVol = (tkm_vol_t *) osi_Alloc(sizeof(tkm_vol_t));
	tkm_volCount++;
    } else {
	long curTime = osi_Time();

	/* We are in trouble let someone know */

	if ((tkm_lastVolWarning + TKM_TIME_BETWEEN_WARNINGS) < curTime){
	    printf("TKM: out of fileset structures\n");
	    tkm_lastVolWarning = curTime;
	}
	return NULL;
    }
    bzero((caddr_t) newVol, sizeof(tkm_vol_t));
    lock_Init(&(newVol->lock));
    newVol->cell = fidP->Cell;
    newVol->id = fidP->Volume;

    code = tkm_SetVolFlags(newVol, fidP);
    if (code != TKM_SUCCESS) {
	/* volreg_Lookup failed */
	printf("TKM: volreg_Lookup() failed for fileset %u,,%u\n",
	       AFS_HGETBOTH(newVol->id));
	tkm_volCount--;
	osi_Free(newVol, sizeof(tkm_vol_t));
	return(NULL);
    }

    newVol->refcount = 1;
    newVol->tokenGrants = 0;
    tkm_TokenListInit(&(newVol->granted));
    /* put it in the list */
    volPos = VOLHASH((&(fidP->Volume)));
    if (tkm_volumeList[volPos] != NULL){
	tkm_volumeList[volPos]->prev = newVol;
	newVol->next = tkm_volumeList[volPos];
    }
    tkm_volumeList[volPos] = newVol;
    return newVol;
}

/* caller must hold the tkm_volumeListLock */

static void tkm_RemoveVol(tkm_vol_t *vol)
{
    register int volPos;

    osi_assert(lock_WriteLocked(&tkm_volumeListLock));
    osi_assert(lock_Check(&vol->lock) == 0);
    osi_assert(vol->granted.list == NULL);
    osi_assert(vol->beingGranted == NULL);
    osi_assert(vol->queuedFileTokens == NULL);
    osi_assert(vol->files == NULL);
    osi_assert(lock_Check(&vol->fileLock) == 0);

    if (vol->next != NULL)
	vol->next->prev = vol->prev;
    if (vol->prev != NULL) {
	vol->prev->next = vol->next;
    } else {
	/* this was the first item in the hash bucket, fix up the bucket */
	volPos = VOLHASH((&(vol->id)));
	osi_assert(vol == tkm_volumeList[volPos]);
	tkm_volumeList[volPos] = vol->next;
    }
    tkm_volCount--;
    osi_Free(vol->granted.mask, sizeof(tkm_tokenMask_t));
    osi_Free(vol, sizeof(tkm_vol_t));
}

tkm_vol_t *tkm_FindVol(afsFid *fidP,
		       int add)
{
    tkm_vol_t *thisVol;
    int depth = 0;
    afs_hyper_t *id = (afs_hyper_t *) (&(fidP->Volume));

    lock_ObtainWrite(&tkm_volumeListLock);
    thisVol = tkm_volumeList[VOLHASH(id)];
    while (thisVol != NULL) {
	if (AFS_hsame(*id, thisVol->id)) {
	    thisVol->refcount++;
	    lock_ReleaseWrite(&tkm_volumeListLock);
	    return(thisVol);
	}
	thisVol = thisVol->next;
    }
    if (add)
	thisVol = tkm_AddVol(fidP);
    else
	thisVol = NULL;
    lock_ReleaseWrite(&tkm_volumeListLock);
    return(thisVol);
}

void tkm_AddFileToVolume(tkm_file_t *file,
			 tkm_vol_t  *vol)
{
    lock_ObtainWrite(&(vol->fileLock));
    if (vol->files != NULL)
	vol->files->prev = file;
    file->prev = NULL;
    file->next = vol->files;
    vol->files = file;
    lock_ReleaseWrite(&(vol->fileLock));
}

void tkm_RemoveFileFromVolume(tkm_file_t *f)
{
    tkm_vol_t *vol;

    vol = f->vol;
    lock_ObtainWrite(&(vol->fileLock));
    if (f->next != NULL)
	f->next->prev = f->prev;
    if (f->prev != NULL)
	f->prev->next = f->next;
    else {
	osi_assert(vol->files == f);
	vol->files = f->next;
    }
    lock_ReleaseWrite(&(vol->fileLock));
}

/* Volume refcount handling */

void tkm_FastHoldVol(tkm_vol_t *vol)
{
    osi_assert(lock_WriteLocked(&tkm_volumeListLock));
    vol->refcount++;
}

void tkm_FastReleVol(tkm_vol_t *vol)
{
    osi_assert(lock_WriteLocked(&tkm_volumeListLock));
    osi_assert(vol->refcount > 0);
    if (--(vol->refcount) == 0){
	tkm_RemoveVol(vol);
    }
}

void tkm_HoldVol(tkm_vol_t *vol)
{
    lock_ObtainWrite(&tkm_volumeListLock);
    tkm_FastHoldVol(vol);
    lock_ReleaseWrite(&tkm_volumeListLock);
}

void tkm_ReleVol(tkm_vol_t *vol)
{
    lock_ObtainWrite(&tkm_volumeListLock);
    tkm_FastReleVol(vol);
    lock_ReleaseWrite(&tkm_volumeListLock);
}


/*
 * Find out if a volume is a readonly
 */

#ifdef	KERNEL
static tkm_SetVolFlags(tkm_vol_t *volP,
		       afsFid *fidP)
{
    long code ;
    struct volume *volumeP;

    code = volreg_Lookup(fidP, &volumeP);
    if (code == 0) {
	if ((volumeP->v_states & VOL_READONLY) != 0)
	    volP->flags = TKM_VOLRO;
	else
	    volP->flags = TKM_VOLRW;
	VOL_RELE(volumeP);
	return (TKM_SUCCESS);
    } else
	return(TKM_ERROR_FILEINVALID);
}
#else /*KERNEL*/
static tkm_SetVolFlags(tkm_vol_t *volP,
		       afsFid *fidP)
{
    long code ;
    struct volume *volumeP;

    if (fidP == NULL) {
	return(TKM_ERROR_FILEINVALID);
    }
    if (AFS_hgetlo(fidP->Cell) == -1)	/* simulate a readonly volume */
	volP->flags = TKM_VOLRO;
    else
	volP->flags = TKM_VOLRW;
    return(TKM_SUCCESS);
}
#endif /*KERNEL*/
