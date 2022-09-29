/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Scope add function wrappers.
 * XXXbe move to sc_addtype.c
 */
#include <string.h>
#include "macros.h"
#include "protocol.h"
#include "scope.h"

void
sc_addaddress(Scope *sc, char *name, int namlen, char *addr, int addrlen)
{
	Symbol *sym;

	sym = sc_addsym(sc, name, namlen, SYM_ADDRESS);
	if (addrlen > sizeof sym->sym_addr)
		addrlen = sizeof sym->sym_addr;
	A_INIT(&sym->sym_addr, addr, addrlen);
}

void
sc_addnumber(Scope *sc, char *name, int namlen, long val)
{
	Symbol *sym;

	sym = sc_addsym(sc, name, namlen, SYM_NUMBER);
	sym->sym_val = val;
}

void
sc_addmacro(Scope *sc, char *name, int namlen, char *def, int deflen,
	    ExprSource *src)
{
	Symbol *sym;
	char *bp, *ap;
	long argno;

	sym = sc_addsym(sc, name, namlen, SYM_MACRO);
	sym->sym_def.md_string.s_ptr = def;
	sym->sym_def.md_string.s_len = deflen;
	sym->sym_def.md_nargs = 0;
	bp = def;
	while (ap = strchr(bp, '$')) {
		ap++;
		argno = strtol(ap, &bp, 10);
		if (bp > ap && argno > sym->sym_def.md_nargs
		    && argno <= MAXMACROARGS) {
			sym->sym_def.md_nargs = argno;
		}
	}
	sym->sym_def.md_src = src;
}

void
sc_addfunction(Scope *sc, char *name, int namlen, int (*func)(), int nargs,
	       char *desc)
{
	Symbol *sym;

	sym = sc_addsym(sc, name, namlen, SYM_FUNCTION);
	sym->sym_func.fd_func = func;
	sym->sym_func.fd_nargs = MIN(nargs, MAXCALLARGS);
	sym->sym_func.fd_desc = desc;
}

void
sc_addmacros(Scope *sc, ProtoMacro *pm, int npm)
{
	while (--npm >= 0) {
		sc_addmacro(sc, pm->pm_name, pm->pm_namlen,
			    pm->pm_def, pm->pm_deflen, 0);
		pm++;
	}
}

void
sc_addfunctions(Scope *sc, ProtoFuncDesc *pfd, int npfd)
{
	while (--npfd >= 0) {
		sc_addfunction(sc, pfd->pfd_name, pfd->pfd_namlen,
			       pfd->pfd_func, pfd->pfd_nargs, pfd->pfd_desc);
		pfd++;
	}
}

ProtoField *
sc_findfield(Scope *sc, int pfid)
{
	Symbol *sym;
	int count;
	ProtoField *pf, *spf;
	ProtoStruct *pst;

	count = sc->sc_length;
	for (sym = sc->sc_table; --count >= 0; sym++) {
		if (sym->sym_type != SYM_FIELD)
			continue;
		pf = sym->sym_field;
		if (pf->pf_id == pfid)
			return pf;
		while (pf->pf_type == EXOP_ARRAY)
			pf = pf_element(pf);
		if (pf->pf_type == EXOP_STRUCT) {
			pst = pf_struct(pf);
			spf = pf_struct(pf)->pst_parent;
			if (spf) {	/* avoid recursion */
				pst->pst_parent = 0;
				pf = sc_findfield(&pst->pst_scope, pfid);
				pst->pst_parent = spf;
				if (pf)
					return pf;
			}
		}
	}
	return 0;
}
