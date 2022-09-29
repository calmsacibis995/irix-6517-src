/* __file__ =============================================== *
 *
 *  Byte Order Mark - this is a stateful decoder
 *  The input type is the same as the output type.
 */

define(CZZ_TNAME(),`
')
CZZ_TNAME()

/* === Following found in __file__ line __line__ ============= */

#define BOM_CODE	    0x0000feffu
#define BOM_CODE_DETECT_16	0xfffeu
#define BOM_CODE_DETECT_32  0xfffe0000u

const unsigned int __iconv_unicode_signature[] = { 0xfffe  };
syscmd(echo "__iconv_unicode_signature" >> CZZ_SYMSFILE)
syscmd(echo "extern const unsigned int __iconv_unicode_signature[];" >> CZZ_DCLSFILE)
const unsigned int __iconv_unicode_signature_swap[] = { 0xfffe  };
syscmd(echo "__iconv_unicode_signature_swap" >> CZZ_SYMSFILE)
syscmd(echo "extern const unsigned int __iconv_unicode_signature_swap[];" >> CZZ_DCLSFILE)
syscmd(echo "__iconv_control_bom" >> CZZ_SYMSFILE)
syscmd(echo "void *  __iconv_control_bom(
    int		       operation_id,
    void	    *  state,
    ICONV_CNVFUNC      iconv_converter,
    short	       num_table,
    void	    ** iconv_tab
);" >> CZZ_DCLSFILE)


void *  __iconv_control_bom(
    int		       operation_id,
    void	    *  state,
    ICONV_CNVFUNC      iconv_converter,
    short	       num_table,
    void	    ** iconv_tab
) {
    void     **	table;

    switch ( operation_id ) {
	case ICONV_C_DESTROY : /* iconv_close call */
	    free( * ( void ** ) state );
	    return ( void * ) 0; 
    
	case ICONV_C_INITIALIZE : {
	    /* iconv_open */
    
	    int		  i;
	    void	* val;
	    void	* sig_addr = ( void * ) __iconv_unicode_signature;
	    void	* sig_addr_swap = ( void * ) __iconv_unicode_signature_swap;

	    /* allocate an extra entry for the bom state		*/
	    table = malloc( sizeof( * table ) * ( num_table + 1 ) );
    
	    if ( ! table ) {
		return ( void * ) -1l;
	    }


	    /* look for table entries that contain __iconv_unicode_signature*/
	    /* In these entries - replace them with a writeable address	*/
	    for ( i = 0; i < num_table; i ++ ) {
		
		val = iconv_tab[ i ];
		
		if ( val == sig_addr ) {
		    table[ i ] = table + num_table;
		    table[ num_table ] = 0;
		} else if ( val == sig_addr_swap ) {
		    table[ i ] = table + num_table;
		    table[ num_table ] = ( void * ) 1l;
		} else {
		    table[ i ] = val;
		}
	    }
    
	    * ( void ** ) state = table;
    
	    return ( void * ) 0;
	}
    }
    
    return ( void * ) -1l;
}

/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    unsigned int    * p_bom_state$1;
    unsigned int      bom_state$1;
    unsigned int      new_bom_state$1;

    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    p_bom_state$1 = ( unsigned int * ) ( handle[ 'czz_count(`czz_num')` ] );
    bom_state$1 = * p_bom_state$1;
    new_bom_state$1 = bom_state$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( bom_state$1 ) {
	if ( sizeof( 'RETURN_VAR` ) == 2 ) {
	    'RETURN_VAR` = ( 'RETURN_VAR` << 8 ) | ( ( ( unsigned ) 'RETURN_VAR` ) >> 8 );
	} else if ( sizeof( 'RETURN_VAR` ) == 4 ) {
	    'RETURN_VAR` =
		  ( ( 'RETURN_VAR` & 0x00ff ) << 24 )
		| ( ( 'RETURN_VAR` & 0xff00 ) << 8  )
		| ( ( 'RETURN_VAR` >> 8  ) & 0xff00 )
		| ( ( 'RETURN_VAR` >> 24 ) & 0x00ff )
	    ;
	}
    }

    if ( sizeof( 'RETURN_VAR` ) == 2 ) {
	if ( 'RETURN_VAR` == BOM_CODE_DETECT_16 ) {
	    'RETURN_VAR` = BOM_CODE;
	    bom_state$1 = ! bom_state$1;
	}
    }
    if ( sizeof( 'RETURN_VAR` ) == 4 ) {
	if ( 'RETURN_VAR` == BOM_CODE_DETECT_32 ) {
	    'RETURN_VAR` = BOM_CODE;
	    bom_state$1 = ! bom_state$1;
	}
    }
	
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */

')
define(`UPDATE_STATE',`
    'defn(`UPDATE_STATE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    * p_bom_state$1 = new_bom_state$1;
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
define(`INITIALIZE_STATE',`
    'defn(`INITIALIZE_STATE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    * p_bom_state$1 = 0;
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
define(`ADJUST_STATE',`
    'defn(`ADJUST_STATE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    new_bom_state$1 = bom_state$1;
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
')

define(CZZ_UNAME(),`
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
