/*
 * Copyright (c) 1994, Transarc Corp.
 */                                                                               

#include "tkm_internal.h"

#define TKM_HOLDTOKEN(token)     \
MACRO_BEGIN \
    osi_assert(TKM_TOKEN_HOLDER_LOCKED(token)); \
    (token)->refcount++; \
MACRO_END 

#define TKM_RELETOKEN(token)	\
MACRO_BEGIN \
    osi_assert((token)->refcount > 0);	\
    (token)->refcount--;    \
    tkm_RemoveTypes((token), 0); \
MACRO_END

#define TKM_RECYCLE_TOKEN(token)  tkm_RemoveTypes((token), (token)->types);


#define TKM_RELE_AND_RECYCLE_TOKEN(token)\
MACRO_BEGIN \
    osi_assert((token)->refcount > 0);	\
    (token)->refcount--;    \
    TKM_RECYCLE_TOKEN(token); \
MACRO_END

/* exported calls */

extern void tkm_InitRecycle _TAKES((void));
extern void tkm_RemoveTypes _TAKES((tkm_internalToken_t *tokenP, long types));
extern tkm_internalToken_t *tkm_AllocToken _TAKES((int n));
extern tkm_internalToken_t *tkm_AllocTokenNoSleep _TAKES((int n, int *got));
extern void tkm_FreeToken _TAKES((tkm_internalToken_t *tokenP));
extern void tkm_AddToGcList _TAKES((tkm_internalToken_t *tokenP));
extern void tkm_RemoveFromGcList _TAKES((tkm_internalToken_t *tokenP));
extern int tkm_SetMaxTokens _TAKES((int n));
extern int tkm_SetMinTokens _TAKES((int n));
