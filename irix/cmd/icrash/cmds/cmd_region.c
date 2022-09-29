#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_region.c,v 1.28 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * region_cmd() -- Set up to dump out region data.
 */
int
region_cmd(command_t cmd)
{
	int i, firsttime = 1, mode, reg_cnt = 0;
	kaddr_t value, npr, nr, proc;
	k_ptr_t procp, rp, avl, prp;

	if (cmd.flags & C_PROC) {
		kl_get_value(pfile, &mode, K_NPROCS, &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_PROC;
			kl_print_error();
			return(1);
		}
		procp = kl_get_proc(value, mode, cmd.flags);
		if (KL_ERROR) {
			kl_print_error();
			return(1);
		}
		if (mode == 1) {
			proc = kl_pid_to_proc(value);
		}
		else {
			proc = value;
		}

		/* Point avl to the offset within proc buffer where
		 * p_region resides. Then get the pointer to the first
		 * pregion.
		 */
		avl = (k_ptr_t)((unsigned)procp + kl_member_offset("proc", "p_region"));
		kl_free_block(procp);
		if (npr = kl_kaddr(avl, "avltree_desc", "avl_firstino")) {
			fprintf(cmd.ofp, "REGIONS FOR PROC 0x%llx:\n\n", proc);
			region_banner(cmd.ofp, BANNER|SMAJOR);
		}
		else {
			fprintf(cmd.ofp, "NO PROCESS REGIONS FOR PROC 0x%llx.\n", proc);
			return(1);
		}

		prp = kl_alloc_block(PREGION_SIZE, K_TEMP);
		rp = kl_alloc_block(REGION_SIZE, K_TEMP);
		while (npr) {
			kl_get_struct(npr, PREGION_SIZE, prp, "pregion");
			if (KL_ERROR) {
				fprintf(cmd.ofp, "Could not read pregion at 0x%llx\n", npr);
				break;
			}

			/* Get the region pointer
			 */
			if (!(nr = kl_kaddr(prp, "pregion", "p_reg"))) {
				break;
			}
			kl_get_struct(nr, REGION_SIZE, rp, "region");
			if (KL_ERROR) {
				fprintf(cmd.ofp, "Could not read region at 0x%llx\n", value);
				npr = kl_kaddr(prp, "avlnode", "avl_nextino");
				continue;
			}
			else {
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
			npr = kl_kaddr(prp, "avlnode", "avl_nextino");
		}
		kl_free_block(rp);
		kl_free_block(prp);
	} 
	else if (cmd.nargs == 0) {
		region_usage(cmd);
		return(1);
	}
	else {
		region_banner(cmd.ofp, BANNER|SMAJOR);
		rp = kl_alloc_block(REGION_SIZE, K_TEMP);
		for (i = 0; i < cmd.nargs; i++) {
			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_REGION;
				kl_print_error();
				continue;
			}
			kl_get_struct(value, REGION_SIZE, rp, "region");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_REGION;
				kl_print_error();
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
		kl_free_block(rp);
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
