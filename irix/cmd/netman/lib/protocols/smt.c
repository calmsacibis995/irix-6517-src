/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Module for SMT X3T9.5/84-89 Rev 6.2 (protocol version 1).
 */

#include <bstring.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>		/* for ntohs() */
#include "index.h"
#include "enum.h"
#include "protodefs.h"
#include "strings.h"
#include "protocols/smt.h"

char smtname[] = "smt";
extern char *fddi_ntoa();

/*
 * SMT field identifiers and descriptors.
 */
enum smtfid {
    CLASS, TYPE, VERS, XID, SID, PAD, ILEN, PTYPE, PLEN, PPAD,
    UNA,
    NCLASS, MAC_CT, NONMAST_CT, MAST_CT,
    TOPOL, DUPLADDR,
    MACIDX, MUNA, MDNA
};

static ProtoField smtfields[] = {
    PFI_UINT("class",	"Frame Class",		CLASS,	u_char,  PV_TERSE),
    PFI_UINT("type",	"Frame Type",		TYPE,	u_char,	 PV_TERSE),
    PFI_UINT("vers",	"Version",		VERS,	u_short, PV_VERBOSE),
    PFI_UINT("xid",	"Transaction ID",	XID,	u_long,  PV_TERSE),
    PFI_ADDR("sid",	"Station ID",		SID,	SIDLEN,  PV_VERBOSE),
    PFI_UINT("pad",	"Pad",			PAD,	u_short, PV_VERBOSE+1),
    PFI_UINT("infolen",	"Info Field Length",	ILEN,	u_short, PV_VERBOSE),
    PFI_UINT("ptype",	"Param Type",		PTYPE,	u_short, PV_DEFAULT),
    PFI_UINT("plen",	"Param Length",		PLEN,	u_short, PV_VERBOSE),
    PFI_UINT("ppad",	"Param Pad",		PPAD,	u_short, PV_VERBOSE+1),

    PFI_ADDR("una",	"Upstream Nbr Addr",	UNA,	UNALEN,  PV_VERBOSE),

    PFI_UINT("nodeclass","Node Class",		NCLASS,	u_char,  PV_VERBOSE),
    PFI_UINT("mac_ct",	"MAC Count",		MAC_CT,	u_char,  PV_VERBOSE),
    PFI_UINT("nonmaster_ct","Nonmaster Count",	NONMAST_CT, u_char, PV_VERBOSE),
    PFI_UINT("master_ct", "Master Count",	MAST_CT, u_char,  PV_VERBOSE),

    PFI_UINT("topology", "Station Topology",	TOPOL,	u_char,	 PV_VERBOSE),
    PFI_UINT("dupladdr", "Duplicate Address",	DUPLADDR, u_char, PV_VERBOSE),

    PFI_UINT("macindex","MAC Index",		MACIDX,	u_short, PV_VERBOSE),
    PFI_ADDR("una",	"Upstream Nbr Addr",	MUNA,	UNALEN,  PV_VERBOSE),
    PFI_ADDR("dna",	"Downstream Nbr Addr",	MDNA,	UNALEN,  PV_VERBOSE),
};

#define doshowhex(pv)	(pv->pv_level > PV_DEFAULT)

#define	SMTFID(pf)	((enum smtfid) (pf)->pf_id)
#define	SMTFIELD(fid)	smtfields[(int) fid]

static Enumeration classes;
static Enumerator classvec[] = {
    EI_VAL("NIF",	CL_NIF),
    EI_VAL("CSIF",	CL_CSIF),
    EI_VAL("OSIF",	CL_OSIF),
    EI_VAL("ECF",	CL_ECF),
    EI_VAL("RAF",	CL_RAF),
    EI_VAL("RDF",	CL_RDF),
    EI_VAL("SRF",	CL_SRF),
    EI_VAL("GETPMF",	CL_GETPMF),
    EI_VAL("CHPMF",	CL_CHPMF),
    EI_VAL("ADDPMF",	CL_ADDPMF),
    EI_VAL("RMPMF",	CL_RMPMF),
    EI_VAL("XSF",	CL_XSF),
};

static Enumeration types;
static Enumerator typevec[] = {
    EI_VAL("ANNOUNCEMENT",	TY_ANNOUNCE),
    EI_VAL("REQUEST",		TY_REQ),
    EI_VAL("RESPONSE",		TY_RESP),
};


static Enumeration ptypes;
static Enumerator ptypevec[] = {
    EI_VAL("Upstream Nbr Address",	PTY_UNA),
    EI_VAL("Station Descriptor",	PTY_SD),
    EI_VAL("Station State",		PTY_SS),
    EI_VAL("Msg Timestamp",		PTY_TS),
    EI_VAL("Station Policies",		PTY_SP),
    EI_VAL("MAC Nbrs",			PTY_MACNBR),
    EI_VAL("Path Descriptor",		PTY_PD),
    EI_VAL("MAC Status",		PTY_MACST),
    EI_VAL("PORT LER Status",		PTY_PLER),
    EI_VAL("MAC Frame Counters",	PTY_FRCNT),
    EI_VAL("MAC Frame Not Copied Cnt",	PTY_FRNTCP),
    EI_VAL("MAC Priority",		PTY_PRI),
    EI_VAL("PORT EB Status",		PTY_PORTEB),
    EI_VAL("Manufacturer Field",	PTY_MANUF),
    EI_VAL("User Field",		PTY_USER),
    EI_VAL("Echo Data",			PTY_ECHO),
    EI_VAL("Reason Code",		PTY_REAS),
    EI_VAL("Reject Frame Begin",	PTY_REJF),
    EI_VAL("Supported Versions",	PTY_SUPVER),
    EI_VAL("Frame Status Capabilities",	PTY_FCS),
    EI_VAL("ESF ID",			PTY_ESFID),
};


static Enumerator topolbits[] = {
    EI_VAL("WRAPPED",		TPL_STWRAP),
    EI_VAL("UNROOT_CONC",	TPL_UNRCON),
    EI_VAL("AA_TWIST",		TPL_AATW),
    EI_VAL("BB_TWIST",		TPL_BBTW),
    EI_VAL("ROOTED_STN",	TPL_RTST),
    EI_VAL("DO_SRF",		TPL_SR),
};

static Enumerator duplbits[] = {
    EI_VAL("FRAME",	DUP_MYDUP),
    EI_VAL("UNA",	DUP_MYUNADUP),
};

static ProtoMacro smtnicknames[] = {
    PMI("SMT",	smtname),
};

static ProtoMacro smtmacros[] = {
    PMI("nif",		"class=NIF"),
    PMI("sif",		"class=CSIF || class=OSIF"),
    PMI("csif",		"class=CSIF"),
    PMI("osif",		"class=OSIF"),
    PMI("ecf",		"class=ECF"),
    PMI("raf",		"class=RAF"),
    PMI("rdf",		"class=RDF"),
    PMI("srf",		"class=SRF"),
    PMI("getpmf",	"class=GETPMF"),
    PMI("chpmf",	"class=CHPMF"),
    PMI("addpmf",	"class=ADDPMF"),
    PMI("rmpmf",	"class=RMPMF"),
    PMI("pmf",		"class=GETPMF||class=CHPMF||class=ADDPMF||class=RMPMF"),
    PMI("xsf",		"class=XSF"),
    PMI("request",	"type=REQUEST"),
    PMI("response",	"type=RESPONSE"),
    PMI("announce",	"type=ANNOUNCEMENT"),
};

/*
 * SMT protocol interface.
 */
#define smt_setopt	pr_stub_setopt
#define smt_embed	pr_stub_embed
#define	smt_match	pr_stub_match

DefineLeafProtocol(smt, smtname, "Station Management Protocol", PRID_SMT,
		     DS_BIG_ENDIAN, sizeof(smt_header));

static int
smt_init()
{
	if (!pr_register(&smt_proto, smtfields, lengthof(smtfields),
			 lengthof(typevec) + lengthof(ptypevec) +
			 lengthof(topolbits) + lengthof(duplbits) +
			 lengthof(smtmacros) +
			 lengthof(classvec)))
		return 0;

	if (!pr_nest(&smt_proto, PRID_FDDI, PRID_SMT,
			smtnicknames, lengthof(smtnicknames)))
		return 0;

	en_init(&types,    typevec,    lengthof(typevec),    &smt_proto);
	en_init(&ptypes,   ptypevec,   lengthof(ptypevec),   &smt_proto);
	en_init(&classes,  classvec,   lengthof(classvec),   &smt_proto);
	pr_addnumbers(&smt_proto, topolbits, lengthof(topolbits));
	pr_addnumbers(&smt_proto, duplbits, lengthof(duplbits));
	pr_addmacros(&smt_proto, smtmacros, lengthof(smtmacros));
	return 1;
}

static ExprType
smt_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	enum smtfid fid;

	fid = SMTFID(pf);
	switch (fid) {
	  case CLASS:
	  case TYPE: 
	  case VERS:
	  case XID:
	  case PAD:
	  case ILEN: {
		long mask;

		if (!pc_intmask(pc, mex, &mask))
			return ET_ERROR;
		if (tex->ex_op != EXOP_NUMBER) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, mask, tex->ex_val,
				 sizeof(struct llc))) {
			return ET_COMPLEX;
		}
		break;
	  }


	  case SID: {
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
				  sizeof(struct llc))) {
			return ET_COMPLEX;
		}
		break;
	  }
	}
	return ET_SIMPLE;
}

/* ARGSUSED */
Expr *
smt_resolve(char *name, int len, struct snooper *sn)
{
	struct sid {
	    u_char b[8];
	} addr;
	Expr *ex;
	int n, v[sizeof(addr)];
	char *bp, *ep;
	unsigned char *vp;

	n = 0;
	bp = name;
	for (;;) {
		v[n] = strtol(bp, &ep, 16);
		if (bp == ep)
			return 0;
		if (*ep == '\0')
			break;
		if (*ep != '-' || ++n >= sizeof(addr))
			return 0;
		bp = ep + 1;
	}
	vp = &addr.b[sizeof(addr)];
	while (n >= 0)
		*--vp = v[n--];
	while (vp > &addr.b[0])
		*--vp = 0;
	ex = expr(EXOP_ADDRESS, EX_NULLARY, name);
	*A_CAST(&ex->ex_addr, struct sid) = addr;
	return ex;
}

/* ARGSUSED */
static int
smt_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	smt_header *sh;

	sh = (smt_header *)ds_inline(ds, sizeof(*sh), sizeof(u_long));
	if (sh == 0)
		return 0;
	switch (SMTFID(pf)) {
	  case CLASS:
		rex->ex_val = sh->fclass;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case TYPE:
		rex->ex_val = sh->ftype;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case VERS:
		rex->ex_val = ntohs(sh->vers);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case XID:
		rex->ex_val = ntohl(sh->xid);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case SID:
		bcopy(sh->sid, A_BASE(&rex->ex_addr, pf->pf_size),
		      pf->pf_size);
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case PAD:
		rex->ex_val = ntohs(sh->pad);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case ILEN:
		rex->ex_val = ntohs(sh->ilen);
		rex->ex_op = EXOP_NUMBER;
	}
	return 1;
}

static char digits[] = "0123456789abcdef";

/*
 * Convert STATION address to printable (loggable) representation.
 */

static char *
sidtoa(register u_char *ap, int len)
{
	register char *cp;
	register i;
	static char sidbuf[SIDLEN*3];

	if (ap == 0)
		return "????";
	cp = sidbuf;
	for (i = len; i > 0; i--) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = '-';
	}
	*--cp = '\0';
	return(sidbuf);
}

#define SMT_HEX_LEN	16

static void
smt_showhex(PacketView *pv, register u_char *ap, int len)
{
	int off, showoff, n, grab;
	char buf[10 + SMT_HEX_LEN * 3 + 1];
	register char *cp;

	showoff = len > SMT_HEX_LEN;
	off = 0;
	for (; len > 0; len -= grab) {
		cp = buf;
		*cp = '\0';
		grab = (len > SMT_HEX_LEN) ? SMT_HEX_LEN : len;
		if (showoff) {
			cp  += (int) sprintf(buf, "%05d:  ", off);
			off += grab;
		}
		for (n = grab; --n >= 0; ap++) {
			*cp++ = digits[*ap >> 4];
			*cp++ = digits[*ap & 0xf];
			*cp++ = ' ';
		}
		*--cp = '\0';
		pv_break(pv);
		pv_text(pv, buf, strlen(buf));
	}
}

static void
smt_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	smt_header *sh, hdr;
	u_short type, len, pad, *usp;
	char *tname;

	sh = (smt_header *)ds_inline(ds, sizeof(*sh), sizeof(u_long));
	if (sh == 0) {
		sh = &hdr;
		(void) ds_bytes(ds, sh, MIN(sizeof(*sh), ds->ds_count));
	}

	pv_showfield(pv, &SMTFIELD(CLASS), &sh->fclass,
		     "%-8.8s", en_name(&classes, sh->fclass));
	pv_showfield(pv, &SMTFIELD(TYPE), &sh->ftype,
		     "%-8.8s", en_name(&types, sh->ftype));
	pv_showfield(pv, &SMTFIELD(VERS), &sh->vers, "%d", sh->vers);
	pv_break(pv);
	pv_showfield(pv, &SMTFIELD(XID), &sh->xid, "%#04x", sh->xid);
	pv_showfield(pv, &SMTFIELD(SID), sh->sid,
		     "%-25.25s", sidtoa(sh->sid, SIDLEN));
	pv_break(pv);
	pv_showfield(pv, &SMTFIELD(PAD), &sh->pad, "%#04x", sh->pad);
	pv_showfield(pv, &SMTFIELD(ILEN), &sh->ilen, "%d", sh->ilen);

	while ((usp = (u_short *) ds_inline(ds, sizeof(type) + sizeof(len),
			sizeof(short))) != 0) {
		type = ntohs(*usp++);
		len  = ntohs(*usp);

		pv_break(pv);
		if ((tname = en_lookup(&ptypes, type)) != NULL)
			pv_showfield(pv, &SMTFIELD(PTYPE), &type,
				     "%#04x (%-s)", type, tname);
		else
			pv_showfield(pv, &SMTFIELD(PTYPE), &type,
				     "%#04x", type);
		pv_break(pv);
		pv_showfield(pv, &SMTFIELD(PLEN), &len, "%d", len);

		switch (type) {
		    case PTY_UNA: {
			    u_char una[UNALEN];

			    (void) ds_u_short(ds, &pad);
			    pv_showfield(pv, &SMTFIELD(PPAD), &pad,
				    "%#04x", pad);
			    (void) ds_bytes(ds, una, MIN(UNALEN, ds->ds_count));
			    pv_showfield(pv, &SMTFIELD(UNA), una,
				    "%-25.25s", fddi_ntoa(una));
			    break;
		    }

		    case PTY_SD: {
			    u_char nodeclass, mac_ct, nonmast_ct, mast_ct;

			    (void) ds_u_char(ds, &nodeclass);
			    pv_showfield(pv, &SMTFIELD(NCLASS), &nodeclass,
				    "%-s",
				    nodeclass ? "Concentrator" : "Station");
			    pv_break(pv);
			    (void) ds_u_char(ds, &mac_ct);
			    pv_showfield(pv, &SMTFIELD(MAC_CT), &mac_ct,
				    "%d", mac_ct);
			    (void) ds_u_char(ds, &nonmast_ct);
			    pv_showfield(pv, &SMTFIELD(NONMAST_CT), &nonmast_ct,
				    "%d", nonmast_ct);
			    (void) ds_u_char(ds, &mast_ct);
			    pv_showfield(pv, &SMTFIELD(MAST_CT), &mast_ct,
				    "%d", mast_ct);
			    break;
		    }

		    case PTY_SS: {
			    u_char topol, dupl;

			    (void) ds_u_short(ds, &pad);
			    pv_showfield(pv, &SMTFIELD(PPAD), &pad,
				    "%#04x", pad);
			    (void) ds_u_char(ds, &topol);
			    pv_showfield(pv, &SMTFIELD(TOPOL), &topol,
				    "%-s", en_bitset(topolbits,
				    lengthof(topolbits), topol));
			    (void) ds_u_char(ds, &dupl);
			    pv_showfield(pv, &SMTFIELD(DUPLADDR), &dupl,
				    "%-s", en_bitset(duplbits,
				    lengthof(duplbits), dupl));
			    break;
		    }

		    case PTY_MACNBR: {
			    u_char una[UNALEN];
			    u_short idx;

			    (void) ds_u_short(ds, &pad);
			    pv_showfield(pv, &SMTFIELD(PPAD), &pad,
				    "%#04x", pad);
			    (void) ds_u_short(ds, &idx);
			    pv_showfield(pv, &SMTFIELD(MACIDX), &idx,
				    "%#04x", idx);
			    (void) ds_bytes(ds, una, MIN(UNALEN, ds->ds_count));
			    pv_showfield(pv, &SMTFIELD(MUNA), una,
				    "%-25.25s", fddi_ntoa(una));
			    (void) ds_bytes(ds, una, MIN(UNALEN, ds->ds_count));
			    pv_showfield(pv, &SMTFIELD(MDNA), una,
				    "%-25.25s", fddi_ntoa(una));
			    break;
		    }

		    default: {
			    u_char *bp;

			    len = MIN(len, ds->ds_count);
			    bp = (u_char *) ds_inline(ds, len, 1);
			    if (bp != 0 && doshowhex(pv))
				    smt_showhex(pv, bp, len);
			    pv->pv_off += len;
			    break;
		    }
		}
	}
}
