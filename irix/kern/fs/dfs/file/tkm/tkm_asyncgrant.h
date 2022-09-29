/*
 *      Copyright (C) 1992 1993 1994 Transarc Corporation
 *      All rights reserved.
 */
/*
 * Routines to handle asynchronously granted (queued) tokens
 */

#ifndef TRANSARC_TKM_ASYNCGRANT_H
#define TRANSARC_TKM_ASYNCGRANT_H
#include "tkm_internal.h"

#ifdef SGIMIPS
extern void tkm_InitAsyncGrant _TAKES((void));
#endif
extern void tkm_TriggerAsyncGrants _TAKES(( tkm_internalToken_t *token));

#endif /*TRANSARC_TKM_ASYNCGRANT_H*/






