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
**      FileName:       evo_scl_reg.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t evo_scl_RegTest(__uint32_t, char **);

/*
 * NAME: evo_scl_RegTest
 *
 * FUNCTION: Writes and reads back walking 0's/1's & 0x00,0x55, 0xaa on to 
 * the indirect address registers.
 *
 * INPUTS:  argc, argv 
 *
 * OUTPUTS: -1 if error, 0 if no error
 */

__uint32_t
evo_scl_RegTest(__uint32_t argc, char **argv)
{

      uchar_t send,rcv ;
      __uint32_t errors = 0, errCode ;
      __uint32_t  badarg = 0, limit, i, j, k;
      __int32_t bus, test = 3, num = 1;


      /* get the args */
      argc--; argv++;
      while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'n':
                        if (argv[0][2]=='\0') {
                            atob(&argv[1][0], &num);
                            argc--; argv++;
                        } else {
                            atob(&argv[0][2], &num);
                        }
                        break;
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
		default: badarg++; break;
	}
	argc --; argv ++;
     }

     if (badarg || argc ||  bus < 0 || bus > 1  ||  num < 1 || num > 2) {
	msg_printf(SUM,"\
	Usage: evo_scl_reg -n [1|2] -b [0|1] [-t [0|1|2|3]]\n\
		-n --> Scaler 1-Scaler 1  2-Scaler 2\n\
		-b --> Bus   0-Address   1-Data\n\
		-t --> Test   0-walking 0   1-walking 1    2-test pattern 3-all values \n\
		( -t option is used only for data bus test)\n");

	return -1;
     }
     
     if (num == 1){
	     evobase->scl = evobase->scl1; 
     } 
     else if (num == 2){
	     evobase->scl = evobase->scl2; 
     }

     
     if ( bus == 0 ) errCode = SCL_ADDR_TEST ;
     else errCode = SCL_DATA_TEST ;

     msg_printf(SUM, "%s\n",evo_err[errCode].TestStr);


     if ( bus == 0 ) {                            /* Address Bus test */
	
	/* only addresses 0 through 0x11 (17) are vaild. */
	/* So to test only these addresses write 0-0x11 */
	   
     	for ( i = 0; i < SCALE_MAX_REGS; i++) { 
		send = (uchar_t) i;
		EVO_SCL_W1(evobase, i, send);
		EVO_SCL_R1(evobase, i, rcv );

        	if (send != rcv) {
	  		errors++;
          		msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
        	}
	}
     }
     else { /* Register tests */

	if ( test < 0 || test > 3 ) test = 3;

        if ( test == 2 ) limit = 3;
        else if ( test == 0 || test ==  1 ) limit = 8 ;
	else limit = 0xff;

   	/* XXX If all registers need to read/ written to use a for loop here*/ 
   	for (j=0; j < SCALE_MAX_REGS; j++){ 
		/*Resetting registers to 0x0 to avoid LSB->MSB carry problems*/
   		for (k=0; k < j; k++){ 
 	   		EVO_SCL_W1(evobase, j, 0x0);
		}
        	for ( i = 0; i < limit ; i++) { 

           		if (test == 0)
				send = ~ (evo_walk_one[i] & 0xff) ;/*  walking zero bit test */
	   		else if (test == 1)
				send =  evo_walk_one[i] & 0xff ;   /*  walking one  bit test */
	   		else if (test == 2) 
				send =  (evo_patrn32[i] & 0xff) ;  /*  test pattern */
	   		else send = (uchar_t) i;		   /*  all values between 0 & 0xff */

			/*Odd Scaler registers are only 4 bits wide*/
			if( (j%2) != 0){
				send = (send & 0xf);
			}
 	   		EVO_SCL_W1(evobase, j, send);
	   		EVO_SCL_R1(evobase, j, rcv );
          		if (send != rcv) {
	    			errors++;
            			msg_printf(ERR, "reg# %d exp 0x%x rcv 0x%x\n", j, send, rcv);
          		}
		}
   	}
     }

   return(_evo_reportPassOrFail((&evo_err[errCode]) ,errors));
}
