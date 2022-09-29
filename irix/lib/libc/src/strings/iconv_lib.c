/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 * SGI implementation of iconv_* calls.
 */

#ifndef linux
	#pragma weak iconv = _iconv
	#define iconv _iconv
	#pragma weak iconv_open = _iconv_open
	#define iconv_open _iconv_open
	#pragma weak iconv_close = _iconv_close
	#define iconv_close _iconv_close
	#pragma weak __iconv_tabname__ = ___iconv_tabname__
	#define __iconv_tabname__ ___iconv_tabname__
	#pragma weak __iconv_tabdirs__ = ___iconv_tabdirs__
	#define __iconv_tabdirs__ ___iconv_tabdirs__
#endif

#include "synonyms.h"

#include <stdio.h>
#include <wchar.h>
#include <alloca.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <sys/mman.h>
#include <iconv.h>
#include <iconv_cnv.h>
#include <iconv_int.h>

#if _PIC
#include <dlfcn.h>
#else
/* These symbols are from nonshrtab.c */
extern void *  __ns_iconv_lookup(
    void            * handle,
    const char      * name
);
extern void * __ns_iconv_open( const char * pathname );
#endif

typedef struct _t_iconv *sgiiconv_t;

/* MMAP the iconvtab, this will save having to read it into many processes*/
#ifndef ICONV_MMAPTAB
#define ICONV_MMAPTAB 1
#endif

#define ALIGN( A, B )	( 1 + ( ((A)-1) | ((B)-1) ) )
#define ICONV_LOCK
#define	ICONV_UNLOCK


#if TESTDEBUG
const char  * __iconv_tabname__ = "iconvtab";
#else
const char  * __iconv_tabname__ = "/usr/lib/iconv/iconvtab";
#endif

static const char * local_iconv_tabdirs[] =
    { "/usr/lib/iconv/tables", "/var/lib/iconv/tables", "/usr/lib/iconv", 0 };
char ** __iconv_tabdirs__ = ( char ** ) local_iconv_tabdirs;

static struct ICONV_HEAD iconv_top[ 1 ]; /* the start of all iconv stuff*/

static const ICONV_HASH_SETTING	iconv_hash_defaults[] = {
    {
	11, 8, { 0xa5e5, 0x385a, 0xe9f8, 0xbeef }, 128, 0
    }
};

typedef int  ( * bsearch_compar_t )( const void *, const void * );

typedef struct  ICONV_COMPAR_SPEC {
        
    char            * iconv_specifier[ 2 ];
    ICONV_HEAD	    * iconv_head;
    ICONV_S_TABLE   * iconv_table;
			            
} ICONV_COMPAR_SPEC;


/* ======== iconv_readtable =========================================== */
/* PURPOSE:
 *	Read the conversions table
 *
 * RETURNS:
 * 	The pointer to the table
 */

static void * iconv_readtable( const char * fname, ssize_t * psize )
{

    ICONV_S_TABLE     * p_st;
    int		        fd;
    off_t		size;
    int			error;

    fd = open( fname, O_RDONLY );

    if ( fd < 0 ) {
	return 0;
    }

    size = lseek( fd, ( off_t ) 0, SEEK_END );

    if ( size == ( off_t ) -1 ) {
clean_up:
	error = oserror();
	close( fd );
	setoserror( error );
	return 0;
    }

#if ICONV_MMAPTAB

    {
	size_t		mapsiz = ALIGN( size, getpagesize() );

	p_st = ( ICONV_S_TABLE * ) mmap(
	    ( void * ) 0,
	    mapsiz,
	    PROT_READ,
	    MAP_SHARED,
	    fd,
	    0
	);

	if ( p_st == ( ICONV_S_TABLE * ) (ssize_t) -1 ) {
	    goto clean_up;
	}

	close( fd );

	* psize = (ssize_t) mapsiz;

    }
#else

    /* Allocate the iconvtab size and read the file into memory		*/
    p_st = malloc( size );
    if ( ! p_st ) {
	goto clean_up;
    }

    lseek( fd, ( off_t ) 0, SEEK_SET );
    if ( size != read( fd, ( char * ) p_st, size ) ) {
	free( p_st );
	goto clean_up;
    }

    close( fd );
    
    * psize = size;

#endif

    return p_st;

} /* end iconv_readtable */

/* ======== __iconv_find_resource_cmp ================================= */
/* PURPOSE:
 *	resource comparator
 *
 * RETURNS:
 * 	
 */

static int	__iconv_find_resource_cmp(
    const char * loc, const ICONV_T_LOC_RESOURCE * p_lr
) {
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;
    char		    * str;
    ICONV_S_DEFMKADDR

    p_h = iconv_top;
    p_st = p_h->iconv_table;

    str = ICONV_S_MKADDR( char *, p_st, p_lr->iconv_resname );

    if ( str ) {
	return strcmp( loc, str );
    }

    return -1;

} /* end __iconv_find_resource_cmp */


/* ======== iconv_find_resource ======================================= */
/* PURPOSE:
 *	Find a resource in the iconvtab assuming only the ICONV_S_TABLE
 *  initialized.
 *
 * RETURNS:
 * 	
 */

static void	* iconv_find_resource(
    ICONV_S_TABLE     * p_st,
    const char	      * loc
) {

    ICONV_S_RESOURCE_LIST   * p_rl;
    ICONV_T_LOC_RESOURCE    * p_lr;
    ICONV_S_DEFMKADDR

    p_rl = ICONV_S_MKADDR(
	ICONV_S_RESOURCE_LIST *, p_st, p_st->iconv_resource_list
    );

    /* find the specifier with a bsearch				*/
    p_lr = bsearch(
	loc,
	p_rl->iconv_resources,
	p_rl->iconv_count,
	sizeof( ICONV_T_LOC_RESOURCE ),
	( bsearch_compar_t ) __iconv_find_resource_cmp
    );

    if ( p_lr ) {
	return ICONV_S_MKADDR( void *, p_st, p_lr->iconv_value );
    }

    return ( void * ) 0;

} /* end iconv_find_resource */


/* ======== iconv_create_tables ====================================== */
/* PURPOSE:
 *	Time to promote to tables
 *
 * RETURNS:
 * 	1 if success
 *	0 for failure (from malloc faulure)
 */

static int	iconv_create_tables(
    ICONV_HEAD	      * p_h,
    ICONV_S_TABLE     * p_st
) {
    ssize_t		size;
    void	     ** mem;
    ICONV_INDEXES     * p_i;

    /* compute the size of memory required to store all entries		*/
    size = (ssize_t) sizeof( void * ) * (
	p_st->iconv_numobject + p_st->iconv_symbols + p_st->iconv_numspecobj
	+ p_st->iconv_nummethods + p_st->iconv_numfiles + p_st->iconv_numlspcobj
    );

    /* allocate memory and return if there is a failure			*/
    mem = malloc( size );
    if ( ! mem ) {
	return 0;
    }

    /* Set everything to zero.  Things will be initalized in a lazy fashion*/
    memset( mem, 0, size );

    p_i = p_h->iconv_tbl->iconv_idx;

    p_i->iconv_spec = (ICONV_SPEC **)    mem;
    p_i->iconv_lspc = (ICONV_LIBSPEC **)(mem + (size  = p_st->iconv_numspecobj));
    p_i->iconv_sym  = (ICONV_SYMBOL **) (mem + (size += p_st->iconv_numlspcobj));
    p_i->iconv_obj  = (ICONV_OBJECT **) (mem + (size += p_st->iconv_symbols));
    p_i->iconv_meth = (ICONV_METHODS **)(mem + (size += p_st->iconv_numobject));
    p_i->iconv_file = (ICONV_FILE **)   (mem + (size +  p_st->iconv_nummethods));

    return 1;

} /* end iconv_create_tables */


/* ======== iconv_initialize ========================================== */
/* PURPOSE:
 *	Initialize iconv structures.
 *
 *	For MP-SAFE-ness, it must be guarenteed that only one thread
 *	will enter this code at any point in time. In other words
 *	put a lock around this function call.
 *
 * RETURNS:
 * 	1 if OK
 *	0 on failure
 */

static int iconv_initialize(
    ICONV_HEAD	      * p_h
) {

    ICONV_S_TABLE     * p_st;
    ssize_t		ssize;
    ssize_t		size;
    int		        error;
    ICONV_HASH_SETTING *p_sett;
    ICONV_HASH_HEAD   * p_hhd;

    /* make sure head is not previously initialized			*/

    if ( p_h->iconv_table ) {
	return 1;
    }

    /* First we need conversions table					*/

    if ( ! ( p_st = p_h->iconv_table = iconv_readtable( ICONV_TABNAME, & ssize ) ) ) {
	return 0;
    }

    /* Verify the iconv-magic						*/
    if ( p_st->iconv_magicnum != ICONV_MAGIC ) {
	error = ELIBBAD;
error_out:
#if ICONV_MMAPTAB
	munmap( p_st, ssize );
#else
	free( p_st );
#endif
	p_h->iconv_table = 0;
	setoserror( error );
	return 0;
    }


    /* initialize iconv hash table - see if there is a resource defined	*/
    /* in the iconvtab.							*/
    if ( ! ( p_sett = iconv_find_resource( p_st, "hash_parameters" ) ) ) {
	p_sett = ( ICONV_HASH_SETTING * ) iconv_hash_defaults;
    }

    /* if iconv_nelems is 0 then the hash phase is skipped and tables	*/
    /* get created directly.						*/
    if (
	( p_sett->iconv_nelems <= 0 )
	|| ( p_sett->iconv_nelems < p_sett->iconv_max_occ )
    ) {
create_indexed:
	/* Create the tables and deal with possible errors		*/
	if ( iconv_create_tables( p_h, p_st ) ) {
	    return 1;
	}
	error = oserror();
	goto error_out;	
    }

    size = (ssize_t)
	(sizeof( ICONV_HASH_ELEM ) * ( p_sett->iconv_nelems - 1 )
	+ sizeof( ICONV_HASH_HEAD ))
    ;
#if TESTDEBUG
printf(
    "Size requested = %d - Size otherwise needed = %d\n", size,
	(ssize_t) sizeof( void * ) * (
	    p_st->iconv_numobject + p_st->iconv_symbols
	    + p_st->iconv_numspecobj + p_st->iconv_nummethods
	    + p_st->iconv_numfiles + p_st->iconv_numlspcobj
	)
);
printf(
"0 iconv_numspecobj   %d\n"
"1 iconv_numlspcobj   %d\n"
"2 iconv_symbols      %d\n"
"3 iconv_numobject    %d\n"
"4 iconv_nummethods   %d\n"
"5 iconv_numfiles     %d\n"
"- iconv_numspec      %d\n"
"- iconv_numlibspec   %d\n",
p_st->iconv_numspecobj, p_st->iconv_numlspcobj, p_st->iconv_symbols,
p_st->iconv_numobject, p_st->iconv_nummethods, p_st->iconv_numfiles,
p_st->iconv_numspec, p_st->iconv_numlibspec
);
#endif

    /* Make sure the hash table uses significantly less memory than the	*/
    /* full index job. 128 is chosen as a threshhold where the difference*/
    /* between a full indexed tables and the hash tables would be used.	*/
    if (
	( size + p_sett->iconv_thresh ) >= (ssize_t) sizeof( void * ) * (
	    p_st->iconv_numobject + p_st->iconv_symbols
	    + p_st->iconv_numspecobj + p_st->iconv_nummethods
	    + p_st->iconv_numfiles + p_st->iconv_numlspcobj
	)
    ) {
	goto create_indexed;
    }

    p_hhd = malloc( size );
    if ( ! p_hhd ) {
	/* initialize failed - back out the shared table so we don't	*/
	/* map it again if user attempts to initialize again		*/
	error = oserror();
	goto error_out;
    }

    /* Set all hash elements to zero					*/
    memset( p_hhd, 0, size );

    /* initialize hash settings						*/
    p_hhd->iconv_hash_set = p_sett;

    p_h->iconv_hashtab = p_hhd;
    
    return 1;

} /* end iconv_initialize */


/* ======== iconv_promote_from_hash =================================== */
/* PURPOSE:
 *	This will promote from hash table operation to the table
 *  driven aproach.  This will allocate all the tables followed
 *  with moving all the entries to the tables and deallocating the
 *  hash table.
 *
 *	One element is added to the parameter list for adding
 *  a record that didn't fit into the hash table.
 *
 * RETURNS:
 * 	0 on failure - ran out of memory
 *	1 on success.
 */

static int  iconv_promote_from_hash(
    ICONV_HEAD	        * p_h,
    unsigned              tblno,
    unsigned              rec_idx,
    void		* value
) {

    ICONV_S_TABLE     * p_st;
    int			i;
    ICONV_HASH_HEAD   * p_hhd = p_h->iconv_hashtab;
    ICONV_HASH_ELEM   * p_he;
    
    p_st = p_h->iconv_table;

    if ( ! iconv_create_tables( p_h, p_st ) ) {
	return 0;
    }

    /* Span all elements of the hash table and place them into the table*/
    for (
	i = p_hhd->iconv_hash_set->iconv_nelems, p_he = p_hhd->iconv_elems;
	i > 0;
	i --, p_he ++
    ) {
	/* Is element occupied ? */
	if ( p_he->iconv_data ) {
#if TESTDEBUG
printf(
    "inserting tblno %d, index %d with 0x%lx\n",
    p_he->iconv_tblno, p_he->iconv_index, p_he->iconv_data
);
#endif
	    /* transfer to the table */
	    p_h->iconv_tbl->iconv_array[
		p_he->iconv_tblno
	    ][
		p_he->iconv_index
	    ] =
		p_he->iconv_data
	    ;
	}
    }

    if ( value ) {
#if TESTDEBUG
printf(
    "Overflow - inserting tblno %d, index %d with 0x%lx\n",
    tblno, rec_idx, value
);
#endif
	p_h->iconv_tbl->iconv_array[ tblno ][ rec_idx ] = value;
    }

    /* Free the hash table */
    free( p_hhd );
    p_h->iconv_hashtab = 0;

    return 1;

} /* end iconv_promote_from_hash */

/* ======== __iconv_get_resource ====================================== */
/* PURPOSE:
 *	Get a resource from the iconvtab.
 *
 * RETURNS:
 * 	0 if resource not found.
 *	absolute pointer to resource
 */

void	* __iconv_get_resource(
    char	    * resname,
    void	   ** relo_base
) {
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return ( char * ) 0;
	}
    }

    /* Base is required if relative pointers are in the struct being	*/
    /* returned.  It is up to the caller to figure out what to do.	*/
    if ( relo_base ) {
	* relo_base = ( void * ) p_st;
    }

    return iconv_find_resource( p_st, resname );

} /* end __iconv_get_resource */


/* ======== iconv_hash_seek =========================================== */
/* PURPOSE:
 *	This will search for an element from the hash table.
 *
 * RETURNS:
 * 	0 if not found
 *	pointer value
 */

static ICONV_HASH_TYP_OF * iconv_hash_seek(
    ICONV_HEAD          * p_h,
    unsigned              tblno,	/* Table number			*/
    unsigned              rec_idx,	/* index of record in table	*/
    int                   lock		/* set to 1 if lock is locked	*/
) {

    int			  hashval;
    ICONV_HASH_HEAD     * p_hhd = p_h->iconv_hashtab;
    ICONV_HASH_SETTING	* p_sett = p_hhd->iconv_hash_set;
    int			  i = p_sett->iconv_nelems;
    int			  k = i;
    ICONV_HASH_ELEM	* p_he;
    ICONV_HASH_TYP_OF	* p_ho;

    hashval = ICONV_HASHFUNC(
	p_sett->iconv_hash_key,
	rec_idx, tblno, i
    );

    if ( ! lock ) {
	ICONV_LOCK
    }

    /* scan all the elements -						*/
    for ( p_he = p_hhd->iconv_elems + hashval; i > 0; i -- ) {

	/* If the entry is empty - the search terminates		*/
	if ( ! ( p_ho = ( ICONV_HASH_TYP_OF * ) ( p_he->iconv_data ) ) ) {
	    if ( ! lock ) {
		ICONV_UNLOCK
	    }
	    return 0;
	}

	/* Check to see is the entry is the correct one			*/
	if ( ( p_he->iconv_tblno == tblno ) && ( p_he->iconv_index == rec_idx ) ) {
	    if ( ! lock ) {
		ICONV_UNLOCK
	    }
#if TESTDEBUG
printf(
    "FOUND tblno %d, index %d with 0x%lx\n",
    tblno, rec_idx, hashval
);
#endif
	    return p_ho;
	}

#if TESTDEBUG
printf(
    "Collision finding tblno %d, index %d - %d\n",
    tblno, rec_idx, hashval
);
#endif
	/* go to the next one						*/
	hashval ++;
	if ( k <= hashval ) {
	    hashval = 0;
	    p_he = p_hhd->iconv_elems;
	} else {
	    p_he ++;
	}
    }
    
    if ( ! lock ) {
	ICONV_UNLOCK
    }
    return 0;

} /* end iconv_hash_seek */


/* ======== iconv_hash_write ========================================== */
/* PURPOSE:
 *	Write to the hash table.
 *
 * RETURNS:
 * 	1 if the write was successful.
 *      0 if there was a failure (already allocated or resource depeltion)
 */

static int iconv_hash_write(
    ICONV_HEAD          * p_h,
    unsigned              tblno,	/* Table number			*/
    unsigned              rec_idx,	/* index of record in table	*/
    void		* value,	/* value to be stored.		*/
    int                   lock		/* set to 1 if lock is locked	*/
) {

    int			  hashval;
    ICONV_HASH_HEAD     * p_hhd = p_h->iconv_hashtab;
    ICONV_HASH_SETTING	* p_sett = p_hhd->iconv_hash_set;
    int			  i = p_sett->iconv_nelems;
    int			  k = i;
    ICONV_HASH_ELEM	* p_he;

    hashval = ICONV_HASHFUNC(
	p_sett->iconv_hash_key,
	rec_idx, tblno, i
    );

    if ( ! lock ) {
	ICONV_LOCK
    }

    /* scan all the elements -						*/
    for ( p_he = p_hhd->iconv_elems + hashval; i > 0; i -- ) {

	/* If the entry is empty - the search termintes			*/
	if ( ! p_he->iconv_data ) {
	    if ( ! lock ) {
		ICONV_UNLOCK
	    }
	    /* found the empty slot - first make sure inserting it will	*/
	    /* not go beyond maximum occupancy.				*/
	    p_hhd->iconv_noccupied ++;
	    if (p_hhd->iconv_noccupied > p_sett->iconv_max_occ) {
		goto nomorespace;
	    }
	    p_he->iconv_tblno = tblno;
	    p_he->iconv_index = rec_idx;
	    p_he->iconv_data = value;
#if TESTDEBUG
printf(
    "INSERTING tblno %d, index %d with 0x%lx\n",
    tblno, rec_idx, hashval
);
#endif
	    return 1;
	}

#if TESTDEBUG
printf(
    "Collision INSERTING tblno %d, index %d with 0x%lx\n",
    tblno, rec_idx, hashval
);
#endif
	/* Check to see is the entry is the correct one			*/
	if ( ( p_he->iconv_tblno == tblno ) && ( p_he->iconv_index == rec_idx ) ) {
	    if ( ! lock ) {
		ICONV_UNLOCK
	    }
	    /* Already allocated */
	    return 0;
	}

	/* go to the next one						*/
	hashval ++;
	if ( k <= hashval ) {
	    hashval = 0;
	    p_he = p_hhd->iconv_elems;
	} else {
	    p_he ++;
	}
    }
    
    if ( ! lock ) {
	ICONV_UNLOCK
    }

nomorespace:
    if ( iconv_promote_from_hash( p_h, tblno, rec_idx, value ) ) {
	return 1;
    } else {
	return 0;
    }

} /* end iconv_hash_write */


/* ======== iconv_specsize ============================================ */
/* PURPOSE:
 *	Calculate the size of a spec.
 *
 * RETURNS:
 * 	
 */

static size_t	iconv_specsize(
    short	    ntable
) {

    return sizeof( void * ) * ntable + offsetof( ICONV_SPEC, iconv_table );

} /* end iconv_specsize */

/* ======== iconv_libspecsize ========================================= */
/* PURPOSE:
 *	Calculate the size of a spec.
 *
 * RETURNS:
 * 	
 */

static size_t	iconv_libspecsize(
    short	    ntable
) {

    return sizeof( void * ) * ntable + offsetof( ICONV_LIBSPEC, iconv_table );

} /* end iconv_libspecsize */

/* ======== iconv_init_spec =========================================== */
/* PURPOSE:
 *	Initialize an element
 *
 * RETURNS:
 * 	
 */

static ICONV_SPEC * iconv_init_spec(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE	* p_st,
    ICONV_T_SPECKEY	* s_sk
) {

    ICONV_S_DEFMKADDR
    size_t		  num;
    ICONV_S_SPEC	* s_sp;
    ICONV_SPEC		* p_sp;

    s_sp = ICONV_S_MKADDR( ICONV_S_SPEC *, p_st, s_sk->iconv_spec );
#if TESTDEBUG
printf(
    "Spec number in table is %d - number in spec rec is %d\n",
    s_sk-p_st->iconv_speckey,
    s_sp->iconv_specnum
);
#endif

    num = iconv_specsize( s_sp->iconv_ntable );

    ICONV_LOCK

    p_sp = ICONV_H_REFTBL( p_h, iconv_spec, s_sp->iconv_specnum, 1 );

    /* if already allocated - jump out					*/
    if ( p_sp ) {
	ICONV_UNLOCK
	return p_sp;
    }
    
    if ( ! ( p_sp = malloc( num ) ) ) {
	/* if malloc failed return a match - recover at bsearch return*/
	ICONV_UNLOCK
	return 0;
    }

    p_sp->iconv_s_spec = s_sp;
    p_sp->iconv_meth = 0;   /* compute the method stuff later		*/

    ICONV_H_WRITETBL( p_h, iconv_spec, s_sp->iconv_specnum, p_sp, 1, failed );

    ICONV_UNLOCK

    return p_sp;

failed:
    ICONV_UNLOCK
    free( p_sp );

    return 0;

} /* end iconv_init_spec */

/* ======== iconv_init_lib_spec ======================================== */
/* PURPOSE:
 *	Initialize an element
 *
 * RETURNS:
 * 	
 */

static ICONV_LIBSPEC * iconv_init_lib_spec(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE	* p_st,
    ICONV_T_SPECKEY	* s_sk
) {
    ICONV_S_DEFMKADDR
    size_t		  num;
    ICONV_S_LSPEC	* s_sp;
    ICONV_LIBSPEC	* p_sp;

    s_sp = ICONV_S_MKADDR( ICONV_S_LSPEC *, p_st, s_sk->iconv_spec );

    num = iconv_libspecsize( s_sp->iconv_ntable_mbwc + s_sp->iconv_ntable_wcmb );

    ICONV_LOCK

    p_sp = ICONV_H_REFTBL( p_h, iconv_lspc, s_sp->iconv_specnum, 1 );

    /* if already allocated - jump out					*/
    if ( p_sp ) {
	ICONV_UNLOCK
	return p_sp;
    }
    
    if ( ! ( p_sp = malloc( num ) ) ) {
	/* if malloc failed return a match - recover at bsearch return*/
	ICONV_UNLOCK
	return 0;
    }

    p_sp->iconv_s_spec = s_sp;
    p_sp->iconv_meth = 0;   /* compute the method stuff later		*/

    ICONV_H_WRITETBL( p_h, iconv_lspc, s_sp->iconv_specnum, p_sp, 1, failed );

    ICONV_UNLOCK

    return p_sp;

failed:
    ICONV_UNLOCK
    free( p_sp );

    return 0;

} /* end iconv_init_lib_spec */

/* ======== iconv_compar ============================================== */
/* PURPOSE:
 *	iconv comparison routine for bsearch().
 *
 * RETURNS:
 * 	<0 if key < p_sp
 * 	 0 if key == p_sp
 * 	>0 if key > p_sp
 */

static int iconv_compar(
    ICONV_COMPAR_SPEC	    * key,
    ICONV_T_SPECKEY	    * s_sk
) { 
    ICONV_S_TABLE	    * p_st;
    char		    * str;
    int			      cmp;

    ICONV_S_DEFMKADDR

    p_st = key->iconv_table;

    str = ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ ICONV_SRC ] );

    if ( ! str ) {
	if ( key->iconv_specifier[ ICONV_SRC ] ) {
	    return -1;
	}
    } else if ( ! key->iconv_specifier[ ICONV_SRC ] ) {
	return 1;
    }

    /* compare strings in iconv_specifier				*/
    if (
	cmp = strcasecmp(
	    key->iconv_specifier[ ICONV_SRC ],
	    str
	)
    ) {
	return cmp;
    }

    str = ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ ICONV_DST ] );
    
    if ( ! str ) {
	if ( key->iconv_specifier[ ICONV_DST ] ) {
	    return -1;
	}
    } else if ( ! key->iconv_specifier[ ICONV_DST ] ) {
	return 1;
    }

    return strcasecmp(
	key->iconv_specifier[ ICONV_DST ],
	str
    );

} /* end iconv_compar */


/* ======== iconv_find_s_spec ========================================= */
/* PURPOSE:
 *	Find a spec in the shared table.
 *
 * RETURNS:
 * 	0 if not found
 *	pointer to speckey if found.
 */

static ICONV_T_SPECKEY * iconv_find_s_spec(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    const char		* src_enc,
    const char		* dst_enc,
    ICONV_T_SPECKEY	* s_first_sk,
    ICONV_INT		  numspec
) {

    ICONV_COMPAR_SPEC	  a_sp[ 1 ];
    ICONV_T_SPECKEY     * s_sk;

    a_sp->iconv_head = p_h;
    a_sp->iconv_table = p_st;
    a_sp->iconv_specifier[ ICONV_SRC ] = ( char * ) src_enc;
    a_sp->iconv_specifier[ ICONV_DST ] = ( char * ) dst_enc;

    /* find the specifier with a bsearch				*/
    s_sk = bsearch(
	a_sp,
	s_first_sk,
	numspec,
	sizeof( ICONV_T_SPECKEY ),
	( bsearch_compar_t ) iconv_compar
    );

    return s_sk;

} /* end iconv_find_s_spec */

/* ======== iconv_find_spec =========================================== */
/* PURPOSE:
 *	Find a specifier
 *
 * RETURNS:
 * 	nil if not found
 *	pointer to ICONV_SPEC if found.
 */

static ICONV_SPEC * iconv_find_spec(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    const char		* src_enc,
    const char		* dst_enc,
    ICONV_T_SPECKEY    ** ps_sk
) {

    ICONV_T_SPECKEY     * s_sk;

    s_sk = iconv_find_s_spec(
	p_h, p_st, src_enc, dst_enc, p_st->iconv_speckey, p_st->iconv_numspec
    );

    if ( ps_sk ) {
	* ps_sk = s_sk;
    }
    
    /* Check the return from bsearch -					*/
    if ( s_sk ) {
	return iconv_init_spec( p_h, p_st, s_sk );
    } else {
	return 0;
    }

} /* end iconv_find_spec */

/* ======== iconv_find_lib_spec ======================================= */
/* PURPOSE:
 *	Find a specifier
 *
 * RETURNS:
 * 	nil if not found
 *	pointer to ICONV_SPEC if found.
 */

static ICONV_LIBSPEC * iconv_find_lib_spec(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    const char		* src_enc,
    const char		* dst_enc,
    ICONV_T_SPECKEY    ** ps_sk
) {

    ICONV_T_SPECKEY     * s_sk;

    s_sk = iconv_find_s_spec(
	p_h, p_st, src_enc, dst_enc,
	p_st->iconv_speckey + p_st->iconv_numspec, p_st->iconv_numlibspec
    );
    
    if ( ps_sk ) {
	* ps_sk = s_sk;
    }
    
    /* Check the return from bsearch -					*/
    if ( s_sk ) {
	return iconv_init_lib_spec( p_h, p_st, s_sk );
    } else {
	return 0;
    }

} /* end iconv_find_lib_spec */


/* ======== iconv_object_init ========================================= */
/* PURPOSE:
 *	Initialize th object
 *
 * RETURNS:
 * 	
 */

static ICONV_OBJECT	* iconv_object_init(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_T_OBJECTSPEC	* p_tos
) {

    ICONV_S_DEFMKADDR
    ICONV_OBJECT	* p_obj;
    void		* dl_handle;
    char		* str;
    
    /* Get the object spec for converter/control for this guy */

    ICONV_LOCK

    /* Make sure that the object is still not allocated			*/
    p_obj = ICONV_H_REFTBL( p_h, iconv_obj, p_tos->iconv_num, 1 );
    if ( ! p_obj  ) {

	/* Object still requires initialization				*/
	str = ICONV_S_MKADDR( char *, p_st, p_tos->iconv_name );

#if _PIC
	dl_handle = dlopen( str, RTLD_LAZY );
#else
	dl_handle = __ns_iconv_open( str );
#endif
#if TESTDEBUG
printf( "dlopen(\"%s\") = 0x%lx\n", str?str:"NIL", dl_handle );
#endif
	if ( ! dl_handle ) {
	    ICONV_UNLOCK
	    return 0;
	}

	p_obj = malloc( sizeof( ICONV_OBJECT ) );

	if ( ! p_obj ) {
errlabel2:
#if _PIC
	    if ( dl_handle ) {
		dlclose( dl_handle );
	    }
#endif
	    ICONV_UNLOCK
	    return 0;
	}

	p_obj->iconv_dlhandle = dl_handle;
	/* assign it to the appropriate places				*/
	ICONV_H_WRITETBL( p_h, iconv_obj, p_tos->iconv_num, p_obj, 1, errlabel );

    }

    ICONV_UNLOCK
    
    return p_obj;

errlabel:
    free( p_obj );
    goto errlabel2;

} /* end iconv_object_init */


/* ======== iconv_get_object ========================================== */
/* PURPOSE:
 *	Get the object referred by the iconvtab
 *
 * RETURNS:
 * 	
 */

static ICONV_OBJECT * iconv_get_object(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_S_OBJECTSPEC	  relptr
) {


    ICONV_S_DEFMKADDR
    ICONV_T_OBJECTSPEC	* p_tos;
    ICONV_OBJECT	* p_obj;
    
    /* Get the object spec for converter/control for this guy */

    p_tos = ICONV_S_MKADDR( ICONV_T_OBJECTSPEC *, p_st, relptr );

    p_obj = ICONV_H_REFTBL( p_h, iconv_obj, p_tos->iconv_num, 0 );
    if ( ! p_obj ) {
	/* Object still requires initialization				*/
	p_obj = iconv_object_init( p_h, p_st, p_tos );

	/* If we failed to get the object we have failed !		*/
	if ( ! p_obj ) {
	    return 0;
	}
    }

    return p_obj;

} /* end iconv_get_object */


/* ======== iconv_get_symbol ========================================== */
/* PURPOSE:
 *	Get a symbol from an object.
 *
 * RETURNS:
 *	( void * ) -1 on failure
 *	pointer to symbol otherwise
 * 	
 */

static void	* iconv_get_symbol(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_S_OBJECTSPEC	  objrelptr,	/* object in which to find symbol*/
    ICONV_S_SYMBOL	  relptr,
    int			  locked
) {

    ICONV_S_DEFMKADDR
    void		* p_sym;
    ICONV_T_SYMBOL	* p_s_sym;
    ICONV_SYMBOL	* p_d_sym;
    char		* s;
    ICONV_OBJECT	* p_obj;

    p_s_sym = ICONV_S_MKADDR( ICONV_T_SYMBOL *, p_st, relptr );

    if ( ! p_s_sym ) {
	return 0;
    }

    p_d_sym = ICONV_H_REFTBL( p_h, iconv_sym, p_s_sym->iconv_num, locked );
    
    if ( ! p_d_sym ) {
	
	s = ICONV_S_MKADDR( char *, p_st, p_s_sym->iconv_name );

	/* We are looking for a symbol - it's not nil !			*/
	if ( s ) {
    
	    p_obj = iconv_get_object( p_h, p_st, objrelptr );

	    /* We had a problem getting the DSO loaded			*/
	    if ( ! p_obj ) {
		setoserror(EINVAL);
		return ( void * ) (ssize_t) -1;
	    }

#if _PIC
	    p_sym = dlsym( p_obj->iconv_dlhandle, s );
#else
	    p_sym = __ns_iconv_lookup( p_obj->iconv_dlhandle, s );
#endif
#if TESTDEBUG
printf( "dlsym(0x%lx,\"%s\") = 0x%lx\n", p_obj->iconv_dlhandle, s, p_sym );
#endif
	
	    if ( ! p_sym ) {
		setoserror(EINVAL);
		return ( void * ) (ssize_t) -1;
	    }

	} else {
	    p_sym = 0;
	}

	if ( ! locked ) {
	    ICONV_LOCK
	}
	
	p_d_sym = ICONV_H_REFTBL( p_h, iconv_sym, p_s_sym->iconv_num, locked );

	if ( ! p_d_sym ) {

	    p_d_sym = malloc( sizeof( ICONV_SYMBOL ) );

	    if ( ! p_d_sym ) {
errlabel2:
		if ( ! locked ) {
		    ICONV_UNLOCK
		}
		return p_sym;
	    }

	    ICONV_H_WRITETBL(
		p_h, iconv_sym, p_s_sym->iconv_num, p_d_sym, locked, errlabel
	    );
	    
	    p_d_sym->iconv_symptr = p_sym;

	}

	if ( ! locked ) {
	    ICONV_UNLOCK
	}
	
    } else {
	p_sym = p_d_sym->iconv_symptr;
    }

    return p_sym;

errlabel:
    free( p_d_sym );
    goto errlabel2;

} /* end iconv_get_symbol */


/* ======== iconv_get_file ============================================ */
/* PURPOSE:
 *	Maps a file into memory.
 *
 * RETURNS:
 * 	
 */

static void	* iconv_get_file(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_S_RELPTR	  relptr
) {

    ICONV_S_DEFMKADDR
    void		* p_file;
    ICONV_T_FILE	* p_s_file;
    ICONV_FILE		* p_d_file;
    char		* s;
    char	       ** dirs;
    ssize_t		  siz;
    unsigned		  mxsiz = 128;
    char		* fname = alloca( mxsiz );
    ssize_t		  s1;
    ssize_t		  s2;

    p_s_file = ICONV_S_MKADDR( ICONV_T_FILE *, p_st, relptr );

    if ( ! p_s_file ) {
	setoserror(EINVAL);
	return 0;
    }

    ICONV_LOCK
    /* has this file been opened before ?				*/
    p_d_file = ICONV_H_REFTBL( p_h, iconv_file, p_s_file->iconv_num, 1 );

    if ( p_d_file ) {
	ICONV_UNLOCK
	return p_d_file->iconv_fileptr;
    }

    /* Find the string.							*/
    s = ICONV_S_MKADDR( char *, p_st, p_s_file->iconv_name );

    if ( ! s ) {
	ICONV_UNLOCK
	setoserror(EINVAL);
	return 0;
    }

    /* Absolute path - just read it from there				*/
    if ( s[ 0 ] == '/' ) {
	p_file = iconv_readtable( s, & siz );
    } else {

	/* We must have a filename in s.  We need to search directories	*/
    
	dirs = ICONV_TABDIRS;
	p_file = 0;
	while ( * dirs && ! p_file ) {

	    /* contstruct a filename from directory and filename	*/
	    s1 = (ssize_t) strlen( * dirs );
	    s2 = (ssize_t) strlen( s );
	    siz = s1 + s2 + 2;	    /* size of resultant string		*/

	    /* Allocate larger fname buffer if nessasary		*/
	    if ( siz > mxsiz ) {
		mxsiz = (unsigned) (siz + 128);
		fname = alloca( mxsiz );
	    }

	    /* contruct the file name					*/
	    memcpy( fname, * dirs, s1 );
	    fname[ s1 ] = '/';
	    memcpy( fname + s1 + 1, s, s2 + 1 );

	    /* fname now contains table	name with a search directory	*/
	    p_file = iconv_readtable( fname, & siz );
	    
	    dirs ++;
	}
    }

    /* mark the file as mapped in.					*/
    if ( p_file ) {
	p_d_file = malloc( sizeof( ICONV_FILE ) );
	if ( ! p_d_file ) {
	    /* We need to drop the file because of danger of memory	*/
	    /* leak if file is mapped multiple times.  Bummer		*/
errlabel2:
#if ICONV_MMAPTAB
	    munmap( p_file, siz );
#else
	    free( p_file );
#endif
	    return 0;
	}
	p_d_file->iconv_fileptr = p_file;
	ICONV_H_WRITETBL(
	    p_h, iconv_file, p_s_file->iconv_num, p_d_file, 1, errlabel
	);
    }
    ICONV_UNLOCK

    return p_file;

errlabel:
    free( p_d_file );
    goto errlabel2;

} /* end iconv_get_file */



/* ======== iconv_get_methods ========================================= */
/* PURPOSE:
 *	Get methods for iconv
 *
 * RETURNS:
 * 	nil on failure
 *	pointer to methods on success
 */

static ICONV_METHODS * iconv_get_methods(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_SPEC		* p_sp
) {

    ICONV_S_DEFMKADDR
    void		* p_sym;
    ICONV_METHODS	* p_mth;
    ICONV_S_SPEC	* p_s_sp;
    ICONV_T_METHOD	* p_s_mth;
    int			  i;

    if ( p_sp->iconv_meth ) {
	return p_sp->iconv_meth;
    }

    p_s_sp = p_sp->iconv_s_spec;
    p_s_mth = ICONV_S_MKADDR( ICONV_T_METHOD *, p_st, p_s_sp->iconv_meth );

    p_mth = ICONV_H_REFTBL( p_h, iconv_meth, p_s_mth->iconv_num, 0 );

    /* If the method is not initialized - initialize it !		*/
    if ( ! p_mth ) {

	ICONV_LOCK
	p_mth = ICONV_H_REFTBL( p_h, iconv_meth, p_s_mth->iconv_num, 1 );
	if ( p_mth ) {
	    ICONV_UNLOCK
	    goto opentables;
	}

	/* Must have a converter function				*/
	p_sym = iconv_get_symbol(
	    p_h, p_st, p_s_mth->iconv_algobject, p_s_mth->iconv_algorithm, 1
	);
	if ( ! p_sym || ( p_sym == ( void * ) (ssize_t) -1 ) ) {
	    ICONV_UNLOCK
	    return 0;
	}
	p_mth = malloc( sizeof( ICONV_METHODS ) );
	if ( ! p_mth ) {
	    ICONV_UNLOCK
	    return 0;
	}
	
	p_mth->iconv_converter = (ICONV_CNVFUNC) p_sym;
    
	/* Control function						*/
	p_sym = iconv_get_symbol(
	    p_h, p_st, p_s_mth->iconv_ctlobject, p_s_mth->iconv_control, 1
	);
	if ( p_sym == ( void * ) (size_t) -1 ) {
errlabel:
	    ICONV_UNLOCK
	    free( p_mth );
	    return 0;
	}
	p_mth->iconv_control = (ICONV_CTLFUNC) p_sym;

	ICONV_H_WRITETBL( p_h, iconv_meth, p_s_mth->iconv_num, p_mth, 1, errlabel );
	ICONV_UNLOCK
    }

opentables:

    for ( i = 0; i < p_s_sp->iconv_ntable; i ++ ) {
	/* Open all the tables associated with this spec		*/
	if ( ! p_s_sp->iconv_table[ i ].iconv_table ) {
	    /* This is a file only specifier				*/
	    p_sym = iconv_get_file(
		p_h, p_st, p_s_sp->iconv_table[ i ].iconv_tableobject
	    );
	    if ( ! p_sym ) {
		return 0;
	    }
    
	} else {
	    /* table is a symbol in the DSO				*/
	    p_sym = iconv_get_symbol(
		p_h, p_st,
		p_s_sp->iconv_table[ i ].iconv_tableobject,
		p_s_sp->iconv_table[ i ].iconv_table,
		0
	    );
	    if ( p_sym == ( void * ) (ssize_t) -1 ) {
		return 0;
	    }
	    if ( ! p_sym ) {
		setoserror(EINVAL);
		return 0;
	    }
	}

	p_sp->iconv_table[ i ] = p_sym;
    }

    p_sp->iconv_numtab = p_s_sp->iconv_ntable;

    /* only assign p_sp->iconv_meth when all is well.  This avoids	*/
    /* leaving the ICONV_SPEC structure in an inconsistant state.	*/
    return p_sp->iconv_meth = p_mth;

} /* end iconv_get_methods */


/* ======== iconv_get_lib_methods ===================================== */
/* PURPOSE:
 *	Get methods for mbtowc etc
 *
 * RETURNS:
 * 	nil on failure
 *	pointer to methods on success
 */

static ICONV_LIB_METHODS * iconv_get_lib_methods(
    ICONV_HEAD		* p_h,
    ICONV_S_TABLE       * p_st,
    ICONV_LIBSPEC	* p_sp
) {

    void		* p_sym;
    ICONV_LIB_METHODS	* p_mth;
    ICONV_S_LSPEC	* p_s_sp;
    int			  i;
    int			  j;

    if ( p_sp->iconv_meth ) {
	return p_sp->iconv_meth;
    }

    p_s_sp = p_sp->iconv_s_spec;

    p_mth = ( ICONV_LIB_METHODS * ) iconv_get_symbol(
	p_h, p_st, p_s_sp->iconv_obj_lmth, p_s_sp->iconv_sym_lmth, 0
    );
    
    if ( p_mth == ( ICONV_LIB_METHODS * ) (ssize_t) -1 ) {
	return 0;
    }

    if ( p_mth->iconv_magic != ICONV_MAGIC_NUM ) {
#if TESTDEBUG
printf( "MAGIC BAD got-> 0x%lx expect-> 0x%lx\n", p_mth->iconv_magic, ICONV_MAGIC_NUM );
#endif
	return 0;
    }

    p_sp->iconv_numtab_mbwc = p_s_sp->iconv_ntable_mbwc;
    p_sp->iconv_numtab_wcmb = p_s_sp->iconv_ntable_wcmb;
    j = p_s_sp->iconv_ntable_mbwc + p_s_sp->iconv_ntable_wcmb;
    
    for ( i = 0; i < j; i ++ ) {
	/* Open all the tables associated with this spec		*/
	if ( ! p_s_sp->iconv_table[ i ].iconv_table ) {
	    /* This is a file only specifier				*/
	    p_sym = iconv_get_file(
		p_h, p_st, p_s_sp->iconv_table[ i ].iconv_tableobject
	    );
	    if ( ! p_sym ) {
		return 0;
	    }
    
	} else {
	    /* table is a symbol in the DSO				*/
	    p_sym = iconv_get_symbol(
		p_h, p_st,
		p_s_sp->iconv_table[ i ].iconv_tableobject,
		p_s_sp->iconv_table[ i ].iconv_table,
		0
	    );
	    if ( p_sym == ( void * ) (ssize_t) -1 ) {
		return 0;
	    }
	    if ( ! p_sym ) {
		setoserror(EINVAL);
		return 0;
	    }
	}

	p_sp->iconv_table[ i ] = p_sym;
    }

    /* only assign p_sp->iconv_meth when all is well.  This avoids	*/
    /* leaving the ICONV_SPEC structure in an inconsistant state.	*/
    return p_sp->iconv_meth = p_mth;

} /* end iconv_get_lib_methods */



/* ======== iconv_open ================================================ */
/* PURPOSE:
 *	Open an iconv converter.
 *
 * RETURNS:
 * 	iconv_handle on success
 *	nil on failure and errno set to the error
 */

iconv_t iconv_open(
    const char		* dst_enc,
    const char		* src_enc
) {

    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    ICONV_SPEC		* p_sp;
    struct _t_iconv	* p_hnd;
    ICONV_METHODS	* p_mth;
    ICONV_T_SPECKEY	* s_sk;

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return (iconv_t) (ssize_t) -1;
	}
    }

    if ( ! ( p_sp = iconv_find_spec( p_h, p_st, src_enc, dst_enc, &s_sk ) ) ) {
	setoserror(EINVAL);
	return (iconv_t) (ssize_t) -1;
    }

    /* iconv_find_spec could also return -1, in that case we leave	*/
    /* the error value set to what it was				*/
    if ( p_sp == ( ICONV_SPEC * ) -1L ) {
	return (iconv_t) (ssize_t) -1;
    }

    /* we have found a specifier, now get the symbols and objects	*/
    /* related to this specifier.					*/

    if ( ! ( p_mth = iconv_get_methods( p_h, p_st, p_sp ) ) ) {
	return (iconv_t) (ssize_t) -1;
    }

    p_hnd = malloc( sizeof( * p_hnd ) );

    /* initialize the handle 1						*/
    if ( p_hnd ) {
	p_hnd->iconv_converter = p_mth->iconv_converter;
	p_hnd->iconv_spec = p_sp;
	p_hnd->iconv_speckey = s_sk;
	p_hnd->iconv_head = p_h;
	if ( p_mth->iconv_control ) {
	    /* initialize by control function				*/
	    if (
		( * p_mth->iconv_control )(
		    ICONV_C_INITIALIZE,
		    ( void * ) & p_hnd->iconv_state,
		    ( void * ) p_hnd,
		    p_sp->iconv_numtab,
		    p_sp->iconv_table
		)
	    ) {
		free( p_hnd );
		return (iconv_t) (ssize_t) -1;
	    }
	} else {
	    p_hnd->iconv_state = p_sp->iconv_table;
	}
	
	return (iconv_t)p_hnd;
    }
    setoserror(EINVAL);
    return (iconv_t) (ssize_t) -1;

} /* end iconv_open */


/* ======== iconv ===================================================== */
/* PURPOSE:
 *	Call the converter.
 *
 * RETURNS:
 * 	As per the method.
 */

size_t   iconv(
    iconv_t		   p_hnd,
    char		** inbuf,
    size_t		 * inbytesleft,
    char		** outbuf,
    size_t		 * outbytesleft
) {

    return ( ( ( struct _t_iconv * ) p_hnd )->iconv_converter )(
	( ( struct _t_iconv * ) p_hnd )->iconv_state,
	inbuf,
	inbytesleft,
	outbuf,
	outbytesleft
    );

} /* end iconv */


/* ======== iconv_close =============================================== */
/* PURPOSE:
 *	Release iconv resources
 *
 * RETURNS:
 * 	
 */

int iconv_close(
    iconv_t		   p_hnd
) {

    int		     retval;
    ICONV_CTLFUNC    ctlfnc;

    if (! p_hnd || ( p_hnd == ( sgiiconv_t ) -1L ) ) {
	setoserror(EBADF);
	return (int)-1;
    }
    if ( ctlfnc =
	( ( struct _t_iconv * ) p_hnd)->iconv_spec->iconv_meth->iconv_control
    ) {
	retval = ( int ) ( long ) ( * ctlfnc )(
	    ICONV_C_DESTROY,
	    & ( ( ( struct _t_iconv * ) p_hnd )->iconv_state ),
	    ( void * ) p_hnd,
	    ( ( struct _t_iconv * ) p_hnd )->iconv_spec->iconv_numtab,
	    ( ( struct _t_iconv * ) p_hnd )->iconv_spec->iconv_table
	);
    } else {
	retval = 0;
    }

    free( p_hnd );

    return retval;

} /* end iconv_close */


/* ======== __mbwc_find =============================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	* __mbwc_find(
    const char		* dst_enc,
    const char		* src_enc,
    ICONV_T_SPECKEY    ** ps_sk
) {

    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    ICONV_LIBSPEC	* p_sp;

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return (void *) (ssize_t) -1;
	}
    }

    if ( ! (
	p_sp = iconv_find_lib_spec(
	    p_h, p_st, src_enc, dst_enc, ps_sk
	)
    ) ) {
	setoserror(EINVAL);
	return (void *) (ssize_t) -1;
    }

    /* iconv_find_spec could also return -1, in that case we leave	*/
    /* the error value set to what it was				*/
    if ( p_sp == ( ICONV_LIBSPEC * ) -1L ) {
	return (void *) (ssize_t) -1;
    }

    /* we have found a specifier, now get the symbols and objects	*/
    /* related to this specifier.					*/

    if ( ! ( iconv_get_lib_methods( p_h, p_st, p_sp ) ) ) {
	return (sgiiconv_t) (ssize_t) -1;
    }

    return ( void * ) p_sp;

} /* end __mbwc_find */


/* ======== __mbwc_open =============================================== */
/* PURPOSE:
 *	Open an mbtowc converter spec - returns an _iconv_mbhandle_t
 *	If given a pointer it will return that mbhandle.
 *
 * RETURNS:
 * 	0 on failure.
 */

void	* __mbwc_open(
    ICONV_LIBSPEC	* p_sp,
    ICONV_T_SPECKEY	* s_sk,
    _iconv_mbhandle_t	  p_mbs
) {

    ICONV_LIB_METHODS	* p_mth;
#define ZEROFILL( array )   memset( array, 0, sizeof( array ) )

    if ( p_sp == ( ICONV_LIBSPEC * ) -1L ) {
	return 0;
    }

    if ( ! p_mbs ) {
	p_mbs = malloc( sizeof( struct _iconv_mbhandle_s ) );
	if ( ! p_mbs ) {
	    return 0;
	}
	ZEROFILL( p_mbs->iconv_mb );
	p_mbs->iconv_mb->iconv_allocated = 1;
    } else {
	ZEROFILL( p_mbs->iconv_mb );
    }

    p_mbs->iconv_spec = p_sp;
    p_mth = p_mbs->iconv_meths;
    p_mth[ 0 ] = * p_sp->iconv_meth;
    p_mbs->iconv_mb->iconv_speckey = s_sk;
    p_mbs->iconv_mb->iconv_head = iconv_top;
    
    if ( p_mth->iconv_mbwcctl ) {
	/* Check the return value of the control function		*/
	if (
	    ( * p_mth->iconv_mbwcctl )(
		ICONV_C_INITIALIZE,
		( void * ) & p_mbs->iconv_handle[ 0 ],
		( void * ) p_mbs,
		p_sp->iconv_numtab_mbwc,
		p_sp->iconv_table
	    )
	) {
error_out:
	    if ( p_mbs->iconv_mb->iconv_allocated ) {
		free( p_mbs );
	    }
	    return 0;
	}
    } else {
	p_mbs->iconv_handle[ 0 ] = p_sp->iconv_table;
    }
	
    if ( p_mth->iconv_wcmbctl ) {
	/* Check the return value of the control function		*/
	if ( 
	    ( * p_mth->iconv_wcmbctl )(
		ICONV_C_INITIALIZE,
		( void * ) & p_mbs->iconv_handle[ 1 ],
		( void * ) p_mbs,
		p_sp->iconv_numtab_wcmb,
		p_sp->iconv_table + p_sp->iconv_numtab_mbwc
	    )
	) {
	    /* destroy whatever came back above */
	    ( * p_mth->iconv_mbwcctl )(
		ICONV_C_DESTROY,
		( void * ) & p_mbs->iconv_handle[ 0 ],
		( void * ) p_mbs,
		p_mbs->iconv_spec->iconv_numtab_mbwc,
		p_mbs->iconv_spec->iconv_table
	    );
	    goto error_out;
	}
    } else {
	p_mbs->iconv_handle[ 0 ] = p_sp->iconv_table;
    }
	
    return p_mbs;

} /* end __mbwc_open */


/* ======== __mbwc_close ============================================== */
/* PURPOSE:
 *	Close mbstate.
 *
 * RETURNS:
 * 	0 if OK
 */

int __mbwc_close(
    _iconv_mbhandle_t	  p_mbs
) {

    int		     retval = 0;
    ICONV_CTLFUNC    ctlfnc;

    if (! p_mbs || ( p_mbs == ( _iconv_mbhandle_t ) -1L ) ) {
	setoserror(EBADF);
	return -1;
    }

    /* If this is the default mbhandle - get out now !			*/
    if ( ! p_mbs->iconv_spec ) {
	return 0;
    }
    
    if ( ctlfnc = p_mbs->iconv_meths->iconv_mbwcctl ) {
	if (
	    ( * ctlfnc )(
		ICONV_C_DESTROY,
		( void * ) & p_mbs->iconv_handle[ 0 ],
		( void * ) p_mbs,
		p_mbs->iconv_spec->iconv_numtab_mbwc,
		p_mbs->iconv_spec->iconv_table
	    )
	) {
	    retval = 1;
	}
    }
    
    if ( ctlfnc = p_mbs->iconv_meths->iconv_wcmbctl ) {
	if (
	    ( * ctlfnc )(
		ICONV_C_DESTROY,
		( void * ) & p_mbs->iconv_handle[ 1 ],
		( void * ) p_mbs,
		p_mbs->iconv_spec->iconv_numtab_wcmb,
		p_mbs->iconv_spec->iconv_table
		    + p_mbs->iconv_spec->iconv_numtab_mbwc
	    )
	) {
	    retval = 1;
	}
    }

    /* Free this if it was allocated by this !				*/
    if ( p_mbs->iconv_mb->iconv_allocated ) {
	free( p_mbs );
    }

    return retval;

} /* end __mbwc_close */


/* ======== __mbwc_setmbstate ========================================= */
/* PURPOSE:
 *	Set the default mbstate
 *
 * RETURNS:
 * 	0 on success -1 on failure
 */

int	__mbwc_setmbstate(
    const char	    * loc
) {
    void	    * p_sp;
    char	    * wcenc = "irix-wc";
    ICONV_S_RELPTR  * wcenc_rp;
    char	    * baseaddr;
    ICONV_T_SPECKEY * s_sk;
    int             * magic_resvals;

    /* Make it so that the wide encoding name can be changed for the	*/
    /* system.								*/
    /* Magic number (int) is at the head of a group of resouce values.  */
    /* Alignment can be ignored because it's int type.                  */
    /*                                                                  */
    magic_resvals = __iconv_get_resource(
	"wide_encoding_name", ( void ** ) & baseaddr
    );

    if ( magic_resvals ) {

         if ( *magic_resvals != ICONV_MAGIC_RESVALS ) return -1;
         wcenc_rp = magic_resvals + 1;

         if ( wcenc_rp ) {
	      wcenc = * wcenc_rp + baseaddr;
         }

#ifdef TESTDEBUG2
	 printf( "*magic_resvals: 0x%x\n", *magic_resvals );
	 printf( "resource value (__mbwc_setmbstate): %s\n", wcenc );
         fflush( stdout );
#endif
    }

    if ( ( p_sp = __mbwc_find( wcenc, loc, &s_sk ) ) == ( void * ) -1L ) {
	char	    * s;

	/* The spec was found, - it was not able to be opened		*/
	if ( s_sk ) {
	    return -1;
	}
	if ( s = __mbwc_locale_codeset( loc ) ) {
	    if ( ( p_sp = __mbwc_find( wcenc, s, &s_sk ) ) == ( void * ) -1L ) {
		return -1;
	    }
	} else {
	    return -1;
	}
    }

    __mbwc_close( __libc_mbhandle );

    __mbwc_open( p_sp, s_sk, __libc_mbhandle );

    return 0;

} /* end __mbwc_setmbstate */


/* ======== __mbwc_gettable =========================================== */
/* PURPOSE:
 *	Get a table of all mbwc converters.
 *
 * RETURNS:
 * 	Pointer nul terminated to a list of char * tuples.
 */

char	** __mbwc_gettable()
{

    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    ICONV_T_SPECKEY     * s_sk;
    ICONV_S_DEFMKADDR

    int			  i;
    static char	       ** p_cs = 0; /* Converter strings		*/
    char	       ** p_tmp = 0; /* Converter strings		*/

    /* Return the table previously computed table if we have one	*/
    if ( p_cs ) {
	return p_cs;
    }

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    return 0;
	}
    }

    p_tmp = p_cs = malloc( p_st->iconv_numlibspec * sizeof( char ** ) * 2 + 1 );

    if ( ! p_tmp ) {
	return p_tmp;
    }

    s_sk = p_st->iconv_speckey + p_st->iconv_numspec;

    for ( i = 0; i < p_st->iconv_numlibspec; i ++, s_sk ++ ) {
	* ( p_tmp ++ ) =
	     ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 0 ] )
	;

	* ( p_tmp ++ ) =
	     ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 1 ] )
	;
    }

    * p_tmp = 0;
    
    return p_cs;

} /* end __mbwc_gettable */


/* ======== __mbwc_alias_locale_cmp =================================== */
/* PURPOSE:
 *	bsearch alias compare
 *
 * RETURNS:
 * 	
 */

static int  __mbwc_alias_locale_cmp(
    const char * loc, const ICONV_T_LOC_ALIAS * p_la
) {
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;
    char		    * str;
    ICONV_S_DEFMKADDR

    p_h = iconv_top;
    p_st = p_h->iconv_table;

    str = ICONV_S_MKADDR( char *, p_st, p_la->iconv_locale );

    if ( str ) {
	return strcmp( loc, str );
    }

    return -1;

} /* end __mbwc_alias_locale_cmp */


/* ======== __mbwc_locale_cmp ========================================= */
/* PURPOSE:
 *	Locale comparator
 *
 * RETURNS:
 * 	Comparison value
 */

static int __mbwc_locale_cmp(
    const char * loc, const ICONV_T_LOC_CODESET * p_lc
) {
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;
    char		    * str;
    ICONV_S_DEFMKADDR

    p_h = iconv_top;
    p_st = p_h->iconv_table;

    str = ICONV_S_MKADDR( char *, p_st, p_lc->iconv_locale );

    if ( str ) {
	return strcmp( loc, str );
    }

    return -1;

} /* end __mbwc_locale_cmp */

/* ======== __mbwc_locale_alias ======================================= */
/* PURPOSE:
 *	Get a locale alias
 *
 * RETURNS:
 * 	
 */

char	* __mbwc_locale_alias( const char * loc )
{
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;
    ICONV_S_ALIAS_LIST	    * p_al;
    ICONV_T_LOC_ALIAS	    * p_la;
    ICONV_S_DEFMKADDR

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return ( char * ) 0;
	}
    }

    p_al = ICONV_S_MKADDR(
	ICONV_S_ALIAS_LIST *, p_st, p_st->iconv_alias_list
    );

    /* find the specifier with a bsearch				*/
    p_la = bsearch(
	loc,
	p_al->iconv_aliases,
	p_al->iconv_count,
	sizeof( ICONV_T_LOC_ALIAS ),
	( bsearch_compar_t ) __mbwc_alias_locale_cmp
    );

    if ( p_la ) {
	return ICONV_S_MKADDR( char *, p_st, p_la->iconv_value );
    }

    return ( char * ) 0;

} /* end __mbwc_locale_alias */



/* ======== __mbwc_locale_codeset ===================================== */
/* PURPOSE:
 *	Get a locale codeset
 *
 * RETURNS:
 * 	
 */

char	* __mbwc_locale_codeset( const char * loc )
{
    ICONV_HEAD		    * p_h;
    ICONV_S_TABLE	    * p_st;
    ICONV_S_CODESET_LIST    * p_cl;
    ICONV_T_LOC_CODESET	    * p_lc;
    ICONV_S_DEFMKADDR

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return ( char * ) 0;
	}
    }

    p_cl = ICONV_S_MKADDR(
	ICONV_S_CODESET_LIST *, p_st, p_st->iconv_codeset_list
    );

    /* find the specifier with a bsearch				*/
    p_lc = bsearch(
	loc,
	p_cl->iconv_codesets,
	p_cl->iconv_count,
	sizeof( ICONV_T_LOC_CODESET ),
	( bsearch_compar_t ) __mbwc_locale_cmp
    );

    if ( p_lc ) {
	return ICONV_S_MKADDR( char *, p_st, p_lc->iconv_value );
    }

    return ( char * ) 0;

} /* end __mbwc_locale_codeset */



/* ======== __iconv_gettable ========================================== */
/* PURPOSE:
 *	Get a table of all converters.
 *
 * RETURNS:
 * 	
 */

char	** __iconv_gettable( void )
{

    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    ICONV_T_SPECKEY     * s_sk;
    ICONV_S_DEFMKADDR
    
    int			  i;
    static char	       ** p_cs = 0; /* Converter strings		*/
    char	       ** p_tmp = 0; /* Converter strings		*/

    /* Return the table previously computed if we have one		*/
    if ( p_cs ) {
	return p_cs;
    }

    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    return 0;
	}
    }

    p_tmp = p_cs = malloc( p_st->iconv_numspec * sizeof( char ** ) * 2 + 1 );

    if ( ! p_tmp ) {
	return p_tmp;
    }

    s_sk = p_st->iconv_speckey;

    for ( i = 0; i < p_st->iconv_numspec; i ++, s_sk ++ ) {
	* ( p_tmp ++ ) =
	     ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 0 ] )
	;

	* ( p_tmp ++ ) =
	     ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 1 ] )
	;
    }

    * p_tmp = 0;
        
    return p_cs;

} /* end __iconv_gettable */


/* ======== __iconv_open_reference  ========================================== */
/* PURPOSE:
 *	Open a reference.
 *
 * RETURNS:
 * 	
 */

void       *  __iconv_open_reference( ICONV_REFER  * p_ref )
{
    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    void                * s;

    p_h = iconv_top;

    if ( !p_ref ) {
	    setoserror(EINVAL);
	    return (void *)(ssize_t)-1;
    }

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    setoserror(EINVAL);
	    return (void *)(ssize_t)-1;
	}
    }

    switch ( p_ref->iconv_node_type ) {

        case ICONV_REFER_FILE:

             s = iconv_get_file( p_h, p_st, p_ref->_objs._file.iconv_file_ref );
             return ( s ? s : (void *)(ssize_t)-1 ); 

	case ICONV_REFER_SYMBOL:

	     return iconv_get_symbol( p_h, p_st, p_ref->_objs._symbols.iconv_object,
                                                 p_ref->_objs._symbols.iconv_symbol,
                                      0 );
    }

    setoserror(EINVAL);
    return (void *)(ssize_t)-1;

} /* end __iconv_open_reference */


#if TESTDEBUG
int iconv_test()
{
    ICONV_HEAD		* p_h;
    ICONV_S_TABLE       * p_st;
    ICONV_SPEC	        * p_sp;
    int			  i;
    char		* st0, * st1;
    ICONV_T_SPECKEY	* s_sk;
    ICONV_S_DEFMKADDR


    p_h = iconv_top;

    /* initialize iconv if nessasary					*/
    if ( ! ( p_st = p_h->iconv_table ) ) {
	ICONV_LOCK
	iconv_initialize( p_h );
	ICONV_UNLOCK
	if ( ! ( p_st = p_h->iconv_table ) ) {
	    return 0;
	}
    }

    for ( i = 0; i < p_st->iconv_numspec; i ++ ) {

	p_sp = iconv_init_spec( p_h, p_st, s_sk = p_st->iconv_speckey + i );
	
	if ( ! p_sp ) {
	    return 0;
    	}
	
	st0 = ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 0 ] );
	st1 = ICONV_S_MKADDR( char *, p_st, s_sk->iconv_specifier[ 1 ] );
    
	printf(
	    " '%s' - '%s'\n", st0, st1
	);
    }
    
    return 1; 
}
#endif
