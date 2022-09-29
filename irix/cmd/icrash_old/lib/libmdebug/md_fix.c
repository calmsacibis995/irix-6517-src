#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_fix.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
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
 * Name: md_proc_sort()
 * Func: Sort the function procedures based on the low PC address.
 */
int
md_proc_sort(md_type_t *m1, md_type_t *m2)
{
	return (m1->m_type->m_func_lowpc - m2->m_type->m_func_lowpc);
}

/*
 * Name: md_get_field_size_loop()
 * Func: Get the field size by looping around on sub-structure items.
 */
int
md_get_field_size_loop(mdebugtab_t *md, long ifd, long sym)
{
	TIR ti;
	pSYMR ps;
	pCFDR pcfd;
	pAUXU paux;
	long iaux, size = 0;

	_md_st_setfd(ifd);
	pcfd = st_pcfd_ifd(ifd);
	ps = &pcfd->psym[sym];
	if ((ps->st == stStruct) || (ps->st == stUnion) || (ps->st == stEnum)) {
			size = ps->value;
	} else if ((ps->st == stIndirect) || (ps->st == stTypedef)) {
		iaux = ps->index;
		paux = st_paux_iaux(iaux++);
		ti = paux->ti;
		if (ti.fBitfield) {
			paux = st_paux_iaux(iaux++);
			size = paux->width;
		} else {
			if (ti.bt <= mbtsize) {
				size = mbtarray[ti.bt].size;
			}
		}
		if (size < 0) {
			if ((ti.bt == btStruct) || (ti.bt == btUnion) ||
				(ti.bt == btTypedef) || (ti.bt == btEnum)) {
					paux = st_paux_iaux(iaux++);
					if (paux->rndx.rfd == ST_RFDESCAPE) {
						size = md_get_field_size_loop(md,
								pcfd->prfd[paux[1].isym], paux->rndx.index);
					} else {
						size = md_get_field_size_loop(md,
								paux->rndx.rfd, paux->rndx.index);
					}
			}
		}
	} else {
		size = ps->value;
	}
	return (size);
}

/*
 * Name: md_fix_type_info_size()
 * Func: Fix up type information sizes based on the sub-information
 *       structures.  Note:  I wish this type of hell on no man.
 */
long
md_fix_type_info_size(mdebugtab_t *md, md_type_t *mp)
{
	int tsize, size = 0, nc = 0, index;
	md_type_info_t *p = mp->m_type;
	md_type_info_sub_t *pt, *pt2;

	if (!p) {
		return (-1);
	}

	if (p->m_size >= 0) {
		return (p->m_size);
	}

	for (pt = p->m_type_sub; pt; pt = pt->mi_sub_next) {
		if (pt->mi_flags & MI_BIT_FIELD) {
			return (pt->mi_bit_width);
		} else if (pt->mi_flags & (MI_POINTER|MI_FUNCTION)) {
			size = 32;
			nc = 1;
		} else if (pt->mi_flags & MI_ARRAY) {
			index = (pt->mi_array_high - pt->mi_array_low + 1);
			if (!size) {
				size = index * pt->mi_array_width;
			} else {
				size *= (index * pt->mi_array_width);
			}
		} else if (pt->mi_flags & MI_BT) {
			if (pt->mi_type_ptr) {
				tsize = pt->mi_type_ptr->m_type->m_size;
				if ((!nc) && (!size)) {
					return (tsize);
				} else if ((!nc) && (size)) {
					return (tsize * size);
				} else if (nc) {
					return (size);
				}
			} else {
				if (mbtarray[pt->mi_bt_type].size >= 0) {
					tsize = mbtarray[pt->mi_bt_type].size;
					if ((!nc) && (!size)) {
						return (tsize);
					} else if ((!nc) && (size)) {
						return (tsize * size);
					} else if (nc) {
						return (size);
					}
				}
			}
		}
	}
	return (size);
}

/*
 * Name: md_fix_type_info_type()
 * Func: Loop through the set of type pointers and setting the type
 *       pointers to look at the right type on sub-structures.
 */
void
md_fix_type_info_type(mdebugtab_t *md, md_type_t *ptr)
{
	TIR ti;
	pSYMR ps;
	pAUXU paux, paux1;
	pCFDR pcfd;
	char *buf, *bufx;
	int tifd, tisym, done = FALSE, sdone = FALSE, iaux;
	md_type_info_sub_t *mir, *mib;
	md_type_t *mtp;

	/*
	 * This loop is a little tricky.  We first find the lowest
	 * common range point here, being sure to handle indirects
	 * as well as typedefs of typedefs of structures.
	 * 
	 * The way we handle this is to find the ifd/isym of the
	 * lowest range point, get the type, and find the base pointer
	 * (if one exists) in the appropriate list.
	 */
	if (!(mir = md_is_range(ptr))) {
		ptr->m_type->m_size = md_fix_type_info_size(md, ptr);
		if (mib = md_is_bt(ptr)) {
			mib->mi_type_name = strdup(mbtarray[mib->mi_bt_type].ptr);
		}
		return;
	}
	tifd = mir->mi_range_ifd;
	tisym = mir->mi_range_isym;
	if ((MD_VALID_STR(ptr->m_name)) && (!strcmp(ptr->m_name, "b_offset"))) {
		done = 0;
	}
	buf  = (char *)malloc(128);
	while (!done) {
		_md_st_setfd(tifd);
		pcfd = st_pcfd_ifd(tifd);
		ps   = &pcfd->psym[tisym];
		done = 1;
		switch (ps->st) {
			case stEnum:
				if ((!sdone) && (bufx = st_str_iss(ps->iss)) &&
					(bufx[0] != '\0')) {
						sdone = TRUE;
						sprintf(buf, "enum %s", st_str_iss(ps->iss));
				} else if (!sdone) {
					sdone = TRUE;
					sprintf(buf, "enum");
				}
				break;

			case stStruct:
				if ((!sdone) && (bufx = st_str_iss(ps->iss)) &&
					(bufx[0] != '\0')) {
						sdone = TRUE;
						sprintf(buf, "struct %s", st_str_iss(ps->iss));
				} else if (!sdone) {
					sdone = TRUE;
					sprintf(buf, "struct");
				}
				break;

			case stUnion:
				if ((!sdone) && (bufx = st_str_iss(ps->iss)) &&
					(bufx[0] != '\0')) {
						sdone = TRUE;
						sprintf(buf, "union %s", st_str_iss(ps->iss));
				} else if (!sdone) {
					sdone = TRUE;
					sprintf(buf, "union");
				}
				break;

			case stTypedef:
				if ((!sdone) && (bufx = st_str_iss(ps->iss)) &&
					(bufx[0] != '\0')) {
						sdone = TRUE;
						sprintf(buf, "%s", st_str_iss(ps->iss));
				}
				/* fall through */

			default:
				done = 0;
				iaux = ps->index;
				paux = st_paux_iaux(iaux++);
				ti   = paux->ti;
				if ((ti.bt == btTypedef) || (ti.bt == btStruct) ||
					(ti.bt == btUnion) || (ti.bt == btEnum)) {
					paux1 = st_paux_iaux(iaux++);
					if (paux1->rndx.rfd == ST_RFDESCAPE) {
						tifd = pcfd->prfd[paux1[1].isym];
					} else {
						tifd = paux1->rndx.rfd;
					}
					tisym = paux1->rndx.index;
				} else {
					if (!sdone) {
						sdone = TRUE;
						sprintf(buf, "%s", mbtarray[ti.bt].ptr);
					}
					done = 1;
				}
		}
	}

	switch (ps->st) {
		case stStruct:
		case stUnion:
			mtp = md_lkup_type_range_type(tifd, tisym, stStruct);
			break;

		case stEnum:
			mtp = md_lkup_type_range_type(tifd, tisym, stEnum);
			break;

		case stTypedef:
			mtp = md_lkup_type_range_type(tifd, tisym, stTypedef);
			break;
	}

	/*
	 * Now set the size and type pointer appropriately.
	 */
	if ((mtp) && (mib = md_is_bt(ptr))) {
		mib->mi_type_ptr = mtp;
		mib->mi_type_name = buf;
	}
	ptr->m_type->m_size = md_fix_type_info_size(md, ptr);
}

/*
 * Name: md_fix_type_info()
 * Func: Fix any type information that could not be set right on the
 *       first pass through a structure.
 */
void
md_fix_type_info(mdebugtab_t *md)
{
	int i, j, tcnt;
	FILE *tfp;
	md_type_t *ptr, *ptr2, *ptt, *pptr;

	for (ptr = md->m_typedef; ptr; ptr = ptr->m_next) {
		md_fix_type_info_type(md, ptr);
	}

	for (ptr = md->m_struct; ptr; ptr = ptr->m_next) {
		if (ptr->m_member != (md_type_t *)NULL) {
			for (ptr2 = ptr->m_member; ptr2; ptr2 = ptr2->m_member) {
				if ((ptr2->m_name) && (!strcmp(ptr2->m_name, "av_forw"))) {
					tcnt = 0;
				}
				md_fix_type_info_type(md, ptr2);
			}
		}
	}

	for (ptr = md->m_enum; ptr; ptr = ptr->m_next) {
		if (ptr->m_member != (md_type_t *)NULL) {
			for (ptr2 = ptr->m_member; ptr2; ptr2 = ptr2->m_member) {
				md_fix_type_info_type(md, ptr2);
			}
		}
	}

	/*
	 * We have to fix proc pointers, because you can only rely on the
	 * lowpc being correct.  Once we line up all of the PC addresses,
	 * we can re-order them, and set the highpc values in the chain one
	 * by one.
	 */
	pptr = (md_type_t *)malloc(md_type_size * proc_cnt);
	for (ptr = md->m_proc, i = 0; ptr; i++) {
		pptr[i].m_member = (md_type_t *)NULL;
		pptr[i].m_next = (md_type_t *)NULL;
		pptr[i].m_type = ptr->m_type;
		pptr[i].m_name = ptr->m_name;
		pptr[i].m_isym = ptr->m_isym;
		pptr[i].m_ifd = ptr->m_ifd;
		ptr2 = ptr;
		ptr = ptr->m_next;
		free(ptr2);
	}
	qsort(pptr, i, md_type_size, (int (*)())md_proc_sort);

	/*
	 * Note: this is totally slow code to clean-up duplicates.  I should
	 * sort the linked list with an insertion sort and drop duplicates as
	 * I go through.
	 */
	md->m_proc = (md_type_t *)NULL;
	for (j = 0; j < i; j++) {
		if (j + 1 < i) {
			pptr[j].m_type->m_func_highpc =
				pptr[j+1].m_type->m_func_lowpc - sizeof(Elf32_Addr);
			pptr[j].m_type->m_size =
				pptr[j].m_type->m_func_highpc - pptr[j].m_type->m_func_lowpc;
		}
		if (!md->m_proc) {
			md->m_proc = &pptr[j];
		} else {
			pptr[j].m_next = md->m_proc;
			md->m_proc = &pptr[j];
		}
	}
}
