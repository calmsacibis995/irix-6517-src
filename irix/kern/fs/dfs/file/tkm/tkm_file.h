/*
 *      Copyright (C) 1992 1993 1994 Transarc Corporation
 *      All rights reserved.
 */

#ifndef TRANSARC_TKM_FILE_H
#define TRANSARC_TKM_FILE_H

extern osi_dlock_t   tkm_fileListLock; /* lock for the file hash table */

extern tkm_file_t *tkm_FindFile _TAKES((afsFid *fid, int add));
extern void tkm_HoldFile _TAKES((tkm_file_t *file));
extern void tkm_ReleFile _TAKES((tkm_file_t *file));
extern void tkm_FastHoldFile _TAKES((tkm_file_t *file));
#ifdef SGIMIPS
extern void tkm_FastReleFile(tkm_file_t *file);
extern void tkm_InitFile(void);
#else
extern void tkm_InitFile();
#endif
extern int tkm_SetFileMax _TAKES((int newMax));
extern tkm_file_t *tkm_GetNextFile(tkm_file_t *fileP);
#ifdef SGIMIPS
extern tkm_file_t *tkm_GetFirstFile(void);
#else
extern tkm_file_t *tkm_GetFirstFile();
#endif

#if TKM_FILE_HASH_STATS
extern int tkm_hashFidMaxDepth, tkm_hashFidSearches;
extern double tkm_hashFidAvgDepth;
#endif /* TKM_FILE_HASH_STATS */

#ifdef AFS_DEBUG
extern tkm_file_t *tkm_ExpAddFile(afsFid	*fid);
#endif

#endif /*TRANSARC_TKM_FILE_H*/






