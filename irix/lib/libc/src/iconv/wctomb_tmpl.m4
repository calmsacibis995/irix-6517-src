/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_wctomb_decl,`
    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)
    define(`RETURN_VAR',inval)
    define(`INPUT_TYPE',wchar_t)
    define(`OUT_CHAR_NOT_FOUND',`	goto illegal_recovery;	    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',)
    define(`INPUT_PTR_ADJUST',)
    define(`INPUT_PTR_VAR',)
    define(`INPUT_REFERENCE',)
    define(`INPUT_CHECK_COUNT',)
    define(`INPUT_CHECK_VALUE',)
    define(`OUTPUT_COUNT_ADJUST',)
    define(`OUTPUT_PTR_ADJUST',`	outstr += '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_REFERENCE',`	* '$`1')
    define(`OUTPUT_ASSIGN',`OUTPUT_REFERENCE('$`1) = ( 'OUTPUT_SCALAR_TYPE` ) '$`2')
    define(`OUTPUT_CHECK_COUNT',)
    define(`OUTPUT_WCTOB_FAIL',)
')

    
define(create_wctomb_4,`

    'defn(`create_wctomb_decl')`
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_out_$4
    czz_tpl_out_$4

    WCTOMB_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_out_$4
')

define(create_wctomb_3,`

    'defn(`create_wctomb_decl')`
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_out_$3
    czz_tpl_out_$3

    WCTOMB_MAKEFUNC($1_$2_$3)

    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_out_$3
')

define(create_wctomb_2,`

    'defn(`create_wctomb_decl')`
    
    czz_tpl_t_out_$2
    czz_tpl_out_$2
    
    WCTOMB_MAKEFUNC($1_$2)

    czz_tpl_u_out_$2
')

define(`WCTOMB_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'define(`CZZ_wctomb',__wctomb_$1)`
int	__wctomb_$1(
    'OUTPUT_TYPE`		  * outstr,
    'INPUT_TYPE`		    inval,
    void		         ** handle
) {

    'OUTPUT_TYPE`		  * outstart;
    'defn(`DECLARATIONS')`
    
    /* If input string is null we have reset of shift state		*/
    if ( ! outstr ) {
	return 0;   /* no shift state					*/
    }

    'defn(`INITIALIZE')`

    outstart = outstr;

    'defn(`OUTPUT_SETVAL')`
    
    return (int) (outstr - outstart);

incomplete :
illegal_recovery : return -1;



} /* end __wctomb_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
