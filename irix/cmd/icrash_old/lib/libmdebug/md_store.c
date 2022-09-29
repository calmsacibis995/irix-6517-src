#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_store.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
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
 * Name: md_create_type_info_sub()
 * Func: Create a sub-information structure with all the variables
 *       initialized properly.
 */
md_type_info_sub_t *
md_create_type_info_sub()
{
	md_type_info_sub_t *mp;

	mp = (md_type_info_sub_t *)malloc(md_type_info_sub_size);
	bzero(mp, md_type_info_sub_size);
	return (mp);
}

/*
 * Name: md_insert_type_info_sub()
 * Func: Insert a sub-information structure into the chain for the
 *       information type.  This has to be done so that as you decend
 *       the list, you always insert at the bottom.
 */
void
md_insert_type_info_sub(md_type_info_t *m1, md_type_info_sub_t *m2)
{
	md_type_info_sub_t *mt;

	if (!m1->m_type_sub) {
		m1->m_type_sub = m2;
		m2->mi_sub_next = (md_type_info_sub_t *)NULL;
	} else {
		if (m2->mi_flags & MI_ARRAY) {
			m2->mi_sub_next = m1->m_type_sub;
			m1->m_type_sub = m2;
		} else {
			for (mt = m1->m_type_sub; mt->mi_sub_next; mt = mt->mi_sub_next) {
				/* do nothing */
			}
			mt->mi_sub_next = m2;
		}
	}
}

/*
 * Name: md_store_tq_info()
 * Func: Store TQ information from a TIR structure.  Return the results.
 *       This function works in conjunction with md_store_type_info().
 */
void
md_store_tq_info(md_type_info_t *pt, pAUXU *ppaux, pAUXU *ppaux2,
	unsigned tq, long *piaux)
{
	md_type_info_sub_t *pt2;

	if (tq == tqNil) {
		return;
	}

	if (tq == tqPtr) {
		pt2 = md_create_type_info_sub();
		pt2->mi_flags |= MI_POINTER;
		md_insert_type_info_sub(pt, pt2);
	} else if (tq == tqProc) {
		pt2 = md_create_type_info_sub();
		pt2->mi_flags |= MI_FUNCTION;
		md_insert_type_info_sub(pt, pt2);
	} else if (tq == tqArray) {
		*ppaux = st_paux_iaux(*piaux);
		*piaux = *piaux + 1;
		if ((*ppaux)->rndx.rfd == ST_RFDESCAPE) {
			*ppaux2 = st_paux_iaux(*piaux);
			*piaux = *piaux + 1;
		}
		*ppaux = st_paux_iaux(*piaux);
		*piaux = *piaux + 1;
		*ppaux2 = st_paux_iaux(*piaux);
		*piaux = *piaux + 1;
		pt2 = md_create_type_info_sub();
		pt2->mi_flags |= MI_ARRAY;
		pt2->mi_array_low = (*ppaux)->dnLow;
		pt2->mi_array_high = (*ppaux2)->dnHigh;
		*ppaux2 = st_paux_iaux(*piaux);
		*piaux = *piaux + 1;
		pt2->mi_array_width = (*ppaux2)->width;
		md_insert_type_info_sub(pt, pt2);
	}
}

/*
 * Name: md_store_type_info()
 * Func: Store specific type information for each field in each list,
 *       depending on the type and what needs to be saved.
 */
void
md_store_type_info(pSYMR ps, md_type_t *ptr, long isym)
{
	TIR ti;
	pSYMR ps2;
	pCFDR pcfd;
	pAUXU paux, paux1, paux2;
	md_type_info_t *pttype;
	md_type_info_sub_t *ptsubtype;
	long iaux, fcomplex = 0, j, k;

	pttype = (md_type_info_t *)malloc(md_type_info_size);
	pttype->m_st_type = ps->st;
	pttype->m_type_sub = (md_type_info_sub_t *)NULL;
	switch (ps->st) {
		case stProc:
		case stStaticProc:
		case stMemberProc:
			_md_st_setfd(ptr->m_ifd);
			pcfd = st_pcfd_ifd(ptr->m_ifd);
			paux = st_paux_iaux(ps->index);
			ps2 = &pcfd->psym[paux->isym - 1];
			pttype->m_func_lowpc = ps->value;
			ptr->m_type = pttype;
			return;

		case stStruct:
		case stUnion:
		case stEnum:
			pttype->m_size = ps->value;
			ptr->m_type = pttype;
			return;

		case stLabel:
		case stGlobal:
			pttype->m_pc = ps->value;
			ptr->m_type = pttype;
			return;

		case stIndirect:
		case stMember:
		case stTypedef:
		case stStatic:
			if (ps->st == stMember) {
				pttype->m_offset = ps->value;
			} else {
				pttype->m_offset = 0;
			}

			if (ps->st != stMember) {
				pttype->m_pc = ps->value;
			} else {
				pttype->m_pc = 0;
			}

			pttype->m_size = -1;

			if (isym < 0) {
				return;
			}

			iaux = ps->index;
			paux = st_paux_iaux(iaux++);
			ti   = paux->ti;

			if (ti.fBitfield) {
				paux = st_paux_iaux(iaux++);
				ptsubtype = md_create_type_info_sub();
				ptsubtype->mi_flags |= MI_BIT_FIELD;
				ptsubtype->mi_bit_width = paux->width;
				for (j = 1; j < 32; j *= 2) {
					k = j * 8;
					if (paux->width <= k) {
						ptsubtype->mi_byte_width = j;
						j = 64;
					}
				}
				md_insert_type_info_sub(pttype, ptsubtype);
			}

			if ((ti.bt == btTypedef) || (ti.bt == btStruct) ||
				(ti.bt == btUnion) || (ti.bt == btEnum)) {
					paux1 = st_paux_iaux(iaux++);
					if (paux1->rndx.rfd == ST_RFDESCAPE) {
						iaux++;
					}
					fcomplex = 1;
			}

			if (ti.bt == btRange) {
				paux1 = st_paux_iaux(iaux);
				if (paux1->rndx.rfd == ST_RFDESCAPE) {
					iaux++;
				}
				iaux += 3;
			}

			while (1) {
				md_store_tq_info(pttype, &paux, &paux2, ti.tq5, &iaux);
				md_store_tq_info(pttype, &paux, &paux2, ti.tq4, &iaux);
				md_store_tq_info(pttype, &paux, &paux2, ti.tq3, &iaux);
				md_store_tq_info(pttype, &paux, &paux2, ti.tq2, &iaux);
				md_store_tq_info(pttype, &paux, &paux2, ti.tq1, &iaux);
				md_store_tq_info(pttype, &paux, &paux2, ti.tq0, &iaux);
				if (ti.continued) {
					paux = st_paux_iaux(iaux++);
					ti   = paux->ti;
				} else {
					break;
				}
			}

			if (ti.bt <= mbtsize) {
				if (pttype->m_size < 0) {
					pttype->m_size = mbtarray[ti.bt].size;
				}
			}

			if (fcomplex) {
				ptsubtype = md_create_type_info_sub();
				if (paux1->rndx.rfd == ST_RFDESCAPE) {
					pcfd = st_pcfd_ifd(ptr->m_ifd);
					ptsubtype->mi_flags |= MI_RANGE;
					ptsubtype->mi_range_ifd = pcfd->prfd[paux1[1].isym];
					ptsubtype->mi_range_isym = paux1->rndx.index;
					md_insert_type_info_sub(pttype, ptsubtype);
				} else {
					ptsubtype->mi_flags |= MI_RANGE;
					ptsubtype->mi_range_ifd = paux1->rndx.rfd;
					ptsubtype->mi_range_isym = paux1->rndx.index;
					md_insert_type_info_sub(pttype, ptsubtype);
				}
			}

			ptsubtype = md_create_type_info_sub();
			ptsubtype->mi_flags |= MI_BT;
			ptsubtype->mi_bt_type = ti.bt;
			md_insert_type_info_sub(pttype, ptsubtype);
			ptr->m_type = pttype;
			return;
	}
	free(pttype);
	return;
}

/*
 * Name: md_store_type()
 * Func: Store type information for each field in each list.
 */
void
md_store_type(mdebugtab_t *md, pCFDR pcfd, long ifd, long isym, pSYMR ps)
{
	pSYMR ps2;
	long isym2, k;
	md_type_t *ptr, *ptr2;

	if (((ps->st == stLabel) || (ps->st == stGlobal) ||
		(ps->st == stStatic) || (ps->st == stProc) ||
		(ps->st == stStaticProc) || (ps->st == stMemberProc))
		&& (!ps->iss)) {
			return;
			
	}

	ptr = (md_type_t *)malloc(md_type_size);
	ptr->m_name   = (isym < 0 ? st_str_extiss(ps->iss) : st_str_iss(ps->iss));
	ptr->m_next   = (md_type_t *)NULL;
	ptr->m_member = (md_type_t *)NULL;
	ptr->m_type   = (md_type_info_t *)NULL;
	ptr->m_isym   = isym;
	ptr->m_ifd    = ifd;

	/*
	 * Store the type information.
	 */
	md_store_type_info(ps, ptr, isym);

	/*
	 * Store this main pointer on the appropriate list.  Note that we
	 * store unions on the structures list, and blocks on the structure
	 * list.
	 */
	if ((ps->st == stProc) || (ps->st == stStaticProc) ||
		(ps->st == stMemberProc)) {
		if (!md->m_proc) {
			md->m_proc = ptr;
		} else {
			ptr->m_next = md->m_proc;
			md->m_proc = ptr;
		}
		proc_cnt++;
	} else if ((ps->st == stStruct) || (ps->st == stUnion)) {
		if (!md->m_struct) {
			md->m_struct = ptr;
		} else {
			ptr->m_next = md->m_struct;
			md->m_struct = ptr;
		}
	} else if (ps->st == stEnum) {
		if (!md->m_enum) {
			md->m_enum = ptr;
		} else {
			ptr->m_next = md->m_enum;
			md->m_enum = ptr;
		}
	} else if (ps->st == stTypedef) {
		if (!md->m_typedef) {
			md->m_typedef = ptr;
		} else {
			ptr->m_next = md->m_typedef;
			md->m_typedef = ptr;
		}
	} else {
		if (!md->m_variable) {
			md->m_variable = ptr;
		} else {
			ptr->m_next = md->m_variable;
			md->m_variable = ptr;
		}
	}

	/*
	 * Now, store sub-structures as main structures, and store members
	 * on the main ptr list.
	 */
	if ((ps->st == stStruct) || (ps->st == stUnion) || (ps->st == stEnum)) {
		if ((ptr->m_name) && (!strcmp(ptr->m_name, "sema_s"))) {
			k = 0;
		}
		for (isym2 = isym + 1; isym2 < ps->index; isym2++) {
			ps2 = &pcfd->psym[isym2];
			if ((ps2->st == stStruct) || (ps2->st == stUnion) ||
				(ps2->st == stEnum)) {
				md_store_type(md, pcfd, ifd, isym2, ps2);
				isym2 = ps2->index - 1;
			} else if (ps2->st == stMember) {
				ptr2 = (md_type_t *)malloc(sizeof(md_type_t));
				ptr2->m_member = (md_type_t *)NULL;
				ptr2->m_type = (md_type_info_t *)NULL;
				ptr2->m_name = st_str_iss(ps2->iss);
				ptr2->m_isym = isym2;
				ptr2->m_ifd = ifd;
				if (!(ptr->m_member)) {
					ptr->m_member = ptr2;
				} else {
					ptr->m_member->m_member = ptr2;
					ptr = ptr->m_member;
				}
				md_store_type_info(ps2, ptr2, isym2);
			}
		}
	}
}
