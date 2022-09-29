#ifndef _ICONV_CNV_H
#define _ICONV_CNV_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.6 $"
/*
 *
 * Copyright 1995-1997 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <sgidefs.h>
#include <stdlib.h>
#include <wchar.h>

/*
 * iconv_cnv.h
 */


#define ICONV_C_INITIALIZE  1	/* initialize the state			*/
#define ICONV_C_DESTROY	    2	/* release resources of the state	*/


typedef int	ICONV_S_RELPTR;


typedef size_t ( * ICONV_CNVFUNC )(
    void		 * handle,
    char		** inbuf,
    size_t		 * inbytesleft,
    char		** outbuf,
    size_t		 * outbytesleft
);

typedef void * ( * ICONV_CTLFUNC )(
    int			   func,
    void		 * state,
    void		 * conv,
    int			   numtab,
    void		** table
);

typedef wint_t ( * ICONV_BTOWC )(
    int			   sb,
    void		** handle
);

typedef int ( * ICONV_MBTOWC )(
    wchar_t		 * p_wc,
    const char		 * p_str,
    size_t		   nstr,
    void		** handle
);

typedef int ( * ICONV_MBLEN )(
    const char		 * p_str,
    size_t		   nstr,
    void		** handle
);

typedef size_t ( * ICONV_MBSTOWCS )(
    wchar_t		 * p_wc,
    const char		 * p_str,
    size_t		   nwc,    
    void		** handle
);

typedef int ( * ICONV_WCTOB )(
    wint_t		   wc,
    void		** handle
);

typedef int ( * ICONV_WCTOMB )(
    char		 * p_str,
    wchar_t		   wc,
    void		** handle
);

typedef size_t ( * ICONV_WCSTOMBS )(
    char		 * p_str,
    const wchar_t	 * p_wc,
    size_t		   nstr,
    void		 * handle
);


/* ======== ICONV_LIB_METHODS ========================================= */
/* PURPOSE:
 *	stdlib methods
 */

typedef struct  ICONV_LIB_METHODS {

    int			iconv_magic;	/* Magic word			*/
#define ICONV_MAGIC_NUM	0xc0afb5aa

    ICONV_BTOWC		iconv_btowc;	/* ptr to btowc function	*/
    ICONV_MBTOWC	iconv_mbtowc;	/* ptr to mbtowc function	*/
    ICONV_MBLEN		iconv_mblen;	/* ptr to mblen function	*/
    ICONV_MBSTOWCS	iconv_mbstowcs;	/* ptr to mbtowcs function	*/
    ICONV_CTLFUNC	iconv_mbwcctl;	/* ptr to ctl func for mbtowc	*/
        
    ICONV_WCTOB		iconv_wctob;	/* ptr to wctob function	*/
    ICONV_WCTOMB	iconv_wctomb;	/* ptr to wctomb function	*/
    ICONV_WCSTOMBS	iconv_wcstombs;	/* ptr to wcstombs function	*/
    ICONV_CTLFUNC	iconv_wcmbctl;	/* ptr to ctl func for wctomb	*/

    struct SGCC_METHS * iconv_sgcc_meths;   /* Pointer to methods for	*/
					/* compose funcs		*/

    long		iconv_fill[ 9 ];    /* Expansion room		*/

} ICONV_LIB_METHODS;


/* ======== _iconv_mbhandle_t ========================================== */
/* PURPOSE:
 *	
 */

typedef struct _iconv_mbhandle_s {

    void	      * iconv_handle[2];/* This is not state but handles*/
					/* that can be used for stateless*/
					/* encodings.			*/
    struct ICONV_LIBSPEC
		      * iconv_spec;	/* Spec.			*/

    ICONV_LIB_METHODS	iconv_meths[ 1 ];

    struct {
	struct ICONV_T_SPECKEY
		      * iconv_speckey;	/* The key used to get this spec*/
	struct ICONV_HEAD
		      * iconv_head;	/* mother of all heads		*/
	char		iconv_allocated;    /* Allocated by malloc	*/
					    /* free()'d on mbwc_close	*/
	char		iconv_cfill[ sizeof( void * ) -1 ];

	long		iconv_state[ 8 ];   /* State for use by conversion*/
	void	      * iconv_fill[ 3 ];    /* expansion space		*/
	
    } iconv_mb[ 1 ];

} * _iconv_mbhandle_t;

extern struct _iconv_mbhandle_s __libc_mbhandle[];


#define ICONV_MAGIC_RESVALS	0xdf023a59

/* ======== ICONV_REFER  ============================================== */
/* PURPOSE:
 *	This structure for a REFERENCE in Resources.
 */

typedef
struct ICONV_REFER {

    int    iconv_node_type;    /* File or Symbols ... */

    union {
           struct {
                   ICONV_S_RELPTR     iconv_file_ref;
	   } _file;

           struct {
                   ICONV_S_RELPTR     iconv_object;
                   ICONV_S_RELPTR     iconv_symbol;
	   } _symbols;
    } _objs;

} ICONV_REFER;

#define ICONV_REFER_FILE    1
#define ICONV_REFER_SYMBOL  2


#ifdef __cplusplus
}
#endif
#endif
