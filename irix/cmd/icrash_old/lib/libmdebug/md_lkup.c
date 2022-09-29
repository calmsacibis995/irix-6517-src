#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_lkup.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
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
 * Name: md_lkup_type_range_type()
 * Func: Look up a md_type_t structure based on the range specified.  Note
 *       that it can be on any list.
 */
md_type_t *
md_lkup_type_range_type(int ifd, int isym, int type)
{
	md_type_t *ptr = (md_type_t *)NULL;

	switch (type) {
		case stStruct:
		case stUnion:
			ptr = mdp->m_struct;
			break;

		case stEnum:
			ptr = mdp->m_enum;
			break;

		case stTypedef:
			ptr = mdp->m_typedef;
			break;
	}

	if (!ptr) {
		return ((md_type_t *)NULL);
	}

	for (; ptr; ptr = ptr->m_next) {
		if ((ptr->m_ifd == ifd) && (ptr->m_isym == isym)) {
			return (ptr);
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_range()
 * Func: Look up a md_type_t structure based on the range specified.  Note
 *       that it can be on any list.
 */
md_type_t *
md_lkup_type_range(int ifd, int isym)
{
	md_type_t *ptr;

	for (ptr = mdp->m_struct; ptr; ptr = ptr->m_next) {
		if ((ptr->m_ifd == ifd) && (ptr->m_isym == isym)) {
			return (ptr);
		}
	}
	for (ptr = mdp->m_typedef; ptr; ptr = ptr->m_next) {
		if ((ptr->m_ifd == ifd) && (ptr->m_isym == isym)) {
			return (ptr);
		}
	}
	for (ptr = mdp->m_enum; ptr; ptr = ptr->m_next) {
		if ((ptr->m_ifd == ifd) && (ptr->m_isym == isym)) {
			return (ptr);
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_proc()
 * Func: Return a md_type_t structure that is a procedure that either
 *       matches a given name or falls within a PC range.
 */
md_type_t *
md_lkup_type_proc(char *name, unsigned long pc)
{
	md_type_t *ptr;

	if (!name) {
		for (ptr = mdp->m_proc; ptr; ptr = ptr->m_next) {
			if ((ptr->m_type->m_func_lowpc <= pc) &&
				(ptr->m_type->m_func_highpc >= pc)) {
					return (ptr);
			}
		}
	} else {
		for (ptr = mdp->m_proc; ptr; ptr = ptr->m_next) {
			if ((ptr->m_name) && (!strcmp(ptr->m_name, name))) {
				return (ptr);
			}
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_typedef()
 * Func: Return a md_type_t structure that is a typedef based on the
 *       name requested or the PC address given.
 */
md_type_t *
md_lkup_type_typedef(char *name)
{
	md_type_t *ptr;

	if (!name) {
		return ((md_type_t *)NULL);
	}

	for (ptr = mdp->m_typedef; ptr; ptr = ptr->m_next) {
		if ((ptr->m_name) && (!strcmp(ptr->m_name, name))) {
			return (ptr);
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_struct()
 * Func: Return a md_type_t structure that is a struct based on the
 *       name requested or the PC address given.
 */
md_type_t *
md_lkup_type_struct(char *name)
{
	md_type_t *ptr;

	if (!name) {
		return ((md_type_t *)NULL);
	}

	for (ptr = mdp->m_struct; ptr; ptr = ptr->m_next) {
		if ((ptr->m_name) && (!strcmp(ptr->m_name, name))) {
			return (ptr);
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_enum()
 * Func: Return a md_type_t structure that is a enum based on the
 *       name requested or the PC address given.
 */
md_type_t *
md_lkup_type_enum(char *name)
{
	md_type_t *ptr;

	if (!name) {
		return ((md_type_t *)NULL);
	}

	for (ptr = mdp->m_enum; ptr; ptr = ptr->m_next) {
		if ((ptr->m_name) && (!strcmp(ptr->m_name, name))) {
			return (ptr);
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type_variable()
 * Func: Return a md_type_t structure that is a variable based on the
 *       name requested or the PC address given.
 */
md_type_t *
md_lkup_type_variable(char *name, unsigned long pc)
{
	md_type_t *ptr;

	if (!name) {
		for (ptr = mdp->m_variable; ptr; ptr = ptr->m_next) {
			if (ptr->m_type->m_pc == pc) {
				return (ptr);
			}
		}
	} else {
		for (ptr = mdp->m_variable; ptr; ptr = ptr->m_next) {
			if ((ptr->m_name) && (!strcmp(ptr->m_name, name))) {
				return (ptr);
			}
		}
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup_type()
 * Func: Look up a specific type based on the st value in the pSYMR.
 */
md_type_t *
md_lkup_type(char *name, long type)
{
	switch (type) {
		case stProc:
			return (md_lkup_type_proc(name, 0));

		case stTypedef:
			return (md_lkup_type_typedef(name));

		case stStruct:
		case stUnion:
			return (md_lkup_type_struct(name));

		case stEnum:
			return (md_lkup_type_enum(name));

		default:
			return (md_lkup_type_variable(name, 0));
	}
	return ((md_type_t *)NULL);
}

/*
 * Name: md_lkup()
 * Func: Look up a md_type_t structure in the loop set through all of
 *       the lists.
 */
md_type_t *
md_lkup(char *name)
{
	md_type_t *ptr;

	if (!name) {
		return;
	}

	if (ptr = md_lkup_type(name, stStruct)) {
		return (ptr);
	}
	if (ptr = md_lkup_type(name, stEnum)) {
		return (ptr);
	}
	if (ptr = md_lkup_type(name, stTypedef)) {
		return (ptr);
	}
	if (ptr = md_lkup_type(name, stLabel)) {
		return (ptr);
	}
	if (ptr = md_lkup_type(name, stProc)) {
		return (ptr);
	}
}

/*
 * md_var_to_type() -- Converts a variable to a type.
 *
 *                     This is a dummy routine that always returns
 *                     failure (NULL). If we can figure out how to 
 *                     get type information for global variables, the
 *                     conversion can go here.
 */
type_t *
md_var_to_type(md_type_t *t)
{
	return((type_t *)NULL);
}

/*
 * md_lkup_member()
 */
type_t *
md_lkup_member(mdebugtab_t *md, md_type_t *mt, char *name)
{
	type_t *tp;
	md_type_t *mt2;

	if (!(mt->m_member)) {
		return ((type_t *)NULL);
	}
	for (mt2 = mt->m_member; mt2; mt2 = mt2->m_member) {
		if ((MD_VALID_STR(mt2->m_name)) && (!(strcmp(mt2->m_name, name)))) {
			tp = (type_t *)alloc_block(sizeof(type_t), B_TEMP);
			tp->flag = MDEBUG_TYPE_FLAG;
			tp->t_ptr = mt2;
			return (tp);
		}
	}
	return ((type_t *)NULL);
}
