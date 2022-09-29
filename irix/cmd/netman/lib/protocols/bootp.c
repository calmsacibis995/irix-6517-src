/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Bootstrap Protocol (BOOTP), RFC 951, September 1985.
 */
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#define iaddr_t struct in_addr
#include <protocols/bootp.h>
#include "debug.h"
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"

char	bootpname[] = "bootp";

/*
 * Tailor certain <protocols/bootp.h> definitions.
 */
#define	vd_cname	vd_clntname
#undef	VM_STANFORD
#define	VM_STANFORD	0x5354414e	/* "STAN" */

#define	BOOTP_HDRLEN	sizeof(struct bootp)	/* header length in bytes */
#define	bootp_inline(ds) \
	((struct bootp *) ds_inline(ds, BOOTP_HDRLEN, sizeof(u_long)))

/*
 * BOOTP field identifiers and descriptors.
 */
enum bootpfid {
	OP, HTYPE, HLEN, HOPS, XID, SECS, UNUSED,
	CIADDR, YIADDR, SIADDR, GIADDR, CHADDR, SNAME, BFILE,
	MAGIC, FLAGS, CNAME
};

static ProtoField bootpfields[] = {
	PFI_UINT("op",    "Operation Code",         OP,     u_char, PV_TERSE),
	PFI_UINT("htype", "Hardware Address Type",  HTYPE,  u_char, PV_VERBOSE),
	PFI_UINT("hlen",  "Hardware Address Length",HLEN,   u_char, PV_VERBOSE),
	PFI_UINT("hops",  "Gateway Hops",           HOPS,   u_char, PV_DEFAULT),
	PFI_UINT("xid",   "Transaction ID",         XID,    u_long, PV_TERSE),
	PFI_UINT("secs",  "Seconds since Boot",     SECS,   u_short,PV_DEFAULT),
	PFI_UINT("unused","Unused",                 UNUSED, u_short,PV_VERBOSE),
	PFI_UINT("ciaddr","Client IP Address",      CIADDR, u_long, PV_TERSE),
	PFI_UINT("yiaddr","Your IP Address",        YIADDR, u_long, PV_TERSE),
	PFI_UINT("siaddr","Server IP Address",      SIADDR, u_long, PV_TERSE),
	PFI_UINT("giaddr","Gateway IP Address",     GIADDR, u_long, PV_TERSE),
	PFI_ADDR("chaddr","Client Hardware Address",CHADDR, 16,     PV_TERSE),
	PFI_BYTE("sname", "Server Host Name",       SNAME,  64,     PV_TERSE),
	PFI_BYTE("file",  "Boot File Name",         BFILE,  128,    PV_TERSE),
	PFI_UINT("magic", "Vendor Magic Number",    MAGIC,  u_long, PV_TERSE),
	PFI_UINT("flags", "Vendor Operation Code",  FLAGS,  u_long, PV_TERSE),
	PFI_BYTE("cname", "Client Host Name",       CNAME,  56,     PV_TERSE),
};

#define	BOOTPFID(pf)	((enum bootpfid) (pf)->pf_id)
#define	BOOTPFIELD(fid)	bootpfields[(int) fid]

/*
 * Nicknames and enumerated type names.
 */
static ProtoMacro bootpnicknames[] = {
	PMI("BOOTP",	bootpname),
};

static Enumeration opcode;
static Enumerator opcodevec[] = {
	EI_VAL("REQUEST",	BOOTREQUEST),
	EI_VAL("REPLY", 	BOOTREPLY),
};

static Enumeration magic;
static Enumerator magicvec[] = {
	EI_VAL("STANFORD",	VM_STANFORD),
	EI_VAL("SGI", 		VM_SGI),
	EI_VAL("AUTOREG",	VM_AUTOREG),
};

static Enumeration flags;
static Enumerator flagsvec[] = {
	EI_VAL("PCBOOT",	VF_PCBOOT),
	EI_VAL("HELP",		VF_HELP),
	EI_VAL("GET_IPADDR",	VF_GET_IPADDR),
	EI_VAL("RET_IPADDR",	VF_RET_IPADDR),
	EI_VAL("NEW_IPADDR",	VF_NEW_IPADDR),
};


/*
 * BOOTP protocol interface.
 */
#define	bootp_setopt	pr_stub_setopt
#define	bootp_embed	pr_stub_embed
#define	bootp_compile	pr_stub_compile
#define	bootp_match	pr_stub_match

DefineLeafProtocol(bootp, bootpname, "Bootstrap Protocol", PRID_BOOTP,
		   DS_BIG_ENDIAN, BOOTP_HDRLEN);

static int
bootp_init()
{
	struct servent *sp;

	if (!pr_register(&bootp_proto, bootpfields, lengthof(bootpfields),
			 lengthof(opcodevec) + lengthof(magicvec)
			 + lengthof(flagsvec))) {
		return 0;
	}
	sp = getservbyname(bootpname, "udp");
	if (!pr_nest(&bootp_proto, PRID_UDP, (sp ? sp->s_port : IPPORT_BOOTPS),
		     bootpnicknames, lengthof(bootpnicknames))) {
		return 0;
	}
	en_init(&opcode, opcodevec, lengthof(opcodevec), &bootp_proto);
	en_init(&magic,  magicvec,  lengthof(magicvec),  &bootp_proto);
	en_init(&flags,  flagsvec,  lengthof(flagsvec),  &bootp_proto);
	return 1;
}

/* ARGSUSED */
static Expr *
bootp_resolve(char *name, int len, struct snooper *sn)
{
	struct in_addr addr;
	Expr *ex;

	if (ip_hostaddr(name, IP_HOST, &addr)) {
		ex = expr(EXOP_NUMBER, EX_NULLARY, name);
		ex->ex_val = addr.s_addr;
		return ex;
	}
	return ex_string(name, len);
}

static int
bootp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct bootp *bp;

	bp = bootp_inline(ds);
	if (bp == 0) {
		int save, ok;

		if (BOOTPFID(pf) == CHADDR) {
			if (!ds_field(ds, &BOOTPFIELD(HLEN), 0, rex))
				return 0;
			save = pf->pf_size;
			pf->pf_size = rex->ex_val;
		}
		ok = ds_field(ds, pf, BOOTP_HDRLEN, rex);
		if (BOOTPFID(pf) == CHADDR)
			pf->pf_size = save;
		return ok;
	}

	rex->ex_op = EXOP_NUMBER;
	switch (BOOTPFID(pf)) {
	  case OP:
		rex->ex_val = bp->bp_op;
		break;
	  case HTYPE:
		rex->ex_val = bp->bp_htype;
		break;
	  case HLEN:
		rex->ex_val = bp->bp_hlen;
		break;
	  case HOPS:
		rex->ex_val = bp->bp_hops;
		break;
	  case XID:
		rex->ex_val = ntohl(bp->bp_xid);
		break;
	  case SECS:
		rex->ex_val = ntohs(bp->bp_secs);
		break;
	  case UNUSED:
		rex->ex_val = ntohs(bp->bp_unused);
		break;
	  case CIADDR:
		rex->ex_val = ntohl(bp->bp_ciaddr.s_addr);
		break;
	  case YIADDR:
		rex->ex_val = ntohl(bp->bp_yiaddr.s_addr);
		break;
	  case SIADDR:
		rex->ex_val = ntohl(bp->bp_siaddr.s_addr);
		break;
	  case GIADDR:
		rex->ex_val = ntohl(bp->bp_giaddr.s_addr);
		break;
	  case CHADDR:
		if (bp->bp_hlen <= sizeof rex->ex_addr) {
			bcopy(bp->bp_chaddr,
			      A_BASE(&rex->ex_addr, bp->bp_hlen),
			      bp->bp_hlen);
			rex->ex_op = EXOP_ADDRESS;
		} else {
			rex->ex_str.s_ptr = (char *) bp->bp_chaddr;
			rex->ex_str.s_len = bp->bp_hlen;
			rex->ex_op = EXOP_STRING;
		}
		break;
	  case SNAME:
		rex->ex_str.s_ptr = (char *) bp->bp_sname;
		goto gotstring;
	  case BFILE:
		rex->ex_str.s_ptr = (char *) bp->bp_file;
		goto gotstring;
	  case MAGIC:
		rex->ex_val = ntohl(*(u_long *)bp->vd_magic);
		break;
	  case FLAGS:
		rex->ex_val = ntohl(bp->vd_flags);
		break;
	  case CNAME:
		rex->ex_str.s_ptr = (char *) bp->vd_cname;
gotstring:
		rex->ex_str.s_len = strlen(rex->ex_str.s_ptr);
		if (rex->ex_str.s_len > pf->pf_size)
			rex->ex_str.s_len = pf->pf_size;
		rex->ex_op = EXOP_STRING;
	}
	return 1;
}

static void
bootp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct bootp *bp, hdr;
	u_char *ap;
	char *cp;
	char buf[3 * sizeof bp->bp_chaddr];
	int rem;
	u_long vm;

	bp = bootp_inline(ds);
	if (bp == 0) {
		bp = &hdr;
		(void) ds_bytes(ds, bp, MIN(BOOTP_HDRLEN, ds->ds_count));
	}
	bp->bp_xid = ntohl(bp->bp_xid);
	bp->bp_secs = ntohs(bp->bp_secs);
	bp->bp_unused = ntohs(bp->bp_unused);
	bp->bp_ciaddr.s_addr = ntohl(bp->bp_ciaddr.s_addr);
	bp->bp_yiaddr.s_addr = ntohl(bp->bp_yiaddr.s_addr);
	bp->bp_siaddr.s_addr = ntohl(bp->bp_siaddr.s_addr);
	bp->bp_giaddr.s_addr = ntohl(bp->bp_giaddr.s_addr);

	pv_showfield(pv, &BOOTPFIELD(OP), &bp->bp_op,
		     "%-8.8s", en_name(&opcode, bp->bp_op));
	pv_showfield(pv, &BOOTPFIELD(HTYPE),  &bp->bp_htype,
		     "%-5d", bp->bp_htype);
	pv_showfield(pv, &BOOTPFIELD(HLEN),  &bp->bp_hlen,
		     "%-5d", bp->bp_hlen);
	pv_showfield(pv, &BOOTPFIELD(HOPS),  &bp->bp_hops,
		     "%-5d", bp->bp_hops);
	pv_showfield(pv, &BOOTPFIELD(XID),  &bp->bp_xid,
		     "%#10x", bp->bp_xid);
	pv_showfield(pv, &BOOTPFIELD(SECS),  &bp->bp_secs,
		     "%-5d", bp->bp_secs);
	pv_showfield(pv, &BOOTPFIELD(UNUSED),  &bp->bp_unused,
		     "%#6x", bp->bp_unused);

	pv_break(pv);
	pv_showfield(pv, &BOOTPFIELD(CIADDR),  &bp->bp_ciaddr,
		     "%-20.20s", ip_hostname(bp->bp_ciaddr, IP_HOST));
	pv_showfield(pv, &BOOTPFIELD(YIADDR),  &bp->bp_yiaddr,
		     "%-20.20s", ip_hostname(bp->bp_yiaddr, IP_HOST));
	pv_showfield(pv, &BOOTPFIELD(SIADDR),  &bp->bp_siaddr,
		     "%-20.20s", ip_hostname(bp->bp_siaddr, IP_HOST));
	pv_showfield(pv, &BOOTPFIELD(GIADDR),  &bp->bp_giaddr,
		     "%-20.20s", ip_hostname(bp->bp_giaddr, IP_HOST));

	ap = bp->bp_chaddr;
	cp = buf;
	for (rem = MIN(bp->bp_hlen, sizeof bp->bp_chaddr); --rem >= 0; ap++)
		cp += (int) sprintf(cp, "%x:", *ap);
	*--cp = '\0';
	pv_showfield(pv, &BOOTPFIELD(CHADDR), bp->bp_chaddr, "%-17s", buf);
	pv_showfield(pv, &BOOTPFIELD(SNAME), bp->bp_sname,
		     "%-60.*s", BOOTPFIELD(SNAME).pf_size, bp->bp_sname);
	pv_showfield(pv, &BOOTPFIELD(BFILE), bp->bp_file,
		     "%-60.*s", BOOTPFIELD(BFILE).pf_size, bp->bp_file);

	vm = *(u_long *)bp->vd_magic;
	if (vm == 0) {
		if (pv->pv_level > PV_VERBOSE)
			pv_hexdump(pv, bp->bp_vend, 0, sizeof bp->bp_vend);
	} else {
		pv_showfield(pv, &BOOTPFIELD(MAGIC), bp->vd_magic,
			     "%-8.8s", en_name(&magic, vm));
		switch (vm) {
		  case VM_STANFORD:
		  case VM_SGI:
		  case VM_AUTOREG:
			pv_showfield(pv, &BOOTPFIELD(FLAGS), &bp->vd_flags,
				     "%-10.10s", en_name(&flags, bp->vd_flags));
			if (vm == VM_STANFORD) {
				if (pv->pv_level > PV_VERBOSE) {
					pv_hexdump(pv, bp->vd_cname, 0,
						   sizeof bp->vd_cname);
				}
			} else {
				pv_showfield(pv, &BOOTPFIELD(CNAME),
					     bp->vd_cname, "-20.*s",
					     BOOTPFIELD(CNAME).pf_size,
					     bp->vd_cname);
			}
			break;
		  default:
			if (pv->pv_level > PV_VERBOSE) {
				pv_hexdump(pv, &bp->bp_vend[4], 0,
					   sizeof bp->bp_vend - 4);
			}
		}
	}
}
