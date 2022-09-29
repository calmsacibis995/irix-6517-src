/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_mbtowc_decl,`
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
    define(`OUTPUT_ASSIGN',`OUTPUT_REFERENCE('$`1) = ( OUTPUT_SCALAR_TYPE ) '$`2')
    define(`OUTPUT_CHECK_COUNT',)
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_WCTOB_FAIL',)
')

    
define(create_mbtowc_4,`

    'defn(`create_mbtowc_decl')`
    
    czz_tpl_t_in_$1(0)
    czz_tpl_in_$1(0)
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    czz_tpl_t_cnv_$3(01)
    czz_tpl_cnv_$3(01)
    
    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)

    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_out_$4
    czz_tpl_out_$4

    MBTOWC_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(01)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_out_$4
')

define(create_mbtowc_3,`

    'defn(`create_mbtowc_decl')`
    
    czz_tpl_t_in_$1(0)
    czz_tpl_in_$1(0)
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)

    czz_tpl_t_in_$1(0)
    czz_tpl_in_$1(0)
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_out_$3
    czz_tpl_out_$3

    MBTOWC_MAKEFUNC($1_$2_$3)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_out_$3
')

define(create_mbtowc_2,`

    'defn(`create_mbtowc_decl')`
    
    czz_tpl_t_in_$1(0)
    czz_tpl_in_$1(0)
    
    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)

    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_out_$2
    czz_tpl_out_$2
    
    MBTOWC_MAKEFUNC($1_$2)

    czz_tpl_u_in_$1
    czz_tpl_u_out_$2
')

define(`MBTOWC_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ==============	*/
/* outstr is always non null - the wrapper catches the null case	*/

'define(`CZZ_mbtowc',__mbtowc_$1)`
int	__mbtowc_$1(
    register 'OUTPUT_TYPE`		  * outstr,
    register const 'INPUT_TYPE`		  * instr,
    register ssize_t			    inleft'ifelse(czz_num,0,,`,
    register void		         ** handle')`
) {

    register ssize_t			    start_inleft;

    'defn(`DECLARATIONS')`
    
    /* If input string is null we have reset of shift state		*/
    if ( instr ) {

	start_inleft = inleft;
    
	'defn(`INITIALIZE')`
	'defn(`OUTPUT_SETVAL')`
	
	return (int) (start_inleft - inleft);

    } else {
	goto retrun_0;   /* no shift state				*/
    }
incomplete_null_term: {
    if ( start_inleft == inleft ) {
	'OUTPUT_ASSIGN( outstr, 0 )`;
retrun_0:
	return 0;
    }
    /* Drop trough to incomplete					*/
}

incomplete :
illegal_recovery : return -1;

} /* end __mbtowc_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
