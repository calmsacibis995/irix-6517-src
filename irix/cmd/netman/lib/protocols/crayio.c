/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Cray Input/Output Subsystem point-to-point protocol.
 */
#include <sys/types.h>
#include <sys/cfeiioctl.h>
#include "index.h"
#include "protodefs.h"

char crayioname[] = "crayio";

/*
 * Struct cy_hdr field ids and descriptors.
 */
enum crayiofid { U1, AD, SECCNT, TO, FROM, COUNT, TYPE, OFFSET, LEN };

static ProtoField crayiofields[] = {
	PFI_UBIT("u1",     "Unused",           U1,       15,      PV_VERBOSE+1),
	PFI_UBIT("ad",     "Associated Data Flag", AD,   1,       PV_VERBOSE),
	PFI_UINT("seccnt", "Sector Count",     SECCNT,   u_short, PV_VERBOSE),
	PFI_UINT("to",     "Destination",      TO,       u_short, PV_TERSE),
	PFI_UINT("from",   "Source",           FROM,     u_short, PV_TERSE),
	PFI_UINT("count",  "Packet Count",     COUNT,    u_short, PV_VERBOSE),
	PFI_UINT("type",   "Protocol Type",    TYPE,     u_char,  PV_VERBOSE),
	PFI_UINT("offset", "Protocol Offset",  OFFSET,   u_char,  PV_VERBOSE),
	PFI_UINT("len",    "Associated Data Length",LEN, u_int,   PV_VERBOSE),
};

#define	CRAYIOFID(pf)		((enum crayiofid) (pf)->pf_id)
#define	CRAYIOFIELD(fid)	crayiofields[(int) fid]

/*
 * Loopback protocol interface and operations.
 */
#define	crayio_setopt	pr_stub_setopt
#define	crayio_resolve	pr_stub_resolve

DefineProtocol(crayio, crayioname, "Cray Input/Output", PRID_CRAYIO,
	       DS_BIG_ENDIAN, sizeof(struct cy_hdr), &CRAYIOFIELD(TYPE),
	       0, 0, 0, 0);

static Index *crayiotypes;

static int
crayio_init()
{
	if (!pr_register(&crayio_proto,crayiofields,lengthof(crayiofields),30))
		return 0;
	in_create(3, sizeof(u_char), 0, &crayiotypes);
	return 1;
}

/* ARGSUSED */
static void
crayio_embed(Protocol *pr, long prototype, long qualifier)
{
	u_char type = prototype;

	in_enter(crayiotypes, &type, pr);
}

static ExprType
crayio_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask, match;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	switch (tex->ex_op) {
	  case EXOP_PROTOCOL:
		if (CRAYIOFID(pf) != TYPE) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		match = tex->ex_prsym->sym_prototype;
		break;
	  case EXOP_NUMBER:
		match = tex->ex_val;
		break;
	  default:
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_intfield(pc, pf, mask, match, sizeof(struct cy_hdr)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

/* ARGSUSED */
static int
crayio_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct cy_hdr *cy;

	cy = (struct cy_hdr *) ds_inline(ds, sizeof *cy, sizeof(u_int));
	if (cy == 0 || cy->CYtype != pex->ex_prsym->sym_prototype)
		return 0;
	return ex_match(pex, ds, ps, rex);
}

/* ARGSUSED */
static int
crayio_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct cy_hdr *cy;

	cy = (struct cy_hdr *) ds_inline(ds, sizeof *cy, sizeof(u_int));
	if (cy == 0)
		return 0;
	switch (CRAYIOFID(pf)) {
	  case U1:
		rex->ex_val = cy->u1;
		break;
	  case AD:
		rex->ex_val = cy->ad;
		break;
	  case SECCNT:
		rex->ex_val = cy->seccnt;
		break;
	  case TO:
		rex->ex_val = cy->to[0] << 8 | cy->to[1];
		break;
	  case FROM:
		rex->ex_val = cy->from[0] << 8 | cy->from[1];
		break;
	  case COUNT:
		rex->ex_val = cy->count;
		break;
	  case TYPE:
		rex->ex_val = cy->CYtype;
		break;
	  case OFFSET:
		rex->ex_val = cy->CYoffset;
		break;
	  case LEN:
		rex->ex_val = cy->len;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
}

static void
crayio_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct cy_hdr *cy;
	Protocol *pr;

	cy = (struct cy_hdr *) ds_inline(ds, sizeof *cy, sizeof(u_int));
	if (cy == 0)
		return;

	pv_showfield(pv, &CRAYIOFIELD(U1), cy, "%-4x", cy->u1);
	pv_showfield(pv, &CRAYIOFIELD(AD), cy, "%d", cy->ad);

	pv_showfield(pv, &CRAYIOFIELD(SECCNT), (char *)cy + 2,
		     "%-5u", cy->seccnt);
	pv_showfield(pv, &CRAYIOFIELD(TO), cy->to,
		     "%2x%2x", cy->to[0], cy->to[1]);
	pv_showfield(pv, &CRAYIOFIELD(FROM), cy->from,
		     "%2x%2x", cy->from[0], cy->from[1]);
	pv_showfield(pv, &CRAYIOFIELD(COUNT), &cy->count,
		     "%-5u", cy->count);

	pr = in_match(crayiotypes, &cy->CYtype);
	if (pr == 0) {
		pv_showfield(pv, &CRAYIOFIELD(TYPE), &cy->CYtype,
			     "@t%-5u", cy->CYtype);
	} else {
		pv_showfield(pv, &CRAYIOFIELD(TYPE), &cy->CYtype,
			     "%-6s", pr->pr_name);
	}

	pv_showfield(pv, &CRAYIOFIELD(OFFSET), &cy->CYoffset,
		     "%-3u", cy->CYoffset);
	pv_showfield(pv, &CRAYIOFIELD(LEN), &cy->len,
		     "%-10u", cy->len);
	pv_decodeframe(pv, pr, ds, ps);
}
