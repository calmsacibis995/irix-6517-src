#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_config.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include "icrash.h"
#include "extern.h"

/*
 * config_cmd() -- Run the 'config' command.  
 */
int
config_cmd(command_t cmd)
{
	int i, nic_size;
	path_rec_t *p;
	path_t *path;
	path_chunk_t *pcp;
	k_ptr_t gvp, gep, gip, nic;
	char *module = 0, *slot = 0, *board = 0; 
	char *MODZERO = "0";

	gvp = alloc_block(VERTEX_SIZE(K), B_TEMP);
	gep = alloc_block(GRAPH_EDGE_S_SIZE(K), B_TEMP);
	gip = alloc_block(GRAPH_INFO_S_SIZE(K), B_TEMP);

	path = init_path_table(K);

	hw_find_pathname(K, 0, "_nic", path, cmd.flags);
	if (!path->count) {
		fprintf(cmd.ofp, "No configuration information available.\n");
		return(1);
	}

	fprintf(cmd.ofp, "MODULE  BOARD           SLOT             SERIAL_NO     "
		"   PART_NO  REV\n");
	fprintf(cmd.ofp, "=================================================="
		"====================\n");

	pcp = path->pchunk;
	do {
		for (i = 0; i <= pcp->current; i++) {
			p = pcp->path[i];

			do {
				if (!strcmp(p->name, "module")) {
					module = p->next->name;
				}
				else if (!strcmp(p->name, "slot")) {
					slot = p->next->name;
					board = p->next->next->name;
				}
				p = p->next;
			} while (p != pcp->path[i]);

			/* Get the board vertex
			 */
			kl_get_graph_vertex_s(K, pcp->path[i]->prev->vhndl, 0, gvp, 0);
			if (!KL_ERROR) {

				/* Get the edge labled "_nic"
				 */
				vertex_info_name(K, gvp, "_nic", gip);
				if (!KL_ERROR) {
					nic_size = KL_UINT(K, gip, "graph_info_s", "i_info_desc");
					nic = (char*)alloc_block(nic_size, B_TEMP);
					kl_get_block(K, kl_kaddr(K, gip, "graph_info_s", "i_info"), 
						nic_size, nic, "NIC");
				}
			}

			/* If module is NULL then check to see if this is a single
			 * module system (e.g., anything that's not an IP27.
			 */
			if (!module && (K_IP(K) != 27)) {
				module = MODZERO;
			}
			print_mfg_info(K, module, slot, board, nic, cmd.flags, cmd.ofp);
			free_block((k_ptr_t)nic);
			nic = 0;
		}
		pcp = pcp->next;
	} while (pcp != path->pchunk);
	fprintf(cmd.ofp, "=================================================="
		"====================\n");

	free_block(gvp);
	free_block(gep);
	free_block(gip);
	free_path(K, path);
	return(0);
}

#define _CONFIG_USAGE "[-f] [-w outfile]"

/*
 * config_usage() -- Print the usage string for the 'config' command.
 */
void
config_usage(command_t cmd)
{
    CMD_USAGE(cmd, _CONFIG_USAGE);
}

/*
 * config_help() -- Print the help information for the 'config' command.
 */
void
config_help(command_t cmd)
{
    CMD_HELP(cmd, _CONFIG_USAGE,
        "Display the board configuration for a system using the "
		"information contained in the hwgraph. ");
}

/*
 * config_parse() -- Parse the command line arguments for 'config'.
 */
int
config_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
