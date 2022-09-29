/* __file__ =============================================== *
 *
 *  Input converter for UTF8
 */

define(CZZ_TNAME(),`
define(`INPUT_SCALAR_TYPE','CZZ_TYPE()`)
define(`INPUT_TYPE','CZZ_NAME()`_inval_type)
')
CZZ_TNAME()

ifelse(`UTF8_TABLE',done, ,`
/* utf8_table_correction is a precomputation of the errors associated	*/
/* with not correcting for the bitmasks at the time of reading the utf8	*/
/* data.								*/

typedef struct utf8_table_t {
    unsigned int    utf8_table_correction[6];
    unsigned char   utf8_num_read[256];
} utf8_table_t;
#define B(N) ((unsigned)0xff&(unsigned)(0xff<<(8-(N))))
#define UTF8_COMPUTE_HEAD( A0, A1, A2, A3, A4, A5 ) \
    (((((((((((A5)<<6)+(A4))<<6)+(A3))<<6)+(A2))<<6)+(A1))<<6)+(A0))
static const utf8_table_t   utf8_table_data[1] = {
{
{
    UTF8_COMPUTE_HEAD( B(0), B(0), B(0), B(0), B(0), B(0) ),
    UTF8_COMPUTE_HEAD( B(1), B(2), B(0), B(0), B(0), B(0) ),
    UTF8_COMPUTE_HEAD( B(1), B(1), B(3), B(0), B(0), B(0) ),
    UTF8_COMPUTE_HEAD( B(1), B(1), B(1), B(4), B(0), B(0) ),
    UTF8_COMPUTE_HEAD( B(1), B(1), B(1), B(1), B(5), B(0) ),
    UTF8_COMPUTE_HEAD( B(1), B(1), B(1), B(1), B(1), B(6) ),
},

#define UTF8_BADVAL 6	/* make this value one larger than the maximum	*/
			/* valid value in utf8_num_read.  This gives the*/
			/* compiler half a chance to optimize the switch*/
			/* statement that these vales are used in.	*/

/* utf8_num_read is a precomputed table of the number of characters to	*/
/* read in a given given the first byte of a UTF8 sequence		*/
#define CNR(V) \
    (									\
	(V) < B(1) ? 0							\
	: (V) < B(2) ? UTF8_BADVAL					\
	: (V) < B(3) ? 1						\
	: (V) < B(4) ? 2						\
	: (V) < B(5) ? 3						\
	: (V) < B(6) ? 4 : 5						\
    )
{
czz_forloop(CZZX,0,255,`CNR(CZZX),
')
}
}
};
#undef B
#undef UTF8_COMPUTE_HEAD
#undef CNR
define(`UTF8_TABLE',done)
')

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    unsigned int	    chin;
    unsigned int	    toread;
    /* === Previous found in ''__file__`` line ''__line__`` ============== */
')
define(`INPUT_REFERENCE',`( '$`1 )->val	/* = ''__file__`` line ''__line__`` =*/')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	'INPUT_CHECK_COUNT(<= 0,)`
	chin = 'INPUT_REFERENCE( INPUT_PTR_VAR )`;
	'INPUT_COUNT_ADJUST(1)`
	'INPUT_CHECK_VALUE( chin )`
	'INPUT_PTR_ADJUST(1)`
	toread = utf8_table_data->utf8_num_read[ chin ];
	if ( toread ) {
	    'INPUT_CHECK_COUNT(< toread,)`
	    'INPUT_PTR_ADJUST(toread)`
	    switch( toread ) {
		case UTF8_BADVAL: {
		    /* started from an incorrect character so this is	*/
		    /* an illegal sequence. Time to recover and exit	*/
		    goto illegal_recovery;
		}
		case 5: chin <<= 6; chin += 'INPUT_REFERENCE( INPUT_PTR_VAR - 5 )`;
			'INPUT_CHECK_VALUE( INPUT_REFERENCE( INPUT_PTR_VAR - 5 ) )`
		case 4: chin <<= 6; chin += 'INPUT_REFERENCE( INPUT_PTR_VAR - 4 )`;
			'INPUT_CHECK_VALUE( INPUT_REFERENCE( INPUT_PTR_VAR - 4 ) )`
		case 3: chin <<= 6; chin += 'INPUT_REFERENCE( INPUT_PTR_VAR - 3 )`;
			'INPUT_CHECK_VALUE( INPUT_REFERENCE( INPUT_PTR_VAR - 3 ) )`
		case 2: chin <<= 6; chin += 'INPUT_REFERENCE( INPUT_PTR_VAR - 2 )`;
			'INPUT_CHECK_VALUE( INPUT_REFERENCE( INPUT_PTR_VAR - 2 ) )`
		case 1: chin <<= 6; chin += 'INPUT_REFERENCE( INPUT_PTR_VAR - 1 )`;
			'INPUT_CHECK_VALUE( INPUT_REFERENCE( INPUT_PTR_VAR - 1 ) )`
	    }
	    'INPUT_COUNT_ADJUST(toread)`
	    chin -= utf8_table_data->utf8_table_correction[ toread ];
	}
	/* === Previous code found in ''__file__`` line ''__line__`` ========= */
	'define(`RETURN_VAR',chin)`
')
')

/* === Following found in __file__ line __line__ ============= */
ifelse(CZZ_PACK(),p,`#pragma pack(1)')
typedef struct INPUT_TYPE {
    INPUT_SCALAR_TYPE	    val;
} INPUT_TYPE;
ifelse(CZZ_PACK(),p,`#pragma pack(0)')
/* === Previous code found in __file__ line __line__ ========= */

define(CZZ_UNAME(),`
    undefine(`INPUT_SCALAR_TYPE')
    undefine(`INPUT_TYPE')
    undefine(`INPUT_GETVAL')'
)

/* __file__ ============================================== */
