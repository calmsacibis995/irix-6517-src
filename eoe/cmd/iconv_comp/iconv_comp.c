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
 * iconv_comp.c
 *
 * Iconv spec compiler
 *
 */

#ifndef TESTDEBUG
#define TESTDEBUG   0
#endif

#define ptr_diff_t  unsigned long

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <ctype.h>
#include <wchar.h>
#include "iconv.h"
#include "iconv_cnv.h"
#include "iconv_int.h"
#include "in_proc.h"
#include "iconv_comp.h"
#include "iconv_comp.c.h"

#define OBJ( XX )   ( ( ICC_OBJ_TYP * ) ( XX ) )
#define offsetof( typ, fld )	( (long) ( ( (typ *) 0 )->fld ) )
#define offsetofb( typ, fld )	( (long) ( &( (typ *) 0 )->fld ) )

ICC_HEAD	icc_hd[ 1 ];

extern ICC_TYPEDEF     icc_typetab[];
extern int     icc_ntypetab;

/* =============================== ALEX_TABLE ======================== */
/* error table
 */    
#define	ALEX_OFFS	0x300
char	* ALEX_TABLE[] = {
    (char *) ALEX_OFFS,
#define ALEX_NO_ERROR				( ALEX_OFFS + 1 )
    "No error",
#define ALEX_PARSER				( ALEX_OFFS + 2 )
    "Parsing error -",
#define ALEX_ESCAPE				( ALEX_OFFS + 3 )
    "Escape unnessasary - converted to",
#define ALEX_STROVR				( ALEX_OFFS + 4 )
    "String too long. String truncated",
#define ALEX_UNKTOKEN				( ALEX_OFFS + 5 )
    "Incorrect input token",
#define ALEX_OVERFLOW				( ALEX_OFFS + 6 )
    "Overflow occured in conversion",
#define ALEX_PREVIOUS				( ALEX_OFFS + 7 )
    "Previously defined - this one ignored",
#define ALEX_FOUND_HERE				( ALEX_OFFS + 8 )
    "Previous defined here",
#define ALEX_TOODEEP				( ALEX_OFFS + 9 )
    "Clone and Joins nest too deep",
#define ALEX_NONULLSPEC				( ALEX_OFFS + 10 )
    "Null encoding specifiers are not allowed",
#define ALEX_LEXERROR				( ALEX_OFFS + 11 )
    "Invalid input token",
#define ALEX_TYPEWRONG				( ALEX_OFFS + 12 )
    "Invalid type specifier",
#define ALEX_CNV_ERROR				( ALEX_OFFS + 13 )
    "Error in converting to integer",
#define ALEX_EXPECT_STRING			( ALEX_OFFS + 14 )
    "Type specified non string type but recieved string",
#define ALEX_EXPECT_INTEGER			( ALEX_OFFS + 15 )
    "Type specified non integer type but recieved integer",
#define ALEX_ALREADY_DEFINED			( ALEX_OFFS + 16 )
    "resource already defined:",
#define ALEX_ALREADY_HERE			( ALEX_OFFS + 17 )
    "resource previously defined here:",
#define ALEX_NOT_DEFINED			( ALEX_OFFS + 18 )
    "resource referenced but not defined",
};

/* ----------------------- ALEX_CON_OCTAL ----------------------- */
/* convert a octal string to a long value
 *
 */

int alex_con_octal(
    char 		* s,		/* the string */
    register int	  l,		/* the length */
    long		* val		/* the address to put the value */
) {
    register	int	i;
    int			z = 1;
    unsigned long	y = 0;

    while ( l -- ) {		/* for every character */

	i = toascii( * (s++) );

	if ( i == 'L' || i == 'l' ) {

	    if ( l ) {
		goto baderr;
	    }

	} else {
	    /* are we going to overflow ? */
	    if ( y & ( 0xe0000000 ) && z ) {
		z = 0;
		inx_error(
		    inx_last,
		    ALEX_TABLE,
		    ALEX_OVERFLOW,
		    INX_LINE | INX_WARN
		);
	    }
	}

	if ( isdigit( i ) ) {			/* normal 0-9 */
	    y <<= 3;	/* Multiply by 8 */
	
	    if ( i > '7' ) {			/* is it an octal digit */
		/* no octal digit */
		inx_error(
		    inx_last,
		    ALEX_TABLE,
		    ALEX_UNKTOKEN,
		    INX_LINE | INX_ERROR | INX_EXTRA,
		    "octal value"
		);
		* val = 0;
		return 0;
	    }

	    y += ( i - '0' );

	} else if ( !l && ( i == 'L' || i == 'l' ) ) {
	    /* must me last charcter defining a long */
	} else {

baderr:
	    inx_error(
		inx_last,
		ALEX_TABLE,
		ALEX_UNKTOKEN,
		INX_LINE | INX_ERROR | INX_EXTRA,
		"decimal value"
	    );
	    * val = 0;
	    return 0;
	}
    } /* while ( l -- ) */

    /* deposit the value and return true for success */
    * val = y;
    return 1;
}

/* ============================= alex_string ============================ */
/* find what the valuw of the escape character is
 */
    
unsigned char alex_escapeof( text )
char	** text;
{
    char	* s;			/* the string */
    long	  i = 0;		/* the value */
    int		  len;			/* length of str */
    char	  str[ 4 ];		/* for error messages */

    /* an escape char was preceeding this one */
    if ( isdigit( ** text ) ) {
	len = 1;
	s = (* text) + 1;

	while ( isdigit( * s ) ) {
	    /* must be an octal sequence */
	    s ++;
	    len ++;
	    if ( len >= 3 ) {
		break;
	    }
	}

	alex_con_octal( * text, len, &i );

	* text = s - 1;

    } else switch ( ** text ) {
	case 'n' : {		/* newline escape */
	    i = '\n';
	    break;
	}
	case 't' : {		/* tab */
	    i = '\t';
	    break;
	}
	case 'r' : {		/* carrige return */
	    i = '\r';
	    break;
	}
	case 'b' : {		/* back space */
	    i = '\b';
	    break;
	}
	case 'f' : {		/* form feed */
	    i = '\f';
	    break;
	}
	case '\\' : {		/* the real escape '\' */
	    i = '\\';
	    break;
	}
	case '\'' : {		/* the single quote */
	    i = '\'';
	    break;
	}
	case '"' : {		/* the single quote */
	    i = '"';
	    break;
	}
	case '>' : {		/* the single quote */
	    i = '>';
	    break;
	}
	case '^' : {		/* the single quote */
	    i = '^';
	    break;
	}
	case '~' : {		/* the single quote */
	    i = '~';
	    break;
	}
	case '-' : {		/* the single quote */
	    i = '-';
	    break;
	}
	default : {
	    /* escape character is not valid - however change it to
	     * a normal charater and insert it - inform the user */

	    str[ 2 ] = str[ 0 ] = '\'';
	    i = str[ 1 ] = **text;
	    str[ 3 ] = 0;

	    inx_error(
		inx_last,		/* the last position */
		ALEX_TABLE,
		ALEX_ESCAPE,
		INX_LINE | INX_INFORM | INX_EXTRA,
		str			/* show what has happened */
	    );
	    break;
	}
    } /* switch */
    

    return i;
}

/* ============================= alex_string ============================ */
/* have something of the form ( "..." )
 */
void icc_fixstr( void )
{

#define MAXSTR	1024
    char	  stacktext[ MAXSTR ];
    int		  length;
    char	* text;
    char	* ptext;
    

    text = stacktext;

    ptext = yytext + 1;
    length = 0;

    /* go through the chars */
    while ( * ptext != '"' ) {
	/* check string length */
	if ( length >= MAXSTR ) {
	    inx_error(
		inx_last,
		ALEX_TABLE,
		ALEX_STROVR,
		INX_LINE | INX_ERROR
	    );
	    break;
	}

	if ( * ptext == '\\' ) {
	    ptext ++;
	    if ( *ptext == '\n' ) {
		ptext ++;
		continue;
	    }
	    * text = alex_escapeof( &ptext );
	} else {
	    * text = * ptext;
	}
	text ++;
	ptext ++;
	length ++;
    } 

    /* allocate memory for it */
    ptext = inx_alloc( length + 1 );

    memcpy( ptext, stacktext, length );
    ptext[ length ] = 0;

    yylval = ( void * ) ptext;
    yyleng = 0;

    return ;
}


/* ======== icc_fixident ============================================== */
/* PURPOSE:
 *	Fix identifie
 *
 * RETURNS:
 * 	
 */

void	icc_fixident()
{

    char	* ptext;

    int		  length = yyleng;

    ptext = inx_alloc( length + 1 );

    memcpy( ptext, yytext, length + 1 );

    yylval = ( void * ) ptext;
    yyleng = 0;

    return;

} /* end icc_fixident */



/* ======== lexerror ================================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	lexerror()
{
    inx_error(
	inx_last,
	ALEX_TABLE,
	ALEX_LEXERROR,
	INX_LINE | INX_ERROR
    );

} /* end lexerror */



/* ============================= YYERROR =============================== */
/* Yacc goes here when a syntax error occurs - or a stack overflow
 *
 */

void yyerror(
    const char * s
) {
    inx_error(
	inx_last,
	ALEX_TABLE,
	ALEX_PARSER,
	INX_LINE | INX_EXTRA | INX_ERROR,
	s
    );
}

int yywrap()
{
	return 1;
}

/* ==================================================================== */
/* ==================================================================== */
/* Function for manipulating lists					*/


/* ======== icc_compar_string ========================================= */
/* PURPOSE:
 *	Compare strings.
 *
 * RETURNS:
 * 	
 */

int	icc_compar_string(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    /* make sure the string actually exists before calling strcmp	*/
    if ( ! p_o_a->icc_data1 ) {
	if ( p_o_b->icc_data1 ) {
	    return 1;
	} else {
	    return 0;
	}
    } else if ( ! p_o_b->icc_data1 ) {
	return -1;
    }

    return strcmp( p_o_a->icc_data1, p_o_b->icc_data1 );

} /* end icc_compar_string */



/* ======== icc_compar_meth =========================================== */
/* PURPOSE:
 *	Method comparator
 *
 * RETURNS:
 * 	
 */

#define CMPIT( TYP, XX )						\
    val = (								\
	((ptr_diff_t) (( TYP * )(p_o_a))->XX) -				\
	((ptr_diff_t) (( TYP * )(p_o_b))->XX)				\
    );									\
    if ( val ) {							\
	return val > 0 ? 1 : -1;					\
    }									\
/* Macro end								*/

int	icc_compar_meth(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_METHODS, icc_aobj )
    CMPIT( ICC_METHODS, icc_cnv )
    CMPIT( ICC_METHODS, icc_cobj )
    CMPIT( ICC_METHODS, icc_ctl )

    return 0;

} /* end icc_compar_meth */


/* ======== icc_compar_clone ========================================== */
/* PURPOSE:
 *	Compare for clone object
 *
 * RETURNS:
 * 	
 */

int	icc_compar_clone(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_CLONEOBJ, icc_src_dst )
    CMPIT( ICC_CLONEOBJ, icc_enc )
    CMPIT( ICC_CLONEOBJ, icc_mth )
    CMPIT( ICC_CLONEOBJ, icc_newenc )

    return 0;

} /* end icc_compar_clone */


/* ======== icc_compar_join =========================================== */
/* PURPOSE:
 *	Compare join records
 *
 * RETURNS:
 * 	
 */

int	icc_compar_join(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_JOINOBJ, icc_dst_enc )
    CMPIT( ICC_JOINOBJ, icc_src_enc )
    CMPIT( ICC_JOINOBJ, icc_mth_dst )
    CMPIT( ICC_JOINOBJ, icc_mth_src )

    return 0;

} /* end icc_compar_join */


/* ======== icc_compar_libspec ======================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

int	icc_compar_libspec(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_STDLIBSPEC, icc_from_enc )
    CMPIT( ICC_STDLIBSPEC, icc_to_enc )

    return 0;

} /* end icc_compar_libspec */


/* ======== icc_compar_libmeth ======================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

int	icc_compar_libmeth(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;
    int		      i;

    CMPIT( ICC_STDLIBMETH, icc_mbwc_obj )
    CMPIT( ICC_STDLIBMETH, icc_mbwc_sym )
    CMPIT( ICC_STDLIBMETH, icc_ntable_mbwc )
    CMPIT( ICC_STDLIBMETH, icc_ntable_wcmb )

    i = (( ICC_STDLIBMETH * )(p_o_a))->icc_ntable_mbwc
	+ (( ICC_STDLIBMETH * )(p_o_a))->icc_ntable_wcmb;

    if ( i ) {
	return memcmp(
	    (( ICC_STDLIBMETH * )(p_o_a))->icc_table,
	    (( ICC_STDLIBMETH * )(p_o_b))->icc_table,
	    sizeof( (( ICC_STDLIBMETH * )(p_o_a))->icc_table[ 0 ] ) * i
	);
    }
    return 0;

} /* end icc_compar_libmeth */



/* ======== icc_compar_spec =========================================== */
/* PURPOSE:
 *	Compare spec records
 *
 * RETURNS:
 * 	
 */

int	icc_compar_spec(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_SPEC, icc_from_enc )
    CMPIT( ICC_SPEC, icc_to_enc )

    return 0;

} /* end icc_compar_spec */



/* ======== icc_compargeneric ========================================= */
/* PURPOSE:
 *	Generic comparison function
 *
 * RETURNS:
 * 	
 */

int icc_compargeneric(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_OBJ_TYP, icc_data1 )
    CMPIT( ICC_OBJ_TYP, icc_data2 )

    return 0;

} /* end icc_compargeneric */

/* ======== icc_compar_1_generic ========================================= */
/* PURPOSE:
 *	Generic comparison function
 *
 * RETURNS:
 * 	
 */

int icc_compar_1_generic(
    ICC_OBJ_TYP	    * p_o_a,
    ICC_OBJ_TYP	    * p_o_b
) {

    ptr_diff_t	      val;

    CMPIT( ICC_OBJ_TYP, icc_data1 )

    return 0;

} /* end icc_compar_1_generic */




/* ======== icc_findelem ============================================== */
/* PURPOSE:
 *	Find an element in the list
 *
 * RETURNS:
 * 	
 */

void * icc_findelem(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_o
) {

    ICC_CMPFUNC	      p_func = p_oh->icc_cmpfunc;
    ICC_OBJ_TYP	    * p_obj;

    p_obj = p_oh->icc_head;

    while ( p_obj ) {
	if ( ! ( p_func )( p_o, p_obj ) ) {
	    return ( void * ) p_obj;
	}
	p_obj = p_obj->icc_next;
    }

    return 0;

} /* end icc_findelem */


/* ======== icc_insert ================================================ */
/* PURPOSE:
 *	Insert an element
 *
 */

void	icc_insert(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_o
) {

    p_o->icc_num = p_oh->icc_count ++;
    p_o->icc_objhead = p_oh;
    p_o->icc_used = 0;
    p_o->icc_used1 = 0;
    p_o->icc_posi[ 0 ] = inx_last[ 0 ];
    p_o->icc_next = p_oh->icc_head;
    p_oh->icc_head = p_o;

    return;

} /* end icc_insert */


/* ======== icc_findgeneric =========================================== */
/* PURPOSE:
 *	A generic find
 *
 * RETURNS:
 * 	
 */

ICC_OBJ_TYP	* icc_findgeneric(
    ICC_OBJ_HEAD    * p_oh,
    void	    * data1,
    void	    * data2
) {

    ICC_OBJ_TYP	      a_obj[ 1 ];

    a_obj->icc_data1 = data1;
    a_obj->icc_data2 = data2;

    return icc_findelem( p_oh, a_obj );

} /* end icc_findgeneric */


/* ======== icc_insertgeneric ========================================= */
/* PURPOSE:
 *	Insert a generic node
 *
 * RETURNS:
 * 	
 */

ICC_OBJ_TYP	* icc_insertgeneric(
    ICC_OBJ_HEAD    * p_oh,
    void	    * data1,
    void	    * data2
) {

    ICC_OBJ_TYP	    * p_obj;

    p_obj = ( ICC_OBJ_TYP * ) inx_alloc( sizeof( ICC_OBJ_TYP ) );

    memset( p_obj, 0, sizeof( * p_obj ) );
    p_obj->icc_data1 = data1;
    p_obj->icc_data2 = data2;

    icc_insert( p_oh, p_obj );

    return p_obj;

} /* end icc_insertgeneric */


/* ======== icc_insertalloc =========================================== */
/* PURPOSE:
 *	Allocate a new obj and insert it !
 *
 * RETURNS:
 * 	
 */

ICC_OBJ_TYP	* icc_insertalloc(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_sobj
) {

    ICC_OBJ_TYP	    * p_obj;

    p_obj = ( ICC_OBJ_TYP * ) inx_alloc( p_oh->icc_size );

    memcpy( p_obj, p_sobj, p_oh->icc_size );

    icc_insert( p_oh, p_obj );

    return p_obj;

} /* end icc_insertalloc */

#if TESTDEBUG

#define PSPEC( msg, spec ) fprintf( stderr, "***%s***\n", msg ); printspec( spec );
#define PLSPEC( msg, spec ) fprintf( stderr, "***%s***\n", msg ); printlspec( spec );

/* ======== icc_stringof ============================================== */
/* PURPOSE:
 *	Get pointer to string
 *
 * RETURNS:
 * 	
 */

void	* icc_stringof(
    ICC_OBJ_TYP	    * p_obj
) {

    if ( ! p_obj ) {
	return "(null)";
    }

    return p_obj->icc_data1;

} /* end icc_stringof */


/* ======== icc_stringof1 ============================================= */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	* icc_stringof1(
    ICC_OBJ_TYP	    * p_obj
) {

    if ( ! p_obj ) {
	return "{null}";
    }

    return icc_stringof( p_obj->icc_data1 );

} /* end icc_stringof1 */




/* ======== printtable ================================================ */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	printtable(
    ICC_SPECTAB	    * p_tab
) {

    if ( p_tab->icc_isfile ) {
	fprintf( stderr, "  FILE %s\n", icc_stringof1( p_tab->icc_tobj ) );
    } else {
	fprintf( stderr, "  OBJECT %s ", icc_stringof1( p_tab->icc_tobj ) );
	fprintf( stderr, "SYMBOL %s\n", icc_stringof1( p_tab->icc_tbl ) );
    }

    return;

} /* end printtable */



/* ======== printlspec ================================================ */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	printlspec(
    ICC_STDLIBSPEC	* p_ls
) {
    int			  j;

    fprintf( stderr, "Lspec %d\n", p_ls->icc_obj->icc_num );
    fprintf( stderr, " from_enc '%s'\n", icc_stringof( p_ls->icc_from_enc ) );
    fprintf( stderr, " to_enc   '%s'\n", icc_stringof( p_ls->icc_to_enc ) );

    fprintf( stderr, " MethNum %d\n", p_ls->icc_meths->icc_obj->icc_num );
    fprintf( stderr, " mbwc_dso '%s'\n", icc_stringof1( p_ls->icc_meths->icc_mbwc_obj ) );
    fprintf( stderr, " mbwc_sym '%s'\n", icc_stringof1( p_ls->icc_meths->icc_mbwc_sym ) );

    fprintf( stderr, " Ntable %d\n", p_ls->icc_meths->icc_ntable_mbwc );
    /* dump all tables */
    for ( j = 0; j < p_ls->icc_meths->icc_ntable_mbwc; j ++ ) {
	printtable( p_ls->icc_meths->icc_table + j );
    }

    fprintf( stderr, " Ntable %d\n", p_ls->icc_meths->icc_ntable_wcmb );
    /* dump all tables */
    for ( j = p_ls->icc_meths->icc_ntable_mbwc; j < p_ls->icc_meths->icc_ntable_mbwc + p_ls->icc_meths->icc_ntable_wcmb; j ++ ) {
	printtable( p_ls->icc_meths->icc_table + j );
    }

    fprintf( stderr, "\n" );

    return;

} /* end printlspec */




/* ======== printmeth ================================================= */
/* PURPOSE:
 *	Print a method !
 *
 * RETURNS:
 * 	
 */

void	printmeth(
    ICC_METHODS		* p_mth
) {

    fprintf( stderr, "Meth %d\n", p_mth->icc_obj->icc_num );
    fprintf( stderr, " aobj '%s'\n", icc_stringof1( p_mth->icc_aobj ) );
    fprintf( stderr, " cnv  '%s'\n", icc_stringof1( p_mth->icc_cnv ) );
    fprintf( stderr, " cobj '%s'\n", icc_stringof1( p_mth->icc_cobj ) );
    fprintf( stderr, " ctl  '%s'\n", icc_stringof1( p_mth->icc_ctl ) );

    return;

} /* end printmeth */


/* ======== printspec ================================================= */
/* PURPOSE:
 *	Print content of a spec
 *
 * RETURNS:
 * 	
 */

void	printspec( ICC_SPEC * p_sp )
{
    int		    j;

    fprintf( stderr, "Spec %d\n", p_sp->icc_obj->icc_num );
    fprintf( stderr, " from_enc '%s'\n", icc_stringof( p_sp->icc_from_enc ) );
    fprintf( stderr, " to_enc   '%s'\n", icc_stringof( p_sp->icc_to_enc ) );

    printmeth( p_sp->icc_mth );

    fprintf( stderr, " Ntable %d\n", p_sp->icc_ntable );
    /* dump all tables */
    for ( j = 0; j < p_sp->icc_ntable; j ++ ) {
	printtable( p_sp->icc_table + j );
    }

    fprintf( stderr, "\n" );

    return;

} /* end printspec */

#else

#define PSPEC( msg, spec )
#define PLSPEC( msg, spec )

#endif



/* ======== icc_init ================================================== */
/* PURPOSE:
 *	Initialize
 *
 * RETURNS:
 * 	
 */

void	icc_init()
{

    ICC_HEAD	    * p_hd = icc_hd;

    p_hd->icc_strings->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_string;
    p_hd->icc_strings->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_objects->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compargeneric;
    p_hd->icc_objects->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_symbols->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compargeneric;
    p_hd->icc_symbols->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_files->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compargeneric;
    p_hd->icc_files->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_specs->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_spec;
    p_hd->icc_specs->icc_size = sizeof( ICC_SPEC );
    
    p_hd->icc_methods->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_meth;
    p_hd->icc_methods->icc_size = sizeof( ICC_METHODS );
    
    p_hd->icc_clones->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_clone;
    p_hd->icc_clones->icc_size = sizeof( ICC_CLONEOBJ );
    
    p_hd->icc_joins->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_join;
    p_hd->icc_joins->icc_size = sizeof( ICC_JOINOBJ );

    p_hd->icc_libspecs->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_libspec;
    p_hd->icc_libspecs->icc_size = sizeof( ICC_STDLIBSPEC );

    p_hd->icc_libmeths->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_libmeth;
    p_hd->icc_libmeths->icc_size = sizeof( ICC_STDLIBMETH );

    p_hd->icc_loc_alias->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_1_generic;
    p_hd->icc_loc_alias->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_loc_codeset->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_1_generic;
    p_hd->icc_loc_codeset->icc_size = sizeof( ICC_OBJ_TYP );
    
    p_hd->icc_resources->icc_cmpfunc = ( ICC_CMPFUNC ) icc_compar_1_generic;
    p_hd->icc_resources->icc_size = sizeof( ICC_OBJ_TYP );
    
    return;

} /* end icc_init */

/* ======== icc_getstr ================================================ */
/* PURPOSE:
 *	Get the string corresponding to the string given
 *
 * RETURNS:
 * 	
 */

ICC_OBJ_TYP * icc_getstr(
    char	* str
) {
    ICC_HEAD		* p_hd = icc_hd;
    ICC_OBJ_TYP		* p_obj;

    p_obj = icc_findgeneric( p_hd->icc_strings, str, 0 );

    if ( ! p_obj ) {
	p_obj = icc_insertgeneric( p_hd->icc_strings, str, 0 );
    } else {
	inx_free( str );
    }

    return p_obj;

} /* end icc_getstr */


/* ======== icc_getspec =============================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

ICC_SPEC * icc_getspec(
    ICC_HEAD		* p_hd,
    ICC_OBJ_TYP		* p_o_a,
    ICC_OBJ_TYP		* p_o_b
) {

    ICC_SPEC		  a_spec[ 1 ];
    ICC_OBJ_TYP		* p_obj;

    a_spec->icc_from_enc = p_o_a;
    a_spec->icc_to_enc = p_o_b;

    p_obj = icc_findelem( p_hd->icc_specs, OBJ( a_spec ) );
    
    return ( ICC_SPEC * ) p_obj;

} /* end icc_getspec */



/* ======== icc_getmkgeneric ================================================= */
/* PURPOSE:
 *	Get or make a generic object
 *
 * RETURNS:
 * 	
 */

ICC_OBJ_TYP	* icc_getmkgeneric(
    ICC_OBJ_HEAD    * p_oh,
    void	    * data1,
    void	    * data2
) {

    ICC_OBJ_TYP	      * p_obj;


    p_obj = icc_findgeneric( p_oh, data1, data2 );

    if ( ! p_obj ) {
/* ###  if ( ! data2 ) printf( "making DSO %s\n", (( ICC_OBJ_TYP * ) data1 )->icc_data1 ); */
	p_obj = icc_insertgeneric( p_oh, data1, data2 );
    }

    return p_obj;

} /* end icc_getmkgeneric */


/* ======== icc_getmk ================================================= */
/* PURPOSE:
 *	getmk !
 *
 * RETURNS:
 * 	
 */

void	* icc_getmk(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_sobj
) {

    ICC_OBJ_TYP	      * p_obj;


    p_obj = icc_findelem( p_oh, p_sobj );

    if ( ! p_obj ) {
	p_obj = icc_insertalloc( p_oh, p_sobj );
    }

    return ( void * ) p_obj;

} /* end icc_getmk */



/* ======== icc_newspec =============================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

ICC_SPEC * icc_newspec(
    ICC_HEAD		* p_hd,
    ICC_OBJ_TYP		* p_o_a,
    ICC_OBJ_TYP		* p_o_b
) {

    ICC_SPEC		  a_spec[ 1 ];
    ICC_OBJ_TYP		* p_obj;

    memset( a_spec, 0, sizeof( a_spec ) );
    a_spec->icc_from_enc = p_o_a;
    a_spec->icc_to_enc = p_o_b;

    p_obj = icc_insertalloc( p_hd->icc_specs, a_spec->icc_obj );
    
    return ( ICC_SPEC * ) p_obj;

} /* end icc_newspec */


/* ======== mkentry =================================================== */
/* PURPOSE:
 *	This is called when an entry is found in the source files.
 *
 * RETURNS:
 * 	
 */

void	mkentry(
    char	* from_enc,
    char	* to_enc,
    char	* alg_dso,
    char	* alg_sym,
    char	* ctl_dso,
    char	* ctl_sym,
    ICC_TBLST	* tablelist
) {

    ICC_OBJ_TYP	    * from_enc_str = icc_getstr( from_enc );
    ICC_OBJ_TYP	    * to_enc_str = icc_getstr( to_enc );
    ICC_OBJ_TYP	    * alg_dso_str = icc_getstr( alg_dso );
    ICC_OBJ_TYP	    * alg_sym_str = icc_getstr( alg_sym );
    ICC_OBJ_TYP	    * ctl_dso_str = icc_getstr( ctl_dso );
    ICC_OBJ_TYP	    * ctl_sym_str = icc_getstr( ctl_sym );
    ICC_OBJ_TYP    ** tbl_lst_str = (ICC_OBJ_TYP **)NULL;
    ICC_HEAD	    * p_hd = icc_hd;
    ICC_METHODS	      a_mth[ 1 ];

    ICC_SPEC	    * spec;
    ICC_TBLST       * p, * pp;       
    int               num_tblst=0, i;

#ifdef TESTDEBUG3
    ICC_TBLST       *q;
    int              count=0;
    printf( "mkentry() [ Head of tablelist ]\n" );
    for ( count=0, q=tablelist; q; q=q->next, count++ ) {
          printf( "- One table -\n" );
          printf( "  SYM: %s\n", OBJ(q->odata->icc_data1)->icc_data1 );
          if ( q->odata->icc_data2 )
               printf( "  DSO: %s\n", OBJ(q->odata->icc_data2)->icc_data1 );
    }
    printf( "- table count: %d\n", count );
    fflush( stdout );
#endif

    if ( ! from_enc || ! to_enc ) {
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_NONULLSPEC,
	    INX_LINE | INX_ERROR
	);
    }

    memset( a_mth, 0, sizeof( a_mth ) );

    spec = icc_getspec( p_hd, from_enc_str, to_enc_str );

    if ( spec ) {
	/* spec is previously defined					*/
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_PREVIOUS,
	    INX_LINE | INX_WARN
	);

	inx_error(
	    spec->icc_posi,
	    ALEX_TABLE,
	    ALEX_FOUND_HERE,
	    INX_LINE | INX_WARN
	);

	return;
    }

    /* Multiple spec: tablelist for tbl_dso and tbl_sym. (num_tblst-1 is fine.) */
    for ( num_tblst=0, p=tablelist; p != NULL; p=p->next, num_tblst++ );
    p_hd->icc_specs->icc_size = sizeof(ICC_SPEC)+sizeof(ICC_SPECTAB)*num_tblst;

    spec = icc_newspec( p_hd, from_enc_str, to_enc_str );
    /* spec->icc_from_enc = from_enc_str;				*/
    /* spec->icc_to_enc = to_enc_str;					*/

    * spec->icc_posi = * inx_last;

    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_str, alg_dso_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_str, ctl_dso_str );

    spec->icc_mth = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );
    
    /* icc_ntable must be specified correctly for potential joins	*/
    spec->icc_ntable = num_tblst;

    /* We don't have to check validity of each data in the tablelist,
       because it's been checked in mktable() and invalid data hasn't 
       been added to this list in mktablelist().
    */
    if ( 0 < num_tblst ) {
         tbl_lst_str = (ICC_OBJ_TYP **)inx_alloc( sizeof(ICC_OBJ_TYP *)*num_tblst );
         memset( tbl_lst_str, 0, sizeof(ICC_OBJ_TYP *)*num_tblst );
         for ( i=num_tblst-1, p=tablelist; -1<i; p=p->next, i-- )
               *(tbl_lst_str+i) = p->odata;
    }

    for ( i=0; i<num_tblst; i++ ) {

          if ( ! tbl_lst_str[i]->icc_data2 ) { /* FILE: (ICC_OBJ_TYP *)0 */
               spec->icc_table[i].icc_isfile = 1;
	       spec->icc_table[i].icc_tobj   = tbl_lst_str[i];
	       spec->icc_table[i].icc_tbl    = 0;
	  }
          else{
               spec->icc_table[i].icc_isfile = 0;
	       spec->icc_table[i].icc_tobj   = icc_getmkgeneric( p_hd->icc_objects,
                                                                 tbl_lst_str[i]->icc_data2,
                                                                 0 ); 
               spec->icc_table[i].icc_tbl    = tbl_lst_str[i];
              }
    }

    PSPEC( "new entry", spec )

    inx_free( tbl_lst_str );
    for ( p=tablelist; p != NULL; p=pp ) {
          pp = p->next;
          inx_free( (void *)p );
    }    

    return;

} /* end mkentry */


/* ======== mkclone =================================================== */
/* PURPOSE:
 *	Clone record for cloning spec's.  This just records them,
 *  they will used when all input has been compiled.
 *
 * RETURNS:
 * 	
 */

void	mkclone(
    int		  to_from,
    char	* enc,
    char	* alg_dso,
    char	* alg_sym,
    char	* ctl_dso,
    char	* ctl_sym,
    char	* enc_new,
    char	* alg_dso_new,
    char	* alg_sym_new,
    char	* ctl_dso_new,
    char	* ctl_sym_new
) {

    ICC_OBJ_TYP	    * enc_str = icc_getstr( enc );
    ICC_OBJ_TYP	    * alg_dso_str = icc_getstr( alg_dso );
    ICC_OBJ_TYP	    * alg_sym_str = icc_getstr( alg_sym );
    ICC_OBJ_TYP	    * ctl_dso_str = icc_getstr( ctl_dso );
    ICC_OBJ_TYP	    * ctl_sym_str = icc_getstr( ctl_sym );
    
    ICC_OBJ_TYP	    * enc_new_str = icc_getstr( enc_new );
    ICC_OBJ_TYP	    * alg_dso_new_str = icc_getstr( alg_dso_new );
    ICC_OBJ_TYP	    * alg_sym_new_str = icc_getstr( alg_sym_new );
    ICC_OBJ_TYP	    * ctl_dso_new_str = icc_getstr( ctl_dso_new );
    ICC_OBJ_TYP	    * ctl_sym_new_str = icc_getstr( ctl_sym_new );

    ICC_HEAD	    * p_hd = icc_hd;
    ICC_METHODS	      a_mth[ 1 ];

    ICC_CLONEOBJ      clone[ 1 ];
    ICC_CLONEOBJ    * p_clone;

    if ( ! enc ) {
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_NONULLSPEC,
	    INX_LINE | INX_ERROR
	);
    }

    memset( a_mth, 0, sizeof( a_mth ) );
    memset( clone, 0, sizeof( clone ) );
    * clone->icc_posi = * inx_last;

    clone->icc_src_dst = to_from;
    clone->icc_enc = enc_str;

    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_str, alg_dso_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_str, ctl_dso_str );

    clone->icc_mth = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );

    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_new_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_new_str, alg_dso_new_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_new_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_new_str, ctl_dso_new_str );

    clone->icc_newenc = enc_new_str;
    clone->icc_newmth = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );

    p_clone = icc_findelem( p_hd->icc_clones, clone->icc_obj );

    if ( p_clone ) {

	if ( p_clone->icc_newmth != clone->icc_newmth ) {
	    /* spec is previously defined	*/
	    inx_error(
		inx_last,		/* the last position */
		ALEX_TABLE,
		ALEX_PREVIOUS,
		INX_LINE | INX_WARN
	    );
    
	    inx_error(
		p_clone->icc_posi,
		ALEX_TABLE,
		ALEX_FOUND_HERE,
		INX_LINE | INX_WARN
	    );
	}

	return;
    }

    icc_insertalloc( p_hd->icc_clones, clone->icc_obj );

    return;

} /* end mkclone */


/* ======== mkjoin ==================================================== */
/* PURPOSE:
 *	This makes a join record.  The join is performed after reading
 * all input.
 *
 * RETURNS:
 * 	
 */

void	mkjoin(
    char	* enc,
    char	* alg_dso,
    char	* alg_sym,
    char	* ctl_dso,
    char	* ctl_sym,
    char	* enc_2,
    char	* alg_dso_2,
    char	* alg_sym_2,
    char	* ctl_dso_2,
    char	* ctl_sym_2,
    char	* alg_dso_new,
    char	* alg_sym_new,
    char	* ctl_dso_new,
    char	* ctl_sym_new
) {

    ICC_OBJ_TYP	    * enc_str = icc_getstr( enc );
    ICC_OBJ_TYP	    * alg_dso_str = icc_getstr( alg_dso );
    ICC_OBJ_TYP	    * alg_sym_str = icc_getstr( alg_sym );
    ICC_OBJ_TYP	    * ctl_dso_str = icc_getstr( ctl_dso );
    ICC_OBJ_TYP	    * ctl_sym_str = icc_getstr( ctl_sym );
    
    ICC_OBJ_TYP	    * enc_2_str = icc_getstr( enc_2 );
    ICC_OBJ_TYP	    * alg_dso_2_str = icc_getstr( alg_dso_2 );
    ICC_OBJ_TYP	    * alg_sym_2_str = icc_getstr( alg_sym_2 );
    ICC_OBJ_TYP	    * ctl_dso_2_str = icc_getstr( ctl_dso_2 );
    ICC_OBJ_TYP	    * ctl_sym_2_str = icc_getstr( ctl_sym_2 );

    ICC_OBJ_TYP	    * alg_dso_new_str = icc_getstr( alg_dso_new );
    ICC_OBJ_TYP	    * alg_sym_new_str = icc_getstr( alg_sym_new );
    ICC_OBJ_TYP	    * ctl_dso_new_str = icc_getstr( ctl_dso_new );
    ICC_OBJ_TYP	    * ctl_sym_new_str = icc_getstr( ctl_sym_new );

    ICC_HEAD	    * p_hd = icc_hd;
    ICC_METHODS	      a_mth[ 1 ];

    ICC_JOINOBJ	      join[ 1 ];
    ICC_JOINOBJ	    * p_join;

    if ( ! enc || ! enc_2 ) {
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_NONULLSPEC,
	    INX_LINE | INX_ERROR
	);
    }

    memset( a_mth, 0, sizeof( a_mth ) );
    memset( join, 0, sizeof( join ) );
    * join->icc_posi = * inx_last;

    join->icc_dst_enc = enc_str;
    join->icc_src_enc = enc_2_str;

    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_str, alg_dso_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_str, ctl_dso_str );

    join->icc_mth_dst = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );
    
    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_2_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_2_str, alg_dso_2_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_2_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_2_str, ctl_dso_2_str );

    join->icc_mth_src = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );

    a_mth->icc_aobj = icc_getmkgeneric( p_hd->icc_objects, alg_dso_new_str, 0 );
    a_mth->icc_cnv = icc_getmkgeneric(  p_hd->icc_symbols, alg_sym_new_str, alg_dso_new_str );
    a_mth->icc_cobj = icc_getmkgeneric( p_hd->icc_objects, ctl_dso_new_str, 0 );
    a_mth->icc_ctl = icc_getmkgeneric(  p_hd->icc_symbols, ctl_sym_new_str, ctl_dso_new_str );

    join->icc_newmth = icc_getmk( p_hd->icc_methods, a_mth->icc_obj );

    p_join = icc_findelem( p_hd->icc_joins, join->icc_obj );

    if ( p_join ) {

	if ( p_join->icc_newmth != join->icc_newmth ) {
	    
	    /* spec is previously defined				*/
	    inx_error(
		inx_last,		/* the last position */
		ALEX_TABLE,
		ALEX_PREVIOUS,
		INX_LINE | INX_WARN
	    );
    
	    inx_error(
		p_join->icc_posi,
		ALEX_TABLE,
		ALEX_FOUND_HERE,
		INX_LINE | INX_WARN
	    );
	}

	return;
    }

    icc_insertalloc( p_hd->icc_joins, join->icc_obj );

    return;

} /* end mkjoin */


/* ======== mktablelist =============================================== */
/* PURPOSE:
 *	This makes a tablelist
 *
 * RETURNS:
 * 	
 */

void	* mktablelist(
    ICC_TBLST   * head,
    ICC_OBJ_TYP * table
) {

    ICC_TBLST *p;

#ifdef TESTDEBUG3
    ICC_TBLST *q;
#endif

    /* mktable() returns 0 for 'table' if the data
       is not valid. Then, ignore them and return
       'head' for a linked list.
    */
    if ( !table ) return head;

    p = (ICC_TBLST *)inx_alloc( sizeof( ICC_TBLST ) );
    memset( p, 0, sizeof( ICC_TBLST ) );

    p->odata = table;
    p->next  = head;

#ifdef TESTDEBUG3
    printf( "mktablelist()  ( Head of tablelist )\n" );
    for ( q=p; q; q=q->next ) {
          printf( "- One table -\n" );
          printf( "  SYM: %s\n", OBJ(q->odata->icc_data1)->icc_data1 );
          if ( q->odata->icc_data2 )
               printf( "  DSO: %s\n", OBJ(q->odata->icc_data2)->icc_data1 );
          fflush( stdout );
    }
#endif
    /*
    head     = p;
    return   head;
    */
    return   p;

} /* end mktablelist */


/* ======== mktable =================================================== */
/* PURPOSE:
 *	This makes a table type
 *
 * RETURNS:
 * 	
 */

void	* mktable(
    char	* tbl_dso,
    char	* tbl_sym,
    int		  file
) {

    ICC_OBJ_TYP	    * tbl_dso_str = icc_getstr( tbl_dso );
    ICC_OBJ_TYP	    * tbl_sym_str = file ? 0 : icc_getstr( tbl_sym );

    ICC_HEAD	    * p_hd = icc_hd;
    
#ifdef TESTDEBUG3
    printf( "mktable() = DSO: %s   SYM: %s\n", tbl_dso, tbl_sym );
    fflush( stdout );
#endif

    if ( file ) {
	if ( tbl_dso ) {
	    return icc_getmkgeneric( p_hd->icc_files, tbl_dso_str, 0 );
	} else {
	    return 0;
	}
    } else {
	if ( tbl_sym ) {
	    return icc_getmkgeneric( p_hd->icc_symbols, tbl_sym_str, tbl_dso_str );
	} else {
	    return 0;
	}
    }

} /* end mktable */


/* ======== mkstdlib ================================================== */
/* PURPOSE:
 *	Make a stdlib entry
 *
 * RETURNS:
 * 	
 */

void	mkstdlib(
    char	* from_enc,
    char	* to_enc,
    char        * mbwc_dso,
    char        * mbwc_sym,
    ICC_TBLST   * tblst_mbwc,
    ICC_TBLST   * tblst_wcmb
) {

    ICC_OBJ_TYP	    * from_enc_str = icc_getstr( from_enc );
    ICC_OBJ_TYP	    * to_enc_str = icc_getstr( to_enc );
    ICC_OBJ_TYP     * mbwc_dso_str = icc_getstr( mbwc_dso );
    ICC_OBJ_TYP     * mbwc_sym_str = icc_getstr( mbwc_sym );

    ICC_HEAD	    * p_hd = icc_hd;

    ICC_STDLIBSPEC    lspec[ 1 ];
    ICC_STDLIBSPEC  * p_lspec;
    ICC_SPECTAB	    * p_tab;
    ICC_STDLIBMETH  * lmeth;
    ICC_STDLIBMETH  * p_lmeth;

    ICC_OBJ_TYP    ** tblmbwc = (ICC_OBJ_TYP **)NULL;
    ICC_OBJ_TYP    ** tblwcmb = (ICC_OBJ_TYP **)NULL;
    ICC_TBLST       * p, * pp;       
    int               n_tblmbwc, n_tblwcmb, i;
    int               table_size = 0;

    if ( ! from_enc || ! to_enc ) {
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_NONULLSPEC,
	    INX_LINE | INX_ERROR
	);
    }

    /* Multiple spec: tablelist. */

    for ( n_tblmbwc=0, p=tblst_mbwc; p != NULL; p=p->next, n_tblmbwc++ );
    if ( 0 < n_tblmbwc ) {
         tblmbwc = (ICC_OBJ_TYP **)inx_alloc( sizeof(ICC_OBJ_TYP *)*n_tblmbwc );
         memset( tblmbwc, 0, sizeof(ICC_OBJ_TYP *)*n_tblmbwc );
         for ( i=n_tblmbwc-1, p=tblst_mbwc; -1<i; p=p->next, i-- )
              *(tblmbwc+i) = p->odata;
    }
    for ( n_tblwcmb=0, p=tblst_wcmb; p != NULL; p=p->next, n_tblwcmb++ );
    if ( 0 < n_tblwcmb ) {
         tblwcmb = (ICC_OBJ_TYP **)inx_alloc( sizeof(ICC_OBJ_TYP *)*n_tblwcmb );
         memset( tblwcmb, 0, sizeof(ICC_OBJ_TYP *)*n_tblwcmb );
         for ( i=n_tblwcmb-1, p=tblst_wcmb; -1<i; p=p->next, i-- )
              *(tblwcmb+i) = p->odata;
    }

    table_size = sizeof(ICC_SPECTAB)*(n_tblmbwc+n_tblwcmb);
    lmeth = (ICC_STDLIBMETH *)inx_alloc( sizeof(ICC_STDLIBMETH) + table_size );

    memset( lspec, 0, sizeof( lspec ) );
    memset( lmeth, 0, sizeof(*lmeth ) + table_size );

    * lspec->icc_posi = * inx_last;
    lspec->icc_from_enc = from_enc_str;
    lspec->icc_to_enc = to_enc_str;

    lmeth->icc_mbwc_obj = icc_getmkgeneric( p_hd->icc_objects, mbwc_dso_str, 0 );
    lmeth->icc_mbwc_sym = icc_getmkgeneric( p_hd->icc_symbols, mbwc_sym_str, mbwc_dso_str );

    /* We don't have to check validity of each data in the tablelist,
       because it's been checked in mktable() and invalid data hasn't 
       been added to this list in mktablelist().
    */
    lmeth->icc_ntable_mbwc = n_tblmbwc;
    for ( p_tab=lmeth->icc_table, i=0; i<n_tblmbwc; i++ ) {

	/* mbwctbl_obj can only belong to files or symbols		*/
	if ( tblmbwc[i]->icc_objhead == p_hd->icc_files ) {
	     p_tab[i].icc_isfile = 1;
	     p_tab[i].icc_tobj = tblmbwc[i];
	} else {
	     /* must be a symbol */
	     p_tab[i].icc_isfile = 0;
	     p_tab[i].icc_tobj = icc_getmkgeneric( p_hd->icc_objects, 
                                                    tblmbwc[i]->icc_data2, 0 );
 	     p_tab[i].icc_tbl = tblmbwc[i];
	}
    }
    
    lmeth->icc_ntable_wcmb = n_tblwcmb;
    for ( p_tab=lmeth->icc_table+lmeth->icc_ntable_mbwc, i=0; i<n_tblwcmb; i++ ) {

	/* mbwctbl_obj can only belong to files or symbols		*/
	if ( tblwcmb[i]->icc_objhead == p_hd->icc_files ) {
	    p_tab[i].icc_isfile = 1;
	    p_tab[i].icc_tobj = tblwcmb[i];
	} else {
	    /* must be a symbol */
	    p_tab[i].icc_isfile = 0;
	    p_tab[i].icc_tobj = icc_getmkgeneric( p_hd->icc_objects, 
                                                   tblwcmb[i]->icc_data2, 0 );
	    p_tab[i].icc_tbl = tblwcmb[i];
	}
    }
    
    p_lspec = icc_findelem( p_hd->icc_libspecs, lspec->icc_obj );

    if ( p_lspec ) {
	/* spec is previously defined - This is allowed ! take the first*/
	/* definition encountered.					*/

	return;
    }

    p_hd->icc_libmeths->icc_size = sizeof( *lmeth ) + table_size;
    p_lmeth = icc_getmk( p_hd->icc_libmeths, lmeth->icc_obj );

    lspec->icc_meths = p_lmeth;

#if TESTDEBUG
    p_lspec = ( ICC_STDLIBSPEC * )
#endif
    icc_insertalloc( p_hd->icc_libspecs, lspec->icc_obj );

#if TESTDEBUG
    printlspec( p_lspec );
#endif

    inx_free( lmeth );
    for ( p=tblst_mbwc; p != NULL; p=pp ) {
          pp = p->next;
          inx_free( (void *)p );
    }    
    for ( p=tblst_wcmb; p != NULL; p=pp ) {
          pp = p->next;
          inx_free( (void *)p );
    }    

    return;

} /* end mkstdlib */


/* ======== mklocale_alias ============================================ */
/* PURPOSE:
 *	Parsed a locale alias
 *
 * RETURNS:
 * 	null
 */

void * mklocale_alias(
    char	* locale_name,
    char	* value
) {

    ICC_OBJ_TYP	    * str_locale_name = icc_getstr( locale_name );
    ICC_OBJ_TYP	    * str_value = icc_getstr( value );

    ICC_HEAD	    * p_hd = icc_hd;

    icc_getmkgeneric( p_hd->icc_loc_alias, str_locale_name, str_value );

    return 0;

} /* end mklocale_alias */



/* ======== mklocale_codeset ========================================== */
/* PURPOSE:
 *	Make a locale/codeset pair
 *
 * RETURNS:
 * 	Null
 */

void * mklocale_codeset(
    char	* locale_name,
    char	* value
) {

    ICC_OBJ_TYP	    * str_locale_name = icc_getstr( locale_name );
    ICC_OBJ_TYP	    * str_value = icc_getstr( value );
    
    ICC_HEAD	    * p_hd = icc_hd;

    icc_getmkgeneric( p_hd->icc_loc_codeset, str_locale_name, str_value );

    return 0;

} /* end mklocale_codeset */



/* ======== icc_read_input ============================================ */
/* PURPOSE:
 *	Read input files.
 *
 * RETURNS:
 * 	
 */

int	icc_read_input( int argc, char ** argv, char ** pname )
{

    int		i;
    int		error = 0;

    for ( i = 1; i < argc; i ++ ) {

	if ( ! strcmp( argv[ i ], "-f" ) ) {
	    if ( i >= argc ) {
		fprintf( stderr, "iconv_comp: -f argument not given\n" );
		exit( 1 );
	    }
	    * pname = argv[ i + 1 ];
	    i ++;
	    continue;
	}

	inx_clear();
	inx_setfile( argv[ i ], ( FILE * ) 0 );
	yyparse();
	if ( inx_iserror() ) {
	    error = 1;
	    fprintf( stderr, "%s : has errors\n", argv[ i ] );
	    inx_stats();
	}
    }

    return error;

} /* end icc_read_input */


/* ======== ICC_SPANEM ================================================ */
/* PURPOSE:
 *	Span all elements in a linked list
 *
 * RETURNS:
 * 	
 */

#define ICC_SPANEM( typ, var, hd )					\
    for (								\
	var = ( typ * ) ( hd->icc_head );				\
	var;								\
	var = ( typ * ) ( ( ICC_OBJ_TYP * ) var )->icc_next		\
    )									\
/* end of macro */

#define ICC_SPANUSED( typ, var, hd )					\
    for (								\
	var = ( typ * ) ( hd->icc_head );				\
	var;								\
	var = ( typ * ) ( ( ICC_OBJ_TYP * ) var )->icc_next		\
    ) if ( ( ( ICC_OBJ_TYP * ) var )->icc_used )			\
/* end of macro */


/* ======== icc_cmpstr ================================================ */
/* PURPOSE:
 *	Compare string by string
 *
 * RETURNS:
 * 	
 */

int	icc_cmpstr(
    ICC_OBJ_TYP		** pp_a,
    ICC_OBJ_TYP		** pp_b
) {
    char		 * sa;
    char		 * sb;
    ICC_OBJ_TYP		 * p_a = * pp_a;
    ICC_OBJ_TYP		 * p_b = * pp_b;

    if ( ! p_a ) {
	if ( ! p_b ) {
	    return 0;
	}
	return 1;
    } else if ( ! p_b ) {
	return -1;
    }

    if ( p_a == p_b ) {
	return 0;
    }

    sa = p_a->icc_data1;
    sb = p_b->icc_data1;

    if ( ! sa ) {
	if ( ! sb ) {
	    return 0;
	}
	return 1;
    } else if ( ! sb ) {
	return -1;
    }

    return strcasecmp( sa, sb );

} /* end icc_cmpstr */



/* ======== icc_compar ================================================ */
/* PURPOSE:
 *	Comparison routine
 *
 * RETURNS:
 * 	
 */

int	icc_compar(
    const ICC_SPEC	** p_a,
    const ICC_SPEC	** p_b
) {

    int		cmp;

    cmp = (*p_a)->icc_from_enc->icc_num - (*p_b)->icc_from_enc->icc_num;

    if ( cmp ) {
	return cmp;
    }
    
    return (*p_a)->icc_to_enc->icc_num - (*p_b)->icc_to_enc->icc_num;

} /* end icc_compar */

/* ======== icc_compar_ls ============================================= */
/* PURPOSE:
 *	Comparison routine
 *
 * RETURNS:
 * 	
 */

int	icc_compar_ls(
    const ICC_STDLIBSPEC	** p_a,
    const ICC_STDLIBSPEC	** p_b
) {

    int		cmp;

    cmp = (*p_a)->icc_from_enc->icc_num - (*p_b)->icc_from_enc->icc_num;

    if ( cmp ) {
	return cmp;
    }
    
    return (*p_a)->icc_to_enc->icc_num - (*p_b)->icc_to_enc->icc_num;

} /* end icc_compar_ls */



/* ======== icc_nulfnc ================================================ */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

#define ALIGN( A, B )	( 1 + ( ((A)-1) | ((B)-1) ) )

int	icc_nulfnc(
    char	    * ptr,
    int		    * prel,
    void	    * data,
    int		      size,
    int		      align
) {

    int		      new;

    new = ALIGN( * prel, align );

    * prel = new + size;

    return new;

} /* end icc_nulfnc */


/* ======== icc_cpyfnc ================================================ */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

int	icc_cpyfnc(
    char	    * ptr,
    int		    * prel,
    void	    * data,
    int		      size,
    int		      align
) {

    int		      new;

    new = ALIGN( * prel, align );

    memset( ptr + * prel, 0xff, new - * prel );

    memcpy( ptr + new, data, size );

    * prel = new + size;

    return new;

} /* end icc_cpyfnc */


/* ======== icc_iconv_r_spec_size ===================================== */
/* PURPOSE:
 *	Calculate a spec size for number of tables
 *
 * RETURNS:
 * 	
 */

int	icc_iconv_r_spec_size(
    int		    n
) {

    return
	offsetof( ICONV_S_SPEC, iconv_table ) +
	n * sizeof( struct ICONV_S_TBLS )
    ;

} /* end icc_iconv_r_spec_size */


/* ======== icc_iconv_r_libspec_size ================================== */
/* PURPOSE:
 *	Calculate a lib spec size for number of tables
 *
 * RETURNS:
 * 	
 */

int	icc_iconv_r_libspec_size(
    int		    n
) {

    return
	offsetof( ICONV_S_LSPEC, iconv_table ) +
	n * sizeof( struct ICONV_S_TBLS )
    ;

} /* end icc_iconv_r_libspec_size */


/* ======== icc_setused =============================================== */
/* PURPOSE:
 *	Set used flag and return relative address
 *
 * RETURNS:
 * 	
 */

ICONV_S_RELPTR	icc_setused(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_obj
) {

    if ( ! p_obj ) {
	return 0;
    }

    if ( ! p_obj->icc_used ) {
	p_oh->icc_usedcount ++;
	p_obj->icc_used = 1;
    }

    return p_obj->icc_robj;

} /* end icc_setused */



/* ======== icc_dump_data ============================================= */
/* PURPOSE:
 *	Dump the data file
 *
 * RETURNS:
 * 	
 */

void	icc_dump_data(
    ICC_HEAD	    * p_hd,
    char	    * data,
    int		    * prel,
    ICC_CPYFNC	      fnc,
    ICC_SPEC	   ** pp_top,
    ICC_STDLIBSPEC ** pp_ls_top,
    int		      nstr,
    ICC_OBJ_TYP    ** pp_str,
    ICONV_S_SPEC    * a_sp,
    ICONV_S_LSPEC   * a_ls_sp
) {

    ICONV_S_TABLE	a_st[ 1 ];
    ICONV_T_OBJECTSPEC	a_os[ 1 ];
    ICONV_T_SYMBOL	a_sy[ 1 ];
    ICONV_T_SPECKEY	a_sk[ 1 ];
    ICONV_T_METHOD	a_mt[ 1 ];
    ICONV_T_FILE	a_fl[ 1 ];
    ICONV_S_CODESET_LIST    a_cl[ 1 ];
    ICONV_T_LOC_CODESET	    a_lc[ 1 ];
    ICONV_S_ALIAS_LIST	    a_al[ 1 ];
    ICONV_T_LOC_ALIAS	    a_la[ 1 ];
    ICONV_S_RESOURCE_LIST   a_rl[ 1 ];
    ICONV_T_LOC_RESOURCE    a_lr[ 1 ];

    ICC_SPEC	      * p_sp;
    ICC_SPEC	     ** pp_sp;
    ICC_STDLIBSPEC    * p_ls_sp;
    ICC_STDLIBMETH    * p_lm_sp;
    ICC_STDLIBSPEC   ** pp_ls_sp;
    int			i;
    int			j;
    int			ntable;
    ICC_OBJ_TYP	      * p_obj;
    ICC_OBJ_TYP	      * p_obj1;
    ICC_OBJ_TYP	     ** pp_obj;
    ICC_METHODS	      * p_mt;

    /* First dump the ICONV_S_TABLE					*/

    a_st->iconv_magicnum = ICONV_MAGIC;
    a_st->iconv_numfiles = p_hd->icc_files->icc_usedcount;
    a_st->iconv_nummethods = p_hd->icc_methods->icc_usedcount;
    a_st->iconv_numobject = p_hd->icc_objects->icc_usedcount;
    a_st->iconv_symbols = p_hd->icc_symbols->icc_usedcount;
    a_st->iconv_numspec = p_hd->icc_specs->icc_count;
    a_st->iconv_numlibspec = p_hd->icc_libspecs->icc_count;
    a_st->iconv_numlspcobj = p_hd->icc_libmeths->icc_usedcount;
    a_st->iconv_numspecobj = p_hd->icc_specs->icc_count;
    a_st->iconv_alias_list = p_hd->icc_loc_alias->icc_robj;
    a_st->iconv_codeset_list = p_hd->icc_loc_codeset->icc_robj;
    a_st->iconv_resource_list = p_hd->icc_resources->icc_robj;

    /* Write the ICONV_S_TABLE record */
    ( * fnc )( data, prel, a_st, offsetof( ICONV_S_TABLE, iconv_speckey ), 8 );

    /* Write the keys. */
    for ( i = p_hd->icc_specs->icc_count, pp_sp = pp_top; i; pp_sp ++, i -- ) {
	p_sp = * pp_sp;

	a_sk->iconv_specifier[ ICONV_SRC ] = p_sp->icc_from_enc->icc_robj;
#if TESTDEBUG
p_obj = p_sp->icc_from_enc;
printf( "Keystr1 %d = %s @ %d\n", i, p_obj->icc_data1, p_obj->icc_robj );
#endif
	a_sk->iconv_specifier[ ICONV_DST ] = p_sp->icc_to_enc->icc_robj;
#if TESTDEBUG
p_obj = p_sp->icc_to_enc;
printf( "Keystr2 %d = %s @ %d\n", i, p_obj->icc_data1, p_obj->icc_robj );
#endif

	a_sk->iconv_spec = p_sp->icc_obj->icc_robj;

	( * fnc )( data, prel, a_sk, sizeof( a_sk ), sizeof( ICONV_S_RELPTR ) );
    }

    /* Write the libspec keys. */
    for ( i = p_hd->icc_libspecs->icc_count, pp_ls_sp = pp_ls_top; i; pp_ls_sp ++, i -- ) {
	p_ls_sp = * pp_ls_sp;

	a_sk->iconv_specifier[ ICONV_SRC ] = p_ls_sp->icc_from_enc->icc_robj;
#if TESTDEBUG
p_obj = p_ls_sp->icc_from_enc;
printf( "LIBKeystr1 %d = %s @ %d\n", i, p_obj->icc_data1, p_obj->icc_robj );
#endif
	a_sk->iconv_specifier[ ICONV_DST ] = p_ls_sp->icc_to_enc->icc_robj;
#if TESTDEBUG
p_obj = p_ls_sp->icc_to_enc;
printf( "LIBKeystr2 %d = %s @ %d\n", i, p_obj->icc_data1, p_obj->icc_robj );
#endif

	a_sk->iconv_spec =
	    icc_setused( p_hd->icc_libmeths, p_ls_sp->icc_meths->icc_obj );

	( * fnc )( data, prel, a_sk, sizeof( a_sk ), sizeof( ICONV_S_RELPTR ) );
    }

    /* Write the encoding strings close to the keys */
    for ( i = 0; i < nstr; i ++ ) {
	p_obj = pp_str[ i ];
	
	p_obj->icc_robj = ( * fnc )(
	    data,
	    prel,
	    p_obj->icc_data1,
	    1 + strlen( p_obj->icc_data1 ),
	    1
	);
	
#if TESTDEBUG
printf( "encstr %d = %s @ %d\n", i, p_obj->icc_data1, p_obj->icc_robj );
#endif

    }

    /* dump the other part of the specs */
    for ( i = p_hd->icc_specs->icc_count, pp_sp = pp_top; i; pp_sp ++, i -- ) {
	p_sp = * pp_sp;

	a_sp->iconv_specnum = pp_sp - pp_top;
	
	a_sp->iconv_meth = icc_setused( p_hd->icc_methods, p_sp->icc_mth->icc_obj );
	
	a_sp->iconv_ntable = p_sp->icc_ntable;

	/* dump all tables */
	for ( j = 0; j < p_sp->icc_ntable; j ++ ) {
	    if ( p_sp->icc_table[ j ].icc_isfile ) {
		a_sp->iconv_table[ j ].iconv_tableobject =
		    icc_setused( p_hd->icc_files, p_sp->icc_table[ j ].icc_tobj );
		a_sp->iconv_table[ j ].iconv_table = 0;
	    } else {
		a_sp->iconv_table[ j ].iconv_tableobject =
		    icc_setused( p_hd->icc_objects, p_sp->icc_table[ j ].icc_tobj );
		a_sp->iconv_table[ j ].iconv_table =
		    icc_setused( p_hd->icc_symbols, p_sp->icc_table[ j ].icc_tbl );
	    }
	}

	p_sp->icc_obj->icc_robj = ( * fnc )(
	    data, prel, a_sp, icc_iconv_r_spec_size( p_sp->icc_ntable ),
	    sizeof( ICONV_S_RELPTR )
	);
    }

    i = 0;
    ICC_SPANUSED( ICC_STDLIBMETH, p_lm_sp, p_hd->icc_libmeths ) {
	
	a_ls_sp->iconv_specnum = i ++;
	
	a_ls_sp->iconv_obj_lmth = icc_setused(
	    p_hd->icc_objects, p_lm_sp->icc_mbwc_obj
	);
	
	a_ls_sp->iconv_sym_lmth = icc_setused(
	    p_hd->icc_symbols, p_lm_sp->icc_mbwc_sym
	);
#if TESTDEBUG
printf( "iconv_sym_lmth offset is %d\n", a_ls_sp->iconv_sym_lmth );
#endif
	
	a_ls_sp->iconv_ntable_mbwc = p_lm_sp->icc_ntable_mbwc;
	a_ls_sp->iconv_ntable_wcmb = p_lm_sp->icc_ntable_wcmb;

	ntable = p_lm_sp->icc_ntable_mbwc + p_lm_sp->icc_ntable_wcmb;

	/* dump all tables */
	for ( j = 0; j < ntable; j ++ ) {
	    if ( p_lm_sp->icc_table[ j ].icc_isfile ) {
		a_ls_sp->iconv_table[ j ].iconv_tableobject =
		    icc_setused( p_hd->icc_files, p_lm_sp->icc_table[ j ].icc_tobj );
		a_ls_sp->iconv_table[ j ].iconv_table = 0;
	    } else {
		a_ls_sp->iconv_table[ j ].iconv_tableobject =
		    icc_setused( p_hd->icc_objects, p_lm_sp->icc_table[ j ].icc_tobj );
		a_ls_sp->iconv_table[ j ].iconv_table =
		    icc_setused( p_hd->icc_symbols, p_lm_sp->icc_table[ j ].icc_tbl );
	    }
	}

	p_lm_sp->icc_obj->icc_robj = ( * fnc )(
	    data, prel, a_ls_sp, icc_iconv_r_libspec_size( ntable ),
	    sizeof( ICONV_S_RELPTR )
	);
    }

    i = 0;
    ICC_SPANUSED( ICC_METHODS, p_mt, p_hd->icc_methods ) {
	
	a_mt->iconv_num = i ++;
	a_mt->iconv_algobject = icc_setused( p_hd->icc_objects, p_mt->icc_aobj );
	a_mt->iconv_algorithm = icc_setused( p_hd->icc_symbols, p_mt->icc_cnv );
	a_mt->iconv_ctlobject = icc_setused( p_hd->icc_objects, p_mt->icc_cobj );
	a_mt->iconv_control = icc_setused( p_hd->icc_symbols, p_mt->icc_ctl );

	p_mt->icc_obj->icc_robj = ( * fnc )(
	    data, prel, a_mt, sizeof( a_mt ), sizeof( ICONV_S_RELPTR )
	);
    }
    
    /* Dump resource list						*/
    memset( a_rl, 0, sizeof( a_rl ) );
    memset( a_lr, 0, sizeof( a_lr ) );
    j = p_hd->icc_resources->icc_count;
    a_rl->iconv_count = j;

    p_hd->icc_resources->icc_robj =
	( * fnc )(
	    data, prel, a_rl,
	    offsetof( ICONV_S_RESOURCE_LIST, iconv_resources ),
	    sizeof( ICONV_S_RELPTR )
	);
    pp_obj = p_hd->icc_resources->icc_array;
    
    /* Dump each resource						*/
    for ( i = 0; i < j; i ++ ) {
	ICC_RES_VALUES	    * p_vvals;
	
	p_obj = pp_obj[ i ];
	a_lr->iconv_resname = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	a_lr->iconv_value = ( (ICC_RES_VALUES * )(p_obj->icc_data2) )->icc_robj;
	
	p_obj->icc_robj = ( * fnc )(
	    data,
	    prel,
	    a_lr, sizeof( a_lr ), sizeof( ICONV_S_RELPTR )
	);
	
#if TESTDEBUG
printf( "resource %d = %s\n", i,
	OBJ(p_obj->icc_data1)->icc_data1
);
#endif
    }

    /* Dump the values for a resource with a magic number      		*/
    for ( i = 0; i < j; i ++ ) {
	p_obj = pp_obj[ i ];

	icc_write_resource(
	    ( ICC_RES_VALUES * )( p_obj->icc_data2 ),
	    data,
	    prel, fnc
	);
	
#if TESTDEBUG
printf( "Dumping values for resource %d = %s\n", i,
	OBJ(p_obj->icc_data1)->icc_data1
);
#endif
    }
    
    i = 0;
    /* Dump objects							*/
    ICC_SPANUSED( ICC_OBJ_TYP, p_obj, p_hd->icc_objects ) {

	a_os->iconv_num = i;
	a_os->iconv_name = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	
	p_obj->icc_robj = 
	    ( * fnc )( data, prel, a_os, sizeof( a_os ), sizeof( ICONV_S_RELPTR ) );

#if TESTDEBUG
printf( "OBJ %d = %s @ %d\n", p_obj->icc_num, OBJ( p_obj->icc_data1 )->icc_data1, p_obj->icc_robj );
#endif

	i ++;
    }

    i = 0;
    /* Dump symbols							*/
    ICC_SPANUSED( ICC_OBJ_TYP, p_obj, p_hd->icc_symbols ) {
	
	a_sy->iconv_num = i;
	a_sy->iconv_name = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	
	p_obj->icc_robj = 
	    ( * fnc )( data, prel, a_sy, sizeof( a_sy ), sizeof( ICONV_S_RELPTR ) );

	i ++;
    }

    i = 0;
    /* Dump files							*/
    ICC_SPANUSED( ICC_OBJ_TYP, p_obj, p_hd->icc_files ) {
	
	a_fl->iconv_num = i;
	a_fl->iconv_name = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	
	p_obj->icc_robj = 
	    ( * fnc )( data, prel, a_fl, sizeof( a_fl ), sizeof( ICONV_S_RELPTR ) );

#if TESTDEBUG
printf( "FILE %d = %s @ %d\n", p_obj->icc_num, OBJ( p_obj->icc_data1 )->icc_data1, p_obj->icc_robj );
#endif

	i ++;
    }

    /* Dump locale alias						*/
    memset( a_al, 0, sizeof( a_al ) );
    memset( a_la, 0, sizeof( a_la ) );
    j = p_hd->icc_loc_alias->icc_count;
    a_al->iconv_count = j;

    p_hd->icc_loc_alias->icc_robj =
	( * fnc )(
	    data, prel, a_al,
	    offsetof( ICONV_S_ALIAS_LIST, iconv_aliases ),
	    sizeof( ICONV_S_RELPTR )
	);
    pp_obj = p_hd->icc_loc_alias->icc_array;
    
    for ( i = 0; i < j; i ++ ) {
	p_obj = pp_obj[ i ];
	a_la->iconv_locale = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	a_la->iconv_value = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data2) );
	
	p_obj->icc_robj = ( * fnc )(
	    data,
	    prel,
	    a_la, sizeof( a_la ), sizeof( ICONV_S_RELPTR )
	);
	
#if TESTDEBUG
printf( "alias %d = %s %s\n", i,
	OBJ(p_obj->icc_data1)->icc_data1,
	OBJ(p_obj->icc_data2)->icc_data1
);
#endif

    }
    
    /* Dump locale codeset						*/
    memset( a_cl, 0, sizeof( a_cl ) );
    memset( a_lc, 0, sizeof( a_lc ) );
    j = p_hd->icc_loc_codeset->icc_count;
    a_cl->iconv_count = j;

    p_hd->icc_loc_codeset->icc_robj =
	( * fnc )(
	    data, prel, a_cl,
	    offsetof( ICONV_S_CODESET_LIST, iconv_codesets ),
	    sizeof( ICONV_S_RELPTR )
	);
    pp_obj = p_hd->icc_loc_codeset->icc_array;
    
    for ( i = 0; i < j; i ++ ) {
	p_obj = pp_obj[ i ];
	a_lc->iconv_locale = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data1) );
	a_lc->iconv_value = icc_setused( p_hd->icc_strings, OBJ(p_obj->icc_data2) );
	
	p_obj->icc_robj = ( * fnc )(
	    data,
	    prel,
	    a_lc, sizeof( a_lc ), sizeof( ICONV_S_RELPTR )
	);
	
#if TESTDEBUG
printf( "codeset %d = %s %s\n", i,
	OBJ(p_obj->icc_data1)->icc_data1,
	OBJ(p_obj->icc_data2)->icc_data1
);
#endif

    }
    


    /* Dump strings							*/    
    ICC_SPANUSED( ICC_OBJ_TYP, p_obj, p_hd->icc_strings ) {

	/* Make sure we have data					*/
	if ( ( ! p_obj->icc_used1 ) && p_obj->icc_data1 ) {
	    p_obj->icc_robj = 
		( * fnc )(
		    data,
		    prel,
		    p_obj->icc_data1,
		    1 + strlen( p_obj->icc_data1 ),
		    1
		);
#if TESTDEBUG
printf( "dumped string \"%s\" at %d\n", p_obj->icc_data1, p_obj->icc_robj );
#endif
	} else if ( ! p_obj->icc_used1 ) {
	    p_obj->icc_robj = 0;
	}
    }

    return;

} /* end icc_dump_data */


/* ======== icc_cmpclone ============================================== */
/* PURPOSE:
 *	Compare a clone record and a spec record.
 *
 * RETURNS:
 * 	
 */

int	icc_cmpclone(
    ICC_HEAD	    * p_hd,
    ICC_CLONEOBJ    * p_clone,
    ICC_SPEC	    * p_spec
) {

    ICC_OBJ_TYP	    * enc;

    enc = p_clone->icc_src_dst ? p_spec->icc_to_enc : p_spec->icc_from_enc;

    return (
	( p_clone->icc_enc == enc )
	&& ( p_spec->icc_mth == p_clone->icc_mth )
    );

} /* end icc_cmpclone */



/* ======== icc_specsize ============================================== */
/* PURPOSE:
 *	Spec size is determined by number of joins !  This is
 *  oversize by one table to accomodate position info.
 *
 * RETURNS:
 * 	
 */

long	icc_specsize(
    long	ntable
) {

    return sizeof( ICC_SPEC ) + ( ntable ) * sizeof( struct ICC_SPECTAB );

} /* end icc_specsize */



/* ======== icc_mkclone =============================================== */
/* PURPOSE:
 *	Make a clone.
 *
 * RETURNS:
 * 	
 */

int	icc_mkclone(
    ICC_HEAD	    * p_hd,
    ICC_CLONEOBJ    * p_clone,
    ICC_SPEC	    * p_spec
) {

    ICC_OBJ_TYP	    * enc_src;
    ICC_OBJ_TYP	    * enc_dst;
    ICC_SPEC	    * a_spec;
    long	      i;

    if ( p_clone->icc_src_dst ) {
	enc_dst = p_clone->icc_newenc;
	enc_src = p_spec->icc_from_enc;
    } else {
	enc_dst = p_spec->icc_to_enc;
	enc_src = p_clone->icc_newenc;
    }

    /* Check is spec already exists					*/
    if ( icc_getspec( p_hd, enc_src, enc_dst ) ) return 0;

    PSPEC( "Cloning from", p_spec )
    
    a_spec = ( ICC_SPEC * ) inx_alloc(i = icc_specsize(p_spec->icc_ntable));
    memcpy( a_spec, p_spec, i );
    
    a_spec->icc_from_enc = enc_src;
    a_spec->icc_to_enc = enc_dst;
    a_spec->icc_mth = p_clone->icc_newmth;

    icc_insert( p_hd->icc_specs, a_spec->icc_obj );
        
    PSPEC( "Cloning to", a_spec )
    
    return 1;

} /* end icc_mkclone */


/* ======== icc_do_clones ============================================= */
/* PURPOSE:
 *	Process clone records
 *
 * RETURNS:
 * 	
 */

int icc_do_clones(
    ICC_HEAD	    * p_hd
) {
    ICC_CLONEOBJ    * p_clone;
    ICC_SPEC	    * p_spec;
    int		      ndone = 0;

    ICC_SPANEM( ICC_CLONEOBJ, p_clone, p_hd->icc_clones ) {
	ICC_SPANEM( ICC_SPEC, p_spec, p_hd->icc_specs ) {
	    if ( icc_cmpclone( p_hd, p_clone, p_spec ) ) {
		ndone += icc_mkclone( p_hd, p_clone, p_spec );
	    }
	}
    }
    
    return ndone;

} /* end icc_do_clones */


/* ======== icc_cmpjoin1 ============================================== */
/* PURPOSE:
 *	Check to see if spec meets first join criteria 
 *
 * RETURNS:
 * 	
 */

int	icc_cmpjoin1(
    ICC_HEAD	    * p_hd,
    ICC_JOINOBJ     * p_join,
    ICC_SPEC	    * p_spec
) {

    return (
	( p_join->icc_dst_enc == p_spec->icc_to_enc )
	&& ( p_join->icc_mth_dst == p_spec->icc_mth )
    );

} /* end icc_cmpjoin1 */


/* ======== icc_cmpjoin2 ============================================== */
/* PURPOSE:
 *	Compare the second element of the join
 *
 * RETURNS:
 * 	
 */

int	icc_cmpjoin2(
    ICC_HEAD	    * p_hd,
    ICC_JOINOBJ     * p_join,
    ICC_SPEC	    * p_spec1,
    ICC_SPEC	    * p_spec2
) {

    return (
	( p_join->icc_src_enc == p_spec2->icc_from_enc )
	&& ( p_join->icc_mth_src == p_spec2->icc_mth )
    );
} /* end icc_cmpjoin2 */


/* ======== icc_mkjoin ================================================ */
/* PURPOSE:
 *	Make a join record
 *
 * RETURNS:
 * 	1 if join made 0 otherwise
 */

int	icc_mkjoin(
    ICC_HEAD	    * p_hd,
    ICC_JOINOBJ     * p_join,
    ICC_SPEC	    * p_spec1,
    ICC_SPEC	    * p_spec2
) {

    ICC_SPEC	    * a_spec;
    int		      num;

    /* Check is spec already exists					*/
    if ( icc_getspec( p_hd, p_spec1->icc_from_enc, p_spec2->icc_to_enc ) ) {
	return 0;
    }
    
    PSPEC( "Joining 1", p_spec1 )
    PSPEC( "and 2", p_spec2 )
    
    num = p_spec1->icc_ntable + p_spec2->icc_ntable;
    
    a_spec = ( ICC_SPEC * ) inx_alloc(icc_specsize( num ));

    memcpy( a_spec, p_spec1, icc_specsize( p_spec1->icc_ntable ) );

    a_spec->icc_to_enc = p_spec2->icc_to_enc;
    a_spec->icc_mth = p_join->icc_newmth;
    a_spec->icc_ntable = num;
    
    /* copy table info							*/

    memcpy(
	a_spec->icc_table + p_spec1->icc_ntable,
	p_spec2->icc_table,
	p_spec2->icc_ntable * sizeof( p_spec2->icc_table[ 0 ] )
    );
    
    
    icc_insert( p_hd->icc_specs, a_spec->icc_obj );

    PSPEC( "to create spec", a_spec )
    
    return 1;

} /* end icc_mkjoin */


/* ======== icc_do_joins ============================================== */
/* PURPOSE:
 *	Process join records
 *
 * RETURNS:
 * 	
 */

int	icc_do_joins(
    ICC_HEAD	    * p_hd
) {
    ICC_JOINOBJ     * p_join;
    ICC_SPEC	    * p_spec_dst;
    ICC_SPEC	    * p_spec_src;
    int		      ndone = 0;

    ICC_SPANEM( ICC_JOINOBJ, p_join, p_hd->icc_joins ) {
	ICC_SPANEM( ICC_SPEC, p_spec_dst, p_hd->icc_specs ) {
	    if ( icc_cmpjoin1( p_hd, p_join, p_spec_dst ) ) {
		ICC_SPANEM( ICC_SPEC, p_spec_src, p_hd->icc_specs ) {
		    if ( icc_cmpjoin2( p_hd, p_join, p_spec_dst, p_spec_src ) ) {
			ndone += icc_mkjoin( p_hd, p_join, p_spec_dst, p_spec_src );
		    }
		}
	    }
	}
    }
    
    return ndone;

} /* end icc_do_joins */


/* ======== icc_procstr =============================================== */
/* PURPOSE:
 *	Process strings that are used in encodings.  These will
 *  be sorted and stored near the spec keys for the sake of
 *  memory locality.
 *
 * RETURNS:
 * 	
 */

void	icc_procstr(
    ICC_HEAD	    * p_hd,
    ICC_OBJ_TYP	  *** ppp_str,
    int		      nstr
) {
    ICC_OBJ_TYP	   ** pp_str;
    ICC_OBJ_TYP	   ** pp_stmp;
    ICC_OBJ_TYP	    * p_obj;
    int		      i;

    * ppp_str = pp_stmp = pp_str = ( ICC_OBJ_TYP ** ) inx_alloc(
	nstr * sizeof( * pp_str )
    );

    ICC_SPANEM( ICC_OBJ_TYP, p_obj, p_hd->icc_strings ) {
	if ( p_obj->icc_used1 ) {
	    * ( pp_stmp ++ ) = p_obj;
	}
	
#if TESTDEBUG
printf( "encstr %d = %s @ %d\n", p_obj->icc_used1, p_obj->icc_data1, p_obj->icc_robj );
#endif	

    }

    /* Sort the string list.						*/
    qsort(
	pp_str,
	nstr,
	sizeof( * pp_str ),
	( icc_qsort_compar ) icc_cmpstr
    );

    /* Assign a numbering for the strings.				*/
    for ( i = 0; i < nstr; i ++ ) {
	pp_str[ i ]->icc_num = i;
	
#if TESTDEBUG
p_obj=pp_str[ i ];
printf( "sortstr %d = %s @ %d\n", p_obj->icc_used1, p_obj->icc_data1, p_obj->icc_robj );
#endif	

    }

    return;

} /* end icc_procstr */


/* ======== icc_markused ============================================== */
/* PURPOSE:
 *	Mark used flags
 *
 * RETURNS:
 * 	
 */

int	icc_markused(
    ICC_OBJ_HEAD    * p_oh,
    ICC_OBJ_TYP	    * p_obj,
    int		      use
) {

    if ( ! p_obj ) {
	return 0;
    }

    if ( ! p_obj->icc_used ) {
	p_oh->icc_usedcount ++;
	p_obj->icc_used = 1;
	p_obj->icc_used1 = use;
	return 1;
    }
    
    return 0;

} /* end icc_markused */


/* ======== icc_strobjcmp_data1 ======================================= */
/* PURPOSE:
 *	Compare objects where data 1 is a string object
 *
 * RETURNS:
 * 	int comparison
 */

int	icc_strobjcmp_data1(
    ICC_OBJ_TYP		** pp_a,
    ICC_OBJ_TYP		** pp_b
) {
    char		 * sa;
    char		 * sb;
    ICC_OBJ_TYP		 * p_a = * pp_a;
    ICC_OBJ_TYP		 * p_b = * pp_b;

    if ( ! p_a ) {
	if ( ! p_b ) {
	    return 0;
	}
	return 1;
    } else if ( ! p_b ) {
	return -1;
    }

    if ( p_a == p_b ) {
	return 0;
    }

    p_a = p_a->icc_data1;
    p_b = p_b->icc_data1;

    if ( ! p_a ) {
	if ( ! p_b ) {
	    return 0;
	}
	return 1;
    } else if ( ! p_b ) {
	return -1;
    }

    if ( p_a == p_b ) {
	return 0;
    }

    sa = p_a->icc_data1;
    sb = p_b->icc_data1;

    if ( ! sa ) {
	if ( ! sb ) {
	    return 0;
	}
	return 1;
    } else if ( ! sb ) {
	return -1;
    }

    return strcmp( sa, sb );

} /* end icc_strobjcmp_data1 */



/* ======== icc_markstring ============================================ */
/* PURPOSE:
 *	Mark a locale object's strings as used
 *
 * RETURNS:
 * 	
 */

void	icc_markstring(
    ICC_OBJ_TYP	    * p_obj,
    int               sel
) {

    ICC_HEAD	    * p_hd = icc_hd;

    if ( sel==0 || sel==1 )
         icc_markused( p_hd->icc_strings, p_obj->icc_data1, 0 );

    if ( sel==0 || sel==2 )
         icc_markused( p_hd->icc_strings, p_obj->icc_data2, 0 );

    return;

} /* end icc_markstring */



/* ======== fix_objs ================================================== */
/* PURPOSE:
 *	Create an array and sort the list per the funcion
 *
 * RETURNS:
 * 	
 */

void	fix_objs(
    ICC_OBJ_HEAD	* p_oh,
    icc_qsort_compar	  func,
    void		( * mark_func )( ICC_OBJ_TYP * p_obj, int sel ),
    int                   sel
) {

    ICC_OBJ_TYP	   ** p_arr;
    ICC_OBJ_TYP	    * p_obj;

    /* remove any previous array					*/
    if ( p_oh->icc_array ) {
	inx_free( p_oh->icc_array );
    }

    /* allocate a new array						*/
    p_arr = ( ICC_OBJ_TYP ** )
	inx_alloc( p_oh->icc_count * sizeof( ICC_OBJ_TYP * ) );
    p_oh->icc_array = p_arr;
    p_oh->icc_robj = 0;

    /* populate the array						*/
    ICC_SPANEM( ICC_OBJ_TYP, p_obj, p_oh ) {
	* ( p_arr ++ ) = p_obj;
	( * mark_func )( p_obj, sel );
    }

    /* Sort the array							*/
    qsort(
	p_oh->icc_array,
	p_oh->icc_count,
	sizeof( p_oh->icc_array[ 0 ] ),
	func
    );

    return;

} /* end fix_objs */


/* ======== icc_prepare_resource ====================================== */
/* PURPOSE:
 *	Find correct alignment etc for resource.
 *
 */

void	icc_prepare_resource(
    ICC_RES_VALUES	* p_vvals
) {

    ICC_RES_VALUE	* p_vval;
    ICC_TYPEDEF		* p_td;
    int			  offset = 0;

    p_vvals->icc_align = 1;	/* alignment must be at least 1		*/
    p_vval = p_vvals->icc_head;

    /* Loop thru all values and align them appropriately		*/
    while ( p_vval ) {
	p_td = p_vval->icc_type;

	/* Find the overall alignment requirement			*/
	if ( p_vvals->icc_align < p_td->icc_algn_output ) {
	    p_vvals->icc_align = p_td->icc_algn_output;
	}

	/* align the object - this is the offset			*/
	offset = ALIGN( offset, p_td->icc_algn_output );
	p_vval->icc_offset = offset;

	/* Increment the offset						*/
	offset += p_td->icc_size_output;

	/* Go to the next object					*/
	p_vval = p_vval->icc_next;	
    }

    /* Align the object to it's final size				*/
    offset = ALIGN( offset, p_vvals->icc_align );
    p_vvals->icc_size = offset;

    return;

} /* end icc_prepare_resource */


/* ======== icc_write_resource ======================================== */
/* PURPOSE:
 *	Write a resource.
 *
 */

void	icc_write_resource(
    ICC_RES_VALUES	* p_vvals,
    char		* ptr,
    int			* prel,
    ICC_CPYFNC	          fnc
) {
    ICC_RES_VALUE	* p_vval;
    ICC_TYPEDEF		* p_td;
    int			  offset;
    int                   magic_resvals = ICONV_MAGIC_RESVALS;
    ICC_OBJ_TYP       * p_obk;
    ICC_OBJ_TYP	      * p_obj;

    /* Function icc_cnv_func() isn't called at 1st icc_dump_data()
       call. These object should be marked because of SPANUSED
       for objects, symbols and files.
    */
    p_vval = p_vvals->icc_head;
    
    while ( p_vval ) {

        if ( p_vval->icc_type->icc_typeid == ICC_TYP_REFER  ||
	     p_vval->icc_type->icc_typeid == ICC_TYP_STRING   ) {

	     p_obj = * ( ICC_OBJ_TYP ** ) ( p_vval->icc_data );
	     icc_markused( p_obj->icc_objhead, p_obj, 0 );

             if ( p_vval->icc_type->icc_typeid == ICC_TYP_REFER && p_obj->icc_data2 ) { 
                  p_obk = *(( ICC_OBJ_TYP ** ) ( p_vval->icc_data )+1);
	          icc_markused( p_obk->icc_objhead, p_obk, 0 );
             }
        }
	p_vval = p_vval->icc_next;	
    }

    /* start with the overall alignment reqmt				*/
    p_vvals->icc_robj = offset = ALIGN( * prel, p_vvals->icc_align );

    /* Dump a magic # of values for a resource     			*/
    if ( ptr ) memset( ptr + * prel, 0xff, offset - * prel );
    ( * fnc )( ptr, prel, &magic_resvals, sizeof( magic_resvals ), 
                                          sizeof( ICONV_S_RELPTR ) 
    );
    offset = ALIGN( * prel, sizeof( ICONV_S_RELPTR ) );

    /* Dump values for a resource     			                */
    if ( ptr ) {
	memset( ptr + * prel, 0xff, offset - * prel + p_vvals->icc_size );

	/* span through all elements					*/
	p_vval = p_vvals->icc_head;
    
	/* Loop thru all values and align them appropriately		*/
	while ( p_vval ) {
	    p_td = p_vval->icc_type;

	    ( * p_td->icc_cnv_func )(
		ptr + p_vval->icc_offset + offset,
		p_vval
	    );
#if TESTDEBUG
printf( "   Dumped type %s at %d\n",
    p_td->icc_typename, p_vval->icc_offset + offset
);
#endif
	    /* Go to the next object					*/
	    p_vval = p_vval->icc_next;	
	}
    }

    * prel = offset + p_vvals->icc_size;

    return;

} /* end icc_write_resource */




/* ======== process =================================================== */
/* PURPOSE:
 *	The function to process.
 *
 * RETURNS:
 * 	
 */

void	process( char * filename )
{

    ICC_HEAD	    * p_hd = icc_hd;
    ICC_SPEC	    * p_sp;
    ICC_SPEC	   ** pp_sp;
    ICC_SPEC	   ** pp_top;
    ICC_STDLIBSPEC  * p_ls_sp;
    ICC_STDLIBSPEC ** pp_ls_sp;
    ICC_STDLIBSPEC ** pp_ls_top;
    int		      i;
    int		      k;

    char	    * data;
    int		      rel;
    char	      fname[ 1024 ];
    int		      fd;

    ICC_OBJ_TYP	    * p_obj;
    ICC_OBJ_TYP	   ** pp_str;
    int		      nstr;
    
    ICONV_S_SPEC    * a_sp;
    ICONV_S_LSPEC   * a_ls_sp;
    int		      maxtable = 1;
    ICC_OBJ_TYP	   ** pp_obj;
    

    /* process clones and joins						*/
    /* A join may produce somthing cloneable and a clone may produce	*/
    /* somthing joinable so loop the icc_do_clones and icc_do_joins	*/
    /* until no more joins or clones are produced.  Limit this to	*/
    /* avoid an infinite loop.						*/
    
    for (
	i = 1024;
	i  && ( k = ( icc_do_clones( p_hd ) || icc_do_joins( p_hd ) ) );
	i --
    );

    if ( k ) {
	inx_error(
	    0,
	    ALEX_TABLE,
	    ALEX_TOODEEP,
	    INX_ERROR
	);
    }

    /* Can only produce an iconvtab file if there exist specs !		*/
    if ( ! (
	p_hd->icc_specs->icc_count
	|| p_hd->icc_libspecs->icc_count
	|| p_hd->icc_loc_alias->icc_count
	|| p_hd->icc_loc_codeset->icc_count
	|| p_hd->icc_resources->icc_count
    ) ) {
	fprintf( stderr, "Found no specs in input\n" );
	exit( 0 );
    }

    pp_top =
    pp_sp = ( ICC_SPEC ** )
	inx_alloc( p_hd->icc_specs->icc_count * sizeof( ICC_SPEC * ) );

    pp_ls_top =
    pp_ls_sp = ( ICC_STDLIBSPEC ** )
	inx_alloc( p_hd->icc_libspecs->icc_count * sizeof( ICC_STDLIBSPEC * ) );

    nstr = 0;
    ICC_SPANEM( ICC_SPEC, p_sp, p_hd->icc_specs ) {
	* ( pp_sp ++ ) = p_sp;

	if ( maxtable < p_sp->icc_ntable ) {
	    maxtable = p_sp->icc_ntable;
	}

	nstr += icc_markused( p_hd->icc_strings, p_sp->icc_from_enc, 1 );	
	nstr += icc_markused( p_hd->icc_strings, p_sp->icc_to_enc, 1 );	
    }

    a_sp = ( ICONV_S_SPEC * ) inx_alloc( icc_iconv_r_spec_size( maxtable ) );
    memset( a_sp, 0, icc_iconv_r_spec_size( maxtable ) );

    maxtable = 0;
    ICC_SPANEM( ICC_STDLIBSPEC, p_ls_sp, p_hd->icc_libspecs ) {
	* ( pp_ls_sp ++ ) = p_ls_sp;

	if ( maxtable < ( p_ls_sp->icc_meths->icc_ntable_mbwc + p_ls_sp->icc_meths->icc_ntable_wcmb ) ) {
	    maxtable = ( p_ls_sp->icc_meths->icc_ntable_mbwc + p_ls_sp->icc_meths->icc_ntable_wcmb );
	}

	nstr += icc_markused( p_hd->icc_strings, p_ls_sp->icc_from_enc, 1 );
	nstr += icc_markused( p_hd->icc_strings, p_ls_sp->icc_to_enc, 1 );
    }

    a_ls_sp = ( ICONV_S_LSPEC * ) inx_alloc( icc_iconv_r_libspec_size( maxtable ) );
    memset( a_ls_sp, 0, icc_iconv_r_libspec_size( maxtable ) );

    /* process strings that are used in encodings			*/
    icc_procstr( p_hd, & pp_str, nstr );

    qsort(
	pp_top,
	p_hd->icc_specs->icc_count,
	sizeof( * pp_top ),
	( icc_qsort_compar ) icc_compar
    );

    for ( i = p_hd->icc_specs->icc_count, pp_sp = pp_top; i; pp_sp ++, i -- ) {
	p_sp = * pp_sp;
	p_sp->icc_obj->icc_num = pp_sp - pp_top;

	PSPEC( "spec dump", p_sp )
    }

    qsort(
	pp_ls_top,
	p_hd->icc_libspecs->icc_count,
	sizeof( * pp_ls_top ),
	( icc_qsort_compar ) icc_compar
    );

    for ( i = p_hd->icc_libspecs->icc_count, pp_ls_sp = pp_ls_top; i; pp_ls_sp ++, i -- ) {
	p_ls_sp = * pp_ls_sp;
	p_ls_sp->icc_obj->icc_num = pp_ls_sp - pp_ls_top;

	PLSPEC( "libspec dump", p_ls_sp )
    }

    /* create arrays for aliases, codesets and resources that will be dumped*/
    fix_objs(
	p_hd->icc_loc_alias,
	( icc_qsort_compar ) icc_strobjcmp_data1,
	icc_markstring, 0
    );
    
    fix_objs(
	p_hd->icc_loc_codeset,
	( icc_qsort_compar ) icc_strobjcmp_data1,
	icc_markstring, 0
    );

    fix_objs(
	p_hd->icc_resources,
	( icc_qsort_compar ) icc_strobjcmp_data1,
	icc_markstring, 1
    );

    pp_obj = p_hd->icc_resources->icc_array;
    for ( i = p_hd->icc_resources->icc_count; i > 0; i -- ) {
	/* Is it really defined or is it just a reference		*/
	if ( ! ( * pp_obj )->icc_data2 ) {
	    inx_error(
		( * pp_obj )->icc_posi,
		ALEX_TABLE,
		ALEX_NOT_DEFINED,
		INX_LINE | INX_ERROR | INX_EXTRA,
		OBJ( ( * pp_obj )->icc_data1 )->icc_data1
	    );
	} else {
	    icc_prepare_resource(
		( ICC_RES_VALUES * ) ( ( * pp_obj )->icc_data2 )
	    );
	}
	pp_obj ++;
    }

    /* Check to make sure no errors are found				*/
    if ( inx_iserror() ) {
	exit( 1 );
    }

    /* Dump data to file - This time we dump it only to get relative	*/
    /* pointers set as well as find size required			*/
    rel = 0;
    icc_dump_data(
	p_hd, ( char * ) 0, & rel, icc_nulfnc,
	pp_top, pp_ls_top, nstr, pp_str, a_sp, a_ls_sp
    );

    data = inx_alloc( rel );
    rel = 0;
    icc_dump_data(
	p_hd, data, & rel, icc_cpyfnc,
	pp_top, pp_ls_top, nstr, pp_str, a_sp, a_ls_sp
    );

    inx_free( a_sp );

    sprintf( fname, "%s.new", filename );
    fd = creat( fname, 0444 );

    if ( fd < 0 ) {
	perror( fname );
	exit( 1 );
    }
    
    fchmod( fd, 0444 );

    if ( rel != write( fd, data, rel ) ) {
	perror( fname );
	close( fd );
	unlink( fname );
	exit( 1 );
    }

    close( fd );

    if ( rename( fname, filename ) < 0 ) {
	perror( fname );
	unlink( fname );
	exit( 1 );
    }

    /* File successfully written !					*/
    inx_free( data );

    return;

} /* end process */


/* ======================================================================== */
/* Type conversion functions						*/


int	icc_cnv_int(
    int		      * p_dst,
    ICC_RES_VALUE     * p_src
) {

    * p_dst = * ( long long * ) p_src->icc_data;

    return sizeof( * p_dst );

} /* end icc_cnv_int */

int	icc_cnv_longlong(
    long long         * p_dst,
    ICC_RES_VALUE     * p_src
) {

    * p_dst = * ( long long * ) p_src->icc_data;

    return sizeof( * p_dst );

} /* end icc_cnv_longlong */

int	icc_cnv_short(
    short	      * p_dst,
    ICC_RES_VALUE     * p_src
) {

    * p_dst = * ( long long * ) p_src->icc_data;

    return sizeof( * p_dst );

} /* end icc_cnv_short */

int	icc_cnv_byte(
    char	      * p_dst,
    ICC_RES_VALUE     * p_src
) {

    * p_dst = * ( long long * ) p_src->icc_data;

    return sizeof( * p_dst );

} /* end icc_cnv_byte */

int	icc_cnv_string(
    ICC_RELPTR	      * p_dst,
    ICC_RES_VALUE     * p_src
) {
    ICC_OBJ_TYP	      * p_obj = * ( ICC_OBJ_TYP ** ) ( p_src->icc_data );

    * p_dst = p_obj->icc_robj;
    icc_markused( p_obj->icc_objhead, p_obj, 0 );

    return sizeof( * p_dst );

} /* end icc_cnv_string */


int	icc_cnv_reference(
    ICONV_REFER       * p_dst,
    ICC_RES_VALUE     * p_src
) {
    ICC_OBJ_TYP	      * p_obj = * ( ICC_OBJ_TYP ** ) ( p_src->icc_data );
    ICC_OBJ_TYP       * p_obk;

    memset( p_dst, 0, sizeof( *p_dst ) );

    if ( ! p_obj->icc_data2 ) {  /* FILE */

	p_dst->iconv_node_type = ICONV_REFER_FILE;
	p_dst->_objs._file.iconv_file_ref  = p_obj->icc_robj;

    } else {                     /* SYMBOLS */

	p_dst->iconv_node_type = ICONV_REFER_SYMBOL;
	p_dst->_objs._symbols.iconv_symbol = p_obj->icc_robj;

        p_obk = *(( ICC_OBJ_TYP ** ) ( p_src->icc_data )+1);
	p_dst->_objs._symbols.iconv_object = p_obk->icc_robj;


#ifdef TESTDEBUG2
	printf( "icc_cnv_reference(): DSO name: %s\n",
	         OBJ(p_obk->icc_data1)->icc_data1 );
        fflush(stdout);
#endif
    }

    return sizeof( * p_dst );

} /* end icc_cnv_reference */



#define ICC_MAKE_TYPE( B, A, C, D, E )					\
    { (A), (B), sizeof(C), sizeof(D), sizeof(D), ( ICC_CNV_FUNC ) (E) }

#define ICC_MAKE_TYPF( B, A, C, D, E, F )					\
    { (A), (B), sizeof(C), sizeof(D), sizeof(E), ( ICC_CNV_FUNC ) (F) }

typedef long long llong;

ICC_TYPEDEF	icc_typetab[] = {
    ICC_MAKE_TYPE(
	           "string",    ICC_TYP_STRING,        void *, 
                                ICONV_S_RELPTR,        icc_cnv_string
    ),
    ICC_MAKE_TYPE( "int64", ICC_TYP_INT, llong, llong, icc_cnv_longlong ),
    ICC_MAKE_TYPE( "int32", ICC_TYP_INT, llong, int,   icc_cnv_int ),
    ICC_MAKE_TYPE( "int",   ICC_TYP_INT, llong, int,   icc_cnv_int ),
    ICC_MAKE_TYPE( "int16", ICC_TYP_INT, llong, short, icc_cnv_short ),
    ICC_MAKE_TYPE( "int8",  ICC_TYP_INT, llong, int,   icc_cnv_byte ),
    ICC_MAKE_TYPE( "short", ICC_TYP_INT, llong, short, icc_cnv_short ),
    ICC_MAKE_TYPE( "char",  ICC_TYP_INT, llong, char,  icc_cnv_byte ),    
    ICC_MAKE_TYPF(     
                   "reference", ICC_TYP_REFER,         void *,       
                                ICONV_REFER,           ICONV_S_RELPTR,                   
                                                       icc_cnv_reference 
    ),
};

int	icc_ntypetab = sizeof( icc_typetab )/sizeof( * icc_typetab );

/* ======== mkresource ================================================ */
/* PURPOSE:
 *	Complete the resource definition.
 *
 * RETURNS:
 * 	
 */

void * mkresource(
    char		* param_name,
    ICC_RES_VALUES	* p_vvals
) {

    ICC_OBJ_TYP	    * param_name_str = icc_getstr( param_name );
    ICC_OBJ_TYP	    * p_obj;

    ICC_HEAD	    * p_hd = icc_hd;

    if ( ! p_vvals ) {
	inx_free( param_name );
	return 0;
    }

    /* Find the object to see if it's already defined			*/
    p_obj = icc_findgeneric( p_hd->icc_resources, param_name_str, 0 );

    if ( ! p_obj ) {
	/* If not found - insert it					*/
	p_obj = icc_insertgeneric(
	    p_hd->icc_resources, param_name_str, p_vvals
	);
    } else if ( p_obj->icc_data2 ) {
	/* Found and already defined					*/
	ICC_RES_VALUES	* p_ovvals = p_obj->icc_data2;

	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_ALREADY_DEFINED,
	    INX_LINE | INX_ERROR | INX_EXTRA,
	    param_name_str->icc_data1
	);
	inx_error(
	    p_ovvals->icc_posi,
	    ALEX_TABLE,
	    ALEX_ALREADY_HERE,
	    INX_LINE | INX_INFORM | INX_EXTRA,
	    param_name_str->icc_data1
	);
	return 0;
    } else {
	/* referenced only,						*/
	p_obj->icc_data2 = p_vvals;
    }

    /* Fill in the defined postion					*/
    p_vvals->icc_posi[ 0 ] = inx_last[ 0 ];

    return 0;

} /* end mkresource */



/* ======== mkvalues ================================================== */
/* PURPOSE:
 *	Create a list of values
 *
 * RETURNS:
 * 	
 */

void	* mkvalues(
    ICC_RES_VALUES   * p_vvals,
    ICC_RES_VALUE    * p_vval
) {

    /* If there was an error - drop away				*/
    if ( ! p_vval ) {
	return 0;
    }

    /* Make sure we don't trample off the end of the list		*/
    p_vval->icc_next = 0;

    /* if this is the first value then create the container		*/
    if ( ! p_vvals ) {
	p_vvals = inx_alloc( sizeof( * p_vvals ) );
	memset( p_vvals, 0, sizeof( * p_vvals ) );
    }

    /* Is this the first element ?					*/
    if ( ! p_vvals->icc_tail ) {
	p_vvals->icc_head = p_vval;
	p_vvals->icc_tail = p_vval;
    } else {
	p_vvals->icc_tail->icc_next = p_vval;
	p_vvals->icc_tail = p_vval;
    }

    return p_vvals;

} /* end mkvalues */


/* ======== mkvalue_str =============================================== */
/* PURPOSE:
 *	Convert a string value
 *
 * RETURNS:
 * 	
 */

void	* mkvalue_str(
    ICC_TYPEDEF	    * p_td,
    char	    * ident,
    char	    * value
) {
    ICC_OBJ_TYP	    * p_val = icc_getstr( value );
    ICC_RES_VALUE   * p_vval;
    int		      i;

    if ( p_td && ( p_td->icc_typeid != ICC_TYP_STRING ) ) {
	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_EXPECT_STRING,
	    INX_LINE | INX_WARN | INX_EXTRA,
	    p_val->icc_data1
	);
	p_td = 0;
    }

    p_vval = inx_alloc(
	i = sizeof( p_val ) + sizeof( * p_vval ) - sizeof( p_vval->icc_data )
    );
    memset( p_vval, 0, i );

    * ( ICC_OBJ_TYP ** ) ( p_vval->icc_data ) = p_val;

    p_vval->icc_name = ident;
    p_vval->icc_type = p_td;

    if ( ! p_td ) {
	inx_free( p_vval->icc_name );
	inx_free( p_vval );
	return 0;
    }

    return p_vval;
    
} /* end mkvalue_str */


/* ======== mkvalue_int =============================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	* mkvalue_int(
    ICC_TYPEDEF	    * p_td,
    char	    * ident,
    char	    * value
) {
    long long	    * p_val;
    char	    * readto = 0;
    ICC_RES_VALUE   * p_vval;
    int		      i;

    /* integer type							*/
    if ( p_td && ( p_td->icc_typeid != ICC_TYP_INT ) ) {
	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_EXPECT_INTEGER,
	    INX_LINE | INX_WARN | INX_EXTRA,
	    value
	);
	p_td = 0;
    }

    p_vval = inx_alloc(
	i = sizeof( * p_val ) + sizeof( * p_vval ) - sizeof( p_vval->icc_data )
    );

    memset( p_vval, 0, i );

    p_val = p_vval->icc_data;

    * p_val = strtoll( value, & readto, 0 );

    if ( readto == value ) {
	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_CNV_ERROR,
	    INX_LINE | INX_WARN | INX_EXTRA,
	    value
	);
    }

    inx_free( value );
    p_vval->icc_name = ident;
    p_vval->icc_type = p_td;

    if ( ! p_td ) {
	inx_free( p_vval->icc_name );
	inx_free( p_vval );
	return 0;
    }

    return p_vval;

} /* end mkvalue_int */


/* ======== mkvalue_table =============================================== */
/* PURPOSE:
 *	Convert a table value
 *
 * RETURNS:
 * 	
 */

void	* mkvalue_table(
    ICC_TYPEDEF	    * p_td,
    char	    * ident,
    ICC_OBJ_TYP	    * table
) {
    ICC_OBJ_TYP	    * p_val = table;
    ICC_RES_VALUE   * p_vval;
    int		      i;
    ICC_OBJ_TYP	    * p_obj;
    ICC_HEAD	    * p_hd = icc_hd;

    if ( p_td && ( p_td->icc_typeid != ICC_TYP_REFER ) ) {
	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_EXPECT_STRING,
	    INX_LINE | INX_WARN | INX_EXTRA,
	    (OBJ(p_val->icc_data1))->icc_data1
	);
	inx_error(
	    inx_last,
	    ALEX_TABLE,
	    ALEX_EXPECT_STRING,
	    INX_LINE | INX_WARN | INX_EXTRA,
	    (OBJ(p_val->icc_data2))->icc_data1
	);
	p_td = 0;
    }

    p_vval = inx_alloc(
	                i  =  sizeof( * p_vval ) - sizeof( p_vval->icc_data )
                           + (sizeof(   p_val  )*2)
    );
    memset( p_vval, 0, i );

    * ( ICC_OBJ_TYP ** )( p_vval->icc_data ) = p_val;

    p_vval->icc_name = ident;
    p_vval->icc_type = p_td;
   
    /* I put this in this new function: mkvalue_talble(). I don't know why this code
       fragment is not included in existing function: mktable().
    */
    if ( p_val->icc_data2 ) {  
         p_obj =  icc_getmkgeneric( p_hd->icc_objects, p_val->icc_data2, 0 );
         *( ( (ICC_OBJ_TYP **)(p_vval->icc_data) )+1 ) = p_obj;

#ifdef TESTDEBUG2
	 printf( "mkvalue_talbe(): DSO name: %s\n",
		 OBJ((*( ( (ICC_OBJ_TYP **)(p_vval->icc_data) )+1 ))->icc_data1)->icc_data1);
         fflush(stdout);
#endif
    }

    if ( ! p_td ) {
	inx_free( p_vval->icc_name );
	inx_free( p_vval );
	return 0;
    }

    return p_vval;

} /* end mkvalue_table */


/* ======== icc_find_type_spec ======================================== */
/* PURPOSE:
 *	Find a type spec.
 *
 * RETURNS:
 * 	0 if not found.
 *	pointer to ICC_TYPEDEF if found
 */

ICC_TYPEDEF * icc_find_type_spec(
    char	* name
) {
    ICC_TYPEDEF	    * p_td;
    int		      i;

    for ( i = icc_ntypetab, p_td = icc_typetab; i > 0; i --, p_td ++ ) {
	if ( ! strcmp( p_td->icc_typename, name ) ) {
	    return p_td;
	}
    }

    return 0;

} /* end icc_find_type_spec */


/* ======== mkchktyp ================================================== */
/* PURPOSE:
 *	convert a string to a typedef
 *
 * RETURNS:
 * 	ICC_TYPEDEF
 */

void	* mkchktyp(
    char	    * typename
) {

    ICC_TYPEDEF	    * p_td;

    p_td = icc_find_type_spec( typename );

    if ( ! p_td ) {
	inx_error(
	    inx_last,		/* the last position */
	    ALEX_TABLE,
	    ALEX_TYPEWRONG,
	    INX_LINE | INX_ERROR | INX_EXTRA,
	    typename
	);
    }

    inx_free( typename );

    return p_td;

} /* end mkchktyp */


#if TESTDEBUG && 0
static moretest()
{
    char	** __iconv_gettable( void );
    char	** __mbwc_gettable( void );
    char	** ps = __iconv_gettable();

    iconv_t	   pi;

    if ( ! ps ) {
	perror( "Can't open iconv at all\n" );
	return;
    }

    while ( * ps ) {

	fprintf( stderr, "Open - '%s' - '%s' ", ps[ 0 ], ps[ 1 ] );
	pi = iconv_open( ps[ 1 ], ps[ 0 ] );

	if ( pi == ( iconv_t ) -1 ) {
	    perror( "Error openeing iconv\n" );
	} else {
	    iconv_close( pi );
	}
	
	ps += 2;
    }

    ps = __mbwc_gettable();
    
    while ( * ps ) {

	fprintf( stderr, "Open - '%s' - '%s' ", ps[ 0 ], ps[ 1 ] );
	pi = ( void * ) __mbwc_find( ps[ 1 ], ps[ 0 ] );

	if ( pi == ( iconv_t ) -1 ) {
	    perror( "Error openeing mbwc\n" );
	}
	
	ps += 2;
    }

}

#endif




/* ======== main ====================================================== */
/* PURPOSE:
 *	
 *
 * RETURNS:
 * 	
 */

void	main( int argc, char ** argv )
{


#if !TESTDEBUG
    char	* name = ( char * ) ICONV_TABNAME;
#else
    char 	* name = "iconvtab";
    ICONV_TABNAME = name;
#endif

    icc_init();

    if ( ! icc_read_input( argc, argv, & name ) ) {
	process( name );	
    }

#if TESTDEBUG && 0
    fflush( stdout );
    fflush( stderr );
    iconv_test();
    moretest();
#endif

    exit( 0 );

} /* end main */

