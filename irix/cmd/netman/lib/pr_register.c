/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Protocol registry, to avoid binding all modules in the forest of
 * dependent protocols with every application that uses just one raw
 * protocol.  Applications must link with exactly the set of protocol
 * interfaces that they require.
 */
#include <values.h>
#include "packetview.h"
#include "protocol.h"
#include "protoid.h"
#include "scope.h"

static Scope	*prscope;
static Protocol	*pridmap[PRID_MAX];

static int	registerfields(ProtoField *, int, Scope *, int *);
static int	registerarray(ProtoField *, Scope *, int *);
static int	registerstruct(ProtoField *, int *);

int
pr_register(Protocol *pr, ProtoField *pf, int npf, int extra)
{
	Symbol *sym;

	if (pr->pr_scope.sc_table)
		sc_finish(&pr->pr_scope);
	sc_init(&pr->pr_scope, npf + extra, pr->pr_name);

	pr->pr_level = MAXINT;
	if (registerfields(pf, npf, &pr->pr_scope, &pr->pr_level) < 0) {
		sc_finish(&pr->pr_scope);
		return 0;
	}

	if (prscope == 0)
		prscope = scope(PRID_MAX-1, "protocols");
	sym = sc_addsym(prscope, pr->pr_name, pr->pr_namlen, SYM_PROTOCOL);
	sym->sym_proto = pr;
	pridmap[pr->pr_id] = pr;
	return 1;
}

static int
registerfields(ProtoField *pf, int npf, Scope *sc, int *minlevel)
{
	int variable, offset, bitoff;
	Symbol *sym;

	variable = 0;
	for (offset = bitoff = 0; --npf >= 0; pf++) {
		switch (pf->pf_type) {
		  case EXOP_ARRAY:
			if (registerarray(pf, sc, minlevel) < 0)
				return -1;
			break;
		  case EXOP_STRUCT:
			if (registerstruct(pf, minlevel) < 0)
				return -1;
		}

		if (pf->pf_size == PF_VARIABLE) {
			/*
			 * Reset automagic offset calculation.
			 */
			offset = 0;
			variable = 1;
		} else if (pf->pf_size < 0) {
			/*
			 * Bit field: bitoff is the bit offset within a byte.
			 */
			if (pf->pf_off < 0)
				pf->pf_off = offset * BITSPERBYTE + bitoff;
			else
				offset = pf->pf_off;
			bitoff -= pf->pf_size;
			if (bitoff >= BITSPERBYTE) {
				offset += bitoff / BITSPERBYTE;
				bitoff %= BITSPERBYTE;
			}
		} else {
			/*
			 * Byte field: bitoff had better be zero.
			 */
			if (pf->pf_off < 0) {
				if (bitoff != 0)
					return -1;
				pf->pf_off = offset;
			} else {
				bitoff = 0;
				offset = pf->pf_off;
			}
			offset += pf->pf_size;
		}

		sym = sc_addsym(sc, pf->pf_name, pf->pf_namlen, SYM_FIELD);
		sym->sym_field = pf;
		if (pf->pf_level < *minlevel)
			*minlevel = pf->pf_level;
	}
	if (variable)
		return PF_VARIABLE;
	if (bitoff != 0)
		offset += BITSPERBYTE;
	return offset;
}

static int
registerarray(ProtoField *pf, Scope *sc, int *minlevel)
{
	ProtoField *tpf;
	int size;

	tpf = pf_element(pf);
	switch (tpf->pf_type) {
	  case EXOP_ARRAY:
		size = registerarray(tpf, sc, minlevel);
		break;
	  case EXOP_STRUCT:
		size = registerstruct(tpf, minlevel);
		break;
	  default:
		size = tpf->pf_size;
	}
	if (size < 0)
		return -1;
	if (size == PF_VARIABLE || pf->pf_size == PF_VARIABLE)
		return PF_VARIABLE;
	return size * pf->pf_size;
}

static int
registerstruct(ProtoField *pf, int *minlevel)
{
	ProtoStruct *pst;
	int size;

	pst = pf_struct(pf);
	if (pst == 0)
		return -1;
	if (pst->pst_scope.sc_table)
		return pf->pf_size;
	sc_init(&pst->pst_scope, pst->pst_numfields, pf->pf_name);
	size = registerfields(pst->pst_fields, pst->pst_numfields,
			      &pst->pst_scope, minlevel);
	if (size < 0) {
		sc_finish(&pst->pst_scope);
		return -1;
	}
	pf->pf_size = size;
	pst->pst_parent = pf;
	return size;
}

Protocol *
findprotobyname(char *name, int namlen)
{
	Symbol *sym;

	if (prscope == 0)
		return 0;
	sym = sc_lookupsym(prscope, name, namlen);
	return (sym == 0) ? 0 : sym->sym_proto;
}

Protocol *
findprotobyid(unsigned int id)
{
	if (id >= PRID_MAX)
		return 0;
	return pridmap[id];
}
