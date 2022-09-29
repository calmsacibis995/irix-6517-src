#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_config.c,v 1.7 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/invent.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * print_hwcmp_info()
 */
void
print_hwcmp_info(int module, char *location, hw_component_t *hwcp, FILE *ofp)
{
	if (!hwcp) {
		return;
	}
	if (module) {
		fprintf(ofp, "%6d  ", module);
	}
	else {
		fprintf(ofp, "    --  ");
	}

	if (hwcp->c_name) {
		fprintf(ofp, "%-14s  ", hwcp->c_name);
	}
	else {
		fprintf(ofp, "NA              ");
	}
	if (location) {
		fprintf(ofp, "%12s  ", location);
	}
	else {
		fprintf(ofp, "              ");
	}
	if (hwcp->c_serial_number) {
		fprintf(ofp, "%12s  ", hwcp->c_serial_number);
	}
	else {
		fprintf(ofp, "          NA  ");
	}
	if (hwcp->c_part_number) {
		fprintf(ofp, "%13s  ", hwcp->c_part_number);
	}
	else {
		fprintf(ofp, "           NA  ");
	}
	if (hwcp->c_revision) {
		fprintf(ofp, "%3s\n", hwcp->c_revision);
	}
	else {
		fprintf(ofp, " NA\n");
	}
}

/*
 * print_hwcmp_inventory()
 */
void
print_hwcmp_inventory(
	int module, 
	char *location, 
	hw_component_t *hwcp, 
	FILE *ofp)
{
	if (!hwcp || (hwcp->c_category != HW_INVENTORY)) {
		return;
	}
	if (module) {
		fprintf(ofp, "%6d  ", module);
	}
	else {
		fprintf(ofp, "    --  ");
	}
	if (hwcp->c_name) {
		fprintf(ofp, "%-14s  ", hwcp->c_name);
	}
	else {
		fprintf(ofp, "                ");
	}
	if (location) {
		fprintf(ofp, "%12s  ", location);
	}
	else {
		fprintf(ofp, "              ");
	}
	if (hwcp->c_serial_number) {
		fprintf(ofp, "%12s  ", hwcp->c_serial_number);
	}
	else {
		fprintf(ofp, "              ");
	}
	if (hwcp->c_part_number) {
		fprintf(ofp, "%13s  ", hwcp->c_part_number);
	}
	else {
		fprintf(ofp, "               ");
	}
	if (hwcp->c_revision) {
		fprintf(ofp, "%3s\n", hwcp->c_revision);
	}
	else {
		fprintf(ofp, "   \n");
	}
}

/*
 * print_module_info()
 */
void
print_module_info(hw_component_t *m, FILE *ofp)
{
	if (m->c_serial_number) {
		fprintf(ofp, "%6d  MODULEID                      %12s\n", 
			atoi(m->c_name), m->c_serial_number);
	}
	else {
		fprintf(ofp, "%6d  MODULEID                                NA\n",
			atoi(m->c_name)); 
	}
}

/* 
 * print_hwcomponent()
 */
void
print_hwcomponent(hw_component_t *hcp, FILE *ofp)
{
	fprintf(ofp, "%5d  ", hcp->c_level);
	fprintf(ofp, "%14s  ", hcp->c_name);
	fprintf(ofp, "%8s  ", hcp->c_location);
	fprintf(ofp, "%10s  ", hcp->c_serial_number);
	fprintf(ofp, "%10s  ", hcp->c_part_number);
	fprintf(ofp, "%8s\n", hcp->c_revision);
}

/*
 * print_hwconfig()
 */
void
print_hwconfig(hwconfig_t *hcp, FILE *ofp, int flags)
{
	int module= 0;
	hw_component_t *hwcmp;

	fprintf(ofp, "MODULE  BOARD                   SLOT     SERIAL_NO     "
				 "   PART_NO  REV\n");
	fprintf(ofp, "=================================================="
				 "====================\n");

	hwcmp = hcp->h_hwcmp_root->c_cmplist;

	while (hwcmp) {
		if (K_IP == 27) {
			if (hwcmp->c_category == HW_MODULE) {
				module = atoi(hwcmp->c_name);
				print_module_info(hwcmp, ofp);	
				hwcmp = next_hwcmp(hwcmp);
				continue;
			}
		}
		if (hwcmp->c_category == HW_INVENTORY) {
			print_hwcmp_inventory(module, hwcmp->c_location, hwcmp, ofp);
		}
		else if ((flags & C_FULL) || (hwcmp->c_category <= HW_BOARD)) {
			print_hwcmp_info(module, hwcmp->c_location, hwcmp, ofp);
		}
		hwcmp = next_hwcmp(hwcmp);
	}
	fprintf(ofp, "=================================================="
				 "====================\n");
}

/*
 * config_cmd() -- Run the 'config' command.  
 */
int
config_cmd(command_t cmd)
{
	int mode;
	hwconfig_t *hcp;
	k_uint_t flag = 0;

	if (cmd.nargs) {
		kl_get_value(cmd.args[0], &mode, 0, &flag);
		if (KL_ERROR) {
			return(1);
		}
	}
    hcp = kl_alloc_hwconfig(K_TEMP|STRINGTAB_FLG);
	kl_get_hwconfig(hcp, (int)flag);
	print_hwconfig(hcp, cmd.ofp, cmd.flags);
	kl_free_hwconfig(hcp);
	return(0);
}

#define _CONFIG_USAGE "[-f] [-w outfile] [flag]"

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
	return (C_MAYBE|C_WRITE|C_FULL);
}
