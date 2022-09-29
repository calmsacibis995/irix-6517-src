/*
 *      Copyright (C) 1996, 1992 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_filetoken.h,v 65.1 1997/10/20 19:18:05 jdoak Exp $ */

#ifndef TRANSARC_TKM_FILETOKEN_H
#define TRANSARC_TKM_FILETOKEN_H
#include "tkm_internal.h"

extern int tkm_GetFileToken _TAKES((tkm_internalToken_t *newTokenP,
				    long flags,
				    afs_recordLock_t *lockDescriptorP));
extern int tkm_FixFileConflicts _TAKES((tkm_internalToken_t *tokenP,
					tkm_file_t *fileP,
					long flags,
					long *otherHostMaskP,
					afs_recordLock_t *lockDescriptorP));
extern void tkm_ReturnFileToken _TAKES((tkm_internalToken_t *tokenP));

#endif /*TRANSARC_TKM_FILETOKEN_H*/






