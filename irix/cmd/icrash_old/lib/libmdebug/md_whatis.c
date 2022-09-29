#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_whatis.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <filehdr.h>
#include <fcntl.h>
#include <stdio.h>
#include <sym.h>
#include <symconst.h>
#include <syms.h>
#include <cmplrs/stsupport.h>
#include <elf_abi.h>
#include <elf_mips.h>
#include <libelf.h>
#include <sex.h>
#include <ldfcn.h>
#include <errno.h>

#include "icrash.h"
#include "eval.h"
#include "libmdebug.h"

extern int errno;
extern int debug;
extern int proc_cnt;
extern int mbtsize;
extern mtbarray_t mbtarray[];
extern mdebugtab_t *mdp;

/*
 * Name: md_dowhatis()
 * Func: Print out what a structure looks like in the MDEBUG data.
 */
void
md_dowhatis(command_t cmd)
{
	int found = 0;
	md_type_t *ptr;
	md_type_info_sub_t *mi, *mibt;

	if (cmd.nargs) {
		if (ptr = md_lkup_type(cmd.args[0], stLabel)) {
			/* do nothing for now */
			found++;
		}
		if (ptr = md_lkup_type(cmd.args[0], stStruct)) {
			if (found) {
				fprintf(cmd.ofp, "---------------------------------------\n");
			}
			md_dump_struct((i_ptr_t)NULL, ptr, (md_type_t *)NULL,
				0, cmd.flags, cmd.ofp);
			found++;
		}
		if (ptr = md_lkup_type(cmd.args[0], stTypedef)) {
			if (found) {
				fprintf(cmd.ofp, "---------------------------------------\n");
			}
			if ((mibt = md_is_bt(ptr)) && (mibt->mi_type_name)) {
				fprintf(cmd.ofp, "typedef %s %s;\n", mibt->mi_type_name,
					ptr->m_name);
			}
			found++;
		}
		if (!found) {
			fprintf(cmd.ofp, "%s not found in symbol table!\n", cmd.args[0]);
		}
		return;
	}

	if (cmd.flags & C_ALL) {
		fprintf(cmd.ofp, "STRUCT DEFINITIONS:\n\n");
		for (ptr = mdp->m_struct; ptr; ptr = ptr->m_next) {
			if (ptr->m_type->m_st_type == stStruct) {
				if (cmd.flags & C_FULL) {
					md_dump_struct((i_ptr_t)NULL, ptr, (md_type_t *)NULL,
						0, cmd.flags, cmd.ofp);
				} else {
					if (MD_VALID_STR(ptr->m_name)) {
						fprintf(cmd.ofp, "%s\n", ptr->m_name);
					}
				}
			}
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "UNION DEFINITIONS:\n\n");

		for (ptr = mdp->m_struct; ptr; ptr = ptr->m_next) {
			if (ptr->m_type->m_st_type == stUnion) {
				if (cmd.flags & C_FULL) {
					md_dump_struct((i_ptr_t)NULL, ptr, (md_type_t *)NULL,
						0, cmd.flags, cmd.ofp);
				} else {
					if (MD_VALID_STR(ptr->m_name)) {
						fprintf(cmd.ofp, "%s\n", ptr->m_name);
					}
				}
			}
		}

		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "TYPEDEF DEFINITIONS:\n\n");

		for (ptr = mdp->m_typedef; ptr; ptr = ptr->m_next) {
			mibt = md_is_bt(ptr);
			if (MD_VALID_STR(mibt->mi_type_name)) {
				fprintf(cmd.ofp, "typedef %s %s;\n",
					mibt->mi_type_name,
					ptr->m_name);
			} else {
				fprintf(cmd.ofp, "typedef %s;\n", ptr->m_name);
			}
		}

#if 0
		fprintf(cmd.ofp, "\n\n");
		fprintf(cmd.ofp, "VARIABLE DEFINITIONS:\n\n");
		for (ptr = mdp->m_variable; ptr; ptr = ptr->m_next) {
		}
#endif
	}
}
