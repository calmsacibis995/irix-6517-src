/*
 * Copyright (c) 1994, Transarc Corp.
 */                                                                               

#ifndef TRANSARC_TKM_VOLTOKEN_H
#define TRANSARC_TKM_VOLTOKEN_H
#include "tkm_internal.h"

extern int tkm_GetVolToken _TAKES((tkm_internalToken_t *newTokenP,
					long flags));
extern int tkm_FixVolConflicts _TAKES((tkm_internalToken_t *tokenP,
					long flags,
					long *otherHostMaskP));

extern void tkm_ReturnVolToken _TAKES((tkm_internalToken_t *tokenP));

#endif /*TRANSARC_TKM_VOLTOKEN_H*/
