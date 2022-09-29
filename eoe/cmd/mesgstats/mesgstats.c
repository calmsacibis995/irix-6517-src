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



#ident "$Revision: 1.7 $"
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

int resetflag;
int succintflag;
int detailedflag;
int errflag;
int tflag;
int helpflag;
int sizeflag;
int debugflag;
int mesgsizes[4];
int tot_size;
int count, Index, checkpoint;
mesgstatsinfo_t MesgInfo;
mesgsizestatsinfo_t *mesgsizeinfo,*ptr;
mesginfo_t *umesginfo;
membershipinfo_t meminfo;
char *cmd, fmt[128] ,tempstr[128];
char *mesgtypes[4][4] = {
			"TOTAL MESSAGES", "SIZE IN BYTES","MESSAGE", "COUNT",
		    	"INLINE MESSAGES","SIZE IN BYTES","MESSAGE", "COUNT",
			"OUTOFLINE MESSAGES","SIZE IN BYTES","MESSAGE", "COUNT",
			"OUTOFlINE BUFFFER COUNT","NUMBUFFS","MESSAGE", "COUNT"
		      	 };

char *Truncate ( char *str , int size)
{
	if(strlen(str) > size)
		str[size] = '\0';
	return str;
}
void usage()
{
	fprintf(stderr,"\nUSAGE : %s -[hrtds] ",cmd);
	fprintf(stderr,"\n\tr - resets the message statistics");
	fprintf(stderr,"\n\td - prints the detailed message statistics");
	fprintf(stderr,"\n\tb - prints a brief description of the mesg stats");
	fprintf(stderr,"\n\tt - displays the elapsed time ");
	fprintf(stderr,"\n\th - prints a brief help message");
	fprintf(stderr,"\n\ts - prints the statistics based on message size");
	fprintf(stderr, "\n\n");
}
main(int argc, char *argv[])
{
	cell_t cell;
	int  c, count;
     
	cmd = argv[0];
	if(argc == 1)
	     helpflag = 1;
	while ((c = getopt(argc, argv, 
		"rsdthb")) != EOF)
	{
		switch(c)
	     	{
			case 'b':
				succintflag++;
				break;
	     		case 'd':
		  		detailedflag++;
		  		break;
	     		case 'h':
		  
		  		helpflag++;
		  		break;
	     		case 'r':
		  		resetflag++;
		  		break;

	     		case 's':
		  		sizeflag++;
		  		break;

			case 't':
		  		tflag++;
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

	if(detailedflag && sizeflag )
	{
		succintflag++;
		detailedflag = 0;
	}
	
	if(resetflag)
	{

	     	if(syssgi(SGI_CELL, SGI_MESG_STATS, SGI_RESET_COUNT,  
				&debugflag) == -1)
	     	{
			perror("\nCannot reset the  message statistics ");
		  	exit(1);
	     	}
	     	if(debugflag == -1)
	     	{
		  	fprintf(stderr, "\n Error : Debugging disabled, Cannot "
			  	"evaluate message statistics\n\n");
		  	exit(1);
	     	}
	     	else
		  	printf("\nResetting the system Message statistics\n\n");
	     	return 0;
	}

	if(detailedflag || succintflag )
	{
	     	if(syssgi(SGI_CELL, SGI_MESG_STATS, SGI_REPORT_MESSAGE_COUNT, 
		        &MesgInfo, &debugflag) == -1)
	     	{
		  	perror("\nCannot count number of messages");
		  	exit(1);
	     	}
	     	if(debugflag == -1)
	     	{
		  	fprintf(stderr, "\n Error : Debugging disabled, Cannot"
			  " evaluate message statistics\n\n");
		  	exit(1);
	     	}
		printf("\n");
	     	printf("\nThe time since last reset    : %d secs",
			 MesgInfo.tdiff);
	     	printf("\nThe cellid                   : %d", MesgInfo.cellid);
	     	printf("\nTotal Messages Sent          : %d", MesgInfo.mesgsnt);
	     	printf("\nTotal Messages Rcvd          : %d", MesgInfo.mesgrcv);
	     	printf("\nTotal Message Count is       : %d", 
			MesgInfo.tot_count);
	     	printf("\nTotal Elapsed time is        : %ul\n", 
			MesgInfo.tot_etime);
	}
	
	if(detailedflag)
	{
	     	int count = 0;
	     	if(MesgInfo.tot_msgs)
	     	{
		  	umesginfo = (mesginfo_t *)malloc(sizeof(mesginfo_t)*
						   MesgInfo.tot_msgs);
		  	if(umesginfo == NULL)
		  	{
				fprintf(stderr, "\n Cannot allocate memory ");
				exit(1);
		  	}
		  	if(syssgi(SGI_CELL, SGI_MESG_STATS, 
				SGI_REPORT_MESSAGE_STATS, 
			    MesgInfo.tot_msgs,umesginfo) == -1)	
		  	{
		       		perror("Cannot obtain message statistics ");
		       		exit(1);
		  	}
		  	printf("\n PRINTING INDIVIDUAL MESSAGES :\n");
		  	if(!tflag)
		       		printf("\n%-30s\t%-30s\t%-30s",
			      		Truncate("MESSAGE", 30), 
					Truncate("CALLER", 30), 
					Truncate("CALLS", 30));
		  	else
		       		printf("\n%-30s\t%-10s\t%-30s",
			      		Truncate("MESSASGE", 30),
					Truncate("ETIME",10),
					Truncate( "CALLS", 30));
		  	while(count < MesgInfo.tot_msgs)
		  	{
		       		if(!tflag)
		       		{
			    		printf("\n%-30s\t%-30s\t%-8d",
				  		Truncate(umesginfo[count].message, 30),
				   		Truncate(umesginfo[count].caller,30),
				   		umesginfo[count].calls);
		       		}
		       		else
		       		{
			    		printf("\n%-30s\t%-10u\t%-8d",
				   		Truncate(umesginfo[count].message,30),
				   		umesginfo[count].etime,
				   		umesginfo[count].calls);
		       		}
		       		count++;
		  	}
		  
	     	}

	}
	
	if(sizeflag)
	{
		if(syssgi(SGI_CELL, SGI_MESG_STATS, 
			SGI_REPORT_MESSAGESIZE_COUNT,
			mesgsizes, &debugflag) == -1)
                {
                        perror("\nCannot count number of messages");
                        exit(1);
                }
		if(debugflag == -1)
		{
			fprintf(stderr, "\n Error : Debugging disabled, Cannot"
				" evaluate message size statistics\n\n");
			exit(1);
	     	}
		tot_size = mesgsizes[0] + mesgsizes[1] +mesgsizes[2]+\
				mesgsizes[3];
		if(!tot_size)
		{
			printf("\n");
			return 0;
		}
		
		mesgsizeinfo = (mesgsizestatsinfo_t *)malloc(\
					sizeof(mesgsizestatsinfo_t )*tot_size);
		if(syssgi(SGI_CELL, SGI_MESG_STATS,SGI_REPORT_MESSAGESIZE_STATS,
				mesgsizes, mesgsizeinfo) == -1)
		{
			perror("\nCannot obtain message size statistics");
			exit(1);
		}
		checkpoint = mesgsizes[Index];
		printf("\n%s\n\n", mesgtypes[Index][0]);
		printf("\n%-15s  %-30s  %-5s  %-5s\n",
			mesgtypes[Index][1], mesgtypes[Index][2]," ", 
			mesgtypes[Index][3]);
		for(count = 0; count < tot_size ; count++)
		{
			ptr = &mesgsizeinfo[count];
			if(count == checkpoint)
			{
				printf("\n\n%s\n", mesgtypes[++Index][0]);
				printf("\n%-15s  %-30s  %-5s  %-5s\n",
		                        mesgtypes[Index][1], mesgtypes[Index][2] 
               			        ," ", mesgtypes[Index][3]);

				checkpoint += mesgsizes[Index];
			}
			if((ptr->size >=0) || ptr->sizehilimit)
			{
				if(ptr->size >= 0)
                                        printf("\n%-15d  ", ptr->size);
                                else
				{	
					sprintf(tempstr, "%-3d - %-3d",
						ptr->sizelolimit,
						ptr->sizehilimit);
                                        printf("\n%-15s  ",
						Truncate(tempstr,15));
				}
				printf("%-30s  ",
					Truncate(ptr->message,30));
				printf("%-5s  ", 
			      		(ptr->replyflag ? "(R)":" "));
				printf("%-5d", ptr->mesgcount);
			
			}
		}

	}
	printf("\n");
	return 0;
	
}


