#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_die.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include "icrash.h"
#include "eval.h"
#include "dwarflib.h"
#include "extern.h"

/* Dwarf specific function prototypes
 */
Dwarf_Die dw_die();
dw_die_t *dw_die_info();
dw_type_t *dw_type();

/*
 * dw_print_die()
 */
void
dw_print_die(dw_die_t *d, FILE *ofp)
{
	fprintf(ofp, "===================================================\n");
	fprintf(ofp, "d_name          = %s\n", d->d_name);
	fprintf(ofp, "d_tag           = 0x%hx\n", d->d_tag);
	fprintf(ofp, "d_off           = %lld\n", d->d_off);
	fprintf(ofp, "d_type_off      = %lld\n", d->d_type_off);
	fprintf(ofp, "d_sibling_off   = %lld\n", d->d_sibling_off);
	fprintf(ofp, "d_loc           = %lld\n", d->d_loc);
	fprintf(ofp, "d_byte_size     = %lld\n", d->d_byte_size);
	fprintf(ofp, "d_bit_offset    = %lld\n", d->d_bit_offset);
	fprintf(ofp, "d_bit_size      = %lld\n", d->d_bit_size);
	fprintf(ofp, "d_address_class = %lld\n", d->d_address_class);
	fprintf(ofp, "d_encoding      = %lld\n", d->d_encoding);
	fprintf(ofp, "d_start_scope   = %lld\n", d->d_start_scope);
	fprintf(ofp, "d_upper_bound   = %lld\n", d->d_upper_bound);
	fprintf(ofp, "d_lower_bound   = %lld\n", d->d_lower_bound);
	fprintf(ofp, "d_const         = %lld\n", d->d_const);
	fprintf(ofp, "d_count         = %lld\n", d->d_count);
	fprintf(ofp, "d_external      = %lld\n", d->d_external);
	fprintf(ofp, "d_declaration   = %lld\n", d->d_declaration);
	fprintf(ofp, "===================================================\n\n");
}


/*
 * die_cmd() -- Run the 'die' command to print a DWARF Information Entry.
 */
die_cmd(command_t cmd)
{
	dw_die_t *d;
	Dwarf_Off die_off;

	GET_VALUE(cmd.args[0], &die_off);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_DIE;
		kl_print_error(K);
		return (1);
	}

	d = alloc_block(sizeof(dw_die_t), B_TEMP);
	dw_die_info(die_off, d, 0);
	dw_print_die(d, cmd.ofp);
	dw_free_die(d);
	return(0);
}

#define _DIE_USAGE "[-w outfile] die_addr"

/*
 * die_usage() -- Print the usage string for the 'die' command.
 */
void
die_usage(command_t cmd)
{
	CMD_USAGE(cmd, _DIE_USAGE);
}

/*
 * die_help() -- Print the help information for the 'die' command.
 */
void
die_help(command_t cmd)
{
	CMD_HELP(cmd, _DIE_USAGE,
		"Print out a DWARF information entry at a given die_addr.");
}

/*
 * die_parse() -- Parse the command line arguments for the 'die' command.
 */
int
die_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE);
}
