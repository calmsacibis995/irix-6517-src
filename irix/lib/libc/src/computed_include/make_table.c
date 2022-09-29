/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * make_table.c  5-Aug-1996
 *
 * Generate efficiently searched (sorted) tables from a list of strings
 * in a file. File format is one line per symbol.  If there is only
 * one word on the line the word is used as both the name of the symbol
 * as well as the symbol reference in the table.  If two words appear
 * on the line, the first word is the name of the table entry and the
 * second is the symbol being referenced.
 *
 * Example 1:
 * file contains line: "object_symbol"
 * gets converted to : "{ "object_symbol", ( void * ) object_symbol },"
 *
 * Example 2:
 * file contains line: "alias_smith object_symbol"
 * gets converted to : "{ "alias_smith", ( void * ) object_symbol },"
 *
 * The table is easily searched using bsearch().  However, at some
 * later time, the table may become a hashed table so don't count on it
 * being sorted.
 *
 * Usage : make_table file_with_strings output_file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <malloc.h>  
#include <sys/stat.h>
#include <errno.h>

const char usage[] = "%s: <strings_file> <output_file>";


/* ======== mktb_str ================================================== */
/* PURPOSE:
 *	String struct
 */

typedef struct  mktb_str {
    int		  mktb_len;	/* String length			*/
    char	* mktb_s;	/* Pointer to string			*/
    int		  mktb_symlen;	/* Symbol name length			*/
    char	* mktb_sym;	/* Pointer to symbol name		*/
} mktb_str;


/* ======== mktb_getstrings =========================================== */
/* PURPOSE:
 *	Get strings from file, place string references in an array
 *  allocated.
 *
 * RETURNS:
 * 	
 */

int	mktb_getstrings(
    char	* filename,
    mktb_str   ** pp_strs,
    int		* p_nstrs,
    char       ** p_errstr
) {
    char		* p_head;
    struct stat		  a_buf[1];
    long		  len;
    int			  fd;
    register char	* fc;
    register char	* fs;
    register long	  fl;
    char		  c;
    int			  ntpl;
    mktb_str	        * p_strs;
    int		          state;

    /* Attempt to open the file						*/
    fd = open( filename, O_RDONLY );

    /* Did we succeed in the open ?					*/
    if ( fd < 0 ) {
	if ( p_errstr ) * p_errstr = strerror( oserror() );
	return 0;
    }

    if ( fstat( fd, a_buf ) == -1 ) {
	if ( p_errstr ) * p_errstr = strerror( oserror() );
	close( fd );
	return 0;
    }

    if ( ! a_buf->st_size ) {
	* p_nstrs = 0;
	* pp_strs = malloc( 1 );
	* p_errstr = 0;
	close( fd );
	return 1;
    }

    /* align the length to a system page boundary			*/
    len = ( 1+( (a_buf->st_size-1)|(getpagesize()-1) ) );
    
    /* Map it to memory							*/
    p_head = ( char * ) mmap(
	( char * ) 0,		/* Address to map to			*/
	len,			/* Length of the mapping		*/
	PROT_READ,		/* Map for read				*/
	MAP_PRIVATE,		/* Map my own private copy		*/
	fd,
	0
    );

    if ( -1l == ( long ) p_head ) {
	if ( p_errstr ) * p_errstr = strerror( oserror() );
	close( fd );
	return 0;
    }
    close( fd );

    fl = a_buf->st_size;
    ntpl = 0;
    fc = p_head;

    while ( fl ) {
	fl --;
	c = * fc ++;
	if ( c == '\n' ) {
	    ntpl ++;
	}
    }

    if ( c != '\n' ) {
	ntpl ++;
    }
    
    p_strs = malloc( sizeof( * p_strs ) * ntpl );
    if ( ! p_strs ) {
	if ( p_errstr ) * p_errstr = strerror( oserror() );
	return 0;
    }

    fl = a_buf->st_size;
    fc = p_head;
    ntpl = 0;
    fs = fc;
    state = 0;

    while ( fl ) {
	fl --;
	c = * fc;
	if ( c == '\n' ) {
	    if ( state == 0 ) {
		p_strs[ ntpl ].mktb_s = fs;
		p_strs[ ntpl ].mktb_len = fc - fs;
	    }
	    p_strs[ ntpl ].mktb_sym = fs;
	    p_strs[ ntpl ].mktb_symlen = fc - fs;
	    fs = fc + 1;
	    state = 0;
	    ntpl ++;
	} else if ( ( c == ' ' ) || ( c == '\t' ) ) {
	    if ( state == 0 ) {
		p_strs[ ntpl ].mktb_s = fs;
		p_strs[ ntpl ].mktb_len = fc - fs;
	    }
	    fs = fc + 1;
	    state = 1;
	}
	fc ++;
    }

    if ( c != '\n' ) {
	if ( state == 0 ) {
	    p_strs[ ntpl ].mktb_s = fs;
	    p_strs[ ntpl ].mktb_len = fc - fs + 1;
	}
	p_strs[ ntpl ].mktb_s = fs;
	p_strs[ ntpl ].mktb_len = fc - fs + 1;
	ntpl ++;
    }

    * pp_strs = p_strs;
    * p_nstrs = ntpl;
    * p_errstr = 0;

    return 1;

} /* end mktb_getstrings */


/* ======== mktb_compar =============================================== */
/* PURPOSE:
 *	Comparison routine
 *
 * RETURNS:
 * 	
 */

int	mktb_compar(
    mktb_str	* p_a,
    mktb_str	* p_b
) {

    int		siz;

    if ( p_a->mktb_len > p_b->mktb_len ) {
	siz = p_b->mktb_len;
    } else {
	siz = p_a->mktb_len;
    }

    if ( siz = strncmp( p_a->mktb_s, p_b->mktb_s, siz ) ) {
	return siz;
    }

    return p_a->mktb_len - p_b->mktb_len;

} /* end mktb_compar */



/* ======== mktb_makeout ============================================== */
/* PURPOSE:
 *	Create the output required
 *
 * RETURNS:
 * 	
 */

int	mktb_makeout(
    FILE	* fout,
    mktb_str    * p_strs,
    int		  p_nstrs
) {

    int		  i;

    qsort(
	p_strs,
	p_nstrs,
	sizeof( * p_strs ),
	( int (*) (const void *, const void *) ) mktb_compar
    );

    for ( i = 0; i < p_nstrs; i ++ ) {
	fprintf(
	    fout, "    { \"%.*s\",\t( void * ) %.*s },\n",
	    p_strs[ i ].mktb_len, p_strs[ i ].mktb_s,
	    p_strs[ i ].mktb_symlen, p_strs[ i ].mktb_sym
	);
    }

    return 1;

} /* end mktb_makeout */



int main( int argc, char ** argv )
{
    mktb_str    * p_strs;
    int		  nstrs;
    char        * errstr = 0;
    FILE	* fout;

    if ( argc != 3 ) {
	fprintf( stderr, "Expecting 2 arguments\n" );
	fprintf( stderr, usage, argv[ 0 ] );
	exit( 1 );
    }

    mktb_getstrings( argv[ 1 ], & p_strs, & nstrs, & errstr );

    if ( errstr ) {
	fprintf( stderr, "%s: %s\n", argv[ 1 ], errstr );
	exit( 1 );
    }

    fout = fopen( argv[ 2 ], "w" );
    if ( ! fout ) {
	perror( argv[ 2 ] );
	exit( 1 );
    }

    mktb_makeout( fout, p_strs, nstrs );

    fclose( fout );

    exit( 0 );
}
