/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * DECnet Network Services Protocol (NSP) snoop module.
 */
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/decnet.h"
#include "protocols/decnet_nsp.h"

char nspname[] = "nsp";

/*
 * NSP field identifiers and descriptors.
 */
#define NSP_PFI_UINT(name, title, id, type, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, sizeof(type), off, EXOP_NUMBER, \
	    level)

enum nspfid {
	MSGFLAG, DSTADDR, SRCADDR,
	ACKNUM, ACKOTH, ACKDAT, SEGNUM, LSFLAGS, FCVAL,
	SERVICES, INFO, SEGSIZE, CF_CTLSIZE,
	REASON, DI_CTLSIZE
};

static ProtoField nspfields[] = {
	/* common header information */
	PFI_UINT("msgflag",     "Message Flags", MSGFLAG, char,	      PV_TERSE),
	NSP_PFI_UINT("dstaddr", "Destination Link Address",
						 DSTADDR, u_short, 1, PV_TERSE),
	NSP_PFI_UINT("srcaddr", "Source Link Address",
						 SRCADDR, u_short, 3, PV_TERSE),

	/* data and ack messages */
	NSP_PFI_UINT("acknum", "Last Message Received",
					ACKNUM,   u_short,  5, PV_DEFAULT),
	NSP_PFI_UINT("ackoth", "Last Other Data Received",
					ACKOTH,   u_short,  7, PV_DEFAULT),
	NSP_PFI_UINT("ackdat", "Last Data Segment Received",
					ACKDAT,	  u_short,  7, PV_DEFAULT),
	NSP_PFI_UINT("segnum", "Message Number",
					SEGNUM,   u_short,  9, PV_DEFAULT),
	/* link service specific */
	NSP_PFI_UINT("lsflags", "Link Service Flags",
					LSFLAGS,  char,    11, PV_VERBOSE),
	NSP_PFI_UINT("fcval",   "Flow Control Delta Value",
					FCVAL,    char,    12, PV_VERBOSE),

	/* connect initiate and connect confirm control messages */
	NSP_PFI_UINT("services", "Requested Services",
					SERVICES, char,     5, PV_VERBOSE),
	NSP_PFI_UINT("info",     "Version Information", 
					INFO,     char,     6, PV_VERBOSE),
	NSP_PFI_UINT("segsize",  "Maximum Data Bytes",
					SEGSIZE,  u_short,  7, PV_DEFAULT),
	/* connect confirm specific */
	NSP_PFI_UINT("cf_ctlsize", "User-supplied Data Length",
					CF_CTLSIZE, u_char, 9, PV_VERBOSE),

	/* disconnect initiate and disconnect confirm control message */
	NSP_PFI_UINT("reason", "Reason", REASON,   u_short, 5, PV_VERBOSE), 
	/* disconnect initiate specific */
	NSP_PFI_UINT("di_ctlsize", "Disconnect Data Length",
					DI_CTLSIZE, u_char, 7, PV_VERBOSE),
};

#define	NSPFID(pf)	((enum nspfid) (pf)->pf_id)
#define	NSPFIELD(fid)	nspfields[(int) fid]

/*
 * Shorthands and aliases for the DECnet NSP
 */
static ProtoMacro nspnicknames[] = {
	PMI("NSP",	nspname),
};

static Enumeration flagcode;
static Enumerator flagcodevec[] = {
	EI_VAL("DATA_begin",	BEG_DATA_MSG),
	EI_VAL("DATA_end",	END_DATA_MSG),
	EI_VAL("DATA_middle",	MID_DATA_MSG),
	EI_VAL("DATA_complete",COM_DATA_MSG),
	EI_VAL("INTERRUPT",	INTR_MSG),
	EI_VAL("LINK_SERVICE",	LINK_SVC_MSG),

	EI_VAL("DATA_ACK",	DATA_ACK),
	EI_VAL("OTHER_DATA_ACK",OTH_ACK),
	EI_VAL("CONNECT_ACK",	CONN_ACK),

	EI_VAL("NOP",  NOP),
	EI_VAL("CONN_INITIATE",		CONN_INIT),
	EI_VAL("CONN_CONFIRM",		CONN_CONF),
	EI_VAL("DISC_INITIATE",		DISC_INIT),
	EI_VAL("DISC_CONFIRM",		DISC_CONF),
	EI_VAL("RESERVED_1",		RESERVED_1),
	EI_VAL("RECONN_INITIATE",	RECON_INIT),
	EI_VAL("RESERVED_2",		RESERVED_2),
};

static ProtoMacro nspmacros[] = {
	PMI("msg",      "msgflag == $1"),
};


/*
 * DECnet NSP protocol interface object.
 */
#define	nsp_compile	pr_stub_compile
#define	nsp_embed	pr_stub_embed
#define	nsp_match	pr_stub_match
#define	nsp_resolve	pr_stub_resolve
#define	nsp_setopt	pr_stub_setopt

DefineLeafProtocol(nsp, nspname, "DECnet Network Services Protocol", PRID_DNNSP,
		   DS_LITTLE_ENDIAN, NSP_MAXHDRLEN);


/* 
 * --------------------------------------------------------------------------
 * NSP protocol operations.
 * --------------------------------------------------------------------------
 */
static Index *nspprotos;


static int
nsp_init()			/* basic Procotol routine */
{
	/*
	 * Register NSP as a known protocol.
	 */
	if (!pr_register(&nsp_proto, nspfields, lengthof(nspfields),
			lengthof(flagcodevec) + lengthof(nspmacros) + 3)) {
		return 0;
	}

	/*
	 * Nest NSP in DECnet Routing Protocol
	 */
	if (!pr_nest(&nsp_proto, PRID_DNRP, RPPROTO_NSP, nspnicknames,
		     lengthof(nspnicknames))) {
		return 0;
	}

#ifdef NOT_YET
	/*
	 * Initialize or reinitialize the protocol hash tables.
	 */
	in_create(3, sizeof(u_char), 0, &nspprotos);
#endif

	/*
	 * Always define constants and macros, in case some were redefined.
	 */
	en_init(&flagcode, flagcodevec, lengthof(flagcodevec), &nsp_proto);
	pr_addmacros(&nsp_proto, nspmacros, lengthof(nspmacros));

	return 1;
} /* nsp_init */


/*
 * Generic get data_ack or other_data_ack message. Note that the message 
 * structure for both acknowledgement messages are the same, with the
 * exception of the optional piggy-backed ack field.
 */
int
nsp_get_ack(NSP_data_ack *ptr, DataStream *ds)
{
	if (!ds_bytes(ds, &ptr->flag, COMMON_HDR_LEN + ACK_LEN))
		return 0;

	/*
	 * Is the optional ack field here?
	 */
	if ( ((ds->ds_count < 2)			/* no more fields */
	    || !(*(ds->ds_next+1) & 0x80))		/* not ack field */
	    || !(ds_bytes(ds, ptr->ackoth, 2)) )	/* couldn't get it */
		{
		/* optional ack field is not here */
		ptr->ackoth[0] = 0;
		ptr->ackoth[1] = 0;
		}
	return 1;
} /* nsp_get_ack() */


/*
 * Generic get data_segment or interrupt message routine. Since the message
 * structure for both messages are the same with exception of the field names,
 * we can just pick either one of the structure with which to work.
 */
int
nsp_get_data(NSP_data_seg *ptr, DataStream *ds)
{
	char	field[2];

	if (!ds_bytes(ds, &ptr->flag, COMMON_HDR_LEN))
		return 0;
	if (!ds_bytes(ds, field, 2))
		return 0;
	if (0x8000 & MakeShort(field)) {
		/* first optional field is here */
		ptr->acknum[0] = field[0];
		ptr->acknum[1] = field[1];
		if (!ds_bytes(ds, field, 2))
			return 0;
		if (0x8000 & MakeShort(field)) {
			/* second optional field is also here */
			ptr->ackoth[0] = field[0];
			ptr->ackoth[1] = field[1];
			if (!ds_bytes(ds, ptr->segnum, 2))
				return 0;
			}
		else	{
			/* second optional field is not here */
			ptr->ackoth[0] = 0;
			ptr->ackoth[1] = 0;
			ptr->segnum[0] = field[0];
			ptr->segnum[1] = field[1];
			}
		}
	else	{
		/* no optional fields */
		ptr->acknum[0] = 0;
		ptr->acknum[1] = 0;
		ptr->ackoth[0] = 0;
		ptr->ackoth[1] = 0;
		ptr->segnum[0] = field[0];
		ptr->segnum[1] = field[1];
		}
	return 1;
} /* nsp_get_data() */


int
nsp_get_link_svc(NSP_link_svc *ptr, DataStream *ds)
{
	/*
	 * first get the common data message fields
	 */
	if (!nsp_get_data((NSP_data_seg *)ptr, ds))
		return 0;
	/*
	 * next get the link service specific fields
	 */
	if (!ds_bytes(ds, &ptr->lsflags, 2))
		return 0;
	return 1;
} /* nsp_get_link_svc() */


/*
 * Decoding routines:
 */
void
nsp_decode_acknum(unsigned char *ptr, enum nspfid fid, PacketView *pv)
{
	u_short field;
	static char nak[] = "NAK";
	static char ack[] = "ACK";
	static char reserved[] = "RESERVED?";
	char *s;

	field = MakeShort(ptr);

	if (field == 0)		/* ack field is not present */
		return;

	if (( field & 0x9000 ) == 0x9000 )
		s = nak;
	else if( ( field & 0x8000 ) == 0x8000 )
		s = ack;
	else
		s = reserved;

	pv_showfield(pv, &NSPFIELD(fid), ptr, "%s %-5d", s, (field & 0x07FF));
} /* nsp_decode_acknum() */

void
nsp_decode_common(void *ptr, PacketView *pv)
{
	char *flag = ptr;
	char *dstaddr = flag+1;
	char *srcaddr = flag+3;

	pv_showfield(pv, &NSPFIELD(MSGFLAG), flag,
		"%-20.20s", en_name(&flagcode, *flag));
	pv_showfield(pv, &NSPFIELD(DSTADDR), dstaddr,
		"%#-5d", MakeShort(dstaddr));
	pv_showfield(pv, &NSPFIELD(SRCADDR), srcaddr,
		"%#-5d", MakeShort(srcaddr));
} /* nsp_decode_common() */


void
nsp_decode_info(char info, PacketView *pv)
{
	static char *ver[] = {
		"version 3.2",		/* 0 */
		"version 3.1",		/* 1 */
		"version 4.0",		/* 2 */
		"reserved"		/* 3 */
	};
	pv_showfield(pv, &NSPFIELD(INFO), &info, "%#-02x(%s)", info, ver[info]);
} /* nsp_decode_info() */


void
nsp_decode_ls(char lsflags, char fcval, PacketView *pv)
{
	static char *fcval_int[] = {
		"data req cnt",		/* 0 */
		"intr req cnt",		/* 1 */
		"reserved",		/* 2 */
		"reserved"		/* 3 */
	};
	static char *fc_mod[] = {
		"no change",		/* 0 */
		"do not send data",	/* 1 */
		"send data",		/* 2 */
		"reserved"		/* 3 */
	};

	pv_showfield ( pv, &NSPFIELD(LSFLAGS), &lsflags,
		"%-#2x=(FCVAL-INT:%s, FC MOD: %s)", lsflags,
		fcval_int[LSINT(lsflags)],
		(LSINT(lsflags) == 1) ? "" : fc_mod[LSMOD(lsflags)] );
	pv_showfield(pv, &NSPFIELD(FCVAL), &fcval, "%-3d", fcval);
} /* nsp_decode_lsflags() */


void
nsp_decode_reason(u_char *reason, PacketView *pv)
{
	char *s;
	u_short why;
	
	why = MakeShort(reason);
	if (why == NoRes)
		s = "no resources";
	else if (why == DisCom)
		s = "disconnect complete";
	else if (why == NoLink)
		s = "no link terminate";
	else
		s = "?";
	pv_showfield(pv, &NSPFIELD(REASON), &reason[0],
		"%#3d(%s)", why, s);
} /* nsp_decode_reason() */


void
nsp_decode_services(char services, PacketView *pv)
{
	static char *fc[] = {
		"no flow control",		/* 0 */
		"segment request flow control",	/* 1 */
		"message request flow control",	/* 2 */
		"reserved"			/* 3 */
	};
	pv_showfield(pv, &NSPFIELD(SERVICES), &services,
		"%#-02x(%s)", services, fc[FCOPT(services)]);
} /* nsp_decode_services() */


static void
nsp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	char msgflag;			/* NSP message type */
	Protocol *pr = 0;		/* next layer protocol */

	if (ds->ds_count < 1)		/* lookahead 1 byte to flag */
		return;
	msgflag = *ds->ds_next;

	/*
	 * Decode this Network Services Protocol message header.
	 */
	switch (msgflag) {
	    case BEG_DATA_MSG:
	    case END_DATA_MSG:
	    case MID_DATA_MSG:
	    case COM_DATA_MSG:
		{
		NSP_data_seg msg;
		if (!nsp_get_data(&msg, ds))
			break;
		nsp_decode_common(&msg, pv);
		nsp_decode_acknum(msg.acknum, ACKNUM, pv);
		nsp_decode_acknum(msg.ackoth, ACKOTH, pv);
		pv_showfield(pv, &NSPFIELD(SEGNUM), msg.segnum,
			"%-4d", MakeShort(msg.segnum));
/* TBD: and what about the data ? */
		break; 
		}
	    case INTR_MSG:
		{
		NSP_intr_seg msg;
		if (!nsp_get_data((NSP_data_seg *)&msg, ds))
			break;
		nsp_decode_common(&msg, pv);
		nsp_decode_acknum(msg.acknum, ACKNUM, pv);
		nsp_decode_acknum(msg.ackdat, ACKDAT, pv);
		pv_showfield(pv, &NSPFIELD(SEGNUM), msg.segnum,
			"%-4d", MakeShort(msg.segnum));
		break;
		}
	    case LINK_SVC_MSG:
		{
		NSP_link_svc msg;
		if (!nsp_get_link_svc(&msg, ds))
			break;
		nsp_decode_common(&msg, pv);
		nsp_decode_acknum(msg.acknum, ACKNUM, pv);
		nsp_decode_acknum(msg.ackdat, ACKDAT, pv);
		pv_showfield(pv, &NSPFIELD(SEGNUM), msg.segnum,
			"%-4d", MakeShort(msg.segnum));
		nsp_decode_ls(msg.lsflags, msg.fcval, pv);
		break;
		}
	    case DATA_ACK:
		{
		NSP_data_ack msg;
		if (!nsp_get_ack(&msg, ds))
			break;
		nsp_decode_common(&msg, pv);
		nsp_decode_acknum(msg.acknum, ACKNUM, pv);
		nsp_decode_acknum(msg.ackoth, ACKOTH, pv);
		break;
		}
	    case OTH_ACK:
		{
		NSP_oth_ack msg;
		if (!nsp_get_ack((NSP_data_ack *)&msg, ds))
			break;
		nsp_decode_common(&msg, pv);
		nsp_decode_acknum(msg.acknum, ACKNUM, pv);
		nsp_decode_acknum(msg.ackdat, ACKDAT, pv);
		break;
		}
	    case CONN_ACK:
		{
		NSP_conn_ack *msg;
		msg = (NSP_conn_ack *) ds_inline (
			ds, sizeof(NSP_conn_ack), sizeof(char));
		if (msg == 0)
			break;
		pv_showfield(pv, &NSPFIELD(MSGFLAG), &msg->flag,
			"%-20.20s", en_name(&flagcode, msg->flag));
		pv_showfield(pv, &NSPFIELD(DSTADDR), &msg->dstaddr[0],
			"%-5d", MakeShort(msg->dstaddr));
		break;
		}
	    case NOP:
		{
		NSP_nop  *msg;
		msg = (NSP_nop *) ds_inline(ds, sizeof(NSP_nop), sizeof(char));
		if (msg == 0)
			break;
		pv_showfield(pv, &NSPFIELD(MSGFLAG), &msg->flag,
			"%-20.20s", en_name(&flagcode, msg->flag));
		break;
		}
	    case CONN_INIT:
	    case RECON_INIT:
		{
		NSP_conn_init *msg;
		msg = (NSP_conn_init *) ds_inline(
			ds, sizeof(NSP_conn_init), sizeof(char));
		if (msg == 0)
			break;
		nsp_decode_common(msg, pv);
		nsp_decode_services(msg->services, pv);
		nsp_decode_info(msg->info, pv);
		pv_showfield(pv, &NSPFIELD(SEGSIZE), &msg->segsize[0],
			"%#-04d", MakeShort(msg->segsize));
		break;
		}
	    case CONN_CONF:
		{
		NSP_conn_conf *msg;
		msg = (NSP_conn_conf *) ds_inline(
			ds, sizeof(NSP_conn_conf), sizeof(char));
		if (msg == 0)
			break;
		nsp_decode_common(msg, pv);
		nsp_decode_services(msg->services, pv);
		nsp_decode_info(msg->info, pv);
		pv_showfield(pv, &NSPFIELD(SEGSIZE), &msg->segsize[0],
			"%#-04d", MakeShort(msg->segsize));
		pv_showfield(pv, &NSPFIELD(CF_CTLSIZE), &msg->cf_ctlsize,
			"%#-02d", msg->cf_ctlsize);
		break;
		}
	    case DISC_INIT:
		{
		NSP_disc_init *msg;
		msg = (NSP_disc_init *) ds_inline(
			ds, sizeof(NSP_disc_init), sizeof(char));
		if (msg == 0)
			break;
		nsp_decode_common(msg, pv);
		pv_showfield(pv, &NSPFIELD(REASON), &msg->reason[0],
			"%-5d", MakeShort(msg->reason));
		pv_showfield(pv, &NSPFIELD(DI_CTLSIZE), &msg->di_ctlsize,
			"%#-02d", msg->di_ctlsize);
		break;
		}
	    case DISC_CONF:
		{
		NSP_disc_conf *msg;
		msg = (NSP_disc_conf *) ds_inline(
			ds, sizeof(NSP_disc_conf), sizeof(char));
		if (msg == 0)
			break;
		nsp_decode_common(msg, pv);
		nsp_decode_reason(msg->reason, pv);
		break;
		}
	    case RESERVED_1:
	    case RESERVED_2:
		{
		/*
		 * already got msg flag so just skip the single byte
		 */
		if (!ds_seek(ds, 1, DS_RELATIVE))
			return;
		pv_showfield(pv, &NSPFIELD(MSGFLAG), &msgflag,
			"%-20.20s", en_name(&flagcode, msgflag));
		break;
		}
	    default:
		pv_printf(pv, "unknown DECnet Network Services Protocol message");
	}

	pv_decodeframe(pv, pr, ds, ps);
} /* nsp_decode */


/*
 * Retrieve the value of the specified field. Since there are several
 * NSP headers, look ahead before doing ds_inline to determine the right
 * message to examine.
 */
static int
nsp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	char msgflag;                   /* NSP message type */

	if (ds->ds_count < 1)           /* lookahead 1 byte to flag */
		return 0;
	msgflag = *ds->ds_next;

	switch (msgflag) {
	    case BEG_DATA_MSG:
	    case END_DATA_MSG:
	    case MID_DATA_MSG:
	    case COM_DATA_MSG:
		{
		NSP_data_seg msg;
		if (!nsp_get_data(&msg, ds))
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg.flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg.dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg.srcaddr);
			break;
		    case ACKNUM:
			rex->ex_val = MakeShort(msg.acknum);
			break;
		    case ACKOTH:
			rex->ex_val = MakeShort(msg.ackoth);
			break;
		    case SEGNUM:
			rex->ex_val = MakeShort(msg.segnum);
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case INTR_MSG:
		{
		NSP_intr_seg msg;
		if (!nsp_get_data((NSP_data_seg *)&msg, ds))
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg.flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg.dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg.srcaddr);
			break;
		    case ACKNUM:
			rex->ex_val = MakeShort(msg.acknum);
			break;
		    case ACKDAT:
			rex->ex_val = MakeShort(msg.ackdat);
			break;
		    case SEGNUM:
			rex->ex_val = MakeShort(msg.segnum);
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case LINK_SVC_MSG:
		{
		NSP_link_svc msg;
		if (!nsp_get_link_svc(&msg, ds))
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg.flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg.dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg.srcaddr);
			break;
		    case ACKNUM:
			rex->ex_val = MakeShort(msg.acknum);
			break;
		    case ACKDAT:
			rex->ex_val = MakeShort(msg.ackdat);
			break;
		    case SEGNUM:
			rex->ex_val = MakeShort(msg.segnum);
			break;
		    case LSFLAGS:
			rex->ex_val = msg.lsflags;
			break;
		    case FCVAL:
			rex->ex_val = msg.fcval;
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case DATA_ACK:
		{
		NSP_data_ack msg;
		if (!nsp_get_ack(&msg, ds))
			break;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg.flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg.dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg.srcaddr);
			break;
		    case ACKNUM:
			rex->ex_val = MakeShort(msg.acknum);
			break;
		    case ACKOTH:
			rex->ex_val = MakeShort(msg.ackoth);
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case OTH_ACK:
		{
		NSP_oth_ack msg;
		if (!nsp_get_ack((NSP_data_ack *)&msg, ds))
			break;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg.flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg.dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg.srcaddr);
			break;
		    case ACKNUM:
			rex->ex_val = MakeShort(msg.acknum);
			break;
		    case ACKDAT:
			rex->ex_val = MakeShort(msg.ackdat);
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case CONN_ACK:
		{
		NSP_conn_ack *msg;
		msg = (NSP_conn_ack *) ds_inline (
			ds, sizeof(NSP_conn_ack), sizeof(char));
		if (msg == 0)
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg->flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg->dstaddr);
			break;
		    default:
			return 0;
		    }
		break;
		}
	    case NOP:
		{
		if (NSPFID(pf) != MSGFLAG)
			return 0;
		(void) ds_inline(ds, sizeof(NSP_nop), sizeof(char));
		rex->ex_val = msgflag;
		break;
		}
	    case CONN_INIT:
	    case RECON_INIT:
	    case CONN_CONF:
		{
		NSP_conn_init *msg;
		msg = (NSP_conn_init *) ds_inline(
			ds, sizeof(NSP_conn_init), sizeof(char));
		if (msg == 0)
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg->flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg->dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg->srcaddr);
			break;
		    case SERVICES:
			rex->ex_val = msg->services;
			break;
		    case INFO:
			rex->ex_val = msg->info;
			break;
		    case SEGSIZE:
			rex->ex_val = MakeShort(msg->segsize);
			break;
		    /* don't fetch image field length */
		    default:
			return 0;
		    }
		break;
		}
	    case DISC_INIT:
	    case DISC_CONF:
		{
		NSP_disc_conf *msg;
		msg = (NSP_disc_conf *) ds_inline(
			ds, sizeof(NSP_disc_conf), sizeof(char));
		if (msg == 0)
			return 0;
		switch (NSPFID(pf)) {
		    case MSGFLAG:
			rex->ex_val = msg->flag;
			break;
		    case DSTADDR:
			rex->ex_val = MakeShort(msg->dstaddr);
			break;
		    case SRCADDR:
			rex->ex_val = MakeShort(msg->srcaddr);
			break;
		    case REASON:
			rex->ex_val = MakeShort(msg->reason);
			break;
		    /* don't fetch image field length */
		    default:
			return 0;
		    }
		break;
		}
	    case RESERVED_1:
	    case RESERVED_2:
		return 0;
	    default:
		return 0;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
} /* nsp_fetch() */
