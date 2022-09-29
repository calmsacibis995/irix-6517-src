#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_zone.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include "icrash.h"
#include "extern.h"

/*
 * zone_cmd() -- Dump out zone information, properly formatted.
 */
int
zone_cmd(command_t cmd)
{
	int i, zone_cnt = 0, pcount = 0, first_time = 1;
	kaddr_t zone = 0;
	k_ptr_t zp;

	if ((ACTIVE(K) && !DEBUG(DC_GLOBAL, 1)) && (cmd.flags & C_FULL)) {
		fprintf(cmd.ofp,
			"Warning: -f option is risky on active systems.\n"
			"Warning: Can lead to *INFINITE* loops within "
			"icrash.\n");
	}
	zp = alloc_block(ZONE_SIZE(K), B_TEMP);
	if (cmd.nargs) {
		if (!(cmd.flags & C_FULL)) {
			zone_banner(cmd.ofp, BANNER|SMAJOR);
		}
		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & C_FULL) {
				zone_banner(cmd.ofp, BANNER|SMAJOR);
			}
			GET_VALUE(cmd.args[i], &zone);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_ZONE;
				kl_print_error(K);
				kl_reset_error();
				continue;
			}
			kl_get_struct(K, zone, ZONE_SIZE(K), zp, "zone");
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_ZONE;
				kl_print_error(K);
			}
			else {
				print_zone(zone, zp, cmd.flags, cmd.ofp);
				zone_cnt++;
				pcount += KL_INT(K, zp, "zone", 
						 "zone_total_pages");
			}
			if (cmd.flags & C_FULL) {
				zone_banner(cmd.ofp, SMAJOR);
				fprintf(cmd.ofp,"\n");
			}
		}
		if (!(cmd.flags & C_FULL)) {
			zone_banner(cmd.ofp, SMAJOR);
		}
	}
	else {
              /*
	       * This stuff has moved to the nodepda_s structure. Zone memory
	       * is now allocated on a per-node basis.
	       *
	       * BUT, A big but, we do not follow the nodepda_s structure..
	       * We could do that but we prefer what idbg_zone does... just for
	       * "compatibility" with idbg.
	       */
		fprintf(cmd.ofp,"KMEM ZONESET:\n");

		if (!(cmd.flags & C_FULL)) {
			zone_banner(cmd.ofp, BANNER|SMAJOR);
		}

		zone = kl_kaddr_to_ptr(K,
				       ((__uint64_t)kl_sym_addr(
					       K,"kmem_zoneset") + 
					(__uint64_t)kl_member_offset(K,
						"gzone_t","zone_list")));
		while (zone) {
			if (cmd.flags & C_FULL) {
				zone_banner(cmd.ofp, BANNER|SMAJOR);
			}
			kl_get_struct(K, zone, ZONE_SIZE(K), zp, "zone");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "zone_cmd");
				}
				break;
			}
			else {
				print_zone(zone, zp, cmd.flags, cmd.ofp);
				zone_cnt++;
				pcount += KL_UINT(K, zp, "zone",
						  "zone_total_pages");
				zone = kl_kaddr(K,zp, "zone", "zone_next");
			}
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
		zone_banner(cmd.ofp, SMAJOR);



		fprintf(cmd.ofp,"KMEM PRIVATE ZONES:\n");
		
		if (!(cmd.flags & C_FULL)) {
			zone_banner(cmd.ofp, BANNER|SMAJOR);
		}
		zone = kl_kaddr_to_ptr(K,
				       ((__uint64_t)kl_sym_addr(
					       K,"kmem_private_zones") + 
					(__uint64_t)kl_member_offset(K,
						"gzone_t","zone_list")));
		while (zone) {
			if (cmd.flags & C_FULL) {
				zone_banner(cmd.ofp, BANNER|SMAJOR);
			}
			kl_get_struct(K, zone, ZONE_SIZE(K), zp, "zone");
			if (KL_ERROR) {
				if (DEBUG(DC_GLOBAL, 1)) {
					kl_print_debug(K, "zone_cmd");
				}
				break;
			}
			else {
				print_zone(zone, zp, cmd.flags, cmd.ofp);
				zone_cnt++;
				pcount += KL_UINT(K,zp, "zone", 
						  "zone_total_pages");
				zone = kl_kaddr(K,zp, "zone", "zone_next");
			}
			if (cmd.flags & C_FULL) {
				fprintf(cmd.ofp, "\n");
			}
		}
		zone_banner(cmd.ofp, SMAJOR);
	}
	PLURAL("zone", zone_cnt, cmd.ofp);
	fprintf(cmd.ofp, "%d page(s) in use\n", pcount);
	K_BLOCK_FREE(K)(NULL,zp);
	return(0);
}

#define _ZONE_USAGE "[-a] [-w outfile] [-f zone_list]"

/*
 * zone_usage() -- Print the usage string for the 'zone' command.
 */
void
zone_usage(command_t cmd)
{
	CMD_USAGE(cmd, _ZONE_USAGE);
}

/*
 * zone_help() -- Print the help information for the 'zone' command.
 */
void
zone_help(command_t cmd)
{
	CMD_HELP(cmd, _ZONE_USAGE,
		"Display information about zone memory allocation resources.");
}

/*
 * zone_parse() -- Parse the command line arguments for 'zone'.
 */
int
zone_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL|C_ALL);
}
