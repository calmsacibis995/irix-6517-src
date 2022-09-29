/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.13 $"

#include <locale.h>
#include <pfmt.h>
#include <sys/systeminfo.h>
#include <sys/syssgi.h>
#include "stdio.h"

unsigned sysid();
/*
 * Print out unique system id
 */
main( argc, argv )
int	argc;
char	*argv[];
{
	register int i;
	register int c;
	short sflag = 0;
	short verbose = 0;
	int ret_val;
	char	systemid[ MAXSYSIDSIZE ];

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel("UX:sysinfo");

	while((c = getopt(argc, argv, "sv")) != -1)
		switch(c) {
		case 's':	/* print sum */
			sflag++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			Usage();
			/* NOTREACHED */
		}


	if (sflag) {
		/* if there was an error (ret_val < 0) or no bytes were copied
		 (ret_val ==0) no serial number was retrieved */
		if ((ret_val = sysinfo(SI_HW_SERIAL, systemid, MAXSYSIDSIZE)) >= 0) {
			printf("%s\n", systemid);
		} else {
			fprintf(stderr, "Could not retrieve system serial number.\n");
			exit(1);
		}
	}
	else if (verbose) {
		module_info_t mod_info;
		int num_modules;


		if ((num_modules = get_num_modules()) < 0) {
			fprintf(stderr, "Could not retrieve system serial number information.\n");
			exit(1);
		}

		for (i = 0; i < num_modules; i++) {

			if ((ret_val = get_module_info(i, &mod_info, sizeof(module_info_t))) >= 0) {
				
				printf("%llu ",mod_info.serial_num);

				if (verbose > 1) {
					printf("%s ",
					       mod_info.serial_str);
				}
				printf("Module: %d\n",mod_info.mod_num);
				
			}
			else {
				fprintf(stderr, "Could not retrieve system serial number"
					" for module index %d.\n",i);
			}
		}
	}
	else {
		/* GET_SYSID returns nonzero on error */
		if (!syssgi( GET_SYSID, systemid)) {
			pfmt(stdout, MM_NOSTD, ":7:System ID:\n");
			for (i = 0; i < MAXSYSIDSIZE; i++) {
				printf("%2.2x  ", systemid[i] & 0xff);
				if ((i % 16) == 15)
					printf("\n");
			}
		} else {
			fprintf(stderr, "Couldn't retrieve serial number.\n");
			exit(1);
		}

	}

	exit(0);
}

Usage()
{
	pfmt(stderr, MM_INFO, ":6:Usage: sysinfo [-s][-v][-vv]\n");
	exit(-1);
}
