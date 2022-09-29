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
    'INPUT_SCALAR_TYPE`	    chin;
    /* === Previous found in ''__file__`` line ''__line__`` ============== */
')
define(`INPUT_REFERENCE',`( '$`1 )->val	/* = ''__file__`` line ''__line__`` =*/')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	'INPUT_CHECK_COUNT(<= 0,)`
		chin = 'INPUT_REFERENCE( INPUT_PTR_VAR )`;
	'INPUT_CHECK_VALUE( chin )`
	'INPUT_COUNT_ADJUST(1)`
	'INPUT_PTR_ADJUST(1)`
	/* === Previous code found in ''__file__`` line ''__line__`` ========= */
	'define(`RETURN_VAR',chin)`
')
')

/* === Following found in __file__ line __line__ ============= */
ifelse(CZZ_PACK(),p,`#pragma pack(1)')
typedef struct INPUT_TYPE {
    INPUT_SCALAR_TYPE	    val;
} INPUT_TYPE;
ifelse(CZZ_PACK(),p,`#pragma pack(0)')
/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_UNAME(),`
    undefine(`INPUT_SCALAR_TYPE')
    undefine(`INPUT_TYPE')
    undefine(`INPUT_GETVAL')'
)

/* __file__ ============================================== */
