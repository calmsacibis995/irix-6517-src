/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: tkm_range.c,v 65.3 1998/03/23 16:27:26 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *	range.c -- Implementations of the byte range manipulation
 *	functions that could not be defined as macros.
 *
 *	Copyright (C) 1996, 1990 Transarc Corporation
 *	All rights reserved.
 */

#include <tkm_internal.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_range.c,v 65.3 1998/03/23 16:27:26 gwehrman Exp $")

void tkm_ByterangeNew(tkm_byterange_p	srcP,
		      tkm_byterange_p *	dstPP)
{
  osi_assert(dstPP != (tkm_byterange_p *)NULL);
  *dstPP = (tkm_byterange_t *) osi_Alloc(sizeof(tkm_byterange_t));
  osi_assert(*dstPP != NULL);
  if (srcP != (tkm_byterange_p)NULL) {
    TKM_BYTERANGE_COPY(*dstPP, srcP);
  }
  else {
    TKM_BYTERANGE_MAKE_RANGE_EMPTY(*dstPP);
  }
}

/**************************************************************************
 * Intersection takes two tkm_byterange_p's as arguments and returns the
 * tkm_byterange_p representing the intersection of the two arguments.
 **************************************************************************/
static void tkm_ByterangeIntersection(tkm_byterange_p		leftP,
				      tkm_byterange_p 	rightP,
				      tkm_byterange_p	resultP)
{
  /* initialize the range to be returned */
  TKM_BYTERANGE_MAKE_RANGE_EMPTY(resultP);

  if (! TKM_DISJOINT_BYTERANGES(leftP, rightP)) {
    TKM_BYTERANGE_SET_RANGE(resultP,
			    TKM_BYTERANGE_HMAX(&(leftP->lowBnd),
					       &(rightP->lowBnd)),
			    TKM_BYTERANGE_HMIN(&(leftP->upBnd),
					       &(rightP->upBnd)));
  }
}

/*************************************************************************
 * Complement takes two tkm_byterange_p's as arguments and returns the
 * tkm_byterange_p representing the complement of the SECOND byterange with
 * respect to the FIRST.		
 *************************************************************************/
void tkm_ByterangeComplement(tkm_byterange_p 	universeRangeP,
			     tkm_byterange_p 	partiallyEnclosedRangeP,
			     tkm_byterangePair_p	resultP)
{
    tkm_byterange_t enclosedRange;

    resultP->lo = (tkm_byterange_p)NULL;
    resultP->hi = (tkm_byterange_p)NULL;

    /*
     * if the first byterange is a subrange of the second there is no c
     * complement
     */
    if (TKM_BYTERANGE_IS_SUBRANGE(universeRangeP, partiallyEnclosedRangeP))
	return;
    /*
     * If the ranges are the same, there is no complement, so only do anything
     * if it is NOT true that both bounds coincide.
     */
    if ((AFS_hcmp(universeRangeP->lowBnd,
		  partiallyEnclosedRangeP->lowBnd) != 0) ||
	(AFS_hcmp(universeRangeP->upBnd,
		  partiallyEnclosedRangeP->upBnd) != 0)) {

	tkm_ByterangeIntersection(universeRangeP, partiallyEnclosedRangeP,
			      &enclosedRange);

	if (! TKM_BYTERANGE_IS_EMPTY(&enclosedRange)) {
	    tkm_ByterangeNew((tkm_byterange_p)NULL, &(resultP->lo));
	    TKM_BYTERANGE_SET_RANGE(resultP->lo,
				    &(universeRangeP->lowBnd),
				    &(universeRangeP->upBnd));

	    if (AFS_hcmp(universeRangeP->lowBnd, enclosedRange.lowBnd) == 0) {
		resultP->lo->lowBnd = enclosedRange.upBnd;
		AFS_hincr(resultP->lo->lowBnd);
	    } else {
		if (AFS_hcmp(universeRangeP->upBnd,
			     enclosedRange.upBnd) == 0) {
		    resultP->lo->upBnd = enclosedRange.lowBnd;
		    AFS_hdecr(resultP->lo->upBnd);
		} else {
		    /*
		     * the enclosed range is strictly enclosed by the universe,
		     * so there will be two pieces to the complement range
		     */
		    TKM_BYTERANGE_SET_RANGE(resultP->lo,
					    &(universeRangeP->lowBnd),
					    &(enclosedRange.lowBnd));
		    AFS_hdecr(resultP->lo->upBnd);
		    tkm_ByterangeNew(NULL, &(resultP->hi));
		    TKM_BYTERANGE_SET_RANGE(resultP->hi,
					    &(enclosedRange.upBnd),
					    &(universeRangeP->upBnd));
		    AFS_hincr(resultP->hi->lowBnd);
		}
	    }
	} else {
	    resultP->lo = universeRangeP;
	}
    }
}
