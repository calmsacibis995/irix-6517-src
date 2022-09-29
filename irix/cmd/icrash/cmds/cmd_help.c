#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_help.c,v 1.9 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

extern struct _commands cmdset[];
/*
 * help_list() -- Print out the list of possible commands that have 'help'.
 *                This prints out duplicates as well (no little alias trick).
 */
int
help_list(command_t cmd)
{
#if 0
	int i;
	static int count = 0;

	/* set up the pointer list */
	if (!count) {
		while (cmdset[count].cmd != (char *)0) {
			count++;
		}
	}

	fprintf(cmd.ofp, "COMMAND LIST:\n\n");
	for (i = 1; i <= count; i++) {
		fprintf(cmd.ofp, "%17-s", cmdset[i-1].cmd);

		if (!(i % 4)) {
			fprintf(cmd.ofp, "\n");
		}
	}
	if (i % 4) {
		fprintf(cmd.ofp, "\n");
	}
#else
	int i, j, index;
	static int count = 0;

	if (!count) {
		while (cmdset[count].cmd != (char *)0) {
			count++;
		}
	}

	index = count / 4 + ( count % 4 ? 1 : 0);
	for (i = 0; i < index; i++) {
		fprintf(cmd.ofp, "%-17s", cmdset[i].cmd);
		if ((j = index + i) < count) {
			fprintf(cmd.ofp, "%-17s", cmdset[j].cmd);
		}
		if ((j = index * 2 + i) < count) {
			fprintf(cmd.ofp, "%-17s", cmdset[j].cmd);
		}
		if ((j = index * 3 + i) < count) {
			fprintf(cmd.ofp, "%-17s", cmdset[j].cmd);
		}
		fprintf(cmd.ofp, "\n");
	}
#endif
	return(0);
}

/*
 * help_all_list() -- Print out help on every command for 'help all'.
 */
void
help_all_list(command_t cmd)
{
	int i = 0;

	while (cmdset[i].cmd != (char *)0) {
		if (cmdset[i].alias == (char *)0) {
			sprintf(cmd.com, "%s", cmdset[i].cmd);
			(*cmdset[i].cmdhelp)(cmd);
		}
		i++;
	}
}

/*
 * help_found() -- Find the index of the help item we are looking for.
 */
int
help_found(char *helpcmd)
{
	char *real_cmd;
	int i = 0;

	/* Find the command ... We might have to do a double-pass.  
	 */
	real_cmd = helpcmd;
	while ((int)cmdset[i].cmd != 0) {
		if (!strcmp(real_cmd, cmdset[i].cmd)) {

			/* We found the command ... Now see if this is an alias. 
			 */
			if (cmdset[i].alias != 0) {
				real_cmd = cmdset[i].alias;
				i = 0;
			} 
			else {
				return (i);
			}
		} 
		else {
			i++;
		}
	}
	return (-1);
}

/*
 * help_cmd() -- Run the 'help' command.
 */
int
help_cmd(command_t cmd)
{
	int i, j;

	/* First off, grab the command passed in.
	 */
	if (cmd.nargs < 1) {
		/* See if we didn't specify anything to get help on.
		 * If we didn't, just dump out all commands.
		 */
		help_list(cmd);
	} 
	else {
		/* Print all help information out if 'all' is the first argument. 
		 */
		if (!strcmp(cmd.args[0], "all")) {
			help_all_list(cmd);
		} 
		else {
			for (j = 0; j < cmd.nargs; j++) {
				if ((i = help_found(cmd.args[j])) < 0) {
					fprintf(cmd.ofp, "\nNo help exists on \"%s\".\n",
						cmd.args[j]);
				} 
				else {
					sprintf(cmd.com, "%s", cmdset[i].cmd);
					(*cmdset[i].cmdhelp)(cmd);
				}
			}
		}
	}
	return(0);
}

#define _HELP_USAGE "[-w outfile] [all | command_list]"
/*
 * help_usage() -- Print the usage string for the 'help' command.
 */
void
help_usage(command_t cmd)
{
	CMD_USAGE(cmd, _HELP_USAGE);
}

/*
 * help_help() -- Print the help information for the 'help' command.
 */
void
help_help(command_t cmd)
{
	CMD_HELP(cmd,_HELP_USAGE,
		"Display a description of the named functions, including "
		"syntax.  The 'all' option displays help information for every "
		"command.");
}

/*
 * help_parse() -- Parse the command line arguments for 'help'.
 */
int
help_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE);
}
