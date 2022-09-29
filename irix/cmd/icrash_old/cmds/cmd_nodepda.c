#ident "$Header: "
#include <stdio.h>
#include <sys/types.h>
#include "icrash.h"
#include "extern.h"

/*
 * nodepda_cmd() -- Dump out nodepda_s structures in the kernel 
 */
int
nodepda_cmd(command_t cmd)
{
	int i, id, mode, npda_cnt = 0, first_time = TRUE;
	k_ptr_t npdap;
	kaddr_t value, nodepdaval;

	if (!(cmd.flags & C_FULL)) {
		nodepda_s_banner(cmd.ofp, BANNER|SMAJOR);
	}
	npdap = alloc_block(NODEPDA_S_SIZE(K), B_TEMP);
	if (!cmd.nargs) {
		for (i = 0; i < K_NUMNODES(K); i++) {
			if (cmd.flags & C_FULL) {
				nodepda_s_banner(cmd.ofp, BANNER|SMAJOR);
			}
			value = (K_NODEPDAINDR(K) + (i * K_NBPW(K)));
			kl_get_kaddr(K, value, &nodepdaval, "nodepda_s");
			if (KL_ERROR) {
				continue;
			}

			kl_get_struct(K, nodepdaval, NODEPDA_S_SIZE(K), npdap, "nodepda_s");
			if (KL_ERROR) {
				KL_SET_ERROR_NVAL(KLE_BAD_NODEPDA, value, mode);
				continue;
			}

			/* XXX -- This needs to be set equal to the correct id
			 */
			id = 0;

			print_nodepda_s(nodepdaval, npdap, cmd.flags, cmd.ofp);
			npda_cnt++;
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & C_FULL) {
				nodepda_s_banner(cmd.ofp, BANNER|SMAJOR);
			}
			get_value(cmd.args[i], &mode, K_MAXCPUS(K), &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_NODEPDA;
				kl_print_error(K);
			}
			else {
				if (mode == 1) {
					id = value;
#ifdef XXX
					nodepdaval = kl_get_nodepda_s(K, id, npdap);
#endif
				}
				else if (mode == 2) {
					nodepdaval = value;
#ifdef XXX
					id = nodepda_to_nodid(nodepdaval);
#else
					id = 0;  
#endif
					kl_get_struct(K, value, 	
						NODEPDA_S_SIZE(K), npdap, "nodepda_s");
				}
				else {
					KL_SET_ERROR_NVAL(KLE_BAD_NODEPDA, value, mode);
				}
				if (KL_ERROR) {
					kl_print_error(K);
				}
				else {
					print_nodepda_s(nodepdaval, npdap, cmd.flags, cmd.ofp);
					npda_cnt++;
				}
			}
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
	}
	nodepda_s_banner(cmd.ofp, SMAJOR);
	PLURAL("nodepda_s struct", npda_cnt, cmd.ofp);
	free_block(npdap);
	return(0);
}

#define _NODEPDA_USAGE "[-f] [-w outfile] [nodepda_list]"

/*
 * nodepda_usage() -- Print the usage string for the 'nodepda' command.
 */
void
nodepda_usage(command_t cmd)
{
	CMD_USAGE(cmd, _NODEPDA_USAGE);
}

/*
 * nodepda_help() -- Print the help information for the 'nodepda' command.
 */
void
nodepda_help(command_t cmd)
{
	CMD_HELP(cmd, _NODEPDA_USAGE,
		"Display the nodepda_s structure associated with each item in "
		"nodepda_list. Entries in nodepda_list can take the form of a "
		"node ID or a virtual address pointing to a nodepda_s struct in "
		"memory. If nodepda_list is empty, the nodepda_s structure for all "
		"nodes will be displayed.");
}

/*
 * nodepda_parse() -- Parse the command line arguments for 'nodepda'.
 */
int
nodepda_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL);
}
