/*
 *
 * Cosmo2 gio 1 chip hardware definitions
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

extern __uint32_t timeOut  ;

int
cos2_set_var (int argc, char **argv )
{
	int offset, time ;

    argc--; argv++;

   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'e':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &offset);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &offset);
                        }
			
		 G_errors = offset ;

	msg_printf(VRB, " G_errors %x \n", G_errors);
		break;
                case 't':
                        if (argv[0][2]=='\0') {
                                atob(&argv[1][0], &time);
                                argc--; argv++;
                        } else {
                                atob(&argv[0][2], &time);
                        }
			timeOut = time;
	msg_printf(VRB, " timeOut %x \n", timeOut);
                break;

	default:
		return 1;
	}
	 argc--; argv++;
   }

   return 0;
}
