/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * BSD "routed" Routing Information Protocol (RIP), version 1.
 */
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#define RIPVERSION RIPv2
#include <protocols/routed.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"

char	ripname[] = "rip";

#define	RIP_PORT	520		/* in case getservbyname fails */
#define RIP_INFOSIZE	sizeof(struct netinfo)
#define	RIP_HDRSIZE	(sizeof(struct rip)-RIP_INFOSIZE)
#define	RIP_NETDIM	NETS_LEN

#define	PV_HIDDEN	(PV_VERBOSE+1)

enum ripfid {
	CMD, VERS, RES1, TRACEFILE,
	NETS, NET,
	    FAMILY, TAG, ADDRESS, MASK, NHOP, METRIC,
	    AUTHTYPE, AUTH,
};

static ProtoField netfields[] = {
	PFI_UINT("family",  "Address Family",	FAMILY,	  u_short, PV_DEFAULT),
	PFI_UINT("tag",	    "Tag",		TAG,	  u_short, PV_VERBOSE),
	PFI_UINT("address", "Destination Address",ADDRESS,u_long,  PV_DEFAULT),
	PFI_UINT("mask",    "Netmask",		MASK,	  u_long,  PV_DEFAULT),
	PFI_UINT("nhop",    "Next Hop",		NHOP,     u_long,  PV_DEFAULT),
	PFI_UINT("metric",  "Cost of Route",	METRIC,	  u_long,  PV_DEFAULT),
	PFI_UINT("authtype","Authentication Type",AUTHTYPE,u_short,PV_DEFAULT),
	PFI_BYTE("auth",    "Authentication",	AUTH, RIP_INFOSIZE,PV_DEFAULT),
};
static ProtoField hidfields[] = {
	PFI_UINT("family",  "",	    FAMILY,	u_short,    PV_HIDDEN),
	PFI_UINT("tag",	    "",	    TAG,	u_short,    PV_HIDDEN),
	PFI_UINT("address", "",	    ADDRESS,	u_long,	    PV_DEFAULT),
	PFI_UINT("mask",    "",	    MASK,	u_long,	    PV_HIDDEN),
	PFI_UINT("nhop",    "",	    NHOP,	u_long,	    PV_HIDDEN),
	PFI_UINT("metric",  "",	    METRIC,	u_long,	    PV_DEFAULT),
	PFI_UINT("authtype","",	    AUTHTYPE,	u_short,    PV_DEFAULT),
	PFI_BYTE("auth",    "",	    AUTH,	RIP_INFOSIZE,PV_DEFAULT),
};
static ProtoStruct netstruct = PSI("netinfo", netfields);
static ProtoField net = PFI_STRUCT("net", "Network", NET, &netstruct, 0);

static ProtoField ripfields[] = {
	PFI_UINT("cmd",	    "Command",		CMD,	  u_char,  PV_TERSE),
	PFI_UINT("vers",    "Version",		VERS,	  u_char,  PV_TERSE),
	PFI_UINT("res1",    "Reserved",		RES1,	  u_short, PV_HIDDEN),
	PFI_VAR("tracefile","Trace File Name",  TRACEFILE,
		RIP_INFOSIZE, PV_TERSE),
	PFI_ARRAY("nets",   "Networks",		NETS,      &net,
		  RIP_NETDIM, PV_HIDDEN),
};

#define	RIPFID(pf)	((enum ripfid)(pf)->pf_id)
#define	NETFIELD(fid)	netfields[(int)fid - (int)FAMILY]
#define HIDFIELD(fid)	hidfields[(int)fid - (int)FAMILY]
#define	RIPFIELD(fid)	ripfields[(int)fid]

static ProtoMacro ripnicknames[] = {
	PMI("RIP",	ripname),
};

static Enumeration cmdcode;
static Enumerator cmdcodevec[] = {
	EI_VAL("REQUEST",  RIPCMD_REQUEST),
	EI_VAL("RESPONSE", RIPCMD_RESPONSE),
	EI_VAL("TRACEON",  RIPCMD_TRACEON),
	EI_VAL("TRACEOFF", RIPCMD_TRACEOFF),
};


/*
 * RIP protocol interface.
 */
#define	rip_setopt	pr_stub_setopt
#define	rip_embed	pr_stub_embed
#define	rip_compile	pr_stub_compile
#define	rip_match	pr_stub_match

DefineLeafProtocol(rip, ripname, "Routing Information Protocol", PRID_RIP,
		   DS_BIG_ENDIAN, RIP_HDRSIZE);

static int
rip_init()
{
	struct servent *sp;

	if (!pr_register(&rip_proto, ripfields, lengthof(ripfields),
			 lengthof(cmdcodevec))) {
		return 0;
	}
	sp = getservbyname("route", "udp");
	if (!pr_nest(&rip_proto, PRID_UDP, (sp ? sp->s_port : RIP_PORT),
		     ripnicknames, lengthof(ripnicknames))) {
		return 0;
	}
	en_init(&cmdcode, cmdcodevec, lengthof(cmdcodevec), &rip_proto);
	return 1;
}

/* ARGSUSED */
static Expr *
rip_resolve(char *name, int len, struct snooper *sn)
{
	struct in_addr addr;
	struct netent *np;
	Expr *ex;

	if (ip_hostaddr(name, IP_HOST, &addr)) {
		ex = expr(EXOP_NUMBER, EX_NULLARY, name);
		ex->ex_val = addr.s_addr;
		return ex;
	}
	np = getnetbyname(name);
	if (np && np->n_addrtype == AF_INET) {
		ex = expr(EXOP_NUMBER, EX_NULLARY, name);
		ex->ex_val = np->n_net;
		return ex;
	}
	return ex_string(name, len);
}

#define	ds_inline_rip(ds) \
	(struct rip *) ds_inline(ds, RIP_HDRSIZE, sizeof(u_short))
#define	ds_inline_netinfo(ds) \
	(struct netinfo *) ds_inline(ds, RIP_INFOSIZE, sizeof(long))
#define NTOAUTH(n) ((struct netauth *)n)


static int
rip_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct rip *rip;
	struct netinfo *n;

	if (pf->pf_id <= (int) NETS) {
		rip = ds_inline_rip(ds);
		if (rip == 0) {
			if (pf->pf_id > (int) RES1)
				return 0;
			return ds_field(ds, pf, RIP_HDRSIZE, rex);
		}
	}
	rex->ex_op = EXOP_NUMBER;
	switch (RIPFID(pf)) {
	case CMD:
		rex->ex_val = rip->rip_cmd;
		break;
	case VERS:
		rex->ex_val = rip->rip_vers;
		break;
	case RES1:
		rex->ex_val = ntohs(rip->rip_res1);
		break;
	case TRACEFILE:
		if (rip->rip_cmd != RIPCMD_TRACEON)
			return 0;
		rex->ex_op = EXOP_STRING;
		rex->ex_str.s_ptr = (char *)rip->rip_tracefile;
		rex->ex_str.s_len = ((ds->ds_next[ds->ds_count] == '\0')
				     ? strlen((char *)rip->rip_tracefile)
				     : ds->ds_count);
		break;
	case NETS:
		if (rip->rip_cmd != RIPCMD_REQUEST
		    && rip->rip_cmd != RIPCMD_RESPONSE) {
			return 0;
		}
		rex->ex_op = EXOP_FIELD;
		rex->ex_field = &net;
		break;
	case NET:
		if (net.pf_cookie >= RIP_NETDIM)
			return 0;
		net.pf_cookie = RIP_HDRSIZE + net.pf_cookie * RIP_INFOSIZE;
		if (net.pf_cookie >= ds->ds_count)
			return 0;
		rex->ex_op = EXOP_FIELD;
		rex->ex_field = &net;
		break;
	default:
		n = ds_inline_netinfo(ds);
		if (n == 0)
			return 0;
		if (n->n_family == RIP_AF_AUTH
		    && rip->rip_vers >= RIPv2) {
			switch (RIPFID(pf)) {
			case AUTHTYPE:
				rex->ex_val = NTOAUTH(n)->a_type;
				break;
			case AUTH:
				rex->ex_op = EXOP_STRING;
				rex->ex_str.s_ptr=(char *)NTOAUTH(n)->au.au_pw;
				rex->ex_str.s_len = RIP_AUTH_PW_LEN;
				break;
			default:
				return 0;
			}
		} else {
			switch (RIPFID(pf)) {
			case FAMILY:
				rex->ex_val = ntohs(n->n_family);
				break;
			case ADDRESS:
				rex->ex_val = ntohl(n->n_dst);
				break;
			case METRIC:
				rex->ex_val = ntohl(n->n_metric);
				break;
			case TAG:
				rex->ex_val = ntohs(n->n_tag);
				break;
			case MASK:
				rex->ex_val = ntohl(n->n_mask);
				break;
			case NHOP:
				rex->ex_val = ntohl(n->n_mask);
				break;
			default:
				return 0;
			}
		}
	}
	return 1;
}

static void
n_decode(int vers,
	 struct netinfo *n,
	 PacketView *pv)
{
	int i, j;
	struct in_addr addr;
	n_long mask;
	u_char c;
	char *p, buf[15+1+2+1+RIP_AUTH_PW_LEN*4];

	if (n->n_family == RIP_AF_AUTH) {
		NTOHS(NTOAUTH(n)->a_type);
		pv_showfield(pv, &NETFIELD(AUTHTYPE),
			     &NTOAUTH(n)->a_type,
			     "%-5u", NTOAUTH(n)->a_type);
		for (i = 0, p = buf; i < RIP_AUTH_PW_LEN; i++) {
			c = NTOAUTH(n)->au.au_pw[i];
			if (c >= ' ' && c < 0x7f && c != '\\') {
				*p++ = c;
			} else {
				if (c == '\0') {
					for (j = i+1; j < RIP_AUTH_PW_LEN; j++)
					    if (NTOAUTH(n)->au.au_pw[j])
						break;
					if (j == RIP_AUTH_PW_LEN)
					    break;
				}
				*p++ = '\\';
				switch (c) {
				case '\\':
					*p++ = '\\';
					break;
				case '\n':
					*p++= 'n';
					break;
				case '\r':
					*p++= 'r';
					break;
				case '\t':
					*p++ = 't';
					break;
				case '\b':
					*p++ = 'b';
					break;
				default:
					p += sprintf(p,"%o",c);
					break;
				}
			}
		}
		*p = '\0';
		pv_showfield(pv, &NETFIELD(AUTH), &NTOAUTH(n)->au.au_pw,
			     "\"%s\"", buf);
		return;
	}

	/*
	 * Suppress the family if =2, the tag if =0, and the next hop if 0
	 */
	NTOHS(n->n_family);
	pv_showfield(pv,
		     (n->n_family != 2
		      ? &NETFIELD(FAMILY)
		      : &HIDFIELD(FAMILY)),
		     &n->n_family,"%-3d",
		     n->n_family);

	NTOHS(n->n_tag);
	pv_showfield(pv, ((vers < RIPv2 || n->n_tag == 0)
			  ? &HIDFIELD(TAG)
			  : &NETFIELD(TAG)),
		     &n->n_tag, "%-5u", n->n_tag);

	mask = ntohl(n->n_mask);
	addr.s_addr = n->n_dst;
	if (vers < RIPv2) {
		pv_showfield(pv, &NETFIELD(ADDRESS), &addr,
			     "%-15.15s", inet_ntoa(addr));
		pv_showfield(pv,&HIDFIELD(MASK), &mask,
			     "%#08x", mask);
	} else if (mask + (mask & -mask) != 0
	    || (ntohl(n->n_dst) & ~mask) != 0) {
		pv_showfield(pv, &NETFIELD(ADDRESS), &addr,
			     "%-15.15s", inet_ntoa(addr));
		pv_showfield(pv,&NETFIELD(MASK), &mask,
			     "%#08x", mask);
	} else {
		for (i = 0; i != 32 && ((1<<i) & mask) == 0; i++)
			continue;
		(void)sprintf(buf, "%s/%d", inet_ntoa(addr), 32-i);
		pv_showfield(pv, &NETFIELD(ADDRESS), &addr,
			     "%-18.18s", buf);
		pv_showfield(pv, &HIDFIELD(MASK), &n->n_mask,
			     "%#08x", n->n_mask);
	}

	addr.s_addr = n->n_nhop;
	pv_showfield(pv, ((vers < RIPv2 || n->n_nhop == 0)
			  ? &HIDFIELD(NHOP)
			  : &NETFIELD(NHOP)),
		     &n->n_nhop,
		     "%-15.15s", inet_ntoa(addr));

	NTOHL(n->rip_metric);
	pv_showfield(pv, &NETFIELD(METRIC), &n->n_metric,
		     "%-3d", n->n_metric);
}


static void
rip_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct rip *rip;

	rip = ds_inline_rip(ds);
	if (rip == 0)
		return;
	pv_showfield(pv, &RIPFIELD(CMD), &rip->rip_cmd,
		     "%-8.8s", en_name(&cmdcode, rip->rip_cmd));
	pv_showfield(pv, &RIPFIELD(VERS), &rip->rip_vers,
		     "%-5d", rip->rip_vers);
	pv_showfield(pv, &RIPFIELD(RES1), &rip->rip_res1,
		     "%#-6x", ntohs(rip->rip_res1));
	if (rip->rip_vers >=  RIPv1) {
		switch (rip->rip_cmd) {
		case RIPCMD_REQUEST:
		case RIPCMD_RESPONSE: {
			int push, i, cc;
			char buf[20], tbuf[20];
			struct netinfo *n;

			push = (pv->pv_level >= RIPFIELD(NETS).pf_level);
			for (i = 0; (n = ds_inline_netinfo(ds)) != 0; i++) {
				if (push) {
					cc = sprintf(buf, "%s[%d]",
						     RIPFIELD(NETS).pf_name,i);
					(void) sprintf(tbuf, "%s[%d]",
						       RIPFIELD(NETS).pf_title,
						       i);
					pv_push(pv, &rip_proto, buf, cc, tbuf);
				}
				pv_break(pv);
				n_decode(rip->rip_vers, n, pv);
				if (push)
					pv_pop(pv);
			}
			break;
		  }
		  case RIPCMD_TRACEON:
			pv_showvarfield(pv, &RIPFIELD(TRACEFILE),
					rip->rip_tracefile, ds->ds_count,
					"%-*.*s",
					RIPFIELD(TRACEFILE).pf_cookie,
					ds->ds_count, rip->rip_tracefile);
			(void) ds_seek(ds, ds->ds_count, DS_RELATIVE);
			break;
		}
	}
}
