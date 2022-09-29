/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_wctob_decl,`
    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)
    define(`RETURN_VAR',inval)
    define(`INPUT_TYPE',wchar_t)
    define(`OUT_CHAR_NOT_FOUND',`	goto illegal_recovery;	    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',)
    define(`INPUT_PTR_ADJUST',)
    define(`INPUT_PTR_VAR',)
    define(`INPUT_REFERENCE',inval)
    define(`INPUT_CHECK_COUNT',)
    define(`INPUT_CHECK_VALUE',)
    define(`OUTPUT_COUNT_ADJUST',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_REFERENCE',)
    define(`OUTPUT_ASSIGN',`return '$`2')
    define(`OUTPUT_CHECK_COUNT',)
    define(`OUTPUT_WCTOB_FAIL',`return EOF;')
')

    
define(create_wctob_4,`

    'defn(`create_wctob_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_out_$4
    czz_tpl_out_$4

    WCTOB_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_arg_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_out_$4
')

define(create_wctob_3,`

    'defn(`create_wctob_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_out_$3
    czz_tpl_out_$3

    WCTOB_MAKEFUNC($1_$2_$3)

    czz_tpl_u_arg_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_out_$3
')

define(create_wctob_2,`

    'defn(`create_wctob_decl')`
    
    czz_tpl_t_arg_$1
    czz_tpl_arg_$1
    
    czz_tpl_t_out_$2
    czz_tpl_out_$2
    
    WCTOB_MAKEFUNC($1_$2)

    czz_tpl_u_arg_$1
    czz_tpl_u_out_$2
')

define(`WCTOB_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'define(`CZZ_wctob',__wctob_$1)`
int	__wctob_$1(
    int				    inval,
    void		         ** handle
) {

    'defn(`DECLARATIONS')`
    
    'defn(`INITIALIZE')`

    'defn(`OUTPUT_WCTOB_RETVAL')`
    
incomplete :
illegal_recovery : return EOF;

} /* end __wctob_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
