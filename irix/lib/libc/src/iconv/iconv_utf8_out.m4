/* __file__ =============================================== *
 *
 *  Output converter for utf8
 */

define(CZZ_TNAME(),`
define(`OUTPUT_SCALAR_TYPE','CZZ_TYPE()`)
define(`OUTPUT_TYPE','CZZ_NAME()`_outval_type)
')
CZZ_TNAME()

define(CZZ_CNAME(),`
define(`OUTPUT_REFERENCE',`( '$`1 )->val')
define(`OUTPUT_SETVAL',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( ! ( 'RETURN_VAR` >> 7 ) ) {
	'OUTPUT_CHECK_COUNT(<= 0,)`
	'OUTPUT_COUNT_ADJUST(1)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, RETURN_VAR )`;
	'OUTPUT_PTR_ADJUST(1)`
    } else if ( ! ( 'RETURN_VAR` >> (6+5) ) ) {
	'OUTPUT_COUNT_ADJUST(2)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-2))`
	'OUTPUT_PTR_ADJUST(2)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, ( ( ( RETURN_VAR      ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, ( ( ( RETURN_VAR >> 6 )        ) + 0xc0 ) )`;
    } else if ( ! ( 'RETURN_VAR` >> (6+5+5) ) ) {
	'OUTPUT_COUNT_ADJUST(3)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-3))`
	'OUTPUT_PTR_ADJUST(3)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, ( ( ( RETURN_VAR      ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, ( ( ( RETURN_VAR >> 6 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 3, ( ( ( RETURN_VAR >>12 )        ) + 0xe0 ) )`;
    } else if ( ! ( 'RETURN_VAR` >> (6+5+5+5) ) ) {
	'OUTPUT_COUNT_ADJUST(4)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-4))`
	'OUTPUT_PTR_ADJUST(4)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, ( ( ( RETURN_VAR      ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, ( ( ( RETURN_VAR >> 6 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 3, ( ( ( RETURN_VAR >>12 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 4, ( ( ( RETURN_VAR >>18 )        ) + 0xf0 ) )`;
    } else if ( ! ( 'RETURN_VAR` >> (6+5+5+5+5) ) ) {
	'OUTPUT_COUNT_ADJUST(5)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-5))`
	'OUTPUT_PTR_ADJUST(5)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, ( ( ( RETURN_VAR      ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, ( ( ( RETURN_VAR >> 6 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 3, ( ( ( RETURN_VAR >>12 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 4, ( ( ( RETURN_VAR >>18 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 5, ( ( ( RETURN_VAR >>24 )        ) + 0xf8 ) )`;
    } else if ( ! ( 'RETURN_VAR` >> (6+5+5+5+5+5) ) ) {
do_6$1:
	'OUTPUT_COUNT_ADJUST(6)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-6))`
	'OUTPUT_PTR_ADJUST(6)`
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, ( ( ( RETURN_VAR      ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, ( ( ( RETURN_VAR >> 6 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 3, ( ( ( RETURN_VAR >>12 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 4, ( ( ( RETURN_VAR >>18 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 5, ( ( ( RETURN_VAR >>24 ) & 0x3f ) + 0x80 ) )`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 6, ( ( ( RETURN_VAR >>30 )        ) + 0xfc ) )`;
    } else {
	/* The value is out of range - drop the top bit and do a 6 byte	*/
	/* load.							*/
	'OUT_CHAR_NOT_FOUND`
	'RETURN_VAR` &= ((~0u)>>1);
	goto do_6$1;
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
define(`OUTPUT_WCTOB_RETVAL',`
    'defn(`GET_VALUE')`
    if ( ! ( 'RETURN_VAR` >> 7 ) ) {
	return 'RETURN_VAR`;
    }
    return EOF;
')
')

/* === Following found in __file__ line __line__ ============= */
ifelse(CZZ_PACK(),p,`#pragma pack(1)')
typedef struct OUTPUT_TYPE {
    OUTPUT_SCALAR_TYPE	    val;
} OUTPUT_TYPE;
ifelse(CZZ_PACK(),p,`#pragma pack(0)')
/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_UNAME(),`
    undefine(`OUTPUT_SCALAR_TYPE')
    undefine(`OUTPUT_TYPE')
    undefine(`OUTPUT_SETVAL')
    undefine(`OUTPUT_WCTOB_RETVAL')
')

/* __file__ ============================================== */
