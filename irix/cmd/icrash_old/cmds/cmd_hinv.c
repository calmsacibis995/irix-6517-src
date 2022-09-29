#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_hinv.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * hinv_cmd() -- Run the 'hinv' command.  
 */
int
hinv_cmd(command_t cmd)
{
	fprintf(cmd.ofp, 
		"\tFull hinv command functionality is currently not available from \n"
		"\ticrash. The icrash config and memory commands do provide some \n"
		"\thardware configuration information.\n");
	return(0);
}

#define _HINV_USAGE "[-f] [-w outfile]"

/*
 * hinv_usage() -- Print the usage string for the 'hinv' command.
 */
void
hinv_usage(command_t cmd)
{
    CMD_USAGE(cmd, _HINV_USAGE);
}

/*
 * hinv_help() -- Print the help information for the 'hinv' command.
 */
void
hinv_help(command_t cmd)
{
    CMD_HELP(cmd, _HINV_USAGE,
        "Display the hardware inventory information in a system.  This will "
        "report a verbose version of the hardware on the system.");
}

/*
 * hinv_parse() -- Parse the command line arguments for 'hinv'.
 */
int
hinv_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE|C_FULL);
}
