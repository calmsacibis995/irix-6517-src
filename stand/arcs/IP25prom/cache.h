/*
 * File: cache.h
 * Purpose: Define structures and extern definitions for cache
 *          access routines.
 *
 *  >>> Brief description of purpose. <<<
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#if _LANGUAGE_C

typedef	struct sl_s {
#  define		SL_ENTRIES (CACHE_SLINE_SIZE/sizeof(__uint64_t))
    __uint64_t		sl_tag;
    __uint64_t		sl_cctag;
    __uint64_t	  	sl_data[SL_ENTRIES];
    unsigned short 	sl_ecc[SL_ENTRIES/2];
} sl_t;

#else 

#define		SL_TAG		0
#define		SL_CCTAG	8
#define		SL_DATA		16
#define		SL_ECC		(SL_DATA+CACHE_SLINE_SIZE)

#endif

#if _LANGUAGE_C

typedef	struct dl_s {
#  define		DL_ENTRIES (CACHE_DLINE_SIZE/sizeof(__uint64_t))
    __uint64_t		dl_tag;
    __uint64_t		dl_data[DL_ENTRIES];
    unsigned char	dl_ecc[DL_ENTRIES];
} dl_t;

#else

#define		DL_TAG		0
#define		DL_DATA		8
#define		DL_ECC		(DL_DATA+CACHE_DLINE_SIZE)

#endif

#if _LANGUAGE_C

typedef	struct	il_s {
#   define		IL_ENTRIES (CACHE_ILINE_SIZE/sizeof(__uint32_t))
    __uint64_t		il_tag;
    __uint64_t		il_data[IL_ENTRIES];
    unsigned char	il_parity[IL_ENTRIES];
}il_t;

#else

#define		IL_TAG		0
#define		IL_DATA		8
#define		IL_PARITY	(IL_DATA+CACHE_ILINE_SIZE*2)

#endif

#if	_LANGUAGE_C

/* typedef for cacheop_t is R10k.h */

#else 

#define		COP_ADDRESS	0
#define		COP_OP		8
#define		COP_TAGHI	12
#define		COP_TAGLO	16
#define		COP_ECC		20

#endif

#if _LANGUAGE_C

extern	void	sLine(__uint64_t, sl_t *);
extern	void	iLine(__uint64_t, il_t *);
extern	void	dLine(__uint64_t, dl_t *);
extern	void	cacheOP(cacheop_t *);

#endif
