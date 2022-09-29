/*
 * nonshrtab.c	     7-Aug-1996
 *
 *	Iconv symbol table lookup for non-shared binaries / without dlopen.
 */

#if ! _PIC

#include    "synonyms.h"
#include    <wchar.h>
#include    <locale.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <unistd.h>
#include    <errno.h>
#include    <sys/types.h>
#include    "iconv_cnv.h"
#include    "_wchar.h"

#include    "stdlib.dcls.h"
#include    "iconv.dcls.h"

/* ======== __NS_ICONV_TABTYP ========================================= */
/* PURPOSE:
 *	Entry in lookup table
 */

typedef struct  __NS_ICONV_TABTYP {
    const char	    * __ns_iconv_name;	/* Name of symbol		*/
    void	    * __ns_iconv_ptr;	/* Pointer to symbol		*/
} __NS_ICONV_TABTYP;


/* ======== __NS_ICONV_HEADTYP =========================================== */
/* PURPOSE:
 *	Header
 */

typedef struct  __NS_ICONV_HEADTYP {
    int		        __ns_iconv_ntable;  /* Number of elements in table*/
    __NS_ICONV_TABTYP const
		      * __ns_iconv_table;   /* Pointer to table		*/
} __NS_ICONV_HEADTYP;

extern const __NS_ICONV_HEADTYP ___ns_iconv_head[];
extern const __NS_ICONV_HEADTYP __ns_iconv_head[];

static const __NS_ICONV_TABTYP	__ns_iconv_symtable[] = {
#include    "symtab.h"
};

const __NS_ICONV_HEADTYP ___ns_iconv_head[] = {
    sizeof( __ns_iconv_symtable )/sizeof( * __ns_iconv_symtable ),
    __ns_iconv_symtable
};

#pragma weak __ns_iconv_head = ___ns_iconv_head


/* ======== __ns_iconv_compare ======================================== */
/* PURPOSE:
 *	Comparison routine for bsearch.
 *
 */

static int __ns_iconv_compare(
    const __NS_ICONV_TABTYP	* p_a,
    const __NS_ICONV_TABTYP	* p_b
)
{

    return strcmp( p_a->__ns_iconv_name, p_b->__ns_iconv_name );

} /* end __ns_iconv_compare */


/* ======== __ns_iconv_lookup ========================================= */
/* PURPOSE:
 *	Lookup a name in the iconv symbol table - works like dlsym
 *
 * RETURNS:
 *	1 if name found
 *	0 otherwise
 */

void *	__ns_iconv_lookup(
    void	    * handle,
    const char	    * name
) {

    __NS_ICONV_TABTYP	* p_elem;
    __NS_ICONV_TABTYP	  a_elem[ 1 ];

    a_elem->__ns_iconv_name = name;

    if ( ! handle ) {
	handle = ( void * ) __ns_iconv_head;
    }

    p_elem = bsearch(
	a_elem,
	( ( __NS_ICONV_HEADTYP * ) handle )->__ns_iconv_table,
	( ( __NS_ICONV_HEADTYP * ) handle )->__ns_iconv_ntable,
	sizeof( * p_elem ),
	( int (*)(const void *, const void *) ) __ns_iconv_compare
    );

    if ( p_elem ) {
	return p_elem->__ns_iconv_ptr;
    } else {
	return 0;
    }

} /* end __ns_iconv_lookup */


/* ======== __ns_iconv_open =========================================== */
/* PURPOSE:
 *	Like a dlopen, returns the pointer to a handle.
 *
 * RETURNS:
 * 	
 */

void	* __ns_iconv_open( const char * pathname )
{

    if ( ! pathname ) {
	return ( void * ) __ns_iconv_head;
    }

    return __ns_iconv_lookup( ( void * ) __ns_iconv_head, pathname );

} /* end __ns_iconv_open */


#endif
