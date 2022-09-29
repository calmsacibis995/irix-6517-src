#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_fstype.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fsid.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * fstype_cmd() -- Print out the VFS types on the system.
 */
int
fstype_cmd(command_t cmd)
{
	int i, mode, vfssw_cnt = 0;
	k_uint_t ix = 0;

	vfssw_banner(cmd.ofp, BANNER|SMAJOR);
	if (!cmd.nargs) {
		for (i = 0; i < nfstype; i++) {
			vfssw_cnt += print_vfssw(i, cmd.flags, cmd.ofp);
		}
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {
			get_value(cmd.args[i], &mode, nfstype, &ix);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VFSSW;
				kl_print_error(K);
			}
			else if (ix >= nfstype) {
				KL_SET_ERROR_NVAL(KLE_BAD_VFSSW, ix, mode);
				kl_print_error(K);
			}
			else {
				if (print_vfssw((int)ix, cmd.flags, cmd.ofp)) {
					vfssw_cnt++;
				}
				else {
					KL_SET_ERROR_NVAL(KLE_BAD_VFSSW, ix, mode);
					kl_print_error(K);
				}
			}
		}
	}
	vfssw_banner(cmd.ofp, SMAJOR);
	PLURAL("vfs struct", vfssw_cnt, cmd.ofp);
	return(0);
}

#define _FSTYPE_USAGE "[-w outfile] [vfssw_addr]"

/*
 * fstype_usage() -- Print the usage string for the 'fstype' command.
 */
void
fstype_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FSTYPE_USAGE);
}

/*
 * fstype_help() -- Print the help information for the 'fstype' command.
 */
void
fstype_help(command_t cmd)
{
	CMD_HELP(cmd, _FSTYPE_USAGE,
		"Display the vfssw structure for each virtual address included "
		"in vfssw_addr.  If no vfssw structures are specified, display "
		"the entire vfssw table.");
}

/*
 * fstype_parse() -- Parse the command line arguments for 'fstype'.
 */
int
fstype_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE);
}
