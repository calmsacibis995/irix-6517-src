/* __file__ =============================================== *
 *
 *  SJIS eucWchar converter.
 */

define(CZZ_TNAME(),`
define(`INTERMEDIATE_TYPE$1',int)
')

define(CZZ_CNAME(),`
define(`DECLARATIONS',`
    /* === Entering in ''__file__`` line ''__line__`` ============= */
    'defn(`DECLARATIONS')`
    /* === Following found in ''__file__`` line ''__line__`` ============= */
    'INTERMEDIATE_TYPE$1`	    state$1;
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`INITIALIZE',`
    /* === Entering in ''__file__`` line ''__line__`` ============= */
    'defn(`INITIALIZE')`
    /* === Previous found in ''__file__`` line ''__line__`` ============= */
')
define(`GET_VALUE',`
	/* === Following found in ''__file__`` line ''__line__`` ============= */
	'defn(`GET_VALUE')`
	/* === Back in ''__file__`` line ''__line__`` ============= */
	
	if ('RETURN_VAR` <= 0x7f) { /* ASCII */
		state$1 =('INTERMEDIATE_TYPE$1`)  'RETURN_VAR`;
		goto cont$1;
	} if ('RETURN_VAR` >= 0xa1 && 'RETURN_VAR` <= 0xdf) {
		state$1 = P01 + ('RETURN_VAR` & 0177);
		goto cont$1;
	} else {
		unsigned int c1, c2;
		c1 = (unsigned int) 'RETURN_VAR`;

        /* ============ Get next input character. ===================== */

        /* === leaving in ''__file__`` line ''__line__`` ============= */
        	'defn(`GET_VALUE')`
        /* === Back in ''__file__`` line ''__line__`` ============= */

		c2 = (unsigned int) 'RETURN_VAR`;

                c1 = c1 + c1 - ((c1 <= 0x9f) ? 0xe1 : 0x161);
                if(c2 < 0x9f)
                     c2 = c2 - ((c2 > 0x7f) ? 0x20 : 0x1f);
                else {
                     c2 = c2 - 0x7e;
                     c1++;
                }
		state$1 = P11 + ((c1&0x7f) << 7) + (c2&0x7f);
		goto cont$1;
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
