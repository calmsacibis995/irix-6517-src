/*
 * Copyright (c) 1996, 1994, Transarc Corporation
 * All Rights Reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/tkm/RCS/tkm_range.h,v 65.2 1998/02/02 22:18:49 lmc Exp $ */
/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *
 * File defining interesting functions and constants for the implementation
 * of byte ranges for files.
 *
 */


#ifndef TRANSARC_TKM_RANGE_H
#define TRANSARC_TKM_RANGE_H

#ifndef SGIMIPS
   /* Defined in values.h   */
#define BITSPERBYTE	8
#define BITS(type)	((int)sizeof(type) * BITSPERBYTE)
#define HIBITL		(1ul << (BITS(long) - 1))
#define MAXLONG		(~HIBITL)
#endif /* SGIMIPS */

typedef struct tkm_byterange {
    afs_hyper_t lowBnd;
    afs_hyper_t upBnd;
} tkm_byterange_t;

typedef tkm_byterange_t * tkm_byterange_p;

#define TKM_BYTERANGE_SET_RANGE(destP, lbP, ubP)	\
   (((destP)->lowBnd = *lbP), ((destP)->upBnd = *ubP))

#define TKM_BYTERANGE_COPY(dstP, srcP)	((*dstP) = (*srcP))

/*
 * We use the convention that a range is empty if its upper bound is smaller
 * than its upper bound or if the pointer to the range is NULL.  The following
 * macro tests a given pointer to a range for this condition.
 */

#define TKM_BYTERANGE_IS_EMPTY(a)      \
    (((a) == (tkm_byterange_p)NULL) || (AFS_hcmp((a)->upBnd, (a)->lowBnd) < 0))

/*
 * The intersection of two (non-empty) ranges is the range bounded below by the
 * largest of the lower bounds and above by the smallest of the upper bounds.
 */

/* we need to get the min & max of two hypers */
#define TKM_BYTERANGE_HMIN(haP, hbP)  \
    ((AFS_hcmp(*(haP), *(hbP)) < 0) ? (haP) : (hbP))
#define TKM_BYTERANGE_HMAX(haP, hbP)  \
    ((AFS_hcmp(*(haP), *(hbP)) > 0) ? (haP) : (hbP))

#define TKM_DISJOINT_BYTERANGES(leftP, rightP)\
    ( TKM_BYTERANGE_IS_EMPTY((leftP)) || \
      TKM_BYTERANGE_IS_EMPTY((rightP)) || \
      (AFS_hcmp(*(TKM_BYTERANGE_HMIN((&((leftP)->upBnd)), \
				 (&((rightP)->upBnd)))), \
	    *(TKM_BYTERANGE_HMAX((&((leftP)->lowBnd)), \
				 (&((rightP)->lowBnd)))))) < 0)


#define TKM_BYTERANGE_IS_SUBRANGE(subrangeP, rangeP)	\
  ((AFS_hcmp((subrangeP)->lowBnd, (rangeP)->lowBnd) >= 0) && \
   (AFS_hcmp((subrangeP)->upBnd, (rangeP)->upBnd) <= 0))

#define TKM_BYTERANGE_MAKE_RANGE_EMPTY(a)		\
    (AFS_hset64((a)->lowBnd,0,1), AFS_hzero((a)->upBnd))
    
void tkm_ByterangeNew _TAKES((tkm_byterange_p	srcP,
			      tkm_byterange_p *	dstPP));

/* functions to be provided by this module that cannot be written as macros */

void tkm_ByterangeIntersection _TAKES((tkm_byterange_p leftP,
				       tkm_byterange_p rightP,
				       tkm_byterange_p resultP ));

/* 
 * The following type is used for slice & dice tokens. The basic idea is
 * that if we might need to revoke only part of a byterange token to 
 * satisfy a request. We do that by revoking the entire token and at the
 * same type offering one or two tokens for the part of the byterange that
 * we don't need to revoke.
 */
   
typedef struct tkm_byterangePair {
  tkm_byterange_p	lo;
  tkm_byterange_p	hi;
} tkm_byterangePair_t;

typedef tkm_byterangePair_t * tkm_byterangePair_p;

#define TKM_BYTERANGEPAIR_FREE_RANGES(oldCellP)	\
  osi_Free((char *)((oldCellP)->lo), (long)sizeof(tkm_byterange_t));	\
  (oldCellP)->lo = (tkm_byterange_p)NULL;				\
  osi_Free((char *)((oldCellP)->hi), (long)sizeof(tkm_byterange_t));	\
  (oldCellP)->hi = (tkm_byterange_p)NULL


/* 
 * Complement takes two tkm_byterange_p's as arguments and returns the  
 * tkm_byterange_p representing the complement of the SECOND byterange with 
 *  respect to the FIRST.		    
 */
void tkm_ByterangeComplement _TAKES((tkm_byterange_p universeRangeP,
				     tkm_byterange_p partiallyEnclosedRangeP,
				     tkm_byterangePair_p resultP ));

#if ((!defined(KERNEL)) && DEBUG)

#define TKM_BYTERANGE_PRINT(stream, range)	\
  MACRO_BEGIN							\
    if ((range) != (tkm_byterange_p)NULL) {			\
      fprintf((stream), "[ %lu,,%lu: %lu,,%lu ]",		\
	      AFS_HGETBOTH((range)->lowBnd), AFS_HGETBOTH((range)->upBnd), \
    }								\
    else {							\
      fprintf((stream), "[ : ]");				\
    }								\
  MACRO_END

#define TKM_BYTERANGEPAIR_PRINT(stream, pair)	\
MACRO_BEGIN					\
  if ((pair) != (byterangePair_p)NULL) {	\
    fprintf((stream), "{");			\
    PrintByterange((stream), (pair)->lo);	\
    fprintf((stream), "; ");			\
    PrintByterange((stream), (pair)->hi);	\
    fprintf((stream), "}");			\
  }						\
  else {					\
    fprintf((stream), "{;}");			\
  }						\
MACRO_END

#endif 

#endif /* TRANSARC_TKM_RANGE_H */


