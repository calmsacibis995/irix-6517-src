/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_tokenlist.c,v 65.3 1998/03/23 16:27:31 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * Copyright (c) 1994, Transarc Corp.
 */

/*
 * tokenList: tokenLists represent lists of granted or queued tokens
 *	      the type tokenList_t contains a mask which represents
 *	      the sum of all the types of tokens in the list and a
 * 	      doubly linked list of tokens.
 *	      The mask has two parts :
 *			typeSum which is the bit representation of the
 *			sum of the token types and a counter which is
 *			is an array of integers, one for each bit in
 *			the typeSum, counting how many of the tokens in
 *			the list have that bit set in their types field.
 *			This counter is used when deleting a token from
 *			the list to allow us to decide if this deletion
 *			should also unset any of the bits in the typeSum
 *			field without having to traverse the remaining
 *			tokens list.
 *	      list is is a double non circullar linked list (i.e. for the
 *	      first element in the list prev==NULL and for the last element
 *	      in the list next==NULL). The list does not have a dummy header
 *	      i.e the list and the first element in the list are identical.
 */

#include "tkm_internal.h"
#include "tkm_tokenlist.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_tokenlist.c,v 65.3 1998/03/23 16:27:31 gwehrman Exp $")

void tkm_AddToTokenMask(tkm_tokenMask_t *maskP,
			long types)
{
    int i;
    long x=1;

    maskP->typeSum |= types;
    for (i = 0; i < TKM_ALL_TOKENS; i++){
	if ((types & TKM_TOKENTYPE_MASK(i)) != 0)
	    (maskP->counter[i])++;
    }
}

void tkm_RemoveFromTokenMask(tkm_tokenMask_t *maskP,
			     long types)
{
    int i;
    long type;

    for (i = 0; i < TKM_ALL_TOKENS; i++){
	type = TKM_TOKENTYPE_MASK(i);
	if ((types & type) != 0){
	    osi_assert(maskP->counter[i] > 0);
	    if (--(maskP->counter[i]) == 0)
		maskP->typeSum &= ~(type);
	}
    }
}

void tkm_AddToTokenList(tkm_internalToken_p grantedP,
			tkm_tokenList_t *tokensP)
{
    tkm_AddToTokenMask(tokensP->mask, grantedP->types);
    tkm_AddToDoubleList(grantedP, &(tokensP->list));
}

void tkm_RemoveFromTokenList(tkm_internalToken_p revokedP,
			     tkm_tokenList_t *tokensP)
{
    tkm_RemoveFromDoubleList(revokedP, &(tokensP->list));
    tkm_RemoveFromTokenMask(tokensP->mask, revokedP->types);
}


void tkm_TokenListInit(tkm_tokenList_t *tokensP)
{
    if (tokensP->mask == NULL) {
	tokensP->mask = (tkm_tokenMask_t *) osi_Alloc(sizeof(tkm_tokenMask_t));
	bzero((caddr_t) tokensP->mask , sizeof(tkm_tokenMask_t));
    } else {
	osi_assert(tokensP->mask->typeSum == 0);
    }
    osi_assert(tokensP->list == NULL);
}


int tkm_FreeTokenList(tkm_tokenList_t *tokensP)
{
    if (tokensP->list != NULL)
	return 1;
    osi_Free(tokensP->mask, (sizeof(tkm_tokenMask_t)));
    osi_Free(tokensP, (sizeof(tkm_tokenList_t)));
    return(0);
}

void tkm_AddToDoubleList(tkm_internalToken_t *tokenP,
			 tkm_internalToken_t **listPP)
{
    if ((*listPP) != NULL)
	(*listPP)->prev = tokenP;
    tokenP->prev = NULL;
    tokenP->next = *listPP;
    *listPP = tokenP;
}

void tkm_RemoveFromDoubleList(tkm_internalToken_t *tokenP,
			      tkm_internalToken_t **listPP)
{

    if (tokenP->next != NULL)
	tokenP->next->prev = tokenP->prev;
    if (tokenP->prev != NULL)
	tokenP->prev->next = tokenP->next;
    else {
	osi_assert(*listPP == tokenP);
	*listPP = tokenP->next;
    }
    tokenP->next = tokenP->prev = NULL;
}





