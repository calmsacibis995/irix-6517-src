
/* __file__ =============================================== *
 *
 *  Output converter for wchar_t to BIG5
 */

define(CZZ_TNAME(),`
define(`OUTPUT_SCALAR_TYPE','CZZ_TYPE()`)
define(`OUTPUT_TYPE','CZZ_NAME()`_outval_type)
')
CZZ_TNAME()

typedef struct b5_code {
	int	boundary;
	int	diff;
} b5_code_t;

static const b5_code_t cns1tab[18] = {
	    147             , 0,
	    148             , 1,
	    149             , -1,
	    408+1411        , 0,
	    408+1835        , 1,
	    408+1836        , -424,
	    408+3712        , 0,
	    408+3713        , 387,
	    408+4100        , -1,
	    408+4625        , 0,
	    408+4626        , 189,
	    408+4815        , -1,
	    408+4899        , 0,
	    408+4900        , 146,
	    408+4954        , -1,
	    408+4955        , 0,
	    408+4956        , -2,
	    408+5046        , -1,
};

static const b5_code_t cns2tab[49] = {
	    9      , 0,
	    42     , 1,
	    43     , 49,
	    91     , 0,
	    137    , 1,
	    138    , 168,
	    305    , 0,
	    2145   , 1,
	    2146   , 648,
	    2254   , 1,
	    2791   , 2,
	    2792   , -645,
	    2892   , 2,
	    2893   , -637,
	    3292   , 1,
	    4929   , 2,
	    4930   , 551,
	    5076   , 1,
	    5077   , 542,
	    5364   , 0,
	    5365   , 863,
	    5481   , -1,
	    5618   , 0,
	    5722   , 1,
	    5723   , 422,
	    5945   , 0,
	    6143   , 1,
	    6225   , 2,
	    6311   , 3,
	    6312   , -366,
	    6320   , 2,
	    6473   , 3,
	    6529   , 4,
	    6530   , 304,
	    6643   , 3,
	    6644   , -321,
	    6786   , 2,
	    6787   , -310,
	    6832   , 1,
	    6903   , 2,
	    6904   , 196,
	    7098   , 1,
	    7588   , 2,
	    7589   , 45,
	    7632   , 1,
	    7633   , 2,
	    7644   , 3,
	    7645   , -9,
	    7649   , 2
};

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    int			codeset$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`OUTPUT_REFERENCE',`( '$`1 )->val')
define(`OUTPUT_SETVAL',`
    'defn(`GET_VALUE')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    if ( ( codeset$1 = (('RETURN_VAR`) >> EUCWC_SHFT)) == ((P11)>>EUCWC_SHFT)) {
	/*  BIG5: cns-1 */
	unsigned char c1, c2;
	int i, num, rem;

	'OUTPUT_WCTOB_FAIL`
	'OUTPUT_COUNT_ADJUST(2)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-2))`
	c1 = ( unsigned char ) (('RETURN_VAR` >> 7) & 0x7f) | 0x80;
	if ( c1 < 040 ) {
	    'OUTPUT_COUNT_ADJUST(-2)`
	    goto illegal_recovery;
	}
	c2 = ( unsigned char ) ('RETURN_VAR` & 0x7f) | 0x80;
	'OUTPUT_PTR_ADJUST(2)`


        if (((c1 == 0xa1) || (c1 == 0xa2)) && ((c2 >= 0xa1) && (c2 <= 0xfe)))
            num = (c1 - 0xa1)*94 + c2 - 0xa1;
        else if ((c1 == 0xa3) && ((c2 >= 0xa1) && (c2 <= 0xce)))
            num = 188 + c2 - 0xa1;
        else if ((c1 == 0xa4) && ((c2 >= 0xa1) && (c2 <= 0xfe)))
            num = 234 + c2 - 0xa1;
        else if ((c1 == 0xa5) && ((c2 >= 0xa1) && (c2 <= 0xf0)))
            num = 328 + c2 - 0xa1;
        else if (((c1 >= 0xc4) && (c1 <= 0xfc)) &&
                         ((c2 >= 0xa1) && (c2 <= 0xfe)))
            num = 408 + (c1 - 0xc4)*0x5e + c2 - 0xa1;
        else if ((c1 == 0xfd) && ((c2 >= 0xa1) && (c2 <= 0xcb)))
            num = 408 + 5358 + c2 - 0xa1;

	for (i = 0 ; i < 18 ; ++i) {
            if (num <= cns1tab[i].boundary) {
                num += cns1tab[i].diff;
                break;
	    }
	}

	if(num >= 408) {
	    num -= 408;
	    c1 =  0xa4 + num/0x9d;
	} else 
	    c1 =  0xa1 + num/0x9d;

	rem = num % 0x9d;
	if (rem < 0x3f)
	    c2 = 0x40 + rem;
	else
	    c2 = 0xa1 + (rem-0x3f);

	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, c2)`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, c1)`;
    } else if ( codeset$1 == ((P01)>>EUCWC_SHFT)) {
	/* BIG5: cns-2 */
	unsigned char c1, c2;
	int i, num, rem;

	'OUTPUT_WCTOB_FAIL`
	'OUTPUT_COUNT_ADJUST(2)`
	'OUTPUT_CHECK_COUNT(< 0,OUTPUT_COUNT_ADJUST(-2))`
	c1 = ( unsigned char ) (('RETURN_VAR` >> 7) & 0x7f) | 0x80;
	if ( c1 < 040 ) {
	    'OUTPUT_COUNT_ADJUST(-2)`
	    goto illegal_recovery;
	}
	c2 = ( unsigned char ) ('RETURN_VAR` & 0x7f) | 0x80;
	'OUTPUT_PTR_ADJUST(2)`

        if (((c1 >= 0xa1) && (c1 <= 0xf1)) && ((c2 >= 0xa1) && (c2 <= 0xfe)))
            num = (c1 - 0xa1)*0x5e + c2 - 0xa1;
        else if ((c1 == 0xf2) && ((c2 >= 0xa1) && (c2 <= 0xc4)))
            num = 7614 + c2 - 0xa1;

	for (i = 0 ; i < 49 ; ++i) {
            if (num <= cns2tab[i].boundary) {
                num += cns2tab[i].diff;
		break;
            }
	}

        c1 = 0xc9 + num/0x9d;
        rem = num % 0x9d;
        if (rem < 0x3f)
            c2 = 0x40 + rem;
        else
            c2 = 0xa1 + (rem-0x3f);

	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 1, c2)`;
	'OUTPUT_ASSIGN( OUTPUT_PTR_VAR - 2, c1)`;
    } else {
	if ('RETURN_VAR` >= 0200) {
	    goto illegal_recovery;
	}
        'OUTPUT_CHECK_COUNT(<= 0,)`
        'OUTPUT_COUNT_ADJUST(1)`
        'OUTPUT_ASSIGN( OUTPUT_PTR_VAR, RETURN_VAR )`;
        'OUTPUT_PTR_ADJUST(1)`
    }
    /* === Previous code found in ''__file__`` line ''__line__`` ========= */
')
define(`OUTPUT_WCTOB_RETVAL',`
    'defn(`OUTPUT_SETVAL')`
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

