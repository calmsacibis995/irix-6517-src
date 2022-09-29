
/*
 * read and write into any register of CGI1
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include "cosmo2_defs.h"

extern __uint32_t cmd_seq  ;

void mcu_read_reg (int argc, char **argv)
{

	int offset, flag1 = 0 ,  num = 1 ;
	UWORD buf[512];
	

        argc--; argv++;

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &offset);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &offset);
                        }
		flag1 = 1;
		break;
                case 'n':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &num);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &num);
                        }
                
                break;

	default:
		msg_printf(SUM, "usage: mcuget -o offset \n"); 
		return ;
	}
	 argc--; argv++;
   }

	if (flag1) {
		mcu_read_cmd( COSMO2_CMD_OTHER, cmd_seq, offset, num);
		DelayFor(100000);
		getCMD(buf);
		if (num == 1)
		msg_printf(SUM, "offset %x value %x \n", offset, 
				(buf[7] & 0xff00) >> 8 );
		else {
		msg_printf(SUM, "offset %x value %x \n", offset,
				buf[7]);
		}
	}
}


void mcu_write_reg (int argc, char **argv)
{
        __uint32_t offset, flag1 = 0, flag2 = 0  , value;
        
	unsigned char  buf[25] ;
	UWORD  num = 1;


        argc--; argv++;

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int *) &offset);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], (int *) &offset);
                        }
                flag1 = 1;
		msg_printf(SUM, "offset %x \n", offset);
                break;

                case 'v':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], (int *) &value);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], (int *) &value);
                        }
                flag2 = 1;
		msg_printf(SUM, "value %x \n", value);
                break;
	
        default: ;
		msg_printf(SUM, "usage: mcuput -o offset -v value\n"); 
		return ;
        }
         argc--; argv++;
   }

	buf[0] = value ;
	msg_printf(VRB, "value %x  num %d \n",value, num);
        if (flag1&flag2) {
	mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, offset,num , buf);	
        }
		else
		msg_printf(SUM, "usage: mcuput -o offset -v value\n"); 
}

