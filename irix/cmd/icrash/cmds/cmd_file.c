#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_file.c,v 1.39 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * file_cmd() -- Dump out information based on the vfile structures.
 */
int
file_cmd(command_t cmd)
{
	int i, mode = 0, file_cnt = 0;
	kaddr_t val;
	k_ptr_t fp, vfbuf;

#ifdef NOT
	/* Do not allow C_NEXT flag on active systems.
	 */
	if ((active && !DEBUG(DC_GLOBAL, 1)) && (cmd.flags & C_NEXT)) {
		fprintf(cmd.ofp, "Warning: -n option not allowed on active systems\n");
		cmd.flags &= ~C_NEXT;
	}
#endif

	/* Allocate the first vfile buffer.
	 */
	vfbuf = kl_alloc_block(VFILE_SIZE, K_TEMP);
	if (!cmd.nargs && (!(cmd.flags & C_PROC))) {
		if (!(val = K_ACTIVEFILES)) {
			kl_free_block(vfbuf);
			fprintf(cmd.ofp, "Listing of open files only available with "
					"DEBUG kernels.\n");
			return(1);
		}
		if (!(cmd.flags & (C_NEXT|C_FULL))) {
			vfile_banner(cmd.ofp, BANNER|SMAJOR);
		}
		for (; val; val = kl_kaddr(vfbuf, "vfile", "vf_next")) {
			if (cmd.flags & (C_NEXT|C_FULL)) {
				vfile_banner(cmd.ofp, BANNER|SMAJOR);
			}
			if (!(fp = get_vfile(val, vfbuf, cmd.flags))) {
				continue;
			}
			print_vfile(val, fp, cmd.flags, cmd.ofp);
			file_cnt++;
			if (cmd.flags & (C_NEXT|C_FULL)) {
				fprintf(cmd.ofp, "\n");
			}
		}
		vfile_banner(cmd.ofp, SMAJOR);
	} 
	else if (cmd.flags & C_PROC) {
		kl_get_value(pfile, &mode, K_NPROCS, &val);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PROC;
			kl_print_error();
		}
		else {
			file_cnt += list_proc_files(val, mode, cmd.flags, cmd.ofp);
		}
	}
	else {
		if (!(cmd.flags & (C_NEXT|C_FULL))) {
			vfile_banner(cmd.ofp, BANNER|SMAJOR);
		}
		for (i = 0; i < cmd.nargs; i++) {

			if (cmd.flags & (C_NEXT|C_FULL)) {
				vfile_banner(cmd.ofp, BANNER|SMAJOR);
			}

			kl_get_value(cmd.args[i], &mode, K_NPROCS, &val);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VFILE;
				kl_print_error();
				continue;
			}

			fp = get_vfile(val, vfbuf, (cmd.flags|C_ALL));
			if (KL_ERROR) {
				kl_print_error();
			}
			else {
				print_vfile(val, fp, cmd.flags, cmd.ofp);
				file_cnt++;
			}

			if (cmd.flags & (C_NEXT|C_FULL)) {
				fprintf(cmd.ofp, "\n");
			}
		}
		vfile_banner(cmd.ofp, SMAJOR);
	}
	PLURAL("vfile struct", file_cnt, cmd.ofp);
	kl_free_block(vfbuf);
	return(0);
}

#define _FILE_USAGE "[-a] [-f] [-n] [-p slot_number] [-w outfile] vfile_list"

/*
 * file_usage() -- Print the usage string for the 'file' command.
 */
void
file_usage(command_t cmd)
{
	CMD_USAGE(cmd, _FILE_USAGE);
}

/*
 * file_help() -- Print the help information for the 'file' command.
 */
void
file_help(command_t cmd)
{
	CMD_HELP(cmd, _FILE_USAGE,
		"Display the vfile structure located at each virtual address "
		"included in vfile_list.  If no addresses are specified, display "
		"the entire kernel file table (only available with DEBUG kernels).  "
		"If the -p option is used, display all files opened by proc.  "
		"Proc can be specific as a virtual address or as a process PID "
		"(following a '#').");
}

/*
 * file_parse() -- Parse the command line arguments for 'file'.
 */
int
file_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT|C_ALL|C_PROC);
}
