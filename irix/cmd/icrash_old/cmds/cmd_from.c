#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_from.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "icrash.h"
#include "extern.h"

/*
 * from_cmd() -- Process a set of commands from a specified file.
 */
int
from_cmd(command_t cmd)
{
	FILE *ifp;
	command_t cb;
	char *buf;

	/* Take the first argument, assume it is the file, and try to
	 * open it up.
	 */
	if (((cmd.fp != cmd.ofp) || (cmd.flags & C_PIPE)) && (!fromflag)) {
		fprintf(cmd.ofp,
			"Error: -w option and | option invalid when using 'from'.\n");
		return -1;
	}
	if ((ifp = fopen(cmd.args[0], "r")) == (FILE *)NULL) {
		perror("dofrom: cannot open command file");
		return -1;
	}

	buf = (char *)alloc_block(BUFSIZ, B_TEMP);
	while (fgets(buf, BUFSIZ-1, ifp) != (char *)NULL) {
		buf[strlen(buf)-1] = '\0';
		fprintf(cmd.ofp, "\n>> %s\n", buf);
		get_cmd(&cb, buf);
		if (checkrun_cmd(cb, cmd.ofp) < 0) {
			fprintf(cmd.ofp, "unknown command\n");
		}
		fflush(cmd.ofp);
	}
	free_block((k_ptr_t)buf);
	fclose(ifp);
	return(0);
}

#define _FROM_USAGE "[-w outfile] from_file"
/*
 * from_usage() -- Print the usage string for the 'from' command.
 */
void
from_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FROM_USAGE);
}

/*
 * from_help() -- Print the help information for the 'from' command.
 */
void
from_help(command_t cmd)
{
	CMD_HELP(cmd, _FROM_USAGE,
		"Allows commands to be read in out of from_file.");
}

/*
 * from_parse() -- Parse the command line arguments for 'from'.
 */
int
from_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
