#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_pfdathash.c,v 1.13 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * pfdathash_cmd() -- Compute page frame data hash information.
 */
int
pfdathash_cmd(command_t cmd)
{
	int i, pfdat_cnt = 0, p, mode, bkt = 0, firsttime = 1;
	char *loc;
	k_ptr_t pfp;
	kaddr_t pfdatpp, npfdat;

	pfp = kl_alloc_block(PFDAT_SIZE, K_TEMP);

	if (cmd.nargs == 0) {
		for (i = 0; ; i++) {

			pfdatpp = (_phash + (i * nbpw));

			if (DEBUG(DC_PAGE, 1)) {
				fprintf(cmd.ofp, "pfdathash: i=%d (0x%x)\n", i, pfdatpp);
			}
			kl_get_kaddr(pfdatpp, &npfdat, "pfdat");
			fprintf(cmd.ofp, "\nPHASH[%d]: 0x%llx\n", i, pfdatpp);
			if (npfdat) {
				fprintf(cmd.ofp, "\n");
				pfdat_banner(cmd.ofp, BANNER|SMAJOR);
			}
			while (npfdat) {
				if (DEBUG(DC_PAGE, 2)) {
					fprintf(cmd.ofp, "pfdathash: npfdat=0x%llx\n", npfdat);
				}
				kl_get_struct(npfdat, PFDAT_SIZE, pfp, "pfdat");
				if (KL_ERROR) {
					break;
				}
				print_pfdat(npfdat, pfp, cmd.flags, cmd.ofp);
				pfdat_cnt++;
				npfdat = kl_kaddr(pfp, "pfdat", "pf_hchain");
			}

			if (pfdatpp == _phashlast) {
				break;
			}
		}
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			fprintf(cmd.ofp, "\n");
			kl_get_value(cmd.args[i], &mode, 
						((_phashlast - _phash) / nbpw) + 1, &pfdatpp);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_PFDATHASH;
				kl_print_error();
				continue;
			}

			if (mode == 0) {
				p = pfdatpp;
				pfdatpp = (_phash + (p * nbpw));
			}
			else if (mode == 2) {
				kl_virtop(pfdatpp, (k_ptr_t)NULL);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_PFDATHASH;
					kl_print_error();
					continue;
				}
				if (pfdatpp % nbpw) {
					KL_SET_ERROR_NVAL(E_INVALID_VADDR_ALIGN|KLE_BAD_PFDATHASH, 
							pfdatpp, 2);
					kl_print_error();
					continue;
				}
				p = ((pfdatpp - _phash) / nbpw);
			}
			kl_get_kaddr(pfdatpp, &npfdat, "pfdat");
			fprintf(cmd.ofp, "PHASH[%d]: 0x%llx\n", p, pfdatpp);
			if (npfdat) {
				pfdat_banner(cmd.ofp, BANNER|SMAJOR);
			}
			while (npfdat) {
				if (DEBUG(DC_PAGE, 2)) {
					fprintf(cmd.ofp, "pfdathash: npfdat=0x%llx\n", npfdat);
				}
				kl_get_struct(npfdat, PFDAT_SIZE, pfp, "pfdat");
				if (KL_ERROR) {
					break;
				}
				print_pfdat(npfdat, pfp, cmd.flags, cmd.ofp);
				pfdat_cnt++;
				npfdat = kl_kaddr(pfp, "pfdat", "pf_hchain");
			}
			if (pfdatpp == _phashlast) {
				break;
			}
		}
	}
	if (pfdat_cnt) {
		pfdat_banner(cmd.ofp, SMAJOR);
		fprintf(cmd.ofp, "%d pfdat struct%s found\n", 
			pfdat_cnt, (pfdat_cnt != 1) ? "s" : "");
	}
	return(0);
}

#define _PFDATHASH_USAGE "[-a] [-f] [-w outfile] [bucket_list]"

/*
 * pfdathash_usage() -- Print the usage string for the 'pfdathash' command.
 */
void
pfdathash_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PFDATHASH_USAGE);
}

/*
 * pfdathash_help() -- Print the help information for the 'pfdathash' command.
 */
void
pfdathash_help(command_t cmd)
{
	CMD_HELP(cmd, _PFDATHASH_USAGE,
		"Display all pfdat structures in each of the page hash table "
		"buckets included in bucket_list.  If no page hash table buckets "
		"are specified, display the pfdat structures in all pfdat hash "
		"table buckets.  Items on bucket_list can consist of phash "
		"indexes (in decimal form) and/or hexadecimal virtual addresses "
		"(in any order).");
}

/*
 * pfdathash_parse() -- Parse the command line arguments for 'pfdathash'.
 */
int
pfdathash_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL);
}
