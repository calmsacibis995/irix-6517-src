/* __file__ =============================================== *
 *
 *  euc to unix wide char converter.
 */

define(`EUC_TYP_NAME',czz_cat(iconv_,CZZ_NAME(),_ttable))
typedef struct EUC_TYP_NAME {
    unsigned int	type_mask;
    unsigned int	mask_shift;
    struct 	{
	int		len;
	char		prefix;
	unsigned int	mask;
    } uwc_eucw1[ 8 ];
} EUC_TYP_NAME;

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1',int)
')

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    ''EUC_TYP_NAME``	  *p_uwct$1;
    'INTERMEDIATE_TYPE$1`	    state$1;
    'INTERMEDIATE_TYPE$1`	    mask$1;
    'INTERMEDIATE_TYPE$1`	    len$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
   p_uwct$1 = ( ''EUC_TYP_NAME`` * ) handle[ 'czz_count(`czz_num')` ];
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	'defn(`GET_VALUE')`

	if ( 'RETURN_VAR` < 0x80 ) {
bad_prefix$1:
	    state$1 = 'RETURN_VAR`;
	    goto cont$1;
	} else {
	    switch ( 'RETURN_VAR` ) {
		case 0x8e : {
		    state$1 = 0;
		    len$1 = p_uwct$1->uwc_euc[ 1 ].len;
		    mask$1 = p_uwct$1->uwc_euc[ 1 ].mask;
		    break;
		}
		case 0x8f : {
		    state$1 = 0;
		    len$1 = p_uwct$1->uwc_euc[ 2 ].len;
		    mask$1 = p_uwct$1->uwc_euc[ 2 ].mask;
		    break;
		}
		default : {
		    if ( c < 0xa0 ) {
			goto bad_prefix$1;
		    }
		    state$1 = 'RETURN_VAR` & 0x7f;
		    len$1 = p_uwct$1->uwc_euc[ 0 ].len;
		    mask$1 = p_uwct$1->uwc_euc[ 0 ].mask;
		}
	    }

	    switch ( len$1 - 1 ) {

		default : {
		    goto illegal_recovery;
		}
		case 4 : 'defn(`GET_VALUE')`
		    if ( 'RETURN_VAR` < 0x80 ) {
			goto illegal_recovery;
		    }
		    state$1 <<= 7;
		    state$1 |= 'RETURN_VAR` & 0x7f;
		}
		case 3 : 'defn(`GET_VALUE')`
		    if ( 'RETURN_VAR` < 0x80 ) {
			goto illegal_recovery;
		    }
		    state$1 <<= 7;
		    state$1 |= 'RETURN_VAR` & 0x7f;
		}
		case 2 : 'defn(`GET_VALUE')`
		    if ( 'RETURN_VAR` < 0x80 ) {
			goto illegal_recovery;
		    }
		    state$1 <<= 7;
		    state$1 |= 'RETURN_VAR` & 0x7f;
		}
		case 1 : 'defn(`GET_VALUE')`
		    if ( 'RETURN_VAR` < 0x80 ) {
			goto illegal_recovery;
		    }
		    state$1 <<= 7;
		    state$1 |= 'RETURN_VAR` & 0x7f;
		}
	    }

	    state$1 |= mask$1;
	}
cont$1:

    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',state$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`INTERMEDIATE_TYPE$1')
    undefine(`GET_VALUE')
')

/* End __file__ ============================================== */
