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
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
#ifdef ARG_READ
#undef ARG_READ
#endif
    'INPUT_SCALAR_TYPE`	    chin;
    /* === Previous found in ''__file__`` line ''__line__`` ============== */
')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
#ifdef ARG_READ
	goto illegal_recovery;
#else
#define ARG_READ
	chin = 'INPUT_REFERENCE`;
#endif
	/* === Previous code found in ''__file__`` line ''__line__`` ========= */
	'define(`RETURN_VAR',chin)`
')
')

define(CZZ_UNAME(),`
/* === Following found in '__file__` line '__line__` ============= */
#ifdef ARG_READ
#undef ARG_READ
#endif
/* === Previous found in '__file__` line '__line__` ============== */
    undefine(`INPUT_SCALAR_TYPE')
    undefine(`INPUT_TYPE')
    undefine(`INPUT_GETVAL')
')

/* __file__ ============================================== */
