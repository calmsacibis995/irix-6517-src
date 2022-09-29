/* __file__ =============================================== *
 *
 *  Output converter for fixed length characters
 */

define(CZZ_TNAME(),`
define(`INPUT_SCALAR_TYPE','CZZ_TYPE()`)
define(`INPUT_TYPE','CZZ_NAME()`_inval_type)
')
CZZ_TNAME()


define(CZZ_CNAME(),`
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	if ( 'INPUT_REFERENCE` >> 7 ) {
	    'INPUT_BTOWC_FAIL`
	}
	/* === Previous code found in ''__file__`` line ''__line__`` ========= */
	'define(`RETURN_VAR',INPUT_REFERENCE)`
')
')

define(CZZ_UNAME(),`
    undefine(`INPUT_SCALAR_TYPE')
    undefine(`INPUT_TYPE')
    undefine(`INPUT_GETVAL')'
)

/* __file__ ============================================== */
