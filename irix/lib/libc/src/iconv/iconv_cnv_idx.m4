/* __file__ =============================================== *
 *
 *  Index converter. This is equivalent to a copy.
 */

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1','CZZ_TYPE()`)
define(`IDX_SCALAR_TYPE','CZZ_TYPE()`)
define(`IDX_TABLE_TYPE','CZZ_NAME()`_idx_type)
')
CZZ_TNAME()

/* === Following found in __file__ line __line__ ============= */
typedef struct IDX_TABLE_TYPE {
    int			    version;
    unsigned int	    maxtval;
    IDX_SCALAR_TYPE	    ttable[1];
} IDX_TABLE_TYPE;
/* === Previous code found in __file__ line __line__ ========= */


define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    ''IDX_TABLE_TYPE``	  * p_idxtab$1;
    'IDX_SCALAR_TYPE`	    xval$1;
    unsigned int	    tblmax$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    p_idxtab$1 = ( ''IDX_TABLE_TYPE`` * ) handle[ 'czz_count(`czz_num')` ];
    tblmax$1 = p_idxtab$1->maxtval;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( sizeof( 'RETURN_VAR` ) > 2 ) {
	if ( 'RETURN_VAR` > tblmax$1 ) {
	    'OUT_CHAR_NOT_FOUND`
	    xval$1 = 0;
	} else {
	    goto compute_index$1;
	}
    } else {
compute_index$1:
	xval$1 = p_idxtab$1->ttable[ 'RETURN_VAR` ];
	if ( ! xval$1 && 'RETURN_VAR` ) {
	    'OUT_CHAR_NOT_FOUND`
	}
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',xval$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`IDX_TABLE_TYPE')
    undefine(`IDX_SCALAR_TYPE')
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
