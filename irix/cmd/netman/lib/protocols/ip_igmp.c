/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Group Management Protocol (IGMP) as defined in RFC 1112.
 */
#include <netdb.h>
#include <sys/types.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"
#include <netinet/igmp.h>

char	igmpname[] = "igmp";

enum igmpfid { VERSION, TYPE, UNUSED, CKSUM, GROUP };

static ProtoField igmpfields[] = {
	PFI_UBIT("version", "Version",	      VERSION, 4,	PV_VERBOSE),
	PFI_UBIT("type",    "Type",	      TYPE,    4,	PV_TERSE),
	PFI_UINT("unused",  "Unused",	      UNUSED,  u_char,	PV_VERBOSE),
	PFI_UINT("cksum",   "Checksum",	      CKSUM,   u_short,	PV_VERBOSE),
	PFI_UINT("group",   "Group Address",  GROUP,   u_long,	PV_TERSE),
};

#define	IGMPFID(pf)	((enum igmpfid) (pf)->pf_id)
#define	IGMPFIELD(fid)	igmpfields[(int) fid]

static ProtoMacro igmpnicknames[] = {
	PMI("IGMP",	igmpname),
};

#define IGMP_TYPE(x)	((x) & 0xf)
#define IGMP_VERS(x)	((x) >> 4)

static Enumeration types;
static Enumerator typesvec[] = {
	EI_VAL("QUERY",		IGMP_TYPE(IGMP_HOST_MEMBERSHIP_QUERY)),
	EI_VAL("REPORT",	IGMP_TYPE(IGMP_HOST_MEMBERSHIP_REPORT)),
	EI_VAL("DVMRP",		IGMP_TYPE(IGMP_DVMRP)),
};

/*
 * IGMP protocol interface.
 */
#define	igmp_setopt	pr_stub_setopt
#define	igmp_embed	pr_stub_embed
#define	igmp_compile	pr_stub_compile
#define	igmp_match	pr_stub_match

DefineLeafProtocol(igmp, igmpname, "Internet Group Management Protocol",
		   PRID_IGMP, DS_BIG_ENDIAN, IGMP_MINLEN);

static int
igmp_init()
{
	struct protoent *pe;

	if (!pr_register(&igmp_proto, igmpfields, lengthof(igmpfields),
			 lengthof(typesvec))) {
		return 0;
	}
	pe = getprotobyname(igmpname);
	if (!pr_nest(&igmp_proto, PRID_IP, (pe ? pe->p_proto : IPPROTO_IGMP),
		     igmpnicknames, lengthof(igmpnicknames))) {
		return 0;
	}
	en_init(&types, typesvec, lengthof(typesvec), &igmp_proto);
	return 1;
}

/* ARGSUSED */
static Expr *
igmp_resolve(char *name, int len, struct snooper *sn)
{
	struct in_addr addr;
	Expr *ex;

	if (!ip_hostaddr(name, IP_HOST, &addr))
		return 0;
	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = addr.s_addr;
	return ex;
}

static int
igmp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct igmp *igmp;

	igmp = (struct igmp *) ds_inline(ds, IGMP_MINLEN, sizeof(u_long));
	if (igmp == 0)
		return ds_field(ds, pf, IGMP_MINLEN, rex);
	rex->ex_op = EXOP_NUMBER;
	switch (IGMPFID(pf)) {
	  case VERSION:
		rex->ex_val = IGMP_VERS(igmp->igmp_type);
		break;
	  case TYPE:
		rex->ex_val = IGMP_TYPE(igmp->igmp_type);
		break;
	  case UNUSED:
		rex->ex_val = igmp->igmp_code;
		break;
	  case CKSUM:
		rex->ex_val = ntohs(igmp->igmp_cksum);
		break;
	  case GROUP:
		rex->ex_val = ntohl(igmp->igmp_group.s_addr);
	}
	return 1;
}

static void
igmp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct igmp *igmp, hdr;
	u_short sum, igmpsum;

	igmp = (struct igmp *) ds_inline(ds, IGMP_MINLEN, sizeof(u_long));
	if (igmp == 0) {
		igmp = &hdr;
		(void) ds_bytes(ds, igmp, MIN(IGMP_MINLEN, ds->ds_count));
		sum = 0;
	} else {
		igmpsum = igmp->igmp_cksum;
		igmp->igmp_cksum = 0;
		if (!ip_checksum_frame(PS_TOP(ps, struct ipframe), (char *)igmp,
				       IGMP_MINLEN + ds->ds_count, &sum)) {
			sum = 0;
		}
		igmp->igmp_cksum = igmpsum;
	}

	pv_showfield(pv, &IGMPFIELD(VERSION), igmp,
		     "%-2d", IGMP_VERS(igmp->igmp_type));
	pv_showfield(pv, &IGMPFIELD(TYPE), igmp,
		     "%-6.6s", en_name(&types, IGMP_TYPE(igmp->igmp_type)));
	pv_showfield(pv, &IGMPFIELD(UNUSED), &igmp->igmp_code,
		     "%#-4x", igmp->igmp_code);
	igmpsum = ntohs(igmpsum);
	if (sum && igmpsum != sum) {
		pv_showfield(pv, &IGMPFIELD(CKSUM), &igmpsum,
			     "%#x [!= %#x]", igmpsum, sum);
	} else {
		pv_showfield(pv, &IGMPFIELD(CKSUM), &igmpsum,
			     "%#-6x", igmpsum);
	}
	igmp->igmp_group = ntohl(igmp->igmp_group);
	pv_showfield(pv, &IGMPFIELD(GROUP), &igmp->igmp_group,
		     "%-15.15s", ip_hostname(igmp->igmp_group, IP_HOST));
}
