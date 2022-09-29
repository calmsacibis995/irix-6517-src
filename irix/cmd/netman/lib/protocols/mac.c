/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * MAC module.
 */
#include <bstring.h>
#include <sys/types.h>
#include <netinet/in.h>		/* for ntohs() */
#include "index.h"
#include "protodefs.h"
#include "strings.h"
#include "protocols/mac.h"

char macname[] = "mac";

/*
 * MAC field identifiers and descriptors.
 */
enum macfid {
	TRT,
	BCN,
	PAD
};

static ProtoField macfields[] = {
	PFI_UINT("claimtrt", "Claim Token Rotation Time",TRT, u_long, PV_TERSE),
	PFI_UINT("beacon", "Beacon Type",	BCN, u_char,	PV_TERSE),
	PFI_BYTE("pad", "Reserved",		PAD, 3, 	PV_VERBOSE+1),
};

#define	MACFID(pf)	((enum macfid) (pf)->pf_id)
#define	MACFIELD(fid)	macfields[(int) fid]

/*
 * MAC protocol interface.
 */
#define mac_setopt	pr_stub_setopt
#define mac_embed	pr_stub_embed
#define mac_resolve	pr_stub_resolve
#define mac_match	pr_stub_match

DefineLeafProtocol(mac, macname, "Medium Access Control", PRID_MAC,
		   DS_BIG_ENDIAN, sizeof(MAC));

static int
mac_init()
{
	if (!pr_register(&mac_proto, macfields, lengthof(macfields), 0))
		return 0;

	return pr_nest(&mac_proto, PRID_FDDI, PRID_MAC, 0, 0);
}

static ExprType
mac_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	enum macfid fid;

	fid = MACFID(pf);
	switch (fid) {
	  case BCN:
	  case TRT: {
		long mask;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		if (tex->ex_op != EXOP_NUMBER) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, tex->ex_val,
				 sizeof(u_long))) {
			return ET_COMPLEX;
		}
		break;
	  }
	}
	return ET_SIMPLE;
}

/* ARGSUSED */
static int
mac_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	MAC *m;

	m = (MAC *)ds_inline(ds, sizeof(*m), sizeof(u_long));
	if (m == 0)
		return 0;
	switch (MACFID(pf)) {
	  case PAD:
		return 0;
	  case TRT:
		rex->ex_val = ntohl(m->clm);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case BCN:
		rex->ex_val = m->bcn.bcn_type;
		rex->ex_op = EXOP_NUMBER;
		break;
	  default:
		return(0);
	}
	return 1;
}

static void
mac_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	MAC *m;

	m = (MAC *)ds_inline(ds, sizeof(*m), sizeof(u_long));
	if (m == 0)
		return;

	switch (PS_TOP(ps, struct fddiframe)->ff_fc & 0x0F) {
	  case MAC_SUBFC_CLM:
		pv_showfield(pv, &MACFIELD(TRT), m, "%d", m->clm);
		break;

	  case MAC_SUBFC_BEACON:
		pv_showfield(pv, &MACFIELD(BCN), &m->bcn.bcn_type,
			"%#02x", m->bcn.bcn_type);
		pv_showfield(pv, &MACFIELD(PAD), m->bcn.bcn_pad,
			"0x%02x%02x%02x",
			m->bcn.bcn_pad[0], m->bcn.bcn_pad[1],
			m->bcn.bcn_pad[2]);
		break;
	}
}
