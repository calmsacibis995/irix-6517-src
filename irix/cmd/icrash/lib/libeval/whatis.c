#ident  "$Header: "

#include <stdio.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * dw_dowhatis()
 */
void
dw_dowhatis(command_t cmd)
{
	int found = 0;
	Dwarf_Off off;
	Dwarf_Half tag;
	dw_type_t *dtp;
	dw_type_info_t *t;

	t = kl_alloc_block(sizeof(dw_type_info_t), K_TEMP);

	if (cmd.nargs) {

		if (dtp = dw_lkup(cmd.args[0], DW_TAG_variable)) {
			dw_type_info(dtp->dt_off, t, 0);
			if (t->t_type_str) {
				fprintf(cmd.ofp, "%s %s;\n", t->t_type_str, t->t_name);
			}
			else {
				fprintf(cmd.ofp, "<UNKNOWN TYPE> %s;\n", t->t_name);
			}
			found++;
		}
		if (dtp = dw_lkup(cmd.args[0], 0)) {
			dw_type_info(dtp->dt_off, t, 0);
			if (found) {
				fprintf(cmd.ofp, "---------------------------------------\n");
			}
			switch (dtp->dt_tag) {
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				break;

			case DW_TAG_enumeration_type:
				fprintf(cmd.ofp, "%s is an DW_TAG_enumeration_type\n",
					dtp->dt_name);
				break;

			case DW_TAG_typedef:
				fprintf(cmd.ofp, "typedef %s %s;\n", 
					t->t_type_str, t->t_name);
				break;

			default:
				break;
			}
		}
		if (!found) {
			fprintf(cmd.ofp, "%s not found in symbol table!\n", cmd.args[0]);
		}
		dw_free_type_info(t);
		return;
	}

	if (cmd.flags & C_ALL) {

		fprintf(cmd.ofp, "STRUCT DEFINITIONS:\n\n");
		dtp = (dw_type_t *)STP->st_struct;

		while (dtp) {

			dw_type_info(dtp->dt_off, t, 0);
			if (cmd.flags & C_FULL) {
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, "%s\n", t->t_type_str);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "UNION DEFINITIONS:\n\n");
		dtp = (dw_type_t *)STP->st_union;

		while (dtp) {

			dw_type_info(dtp->dt_off, t, 0);
			if (cmd.flags & C_FULL) {
				dw_dump_struct((k_ptr_t)NULL, t, 0, cmd.flags, cmd.ofp);
				fprintf(cmd.ofp, "\n");
			}
			else {
				fprintf(cmd.ofp, "%s\n", t->t_type_str);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "TYPEDEF DEFINITIONS:\n\n");
		dtp = (dw_type_t *)STP->st_typedef;

		while (dtp) {
			dw_type_info(dtp->dt_off, t, 0);
			fprintf(cmd.ofp, "typedef %s %s;\n", t->t_type_str, t->t_name);
			dtp = (dw_type_t *)dtp->dt_next;
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "VARIABLE DEFINITIONS:\n\n");
		dtp = (dw_type_t *)STP->st_var_ptr;

		while (dtp) {
			dw_type_info(dtp->dt_off, t, 0);
			if (t->t_type_str) {
				fprintf(cmd.ofp, "%s %s;\n", t->t_type_str, t->t_name);
			}
			else {
				fprintf(cmd.ofp, "<UNKNOWN TYPE> %s;\n", t->t_name);
			}
			dtp = (dw_type_t *)dtp->dt_next;
		}
		return;
	}
}
