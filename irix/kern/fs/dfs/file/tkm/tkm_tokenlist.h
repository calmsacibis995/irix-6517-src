/*
 * Copyright (c) 1994, Transarc Corp.
 */                                                                               

#ifndef TRANSARC_TKM_TOKENLIST_H
#define TRANSARC_TKM_TOKENLIST_H
#include "tkm_internal.h"

extern void tkm_AddToTokenMask _TAKES((tkm_tokenMask_t *maskP,
				       long types));

extern void tkm_RemoveFromTokenMask _TAKES((tkm_tokenMask_t *maskP,
					    long types));

extern void tkm_AddToTokenList _TAKES((tkm_internalToken_p grantedP,
				       tkm_tokenList_t *tokensP));

extern void tkm_RemoveFromTokenList _TAKES((tkm_internalToken_p revokedP,
					    tkm_tokenList_t *tokens));

extern int tkm_FreeTokenList _TAKES((tkm_tokenList_t *tokensP));

extern void tkm_TokenListInit _TAKES((tkm_tokenList_t *tokensP));

extern void tkm_AddToDoubleList _TAKES((tkm_internalToken_t *tokenP,
					tkm_internalToken_t **listPP));

extern void tkm_RemoveFromDoubleList _TAKES((tkm_internalToken_t *tokenP,
					tkm_internalToken_t **listPP));
#endif /*TRANSARC_TKM_TOKENLIST_H*/



