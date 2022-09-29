/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Address Resolution Protocol (ARP) and Reverse ARP (RARP) module.
 * ARP is defined in RFC 826; RARP is defined in RFC 903.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "enum.h"
#include "index.h"
#include "protodefs.h"
#include "protocols/arp.h"
#include "llc.h"   /* for tokenring */

char arpname[] = "arp";
char rarpname[] = "rarp";

/*
 * ARP field identifiers and descriptors.
 */
enum arpfid { HRD, PRO, HLN, PLN, OP };

static ProtoField arpfields[] = {
	PFI_UINT("hrd",	"Hardware Address Space",  HRD, u_short, PV_VERBOSE),
	PFI_UINT("pro",	"Protocol Address Space",  PRO, u_short, PV_VERBOSE),
	PFI_UINT("hln",	"Hardware Address Length", HLN, u_char,  PV_VERBOSE),
	PFI_UINT("pln",	"Protocol Address Length", PLN, u_char,  PV_VERBOSE),
	PFI_UINT("op",	"Operation Code",	   OP,	u_short, PV_TERSE),
};

#define	ARPFID(pf)	((enum arpfid) (pf)->pf_id)
#define	ARPFIELD(fid)	arpfields[(int) fid]

static ProtoMacro arpnicknames[] = {
	PMI("ARP",	arpname),
	PMI("arpip",	"arp.arpip"),
	PMI("ARPIP",	"arp.arpip"),
};

static ProtoMacro rarpnicknames[] = {
	PMI("RARP",	rarpname),
	PMI("rarpip",	"rarp.arpip"),
	PMI("RARPIP",	"rarp.arpip"),
};

/*
 * ARP and RARP enumerated types.
 */
static Enumeration hrd, op;
static Enumerator
	hrdvec[] = { EI_VAL("ETHER", ARPHRD_ETHER),
		     EI_VAL("LLC", ARPHRD_802) },
	opvec[]  = { EI_VAL("REQUEST", ARPOP_REQUEST),
		     EI_VAL("REPLY", ARPOP_REPLY),
		     EI_VAL("REVREQUEST", RARPOP_REQUEST),
		     EI_VAL("REVREPLY", RARPOP_REPLY) };

/*
 * ARP and RARP protocol interfaces.
 */
#define	arp_setopt	pr_stub_setopt
#define	arp_resolve	pr_stub_resolve

DefineBranchProtocol(arp, arpname, "Address Resolution Protocol", PRID_ARP,
		     DS_BIG_ENDIAN, sizeof(struct arphdr), &ARPFIELD(PRO));

int	rarp_init();
Protocol rarp_proto = {
	rarpname, constrlen(rarpname), "Reverse Address Resolution Protocol",
	PRID_RARP, DS_BIG_ENDIAN, sizeof(struct arphdr), &ARPFIELD(PRO),
	rarp_init, arp_setopt, arp_embed, arp_resolve, arp_compile, arp_match,
	arp_fetch, arp_decode,
};

/*
 * ARP operations.
 */
static Index *arptypes;

static int
arp_init()
{
	int nested;

	if (!pr_register(&arp_proto, arpfields, lengthof(arpfields),
			 lengthof(hrdvec) + lengthof(opvec) + 5)) {
		return 0;
	}
	nested = 0;
	if (pr_nest(&arp_proto, PRID_ETHER, ETHERTYPE_ARP, arpnicknames,
		    lengthof(arpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&arp_proto, PRID_LLC, ETHERTYPE_ARP, arpnicknames,
		    lengthof(arpnicknames))) {
		nested = 1;
	}
	if (!nested)
		return 0;
	in_create(1, sizeof(u_short), 0, &arptypes);
	en_init(&hrd, hrdvec, lengthof(hrdvec), &arp_proto);
	en_init(&op,  opvec,  lengthof(opvec),  &arp_proto);
	return 1;
}

static int
rarp_init()
{
	int nested;

	if (arptypes == 0 && !arp_init())
		return 0;
	if (!pr_register(&rarp_proto, arpfields, lengthof(arpfields),
			 lengthof(hrdvec) + lengthof(opvec) + 5)) {
		return 0;
	}
	nested = 0;
	if (pr_nest(&rarp_proto, PRID_ETHER, ETHERTYPE_RARP, rarpnicknames,
		    lengthof(rarpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&rarp_proto, PRID_LLC, ETHERTYPE_RARP, rarpnicknames,
		    lengthof(rarpnicknames))) {
		nested = 1;
	}
	if (!nested)
		return 0;
	pr_addnumbers(&rarp_proto, hrdvec, lengthof(hrdvec));
	pr_addnumbers(&rarp_proto, opvec, lengthof(opvec));
	return 1;
}

/* ARGSUSED */
static void
arp_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short type = prototype;

	in_enter(arptypes, &type, pr);
}

static ExprType
arp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask, match;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	switch (tex->ex_op) {
	  case EXOP_PROTOCOL:
		if (ARPFID(pf) != PRO) {
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
	if (!pc_intfield(pc, pf, mask, match, sizeof(struct arphdr)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

static int
arp_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct arphdr *ar;
	struct arpframe af;
	int matched;

	ar = (struct arphdr *) ds_inline(ds, sizeof *ar, sizeof(u_short));
	if (ar == 0 || ntohs(ar->ar_pro) != pex->ex_prsym->sym_prototype)
		return 0;
	PS_PUSH(ps, &af.af_frame, &arp_proto);
	af.af_op = ar->ar_op;
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
}

/* ARGSUSED */
static int
arp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct arphdr *ar;

	ar = (struct arphdr *) ds_inline(ds, sizeof *ar, sizeof(u_short));
	if (ar) {
		switch (ARPFID(pf)) {
		  case HRD:
			rex->ex_val = ntohs(ar->ar_hrd);
			break;
		  case PRO:
			rex->ex_val = ntohs(ar->ar_pro);
			break;
		  case HLN:
			rex->ex_val = ar->ar_hln;
			break;
		  case PLN:
			rex->ex_val = ar->ar_pln;
			break;
		  case OP:
			rex->ex_val = ntohs(ar->ar_op);
		}
		rex->ex_op = EXOP_NUMBER;
		return 1;
	}
	return ds_field(ds, pf, sizeof *ar, rex);
}

static void
arp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct arphdr *ar, hdr;
	Protocol *pr;
	struct arpframe af;

	ar = (struct arphdr *) ds_inline(ds, sizeof *ar, sizeof(u_short));
	if (ar == 0) {
		ar = &hdr;
		ds_arphdr(ds, ar);
	} else {
		ar->ar_hrd = ntohs(ar->ar_hrd);
		ar->ar_pro = ntohs(ar->ar_pro);
		ar->ar_op = ntohs(ar->ar_op);
	}

	pv_showfield(pv, &ARPFIELD(HRD), &ar->ar_hrd,
		     "%-10s", en_name(&hrd, ar->ar_hrd));
	pr = in_match(arptypes, &ar->ar_pro);
	if (pr) {
		pv_showfield(pv, &ARPFIELD(PRO), &ar->ar_pro,
			     "%-6s", pr->pr_name);
	} else {
		pv_showfield(pv, &ARPFIELD(PRO), &ar->ar_pro,
			     "%#06x", ar->ar_pro);
	}
	pv_showfield(pv, &ARPFIELD(HLN), &ar->ar_hln, "%-3u", ar->ar_hln);
	pv_showfield(pv, &ARPFIELD(PLN), &ar->ar_pln, "%-3u", ar->ar_pln);
	pv_showfield(pv, &ARPFIELD(OP), &ar->ar_op,
		     "%-10s", en_name(&op, ar->ar_op));
	PS_PUSH(ps, &af.af_frame, &arp_proto);
	af.af_op = ar->ar_op;
	pv_decodeframe(pv, pr, ds, ps);
	PS_POP(ps);
}
