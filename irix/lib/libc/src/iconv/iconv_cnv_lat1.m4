/* __file__ =============================================== *
 *
 *  iso8859-1 converter.
 */

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1','CZZ_TYPE()`)
')
CZZ_TNAME()

define(CZZ_CNAME(),`
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( ( ( unsigned ) 'RETURN_VAR` ) >> 8 ) {
	'OUT_CHAR_NOT_FOUND`
	'RETURN_VAR` = 0;
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
')

define(CZZ_UNAME(),`
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
