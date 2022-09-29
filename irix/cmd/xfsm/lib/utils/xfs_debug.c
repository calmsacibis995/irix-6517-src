#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "xfs_utils.h"

FILE *debug_fp=NULL;

void
open_debug_file(const char* filename)
{
	if (filename != NULL)
	{
		debug_fp = fopen(filename,"w");
	}
}


void
dump_to_debug_file(const char* buffer)
{
	time_t t;
	char str[BUFSIZ];
	
	if (debug_fp != NULL)
	{
		/* Get the current time */
		t = time(NULL);
		
		ascftime(str,"%D %T",localtime(&t));
		fprintf(debug_fp,"%s : %s\n",str,buffer);
		fflush(debug_fp);
	}
}

void
close_debug_file(void)
{
	if (debug_fp != NULL)
	{
		fclose(debug_fp);
	}
}


