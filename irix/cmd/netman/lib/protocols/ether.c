/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Ethernet snoop module.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>		/* for ntohs() */
#include <netinet/if_ether.h>	/* for ETHERMTU */
#include "index.h"
#include "protodefs.h"
#include "strings.h"
#include "protocols/ether.h"

char ethername[] = "ether";

/*
 * Ethernet field identifiers and descriptors.
 */
enum etherfid {
	DST=ETHERFID_DST, SRC=ETHERFID_SRC, TYPE=ETHERFID_TYPE, LEN,
	DISCRIM
};

static ProtoField etherfields[] = {
	PFI_ADDR("dst",  "Destination Address",	DST,  ETHERADDRLEN, PV_TERSE),
	PFI_ADDR("src",  "Source Address",	SRC,  ETHERADDRLEN, PV_TERSE),
	PFI_UINT("type", "Packet Type",		TYPE, u_short,	    PV_VERBOSE),
	PFI("len",       "Packet Length",       LEN,  (void*)DS_ZERO_EXTEND,
	    sizeof(u_short), 12, EXOP_NUMBER, PV_VERBOSE),
};

static ProtoField discrim =
	PFI("discrim",   "Type Discriminant",   DISCRIM, (void*)DS_ZERO_EXTEND,
	    sizeof(u_short), 12, EXOP_NUMBER, PV_VERBOSE+1);

#define	ETHERFID(pf)	((enum etherfid) (pf)->pf_id)
#define	ETHERFIELD(fid)	etherfields[(int) fid]

/*
 * Ethernet macros.
 */
static ProtoMacro ethermacros[] = {
	PMI("between",	"src == $1 && dst == $2 || dst == $1 && src == $2"),
	PMI("host",	"src == $1 || dst == $1"),
	PMI("multicast",
	    "(dst & 1:0:0:0:0:0) == 1:0:0:0:0:0 && dst != BROADCAST"),
};

static ProtOptDesc etheroptdesc[] = {
	POD("hostbyname", ETHER_PROPT_HOSTBYNAME,
	    "Decode Ethernet addresses into hostnames"),
};

/*
 * Ethernet protocol interface and operations.
 */
DefineProtocol(ether, ethername, "Ethernet", PRID_ETHER, DS_BIG_ENDIAN,
	       ETHERHDRLEN, &discrim, 0, etheroptdesc, lengthof(etheroptdesc),
	       0);

struct etheraddr etherbroadcastaddr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static Index *ethertypes;
static Protocol *llc, *idp;
static u_short llctype, idptype;

static int
ether_init()
{
	if (!pr_register(&ether_proto, etherfields, lengthof(etherfields),
			 lengthof(ethermacros) + 100)) {
		return 0;
	}
	pr_addmacros(&ether_proto, ethermacros, lengthof(ethermacros));
	pr_addconstaddress(&ether_proto, "BROADCAST",
		(char *) &etherbroadcastaddr, sizeof etherbroadcastaddr);
	pr_addconstnumber(&ether_proto, "MTU", ETHERMTU);
	in_create(3, sizeof(u_short), 0, &ethertypes);
	return 1;
}

int
ether_setopt(int id, char *val)
{
	extern int _etherhostbyname;

	if ((enum ether_propt) id != ETHER_PROPT_HOSTBYNAME)
		return 0;
	_etherhostbyname = (*val != '0');
	return 1;
}

/* ARGSUSED */
static void
ether_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short type;

	if (pr->pr_id == PRID_LLC) {
		llc = pr, llctype = prototype;
		return;
	}
	if (pr->pr_id == PRID_IDP)
		idp = pr, idptype = prototype;

	type = prototype;
	in_enter(ethertypes, &type, pr);
}

/* ARGSUSED */
Expr *
ether_resolve(char *name, int len, struct snooper *sn)
{
	struct etheraddr *ea;
	struct etheraddr addr;
	Expr *ex;

	ea = ether_hostaddr(name);
	if (ea == 0) {
		int n, v[5];
		unsigned long b;
		char *bp, *ep;
		unsigned char *vp;

		n = 0;
		bp = name;
		for (;;) {
			b = strtoul(bp, &ep, 16);
			if (bp == ep || b > 255)
				return 0;
			v[n++] = b;
			if (*ep == '\0')
				break;
			if (*ep != ':' || n == 6)
				return 0;
			bp = ep + 1;
		}
		ea = &addr;
		vp = &ea->ea_vec[ETHERADDRLEN];
		while (--n >= 0)
			*--vp = v[n];
		while (vp > &ea->ea_vec[0])
			*--vp = 0;
	}
	ex = expr(EXOP_ADDRESS, EX_NULLARY, name);
	*A_CAST(&ex->ex_addr, struct etheraddr) = *ea;
	return ex;
}

static ExprType
ether_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	enum etherfid fid;

	fid = ETHERFID(pf);
	switch (fid) {
	  case DST:
	  case SRC: {
		u_char *mask;

		if (mex == 0)
			mask = (u_char *) PC_ALLBYTES;
		else {
			if (mex->ex_op != EXOP_ADDRESS) {
				pc_badop(pc, mex);
				return ET_ERROR;
			}
			mask = A_BASE(&mex->ex_addr, pf->pf_size);
		}
		if (tex->ex_op != EXOP_ADDRESS) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_bytefield(pc, pf, (char *) mask,
				  (char *) A_BASE(&tex->ex_addr, pf->pf_size),
				  ETHERHDRLEN)) {
			return ET_COMPLEX;
		}
		break;
	  }

	  case TYPE:
	  case DISCRIM: {
		long mask, match;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		switch (tex->ex_op) {
		  case EXOP_PROTOCOL:
			match = tex->ex_prsym->sym_prototype;
			if (fid == DISCRIM
			    && (match == llctype || match == idptype)) {
				mask &= ~0x05FF;
				match = 0;
			}
			break;
		  case EXOP_NUMBER:
			match = tex->ex_val;
			break;
		  default:
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, match, ETHERHDRLEN))
			return ET_COMPLEX;
		break;
	  }

	  case LEN: {
		long mask, match;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		if (tex->ex_op == EXOP_NUMBER)
			match = tex->ex_val;
		else {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, match, ETHERHDRLEN))
			return ET_COMPLEX;
	  }
	}
	return ET_SIMPLE;
}

#define	NETWARE_IDP_LOOKAHEAD(ds) \
	(ds->ds_count > 1 && ds->ds_next[0] == 0xFF && ds->ds_next[1] == 0xFF)

static int
ether_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct etherhdr *eh;
	struct etherframe ef;
	int matched;

	eh = (struct etherhdr *) ds_inline(ds, sizeof *eh, sizeof(u_short));
	if (eh == 0)
		return 0;
	if (ntohs(eh->eh_type) <= ETHERMTU) {
		if (NETWARE_IDP_LOOKAHEAD(ds)) {
			if (pex->ex_prsym->sym_proto != idp)
				return 0;
		} else {
			if (pex->ex_prsym->sym_proto != llc) {
				/* Eat llc and keep trying */
				PS_PUSH(ps, &ef.ef_frame, &ether_proto);
				ef.ef_src = eh->eh_src;
				ef.ef_dst = eh->eh_dst;
				matched = pr_match(llc, pex, ds, ps, rex);
				PS_POP(ps);
				return matched;
			}
		}
	} else {
		if (ntohs(eh->eh_type) != pex->ex_prsym->sym_prototype)
			return 0;
	}
	PS_PUSH(ps, &ef.ef_frame, &ether_proto);
	ef.ef_src = eh->eh_src;
	ef.ef_dst = eh->eh_dst;
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
}

/* ARGSUSED */
static int
ether_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct etherhdr *eh;
	u_short type;

	eh = (struct etherhdr *) ds_inline(ds, sizeof *eh, sizeof(u_short));
	if (eh == 0)
		return 0;
	switch (ETHERFID(pf)) {
	  case DST:
		*A_CAST(&rex->ex_addr, struct etheraddr) = eh->eh_dst;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case SRC:
		*A_CAST(&rex->ex_addr, struct etheraddr) = eh->eh_src;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case TYPE:
	  case LEN:
		rex->ex_val = ntohs(eh->eh_type);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case DISCRIM:
		type = ntohs(eh->eh_type);
		if (type <= ETHERMTU)
			type = NETWARE_IDP_LOOKAHEAD(ds) ? idptype : llctype;
		rex->ex_val = type;
		rex->ex_op = EXOP_NUMBER;
	}
	return 1;
}

static void
ether_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct etherhdr *eh;
	Protocol *pr;
	struct etherframe ef;

	eh = (struct etherhdr *) ds_inline(ds, sizeof *eh, sizeof(u_short));
	if (eh == 0)
		return;
	eh->eh_type = ntohs(eh->eh_type);

	/*
	 * Show src before dst so that higher layer protocols' source and
	 * destination fields line up.
	 */
	pv->pv_off = ETHERFIELD(SRC).pf_off;
	pv_showfield(pv, &ETHERFIELD(SRC), &eh->eh_src,
		     "%-25.25s", ether_hostname(&eh->eh_src));
	pv->pv_off = ETHERFIELD(DST).pf_off;
	pv_showfield(pv, &ETHERFIELD(DST), &eh->eh_dst,
		     "%-25.25s", ether_hostname(&eh->eh_dst));
	pv->pv_off = ETHERFIELD(TYPE).pf_off;

	/*
	 * If we can't decode the network protocol, then promote type's level
	 * to terse and try to print a description of it.
	 */
	if (eh->eh_type <= ETHERMTU) {
		pv_showfield(pv, &ETHERFIELD(LEN), &eh->eh_len,
			     "%-5u", eh->eh_len);

		pr = NETWARE_IDP_LOOKAHEAD(ds) ? idp : llc;
	} else {
		pr = in_match(ethertypes, &eh->eh_type);
		if (pr) {
			pv_showfield(pv, &ETHERFIELD(TYPE), &eh->eh_type,
				     "%-6s", pr->pr_name);
		} else {
			char *desc;

			desc = ether_typedesc(eh->eh_type);
			if (desc) {
				pv_showfield(pv, &ETHERFIELD(TYPE),
					     &eh->eh_type, "@t%-6s", desc);
			} else {
				pv_showfield(pv, &ETHERFIELD(TYPE),
					     &eh->eh_type, "@t%#06x",
					     eh->eh_type);
			}
		}
	}

	PS_PUSH(ps, &ef.ef_frame, &ether_proto);
	ef.ef_src = eh->eh_src;
	ef.ef_dst = eh->eh_dst;
	pv_decodeframe(pv, pr, ds, ps);
	PS_POP(ps);
}
