/* __file__ =============================================== *
 *
 *  Null converter. This is equivalent to a copy.
 */

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1',int)
')

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    INTERMEDIATE_TYPE$1	    state$1;
')
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    state$1 = 'RETURN_VAR`;
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',state$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`INTERMEDIATE_TYPE$1')
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
