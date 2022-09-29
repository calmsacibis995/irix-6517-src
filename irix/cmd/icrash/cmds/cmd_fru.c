#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_fru.c,v 1.6 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * fru_cmd() -- Perform the 'fru' command for Origin and Challenge systems
 */
int
fru_cmd(command_t cmd)
{
	int found = FALSE, pbufsize;
	char *nbuf, *pbuf, *sp, *stop;
	struct syment *putbuf_cpusz;
	int psize;

	/* this version only works on IP{19,21,25,27} systems 
	 */
	if ((K_IP != 19) && (K_IP != 21) && (K_IP != 25) && (K_IP != 27)) {
		if (!execute_flag) {
			fprintf(cmd.ofp,
				"Error: fru can only be run on IP19, IP21, "
				"IP25 and IP27 systems\n");
		}
		return (1);
	}
   
	if (!(putbuf_cpusz = kl_get_sym("putbuf_cpusz", K_TEMP))) {
		pbufsize = 1000;
	} 
	else if (!kl_get_block(putbuf_cpusz->n_value, 4, &psize, "putbuf_cpusz")) {
		pbufsize = K_MAXCPUS * 0x800;
	} 
	else {
		pbufsize = K_MAXCPUS * psize;
	}

	/* allocate a buffer to grab the putbuf, and one to store the fru 
	 */
	nbuf = (char *)kl_alloc_block(pbufsize + 500, K_TEMP);
	pbuf = (char *)kl_alloc_block(pbufsize, K_TEMP);
	kl_get_block(K_PUTBUF, pbufsize, pbuf, "putbuf");

	/* scan through the buffer looking for our initialization string 
	 */
	sp = strtok(pbuf, "\n");
	while (sp) {
		if (strstr(sp, "FRU ANALYSIS END")) {
			/* if we reach the end, save string and terminate 
			 */
			found = FALSE;
			strcat(nbuf, "\t");
			strcat(nbuf, sp);
			sp = NULL;
		} 
		else if ((found) || (strstr(sp, "FRU ANALYSIS BEGIN"))) {
			/* if we find the beginning or have a string, save it 
			 */
			found = TRUE;
			strcat(nbuf, "\t");
			strcat(nbuf, sp);
			strcat(nbuf, "\n");
		}
		sp = strtok(NULL, "\n");
	}

	/* make sure we didn't flow into the top of the putbuf 
	 */
	if (found) {
		sp = strtok(pbuf, "\n");
		while (sp) {
			if (strstr(sp, "FRU ANALYSIS END")) {
				/* if we get this, we've reached the end on the overflow 
				 */
				strcat(nbuf, "\t");
				strcat(nbuf, sp);
				sp = NULL;
			} 
			else if (strstr(sp, "FRU ANALYSIS BEGIN")) {
				/* there was no end to the fru analysis! error ... 
				 */
				fprintf(cmd.ofp,
					"Error: fru data in putbuf looks wrong.  Run 'stat'.\n");
				bzero(nbuf, pbufsize + 500);
				sp = NULL;
			} 
			else {
				strcat(nbuf, "\t");
				strcat(nbuf, sp);
				strcat(nbuf, "\n");
				sp = strtok(NULL, "\n");
			}
		}
	}

	/* print out any results 
	 */
	if (nbuf[0] != '\0') {
		fprintf(cmd.ofp, "%s\n", nbuf);
	} 
	else {
		fprintf(cmd.ofp, "fru: no error state found.\n");
	}

	/* free up our buffers 
	 */
	kl_free_block((k_ptr_t)nbuf);
	kl_free_block((k_ptr_t)pbuf);
	return(0);
}

#define _FRU_USAGE "[-w outfile]"

/*
 * fru_usage() -- Print the usage string for the 'fru' command.
 */
void
fru_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FRU_USAGE);
}

/*
 * fru_help() -- Print the help information for the 'fru' command.
 */
void
fru_help(command_t cmd)
{
	CMD_HELP(cmd, _FRU_USAGE,
		"Display the fru output for Origin and Challenge systems.  This "
		"includes all IP19, IP21, IP25 and IP27 type systems.");
}

/*
 * fru_parse() -- Parse the command line arguments for 'fru'.
 */
int
fru_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
