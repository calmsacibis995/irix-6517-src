/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_ctable.c,v 65.4 1998/04/01 14:16:25 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *	conflictTable.c -- Token manager conflict table management
 */

/*
 *	Copyright (C) 1990, 1991, 1992, 1993, 1994 Transarc Corporation
 *	All rights reserved.
 */

#include "tkm_internal.h"
#include "tkm_tokens.h"
#include "tkm_ctable.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_ctable.c,v 65.4 1998/04/01 14:16:25 gwehrman Exp $")

/* the actual compatibilty table */

static long rangedTypes;

short tkm_ctable0[256];	/* conf table for first 8 bits */
short tkm_ctable1[256];	/* conf table for 2nd 8 bits */

static unsigned short ranged[] = {
  TKM_LOCK_READ,
  TKM_LOCK_WRITE,
  TKM_DATA_READ,
  TKM_DATA_WRITE
};

unsigned short conflict[][2] = {
  { TKM_OPEN_READ,	TKM_OPEN_EXCLUSIVE },
  { TKM_OPEN_WRITE,	TKM_OPEN_EXCLUSIVE },
  { TKM_OPEN_WRITE,	TKM_OPEN_SHARED },
  { TKM_OPEN_EXCLUSIVE, TKM_OPEN_EXCLUSIVE },
  { TKM_OPEN_EXCLUSIVE, TKM_OPEN_SHARED },
  { TKM_STATUS_READ,	TKM_STATUS_WRITE },
  { TKM_STATUS_WRITE,	TKM_STATUS_WRITE },
  { TKM_LOCK_READ,	TKM_LOCK_WRITE },
  { TKM_LOCK_WRITE,	TKM_LOCK_WRITE },
  { TKM_DATA_READ,	TKM_DATA_WRITE },
  { TKM_DATA_WRITE,	TKM_DATA_WRITE },
  { TKM_OPEN_DELETE,	TKM_OPEN_READ },
  { TKM_OPEN_DELETE,	TKM_OPEN_WRITE },
  { TKM_OPEN_DELETE,	TKM_OPEN_SHARED },
  { TKM_OPEN_DELETE,	TKM_OPEN_EXCLUSIVE },
  { TKM_OPEN_DELETE,	TKM_OPEN_DELETE },
  { TKM_OPEN_DELETE,	TKM_OPEN_PRESERVE },
  /* the token types for moving filesets conflict with each other */
  { TKM_SPOT_HERE,	TKM_SPOT_THERE }
};

static unsigned char ctable[TKM_ALL_TOKENS][TKM_ALL_TOKENS];

void tkm_InitConflictTable()
{
  int	totalConflicts = (sizeof(conflict) /  sizeof(conflict[0]));
  int	i,j,k;	/* indices */
  int onion;	/*union of all guys conflicting (union is a reserved word)*/


  bzero( (caddr_t) ctable, sizeof(ctable));
  for (i = 0; i < totalConflicts; i++) {
      ctable[conflict[i][0]][conflict[i][1]] =
		ctable[conflict[i][1]][conflict[i][0]] = 1;
  }

  /* do some init of some quick tables, based on noticing that there
   * are only 16 bits (really less) of tokens today, and that we
   * can thus lookup the conflicting tokens by doing two indexes
   * into two 256-element integer arrays.
   *
   * That is, to generate the set of tokens that conflict with foo,
   * we start with tkm_ctable0[foo & 0xff].  This represents the
   * set of tokens that conflict with the set of tokens represented
   * by the low-order 8 bits of foo.  We then compute
   * tkm_ctable1[(foo>>8)&0xff], representing the set of tokens
   * that conflict with the 2nd-most low order bits of foo.
   * The union of these two (via the OR operation) represents those
   * tokens that conflict with any of the tokens foo.
   *
   * Of course, we only have to check the byte range tokens for conflict
   * if the byte ranges overlap.  If they don't overlap, then we only
   * check the non-byte range tokens.
   */

#ifdef SGIMIPS
/* Turn off "expression has no effect" error. */
#pragma set woff 1171
#endif
  osi_assert (TKM_ALL_TOKENS <= 16);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
  /* compute tkm_ctable0 by brute force checking each token for conflicts */
  for(i=0;i<256;i++)	{	/* for each 8 bit bit pattern */
    onion = 0;
    for(j=0;j<8;j++) {		/* for each token we have in this pattern */
      if (i & (1<<j))	{ /* this is one of ours */
	for(k=0; k<16; k++) {	/* and each token it may conflict with */
	  /* see if token j and k conflict */
	  if (ctable[j][k])
	    onion |= (1<<k);
	}
      }
    }
    tkm_ctable0[i] = onion;	/* tokens that conflict with i */
  }

  /* compute tkm_ctable1, too */
  for(i=0;i<256;i++)	{	/* for each 8 bit bit pattern */
    onion = 0;
    for(j=0;j<8;j++) {		/* for each token we have in this pattern */
      if (i & (1<<j))	{ /* this is one of ours */
	for(k=0; k<16; k++) {	/* and each token it may conflict with */
	  /* see if token j and k conflict */
	  if (ctable[j+8][k])
	    onion |= (1<<k);
	}
      }
    }
    tkm_ctable1[i] = onion;	/* tokens that conflict with i */
  }
}
