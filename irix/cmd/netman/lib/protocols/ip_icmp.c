/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Control Message Protocol (ICMP), defined in RFCs 777 and 792.
 */
#include <netdb.h>
#include <sys/types.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"
#include <netinet/ip_icmp.h>

/* These are for sun */
#ifndef ICMP_ROUTERADVERT
#define ICMP_ROUTERADVERT       9		/* router advertisement */
#endif
#ifndef ICMP_ROUTERSOLICIT
#define ICMP_ROUTERSOLICIT      10		/* router solicitation */
#endif
/* router advertisement ICMP packet */
struct icmp_ad {
	u_int8_t    icmp_type;		/* type of message */
	u_int8_t    icmp_code;		/* type sub code */
	u_int16_t   icmp_cksum;		/* ones complement cksum of struct */
	u_int8_t    icmp_ad_num;	/* # of following router addresses */
	u_int8_t    icmp_ad_asize;	/* 2--words in each advertisement */
	u_int16_t   icmp_ad_life;	/* seconds of validity */
	struct icmp_ad_info {
	    struct in_addr  ad_addr;
	    n_long  ad_pref;
	} icmp_ad_info[1];
};
#define ADVERT(icmp) ((struct icmp_ad *)(icmp))

char	icmpname[] = "icmp";

enum icmpfid {
	TYPE, CODE, CKSUM,
	PPTR, UNUSED, GWADDR, ID, SEQ, VOID, PMVOID, NEXTMTU,
	OTIME, RTIME, TTIME, MASK,
	AD_NUM, AD_ASIZE, AD_LIFE, AD_ADDR, AD_PREF
};

static ProtoField icmpfields[] = {
	PFI_UINT("type",   "Type",		  TYPE,   u_char,  PV_TERSE),
	PFI_UINT("code",   "Code",		  CODE,	  u_char,  PV_TERSE),
	PFI_UINT("cksum",  "Checksum",		  CKSUM,  u_short, PV_VERBOSE),
	PFI_UINT("pptr",   "Pointer",		  PPTR,	  u_char,  PV_TERSE),
	PFI_BYTE("unused", "Unused",		  UNUSED, 3,	   PV_VERBOSE),
	PFI_UINT("gwaddr", "Gateway Address",	  GWADDR, u_long,  PV_TERSE),
	PFI_UINT("id",     "Identifier",	  ID,	  u_short, PV_TERSE),
	PFI_UINT("seq",    "Sequence Number",	  SEQ,	  u_short, PV_TERSE),
	PFI_UINT("void",   "Void",		  VOID,   u_long,  PV_VERBOSE),
	PFI_UINT("pmvoid", "Void",		  PMVOID, u_short, PV_VERBOSE),
	PFI_UINT("nextmtu","Next-hop MTU",	  NEXTMTU,u_short, PV_TERSE),
	PFI_UINT("otime",  "Originate Timestamp", OTIME,  n_time,  PV_TERSE),
	PFI_UINT("rtime",  "Receive Timestamp",   RTIME,  n_time,  PV_TERSE),
	PFI_UINT("ttime",  "Transmit Timestamp",  TTIME,  n_time,  PV_TERSE),
	PFI_UINT("mask",   "Address Mask",	  MASK,   u_long,  PV_TERSE),
	PFI_UINT("num",	   "Num routes",	  AD_NUM, u_char,  PV_VERBOSE),
	PFI_UINT("asize",  "Adv Sizes",		  AD_ASIZE,u_char, PV_VERBOSE),
	PFI_UINT("life",   "Adv lifetime",	  AD_LIFE,u_short, PV_TERSE),
	PFI_UINT("gwaddr", "Default Gateway",	  AD_ADDR,u_long,  PV_TERSE),
	PFI_UINT("pref",   "Gateway Preference",  AD_PREF,u_long,  PV_TERSE),
};

#define	ICMPFID(pf)	((enum icmpfid) (pf)->pf_id)
#define	ICMPFIELD(fid)	icmpfields[(int) fid]

static ProtoMacro icmpnicknames[] = {
	PMI("ICMP",	icmpname),
};

static Enumeration types;
static Enumerator typesvec[] = {
	EI_VAL("ECHOREPLY",	ICMP_ECHOREPLY),
	EI_VAL("UNREACHABLE",	ICMP_UNREACH),
	EI_VAL("SOURCEQUENCH",	ICMP_SOURCEQUENCH),
	EI_VAL("REDIRECT",	ICMP_REDIRECT),
	EI_VAL("ECHO",		ICMP_ECHO),
	EI_VAL("ROUTERADVERT",	ICMP_ROUTERADVERT),
	EI_VAL("ROUTERSOLICIT",	ICMP_ROUTERSOLICIT),
	EI_VAL("TIMXCEED",	ICMP_TIMXCEED),
	EI_VAL("PARAMPROB",	ICMP_PARAMPROB),
	EI_VAL("TSTAMP",	ICMP_TSTAMP),
	EI_VAL("TSTAMPREPLY",	ICMP_TSTAMPREPLY),
	EI_VAL("IREQ",		ICMP_IREQ),
	EI_VAL("IREQREPLY",	ICMP_IREQREPLY),
	EI_VAL("MASKREQ",	ICMP_MASKREQ),
	EI_VAL("MASKREPLY",	ICMP_MASKREPLY),
};

static Enumeration unreachcodes;
static Enumerator unreachcodesvec[] = {
	EI_VAL("NET",		ICMP_UNREACH_NET),
	EI_VAL("HOST",		ICMP_UNREACH_HOST),
	EI_VAL("PORT",		ICMP_UNREACH_PORT),
	EI_VAL("PROTOCOL",	ICMP_UNREACH_PROTOCOL),
	EI_VAL("NEEDFRAG",	ICMP_UNREACH_NEEDFRAG),
	EI_VAL("SRCFAIL",	ICMP_UNREACH_SRCFAIL),
};

static Enumeration redirectcodes;
static Enumerator redirectcodesvec[] = {
	EI_VAL("NET",		ICMP_REDIRECT_NET),
	EI_VAL("HOST",		ICMP_REDIRECT_HOST),
	EI_VAL("TOSNET",	ICMP_REDIRECT_TOSNET),
	EI_VAL("TOSHOST",	ICMP_REDIRECT_TOSHOST),
};

static Enumeration timxceedcodes;
static Enumerator timxceedcodesvec[] = {
	EI_VAL("INTRANS",	ICMP_TIMXCEED_INTRANS),
	EI_VAL("REASS",		ICMP_TIMXCEED_REASS),
};


/*
 * ICMP protocol interface.  Borrow resolve and decode (the latter via
 * ip_proto.prop_decode) from the IP module.
 */
extern Expr	*ip_resolve(char *, int, struct snooper *);
extern Protocol	ip_proto;

#define	icmp_setopt	pr_stub_setopt
#define	icmp_embed	pr_stub_embed
#define	icmp_resolve	ip_resolve
#define	icmp_compile	pr_stub_compile
#define	icmp_match	pr_stub_match

DefineLeafProtocol(icmp, icmpname, "Internet Control Message Protocol",
		   PRID_ICMP, DS_BIG_ENDIAN, ICMP_MINLEN);

static int
icmp_init()
{
	struct protoent *pp;

	if (!pr_register(&icmp_proto, icmpfields, lengthof(icmpfields),
			 lengthof(typesvec)
			 + lengthof(unreachcodesvec)
			 + lengthof(redirectcodesvec)
			 + lengthof(timxceedcodesvec))) {
		return 0;
	}
	pp = getprotobyname(icmpname);
	if (!pr_nest(&icmp_proto, PRID_IP, (pp ? pp->p_proto : IPPROTO_ICMP),
		     icmpnicknames, lengthof(icmpnicknames))) {
		return 0;
	}
	en_init(&types, typesvec, lengthof(typesvec), &icmp_proto);
	en_init(&unreachcodes, unreachcodesvec, lengthof(unreachcodesvec),
		&icmp_proto);
	en_init(&redirectcodes, redirectcodesvec, lengthof(redirectcodesvec),
		&icmp_proto);
	en_init(&timxceedcodes, timxceedcodesvec, lengthof(timxceedcodesvec),
		&icmp_proto);
	return 1;
}

static int
icmp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct icmp *icmp;

	icmp = (struct icmp *) ds_inline(ds, ICMP_MINLEN, sizeof(u_short));
	if (icmp == 0) {
		if (pf->pf_id > (int) CKSUM)
			return 0;
		return ds_field(ds, pf, ICMP_MINLEN, rex);
	}
	rex->ex_op = EXOP_NUMBER;
	switch (ICMPFID(pf)) {
	  case TYPE:
		rex->ex_val = icmp->icmp_type;
		break;
	  case CODE:
		rex->ex_val = icmp->icmp_code;
		break;
	  case CKSUM:
		rex->ex_val = ntohs(icmp->icmp_cksum);
		break;
	  default:
		return 0;
	}
	return 1;
}

static void
icmp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct icmp *icmp, hdr;
	u_short sum, icmpsum;
	Enumeration *codes;

	icmp = (struct icmp *) ds_inline(ds, ICMP_MINLEN, sizeof(u_short));
	if (icmp == 0) {
		icmp = &hdr;
		(void) ds_bytes(ds, icmp, MIN(ICMP_MINLEN, ds->ds_count));
		sum = 0;
	} else {
		icmpsum = icmp->icmp_cksum;
		icmp->icmp_cksum = 0;
		if (!ip_checksum_frame(PS_TOP(ps,struct ipframe), (char *)icmp,
				       ICMP_MINLEN + ds->ds_count, &sum)) {
			sum = 0;
		}
		icmp->icmp_cksum = icmpsum;
	}

	pv_showfield(pv, &ICMPFIELD(TYPE), &icmp->icmp_type,
		     "%-12.12s", en_name(&types, icmp->icmp_type));
	switch (icmp->icmp_type) {
	  case ICMP_UNREACH:
		codes = &unreachcodes;
		break;
	  case ICMP_REDIRECT:
		codes = &redirectcodes;
		break;
	  case ICMP_TIMXCEED:
		codes = &timxceedcodes;
		break;
	  default:
		codes = 0;
	}
	if (codes) {
		pv_showfield(pv, &ICMPFIELD(CODE), &icmp->icmp_code,
			     "%-8.8s", en_name(codes, icmp->icmp_code));
	} else {
		pv_showfield(pv, &ICMPFIELD(CODE), &icmp->icmp_code,
			     "@d%-3u", icmp->icmp_code);
	}

	icmpsum = ntohs(icmpsum);
	if (sum && icmpsum != sum) {
		pv_showfield(pv, &ICMPFIELD(CKSUM), &icmpsum,
			     "%#x [!= %#x]", icmpsum, sum);
	} else {
		pv_showfield(pv, &ICMPFIELD(CKSUM), &icmpsum,
			     "%#-6x", icmpsum);
	}

	switch (icmp->icmp_type) {
	  case ICMP_UNREACH:
		if (icmp->icmp_code == ICMP_UNREACH_NEEDFRAG) {
			icmp->icmp_pmvoid = ntohs(icmp->icmp_pmvoid);
			pv_showfield(pv,&ICMPFIELD(PMVOID), &icmp->icmp_pmvoid,
				     "%#-5x", icmp->icmp_pmvoid);
			icmp->icmp_nextmtu = ntohs(icmp->icmp_nextmtu);
			pv_showfield(pv, &ICMPFIELD(NEXTMTU),
				     &icmp->icmp_nextmtu,
				     "%-5u", icmp->icmp_nextmtu);
		} else {
			icmp->icmp_void = ntohl((u_long)icmp->icmp_void);
			pv_showfield(pv, &ICMPFIELD(VOID), &icmp->icmp_void,
				     "%#-10x", icmp->icmp_void);
		}
		break;
	  case ICMP_PARAMPROB:
		pv_showfield(pv, &ICMPFIELD(PPTR), &icmp->icmp_pptr,
			     "%-3u", icmp->icmp_pptr);
		pv_showfield(pv, &ICMPFIELD(UNUSED), &icmp->icmp_pptr + 1,
			     "%#-8x", icmp->icmp_void & 0xffffff);
		break;
	  case ICMP_REDIRECT:
		NTOHL(icmp->icmp_gwaddr.s_addr);
		pv_showfield(pv, &ICMPFIELD(GWADDR), &icmp->icmp_gwaddr,
			     "%-25.25s",
			     ip_hostname(icmp->icmp_gwaddr, IP_HOST));
		break;
	  case ICMP_ECHOREPLY:
	  case ICMP_ECHO:
	  case ICMP_TSTAMP:
	  case ICMP_TSTAMPREPLY:
	  case ICMP_IREQ:
	  case ICMP_IREQREPLY:
	  case ICMP_MASKREQ:
	  case ICMP_MASKREPLY:
		icmp->icmp_id = ntohs(icmp->icmp_id);
		pv_showfield(pv, &ICMPFIELD(ID), &icmp->icmp_id,
			     "%-5u", icmp->icmp_id);
		icmp->icmp_seq = ntohs(icmp->icmp_seq);
		pv_showfield(pv, &ICMPFIELD(SEQ), &icmp->icmp_seq,
			     "%-5u", icmp->icmp_seq);
		break;
	  case ICMP_ROUTERADVERT:
		pv_showfield(pv, &ICMPFIELD(AD_NUM),
			     &ADVERT(icmp)->icmp_ad_num,
			     "%-3u", ADVERT(icmp)->icmp_ad_num);
		pv_showfield(pv, &ICMPFIELD(AD_ASIZE),
			     &ADVERT(icmp)->icmp_ad_asize,
			     "%-3u", ADVERT(icmp)->icmp_ad_asize);
		pv_showfield(pv, &ICMPFIELD(AD_LIFE),
			     &ADVERT(icmp)->icmp_ad_life,
			     "%-5u", ADVERT(icmp)->icmp_ad_life);
		for (;;) {
			struct icmp_ad_info *rt;

			rt = (struct icmp_ad_info *)ds_inline(ds, sizeof(*rt),
							sizeof(long));
			if (!rt)
				break;
			pv_break(pv);
			NTOHL(rt->ad_addr);
			pv_showfield(pv, &ICMPFIELD(AD_ADDR),
				     &rt->ad_addr, "%-25.25s",
				     ip_hostname(rt->ad_addr, IP_HOST));
			pv_showfield(pv, &ICMPFIELD(AD_PREF),
				     &rt->ad_pref, "%-3u", rt->ad_pref);
		}
		break;
	  default:
		icmp->icmp_void = ntohl((u_long)icmp->icmp_void);
		pv_showfield(pv, &ICMPFIELD(VOID), &icmp->icmp_void,
			     "%#-10x", icmp->icmp_void);
	}
	pv_break(pv);

	switch (icmp->icmp_type) {
	  case ICMP_UNREACH:
	  case ICMP_TIMXCEED:
	  case ICMP_PARAMPROB:
	  case ICMP_SOURCEQUENCH:
	  case ICMP_REDIRECT:
		pv_decodeframe(pv, &ip_proto, ds, ps);
		break;
	  case ICMP_TSTAMP:
	  case ICMP_TSTAMPREPLY:
		(void) ds_n_time(ds, &icmp->icmp_otime);
		pv_showfield(pv, &ICMPFIELD(OTIME), &icmp->icmp_otime,
			     ip_timestamp(icmp->icmp_otime));
		(void) ds_n_time(ds, &icmp->icmp_rtime);
		pv_showfield(pv, &ICMPFIELD(RTIME), &icmp->icmp_rtime,
			     ip_timestamp(icmp->icmp_rtime));
		(void) ds_n_time(ds, &icmp->icmp_ttime);
		pv_showfield(pv, &ICMPFIELD(TTIME), &icmp->icmp_ttime,
			     ip_timestamp(icmp->icmp_ttime));
		break;
	  case ICMP_MASKREQ:
	  case ICMP_MASKREPLY:
		(void) ds_u_long(ds, &icmp->icmp_mask);
		pv_showfield(pv, &ICMPFIELD(MASK), &icmp->icmp_mask,
			     "%-8X", icmp->icmp_mask);
		break;
	}
}
