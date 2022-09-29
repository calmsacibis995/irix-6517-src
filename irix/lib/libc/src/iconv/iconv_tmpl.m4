/* __file__ =============================================== *
 *
 *  Main iconv converter template.
 */


define(create_iconv_decl,`
    define(`DECLARATIONS',)
    define(`UPDATE_STATE',)
    define(`INITIALIZE_STATE',)
    define(`ADJUST_STATE',)
    define(`INITIALIZE',define(`czz_num',0))
    define(`OUT_CHAR_NOT_FOUND',`	bad_conv ++;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_COUNT_ADJUST',`	inleft -= '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_PTR_ADJUST',`	instr += '$`1;			    /* = ''__file__`` line ''__line__`` =*/')
    define(`INPUT_PTR_VAR',`instr')
    define(`INPUT_REFERENCE',`	* '$`1')
    define(`INPUT_CHECK_COUNT',`	if ( inleft '$`1 ) {	/* = ''__file__`` line ''__line__`` =*/
		    '$`2 goto incomplete;  /* = ''__file__`` line ''__line__`` =*/
		}')
    define(`INPUT_CHECK_VALUE',`')
    define(`OUTPUT_COUNT_ADJUST',`	outleft -= '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`OUTPUT_PTR_ADJUST',`	outstr += '$`1;		    /* = ''__file__`` line ''__line__`` =*/')
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_REFERENCE',`	* '$`1')
    define(`OUTPUT_ASSIGN',`OUTPUT_REFERENCE('$`1) = ( OUTPUT_SCALAR_TYPE ) '$`2')
    define(`OUTPUT_CHECK_COUNT',`	if ( outleft '$`1 ) { /* = ''__file__`` line ''__line__`` =*/
		    '$`2
		    goto output_full;  /* = ''__file__`` line ''__line__`` =*/
		}')
    define(`OUTPUT_PTR_VAR',`outstr')
    define(`OUTPUT_WCTOB_FAIL',)
')


define(create_iconv_6,`

    'defn(`create_iconv_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_cnv_$4(2)
    czz_tpl_cnv_$4(2)
    
    czz_tpl_t_cnv_$5(3)
    czz_tpl_cnv_$5(3)
    
    czz_tpl_t_out_$6
    czz_tpl_out_$6

    MAKE_FUNCTION($1_$2_$3_$4_$5_$6)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_cnv_$4(2)
    czz_tpl_u_cnv_$5(3)
    czz_tpl_u_out_$6
')

define(create_iconv_5,`

    'defn(`create_iconv_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_cnv_$4(2)
    czz_tpl_cnv_$4(2)
    
    czz_tpl_t_out_$5
    czz_tpl_out_$5

    MAKE_FUNCTION($1_$2_$3_$4_$5)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_cnv_$4(2)
    czz_tpl_u_out_$5
')

define(create_iconv_4,`

    'defn(`create_iconv_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_cnv_$3(1)
    czz_tpl_cnv_$3(1)
    
    czz_tpl_t_out_$4
    czz_tpl_out_$4

    MAKE_FUNCTION($1_$2_$3_$4)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_cnv_$3(1)
    czz_tpl_u_out_$4
')

define(create_iconv_3,`

    'defn(`create_iconv_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_cnv_$2(0)
    czz_tpl_cnv_$2(0)
    
    czz_tpl_t_out_$3
    czz_tpl_out_$3

    MAKE_FUNCTION($1_$2_$3)

    czz_tpl_u_in_$1
    czz_tpl_u_cnv_$2(0)
    czz_tpl_u_out_$3
')

define(create_iconv_2,`

    'defn(`create_iconv_decl')`
    
    czz_tpl_t_in_$1
    czz_tpl_in_$1
    
    czz_tpl_t_out_$2
    czz_tpl_out_$2
    
    MAKE_FUNCTION($1_$2)

    czz_tpl_u_in_$1
    czz_tpl_u_out_$2
')

define(`MAKE_FUNCTION',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

'syscmd(echo "__iconv_$1" >> CZZ_SYMSFILE)`
'syscmd(echo "extern size_t	__iconv_$1(
    void		 ** handle,
    uchar_t		 ** p_inbuf,
    ssize_t		  * p_inleft,
    uchar_t		 ** p_outbuf,
    ssize_t		  * p_outleft
);" >> CZZ_DCLSFILE)`
size_t	__iconv_$1(
    void		 ** handle,
    uchar_t		 ** p_inbuf,
    ssize_t		  * p_inleft,
    uchar_t		 ** p_outbuf,
    ssize_t		  * p_outleft
) {
    
    'defn(`DECLARATIONS')`

    ssize_t		    start_inleft;
    ssize_t		    inleft;
    ssize_t		    outleft;
    'INPUT_TYPE`		  * instr;
    'OUTPUT_TYPE`		  * outstr;
    int			    bad_conv;

    'defn(`INITIALIZE')`

    if (!p_inbuf || !(*p_inbuf)) {
	/* stateful ecoding reset   */
	/* do nothing at this point */
	'defn(`INITIALIZE_STATE')`
        return (0);
    }

    inleft = ( * p_inleft ) / (ssize_t) sizeof( 'INPUT_TYPE` );
    instr = ( 'INPUT_TYPE` * ) * p_inbuf ;
    outleft = ( * p_outleft ) / (ssize_t) sizeof( 'OUTPUT_TYPE` );
    outstr = ( 'OUTPUT_TYPE` * ) * p_outbuf;
    bad_conv = 0;

    while ( inleft ) {

	start_inleft = inleft;	    /* This is for error recovery	*/

	'defn(`OUTPUT_SETVAL')`
	'defn(`ADJUST_STATE')`
    }

complete:

    if ( sizeof( * instr ) == 1 ) {
	* p_inleft = inleft;
	* p_inbuf = ( void * ) instr;
    } else {
	* p_inleft = ( * p_inleft ) - ( ( ( char * ) instr ) - ( ( char *) * p_inbuf ) );;
	* p_inbuf = ( void * ) instr;
    }

    if ( sizeof( * outstr ) == 1 ) {
	* p_outleft = outleft;
	* p_outbuf = ( void * ) outstr;
    } else {
	* p_outleft = ( * p_outleft ) - ( ( ( char * ) outstr ) - ( ( char *) * p_outbuf ) );
	* p_outbuf = ( void * ) outstr;
    }

    'defn(`UPDATE_STATE')`

    return bad_conv;
    
    /* conversion ended due to either source or destination being used up*/

incomplete: {
    setoserror( EINVAL );
cleanup_exit:
    instr = ( 'INPUT_TYPE` * ) (
	* p_inbuf + sizeof( * instr ) * (
	    ( * p_inleft ) / sizeof( * instr ) - start_inleft
	)
    );
    inleft = start_inleft;
    bad_conv = -1;
    goto complete;
}

illegal_recovery : {
    setoserror( EILSEQ );
    goto cleanup_exit;
}

output_full : {
    setoserror( E2BIG );
    goto cleanup_exit;
}

} /* end __iconv_$1 */

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')


/* End __file__ ============================================== */
