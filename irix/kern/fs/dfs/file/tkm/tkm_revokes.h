/* Copyright (C) 1996, 1994 Transarc Corporation - All rights reserved. */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_revokes.h,v 65.2 1998/04/01 14:16:33 gwehrman Exp $ */

#ifndef TRANSARC_TKM_REVOKES
#define TRANSARC_TKM_REVOKES
#include "tkm_conflicts.h"

#ifdef SGIMIPS
extern void tkm_InitRevokes _TAKES((void));
#endif
extern long tkm_ParallelRevoke _TAKES((struct tkm_tokenConflictQ *conflicts,
				       int flags,
				       afs_recordLock_t *lockDescriptorP));
#define TKM_PARA_REVOKE_FLAG_DOINGGC	0x1
#define TKM_PARA_REVOKE_FLAG_FORVOLTOKEN	0x2

#endif /* TRANSARC_TKM_REVOKES */
