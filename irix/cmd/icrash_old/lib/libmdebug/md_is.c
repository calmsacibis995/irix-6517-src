#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_is.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
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

/*
 * Name: md_is_bit_field()
 * Func: See whether an MD is a bit field or not.  Return MI.
 */
md_type_info_sub_t *
md_is_bit_field(md_type_t *md)
{
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_BIT_FIELD) {
			return (mi);
		}
	}
	return ((md_type_info_sub_t *)0);
}

/*
 * Name: md_is_function()
 * Func: See whether an MD is a function or not.
 */
int
md_is_function(md_type_t *md)
{
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_FUNCTION) {
			return (1);
		}
	}
	return (0);
}

/*
 * Name: md_is_pointer()
 * Func: See whether an MD is a pointer or not.  Return total number
 *       of pointers.
 */
int
md_is_pointer(md_type_t *md)
{
	int i = 0;
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_POINTER) {
			i++;
		}
	}
	return (i);
}

/*
 * Name: md_is_range()
 * Func: See whether an MD is a range or not.  Return the MI structure
 *       that the range points to.
 */
md_type_info_sub_t *
md_is_range(md_type_t *md)
{
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_RANGE) {
			return (mi);
		}
	}
	return ((md_type_info_sub_t *)NULL);
}

/*
 * Name: md_is_bt()
 * Func: See whether an MD is a BT or not.  Return the MI structure
 *       that the BT points to.
 */
md_type_info_sub_t *
md_is_bt(md_type_t *md)
{
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_BT) {
			return (mi);
		}
	}
	return ((md_type_info_sub_t *)NULL);
}

/*
 * Name: md_is_array()
 * Func: See whether an MD is an array or not.  Return the MI structure
 *       that the array points to.
 */
md_type_info_sub_t *
md_is_array(md_type_t *md)
{
	md_type_info_sub_t *mi;

	for (mi = md->m_type->m_type_sub; mi; mi = mi->mi_sub_next) {
		if (mi->mi_flags & MI_ARRAY) {
			return (mi);
		}
	}
	return ((md_type_info_sub_t *)NULL);
}
