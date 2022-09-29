/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_mblen_decl,`
    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`OUT_CHAR_NOT_FOUND',`	goto illegal_recovery;	    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',`	inleft -= '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_PTR_ADJUST',`	instr += '$`1;			    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_PTR_VAR',`instr')
    define(`INPUT_REFERENCE',`	* '$`1')
    define(`INPUT_CHECK_COUNT',`	if ( inleft '$`1 ) {	/* = ''__file__`` line ''__line__`` =*/
		    '$`2
		    goto incomplete;  /* = ''__file__`` line ''__line__`` =*/
		}')
    define(`INPUT_CHECK_VALUE',`	if ( ! '$`1 ) {	/* = ''__file__`` line ''__line__`` =*/
		    '$`2
		    goto incomplete_null_term;  /* = ''__file__`` line ''__line__`` =*/
		}')
    define(`OUTPUT_COUNT_ADJUST',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_REFERENCE',`	* '$`1')
    define(`OUTPUT_ASSIGN',`OUTPUT_REFERENCE('$`1) = ( 'OUTPUT_SCALAR_TYPE` ) '$`2')
    define(`OUTPUT_CHECK_COUNT',)
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_WCTOB_FAIL',)
')

    
define(create_mblen_4,`

    'defn(`create_mblen_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    czz_tpl_t_cnv_$3(01)
    czz_tpl_cnv_$3(01)
    
    MBLEN_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$3(01)
')

define(create_mblen_3,`

    'defn(`create_mblen_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    MBLEN_MAKEFUNC($1_$2_$3)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
')

define(create_mblen_2,`

    'defn(`create_mblen_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    MBLEN_MAKEFUNC($1_$2)

    czz_tpl_u_in_$1
')

define(`MBLEN_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'define(`CZZ_mblen',__mblen_$1)`
int	__mblen_$1(
    'INPUT_TYPE`		  * instr,
    ssize_t			    inleft,
    void		         ** handle
) {

    ssize_t			    start_inleft;
    'defn(`DECLARATIONS')`
    'defn(`INITIALIZE')`	

    /* If input string is null we have reset of shift state		*/
    if ( ! instr ) {
	return 0;   /* no shift state					*/
    }

    start_inleft = inleft;
    'defn(`GET_VALUE')`
    
    return (int) (start_inleft - inleft);

incomplete_null_term: {
    return start_inleft == inleft ? 0 : -1;
}

incomplete :
illegal_recovery : return -1;



} /* end __mblen_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
