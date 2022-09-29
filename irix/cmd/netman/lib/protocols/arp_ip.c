/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * IP-to-Ethernet Address Resolution Protocol (ARP), defined in RFC 826.
 */
#include <bstring.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "protodefs.h"
#include "protocols/arp.h"
#include "protocols/ether.h"
#include "protocols/ip.h"

char arpipname[] = "arpip";

/*
 * IP-ARP field identifiers and descriptors.
 */
enum arpipfid { SHA, SPA, THA, TPA };

static ProtoField arpipfields[] = {
	PFI_ADDR("sha",	"Sender Hardware Address", SHA,	ETHERADDRLEN, PV_TERSE),
	PFI_UINT("spa",	"Sender Protocol Address", SPA,	u_long,	      PV_TERSE),
	PFI_ADDR("tha",	"Target Hardware Address", THA,	ETHERADDRLEN, PV_TERSE),
	PFI_UINT("tpa",	"Target Protocol Address", TPA,	u_long,	      PV_TERSE),
};

#define	ARPIPFID(pf)	((enum arpipfid) (pf)->pf_id)
#define	ARPIPFIELD(fid)	arpipfields[(int) fid]

/*
 * Shorthands for IP-to-Ethernet fields.
 */
static ProtoMacro arpipnicknames[] = {
	PMI("sha",	"arpip.sha"),
	PMI("spa",	"arpip.spa"),
	PMI("tha",	"arpip.tha"),
	PMI("tpa",	"arpip.tpa"),
};

/*
 * Ethernet hostname/address cache update option.
 */
static ProtOptDesc arpipoptdesc[] = {
	POD("etherupdate", ARPIP_PROPT_ETHERUPDATE,
	    "Update Ethernet hostname/address cache"),
};

/*
 * IP-ARP protocol interface.
 */
#define	arpip_embed	pr_stub_embed
#define	arpip_match	pr_stub_match
#define	arpip_fetch	pr_stub_fetch

DefineProtocol(arpip, arpipname, "IP Address Resolution Protocol", PRID_ARPIP,
	       DS_BIG_ENDIAN, ARPIP_SEGLEN, 0, 0,
	       arpipoptdesc, lengthof(arpipoptdesc), 0);

/*
 * IP-ARP operations.
 */
static int
arpip_init()
{
	int nested;

	if (!pr_register(&arpip_proto, arpipfields, lengthof(arpipfields), 0))
		return 0;
	nested = 0;
	if (pr_nest(&arpip_proto, PRID_ARP, ETHERTYPE_IP, arpipnicknames,
		    lengthof(arpipnicknames))) {
		nested = 1;
	}
	if (pr_nest(&arpip_proto, PRID_RARP, ETHERTYPE_IP, arpipnicknames,
		    lengthof(arpipnicknames))) {
		nested = 1;
	}
	return nested;
}

static int etherupdate;

static int
arpip_setopt(int id, char *val)
{
	if ((enum arpip_propt) id != ARPIP_PROPT_ETHERUPDATE)
		return 0;
	etherupdate = (*val != '0');
	return 1;
}

/*
 * Try to resolve name to an IP hostname first, then to an IP address.
 * If that fails, try Ethernet hostname or address.
 */
/* ARGSUSED */
static Expr *
arpip_resolve(char *name, int len, struct snooper *sn)
{
	struct hostent *hp;
	u_long ipa;
	Expr *ex;

	hp = gethostbyname(name);
	if (hp && hp->h_addrtype == AF_INET)
		bcopy(hp->h_addr, &ipa, sizeof ipa);
	else if (ip_addr(name, &ipa) == 0) {
		struct etheraddr *ea;

		ea = ether_hostaddr(name);
		if (ea == 0)
			return 0;
		ex = expr(EXOP_ADDRESS, EX_NULLARY, name);
		*A_CAST(&ex->ex_addr, struct etheraddr) = *ea;
		return ex;
	}
	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = ipa;
	return ex;
}

static ExprType
arpip_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	switch (ARPIPFID(pf)) {
	  case SHA:
	  case THA: {
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
				  ARPIP_SEGLEN)) {
			return ET_COMPLEX;
		}
		break;
	  }

	  case SPA:
	  case TPA: {
		long mask;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		if (tex->ex_op != EXOP_NUMBER) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, tex->ex_val, ARPIP_SEGLEN))
			return ET_COMPLEX;
	  }
	}
	return ET_SIMPLE;
}

static void
arpip_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct arpipseg seg;
	char *name;

	ds_arpipseg(ds, &seg);

	pv_showfield(pv, &ARPIPFIELD(SHA), &seg.ai_spa,
		     "%-25.25s", ether_hostname(&seg.ai_sha));
	name = ip_hostname(seg.ai_spa, IP_HOST);
	pv_showfield(pv, &ARPIPFIELD(SPA), &seg.ai_spa,
		     "%-25.25s", name);
	if (etherupdate)
		(void) ether_addhost(name, &seg.ai_sha);

	pv_showfield(pv, &ARPIPFIELD(THA), &seg.ai_tha,
		     "%-25.25s", ether_hostname(&seg.ai_tha));
	name = ip_hostname(seg.ai_tpa, IP_HOST);
	pv_showfield(pv, &ARPIPFIELD(TPA), &seg.ai_tpa, "%-25.25s", name);
	if (etherupdate && PS_TOP(ps, struct arpframe)->af_op == ARPOP_REPLY)
		(void) ether_addhost(name, &seg.ai_tha);
}
