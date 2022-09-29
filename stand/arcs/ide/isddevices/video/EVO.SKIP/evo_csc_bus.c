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
**      FileName:       evo_csc_bus.c
*/

#include "evo_diag.h"

/*
 * Forward Function References
 */

__uint32_t evo_csc_BusTest(__uint32_t, char **);

/*
 * NAME: evo_csc_BusTest
 *
 * FUNCTION: Writes and reads back walking 0's/1's & 0x00,0x55, 0xaa on to 
 * the indirect address register.
 *
 * INPUTS:  argc, argv 
 *
 * OUTPUTS: -1 if error, 0 if no error
 */

__uint32_t
evo_csc_BusTest(__uint32_t argc, char **argv)
{

      uchar_t send,rcv ;
      __uint32_t errors = 0, errCode ;
      __uint32_t  badarg = 0, limit, i;
      __int32_t bus, test = 3;

      evo_csc_indregs     regInfo[] = {
	        "CSC Input Control", CSC_IN_CTRL, 0x7,
	        "CSC Output Control", CSC_OUT_CTRL, 0x7f,
	        "CSC Coef_Lut LSB", CSC_OFFSET_LS, 0xff,
	        "CSC Coef_Lut_MSB", CSC_OFFSET_MS, 0x3f,
	        "",                                            -1,     0
      };

      evo_csc_indregs     *pRegInfo = regInfo;


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
		default: badarg++; break;
	}
	argc --; argv ++;
     }

     if (badarg || argc ||  bus < 0 || bus > 1) {
	msg_printf(SUM,"\
	Usage: evo_csc_bus -b [0|1] [-t [0|1|2|3]]\n\
		-b --> Bus   0-Address   1-Data\n\
		-t --> Test   0-walking 0   1-walking 1    2-test pattern 3-all values \n\
		( -t option is used only for data tests)\n");

	return -1;
     }
     
     if ( bus == 0 ) errCode = CSC_ADDR_BUS_TEST ;
     else errCode = CSC_DATA_BUS_TEST ;

     msg_printf(SUM, "%s\n",evo_err[errCode].TestStr);

     if ( bus == 0 ) {                            /* Address Bus test */
	
	/* only bits b0, b1, b2 are valid (addresses 0 thorugh 3). H/W 
	 * ignores other bits. So to test only addresses 0-3 		*/
 	msg_printf(DBG, "Testing (Writing/Reading) all valid CSC_ADDR addresses\n");
	   
     	for ( i = 0; i < 3; i++) { 
		send = (uchar_t) i;

		/****************************************************************************************
		*Note the DCB-CRS register introduces another level of indirection in CSC read/writes***
		*Direct registers must now be accessed through the DCB-CRS register indirection ********
		*Indirect CSC registers now involve two levels of indirection **************************
		****************************************************************************************/

		CSC_W1(evobase, i, send);
          	msg_printf(DBG, "After write i 0x%x send 0x%x\n",i, send);
		CSC_R1(evobase, i, rcv);
          	msg_printf(DBG, "After read i 0x%x rcv 0x%x\n",i, rcv);

        	if (send != rcv) {
	  		errors++;
          		msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
        	}
	}
     }
     else { 					/* Data bus test */
	if ( test < 0 || test > 3 ) test = 3;

        if ( test == 2 ) limit = 3;
        else if ( test == 0 || test ==  1 ) limit = 8 ;
	else limit = 0xff;

	/*Begin while loop to write to test all indirect registers*/
    	while (pRegInfo->offset != -1) {
		if (pRegInfo->offset == CSC_OFFSET_MS){
				CSC_IND_W1(evobase, CSC_OFFSET_LS, 0x0);
		}
        	for ( i = 0; i < limit ; i++) { 
			if (test == 0)
				send = ~ (evo_walk_one[i] & pRegInfo->mask) ;/*  walking zero bit test */
	   		else if (test == 1)
				send =  evo_walk_one[i] & pRegInfo->mask ;   /*  walking one  bit test */
	   		else if (test == 2) 
				send =  (evo_patrn32[i] & pRegInfo->mask) ;  /*  test pattern */
	   		else send = ((uchar_t) i & pRegInfo->mask);	     /*  all values between 0 & 0xff */

            		msg_printf(DBG, "Indirect register: %s, data: 0x%x; mask 0xll%x\n",
                		pRegInfo->str, send, pRegInfo->mask);

	  		CSC_IND_W1(evobase, pRegInfo->offset, send); 
			us_delay(1);
	  		CSC_IND_R1(evobase, pRegInfo->offset, rcv); 
			/*rcv = rcv & pRegInfo->mask;*/


            		msg_printf(DBG, "%s exp(before) 0x%llx rcv 0x%llx\n", pRegInfo->str, send , rcv);
			
          		if (pRegInfo->offset == CSC_OFFSET_LS)  
                		send = send << 1;  /*for this reg. patrn is left shifted one bit upon reading */
            		if (pRegInfo->offset == CSC_OFFSET_MS) 
                		send = (send << 1) & pRegInfo->mask;  /*for this reg. patrn is left shifted one bit upon reading*/ 

	   		if (send != rcv) { 
	      			errors++; msg_printf(ERR, "Register %s exp(after) 0x%x rcv 0x%x\n",pRegInfo->str,send, rcv);
           		}
			send = 0x0;
			rcv = 0x0;
            	}
        	pRegInfo++;
        }
     }
   return(_evo_reportPassOrFail((&evo_err[errCode]) ,errors));
}

/* Indirect address register 2/3 are the only (all bits) R/W registers, so choose them */ 
/* XXX If all registers need to read/ written to use a for loop here*/ 
/*
	  CSC_W1(evobase, CSC_ADDR, CSC_OFFSET_LS); 
	  CSC_W1(evobase, CSC_DATA, send); 
	  CSC_R1(evobase, CSC_DATA, rcv ); 

	   if (send != rcv) { 
	      errors++; msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
           }

	   *Choosing address register 3*

	   CSC_W1(evobase, CSC_ADDR, CSC_OFFSET_MS);
 	   CSC_W1(evobase, CSC_DATA, send);
	   CSC_R1(evobase, CSC_DATA, rcv );

          if (send != rcv) {
	    errors++;
            msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
          }
      }

   }
*/
