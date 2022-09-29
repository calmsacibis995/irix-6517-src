/*
 * ABIinfo.c
 *
 *      query information about MIPS ABI systems
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

#ident "$Revision: 1.1 $"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <ABIinfo.h>
#include <msgs/uxsgicore.h>

struct vals {
    char *name;
    int val;
};

struct vals selector_tab[] = { /* command names for MIPS ABI ABIinfo */
	"ABIinfo_BlackBook",	ABIinfo_BlackBook,
	"ABIinfo_mpconf",	ABIinfo_mpconf,
	"ABIinfo_abicc",	ABIinfo_abicc,
	"ABIinfo_XPG",		ABIinfo_XPG,
	"ABIinfo_backtrace",	ABIinfo_backtrace,
	"ABIinfo_largefile",	ABIinfo_largefile,  
	"ABIinfo_longlong",	ABIinfo_longlong, 
	"ABIinfo_X11",		ABIinfo_X11,
	"ABIinfo_mmap",		ABIinfo_mmap,
	0
};

int
main(int argc, char **argv)
{
	int		c,i;
	size_t		ret;
	int		err=0;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore.abi");
	(void)setlabel("UX:ABIinfo");

	errno = 0;
	while ((c=getopt(argc, argv, "")) != EOF) 
	{
		switch(c) 
		{
			case '?':
				++err;
				break;
		}
	}
	if(err || argc == optind || (argc-optind) > 2 || ((argc-optind) != 1))
	{
		_sgi_nl_usage(SGINL_USAGE, "UX:ABIinfo",
			gettxt(_SGI_DMMX_ABIinfo_usage, "ABIinfo selector"));
		exit(2);
	}
	/* Check selector string value */
	for(i=0 ; selector_tab[i].name != NULL; i++) {
		if ((strcmp((const char *)argv[optind], 
			(const char *)selector_tab[i].name)) == 0)
		{
			ret = ABIinfo(selector_tab[i].val);
			if( ret == -1 && errno == EINVAL )
				exit(2);
		}
		exit(0);
	}
	exit(1);
}
