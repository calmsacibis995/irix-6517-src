#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_file.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * file_cmd() -- Dump out information based on the file structures.
 */
int
file_cmd(command_t cmd)
{
	int i, mode = 0, file_cnt = 0;
	kaddr_t val;
	k_ptr_t fp, fbuf;

#ifdef NOT
	/* Do not allow C_NEXT flag on active systems.
	 */
	if ((active && !DEBUG(DC_GLOBAL, 1)) && (cmd.flags & C_NEXT)) {
		fprintf(cmd.ofp, "Warning: -n option not allowed on active systems\n");
		cmd.flags &= ~C_NEXT;
	}
#endif

	/* Allocate the first file buffer.
	 */
	fbuf = alloc_block(VFILE_SIZE(K), B_TEMP);
	if (!cmd.nargs && (!(cmd.flags & C_PROC))) {
		if (!(val = K_ACTIVEFILES(K))) {
			free_block(fbuf);
			fprintf(cmd.ofp, "Listing of open files only available with "
					"DEBUG kernels.\n");
			return(1);
		}
		if (!(cmd.flags & (C_NEXT|C_FULL))) {
			vfile_banner(cmd.ofp, BANNER|SMAJOR);
		}
		for (; val; val = kl_kaddr(K, fbuf, "vfile", "vf_next")) {
			if (cmd.flags & (C_NEXT|C_FULL)) {
				vfile_banner(cmd.ofp, BANNER|SMAJOR);
			}
			if (!(fp = get_vfile(val, fbuf, cmd.flags))) {
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
		get_value(pfile, &mode, K_NPROCS(K), &val);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PROC;
			kl_print_error(K);
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

			get_value(cmd.args[i], &mode, K_NPROCS(K), &val);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_VFILE;
				kl_print_error(K);
				continue;
			}

			fp = get_vfile(val, fbuf, (cmd.flags|C_ALL));
			if (KL_ERROR) {
				kl_print_error(K);
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
	free_block(fbuf);
	return(0);
}

#define _FILE_USAGE "[-a] [-f] [-n] [-p slot_number] [-w outfile]"

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
		"Display the file structure located at each virtual address "
		"included in file_list.  If no addresses are specified, display "
		"the entire kernel file table.  If the -p option is used, "
		"display all files opened by proc.  Proc can be specific as a "
		"virtual address or as a process PID (following a '#').");
}

/*
 * file_parse() -- Parse the command line arguments for 'file'.
 */
int
file_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT|C_ALL|C_PROC);
}
