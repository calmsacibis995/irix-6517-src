/* __file__ =============================================== *
 *
 *  BIG5 eucWchar converter.
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
	} else {
		int num, rem;
		unsigned int c1, c2;
		c1 = (unsigned int) 'RETURN_VAR`;

        /* ============ Get next input character. ===================== */

        /* === leaving in ''__file__`` line ''__line__`` ============= */
        	'defn(`GET_VALUE')`
        /* === Back in ''__file__`` line ''__line__`` ============= */

		c2 = (unsigned int) 'RETURN_VAR`;

		if ((c1 == 0xa1) || (c1 == 0xa2)) {
		    if ((c2 >= 0x40) && (c2 <= 0x7e))
			num = (c1 - 0xa1)*0x9d + (c2 -0x40);
		    else if ((c2 >= 0xa1) && (c2 <= 0xfe))
			num = (c1 - 0xa1)*0x9d + 0x3f + (c2 - 0xa1);
		}
		else if (c1 == 0xa3) {
		    if ((c2 >= 0x40) && (c2 <= 0x7e))
			num = 314 + (c2 - 0x40);
		    else if ((c2 >= 0xa1) && (c2 <= 0xbf))
			num = 377 + (c2 - 0xa1);
		}
		else {
		    if ((c1 >= 0xa4) && (c1 <= 0xc5)) {
		        if ((c2 >= 0x40) && (c2 <= 0x7e))
			    num = (c1 - 0xa4)*0x9d + (c2 - 0x40);
		        else if ((c2 >= 0xa1) && (c2 <= 0xfe))
			    num = (c1 - 0xa4)*0x9d + 0x3f + (c2 - 0xa1);
		    }
		    else if (c1 == 0xc6) {
		        if ((c2 >= 0x40) && (c2 <= 0x7e))
			    num = 5338 + (c2 - 0x40);
		    }
		    else {
			if ((c1 >= 0xc9) && (c1 <= 0xf8)) {
		    	    if ((c2 >= 0x40) && (c2 <= 0x7e))
			        num = (c1 - 0xc9)*0x9d + (c2 - 0x40);
		            else if ((c2 >= 0xa1) && (c2 <= 0xfe))
			        num = (c1 - 0xc9)*0x9d + 0x3f + (c2 - 0xa1);
		        }
		        else if (c1 == 0xf9) {
		            if ((c2 >= 0x40) && (c2 <= 0x7e))
			        num = 7536 + (c2 - 0x40);
		            else if ((c2 >= 0xa1) && (c2 <= 0xd5))
			        num = 7599 + (c2 - 0xa1);
		        }

		        if (num == 92)        num -= 49;
		        else if (num == 306)  num -= 168;
		        else if (num == 2147) num += 645;
		        else if (num == 2256) num += 637;
		        else if (num == 2794) num -= 648;
		        else if (num == 5481) num -= 551;
		        else if (num == 5619) num -= 542;
		        else if (num == 5946) num += 366;
		        else if (num == 6145) num -= 422;
		        else if (num == 6228) num -= 863;
		        else if (num == 6323) num += 321;
		        else if (num == 6477) num += 310;
		        else if (num == 7100) num -= 196;
		        else if (num == 7634) num -= 45;
		        else if (num == 7636) num += 9;
		        else if (num == 6834) num -= 304;
		        else if (((num>=11)   && (num<44))   || ((num>=93)   && (num<139)) ||
		 	        ((num>=307)  && (num<2147)) || ((num>=2148) && (num<2256)) ||
		 	        ((num>=2895) && (num<3294)) || ((num>=4932) && (num<5078)) ||
		 	        ((num>=5620) && (num<5724)) || ((num>=5947) && (num<6145)) ||
		 	        ((num>=6789) && (num<6834)) || ((num>=6906) && (num<7100)) ||
		 	        ((num>=7591) && (num<7634)))
			        num--;
		        else if (((num>=2257) && (num<2794)) || ((num>=2795) && (num<2895)) ||
		 	        ((num>=3295) && (num<4932)) || ((num>=6146) && (num<6228)) ||
		 	        ((num>=6315) && (num<6323)) || ((num>=6647) && (num<6789)) ||
		 	        ((num>=6835) && (num<6906)) || ((num>=7101) && (num<7591)) ||
		 	        ((num>=7648) && (num<7652)) || (num==7635))
			        num -= 2;
		        else if ((num >= 5365) && (num < 5481))    num++;
		        else if (((num>=6229) && (num<6315)) || ((num>=6324) && (num<6477)) ||
		 	        ((num>=6534) && (num<6647)) || ((num>=7637) && (num<7648)))
			        num -= 3;
		        else if ((num >= 6478) && (num < 6534))
			        num -= 4;
		        if ((num < 0) || (num > 7650)) {
		    	    goto illegal_recovery;
		        }
		        c1 = 0xa1 + num/0x5e;
		        rem = num % 0x5e;
		        c2 = 0xa1 + rem;
		        state$1 = P01 + (0x22 << 14) 
					+ ((c1&0x7f) << 7) + (c2&0x7f);
		        goto cont$1;
		    }
		    if (num == 1412)			num += 424;
		    else if (num == 4100)			num -= 387;
		    else if (num == 4815)			num -= 189;
		    else if (num == 4954)			num += 2;
		    else if (num == 5046)			num -= 146;
		    else if ((num >= 1413) && (num < 1837)) num--;
		    else if ((num >= 3713) && (num < 4100)) num++;
		    else if ((num >= 4626) && (num < 4815)) num++;
		    else if ((num >= 4900) && (num < 4954)) num++;
		    else if ((num >= 4956) && (num < 5046)) num++;
		    if ((num < 0) || (num > 5400)) {
		        goto illegal_recovery;
		    }
		    c1 = 0xc4 + num/0x5e;
		    rem = num % 0x5e;
		    c2 = 0xa1 + rem;
		    state$1 = P11 + ((c1&0x7f) << 7) + (c2&0x7f);
		    goto cont$1;
		}

		if ((num < 0) || (num > 407)) {
		    goto illegal_recovery;
		}
	        if (num < 94) {
		    c1 = 0xa1;
		    c2 = num + 0xa1;
		}
		else if ((num >= 84) && (num < 188)) {
		    c1 = 0xa2;
		    c2 = num - 94 + 0xa1;
		}
		else if ((num >= 188) && (num < 234)) {
		    c1 = 0xa3;
		    c2 = num - 188 + 0xa1;
		}
		else if ((num >= 234) && (num < 328)) {
		    c1 = 0xa4;
		    c2 = num - 234 + 0xa1;
		}
		else {
		    c1 = 0xa5;
		    c2 = num - 328 + 0xa1;
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
