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
 * iconv_comp.h
 *
 *
 */


#ifdef linux
extern char * yytext;
#else
extern char yytext[];
#endif

typedef int	ICC_RELPTR;

/* ======== ICC_OBJ_TYP =============================================== */
/* PURPOSE:
 *	Descriptor for DSO !
 */

typedef struct  ICC_OBJ_TYP {

    struct ICC_OBJ_TYP
		    * icc_next;
    struct ICC_OBJ_HEAD
		    * icc_objhead;  /* Object header			*/
    int		      icc_num;	    /* Object number			*/
    char	      icc_used;	    /* Set when used			*/
    char	      icc_used1;    /* Set when used			*/
    inx_posi	      icc_posi[ 1 ];	/* Where stdlib stuff defined	*/
    ICC_RELPTR	      icc_robj;	    /* Relative address of object	*/
    void	    * icc_data1;    /* Pointer to data for object	*/
    void	    * icc_data2;    /* More data.			*/

} ICC_OBJ_TYP;


/* ======== ICC_OBJ_HEAD ============================================== */
/* PURPOSE:
 *	Header for object
 */

typedef int	( * ICC_CMPFUNC )( void *, void * );

typedef struct  ICC_OBJ_HEAD {

    int			icc_count;	/* Number of objects		*/
    int			icc_usedcount;	/* Count of number used		*/
    int			icc_size;	/* Size of object		*/
    ICC_CMPFUNC		icc_cmpfunc;	/* Comparison function		*/
    ICC_OBJ_TYP	      * icc_head;	/* head of list of data		*/
    ICC_RELPTR	        icc_robj;	/* Relative address of object	*/
    ICC_OBJ_TYP	     ** icc_array;	/* Temporary array		*/

} ICC_OBJ_HEAD;


/* ======== ICC_SPECTAB =============================================== */
/* PURPOSE:
 *	For specifying table
 */

typedef struct  ICC_SPECTAB {

    char		icc_isfile;	/* is the table is a file	*/
    ICC_OBJ_TYP	      * icc_tobj;	/* Object for tables		*/
    ICC_OBJ_TYP	      * icc_tbl;	/* Table symbol			*/

} ICC_SPECTAB;


/* ======== ICC_METHODS =============================================== */
/* PURPOSE:
 *	
 */

typedef struct  ICC_METHODS {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/

    ICC_OBJ_TYP	      * icc_aobj;	/* Object for algorithms	*/
    ICC_OBJ_TYP	      * icc_cnv;	/* Converter symol		*/
    ICC_OBJ_TYP	      * icc_cobj;	/* Object for converter		*/
    ICC_OBJ_TYP	      * icc_ctl;	/* Control symol		*/

} ICC_METHODS;



/* ======== ICC_STDLIBMETH ============================================ */
/* PURPOSE:
 *	
 */

typedef struct  ICC_STDLIBMETH {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/

    ICC_OBJ_TYP	      *	icc_mbwc_obj;	/* mbwc table DSO		*/
    ICC_OBJ_TYP	      * icc_mbwc_sym;	/* mbwc table symbol		*/
    
    short		icc_ntable_mbwc;
    short		icc_ntable_wcmb;

    ICC_SPECTAB		icc_table[ 2 ];    

} ICC_STDLIBMETH;



/* ======== ICC_STDLIBSPEC ============================================ */
/* PURPOSE:
 *	Standard library specifier
 */

typedef struct  ICC_STDLIBSPEC {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/

    inx_posi		icc_posi[ 1 ];	/* Where stdlib stuff defined	*/

    ICC_OBJ_TYP	      * icc_from_enc;	/* From encoding		*/
    ICC_OBJ_TYP	      * icc_to_enc;	/* To encoding			*/

    ICC_STDLIBMETH    * icc_meths;	/* Methods			*/

} ICC_STDLIBSPEC;


/* ======== ICC_SPEC ================================================== */
/* PURPOSE:
 *	Spec class
 */

typedef struct  ICC_SPEC {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/
    
    inx_posi		icc_posi[ 1 ];	/* Where defined		*/

    ICC_OBJ_TYP	      * icc_from_enc;	/* From encoding		*/
    ICC_OBJ_TYP	      * icc_to_enc;	/* To encoding			*/

    ICC_METHODS	      * icc_mth;	/* Method spec			*/
    
    short		icc_ntable;

    ICC_SPECTAB		icc_table[ 1 ];

} ICC_SPEC;


/* ======== ICC_CLONEOBJ ============================================== */
/* PURPOSE:
 *	Clone record
 */

typedef struct  ICC_CLONEOBJ {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/

    inx_posi		icc_posi[ 1 ];	/* Where defined		*/

    char		icc_src_dst;	/* clone source or dst		*/

    ICC_OBJ_TYP	      * icc_enc;	/* src/dst encoding		*/
    
    ICC_METHODS	      * icc_mth;	/* Method spec to clone		*/

    ICC_OBJ_TYP	      * icc_newenc;	/* src/dst encoding		*/
    
    ICC_METHODS	      * icc_newmth;	/* new methods			*/

} ICC_CLONEOBJ;


/* ======== ICC_JOINOBJ =============================================== */
/* PURPOSE:
 *	Join object
 */

typedef struct  ICC_JOINOBJ {

    ICC_OBJ_TYP		icc_obj[ 1 ];	/* Object header		*/

    inx_posi		icc_posi[ 1 ];	/* Where defined		*/

    ICC_OBJ_TYP	      * icc_dst_enc;	/* src/dst encoding		*/
    
    ICC_OBJ_TYP	      * icc_src_enc;	/* src/dst encoding		*/
    
    ICC_METHODS	      * icc_mth_dst;	/* Method spec to clone		*/

    ICC_METHODS	      * icc_mth_src;	/* Method spec to clone		*/

    ICC_METHODS	      * icc_newmth;	/* new methods			*/

} ICC_JOINOBJ;


/* ======== ICC_HEAD ================================================== */
/* PURPOSE:
 *	Head of iconv compiler structures
 */

typedef struct  ICC_HEAD {

    ICC_OBJ_HEAD	icc_strings[ 1 ];/* String header		*/
    ICC_OBJ_HEAD	icc_objects[ 1 ];/* Object Header		*/
    ICC_OBJ_HEAD	icc_symbols[ 1 ];/* Symbols			*/
    ICC_OBJ_HEAD	icc_files[ 1 ];	 /* Files			*/
    ICC_OBJ_HEAD	icc_specs[ 1 ];	 /* Specifiers			*/
    ICC_OBJ_HEAD	icc_methods[ 1 ];/* Methods			*/
    ICC_OBJ_HEAD	icc_clones[ 1 ]; /* Clones			*/
    ICC_OBJ_HEAD	icc_joins[ 1 ];	 /* Joins			*/
    ICC_OBJ_HEAD	icc_libspecs[ 1 ];  /* stdlib specs		*/
    ICC_OBJ_HEAD	icc_libmeths[ 1 ];  /* stdlib methods		*/
    ICC_OBJ_HEAD	icc_loc_alias[ 1 ]; /* locale aliases		*/
    ICC_OBJ_HEAD	icc_loc_codeset[ 1 ]; /* locale codesets	*/
    ICC_OBJ_HEAD	icc_resources[ 1 ]; /* iconv resources		*/

} ICC_HEAD;

typedef struct ICC_RES_VALUE	* ICC_P_RES_VALUE;

/* Destination and source functions					*/
typedef int ( * ICC_CNV_FUNC )(
    void	    * p_dst,
    ICC_P_RES_VALUE   p_src
);

/* ======== ICC_TYPEDEF =============================================== */
/* PURPOSE:
 *	Builtin types
 */

typedef struct  ICC_TYPEDEF {

    int			icc_typeid;	/* id of the type		*/
    char	      * icc_typename;	/* Descriptor of the type	*/
    int			icc_size_internal; /* sizeof internal val	*/
    int			icc_size_output;   /* size of the output value	*/
    int			icc_algn_output;   /* alignment of output	*/
    ICC_CNV_FUNC	icc_cnv_func;	/* function to convert between	*/
					/* internal and output value	*/
} ICC_TYPEDEF;



/* ======== ICC_RES_VALUE ============================================= */
/* PURPOSE:
 *	An instance of a resource value
 */

typedef struct  ICC_RES_VALUE {

    struct ICC_RES_VALUE    * icc_next;
    int			      icc_offset;
    char		    * icc_name;
    ICC_TYPEDEF		    * icc_type;
    long long		      icc_data[ 1 ];

} ICC_RES_VALUE;


/* ======== ICC_RES_VALUES ============================================ */
/* PURPOSE:
 *	A list of resource values
 */

typedef struct  ICC_RES_VALUES {

    ICC_RES_VALUE	    * icc_head;
    ICC_RES_VALUE	    * icc_tail;
    int			      icc_size;	    /* Collective size of resource */
    int			      icc_align;    /* overall alignment of struct*/
    inx_posi		      icc_posi[ 1 ];	/* Where defined	*/
    ICC_RELPTR		      icc_robj;	    /* Relative address of object*/

} ICC_RES_VALUES;


/* ======== ICC_TBLST ================================================= */
/* PURPOSE:
 *	A list of table
 */

typedef struct _ICC_TBLST {

    ICC_OBJ_TYP             * odata;
    struct  _ICC_TBLST      * next;

} ICC_TBLST;


/* ======== icc_yystype =============================================== */
/* PURPOSE:
 *	YYSTYPE definition
 */

typedef union icc_yystype {

    char	    val[ 1 ];
    ICC_OBJ_TYP	    table[ 1 ];
    ICC_RES_VALUES  resource[ 1 ];
    ICC_RES_VALUES  values[ 1 ];
    ICC_RES_VALUE   value[ 1 ];
    ICC_TYPEDEF	    type_spec[ 1 ];
    ICC_TBLST       tblst[ 1 ];

} icc_yystype, * icc_pyystype;

#define YYSTYPE	    icc_pyystype
extern int yyleng;
extern YYSTYPE yylval;

typedef int ( * ICC_CPYFNC )( char *, int *, void *, int, int );
typedef int ( * icc_qsort_compar )(const void *, const void *);

#define ICC_TYP_STRING		1
#define ICC_TYP_INT		2
#define ICC_TYP_REFER    	3

