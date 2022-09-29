#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_hinv.c,v 1.10 1999/05/25 19:21:38 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * do_hinv()
 */
int
do_hinv(FILE *ofp)
{
	int i;
	char *name;
	hwconfig_t *hcp;
	hw_component_t *hwcp;

	hcp = kl_alloc_hwconfig(K_TEMP|STRINGTAB_FLG);
	kl_get_hwconfig(hcp, INVENT_ALL);

	hwcp = hcp->h_hwcmp_root->c_cmplist;

	while(hwcp) {

		/* Indent for level
		 */
		for (i = 1; i < hwcp->c_level; i++) {
			fprintf(ofp, "  ");
		}
		if (name = hwcp_description(hwcp, 0, K_TEMP)) {
			fprintf(ofp, "%s\n", name);
			kl_free_block(name);
		}
		else if (hwcp->c_name) {
			fprintf(ofp, "%s\n", hwcp->c_name);
		}
		hwcp = next_hwcmp(hwcp);
	}
	return(0);
}

/*
 * hinv_cmd() -- Run the 'hinv' command.  
 */
int
hinv_cmd(command_t cmd)
{
	int i, vhndl;
	invent_rec_t *invent_table;
	invent_data_t *iptr; 
	invstruct_t *inventp;
	char *Name;

	invent_table = get_invent_table(0, INVENT_ALL, K_TEMP);
	for (i = 0; i < invent_table->count; i++) {
		vhndl = invent_table->table[i]->path->prev->vhndl;
		inventp = invent_table->table[i];
		Name = inv_description(inventp->ptr, 0, K_TEMP);
		if (Name) {
			fprintf(cmd.ofp, "%s\n", Name);
			kl_free_block(Name);
		}
		else {
			iptr = (invent_data_t *)inventp->ptr;
			fprintf(cmd.ofp, "vertex=%d", inventp->path->prev->vhndl);
			fprintf(cmd.ofp, ", class=%d", iptr->inv_class);
			fprintf(cmd.ofp, ", type=%d", iptr->inv_type);
			fprintf(cmd.ofp, ", controller=%d", iptr->inv_controller);
			fprintf(cmd.ofp, ", unit=%d", iptr->inv_unit);
			fprintf(cmd.ofp, ", state=%d\n", iptr->inv_state);
		}
	}
	free_invent_table(invent_table);
#ifdef NOT
	return(do_hinv(cmd.ofp));
#endif
	return(0);
}

#define _HINV_USAGE "[-f] [-w outfile]"

/*
 * hinv_usage() -- Print the usage string for the 'hinv' command.
 */
void
hinv_usage(command_t cmd)
{
	CMD_USAGE(cmd, _HINV_USAGE);
}

/*
 * hinv_help() -- Print the help information for the 'hinv' command.
 */
void
hinv_help(command_t cmd)
{
	CMD_HELP(cmd, _HINV_USAGE,
		"Display the hardware inventory information in a system.  This will "
		"report a verbose version of the hardware on the system.");
}

/*
 * hinv_parse() -- Parse the command line arguments for 'hinv'.
 */
int
hinv_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE|C_FULL);
}

