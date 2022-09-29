#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_from.c,v 1.8 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

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
		return(1);
	}
	if ((ifp = fopen(cmd.args[0], "r")) == (FILE *)NULL) {
		perror("dofrom: cannot open command file");
		return(1);
	}

	/* Turn off the fromflag just in case one of the commands in the
	 * from file is the report command.
	 */
	fromflag = 0;
	buf = (char *)kl_alloc_block(BUFSIZ, K_TEMP);
	while (fgets(buf, BUFSIZ-1, ifp) != (char *)NULL) {
		buf[strlen(buf)-1] = '\0';
		fprintf(cmd.ofp, "\n>> %s\n", buf);
		if (strncmp(buf, "from", 4) == 0) {
			fprintf(cmd.ofp, "Error: The from command cannot make a call to "
				"the from command.\n");
			continue;
		}
		get_cmd(&cb, buf);
		if (checkrun_cmd(cb, cmd.ofp) < 0) {
			fprintf(cmd.ofp, "unknown command\n");
		}
		fflush(cmd.ofp);
	}
	kl_free_block((k_ptr_t)buf);
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
