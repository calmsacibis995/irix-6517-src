/*
 * matrix.c
 *
 * PURPOSE:
 * Create a list of converter functions to be generated in
 * iconv_converter.c .
 *
 * This automagically generates all converters based on a tranistional
 * network.  It's output is bascally input to the m4 scripts that
 * actually create iconv_converter.c
 *
 * The computation is basically a generation of all possible paths
 * through a network.  Each node of the network is an encoding type.
 * There are two special nodes, INPUT and OUTPUT.  When a converter
 * is created, it is a matter of adding the converter node and
 * new transitions to the network and the list is created.  This
 * operation avoids the tedious and fault prone task of going through
 * over 100 converter functions and adding the appropriate ones.
 *
 * To modify the converter list goto the line starting with HERE
 *
 */

#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <stdio.h>

/* ======== mtx_reference ============================================= */
/* PURPOSE:
 *	A work struct for holding things together.
 */

typedef struct  mtx_reference {

    struct mtx_reference    * next;
    struct mtx_converters   * ref;
    struct mtx_node	    * node;

} mtx_reference;



/* ======== mtx_node ================================================== */
/* PURPOSE:
 *	A node - either a converter type *OR* a node of the network.
 */

#define	    INP	0
#define	    OUT	1
#define	    CNV	2

typedef struct  mtx_node {

    char		* node_name;

    mtx_reference	* refs[ 3 ];   /* References as inputs		*/

} mtx_node;



/* ======== mtx_converters ============================================ */
/* PURPOSE:
 *	Conversion node
 */

#define NEXCL	    5
typedef struct  mtx_converters {

    mtx_node		* ioc_refs[ 3 ];

    mtx_node		* ioc_excludes[ NEXCL ];

    int			  in_use;

} mtx_converters;


/* ======== mtx_path ================================================== */
/* PURPOSE:
 *	For path logging
 */

#define MXSIZ	10

typedef struct  mtx_path {

    struct {
	char		included;	/* path is included		*/
	char		generated;	/* Path is generated		*/
	struct mtx_path * next;
	int		dup;		/* This is a duplicate !	*/
	int		npath;		/* Path size			*/
    } xx[1];
    mtx_node	      * path[ MXSIZ ];	/* All path elements		*/

    void	      * stops_here;

} mtx_path;


/* ======== mtx_base ================================================== */
/* PURPOSE:
 *	A place where everting hangs off
 */

typedef struct  mtx_base {

    mtx_converters    * conver;
    int		        nconver;
    mtx_path	      * excludes;
    int		        nexcludes;
    mtx_path	      * mtx_top;
    int		        npath;
    int			dup;

} mtx_base;



#define numof( A )  ( sizeof( A ) / sizeof( * (A) ) )

#define DEFNODE( X )	mtx_node    X[] = { { #X } };

/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */
/* ====================================================================
HERE									*/
/* THIS IS WERE YOU CAN ADD MORE NODES					*/


/* The EmPtY node is a once way transition node */
mtx_node    EmPtY[]	= { 0 };

/* Add nodes of the network here - these are basically data type	*/
/* or assumptions of what the data at that node may be.  Converters	*/
/* may convert from node to node with the special EmPtY node making a	*/
/* transition from node to node without actual conversion		*/

DEFNODE(INPUT)
DEFNODE(OUTPUT)
DEFNODE(BAD_UNI)    /* UTF-8L would produce this stream			*/
DEFNODE(BOM_UNI)    /* byte order mark checked BAD_UNI			*/
DEFNODE(FIXED_UNI)  /* A utf16'd BOM_UNI				*/
DEFNODE(DIRECT)	    /* Output of an mbs lookup				*/
DEFNODE(EUC_JP_WC)  /* IRIX EUC_JP wide char				*/
DEFNODE(EUC_TW_WC)  /* IRIX EUC_TW wide char				*/
DEFNODE(GOOD_UNI)   /* Guarenteed valid ISO10646 UCS4 value		*/
DEFNODE(LATIN1)	    /* Special for LATIN1				*/
DEFNODE(LAT1_RAW)   /* Special for LATIN1 - avoid too many lat1 converters*/
DEFNODE(MBCS)	    /* Special MBCS value for mbs output converter	*/
DEFNODE(RAW_BYTES)  /* Bytes as they come in				*/
DEFNODE(RAW_W_UNI)  /* Possible UCS-4 value as it is read in		*/
DEFNODE(RAW_UNI)    /* Possible UNICODE value as it is read in		*/
DEFNODE(RAW_SHORTS) /* 16 bits as a chunk directly from input		*/
DEFNODE(RAW_WORDS)  /* 32 bits as a chunk directly from input		*/
DEFNODE(SBCS)	    /* Single byte character set ready for output	*/
DEFNODE(KEYS)       /* X KeySyms (32bits)                               */
DEFNODE(KEYS_UNI)   /* X KeySyms (UCS4)                                 */

/* Add converter types here - these are transition operations not	*/
/* actual transitions.  When a new converter is made it will operate	*/
/* somehow between two nodes described above				*/
mtx_node    x7dbcs[]	= { "7dbcs" };
DEFNODE(big5wc)	    /* Converts from big5wc to EUC wide characters */
DEFNODE(bom)	    /* Detects reversed byte order mark and reverses bytes */
DEFNODE(dbcs)	    /* takes bytes for an SJIS-like encoding to a 16 bit value*/
DEFNODE(euc)	    /* Convert euc input stream to a 32 bit value using a table*/
DEFNODE(eucjpwc)    /* Convert euc_jp to IRIX wide char			*/
DEFNODE(euctwwc)    /* Convert euc_tw to IRIX wide char			*/
DEFNODE(flb)	    /* Input and output a fixed length byte		*/
DEFNODE(flh)	    /* Input and output a fixed length short (16 bits)	*/
DEFNODE(flw)	    /* Input and output a fixed length word (32 bits)	*/
DEFNODE(idxb)	    /* Take input as an index and get a byte from a table*/
DEFNODE(idxh)	    /* Take input as an index and get a short from a table*/
DEFNODE(idxw)	    /* Take input as an index and get a word from a table*/
DEFNODE(lat1)	    /* Conversion from Unicode to latin 1		*/
DEFNODE(mbs)	    /* Output special MBS output byte			*/
DEFNODE(sjiswc)	    /* Convert SJIS to IRIX wide chars			*/
DEFNODE(utf16)	    /* Convert to and output a UTF16 stream		*/
DEFNODE(utf8)	    /* Input and output UTF8				*/
DEFNODE(wcbig5mbs)  /* convert IRIX wide chars to mbs for BIG5		*/
DEFNODE(wceucjpmbs) /* convert IRIX wide chars to mbs for EUCJP		*/
DEFNODE(wceuctwmbs) /* convert IRIX wide chars to EUC_TW		*/
DEFNODE(wcsjismbs)  /* convert IRIX wide chars to sjis			*/
DEFNODE(hash)       /* convert X KeySyms to UCS4                        */

/* Add transitions from node to node using converters for transition.	*/
/* A transition may only be made once per conversion (or path through	*/
/* the network). If it is nessasary, there is allowed to be multiple	*/
/* transitions between nodes of the same converter.  Making converters	*/
/* that have more than 6 in depth will produce compilation errors	*/
/* in iconv_converter.c							*/

mtx_converters converters[] = {
{{ INPUT,	RAW_BYTES,	flb },},
{{ INPUT,	RAW_SHORTS,	flh },},
{{ INPUT,	RAW_WORDS,	flw },},
{{ INPUT,	GOOD_UNI,	utf8 },},
{{ INPUT,	BAD_UNI,	utf8 },},
{{ BAD_UNI,	BOM_UNI,	bom },},
{{ BOM_UNI,	FIXED_UNI,	utf16 },},
{{ FIXED_UNI,	OUTPUT,		utf8 },},
{{ FIXED_UNI,	OUTPUT,		flw },},
{{ RAW_BYTES,	DIRECT,		euc },},
{{ RAW_BYTES,	DIRECT,		dbcs },},
{{ RAW_BYTES,	DIRECT,		x7dbcs },},
{{ RAW_BYTES,	SBCS,		idxb },},
{{ RAW_BYTES,	LAT1_RAW,	EmPtY },},
{{ RAW_BYTES,	LATIN1,		EmPtY },},
{{ RAW_BYTES,	GOOD_UNI,	idxh },},
{{ RAW_BYTES,	GOOD_UNI,	idxw },},
{{ RAW_BYTES,	OUTPUT,		flw },},
{{ RAW_BYTES,	OUTPUT,		flh },},
{{ SBCS,	OUTPUT,		flb },},
{{ SBCS,	OUTPUT,		mbs },},
{{ RAW_WORDS,	RAW_W_UNI,	EmPtY },},
{{ RAW_WORDS,	BAD_UNI,	EmPtY },},
{{ RAW_W_UNI,	GOOD_UNI,	bom },},
{{ RAW_SHORTS,	RAW_WORDS,	EmPtY },},
{{ RAW_SHORTS,	RAW_UNI,	bom },},
{{ RAW_UNI,	GOOD_UNI,	utf16 },},
{{ RAW_UNI,	GOOD_UNI,	EmPtY },},
{{ GOOD_UNI,	LATIN1,		lat1 },},
{{ GOOD_UNI,	MBCS,		idxw },},
{{ GOOD_UNI,	MBCS,		idxh },},
{{ GOOD_UNI,	SBCS,		idxb },},
{{ GOOD_UNI,	OUTPUT,		flw },},
{{ GOOD_UNI,	OUTPUT,		flh },},
{{ GOOD_UNI,	OUTPUT,		utf16 },},
{{ GOOD_UNI,	OUTPUT,		utf8 },},
{{ DIRECT,	GOOD_UNI,	EmPtY },},
{{ DIRECT,	MBCS,		EmPtY },},
{{ MBCS,	OUTPUT,		mbs },},
{{ LATIN1,	OUTPUT,		flb },},
{{ LAT1_RAW,	OUTPUT,		utf8 },},
{{ RAW_BYTES,	EUC_JP_WC,	sjiswc },},
{{ RAW_BYTES,	EUC_JP_WC,	eucjpwc },},
{{ RAW_BYTES,	EUC_TW_WC,	big5wc },},
{{ RAW_BYTES,	EUC_TW_WC,	euctwwc },},
{{ EUC_JP_WC,	OUTPUT,		wceucjpmbs },},
{{ EUC_JP_WC,	OUTPUT,		wcsjismbs },},
{{ EUC_TW_WC,	OUTPUT,		wceuctwmbs },},
{{ EUC_TW_WC,	OUTPUT,		wcbig5mbs },},

{{ BOM_UNI,  	KEYS,		idxw },},
{{ RAW_WORDS,	KEYS_UNI,	hash },},
{{ KEYS,	OUTPUT,	        flw },},
{{ KEYS_UNI,	OUTPUT,	        flw },},
};

/* If there are any converters in particular that need to be excluded	*/
/* or (NCLUDE'ed) list them in the mtx_exclude array.			*/

#define XCLUDE		0
#define NCLUDE		1

mtx_path	mtx_exclude[] = {
{ { NCLUDE, }, { utf8, bom, utf16, utf8, } },
{ { XCLUDE, }, { utf8, utf8, } },
{ { XCLUDE, }, { flw,  flw, } },
};

/* You need not edit anything below this line				*/
/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */
/* ==================================================================== */


mtx_base    mtx_table[] = {
    { converters, numof( converters ), mtx_exclude, numof( mtx_exclude ) }    
};


/* ======== mtx_make_reference ======================================== */
/* PURPOSE:
 *	Make a reference to a node
 *
 * RETURNS:
 * 	
 */

#define NEW( id ) \
    id = malloc( sizeof( * id ) ); memset( id, 0, sizeof( * id ) )

void	mtx_make_reference(
    mtx_converters	* p_conv,
    mtx_node		* p_node,
    int			  ref_typ
) {

    mtx_reference	* p_ref;

    NEW( p_ref );

    p_ref->ref = p_conv;
    p_ref->next = p_node->refs[ ref_typ ];
    p_node->refs[ ref_typ ] = p_ref;
    p_ref->node = p_node;

    return;

} /* end mtx_make_reference */



/* ======== mtx_initialize ============================================ */
/* PURPOSE:
 *	Initialize references to all nodes.
 *
 * RETURNS:
 * 	
 */

void	mtx_initialize(
    mtx_base		* p_base
) {

    int		      i;
    mtx_converters  * p_conv = p_base->conver;

    for ( i = 0; i < p_base->nconver; i ++, p_conv ++ ) {
	mtx_make_reference( p_conv, p_conv->ioc_refs[ INP ], INP );
	mtx_make_reference( p_conv, p_conv->ioc_refs[ OUT ], OUT );
	mtx_make_reference( p_conv, p_conv->ioc_refs[ CNV ], CNV );
    }

    return;

} /* end mtx_initialize */



/* ======== mtx_log_a_path ============================================ */
/* PURPOSE:
 *	Print a path !
 *
 * RETURNS:
 * 	
 */

void	mtx_log_a_path(
    mtx_reference   * p_start,
    mtx_reference   * p_end
) {


    mtx_reference   * p_ref;
    int		      depth= 0;
    mtx_path	    * p_pth;
    mtx_node	   ** pp_node;
    int		      size;

    /* Find how many non-empty nodes					*/
    for (p_ref = p_start; p_ref; p_ref = p_ref->next ) {

	if ( p_ref->node->node_name ) {
	    depth ++;
	}
    }

    if ( ! depth ) {
	return;
    }

    /* Make a path struct of suitable size				*/
    size = sizeof( * p_pth ) + ( depth - MXSIZ ) * sizeof( mtx_node * );
    p_pth = malloc( size );

    /* Initialize the path struct and hand it off a linked list		*/
    p_pth->xx->npath = depth;
    p_pth->xx->next = mtx_table->mtx_top;

    p_pth->xx->included = 1;	/* Mark this node appropriately		*/
    p_pth->xx->generated = 1;
    
    mtx_table->mtx_top = p_pth;
    mtx_table->npath ++;
    pp_node = p_pth->path;
    p_pth->xx->dup = 0;
    p_pth->xx->generated = 1;
    p_pth->xx->included = 1;

/*    printf( "create_iconv_%d(" ); */
    for (p_ref = p_start; p_ref; p_ref = p_ref->next ) {

	if ( p_ref->node->node_name ) {
	    * pp_node = p_ref->node;
	    pp_node ++;
#if 0
	    if ( ! flag ) {
		flag = 1;
	    } else {
		printf(",");
	    }
	    printf("%s", p_ref->node->node_name );
#endif
	}
    }
/*printf(")\n"); */

    return;

} /* end mtx_log_a_path */


/* ======== mtx_compar ================================================ */
/* PURPOSE:
 *	Comparison of two paths - tagging duplicates too !
 *
 * RETURNS:
 * 	
 */

int	mtx_compar(
    mtx_path	   ** pp_A,
    mtx_path	   ** pp_B
) {

    mtx_path	    * p_A = * pp_A;
    mtx_path	    * p_B = * pp_B;
    mtx_node	   ** p_nA;
    mtx_node	   ** p_nB;
    int		      x;
    static int	      dupno;

    int	i = p_A->xx->npath < p_B->xx->npath ? p_A->xx->npath : p_B->xx->npath;

    p_nA = p_A->path;
    p_nB = p_B->path;

    if ( p_nA == p_nB ) {
	return 0;
    }

    for ( ; i; p_nA ++, p_nB ++, i -- ) {
	if ( (*p_nA) != (*p_nB) ) {
	    if ( x = strcmp( (*p_nA)->node_name, (*p_nB)->node_name ) ) {
		return x;
	    } else {
		return p_nA - p_nB;
	    }
	}
    }
    
    if ( p_A->xx->npath != p_B->xx->npath ) {
	return p_A->xx->npath - p_B->xx->npath;
    }

    /* this is two instances of the same path !				*/

    /* Do duplicate removal !						*/

    if ( p_A->xx->generated != p_B->xx->generated ) {
	if ( p_A->xx->generated ) {
	    p_A->xx->dup = dupno ++;
	    p_A->xx->included = 0;
	    return 1;
	} else {
	    p_B->xx->dup = dupno ++;
	    p_B->xx->included = 0;
	    return -1;
	}
    }

    if ( ! p_A->xx->generated ) {
	if ( p_B->xx->dup && p_A->xx->dup ) {
	    return p_A->xx->dup - p_B->xx->dup;
	}
	if ( p_B->xx->dup ) {
	    return 1;
	}
	if ( p_A->xx->dup ) {
	    return -1;
	}
	if ( p_A > p_B ) {
	    p_B->xx->dup = dupno ++;
	    p_B->xx->included = 0;
	    return 1;
	} else {
	    p_A->xx->dup = dupno ++;
	    p_A->xx->included = 0;
	    return -1;
	}
    }
	    
    if ( p_A->xx->dup > p_B->xx->dup ) {
	return 1;
    } else if ( p_A->xx->dup < p_B->xx->dup ) {
	return -1;
    }
    
    p_A->xx->dup = dupno ++;
    p_A->xx->included = 0;
    return -1;

} /* end mtx_compar */



/* ======== mtx_dump_paths ============================================ */
/* PURPOSE:
 *	Print out all unique paths
 *
 * RETURNS:
 * 	
 */

typedef int (* compar )(const void *, const void *);

void	mtx_dump_paths(
    mtx_base		* p_base,
    int			  xclude_temps
) {

    int		      i;
    mtx_path	    ** p_paths;
    mtx_path	    ** p_xpaths;
    mtx_path	     * p_path;
    mtx_node	    ** pp_node;

    i = ( p_base->npath + p_base->nexcludes ) * sizeof( * p_paths );

    p_xpaths = alloca( i );

    p_path = p_base->mtx_top;

    /* load them into an array						*/
    for ( i = p_base->npath, p_paths = p_xpaths; i; i --, p_paths ++ ) {
	* p_paths = p_path;
	p_path = p_path->xx->next;
    }

    p_path = p_base->excludes;
    for ( i = p_base->nexcludes; i; i --, p_paths ++ ) {
	* p_paths = p_path;
	pp_node = p_path->path;
	while ( * pp_node ) {
	    pp_node ++;
	}
	p_path->xx->npath = pp_node - p_path->path;
	p_path ++;
    }

    /* now sort these	*/

    qsort(
	p_xpaths,
	p_base->nexcludes + p_base->npath,
	sizeof( * p_xpaths ),
	( compar ) mtx_compar
    );

    /* Now print them							*/

    for(
	p_paths = p_xpaths,
	i = ( p_base->npath + p_base->nexcludes );
	i;
	i --, p_paths ++
    ) {
	p_path = * p_paths;
	if ( p_path->xx->included ) {
	    int	    j;

	    if ( ! xclude_temps ) {
		printf( "create_iconv_%d(", p_path->xx->npath );
		for ( j = 0; j < p_path->xx->npath; j ++ ) {
		    if ( j && ( j - p_path->xx->npath != 1 ) ) {
			printf(",");
		    }
		    printf( "%s", p_path->path[ j ]->node_name );
		}
		printf( ")\n" );
	    } else {
		printf( "{ { XCLUDE, }, { " );
		for ( j = 0; j < p_path->xx->npath; j ++ ) {
		    if ( j && ( j - p_path->xx->npath != 1 ) ) {
		    }
		    printf( "%s, ", p_path->path[ j ]->node_name );
		}
		printf( "} },\n" );
	    }
	}
    }

    return;

} /* end mtx_dump_paths */



/* ======== mtx_excluded ============================================== */
/* PURPOSE:
 *	Check to see if there is a specific exclusion.  This
 *  excludes nodes that specifically are used at first conversion.
 *  I don't think it's very useful but it's more work to take it
 *  out and it may have limited value sometime.
 *
 * RETURNS:
 * 	1 exclude - 0 include
 */

int	mtx_excluded(
    mtx_reference   * p_start,
    mtx_reference   * p_end
) {

    int		      i;
    mtx_node	    * p_start_cnv_node = p_start->ref->ioc_refs[ CNV ];
    mtx_node	   ** pp_node = p_end->ref->ioc_excludes;

    for ( i = 0; i < NEXCL; i ++, pp_node ++ ) {

	if ( ! * pp_node ) {
	    return 0;
	}

	if ( * pp_node == p_start_cnv_node ) {
	    return 1;
	}
    }

    return 0;

} /* end mtx_excluded */



/* ======== mtx_find_paths ============================================ */
/* PURPOSE:
 *	Find all paths for conversion
 *
 * RETURNS:
 * 	Nothing
 */

void mtx_find_paths(
    mtx_reference   * p_start,
    mtx_reference   * p_end,
    mtx_node	    * inputnode,
    mtx_node	    * output
) {

    mtx_reference   * p_ref;
    mtx_reference   * p_xref;

    if ( mtx_excluded( p_start, p_end ) ) {
	return;
    }

    if ( inputnode == output ) {
	mtx_log_a_path( p_start, p_end );
	return;
    }

    p_ref = inputnode->refs[ INP ];

    NEW( p_xref );
    p_end->next = p_xref;

    while ( p_ref ) {

	if ( ! p_ref->ref->in_use ) {
	    p_ref->ref->in_use = 1;
	    p_xref->ref = p_ref->ref;
	    p_xref->node = p_ref->ref->ioc_refs[ CNV ];
	    mtx_find_paths( p_start, p_xref, p_ref->ref->ioc_refs[ OUT ], output );
	    p_ref->ref->in_use = 0;
	}
	p_ref = p_ref->next;
    }

    free( p_xref );
    p_end->next = 0;

    return;

} /* end mtx_find_paths */



/* ======== mtx_start_path ============================================ */
/* PURPOSE:
 *	Start the search of all possible paths.
 *
 * RETURNS:
 * 	Nothing
 */

void	mtx_start_path(
    mtx_node	    * inputnode,
    mtx_node	    * output
) {

    mtx_reference   * p_ref;
    mtx_reference   * p_xref;

    p_ref = inputnode->refs[ INP ];

    NEW( p_xref );

    while ( p_ref ) {
	/* Mark it in use */
	p_ref->ref->in_use = 1;

	p_xref->ref = p_ref->ref;
	
	p_xref->node = p_ref->ref->ioc_refs[ CNV ];

	mtx_find_paths( p_xref, p_xref, p_ref->ref->ioc_refs[ OUT ], output );

	p_ref->ref->in_use = 0;
	p_ref = p_ref->next;
    }

    free( p_xref );

    return;

} /* end mtx_start_path */


main( int argc, char ** argv )
{
    mtx_initialize( mtx_table );
    mtx_start_path( INPUT, OUTPUT );
    mtx_dump_paths( mtx_table, argc == 2 );
    return 0;
}
