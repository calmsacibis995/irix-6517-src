/* __file__ =============================================== *
 *
 *  Index converter. This is equivalent to a copy.
 */

define(CZZ_TNAME(),`
define(`ucs_4_t','CZZ_TYPE()`)
')
CZZ_TNAME()

/* === Following found in __file__ line __line__ ============= */
#define UTF16_HIGH	0xd800
#define UTF16_LOW	0xdc00
#define	UTF16_BITS	10
#define UTF16_MASK	((1<<UTF16_BITS)-1)


/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    'ucs_4_t`		    outval$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    'defn(`INITIALIZE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    /* No init reqd for utf-16 */
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( ( 'RETURN_VAR` & ~UTF16_MASK ) == UTF16_HIGH ) {
	outval$1 = (  ( ( 'ucs_4_t` ) 'RETURN_VAR` ) & UTF16_MASK ) << UTF16_BITS;
	'defn(`GET_VALUE')`
	if ( ( 'RETURN_VAR` & ~UTF16_MASK ) != UTF16_LOW ) {
	    goto illegal_recovery;
	}
	outval$1 =
	    ( outval$1 ) + ( ( 'RETURN_VAR` ) & UTF16_MASK ) + ( 1 << 16 );

    } else {
	outval$1 = 'RETURN_VAR`;
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
    'define(`RETURN_VAR',outval$1)`
')
')

define(CZZ_UNAME(),`
    undefine(`GET_VALUE')
')

/* __file__ ============================================== */
