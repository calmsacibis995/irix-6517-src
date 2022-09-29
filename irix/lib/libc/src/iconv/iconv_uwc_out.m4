/* __file__ =============================================== *
 *
 *  Output converter for unix style wide chars
 */

define(CZZ_TNAME(),`
define(`OUTPUT_SCALAR_TYPE','CZZ_TYPE()`)
define(`OUTPUT_TYPE','CZZ_NAME()`_outval_type)
')
CZZ_TNAME()

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    ''EUC_TYP_NAME``	  *p_uwct$1;
    'int`		    outlen;
    unsigned int	    outtmpval;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
   p_uwct$1 = ( ''EUC_TYP_NAME`` * ) handle[ 'czz_count(`czz_num')` ];
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`OUTPUT_REFERENCE',`( '$`1 )->val')
define(`OUTPUT_SETVAL',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    outlen =
    
    tmpval = ( ( unsigned int ) ( 'RETURN_VAR` & p_uwct$1->type_mask ) ) >> p_uwct$1->mask_shift;
    
    'OUTPUT_CHECK_COUNT(>= p_uwct$1->uwc_eucw1[ tmpval ].len,)`
    'OUTPUT_COUNT_ADJUST(p_uwct$1->uwc_eucw1[ tmpval ].len)`
    'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, RETURN_VAR )`;
    'OUTPUT_PTR_ADJUST(1)`

    
    
    if ( tmpval = ( ((unsigned int) 'RETURN_VAR`) >> ( 8 * sizeof(''OUTPUT_SCALAR_TYPE``)))) {
	'OUTPUT_CHECK_COUNT(== 0,
	    OUTPUT_COUNT_ADJUST(-1)
	    OUTPUT_PTR_ADJUST(-1)
	)`
	'OUTPUT_COUNT_ADJUST(1)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, tmpval )`;
	'OUTPUT_PTR_ADJUST(1)`
	if ( tmpval = ( ((unsigned int) 'RETURN_VAR`) >> (16 * sizeof(''OUTPUT_SCALAR_TYPE``)))) {
	    'OUTPUT_CHECK_COUNT(== 0,
		OUTPUT_COUNT_ADJUST(-2)
		OUTPUT_PTR_ADJUST(-2)
	    )`
	    'OUTPUT_COUNT_ADJUST(1)`
	    'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, tmpval )`;
	    'OUTPUT_PTR_ADJUST(1)`
	    if ( tmpval = ( ((unsigned int) 'RETURN_VAR`) >> (24 * sizeof(''OUTPUT_SCALAR_TYPE``)))) {
		'OUTPUT_CHECK_COUNT(== 0,
		    OUTPUT_COUNT_ADJUST(-3)
		    OUTPUT_PTR_ADJUST(-3)
		)`
		'OUTPUT_COUNT_ADJUST(1)`
		'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, tmpval )`;
		'OUTPUT_PTR_ADJUST(1)`
	    }
	}
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
define(`OUTPUT_WCTOB_RETVAL',`
    'defn(`GET_VALUE')`
    ????????????????
    return 'RETURN_VAR`;
')
')

/* === Following found in __file__ line __line__ ============= */
ifelse(CZZ_PACK(),p,`#pragma pack(1)')
typedef struct OUTPUT_TYPE {
    OUTPUT_SCALAR_TYPE	    val;
} OUTPUT_TYPE;
ifelse(CZZ_PACK(),p,`#pragma pack(0)')
/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_UNAME(),`
    undefine(`OUTPUT_SCALAR_TYPE')
    undefine(`OUTPUT_TYPE')
    undefine(`OUTPUT_SETVAL')
    undefine(`OUTPUT_WCTOB_RETVAL')
')

/* __file__ ============================================== */
