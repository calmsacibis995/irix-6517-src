#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_uipcvn.c,v 1.15 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * uipc_vnsocket() -- Print out information on a uipc socket address.
 */
int
uipc_vnsocket(kaddr_t soc, int hold_flag, int *flags, FILE *ofp)
{
	k_ptr_t sp;

	sp = kl_alloc_block(SOCKET_SIZE, K_TEMP);

	get_socket(soc, sp, *flags);
	if (KL_ERROR) {
		/*
		 * This should never happen with a core dump. It might
		 * happen on a live system (moving target). If it does,
		 * Just go onto the next command argument.
		 */
		if (DEBUG(DC_GLOBAL, 1)) {
			kl_print_debug("uipc_vnsocket");
		}
		kl_free_block(sp);
		return (0);
	}

	/* Turn the C_NEXT flag back on if it was saved 
	 */
	if (hold_flag) {
		*flags |= C_NEXT;
	}

	socket_banner(ofp, BANNER|C_INDENT);
	socket_banner(ofp, SMINOR|C_INDENT);
	print_socket(soc, sp, *flags|C_INDENT, ofp);
	fprintf(ofp, "\n");
	kl_free_block(sp);
	return (1);
}

/*
 * uipcvn_cmd() -- Run the 'uipcvn' command to print UIPC vnodes.
 */
int
uipcvn_cmd(command_t cmd)
{
	int i, c, hold_flag = 0, soc_cnt = 0;
	int cpu, ncpu = (int)numcpus;
	kaddr_t soc, vn, value;
	struct vnlist *vlp = (struct vnlist *)uipc_vnlist;
	k_ptr_t vp, tmpptr;

	/*
	 * Save the C_NEXT flag if it is set so that two socket structs
	 * aren't printed.
	 */
	if (cmd.flags & C_NEXT) {
		hold_flag++;
		cmd.flags &= ~(C_NEXT);
	}

	vp = kl_alloc_block(VNODE_SIZE, K_TEMP);

	if (cmd.nargs == 0) {
		for (cpu = 0; cpu < ncpu; cpu++) {
			tmpptr = (k_ptr_t)((unsigned)vlp + (cpu * kl_struct_len("vnlist")));
			vn = kl_kaddr(tmpptr, "vnlist", "vl_next");
			while (vn != (kaddr_t)(_uipc_vnlist + 
						(cpu * kl_struct_len("vnlist")))) {
				get_vnode(vn, vp, cmd.flags);
				if (KL_ERROR) {
					if (DEBUG(DC_GLOBAL, 1)) {
						kl_print_debug("uipcvn_cmd");
					}
					break;
				}
				soc = kl_kaddr(vp, "vnode", "v_data");
				vnode_banner(cmd.ofp, BANNER|SMAJOR);
				print_vnode(vn, vp, cmd.flags, cmd.ofp);
				fprintf (cmd.ofp, "\n");
				soc_cnt += uipc_vnsocket(soc, hold_flag, &(cmd.flags), cmd.ofp);
				vn = kl_kaddr(vp, "vnode", "v_next");
			}
		}
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			GET_VALUE(cmd.args[i], &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_SOCKET;
				kl_print_error();
				continue;
			}

			kl_is_valid_kaddr(value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_SOCKET;
				kl_print_error();
				continue;
			}

			c = soc_cnt;
			for (cpu = 0; cpu < ncpu; cpu++) {
				tmpptr = 
					(k_ptr_t)((unsigned)vlp + (cpu * kl_struct_len("vnlist")));
				vn = kl_kaddr(tmpptr, "vnlist", "vl_next");
				while (vn != (kaddr_t)(_uipc_vnlist + 
										(cpu * kl_struct_len("vnlist")))) {
					get_vnode(vn, vp, cmd.flags);
					if (KL_ERROR) {
						kl_print_error();
						break;
					}
					soc = kl_kaddr(vp, "vnode", "v_data");
					if (soc == value) {
						vnode_banner(cmd.ofp, BANNER|SMAJOR);
						print_vnode(vn, vp, cmd.flags, cmd.ofp);
						fprintf (cmd.ofp, "\n");
						soc_cnt += uipc_vnsocket(soc, hold_flag,
								&(cmd.flags), cmd.ofp);
					}
					vn = kl_kaddr(vp, "vnode", "v_next");
				}
			}
			if (c == soc_cnt) {
				fprintf(cmd.ofp, "0x%llx: socket not found\n\n", value);
			}
		}
	}
	if (soc_cnt) {
		vnode_banner(cmd.ofp, SMAJOR);
		fprintf(cmd.ofp, "%d socket struct%s found\n", 
			soc_cnt, (soc_cnt != 1) ? "s" : "");
	}
	kl_free_block(vp);
	return(0);
}

#define _UIPCVN_USAGE "[-f] [-n] [-w outfile] [socket_list]"

/*
 * uipcvn_usage() -- Print the usage string for the 'uipcvn' command.
 */
void
uipcvn_usage(command_t cmd)
{
	CMD_USAGE(cmd, _UIPCVN_USAGE);
}

/*
 * uipcvn_help() -- Print the help information for the 'uipcvn' command.
 */
void
uipcvn_help(command_t cmd)
{
	CMD_HELP(cmd, _UIPCVN_USAGE,
		"Walk the uipc_vnlist and search for each socket on socket_list. "
		"When a match is found print the vnode and socket structures.  If "
		"no entries are specified, display all vnode/socket structures on "
		"uipc_vnlist.  If the next option (-n) is specified, a linked list "
		"of protocol control block structures associated with each socket "
		"will also be displayed.");
}

/*
 * uipcvn_parse() -- Parse the command line arguments for 'uipcvn'.
 */
int
uipcvn_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_NEXT);
}
