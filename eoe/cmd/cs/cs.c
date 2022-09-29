/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/



#ident "$Revision: 1.3 $"
#include <sys/types.h>
#include <stdio.h>
#include <sys/syssgi.h>
#include <sys/cellkernstats.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

extern void exit(int);
extern int errno;

int errflag;
int succintflag;
int helpflag;
int membershipflag;
membershipinfo_t meminfo;
char *cmd;

void usage()
{
	fprintf(stderr,"\nUSAGE : %s -[hcs] ",cmd);
	fprintf(stderr,"\n\t c - display the cell membership info ");
	fprintf(stderr,"\n\t h - prints a brief help message");
	fprintf(stderr, "\n\n");
}
main(int argc, char *argv[])
{
	cell_t cell;
	int  c, count;
     
     	cmd = argv[0];
	if(argc == 1)
	     	succintflag = 1;
	while ((c = getopt(argc, argv, 
		"hcs")) != EOF)
	{
		switch(c)
	     	{
	    		case 'c':
		 		membershipflag++;
		  		break;
			case 'h':
		  		helpflag++;
		  		break;
	     		case 's':
				succintflag++;
				break;
	     		default:
				errflag++;
				break;
	     	}
	}
	
	if (errflag || helpflag)
	{
	     	usage();
	     	exit(1);
	}
	
	if (syssgi(SGI_CELL, SGI_IS_OS_CELLULAR))
        {
	     	fprintf(stderr,
			"\nThis is not a cellular operating System\n");
	     	exit(1);
        }
	
     	if(syssgi(SGI_CELL, SGI_MEMBERSHIP_STATS, &meminfo) == -1)
     	{
	  	perror("\nCannot find membership statistics");
	  	exit(1);
     	}

	if(membershipflag)
	{
	     	printf("\nGolden Cell            : %d", meminfo.golden_cell);
	     	printf("\nNumber of Active Cells : %d",
			meminfo.num_active_cells);
	     	printf("\n\n\n%-10s\t%-10s","CELL", "AGE");
	     	for(count = 0 ; count < MAX_CELLS; count++)
			if(meminfo.membership[count])
				printf("\n%-10d\t%-10d", count, 
					meminfo.cell_age[count]);
	}

	if(succintflag)
	{
		for(count = 0 ; count < MAX_CELLS; count++)
                	if(meminfo.membership[count])
                        	printf("  %d", count);
	}
	printf("\n");
	return 0;

	
}


