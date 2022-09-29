#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_region.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * region_cmd() -- Set up to dump out region data.
 */
int
region_cmd(command_t cmd)
{
	int i, firsttime = 1, mode, reg_cnt = 0;
	kaddr_t value, npr, nr, proc;
	k_ptr_t procp, rp, avl, rbuf, prp, prbuf;

	if (cmd.flags & C_PROC) {
		get_value(pfile, &mode, K_NPROCS(K), &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PROC;
			kl_print_error(K);
			return(1);
		}
		procp = kl_get_proc(K, value, mode, cmd.flags);
		if (KL_ERROR) {
			kl_print_error(K);
			return(1);
		}
		if (mode == 1) {
			proc = kl_pid_to_proc(K, value);
		}
		else {
			proc = value;
		}

		/* Point avl to the offset within proc buffer where
		 * p_region resides. Then get the pointer to the first
		 * pregion.
		 */
		avl = (k_ptr_t)((unsigned)procp + FIELD("proc", "p_region"));
		free_block(procp);
		if (npr = kl_kaddr(K, avl, "avltree_desc", "avl_firstino")) {
			fprintf(cmd.ofp, "REGIONS FOR PROC 0x%llx:\n\n", proc);
			region_banner(cmd.ofp, BANNER|SMAJOR);
		}
		else {
			fprintf(cmd.ofp, "NO PROCESS REGIONS FOR PROC 0x%llx.\n", proc);
			return(1);
		}

		prbuf = alloc_block(PREGION_SIZE(K), B_TEMP);
		rbuf = alloc_block(REGION_SIZE(K), B_TEMP);
		while (npr) {
			prp = kl_get_struct(K, npr, PREGION_SIZE(K), prbuf, "pregion");
			if (!prp) {
				fprintf(cmd.ofp, "Could not read pregion at 0x%llx\n", npr);
				break;
			}

			/* Get the region pointer
			 */
			if (!(nr = kl_kaddr(K, prp, "pregion", "p_reg"))) {
				break;
			}
			if (rp = kl_get_struct(K, nr, REGION_SIZE(K), rbuf, "region")) {
				if (DEBUG(DC_GLOBAL, 1) ||
					(cmd.flags & (C_FULL|C_NEXT))) {
						if (!firsttime) {
							region_banner(cmd.ofp, BANNER|SMAJOR);
						} 
						else {
							firsttime = 0;
						}
				}
				print_region(nr, rp, cmd.flags, cmd.ofp);
				if (cmd.flags & C_NEXT) {
					list_region_pfdats(rp, cmd.flags, cmd.ofp);
					fprintf (cmd.ofp, "\n");
				}
				reg_cnt++;
			} 
			else {
				fprintf(cmd.ofp, "Could not read region at 0x%llx\n", value);
				npr = kl_kaddr(K, prp, "avlnode", "avl_nextino");
				continue;
			}
			npr = kl_kaddr(K, prp, "avlnode", "avl_nextino");
		}
		free_block(rbuf);
		free_block(prbuf);
	} 
	else if (cmd.nargs == 0) {
		region_usage(cmd);
		return(1);
	}
	else {
		region_banner(cmd.ofp, BANNER|SMAJOR);
		rbuf = alloc_block(REGION_SIZE(K), B_TEMP);
		for (i = 0; i < cmd.nargs; i++) {
			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_REGION;
				kl_print_error(K);
				continue;
			}
			rp = kl_get_struct(K, value, REGION_SIZE(K), rbuf, "region");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_REGION;
				kl_print_error(K);
				continue;
			}
			else {
				if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & (C_FULL|C_NEXT))) {
					if (!firsttime) {
						region_banner(cmd.ofp, BANNER|SMAJOR);
					} 
					else {
						firsttime = 0;
					}
				}
				print_region(value, rp, cmd.flags, cmd.ofp);
				if (cmd.flags & C_NEXT) {
					list_region_pfdats(rp, cmd.flags, cmd.ofp);
					fprintf (cmd.ofp, "\n");
				}
				reg_cnt++;
			} 
		}
	}
	region_banner(cmd.ofp, SMAJOR);
	PLURAL("region", reg_cnt, cmd.ofp);
	return(0);
}

#define _REGION_USAGE "[-f] [-n] [-p proc] [-w outfile] region_list"

/*
 * region_usage() -- Print the usage string for the 'region' command.
 */
void
region_usage(command_t cmd)
{
	CMD_USAGE(cmd, _REGION_USAGE);
}

/*
 * region_help() -- Print the help information for the 'region' command.
 */
void
region_help(command_t cmd)
{
	CMD_HELP(cmd, _REGION_USAGE,
		"Display the region structure located at each virtual address "
		"included in region_list.  If the -p option is used, display all "
		"regions allocated to proc.  Proc can be specific as a process "
		"PID (following a '#'), or as a virtual address.");
}

/*
 * region_parse() -- Parse the command line arguments for 'region'.
 */
int
region_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT|C_PROC);
}
