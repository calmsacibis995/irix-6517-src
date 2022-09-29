#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_hwpath.c,v 1.5 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * hwpath_cmd() -- Run the 'hwpath' command.  
 */
int
hwpath_cmd(command_t cmd)
{
	int i, vhndl = 0, mode, vertex_cnt;
	path_rec_t *prec, *p;
	path_t *path;
	path_chunk_t *pcp;
	char *name;
	kaddr_t vertex;

	path = kl_init_path_table();

	if (cmd.flags & C_VERTEX) {
		/* With the vertex flag, there have to be command line arguments
		 */
		if (cmd.nargs == 0) {
			(void)hwpath_usage(cmd);
			return(1);
		}
		fprintf(cmd.ofp, "\n");
		for (i = 0; i < cmd.nargs; i++) {
			GET_VALUE(cmd.args[i], &vertex);
			if (KL_ERROR) {
				/* XXX - KLE_BAD_STRUCT is place holder for proper error flag
				 */
				KL_ERROR |= KLE_BAD_STRUCT;
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

				if (prec = kl_vertex_to_pathname(vertex, mode, K_TEMP)) {
					fprintf(cmd.ofp, "pathname for vertex ");
					if (mode == 0) {
						fprintf(cmd.ofp, "handle %lld\n", vertex);
					}
					else {
						fprintf(cmd.ofp, "at 0x%llx\n", vertex);
					}
					vertex_cnt++;
					p = prec;
					while (p) {
						fprintf(cmd.ofp, "/%s", p->name);
						p = p->next;
					}
					if (cmd.flags & C_FULL) {
						p = prec;
						fprintf(cmd.ofp, "\n    ");
						while (p) {
							if (p == prec) {
								fprintf(cmd.ofp, "%d", p->vhndl);
							}
							else {
								fprintf(cmd.ofp, "=>%d", p->vhndl);
							}
							p = p->next;
						}
					}
					kl_free_pathname(prec);
				}
				else {
					if (mode == 0) {
						fprintf(cmd.ofp, "%lld is an invalid vertex handle",
							vertex);
					}
					else {
						fprintf(cmd.ofp, "0x%llx is an invalid vertex address",
							vertex);
					}
				}
				fprintf(cmd.ofp, "\n\n");
			}
		}
		PLURAL("pathname", vertex_cnt, cmd.ofp);
		return(0);
	}
	if (cmd.nargs == 0) {
		kl_find_pathname(0, (char*)NULL, path, cmd.flags, 0);
	}
	else {
		if (cmd.nargs == 2) {
			vhndl = atoi(cmd.args[1]);
		}
		kl_find_pathname(0, cmd.args[0], path, cmd.flags, vhndl);
	}

	pcp = path->pchunk;
	do {
		for (i = 0; i <= pcp->current; i++) {
			p = pcp->path[i];
			do {
				fprintf(cmd.ofp, "/%s", p->name);
				p = p->next;
			} while (p != pcp->path[i]);

			if (cmd.flags & C_FULL) {
				p = pcp->path[i];
				fprintf(cmd.ofp, "\n    ");
				do {
					if (p == pcp->path[i]) {
						fprintf(cmd.ofp, "%d", p->vhndl);
					}
					else {
						fprintf(cmd.ofp, "=>%d", p->vhndl);
					}
					p = p->next;
				} while (p != pcp->path[i]);
			}
			fprintf(cmd.ofp, "\n");
		}
		pcp = pcp->next;
	} while (pcp != path->pchunk);
	kl_free_path(path);
	return(0);
}

#define _HWPATH_USAGE "[-f] [-v] [-w outfile] [element_list] | vertex_list"

/*
 * hwpath_usage() -- Print the usage string for the 'hwpath' command.
 */
void
hwpath_usage(command_t cmd)
{
	CMD_USAGE(cmd, _HWPATH_USAGE);
}

/*
 * hwpath_help() -- Print the help information for the 'hwpath' command.
 */
void
hwpath_help(command_t cmd)
{
	CMD_HELP(cmd, _HWPATH_USAGE,
		"Display all hwgraph pathnames that terminate with the names "
		"included in element_list, or that contain label names included "
		"in element_list. If no names are specified, display all unique "
		"pathnames in the hwgraph. Unique pathnames are ones that "
		"terminate in a file or symbolic link rather than in a "
		"directory. When the -v command line option is specified, treat "
		"each argument as a vertex handle or address and display its full "
		"pathname. When the hwpath command is issued with a -f command "
		"line option, the vertex handle for each element in the path is "
		"also displayed. Note that the hwpath command provides a view "
		"of the hwgraph similar to the filesystem-like representation "
		"presented on the system. ");
}

/*
 * hwpath_parse() -- Parse the command line arguments for 'hwpath'.
 */
int
hwpath_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_WRITE|C_VERTEX);
}
