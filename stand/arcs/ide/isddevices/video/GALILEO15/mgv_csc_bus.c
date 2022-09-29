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
**      FileName:       mgv_csc_bus.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */

__uint32_t mgv_csc_BusTest(__uint32_t, char **);

/*
 * NAME: mgv_csc_BusTest
 *
 * FUNCTION: Writes and reads back walking 0's/1's & 0x00,0x55, 0xaa on to 
 * the indirect address register.
 *
 * INPUTS:  argc, argv 
 *
 * OUTPUTS: -1 if error, 0 if no error
 */

__uint32_t
mgv_csc_BusTest(__uint32_t argc, char **argv)
{

      uchar_t send,rcv,mask ;
      __uint32_t errors = 0, errCode ;
      __uint32_t  badarg = 0, limit, i;
      __int32_t bus, test = 3;
      __int32_t ind_addr;


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
	Usage: mgv_csc_bus -b [0|1] [-t [0|1|2|3]]\n\
		-b --> Bus   0-Address   1-Data\n\
		-t --> Test   0-walking 0   1-walking 1    2-test pattern 3-all values \n\
		( -t option is used only for data bus test)\n");

	return -1;
     }
     
     if ( bus == 0 ) errCode = CSC_ADDR_BUS_TEST ;
     else errCode = CSC_DATA_BUS_TEST ;

     msg_printf(SUM, "%s\n",mgv_err[errCode].TestStr);

     if ( bus == 0 ) {                            /* Address Bus test */
	
     	for ( i = 0; i < 0xff; i++) { 

		send = (uchar_t) i;

		CSC_W1(mgvbase, CSC_ADDR, send);
		CSC_R1(mgvbase, CSC_ADDR, rcv );

        	if (send != rcv) {
	  		errors++;
          		msg_printf(ERR, "exp 0x%x rcv 0x%x\n",send, rcv);
        	}
	}
     }
     else { /* Data bus test */

	if ( test < 0 || test > 3 ) test = 3;

	for (ind_addr = 0x0; ind_addr < 0x4; ind_addr++) {  /* for each indirect reg */
	  switch(ind_addr) {
	    case (0x0) :
		limit = 3 ; /* no of defined bits in register */
		mask = 0x7; /* ie. 111 for the 3 bits above */
		break;
	    case (0x1) :
		limit = 6;
		mask = 0x3f;
		break;
	    case (0x2) :
		limit = 8;
		mask = 0xff;
		break; 
	    case (0x3) :
		limit = 6;
		mask = 0x3f;
	        CSC_W1(mgvbase, CSC_ADDR, 0x2);
 	        CSC_W1(mgvbase, CSC_DATA, 0x0);
		break; 
 	    default :
		msg_printf(DBG, "Error - Invalid register\n");
		break;
	  }
        if ( test == 2 ) limit = 3;
	else if (test == 3)
		limit = mask;

        for ( i = 0; i < limit ; i++) { 

           if (test == 0)
		send = ~ (mgv_walk_one[i] & mask) ;/*  walking zero bit test */
	   else if (test == 1)
		send =  mgv_walk_one[i] & mask ;   /*  walking one  bit test */
	   else if (test == 2) 
		send =  (mgv_patrn32[i] & mask) ;  /*  test pattern */
	   else send = (uchar_t) i;		   /*  all values between 0 & 0xff */

	   /* Indirect address register 2 is the only R/W register, so choose that */
	   CSC_W1(mgvbase, CSC_ADDR, ind_addr);
 	   CSC_W1(mgvbase, CSC_DATA, send);
	   CSC_R1(mgvbase, CSC_DATA, rcv );

          if (ind_addr == 2)  
		send = send << 1; /* for these regs. pattern is left shifted one bit upon reading */
            if (ind_addr == 3) 
		send = (send << 1) & mask; /* for these two regs. pattern is left shifted one bit upon reading */

          if (send != rcv) {
	    errors++;
            msg_printf(ERR, "ind_addr 0x%x exp 0x%x rcv 0x%x\n",ind_addr,send, rcv);
          }
      }

      }
   }
   return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}
