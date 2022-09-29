/*
 *      Copyright (C) 1992 1993 1994 Transarc Corporation
 *      All rights reserved.
 */
#ifndef TRANSARC_TKM_MISC_H
#define TRANSARC_TKM_MISC_H
#include "tkm_tokens.h"

/* How many threads do parallel RPCs */
#define TKM_MAXPARALLELRPC 10
/* A pointer to these threads */
extern void * tkm_threadPoolHandle;

#define TKM_TOKEN_EXPIRED(tokenP) (((tokenP)->expiration < osi_Time()) &&\
				   !((tokenP)->flags & TKM_TOKEN_FOREVER))
#ifdef SGIMIPS
extern long tkm_Init(void);
#else
extern long tkm_Init();
#endif
extern u_long tkm_FreshExpTime _TAKES((tkm_internalToken_t *tokenP));
extern int afscall_tkm_control _TAKES((long	controlSwitch,
				       void *	parm2,
				       void *	parm3,
				       void *	parm4,
				       int *	retValP));
#ifdef SGIMIPS
extern u_long tkm_SetTokenExpirationInterval _TAKES((u_long newInterval));
#else
extern u_long tkm_SetTokenExpirationInterval _TAKES((int newInterval));
#endif
/*XXX we should get rid of this one too */
extern ulong tkm_expiration_secs;

/* how long to make the lists of conflicting tokens on each pass */
#define TKM_MAX_CONFLICTS 4 * TKM_MAXPARALLELRPC

#endif /*TRANSARC_TKM_MISC_H*/
