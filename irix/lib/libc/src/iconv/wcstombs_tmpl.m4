/* __file__ =============================================== *
 *
 *  Main stdlib converter template.
 */


define(create_wcstombs_decl,`
    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`OUT_CHAR_NOT_FOUND',`	goto illegal_recovery;	    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',)
    define(`INPUT_PTR_ADJUST',`	instr += '$`1;			    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_PTR_VAR',`instr')
    define(`INPUT_REFERENCE',`	* '$`1')
    define(`INPUT_CHECK_COUNT',)
    define(`INPUT_CHECK_VALUE',`	if ( ! '$`1 ) {	/* = ''__file__`` line ''__line__`` =*/
		    '$`2 goto incomplete_null_term;  /* = ''__file__`` line ''__line__`` =*/
		}')
    define(`OUTPUT_COUNT_ADJUST',`	outleft -= '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`OUTPUT_PTR_ADJUST',`	outstr += '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_REFERENCE',`	* '$`1')
    define(`OUTPUT_ASSIGN',`OUTPUT_REFERENCE('$`1) = ( 'OUTPUT_SCALAR_TYPE` ) '$`2')
    define(`OUTPUT_CHECK_COUNT',`	if ( outleft '$`1 ) {	/* = ''__file__`` line ''__line__`` =*/
		    '$`2
		    goto output_full;  /* = ''__file__`` line ''__line__`` =*/
		}')

    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_WCTOB_FAIL',)
')

    
define(create_wcstombs_4,`

    'defn(`create_wcstombs_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    czz_tpl_t_cnv_$3(01)
    czz_tpl_cnv_$3(01)
    
    czz_tpl_t_out_$4(0)
    czz_tpl_out_$4(0)

    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))
    define(`FIRST_OUTPUT_SETVAL',defn(`OUTPUT_SETVAL'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)
    define(`OUTPUT_SETVAL',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_REFERENCE',)
    define(`OUTPUT_ASSIGN',)
    define(`OUTPUT_CHECK_COUNT',)
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_out_$4
    czz_tpl_out_$4

    WCSTOMBS_MAKEFUNC($1_$2_$3_$4)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(01)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_out_$4
')

define(create_wcstombs_3,`

    'defn(`create_wcstombs_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(00)
    czz_tpl_cnv_$2(00)
    
    czz_tpl_t_out_$3(0)
    czz_tpl_out_$3(0)

    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))
    define(`FIRST_OUTPUT_SETVAL',defn(`OUTPUT_SETVAL'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)
    define(`OUTPUT_SETVAL',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_REFERENCE',)
    define(`OUTPUT_ASSIGN',)
    define(`OUTPUT_CHECK_COUNT',)
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_out_$3
    czz_tpl_out_$3

    WCSTOMBS_MAKEFUNC($1_$2_$3)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(00)
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_out_$3
')

define(create_wcstombs_2,`

    'defn(`create_wcstombs_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_out_$2(0)
    czz_tpl_out_$2(0)
    
    define(`FIRST_DECLARATIONS',defn(`DECLARATIONS'))
    define(`FIRST_INITIALIZE',defn(`INITIALIZE'))
    define(`FIRST_GET_VALUE',defn(`GET_VALUE'))
    define(`FIRST_OUTPUT_SETVAL',defn(`OUTPUT_SETVAL'))

    define(`DECLARATIONS',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`GET_VALUE',)
    define(`OUTPUT_SETVAL',)
    define(`OUTPUT_PTR_ADJUST',)
    define(`OUTPUT_PTR_VAR',)
    define(`OUTPUT_REFERENCE',)
    define(`OUTPUT_ASSIGN',)
    define(`OUTPUT_CHECK_COUNT',)
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_out_$2
    czz_tpl_out_$2
    
    WCSTOMBS_MAKEFUNC($1_$2)

    czz_tpl_u_in_$1
    czz_tpl_u_out_$2
')

define(`WCSTOMBS_MAKEFUNC',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'define(`CZZ_wcstombs',__wcstombs_$1)`
size_t	__wcstombs_$1(
    'OUTPUT_TYPE`		  * outstr,
    'INPUT_TYPE`		  * instr,
    ssize_t			    outleft,
    void		         ** handle
) {

    'INPUT_TYPE`		  * start_instr;
    ssize_t			    numtowrite;

    /* If input string is null we have reset of shift state		*/
    if ( ! instr ) {
	return 0;   /* no shift state					*/
    }

    numtowrite = outleft;
    if ( ! outstr ) {
	'defn(`DECLARATIONS')`
	'defn(`INITIALIZE')`
	while ( 1 ) {
	    start_instr = instr;
	    'defn(`OUTPUT_SETVAL')`
	}
    } else {
	'defn(`FIRST_DECLARATIONS')`
	'defn(`FIRST_INITIALIZE')`
	while ( 1 ) {
	    start_instr = instr;
	    'defn(`FIRST_OUTPUT_SETVAL')`
	}
    }

incomplete_null_term: {
    if ( start_instr == instr ) {
	goto output_full;
    } else {
	setoserror(EILSEQ);
	return (size_t) -1;
    }
}

output_full: {
    if ( outstr && outleft ) {
	'OUTPUT_REFERENCE(outstr)` = 0;
    }
    return (size_t) (numtowrite - outleft);
}
    
incomplete :
illegal_recovery : {
    setoserror(EILSEQ);
    return (size_t) -1;
}

} /* end __wcstombs_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
