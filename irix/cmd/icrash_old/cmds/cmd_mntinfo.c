#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_mntinfo.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * mntinfo_cmd() -- Dump out data related to mntinfo.
 */
int
mntinfo_cmd(command_t cmd)
{
	int i, mntinfo_cnt = 0;
	kaddr_t value;
	k_ptr_t rp, rbuf;
	int first_time = TRUE;

	rbuf = alloc_block(STRUCT("mntinfo"), B_TEMP);
	mntinfo_banner(cmd.ofp, BANNER|SMAJOR);

	for (i = 0; i < cmd.nargs; i++) {
		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_MNTINFO;
			kl_print_error(K);
			continue;
		}
		rp = get_mntinfo(value, rbuf, (cmd.flags|C_ALL));
		if (KL_ERROR) {
			kl_print_error(K);
		}
		else {
			if (first_time) {
				first_time = FALSE;
			}
			else if (cmd.flags & (C_FULL|C_NEXT)) {
				mntinfo_banner(cmd.ofp, BANNER|SMAJOR);
			}
			mntinfo_cnt = print_mntinfo(value, rp, cmd.flags, cmd.ofp);
		}
		if (cmd.flags & C_FULL) {
			fprintf(cmd.ofp, "\n");
		}
	}
	mntinfo_banner(cmd.ofp, SMAJOR);
	PLURAL("mntinfo struct", mntinfo_cnt, cmd.ofp);
	free_block(rbuf);
	return(0);
}

#define _MNTINFO_USAGE "[-f] [-n] [-w outfile] mntinfo_list"

/*
 * mntinfo_usage() -- Print the usage string for the 'mntinfo' command.
 */
void
mntinfo_usage(command_t cmd)
{
	CMD_USAGE(cmd, _MNTINFO_USAGE);
}

/*
 * mntinfo_help() -- Print the help information for the 'mntinfo' command.
 */
void
mntinfo_help(command_t cmd)
{
	CMD_HELP(cmd, _MNTINFO_USAGE,
		"Display the mntinfo structure for each virtual address included "
		"in mntinfo_list.  The mntinfo contains information about NFS "
		"mounted file systems.");
}

/*
 * mntinfo_parse() -- Parse the command line arguments for 'mntinfo'.
 */
int
mntinfo_parse(command_t cmd)
{
	return (C_WRITE|C_FULL|C_ALL|C_NEXT|C_TRUE);
}
