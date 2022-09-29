#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/gfx.h>
#include <sys/kona.h>
#include <sys/kona_diag.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * print_nic_info()
 */
void
print_nic_info(nic_info_t *nip, FILE *ofp, char *board)
{
	int len, ret_val;
	char *c, buf[30];
	char name[30], nic_num[30], serial_num[30], part_num[30], rev[30];

	fprintf(ofp, "%6s:  ", board);
	ret_val = get_nic_info(nip, nic_num, name, serial_num, part_num, rev);
	if (ret_val < 0) {
		fprintf(ofp, "No NIC serial number available\n");
		return;
	}
	if (nic_num[0]) {
		fprintf(ofp, "NIC #:      %s\n", nic_num);
	}

	switch(ret_val) {

		case 0:
			if (name[0]) {
				fprintf(ofp, "         NAME:       %s\n", name);
			}
			if (serial_num[0]) {
				fprintf(ofp, "         SERIAL #:   %s\n", serial_num);
			}
			if (part_num[0]) {
				fprintf(ofp, "         PART #:     %s", part_num);
			}
			if (rev[0]) {
				fprintf(ofp, "         REVISION:   %s\n", rev);
			}
			break;

		case 2:
			fprintf(ofp, "         WARNING: No NIC page A found.\n");
			break;;

		case 3:
			fprintf(ofp, "         WARNING: Programmed NIC data not "
					"reportable.\n");
			break;

		case 4:
			fprintf(ofp, "         WARNING: Uninitialized NIC.\n");
			break;

		case 5:
			fprintf(ofp, "         WARNING: Unknown NIC format (format=%d).\n",
					nip->page_a.sgi_fmt_1.format);
			break;
	}
}

/*
 * print_kona_info()
 */
int
print_kona_info(kaddr_t info, k_ptr_t infop, FILE *ofp)
{
	int hilo, hilo_lite, kona_len;
	kona_info_t *konainfo;
	kona_config *psysconf;

	kona_len = KL_INT(infop, "gfx_info", "length");

	konainfo = (kona_info_t*)kl_alloc_block(kona_len, K_TEMP);
	kl_get_block(info, kona_len, konainfo, "kona_info_t");
	psysconf = &konainfo->config_info;


	hilo = ((psysconf->bdset == BDSET_HILO) ||
			(psysconf->bdset == BDSET_HILO_8C) ||
			(psysconf->bdset == BDSET_HILO_GE16));

	hilo_lite = (psysconf->bdset == BDSET_HILO_LITE);

	print_nic_info(&psysconf->nic.ge, ofp, "GE");
	if (hilo || hilo_lite) {
		print_nic_info(&psysconf->nic.ktown, ofp, "KT");
	}
	print_nic_info(&psysconf->nic.rm[0], ofp, "RM0");
	if (hilo) {
		print_nic_info(&psysconf->nic.tm[0], ofp, "TM0");
	}
	print_nic_info(&psysconf->nic.rm[1], ofp, "RM1");
	if (hilo) {
		print_nic_info(&psysconf->nic.tm[1], ofp, "TM1");
	}
	print_nic_info(&psysconf->nic.rm[2], ofp, "RM2");
	if (hilo) {
		print_nic_info(&psysconf->nic.tm[2], ofp, "TM2");
	}
	print_nic_info(&psysconf->nic.rm[3], ofp, "RM3");
	if (hilo) {
		print_nic_info(&psysconf->nic.tm[3], ofp, "TM3");
	}
	print_nic_info(&psysconf->nic.backplane, ofp, "BP");
	print_nic_info(&psysconf->nic.dg, ofp, "DG");
	print_nic_info(&psysconf->nic.dgopt, ofp, "DGOPT");
	kl_free_block(infop);
	kl_free_block(konainfo);
	return(0);
}

/*
 * print_gfx_board()
 */
int
print_gfx_board(kaddr_t value, k_ptr_t gfxbp, int flags, FILE *ofp)
{
	int info_size;
	k_ptr_t *infop;
	kaddr_t info;

	fprintf(ofp, "  gb_info=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_info"));
	fprintf(ofp, "  gb_manager=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_manager"));
	fprintf(ofp, "  gb_owner=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_owner"));
	fprintf(ofp, "  gb_number=%lld\n", 
		KL_INT(gfxbp, "gfx_board", "gb_number"));
	fprintf(ofp, "  gb_rq_ptr=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_rq_ptr"));
	fprintf(ofp, "  gb_wq_ptr=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_wq_ptr"));
	fprintf(ofp, "  gb_devminor=%lld\n", 
		KL_INT(gfxbp, "gfx_board", "gb_devminor"));
	fprintf(ofp, "  gb_index=%lld\n", 
		KL_INT(gfxbp, "gfx_board", "gb_index"));
	fprintf(ofp, "  gb_fncs=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_fncs"));
	fprintf(ofp, "  gb_data=0x%llx\n", 
		kl_kaddr(gfxbp, "gfx_board", "gb_data"));
	fprintf(ofp, "\n");

	info_size = kl_struct_len("gfx_info");
	infop = kl_alloc_block(info_size, K_TEMP);
	info = kl_kaddr(gfxbp, "gfx_board", "gb_info");
	kl_get_block(info, info_size, infop, "gfx_info");
	if (strstr(CHAR(infop, "gfx_info", "name"), "KONA")) {
		print_kona_info(info, infop, ofp);
	}
	else {
	}
	kl_free_block(infop);
	return(0);
}

/*
 * gfxinfo_cmd() -- Dump out information about graphics boards
 */
int
gfxinfo_cmd(command_t cmd)
{
	int i, mode, size, gfxbrd_cnt = 0, first_time = TRUE;
	int numofboards;
	struct syment *symp;    /* temp syment pointer */
	kaddr_t value;
	k_ptr_t gfxbp;

	size = kl_struct_len("gfx_board");
	gfxbp = kl_alloc_block(size, K_TEMP);

	/*
	gfxbd_banner(cmd.ofp, BANNER|SMAJOR);
	 */
	if (cmd.nargs) {
		for (i = 0; i < cmd.nargs; i++) {
			if (cmd.flags & C_FULL) {
				if (first_time) {
					first_time = FALSE;
				}
				else {
					/*
					gfxbd_banner(cmd.ofp, BANNER|SMAJOR);
					 */
				}
			}
			kl_get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				kl_print_error();
			}
			else {
				get_gfx_board(value, mode, gfxbp);
				if (KL_ERROR) {
					KL_ERROR |= KLE_BAD_STRUCT;
					kl_print_error();
					continue;
				}
				gfxbrd_cnt++;
				print_gfx_board(value, gfxbp, cmd.flags, cmd.ofp);
			}
			fprintf(cmd.ofp, "\n");
		}
	}
	else {
		if (symp = kl_get_sym("GfxNumberOfBoards", K_TEMP)) {
			kl_get_block(symp->n_value, 4, &numofboards, "GfxNumberOfBoards");
			kl_free_sym(symp);
		}
		else {
			kl_free_block(gfxbp);
			return(1);
		}
		for (i = 0; i < numofboards; i++) {
			get_gfx_board(i, 1, gfxbp);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_STRUCT;
				kl_print_error();
				continue;
			}
			gfxbrd_cnt++;
			print_gfx_board(i, gfxbp, cmd.flags, cmd.ofp);
		}
	}

	/*
	gfxbd_banner(cmd.ofp, SMAJOR);
	*/
	PLURAL("gfx_board struct", gfxbrd_cnt, cmd.ofp);
	kl_free_block(gfxbp);
	return(0);
}

#define _GFXINFO_USAGE "[-f] [-w outfile] [board]"

/*
 * gfxinfo_usage() -- Print the usage string for the 'gfxinfo' command.
 */
void
gfxinfo_usage(command_t cmd)
{
	CMD_USAGE(cmd, _GFXINFO_USAGE);
}

/*
 * gfxinfo_help() -- Print the help information for the 'gfxinfo' command.
 */
void
gfxinfo_help(command_t cmd)
{
	CMD_HELP(cmd, _GFXINFO_USAGE,
		"gfxinfo command help.");
}

/*
 * gfxinfo_parse() -- Parse the command line arguments for 'gfxinfo'.
 */
int
gfxinfo_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE|C_FULL);
}
