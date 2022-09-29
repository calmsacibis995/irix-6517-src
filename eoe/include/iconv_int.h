#ifndef _ICONV_INT_H
#define _ICONV_INT_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.7 $"
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
#include <iconv_cnv.h>

/*
 * iconv_int.h
 */

/* ====================================================================	*/
/* ====================================================================	*/


/* ======== ICONV_METHODS ============================================= */
/* PURPOSE:
 *	Methods for the converter
 */

typedef struct  ICONV_METHODS {

    ICONV_CNVFUNC	iconv_converter;    /* ptr to cnv function	*/
    ICONV_CTLFUNC	iconv_control;	    /* ptr to control function	*/

} ICONV_METHODS;


/* ====================================================================	*/
/* ====================================================================	*/
/* Helper macros							*/


#define ICONV_S_MKADDR( typ, baddr, reladd )				\
    ( (typ)(								\
	(   __mkaddr = ( reladd ),					\
	    __mkaddr ? ( __mkaddr + ( char * ) ( baddr ) ) : 0		\
	)								\
    ) )
    
#define ICONV_S_DEFMKADDR	register ICONV_S_RELPTR __mkaddr;

/* ====================================================================	*/
/* ====================================================================	*/
/* Internal structures							*/

typedef int	ICONV_INT;
typedef short	ICONV_SHORT;

typedef ICONV_S_RELPTR ICONV_S_OBJECTSPEC;
typedef ICONV_S_RELPTR ICONV_S_SYMBOL;
typedef ICONV_S_RELPTR ICONV_S_METHSPEC;
typedef ICONV_S_RELPTR ICONV_R_SPEC;


/* ======== ICONV_S_TBLS ============================================== */
/* PURPOSE:
 *	Specifier for table
 */

typedef struct  ICONV_S_TBLS {
    ICONV_S_RELPTR	iconv_tableobject;  /* DSO or a file relptr	*/
    ICONV_S_SYMBOL	iconv_table;	    /* iconv_table == 0 means file*/
} ICONV_S_TBLS;

/* ======== ICONV_S_SPEC ============================================== */
/* PURPOSE:
 *	Iconv converter specifier
 *
 *	specifiers must be sorted.
 */

typedef struct  ICONV_S_SPEC {

    ICONV_INT		iconv_specnum;	    /* specnum			*/
    ICONV_S_METHSPEC	iconv_meth;	    /* relptr to methods	*/

    short		iconv_ntable;	    /* This spec is a join of n tables */

    ICONV_S_TBLS	iconv_table[ 1 ];

} ICONV_S_SPEC;


/* ======== ICONV_S_LSPEC ============================================= */
/* PURPOSE:
 *	STDLIB converter spec
 */

typedef struct  ICONV_S_LSPEC {

    ICONV_INT		iconv_specnum;	    /* specnum			*/

    ICONV_S_OBJECTSPEC  iconv_obj_lmth;	    /* relptr to object specifier*/
    ICONV_S_SYMBOL      iconv_sym_lmth;	    /* Symbol of stdlib methods	*/
    
    short		iconv_ntable_mbwc;  /* Number of tables	for mbwc*/
    short		iconv_ntable_wcmb;  /* Number of tables for wcmb*/

    ICONV_S_TBLS	iconv_table[ 1 ];

} ICONV_S_LSPEC;



/* ======== ICONV_T_SPECKEY =========================================== */
/* PURPOSE:
 *	Spec keys are loaded first and are searchable.  These are used
 *  both by iconv specs and stdlib specs.
 */

typedef struct  ICONV_T_SPECKEY {

    ICONV_S_RELPTR	iconv_specifier[ 2 ];
#define ICONV_SRC	0
#define ICONV_DST	1

    ICONV_R_SPEC	iconv_spec;	    /* relative pntr to spec	*/
					    /* ICONV_S_SPEC		*/

} ICONV_T_SPECKEY;


/* ======== ICONV_T_METHOD ============================================ */
/* PURPOSE:
 *	Method record
 */

typedef struct  ICONV_T_METHOD {

    ICONV_INT		iconv_num;	    /* method number		*/

    ICONV_S_OBJECTSPEC  iconv_algobject;    /* relptr to object specifier*/
    ICONV_S_SYMBOL      iconv_algorithm;    /* symbol to conversion algorithm*/
    
    ICONV_S_OBJECTSPEC  iconv_ctlobject;    /* relptr to object specifier*/
    ICONV_S_SYMBOL      iconv_control;	    /* symbol to compute the size*/
					    /* of the conversion state	*/
} ICONV_T_METHOD;


/* ======== ICONV_T_SYMBOL ============================================ */
/* PURPOSE:
 *	Symbol structure type for shared file
 */

typedef struct  ICONV_T_SYMBOL {

    ICONV_INT		iconv_num;	/* symbol number		*/
    ICONV_S_RELPTR	iconv_name;	/* relative pointer to symbol name */

} ICONV_T_SYMBOL;


/* ======== ICONV_T_OBJECTSPEC ======================================== */
/* PURPOSE:
 *	Object structure type for shared file
 */

typedef struct  ICONV_T_OBJECTSPEC {

    ICONV_INT		iconv_num;	/* object number		*/
    ICONV_S_RELPTR	iconv_name;	/* relative pointer to object name */

} ICONV_T_OBJECTSPEC;


/* ======== ICONV_T_FILE ============================================ */
/* PURPOSE:
 *	File structure type for shared file
 */

typedef struct  ICONV_T_FILE {

    ICONV_INT		iconv_num;	/* file number			*/
    ICONV_S_RELPTR	iconv_name;	/* relative pointer to file name */

} ICONV_T_FILE;


/* ======== ICONV_T_LOC_ALIAS ========================================= */
/* PURPOSE:
 *	A locale alias record
 */

typedef struct  ICONV_T_LOC_ALIAS {

    ICONV_S_RELPTR	iconv_locale;	/* relative pointer to locale name */
    ICONV_S_RELPTR	iconv_value;	/* relative pointer to alias value */

} ICONV_T_LOC_ALIAS;

/* ======== ICONV_S_ALIAS_LIST ======================================== */
/* PURPOSE:
 *	Locale alias list
 */

typedef struct  ICONV_S_ALIAS_LIST {
    ICONV_INT		iconv_count;
    ICONV_S_RELPTR	iconv_filler;
    ICONV_T_LOC_ALIAS	iconv_aliases[ 1 ];
} ICONV_S_ALIAS_LIST;


/* ======== ICONV_T_LOC_CODESET ======================================= */
/* PURPOSE:
 *	A locale codeset specifier
 */

typedef struct  ICONV_T_LOC_CODESET {
    ICONV_S_RELPTR	iconv_locale;	/* relative pointer to locale name */
    ICONV_S_RELPTR	iconv_value;	/* relative pointer to alias value */
} ICONV_T_LOC_CODESET;


/* ======== ICONV_S_CODESET_LIST ====================================== */
/* PURPOSE:
 *	Codeset list
 */

typedef struct  ICONV_S_CODESET_LIST {
    ICONV_INT		iconv_count;
    ICONV_S_RELPTR	iconv_filler;
    ICONV_T_LOC_CODESET	iconv_codesets[ 1 ];
} ICONV_S_CODESET_LIST;


/* ======== ICONV_T_LOC_RESOURCE ====================================== */
/* PURPOSE:
 *	A resource specifier
 */

typedef struct  ICONV_T_LOC_RESOURCE {
    ICONV_S_RELPTR	iconv_resname;	/* relative pointer to resource name */
    ICONV_S_RELPTR	iconv_value;	/* relative pointer to resource value */
} ICONV_T_LOC_RESOURCE;

/* ======== ICONV_S_RESOURCE_LIST ===================================== */
/* PURPOSE:
 *	Resource list
 */

typedef struct  ICONV_S_RESOURCE_LIST {
    ICONV_INT		 iconv_count;
    ICONV_S_RELPTR	 iconv_filler;
    ICONV_T_LOC_RESOURCE iconv_resources[ 1 ];
} ICONV_S_RESOURCE_LIST;


/* ======== ICONV_S_TABLE ============================================= */
/* PURPOSE:
 *	On disk image of table of converters
 */

typedef struct  ICONV_S_TABLE {

#define ICONV_MAGIC	0x3cb4ca1e
    ICONV_INT		iconv_magicnum;	    /* magic number		*/

    ICONV_INT		iconv_numfiles;	    /* number of files		*/
    ICONV_INT		iconv_nummethods;   /* number of methods	*/
    ICONV_INT		iconv_numobject;    /* number of objects	*/
    ICONV_INT		iconv_symbols;	    /* number of symbols	*/
    ICONV_INT		iconv_numspecobj;   /* number of spec objects	*/
    ICONV_INT		iconv_numlspcobj;   /* number of libspec objects*/

    ICONV_S_RELPTR	iconv_alias_list;   /* relptr to list of aliases*/
    ICONV_S_RELPTR	iconv_codeset_list; /* relptr to list of codesets*/
    ICONV_S_RELPTR	iconv_resource_list;/* relptr to list of codesets*/
    ICONV_INT		iconv_numlibspec;   /* number of stdlib specs	*/
    ICONV_INT		iconv_numspec;	    /* number of specifiers	*/
    ICONV_T_SPECKEY	iconv_speckey[1];   /* specifier keys		*/

} ICONV_S_TABLE;


/* ======== ICONV_SPEC ================================================ */
/* PURPOSE:
 *	Working version of iconv specifier
 */

typedef struct  ICONV_SPEC {

    ICONV_S_SPEC    * iconv_s_spec;
    ICONV_METHODS   * iconv_meth;	    /* methods evaluated	*/
    short	      iconv_numtab;	    /* number of tables		*/
    void	    * iconv_table[ 1 ];	    /* ptr to converter	table	*/

} ICONV_SPEC;


/* ======== ICONV_LIBSPEC ============================================= */
/* PURPOSE:
 *	Working version of STDLIB spec
 */

typedef struct  ICONV_LIBSPEC {

    ICONV_S_LSPEC   * iconv_s_spec;
    ICONV_LIB_METHODS
		    * iconv_meth;	    /* methods evaluated	*/
    short	      iconv_numtab_mbwc;    /* number of tables for mbwc*/
    short	      iconv_numtab_wcmb;    /* number of tables for wcmb*/
    void	    * iconv_table[ 1 ];	    /* ptr to converter	table	*/

} ICONV_LIBSPEC;


/* ======== ICONV_OBJECT ============================================== */
/* PURPOSE:
 *	Object pointer !
 */

typedef struct  ICONV_OBJECT {

    void	      * iconv_dlhandle;	/* DSO handle			*/

} ICONV_OBJECT;


/* ======== ICONV_SYMBOL ============================================== */
/* PURPOSE:
 *	Symbol specifier.
 */

typedef struct  ICONV_SYMBOL {

    void	      * iconv_symptr;	/* Pointer for symbol		*/

} ICONV_SYMBOL;


/* ======== ICONV_FILE ================================================= */
/* PURPOSE:
 *	File specifier.
 */

typedef struct  ICONV_FILE {

    void	      * iconv_fileptr;	/* Pointer for symbol		*/

} ICONV_FILE;


/* ======== ICONV_HEAD ================================================ */
/* PURPOSE:
 *	Head of the iconv structures
 */

#define ICONV_DEF_TYPE( NAME, TYPNAME, STUN, DEFREF, ARRAY )		\
    STUN TYPNAME {							\
	ICONV_SPEC	    DEFREF iconv_spec ARRAY;			\
									\
	ICONV_LIBSPEC	    DEFREF iconv_lspc ARRAY;			\
									\
	ICONV_SYMBOL	    DEFREF iconv_sym ARRAY;			\
									\
	ICONV_OBJECT	    DEFREF iconv_obj ARRAY;			\
									\
	ICONV_METHODS	    DEFREF iconv_meth ARRAY;			\
									\
	ICONV_FILE	    DEFREF iconv_file ARRAY;			\
    } NAME								\
/* End of define							*/

#define __ICONV_NADA

typedef ICONV_DEF_TYPE( ICONV_HASH_TYP_OF, ICONV_HASH_TYP_OF, union, __ICONV_NADA, [1]);
typedef ICONV_DEF_TYPE( ICONV_HASH_TYP, ICONV_HASH_TYP, union, *,__ICONV_NADA );
typedef ICONV_DEF_TYPE( ICONV_INDEXES, ICONV_INDEXES, struct, **,__ICONV_NADA );

/* A macro to compute table number from the ICONV_INDEXES structure	*/
#define ICONV_TBLNO( A ) ( ( int )					\
    (									\
	( ( void ** ) &( ( ICONV_INDEXES * ) 0 )->A )			\
	- ( ( void ** ) 0 )						\
    )									\
)									\
/* End of define							*/

typedef struct  ICONV_HASH_ELEM {
    
    unsigned		iconv_tblno:6;	/* Which table it belongs to	*/
    unsigned		iconv_fill:2;	/* unused bits			*/
    unsigned		iconv_index:24;	/* What is it's index in the table*/

    ICONV_HASH_TYP    * iconv_data;	/* the data being pointed to	*/
    
} ICONV_HASH_ELEM;


/* ======== ICONV_HASH_SETTING ======================================== */
/* PURPOSE:
 *	The settings for the hash operation.  These values are extracted
 *  from the iconvtab as resource "hash_parameters".  Default values
 *  are coded into iconv_lib.c .
 */

typedef struct  ICONV_HASH_SETTING {
    unsigned short	iconv_nelems;	/* Number of elems in tbl	*/
    unsigned short	iconv_max_occ;	/* maximum allowed occupancy	*/
    unsigned short	iconv_hash_key[4];/* random hash key		*/
    unsigned short	iconv_thresh;
    unsigned short	iconv_flags;
} ICONV_HASH_SETTING;


typedef struct  ICONV_HASH_HEAD {

    ICONV_HASH_SETTING *iconv_hash_set;
    unsigned short	iconv_noccupied;/* Number currently inserted	*/
    ICONV_HASH_ELEM	iconv_elems[1];	/* hang the hash elements	*/
					/* off here			*/
} ICONV_HASH_HEAD;

/* The purpose for hasing is that the iconvtab is often very large and	   */
/* likely getting bigger and large majority of applications will only call */
/* setlocale and it is a waste of memory and time initializing the entire  */
/* indexing array for all possible iconv elements. Using a two tiered	   */
/* system, allocation is done by dropping elements into a hash table until */
/* it fills and then to allocate the full size index tables so that the	   */
/* majority of applications will only require the first tier.  Setting	   */
/* the hash table size should be small so that little memory is requested  */
/* but large enough to handle a couple of calls to setlocale for the two   */
/* worst locales and also be a reasonable hashing number (prime) with some */
/* left over just in case since hash table elements are small.		   */

typedef struct ICONV_HEAD {

    ICONV_S_TABLE     * iconv_table;	/* if this is nil head is not init'd*/

    ICONV_HASH_HEAD   * iconv_hashtab;	/* ptr to hash table		*/
    union ICONV_TABLE {
	ICONV_INDEXES	  iconv_idx[ 1 ]; /* Tables when in index mode	*/
	ICONV_HASH_TYP ** iconv_array[ sizeof(ICONV_INDEXES)/sizeof(void *) ];
    }			iconv_tbl[ 1 ];

} ICONV_HEAD;

/* MACRO's to handle hash two tiered access.				*/

#define ICONV_H_REFTBL( head, tbl, index, lock )			\
    (									\
	head->iconv_hashtab ?						\
	iconv_hash_seek( head, ICONV_TBLNO( tbl ), index, lock )->tbl :	\
	( head->iconv_tbl->iconv_idx->tbl[ index ] )			\
    )									\
/* end of macro								*/

#define ICONV_H_WRITETBL( head, tbl, index, value, lock, errlabel )	\
	if ( head->iconv_hashtab ) {					\
	    if (!iconv_hash_write(head, ICONV_TBLNO(tbl),index,value,lock)) {\
		goto errlabel;						\
	    }								\
	} else {							\
	    if ( ! lock ) {						\
		ICONV_LOCK						\
	    }								\
	    if ( head->iconv_tbl->iconv_idx->tbl[ index ] ) {		\
		if ( ! lock ) {						\
		    ICONV_UNLOCK					\
		}							\
		goto errlabel;						\
	    }								\
	    head->iconv_tbl->iconv_idx->tbl[ index ] = value;		\
	    if ( ! lock ) {						\
		ICONV_UNLOCK						\
	    }								\
	}								\
/* end of macro								*/

#define ICONV_HASHFUNC( hash_key, index, tblno, nelem )			\
    ( (									\
	(((index)^((hash_key)[0])) * ((hash_key)[1]))			\
	+ (((tblno)^((hash_key)[2])) * ((hash_key)[3]))			\
    ) % (nelem) )							\
/* end of macro								*/
	    
extern const char * __iconv_tabname__;
#define ICONV_TABNAME	__iconv_tabname__

extern char ** __iconv_tabdirs__;
#define ICONV_TABDIRS   __iconv_tabdirs__


/* ======== iconv_t ============================================== */
/* PURPOSE:
 *	This structure contains the header of 
 */

struct _t_iconv {

    ICONV_CNVFUNC       iconv_converter;/* Keep converter function in a	*/
					/* convienient place to reduce	*/
					/* access time when iconv() is called*/
    void	      * iconv_state;	/* any required cnv state memory*/
					/* required by the algoritm	*/
    ICONV_SPEC	      * iconv_spec;	/* Spec.			*/

    ICONV_T_SPECKEY   * iconv_speckey;	/* Spec key			*/
    struct ICONV_HEAD * iconv_head;	/* mother of all heads		*/
    long		iconv_a_state[ 8 ];   /* State for use by conversion*/
};


/* set default mbstate -						*/
extern int __mbwc_setmbstate( const char * loc );
extern char * __mbwc_locale_alias( const char * loc );
extern char * __mbwc_locale_codeset( const char * loc );
extern void * __iconv_get_resource ( char * resname, void ** relo_base );
extern void * __iconv_open_reference ( ICONV_REFER * p_ref );


#ifdef __cplusplus
}
#endif
#endif
