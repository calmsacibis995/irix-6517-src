
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

int
read_reg (int argc, char **argv)
{

	int offset, flag1 = 0 ;
	__uint64_t value ;
	cgi1_t *nptr;
        _CTest_Info      *pTestInfo = cgi1_info+7;
	__uint32_t recv, recvh;
        long long recv1;


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
	default: break;
	}
	 argc--; argv++;
   }

	if (flag1) {
		recv1=cgi1_read_reg(cosmobase+offset/cgi1_register_bytes) ;
                recv = (__uint32_t)  recv1;
                recvh= (__uint32_t) (recv1>>32);
                msg_printf(SUM, " read : value hi=%x lo=%x from base addres %x\n", recvh, recv , cosmobase+offset/cgi1_register_bytes);
	}
	else
        	msg_printf(SUM, "incorrect usage\n" );

	return 0;
}


int
write_reg (int argc, char **argv)
{
        int offset, flag1 = 0, flag2 = 0 ;
        __uint64_t value ;
        cgi1_t *nptr;
        _CTest_Info      *pTestInfo = cgi1_info+7;
        __uint32_t recv, recvh;
        long long recv1;

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
                case 'v':
                        if (argv[0][2]=='\0') {
                                atobu_L(&argv[1][0],(unsigned long long *)&value);
                                argc--; argv++;
                        } else {
                        	atobu_L(&argv[0][2],(unsigned long long *)&value);
                        }
                flag2 = 1;
                break;
	
        default: break;
        }
         argc--; argv++;
   }

        if (flag1 && flag2) 
		cgi1_write_reg(value, cosmobase+offset/cgi1_register_bytes);
        else
        	msg_printf(SUM, "incorrect usage\n" );

	return 0;
}

void RepeatRead (int argc, char **argv)
{

        int i = 0, repeat = -1, offset = -1;
        __uint64_t temp64;

        --argc; argv++;

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &offset);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &offset);
                        }
                break;
                case 'r':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &repeat);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &repeat);
                        }
                break;
                default: break ;
        }
        --argc; argv++;
   }


        if ((offset == -1) && (repeat == -1) ) {
                msg_printf(SUM, " wrong usage: repeat -o offset -r times\n" );
                return ;
        }

        for ( i = 0; i < repeat ; i++) {
                temp64 = cgi1_read_reg(cosmobase+offset/cgi1_register_bytes);
                msg_printf(SUM, " read from offset %x value %llx at iteration %d\n", offset, temp64, i);
        }
}

void RepeatWrite (int argc, char **argv)
{

        int i = 0, repeat = -1, offset = -1;
        long long temp64, value;

        --argc; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'o':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &offset);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &offset);
                        }
                break;
                case 'v':
                        if (argv[0][2]=='\0') {
                                atob_L(&argv[1][0], &value);
                                argc--; argv++;
                        } else {
                                atob_L(&argv[0][2], &value);
                        }
                break;
                case 'r':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &repeat);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &repeat);
                        }
                break;
                default: break ;
        }
        --argc; argv++;
   }

        if ((offset == -1) && (repeat == -1) ) {
                msg_printf(SUM,"wrong usage: repeat -o offset -r times\n" );
                return ;
        }

        for ( i = 0; i < repeat ; i++) {
                cgi1_write_reg(value, cosmobase+ offset/cgi1_register_bytes);
        }
}
