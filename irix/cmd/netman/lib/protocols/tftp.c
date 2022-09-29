/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Trivial File Transfer Protocol (TFTP), defined in RFC 783.
 */
#include <netdb.h>
#include <sys/types.h>
#include <arpa/tftp.h>
#include "debug.h"
#include "enum.h"
#include "protodefs.h"
#include "protocols/ip.h"

char	tftpname[] = "tftp";

#define	th_errcode	th_code		/* use a more discernable name */
#define	TFTP_PORT	69		/* in case getservbyname fails */
#define	TFTP_HDRSIZE	4		/* header length in bytes */

enum tftpfid { OPCODE, STUFF, BLOCK, ERRCODE };

static ProtoField tftpfields[] = {
	PFI_SINT("opcode", "Operation Code", OPCODE,  short,	PV_TERSE),
	PFI_VAR("stuff",   "Filename/Mode",  STUFF,   2,	PV_TERSE),
	PFI("block",	   "Block Number",   BLOCK,   DS_SIGN_EXTEND,
	    sizeof(short), 2, EXOP_NUMBER, PV_DEFAULT),
	PFI("errcode",     "Error Code",     ERRCODE, DS_SIGN_EXTEND,
	    sizeof(short), 2, EXOP_NUMBER, PV_TERSE),
};

#define	TFTPFID(pf)	((enum tftpfid) (pf)->pf_id)
#define	TFTPFIELD(fid)	tftpfields[(int) fid]

static ProtoMacro tftpnicknames[] = {
	PMI("TFTP",	tftpname),
};

static Enumeration opcode;
static Enumerator opcodevec[] =
    { EI_VAL("RRQ", RRQ), EI_VAL("WRQ", WRQ), EI_VAL("DATA", DATA),
      EI_VAL("ACK", ACK), EI_VAL("ERROR", ERROR) };

static Enumeration errcode;
static Enumerator errcodevec[] =
    { EI_VAL("EUNDEF", EUNDEF), EI_VAL("ENOTFOUND", ENOTFOUND),
      EI_VAL("EACCESS", EACCESS), EI_VAL("ENOSPACE", ENOSPACE),
      EI_VAL("EBADOP", EBADOP), EI_VAL("EBADID", EBADID),
      EI_VAL("EEXISTS", EEXISTS), EI_VAL("ENOUSER", ENOUSER) };

/*
 * TFTP protocol interface and operations.
 */
#define	tftp_setopt	pr_stub_setopt
#define	tftp_embed	pr_stub_embed

DefineProtocol(tftp, tftpname, "Trivial File Transfer Protocol", PRID_TFTP,
	       DS_BIG_ENDIAN, TFTP_HDRSIZE, 0, PR_MATCHNOTIFY, 0, 0, 0);

static int
tftp_init()
{
	struct servent *sp;

	if (!pr_register(&tftp_proto, tftpfields, lengthof(tftpfields),
			 lengthof(opcodevec) + lengthof(errcodevec))) {
		return 0;
	}
	sp = getservbyname(tftpname, "udp");
	if (!pr_nest(&tftp_proto, PRID_UDP, (sp ? sp->s_port : TFTP_PORT),
		     tftpnicknames, lengthof(tftpnicknames))) {
		return 0;
	}
	en_init(&opcode, opcodevec, lengthof(opcodevec), &tftp_proto);
	en_init(&errcode, errcodevec, lengthof(errcodevec), &tftp_proto);
	return 1;
}

/* ARGSUSED */
static Expr *
tftp_resolve(char *name, int len, struct snooper *sn)
{
	return ex_string(name, len);
}

static ExprType
tftp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	long mask;

	if (!pc_intmask(pc, mex, &mask))
		return ET_ERROR;
	if (TFTPFID(pf) == STUFF) {
		pc_badop(pc, tex);
		return ET_ERROR;
	}
	if (!pc_intfield(pc, pf, mask, tex->ex_val, sizeof(struct tftphdr)))
		return ET_COMPLEX;
	return ET_SIMPLE;
}

static void
tftp_register(ProtoStack *ps)
{
	struct ipframe *ipf;

	ipf = PS_TOP(ps, struct ipframe);
	(void) pr_nestqual(&tftp_proto, PRID_UDP,
			   ipf->ipf_sport, ipf->ipf_src.s_addr,
			   tftpnicknames, lengthof(tftpnicknames));
}

/* ARGSUSED */
static int
tftp_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tftphdr *th;

	th = (struct tftphdr *) ds_inline(ds, TFTP_HDRSIZE, sizeof(u_short));
	if (th == 0)
		return 0;
	th->th_opcode = ntohs(th->th_opcode);
	if (th->th_opcode == RRQ || th->th_opcode == WRQ)
		tftp_register(ps);
	return 1;
}

static int
tftp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tftphdr *th;

	th = (struct tftphdr *) ds_inline(ds, TFTP_HDRSIZE, sizeof(u_short));
	if (th == 0)
		return 0;
	th->th_opcode = ntohs(th->th_opcode);
	if (th->th_opcode == RRQ || th->th_opcode == WRQ)
		tftp_register(ps);
	rex->ex_op = EXOP_NUMBER;
	switch (TFTPFID(pf)) {
	  case OPCODE:
		rex->ex_val = th->th_opcode;
		break;
	  case BLOCK:
		rex->ex_val = ntohs(th->th_block);
		break;
	  case ERRCODE:
		rex->ex_val = ntohs(th->th_errcode);
		break;
	  case STUFF:
		rex->ex_op = EXOP_STRING;
		rex->ex_str.s_ptr = th->th_stuff;
		rex->ex_str.s_len = strlen(th->th_stuff);
	}
	return 1;
}

static void
tftp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct tftphdr *th;

	th = (struct tftphdr *) ds_inline(ds, TFTP_HDRSIZE, sizeof(u_short));
	if (th == 0)
		return;
	th->th_opcode = ntohs(th->th_opcode);
	pv_showfield(pv, &TFTPFIELD(OPCODE), &th->th_opcode,
		     "%-5.5s", en_name(&opcode, th->th_opcode));
	switch (th->th_opcode) {
	  case RRQ:
	  case WRQ: {
		char *mode, *cp;
		int size;

		tftp_register(ps);
		mode = (th->th_stuff[1] == '\0') ? &th->th_stuff[2] : 0;
		size = TFTP_HDRSIZE - sizeof(th->th_opcode);
		while ((cp = ds_inline(ds, 1, 1)) != 0) {
			size++;
			if (*cp != '\0')
				continue;
			if (mode)
				break;
			mode = cp + 1;
		}
		pv_showvarfield(pv, &TFTPFIELD(STUFF), th->th_stuff, size,
				"%s (%s)", th->th_stuff, mode ? mode : "???");
		break;
	  }
	  case DATA:
		pv_showfield(pv, &TFTPFIELD(BLOCK), &th->th_block,
			     "%-5u", ntohs(th->th_block));
		if (pv->pv_level < PV_VERBOSE)
			ds_seek(ds, ds->ds_count, DS_RELATIVE);
		else
			pv_decodehex(pv, ds, 0, ds->ds_count);
		break;
	  case ACK:
		pv_showfield(pv, &TFTPFIELD(BLOCK), &th->th_block,
			     "%-5u", ntohs(th->th_block));
		break;
	  case ERROR:
		pv_showfield(pv, &TFTPFIELD(ERRCODE), &th->th_errcode,
			     "%-9.9s", en_name(&errcode,ntohs(th->th_errcode)));
		break;
	}
}
