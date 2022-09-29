#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_type.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <libelf.h>
#include <dwarf.h>
#include <libdwarf.h>
#include <syms.h>
#include <sym.h>

#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "dwarflib.h"

void
walk_type_tree(dw_type_t *dtp, FILE *ofp)
{
	if (!dtp) {
		return;
	}
	fprintf(ofp, "0x%x  %2d - (%d) %s\n", 
		dtp, dtp->dt_depth, dtp->dt_nmlist, dtp->dt_name);
	walk_type_tree((dw_type_t*)dtp->dt_left, ofp);
	walk_type_tree((dw_type_t*)dtp->dt_right, ofp);
}

/*
 * type_cmd() -- Run the 'type' command.
 */
int
type_cmd(command_t cmd)
{
	int i, nodes;
	dw_type_t *dtp;

	if (cmd.nargs) {
		for (i = 0; i < cmd.nargs; i++) {
			nodes = 0;
			if (dtp = dw_lkup(stp, cmd.args[i], 0)) {
				fprintf(cmd.ofp, "Found %s in type tree!\n", cmd.args[i]);
				if (cmd.flags & C_FULL) {
					fprintf(cmd.ofp, "  ADDR     = 0x%x\n", dtp);
					fprintf(cmd.ofp, "  left     = 0x%x\n", dtp->dt_left);
					fprintf(cmd.ofp, "  right    = 0x%x\n", dtp->dt_right);
					fprintf(cmd.ofp, "  name     = %s\n", dtp->dt_name);
					fprintf(cmd.ofp, "  depth    = %d\n", dtp->dt_depth);
					fprintf(cmd.ofp, "  type     = %d\n", dtp->dt_type);
					fprintf(cmd.ofp, "  size     = %d\n", dtp->dt_size);
					fprintf(cmd.ofp, "  offset   = %d\n", dtp->dt_offset);
					fprintf(cmd.ofp, "  member   = 0x%x\n", dtp->dt_member);
					fprintf(cmd.ofp, "  next     = 0x%x\n", dtp->dt_next);
					fprintf(cmd.ofp, "  state    = %d\n", dtp->dt_state);
					fprintf(cmd.ofp, "  tag      = %d\n", dtp->dt_tag);
					fprintf(cmd.ofp, "  off      = %lld\n", dtp->dt_off);
					fprintf(cmd.ofp, "  type_off = %lld\n", dtp->dt_type_off);
					fprintf(cmd.ofp, "  nmlist   = %d\n", dtp->dt_nmlist);
				}
			}
			else {
				fprintf(cmd.ofp, "Didn't find %s in type tree!\n", cmd.args[i]);
			}
		}
	}
	else if (cmd.flags & C_ALL) {
		/* Print out existing nodes
		 */
		walk_type_tree((dw_type_t *)stp->st_type_ptr, cmd.ofp);
	}
	return(0);
}

#define _TYPE_USAGE "[-a] [-w outfile] type_name(s)"

/*
 * type_usage() -- Print the usage string for the 'type' command.
 */
void
type_usage(command_t cmd)
{
	CMD_USAGE(cmd, _TYPE_USAGE);
}

/*
 * type_help() -- Print the help information for the 'type' command.
 */
void
type_help(command_t cmd)
{
	CMD_HELP(cmd, _TYPE_USAGE,
		"Display type information from entries in the type tree.");
}

/*
 * type_parse() -- Parse the command line arguments for 'type'.
 */
int
type_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_ALL|C_FULL);
}
