/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)nice:nice.c	1.5" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/nice/RCS/nice.c,v 1.12 1996/10/13 05:15:07 kostadis Exp $"
/*
**	nice
*/


#include	<stdio.h>
#include	<ctype.h>
#include	<sys/errno.h>
#include 	<stdio.h>
static char* usage = "nice: usage: nice [-n num | -num] command\n";

static void error(char* cmd) 
{
	fprintf(stderr, cmd);
	exit(2);
}

static int  number(char* num) 
{	
	int value; 	
	int num_chars;
	if (!num)
		error(usage);
	sscanf(num," %d ", &value);
	num_chars = strlen(num);

	for(num_chars = 0; num_chars < strlen(num); num_chars++)
		if (!isdigit(num[num_chars]))
			error(usage);

	return value;
}
	

int main(int argc, char** argv)
{
	int	nicarg = 10;
	int expnum = 0;
	int err;
	int command = 1;
	extern	errno;
	extern	char *sys_errlist[];
	if (argc < 2) 
		error(usage); 

	if (argv[1][0] == '-') {  /* Case -n increment or -- incrememnt */
		command++;
		if (argv[1][1] == 'n') {
			command++;
			if (argc < 4) 
				error(usage);	
			if (argv[2][0] == '-') 
				nicarg = -number(&argv[2][1]);
			else	
				nicarg = number(argv[2]);
				 
		} else if (argv[1][1] == '-')
			nicarg = -number(&argv[1][2]);
		else 
			nicarg = number(&argv[1][1]);

	}
	if (command >= argc)
		error(usage);
	errno = 0;

	if (nice(nicarg) == -1) {
		/*
		 * Could be an error or a legitimate return value.
		 * The only error we care about is EINVAL, which will
		 * be returned if we are not in the time sharing
		 * scheduling class.  For any error other than EINVAL
		 * we will go ahead and exec the command even though
		 * the priority change failed.
		 */
		if (errno == EINVAL) {
			fprintf(stderr, "nice: invalid operation; \
not a time sharing process\n");
			exit(2);
		}
	}
	execvp(argv[command], &argv[command]);
	err = errno;
	fprintf(stderr, "nice: %s: %s\n", sys_errlist[errno], argv[command]);
	exit(err == ENOENT ? 127 : 126);
}

