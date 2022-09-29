/* __file__ =============================================== *
 *
 *  dbcs converter. This is equivalent to a copy.
 */

define(`EUC_TYP_NAME',czz_cat(iconv_,CZZ_NAME(),_ttable))
typedef struct EUC_TYP_NAME {
    int		version;
    int		euc_tbl_mask;
    int		euc_tbl_offs;
    int		tbl[ 1 ];
} EUC_TYP_NAME;

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1',int)
')

#define dbcs_leaf_lookup( TBL, inch, tbloff ) \
    ( * ( inch + (unsigned short * ) ( tbloff + ( char * ) ( TBL->tbl ) ) ) )
#define dbcs_nonleaf_lookup( TBL, inch, tbloff )	\
    ( * ( inch + ( int * ) ( tbloff + ( char * ) ( TBL->tbl ) ) ) )

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    /* === Entering in ''__file__`` line ''__line__`` ============= */
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    ''EUC_TYP_NAME``	  * p_euct$1;
    'INTERMEDIATE_TYPE$1`	    state$1;
    'INTERMEDIATE_TYPE$1`	    tblno$1;
    'INTERMEDIATE_TYPE$1`	    tblmask$1;
    'INTERMEDIATE_TYPE$1`	    tbloffs$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    /* === Entering in ''__file__`` line ''__line__`` ============= */
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    p_euct$1 = ( ''EUC_TYP_NAME`` * ) handle[ 'czz_count(`czz_num')` ];
    tblmask$1 = p_euct$1->euc_tbl_mask;
    tbloffs$1 = p_euct$1->euc_tbl_offs;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	'defn(`GET_VALUE')`
	/* === Back in ''__file__`` line ''__line__`` ============= */
	

	/* first table must be 256 elements				*/
	state$1 = dbcs_nonleaf_lookup( p_euct$1, 'RETURN_VAR`, 0 ); 

	/* if the character is less than tblmask$1 we have found a terminal*/
	/* entry.  State actually contains the character we want	*/
	if ( ! state$1 ) {
	    if ( ! 'RETURN_VAR` ) goto cont$1;
	    goto illegal_recovery;
	}

	if ( state$1 < tblmask$1 ) {
	    goto cont$1;
	}
	
	/* ============ Get next input character. ===================== */
	
	/* === leaving in ''__file__`` line ''__line__`` ============= */
	'defn(`GET_VALUE')`
	/* === Back in ''__file__`` line ''__line__`` ============= */

	/* DBCS begins at 0x40 */
	if ( ( 'RETURN_VAR` ) < 0x40 ) {
	    goto illegal_recovery;
	}

	tblno$1 =  (state$1 & tblmask$1);

	/* check if we need a leaf or non-leaf lookup			*/
	if ( tblno$1 > tbloffs$1 ) {

	    /* This is a leaf lookup					*/
	    state$1 = dbcs_leaf_lookup( p_euct$1, 'RETURN_VAR`, tblno$1 );

	    if ( state$1 ) {
		goto cont$1;
	    }
	    
	    goto illegal_recovery;
	}

	/* DBCS terminates at the second byte */
	/* if we get here the table is corrupted			*/
	goto illegal_recovery;

cont$1:
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',state$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`INTERMEDIATE_TYPE$1')
    undefine(`GET_VALUE')
')
undefine(`EUC_TYP_NAME')

/* End __file__ ============================================== */
