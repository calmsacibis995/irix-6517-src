#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_pfind.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "icrash.h"
#include "extern.h"

/*
 * pfind_cmd() -- Find a page in the page table.
 */
int
pfind_cmd(command_t cmd)
{
	int pfdat_cnt = 0;
	kaddr_t tag, value = 0;
	int i, firsttime = 1;
	k_ptr_t pfp;
	k_uint_t pgno;
	kaddr_t npfdat, pfdatpp;

	pfp = alloc_block(PFDAT_SIZE(K), B_TEMP);

	GET_VALUE(cmd.args[0], &tag);
	if (KL_ERROR) {
		kl_print_error(K);
		return (1);
	}
	if (cmd.nargs == 2) {
		GET_VALUE(cmd.args[1], &pgno);
		if (KL_ERROR) {
			kl_print_error(K);
			return (1);
		}
		if (!locate_pfdat(pgno, tag, pfp, &value)) {
			fprintf(cmd.ofp, "pfdat structure for 0x%llx/%lld not found in "
					"page hash table!\n", tag, pgno);
			return (1);
		}
		pfdat_banner(cmd.ofp, BANNER|SMAJOR);
		print_pfdat(value, pfp, cmd.flags, cmd.ofp);
		free_block(pfp);
		return(0);
	}
	else {
		pfdat_banner(cmd.ofp, BANNER|SMAJOR);
		for (i = 0; ; i++) {
			pfdatpp = (_phash + (i * nbpw));

			if (DEBUG(DC_PAGE, 2)) {
				fprintf(cmd.ofp, "pfind: i=%d (0x%llx)\n", i, pfdatpp);
			}

			kl_get_kaddr(pfdatpp, &npfdat, "pfdat");
			kl_get_kaddr(pfdatpp, &npfdat, "pfdat");

			/* Walk the hash chain looking for pfdat structs with
			 * matching tags.
			 */
			while (npfdat) {
				if (DEBUG(DC_PAGE, 2)) {
					fprintf(cmd.ofp, "pfind: npfdat=0x%llx\n", npfdat);
				}
				kl_get_struct(K, npfdat, PFDAT_SIZE(K), pfp, "pfdat");
				if (KL_ERROR) {
					break;
				}
				if (kl_kaddr(pfp, "pfdat", "p_un") == tag) {
					print_pfdat(npfdat, pfp, cmd.flags, cmd.ofp);
					pfdat_cnt++;
				}

				/* Get the next pfdat struct on the hash chain.
				 */
				npfdat = kl_kaddr(pfp, "pfdat", "pf_hchain");
			}

			if (pfdatpp == _phashlast) {
				break;
			}
		}
	}
	pfdat_banner(cmd.ofp, SMAJOR);
	free_block(pfp);
	PLURAL("pfdat struct", pfdat_cnt, cmd.ofp);
	return(0);
}

#define _PFIND_USAGE "[-f] [-w outfile] tag [pageno]"

/*
 * pfind_usage() -- Print the usage string for the 'pfind' command.
 */
void
pfind_usage(command_t cmd)
{
	CMD_USAGE(cmd, _PFIND_USAGE);
}

/*
 * pfind_help() -- Print the help information for the 'pfind' command.
 */
void
pfind_help(command_t cmd)
{
	CMD_HELP(cmd, _PFIND_USAGE,
		"Display all pfdat structures currently in the page hash table "
		"with tag.  If pgno is specified, display only the pfdat "
		"structure that exactly matches tag and pgno. Note that when a pgno "
		"is not specified, the search can take quite some time to complete -- "
		"due to the extremely large size of the pfdat hash table.");
}

/*
 * pfind_parse() -- Parse the command line arguments for 'pfind'.
 */
int
pfind_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL);
}
