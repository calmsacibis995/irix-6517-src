#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_avlnode.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/* 
 * next_avlnode()
 */
int
next_avlnode(kaddr_t avl, k_ptr_t avlp, int flags, FILE *ofp, int *avl_cnt)
{
	kaddr_t forw, back;

	if (!avl) {
		return(-1);
	}

	if (*avl_cnt && (DEBUG(DC_GLOBAL, 1) || (flags & C_FULL))) {
		avlnode_banner(ofp, BANNER|SMAJOR);
	}
	print_avlnode(avl, avlp, flags, ofp);
	(*avl_cnt)++;
	
	forw = kl_kaddr(K, avlp, "avlnode", "avl_forw");
	back = kl_kaddr(K, avlp, "avlnode", "avl_back");

	if (forw) {
		if (!kl_get_struct(K, forw, AVLNODE_SIZE(K), avlp, "avlnode")) {
			return(-1);
		}
		next_avlnode(forw, avlp, flags, ofp, avl_cnt);
	}

	if (back) {
		if (!kl_get_struct(K, back, AVLNODE_SIZE(K), avlp, "avlnode")) {
			return(-1);
		}
		next_avlnode(back, avlp, flags, ofp, avl_cnt);
	}
	return(0);
}

/*
 * walk_avltree()
 */
int
walk_avltree(kaddr_t avl, int flags, FILE *ofp)
{
	int i, firsttime = 1, avl_cnt = 0;
	kaddr_t parent;
	k_ptr_t avlp, avlbuf;

	avlbuf = alloc_block(AVLNODE_SIZE(K), B_TEMP);
	avlp = kl_get_struct(K, avl, AVLNODE_SIZE(K), avlbuf, "avlnode");
	if (!avlp) {
		fprintf(ofp, "Could not read avlnode at 0x%llx\n", avl);
		return(0);
	}

	/* Walk up the tree to the root node using the avl_parent pointer
	 */
	parent = kl_kaddr(K, avlp, "avlnode", "avl_parent");
	while(parent) {
		avlp = kl_get_struct(K, parent, AVLNODE_SIZE(K), avlbuf, "avlnode");
		if (!avlp || KL_ERROR) {
		}
		avl = parent;
		parent = kl_kaddr(K, avlp, "avlnode", "avl_parent");
	}

	/* We should have the root node. Now traverse the tree, printing
	 * out info for each node.
	 */
	next_avlnode(avl, avlp, flags, ofp, &avl_cnt);
	free_block(avlbuf);
	return(avl_cnt);
}

/*
 * avlnode_cmd() -- Display one or more avlnode structs
 */
int
avlnode_cmd(command_t cmd)
{
	int i, firsttime = 1, avl_cnt = 0;
	kaddr_t  value;
	kaddr_t avl;
	k_ptr_t avlp, avlbuf;

	avlnode_banner(cmd.ofp, BANNER|SMAJOR);
	avlbuf = alloc_block(AVLNODE_SIZE(K), B_TEMP);
	for (i = 0; i < cmd.nargs; i++) {

		GET_VALUE(cmd.args[i], &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_AVLNODE;
			kl_print_error(K);
			continue;
		}

		if (cmd.flags & C_NEXT) {
			if (i) {
				fprintf(cmd.ofp, "\n");
				avlnode_banner(cmd.ofp, BANNER|SMAJOR);
			}
			avl_cnt += walk_avltree(value, cmd.flags, cmd.ofp);
		}
		else {
			avlp = kl_get_struct(K, value, AVLNODE_SIZE(K), avlbuf, "avlnode");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_AVLNODE;
				kl_print_error(K);
			}
			else {
				if (DEBUG(DC_GLOBAL, 1) || (cmd.flags & C_FULL)) {
					if (!firsttime) {
						avlnode_banner(cmd.ofp, BANNER|SMAJOR);
					} 
					else {
						firsttime = 0;
					}
				}
				print_avlnode(value, avlp, cmd.flags, cmd.ofp);
				avl_cnt++;
			} 
		}
	}
	avlnode_banner(cmd.ofp, SMAJOR);
	PLURAL("avlnode struct", avl_cnt, cmd.ofp);
	free_block(avlbuf);
	return(0);
}

#define _AVLNODE_USAGE "[-f] [-n] [-w outfile] avlnode_list"

/*
 * avlnode_usage() -- Print the usage string for the 'avlnode' command.
 */
void
avlnode_usage(command_t cmd)
{
	CMD_USAGE(cmd, _AVLNODE_USAGE);
}

/*
 * avlnode_help() -- Print the help information for the 'avlnode' command.
 */
void
avlnode_help(command_t cmd)
{
	CMD_HELP(cmd, _AVLNODE_USAGE,
		"Display the avlnode structure located at each virtual address "
		"included in avlnode_list. If the -n option is specified, the root "
		"of the avltree is identified (using the parent pointer contained "
		"in the avlnode structure) and all the avlnodes in the tree are "
		"displayed.");
}

/*
 * avlnode_parse() -- Parse the command line arguments for 'avlnode'.
 */
int
avlnode_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT);
}
