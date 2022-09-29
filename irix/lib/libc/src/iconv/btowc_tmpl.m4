/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_btowc_decl,`
    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`OUT_CHAR_NOT_FOUND',`	return WEOF;	    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',)
    define(`INPUT_PTR_ADJUST',)
    define(`INPUT_PTR_VAR',)
    define(`INPUT_REFERENCE',`inval')
    define(`INPUT_BTOWC_FAIL',`	return WEOF;')
    define(`INPUT_BTOWC',`1')
    define(`INPUT_CHECK_COUNT',)
    define(`INPUT_CHECK_VALUE',)
    define(`OUTPUT_COUNT_ADJUST',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_REFERENCE',)
    define(`OUTPUT_ASSIGN',)
    define(`OUTPUT_CHECK_COUNT',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_WCTOB_FAIL',)
')

    
define(create_btowc_4,`

    'defn(`create_btowc_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    czz_tpl_t_cnv_$3(01)
    czz_tpl_cnv_$3(01)
    
    define(`INPUT_REFERENCE',`	inval')
    
    BTOWC_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_arg_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$3(01)
')

define(create_btowc_3,`

    'defn(`create_btowc_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    define(`INPUT_REFERENCE',`	inval')
    
    BTOWC_MAKEFUNC($1_$2_$3)

    czz_tpl_u_arg_$1
    czz_tpl_u_cnv_$2(00)
')

define(create_btowc_2,`

    'defn(`create_btowc_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    define(`INPUT_REFERENCE',`	inval')
    
    BTOWC_MAKEFUNC($1_$2)

    czz_tpl_u_arg_$1
')

define(`BTOWC_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'define(`CZZ_btowc',__btowc_$1)`
wint_t	__btowc_$1(
    int				    inval,
    void			 ** handle
) {

    'defn(`DECLARATIONS')`
    'defn(`INITIALIZE')`	

    'defn(`GET_VALUE')`
    
    return (wint_t) 'RETURN_VAR`;

incomplete_null_term:
incomplete :
illegal_recovery : return WEOF;

} /* end __btowc_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
