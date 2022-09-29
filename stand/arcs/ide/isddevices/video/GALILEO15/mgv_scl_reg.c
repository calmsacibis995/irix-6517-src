/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_scl_reg.c
*/
#ifdef IP30

#include "mgv_diag.h"

/*
 * Forward Function References
 */

__uint32_t mgv_scl_RegTest(__uint32_t, char **);

/*
 * NAME: mgv_scl_RegTest
 *
 * FUNCTION: Writes and reads back walking 0's/1's & 0x00,0x55, 0xaa on to 
 * the indirect address registers.
 *
 * INPUTS:  argc, argv 
 *
 * OUTPUTS: -1 if error, 0 if no error
 */

__uint32_t
mgv_scl_RegTest(__uint32_t argc, char **argv)
{

      uchar_t send,rcv ;
      __uint32_t errors = 0, errCode ;
      __uint32_t  badarg = 0, limit, i, j, k;
      __int32_t bus, test = 3, num = 1;
      __int32_t min_addr, max_addr;

      /* get the args */
      argc--; argv++;
      while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'b':
                        if (argv[0][2]=='\0') {
                            atob(&argv[1][0], &bus);
                            argc--; argv++;
                        } else {
                            atob(&argv[0][2], &bus);
                        }
                        break;
                case 't':
                        if (argv[0][2]=='\0') {
                            atob(&argv[1][0], &test);
                            argc--; argv++;
                        } else {
                            atob(&argv[0][2], &test);
                        }
                        break;
                 case 'n':
                        if (argv[0][2]=='\0') {
                           atob(&argv[1][0], &num);
                             argc--; argv++;
                        } else {
                            atob(&argv[0][2], &num);
                        }
                        break;

		default: badarg++; break;
	}
	argc --; argv ++;
     }

     if (badarg || argc ||  bus < 0 || bus > 1  ||  num < 1 || num > 2) {
	msg_printf(SUM,"\
	Usage: mgv_scl_reg -b [0|1] [-t [0|1|2|3]]\n\
		-b --> Bus   0-Address   1-Data\n\
		-t --> Test   0-walking 0   1-walking 1    2-test pattern 3-all values \n\
		( -t option is used only for data bus test)\n");

	return -1;
     }
     
     if (num == 1) {
       min_addr = 0x0;
       max_addr = SCALE1_MAX_REG_ADDR;
     }
     else {
       min_addr = SCALE2_MIN_REG_ADDR;
       max_addr = SCALE2_MAX_REG_ADDR;
     }

     if ( bus == 0 ) errCode = SCL_ADDR_BUS_TEST ;
     else errCode = SCL_DATA_BUS_TEST ;

     msg_printf(SUM, "%s\n",mgv_err[errCode].TestStr);
     msg_printf(SUM, "IN scaler reg code\n");

        /*** Initialize the scaler / TMI  ***/

        /* Reset TMI ASIC */
                GAL_W1(mgvbase, 0x3,0x4);
        /* Clear TMI ASIC */
                GAL_W1(mgvbase, 0x3,0x0);
        /* Reset scaler (bit0) and TMI hardware (bit2) */
                GAL_W1(mgvbase, 0x5,0x5);
                DELAY(500);
        /* Reset TMI hardware (bit2) and pulse the scaler reset (bit3) */
                GAL_W1(mgvbase, 0x5,0xc);
                DELAY(500);
	/* clear the reset */
		GAL_W1(mgvbase, 0x5,0x0);

     if ( bus == 0 ) {                            /* Address Bus test */
	
	/* For Scaler                                   */
	/* only addresses 0 through 0x11 (17) are valid. */
        /*  for SCL 1 and 0x20 thru 0x31 for SCL 2       */
	
        msg_printf(SUM,"Address bus test for Scaler %d\n", num);
     	for ( i = min_addr; i < max_addr; i++) { 
		send = (uchar_t) i;
		SCL_IND_W1(mgvbase, i, send);
		us_delay(500);
	}

     	for ( i = min_addr; i < max_addr; i++) { 
		SCL_IND_R1(mgvbase, i, rcv );
		us_delay(500);
		send = (uchar_t) i;
		if (i%2 != 0) {
			send = send & 0xf;
		}
        	if (send != rcv) {
	  		errors++;
          		msg_printf(ERR, "Address 0x%x exp 0x%x rcv 0x%x\n",i,send, rcv);
        	}
	}

     }
     else { /* Register tests */

	if ( test < 0 || test > 3 ) test = 3;

        if ( test == 2 ) limit = 3;
        else if ( test == 0 || test ==  1 ) limit = 8 ;
	else limit = 0xff;

   	/* XXX If all registers need to read/ written to use a for loop here*/ 
   	for (j=min_addr; j < max_addr; j++){ 
        	for ( i = 0; i < limit ; i++) { 

           		if (test == 0)
				send = ~ (mgv_walk_one[i] & 0xff) ;/*  walking zero bit test */
	   		else if (test == 1)
				send =  mgv_walk_one[i] & 0xff ;   /*  walking one  bit test */
	   		else if (test == 2) 
				send =  (mgv_patrn32[i] & 0xff) ;  /*  test pattern */
	   		else send = (uchar_t) i;		   /*  all values between 0 & 0xff */

			/*Odd Scaler registers are only 4 bits wide*/
			if( (j%2) != 0){
				send = (send & 0xf);
			}
 	   		SCL_IND_W1(mgvbase, j, send);
			us_delay(500);
	   		SCL_IND_R1(mgvbase, j, rcv );
          		if (send != rcv) {
	    			errors++;
            			msg_printf(ERR, "reg# %d exp 0x%x rcv 0x%x\n", j, send, rcv);
          		}
		}
   	}
     }

   return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

#endif
