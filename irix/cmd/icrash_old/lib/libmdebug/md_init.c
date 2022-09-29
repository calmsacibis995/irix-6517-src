#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_init.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
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

/*
 * String definitions used for variables and typedefs (and members of
 * structures).
 */
mtbarray_t mbtarray[] = {
	{ "btNil",					-1 /*  btNil			0 */  },
	{ "btAdr",					32 /*  btAdr32			1 */  },
	{ "char",					 8 /*  btChar			2 */  },
	{ "unsigned char",			 8 /*  btUChar			3 */  },
	{ "short",					16 /*  btShort			4 */  },
	{ "unsigned short",			16 /*  btUShort			5 */  },
	{ "int",					32 /*  btInt32			6 */  },
	{ "unsigned int",			32 /*  btUInt32			7 */  },
	{ "long",					32 /*  btLong32			8 */  },
	{ "unsigned long",			32 /*  btULong32		9 */  },
	{ "float",					32 /*  btFloat			10 */ },
	{ "double",					64 /*  btDouble			11 */ },
	{ "struct",					-1 /*  btStruct			12 */ },
	{ "union",					-1 /*  btUnion			13 */ },
	{ "enum",					-1 /*  btEnum			14 */ },
	{ "typedef",				-1 /*  btTypedef		15 */ },
	{ "range",					-1 /*  btRange			16 */ },
	{ "set of",					-1 /*  btSet			17 */ },
	{ "complex",				-1 /*  btComplex		18 */ },
	{ "double complex",			-1 /*  btDComplex		19 */ },
	{ "indirect type",			-1 /*  btIndirect		20 */ },
	{ "fixed decimal",			-1 /*  btFixedDec		21 */ },
	{ "float decimal",			-1 /*  btFloatDec		22 */ },
	{ "string",					-1 /*  btString			23 */ },
	{ "bit string",				-1 /*  btBit			24 */ },
	{ "picture",				-1 /*  btPicture		25 */ },
	{ "void",					32 /*  btVoid			26 */ },
	{ "ptr mem",              	-1 /*  btPtrMem			27 */ },
	{ "64-bit int",           	64 /*  btInt64			28 */ },
	{ "64-bit unsigned int",  	64 /*  btUInt64			29 */ },
	{ "64-bit long",          	64 /*  btLong64			30 */ },
	{ "64-bit unsigned long", 	64 /*  btULong64   		31 */ },
	{ "long long",            	64 /*  btLongLong64		32 */ },
	{ "unsigned long long",   	64 /*  btULongLong64	33 */ },
	{ "btAdr64",              	64 /*  btAdr64			34 */ },
	{ "class",					-1 /*  btClass			35 */ },
	{ "36",						-1 },
	{ "ptr based variable",		-1 /*  btPtrBasedVar	37 */ },
	{ "38",						-1 },
	{ "39",						-1 },
	{ "40",						-1 },
	{ "41",						-1 },
	{ "42",						-1 },
	{ "43",						-1 },
	{ "44",						-1 },
	{ "45",						-1 },
	{ "46",						-1 },
	{ "47",						-1 },
	{ "48",						-1 },
	{ "49",						-1 },
	{ "50",						-1 },
	{ "51",						-1 },
	{ "52",						-1 },
	{ "53",						-1 },
	{ "54",						-1 },
	{ "55",						-1 },
	{ "56",						-1 },
	{ "57",						-1 },
	{ "58",						-1 },
	{ "59",						-1 },
	{ "60",						-1 },
	{ "61",						-1 },
	{ "62",						-1 },
	{ "63",						-1 },
	{ "64",						-1 },
};

mdebugtab_t *mdp = (mdebugtab_t *)0;
int proc_cnt = 0;
int mbtsize = (sizeof(mbtarray) / sizeof(mbtarray[0]));

/*
 * Name: md_init()
 * Func: Initialize MDEBUG tables of data.  Note that it is more optimal
 *       to store variables, functions and types all at the same time
 *       (one loop, one time).
 *
 *       Note that the variables are stored on two different lists, so we
 *       are double-allocating for each symbol.  Hopefully we'll be able
 *       to clean this up soon.
 */
mdebugtab_t *
md_init()
{
	pSYMR ps;
	pCFDR pcfd;
	pEXTR pext;
	char *sbfile;
	long  ifd, isym, cext, iext;

	mdp = (mdebugtab_t *)malloc(sizeof(mdebugtab_t));
	mdp->m_struct = (md_type_t *)NULL;
	mdp->m_enum = (md_type_t *)NULL;
	mdp->m_typedef = (md_type_t *)NULL;
	mdp->m_proc = (md_type_t *)NULL;
	mdp->m_variable = (md_type_t *)NULL;

	/*
	 * Go through every file descriptor in the symbol table and dump
	 * out appropriate symbol information.
	 */
	for (ifd = 0; ifd < _md_st_ifdmax(); ifd++) {
		_md_st_setfd(ifd);
		pcfd = st_pcfd_ifd(ifd);
		sbfile = st_str_ifd_iss(ifd,pcfd->pfd->rss);
		for (isym = 0; isym < pcfd->pfd->csym; isym++) {
			ps = &pcfd->psym[isym];
			if ((ps->st == stStruct) || (ps->st == stUnion) ||
				(ps->st == stEnum)) {
				md_store_type(mdp, pcfd, ifd, isym, ps);
				isym = ps->index - 1;
			} else if ((ps->st == stProc) || (ps->st == stStaticProc) ||
				(ps->st == stMemberProc) || (ps->st == stTypedef)) {
				md_store_type(mdp, pcfd, ifd, isym, ps);
			}
		}
	}

	/*
	 * Go through all of the externals and store all labels and globals
	 * as variables in our list.  Note that these typically don't have
	 * symbol references, but at least we'll store them appropriately on
	 * our lists.
	 */
	for (iext = 0, cext = _md_st_iextmax(); iext < cext; iext++) {
		pext = st_pext_iext(iext);
		_md_st_setfd((unsigned)pext->ifd == 65535 ? 0 : pext->ifd);
		pcfd = st_pcfd_ifd(_md_st_currentifd());
		ps = &pext->asym;

		/*
		 * Unfortunately, there are no references to types for any
		 * of these external variables.
		 */
		if ((ps->st == stLabel) || (ps->st == stGlobal)) {
			md_store_type(mdp, pcfd, pext->ifd, -1, ps);
		}
	}
	md_fix_type_info(mdp);
	/* md_print_types(mdp); */
	return (mdp);
}
