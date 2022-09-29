#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_vertex.c,v 1.7 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * vertex_cmd() -- Print out information on a hwgraph vertex
 */
int
vertex_cmd(command_t cmd)
{
	int i, mode, vertex_cnt = 0, first_time = 1;
	kaddr_t vertex;
	k_ptr_t gvp;

	gvp = kl_alloc_block(K_VERTEX_SIZE, K_TEMP);
	graph_vertex_s_banner(cmd.ofp, BANNER|SMAJOR);
	if (!cmd.nargs) {
		int ret, vertex_found = 0, placeptr = 0;

		while (1) {

			if (ret = kl_vertex_get_next(&vertex_found, &placeptr)) {
				break;
			}

			kl_get_graph_vertex_s(vertex_found, 0, gvp, cmd.flags);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_GRAPH_VERTEX_S;
				kl_print_error();
			}
			else if (kl_kaddr(gvp, "graph_vertex_s", "v_info_list") || 
													(cmd.flags & C_ALL)) {
				if (cmd.flags & (C_NEXT|C_FULL)) {
					if (first_time) {
						first_time = 0;
					}
					else {
						graph_vertex_s_banner(cmd.ofp, BANNER|SMAJOR);
					}
				}
				print_graph_vertex_s(vertex_found, gvp, cmd.flags, cmd.ofp);
				vertex_cnt++;

				if (cmd.flags & (C_NEXT|C_FULL)) {
					fprintf(cmd.ofp, "\n");
				}
			}
		}
	}
	else {
		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & (C_NEXT|C_FULL)) {
				if (first_time) {
					first_time = 0;
				}
				else {
					graph_vertex_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
			}
			GET_VALUE(cmd.args[i], &vertex);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_GRAPH_VERTEX_S;
				kl_print_error();
			}
			else {
				kl_is_valid_kaddr(vertex, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
				if (KL_ERROR) {
					mode = 0;
					kl_reset_error();
				}
				else {
					mode = 2;
				}
				kl_get_graph_vertex_s(vertex, mode, gvp, cmd.flags);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_GRAPH_VERTEX_S;
					kl_print_error();
				}
				else {
					print_graph_vertex_s(vertex, gvp, cmd.flags, cmd.ofp);
					vertex_cnt++;
				}
			}
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
	}
	kl_free_block(gvp);
	graph_vertex_s_banner(cmd.ofp, SMAJOR);
	PLURAL("graph_vertex_s struct", vertex_cnt, cmd.ofp);
	return(0);
}

#define _VERTEX_USAGE "[-a] [-f] [-n] [-w outfile] [vertex_list]"

/*
 * vertex_usage() -- Print the usage string for the 'vertex' command.
 */
void
vertex_usage(command_t cmd)
{
	CMD_USAGE(cmd, _VERTEX_USAGE);
}

/*
 * vertex_help() -- Print the help information for the 'vertex' command.
 */
void
vertex_help(command_t cmd)
{
	CMD_HELP(cmd, _VERTEX_USAGE,
		"Display the graph_vertex_s structure for each entry in vertex_list. "
		"If no entries are specified, display all active entries in the "
		"hwgraph. Entries in vertex_list can take the form of a vertex handle "
		"or virtual address. If the -a option is specified, display all "
		"entries in the hwgraph (in use or free); if the -f option is "
		"specified, display additional information about each vertex "
		"(connect point and index values); if the -n option is specified, "
		"display all edge and info information related to a given vertex.");
}

/*
 * vertex_parse() -- Parse the command line arguments for 'vertex'.
 */
int
vertex_parse(command_t cmd)
{
	return (C_WRITE|C_FULL|C_NEXT|C_ALL);
}
