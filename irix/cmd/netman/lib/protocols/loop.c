/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * A link-level pseudo-protocol used by the loopback interface.
 */
#include <sys/types.h>
#include <net/raw.h>
#include "index.h"
#include "protodefs.h"

char loopname[] = "loop";

/*
 * Struct loopheader field ids and descriptors.
 */
enum loopfid { FAMILY, SPARE };

static ProtoField loopfields[] = {
	PFI_UINT("family", "Address Family", FAMILY, u_short, PV_VERBOSE),
	PFI_UINT("spare",  "Spare Padding",  SPARE,  u_short, PV_VERBOSE+1),
};

#define	LOOPFID(pf)	((enum loopfid) (pf)->pf_id)
#define	LOOPFIELD(fid)	loopfields[(int) fid]

/*
 * Loopback protocol interface and operations.
 */
#define	loop_setopt	pr_stub_setopt
#define	loop_resolve	pr_stub_resolve

DefineBranchProtocol(loop, loopname, "Loopback", PRID_LOOP, DS_BIG_ENDIAN,
		     sizeof(struct loopheader), &LOOPFIELD(FAMILY));

static Index *loopfamilies;

static int
loop_init()
{
	if (!pr_register(&loop_proto, loopfields, lengthof(loopfields), 20))
		return 0;
	pr_addconstnumber(&loop_proto, "MTU", 8304);
	in_create(3, sizeof(u_short), 0, &loopfamilies);
	return 1;
}

/* ARGSUSED */
static void
loop_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short family = prototype;

	in_enter(loopfamilies, &family, pr);
}

static ExprType
loop_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask, match;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	switch (tex->ex_op) {
	  case EXOP_PROTOCOL:
		if (LOOPFID(pf) != FAMILY) {
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
	if (!pc_intfield(pc, pf, mask, match, sizeof(struct loopheader)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

/* ARGSUSED */
static int
loop_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct loopheader *lh;

	lh = (struct loopheader *) ds_inline(ds, sizeof *lh, sizeof(u_short));
	if (lh == 0 || ntohs(lh->loop_family) != pex->ex_prsym->sym_prototype)
		return 0;
	return ex_match(pex, ds, ps, rex);
}

/* ARGSUSED */
static int
loop_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct loopheader *lh;

	lh = (struct loopheader *) ds_inline(ds, sizeof *lh, sizeof(u_short));
	if (lh == 0)
		return 0;
	switch (LOOPFID(pf)) {
	  case FAMILY:
		rex->ex_val = ntohs(lh->loop_family);
		break;
	  case SPARE:
		rex->ex_val = ntohs(lh->loop_spare);
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
}

static void
loop_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct loopheader *lh;
	Protocol *pr;

	lh = (struct loopheader *) ds_inline(ds, sizeof *lh, sizeof(u_short));
	if (lh == 0)
		return;
	lh->loop_family = ntohs(lh->loop_family);
	lh->loop_spare = ntohs(lh->loop_spare);

	pr = in_match(loopfamilies, &lh->loop_family);
	if (pr == 0) {
		pv_showfield(pv, &LOOPFIELD(FAMILY), &lh->loop_family,
			     "%-5u", lh->loop_family);
	} else {
		pv_showfield(pv, &LOOPFIELD(FAMILY), &lh->loop_family,
			     "%-6s", pr->pr_name);
	}
	pv_showfield(pv, &LOOPFIELD(SPARE), &lh->loop_spare,
		     "%-5u", lh->loop_spare);
	pv_decodeframe(pv, pr, ds, ps);
}
