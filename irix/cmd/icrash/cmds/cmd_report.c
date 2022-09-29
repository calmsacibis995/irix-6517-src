#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_report.c,v 1.12 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <klib/klib.h>
#include "icrash.h"

extern jmp_buf klib_jbuf;

/*
 * report_cmd() -- Dump out a report of commands.
 */
int
report_cmd(command_t cmd)
{
	/* The 'p' buffer really only needs to be 256 characters, but
	 * I've padded it for sprintf() additions.  BFD.
	 */
	char p[300];

	/* Make sure this isn't a live system. If it is, print an error
	 * message and return.
	 */
	if (ACTIVE && !fromflag) {
		fprintf(cmd.ofp, "Not able to generate a report on a live system\n");
		return(1);
	}

	/* If the report was initiated from the command line, then
	 * do the report and return. Interactive reports generated 
	 * from a "fromfile" will not come through this command (they 
	 * will be initiated by the 'from' comamnd).
	 */
	if (!report_flag && !fromflag) {
		base_report(cmd, p, cmd.ofp);
		return(0);
	}

	/* If there is a fromfile containing icrash commands, issue the 
	 * commands instead of the standard report.
	 */
	if (fromflag) {
		sprintf(p, "from %s", fromfile);
		get_cmd(&cmd, p);
		checkrun_cmd(cmd, cmd.ofp);
		return(0);
	}

	if (setjmp(klib_jbuf)) {
		return(1);
	}

	/* If we happen to be able to grab availability information,
	 * dump it out FIRST.
	 */
	availmon_report();

	/* Now handle the base reporting information, which is the
	 * general analysis data.
	 */
	base_report(cmd, p, cmd.ofp);

	/* Try to clear the dump, if it's necessary.
	 */
	if (cleardump) {
		clear_dump();
	}
	return(0);
}

#define _REPORT_USAGE "[-w outfile]"

/*
 * report_usage() -- Print the usage string for the 'report' command.
 */
void
report_usage(command_t cmd)
{
	CMD_USAGE(cmd, _REPORT_USAGE);
}

/*
 * report_help() -- Print the help information for the 'report' command.
 */
void
report_help(command_t cmd)
{
	CMD_HELP(cmd, _REPORT_USAGE,
		"Print out the base report that is seen when run against a core "
		"dump after a system crash.");
}

/*
 * report_parse() -- Parse the command line arguments for 'report'.
 */
int
report_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
