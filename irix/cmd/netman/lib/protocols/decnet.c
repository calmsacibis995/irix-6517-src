/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * DECnet Routing Protocol (RP) snoop module.
 */
#include <netdb.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include "enum.h"
#include "protodefs.h"
#include "protocols/ether.h"
#include "protocols/decnet.h"
#include "strings.h"

char rpname[] = "decnet";

/*
 * RP field identifiers and descriptors.
 */
enum rpfid {
	SIZE, PADDING, FLAGS,

	/* short data format - non Ethernet Phase IV node
	 */
	DSTNODE, SRCNODE, FORWARD,

	/* long data format - Ethernet Endnode
	 */
	D_AREA, D_SUBAREA, D_ID, S_AREA, S_SUBAREA, S_ID, NL2R,
	VISIT, S_CLASS, PT,

	/* common hello format - endnode and router
	 */
	VER, ECO, USER_ECO, ID, IINFO, BLKSIZE,

	    /* endnode hello specific
	     */
	    E_AREA, SEED, NEIGHBOR, E_TIMER, E_MPD, 

	    /* router hello specific
	    */
	    PRIORITY, R_AREA, R_TIMER, R_MPD
};

static char reserved[] = "Reserved";

#define RP_PFI_UINT(name, title, id, type, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, sizeof(type), off, EXOP_NUMBER, \
	    level)

#define RP_PFI_ADDR(name, title, id, size, off, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, size, off, EXOP_ADDRESS, level)

static ProtoField rpfields[] = {
	PFI_UINT("size", "Size",    SIZE,	u_short,	PV_VERBOSE),
	PFI_VAR("pad",   "Padding", PADDING,	MAXPADSIZE,	PV_VERBOSE),

	RP_PFI_UINT("flags", "Flags", FLAGS, u_char,	0,	PV_TERSE),

	/* ----- short data format ----- */
	RP_PFI_UINT("dstnode",	"Destination Node Address",
					    DSTNODE, u_short, 1, PV_TERSE),
	RP_PFI_UINT("srcnode",	 "Source Node Address",
					    SRCNODE, u_short, 3, PV_TERSE),
	RP_PFI_UINT("forward",	 "Forward", FORWARD, u_char,  5, PV_VERBOSE),

	/* ----- long data format ----- */
	RP_PFI_UINT("d_area",	 reserved, D_AREA,    u_char,  1, PV_VERBOSE),
	RP_PFI_UINT("d_subarea", reserved, D_SUBAREA, u_char,  2, PV_VERBOSE),
	RP_PFI_ADDR("d_id",      "Destination ID",    D_ID, 6, 3, PV_TERSE),
	RP_PFI_UINT("s_area",    reserved, S_AREA,    u_char,  9, PV_VERBOSE),
	RP_PFI_UINT("s_subarea", reserved, S_SUBAREA, u_char, 10, PV_VERBOSE),
	RP_PFI_ADDR("s_id",      "Source ID",         S_ID, 6,11, PV_TERSE),
	RP_PFI_UINT("nl2",       "Next Level 2 Router (Reserved)",
					   NL2R,      u_char, 17, PV_VERBOSE),
	RP_PFI_UINT("visit",     "Visit Count (0)",
					   VISIT,     u_char, 18, PV_VERBOSE),
	RP_PFI_UINT("s_class",   "Service Class (Reserved)",
					   S_CLASS,   u_char, 19, PV_VERBOSE),
	RP_PFI_UINT("pt",        "Protocol Type (Reserved)",
					   PT,        u_char, 20, PV_VERBOSE),

	/* ----- common hello format - endnode and router ----- */
	RP_PFI_UINT("ver",      "Version (2)",    VER,  u_char,  1, PV_VERBOSE),
	RP_PFI_UINT("eco",      "ECO Number (0)", ECO,  u_char,  2, PV_VERBOSE),
	RP_PFI_UINT("user_eco",	"USER ECO Number (0)",
					     USER_ECO,  u_char,  3, PV_VERBOSE),
	RP_PFI_ADDR("id",       "Source ID", ID,             6,  4, PV_TERSE),
	RP_PFI_UINT("iinfo",    "I-Info",    IINFO,     u_char, 10, PV_VERBOSE),
	RP_PFI_UINT("blksize", "Maximum Receive Block Size",
					     BLKSIZE,  u_short, 11, PV_VERBOSE),

		/* ----- endnode hello specific ----- */
	RP_PFI_UINT("e_area",  "Area (Reserved)",
					      E_AREA,   u_char, 13, PV_VERBOSE),
	RP_PFI_ADDR("seed", "Verification Seed (Reserved)",
		    			      SEED,          8, 14, PV_VERBOSE),
	RP_PFI_ADDR("neighbor", "Neighbor/Designated Router ID",
		    			      NEIGHBOR,      6, 22, PV_VERBOSE),
	RP_PFI_UINT("e_timer", "Hello Timer", E_TIMER, u_short,	28, PV_VERBOSE),
	RP_PFI_UINT("e_mpd",   reserved,      E_MPD,   u_char,  30, PV_VERBOSE),

		/* ----- router hello specific ----- */
	RP_PFI_UINT("priority",	"Router's Priority",
					      PRIORITY, u_char, 13, PV_VERBOSE),
	RP_PFI_UINT("r_area",  reserved,      R_AREA,   u_char, 14, PV_VERBOSE),
	RP_PFI_UINT("r_timer", "Hello Timer", R_TIMER, u_short, 15, PV_VERBOSE),
	RP_PFI_UINT("r_mpd",   reserved,      R_MPD,    u_char, 17, PV_VERBOSE),
};

#define	RPFID(pf)	((enum rpfid) (pf)->pf_id)
#define	RPFIELD(fid)	rpfields[(int) fid]

/*
 * Shorthands and aliases for the DECnet (RP-based) protocols.
 */
static ProtoMacro rpnicknames[] = {
	PMI("DECNET",	rpname),
	PMI("nsp",	"decnet.nsp"),
	PMI("NSP",	"decnet.nsp"),
	PMI("scp",	"decnet.nsp.scp"),
	PMI("SCP",	"decnet.nsp.scp"),
};

static Enumeration msgcode;
static Enumerator msgcodevec[] = {
	EI_VAL("LEVEL_1_ROUTER", PKT_LEVEL_1_ROUTER),
	EI_VAL("LEVEL_2_ROUTER", PKT_LEVEL_2_ROUTER),
	EI_VAL("ROUTER_HELLO",   PKT_ROUTER_HELLO),
	EI_VAL("ENDNODE_HELLO",  PKT_ENDNODE_HELLO),
	EI_VAL("LONG_FORMAT",    PKT_LONG_FORMAT),
	EI_VAL("SHORT_FORMAT",   PKT_SHORT_FORMAT),
};

static ProtoMacro rpmacros[] = {
	PMI("between",	"s_id == $1 && d_id == $2 || d_id == $1 && s_id == $2"),
	PMI("host",	"s_id == $1 || d_id == $1"),
	PMI("msg",	"(flags & $1) == $1"),
};


/*
 * Macro to call ds_inline() with the correct RP header structure.
 */
#define	RP_ds_inline(type, rp, ds) \
	switch (type) { \
	    case PKT_LEVEL_1_ROUTER: \
	    case PKT_LEVEL_2_ROUTER: \
		rp = 0; \
		break; \
	    case PKT_ROUTER_HELLO: \
		rp = ds_inline(ds, sizeof(RP_router_hello), sizeof(char)); \
		break; \
	    case PKT_ENDNODE_HELLO: \
		rp = ds_inline(ds, sizeof(RP_endnode_hello), sizeof(char)); \
		break; \
	    case PKT_LONG_FORMAT: \
		rp = ds_inline(ds, sizeof(RP_long_msg), sizeof(char)); \
		break; \
	    case PKT_SHORT_FORMAT: \
		rp = ds_inline(ds, sizeof(RP_short_msg), sizeof(char)); \
		break; \
	}

/*
 * Macro to get the RP header type.
 */
#define	RP_MSG_TYPE(type, flags) \
	if (flags & ROUT_PKT_FLAG) \
		type = (flags & (ROUT_PKT_TYPE | ROUT_PKT_FLAG)); \
	else \
		type = (flags & DATA_PKT_TYPE)

/*
 * If flag indicates there is padding, skip the padding. After skipping
 * the pads, there must be at least one byte left (the message flag).
 */
#define	SKIP_PADDING(flag, ds) \
	if ( ( flag & ROUT_PAD_TYPE ) \
	    && ( !ds_seek(ds, (flag & ROUT_PADDING), DS_RELATIVE) \
		|| (ds->ds_count < 1) ) ) { \
		return 0; \
	}


/*
 * DECnet RP protocol interface.
 *
 * Cannot compile filters for the fields because the ethernet specific two
 * bytes preceeding the frame and the variable length pad bytes throw the
 * compile time filtering scheme out the door.
 * 
 * The special size field and possibly the optional (and variable length)
 * pads can be compiled into a filter, but it is not interesting to do so.
 */
#define rp_setopt	pr_stub_setopt
#define	rp_compile	pr_stub_compile

DefineBranchProtocol(rp, rpname, "DECnet Routing Protocol", PRID_DNRP,
		     DS_LITTLE_ENDIAN, RP_MAXHDRLEN, &RPFIELD(PT));

/*
 * RP protocol operations.
 */
static Index *rpprotos;

static int
rp_init()			/* basic Procotol routine */
{
	int nested;
	extern unsigned char ether_hiorder[];
	extern Ether_addr allroutersaddr;
	extern Ether_addr allendnodesaddr;

	/*
	 * Register RP as a known protocol.
	 */
	if (!pr_register(&rp_proto, rpfields, lengthof(rpfields),
			 lengthof(msgcodevec) + lengthof(rpmacros) + 3)) {
		return 0;
	}

	/*
	 * Nest RP in loopback, Ethernet, LLC, etc.
	 */
	nested = 0;
	if (pr_nest(&rp_proto, PRID_LOOP, AF_DECnet, rpnicknames,
		    lengthof(rpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&rp_proto, PRID_ETHER, ETHERTYPE_DECnet, rpnicknames,
		    lengthof(rpnicknames))) {
		nested = 1;
	}
	if (pr_nest(&rp_proto, PRID_LLC, ETHERTYPE_DECnet, rpnicknames,
		    lengthof(rpnicknames))) {
		nested = 1;
	}
	if (!nested)
		return 0;

	/*
	 * Initialize or reinitialize the protocol hash tables.
	 */
	in_create(3, sizeof(u_char), 0, &rpprotos);

	/*
	 * Always define constants and macros, in case some were redefined.
	 */
	en_init(&msgcode, msgcodevec, lengthof(msgcodevec), &rp_proto);
	pr_addmacros(&rp_proto, rpmacros, lengthof(rpmacros));

	pr_addconstaddress(&rp_proto, "ETHER_HIORDER",
		(char *) &ether_hiorder[0], 4);
	pr_addconstaddress(&rp_proto, "ALL_ROUTERS",
		(char *) &allroutersaddr, sizeof(allroutersaddr));
	pr_addconstaddress(&rp_proto, "ALL_ENDNODES",
		(char *) &allendnodesaddr, sizeof(allendnodesaddr));
	return 1;
} /* rp_init() */


/* ARGSUSED */
static void
rp_embed(Protocol *pr, long prototype, long qualifier)
{
	u_short type = prototype;

	in_enter(rpprotos, &type, pr);
} /* rp_embed */


/*
 * Resolve non-protocol specific stuff. i.e ":" and "." in addresses.
 * Return 0 on error.
 *
 * TBD: What should we do about the multicast addresses? It is currently
 * being ignored.
 */
/* ARGSUSED */
static Expr *
rp_resolve(char *name, int len, struct snooper *sn)
{
	dn_addr addr;
	u_long val;
	Expr *ex;

	if (rp_hostaddr(name, RP_HOST, &addr))
		val = addr;
	else {
		int cmd;

		if (len == 9 && !strcmp(name, "multicast"))
			cmd = 0;
		else if (len == 10 && !strcmp(name, "allrouters"))
			cmd = 0;
		else if (len == 11 && !strcmp(name, "allendnodes"))
			cmd = 0;
		else {
			return 0;
		}
		val = cmd;
	}

	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = val;
	return ex;
} /* rp_resolve() */


static int
rp_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct rpframe rpf;
	char *rp;			/* cast to the correct header */
	int pkt, matched;

	if ( !ds_seek(ds, sizeof(u_short), DS_RELATIVE) )	/* size */
		return 0;

	if (ds->ds_count < 1)		/* look ahead 1 byte to pad or flag? */
		return 0;

	SKIP_PADDING(*ds->ds_next, ds);
	RP_MSG_TYPE(pkt, *ds->ds_next);		/* get message type */
	RP_ds_inline(pkt, rp, ds);		/* set up ds for match */

	if (rp == 0)
		return 0;

	if ( (pkt == PKT_LONG_FORMAT) 
	    && (((RP_long_msg *)(rp))->pt != pex->ex_prsym->sym_prototype) )
		return 0;

	/*
	 * Push a RP protocol stack frame.
	 */
	PS_PUSH(ps, &rpf.rpf_frame, &rp_proto);
	rpf.rpf_type = pkt;
	switch (pkt) {
	    case PKT_LEVEL_1_ROUTER:
	    case PKT_LEVEL_2_ROUTER:
		break;
	    case PKT_ROUTER_HELLO:
		rpf.rpf_src = ((RP_router_hello *)(rp))->trans_id;
		break; 
	    case PKT_ENDNODE_HELLO:
		rpf.rpf_src = ((RP_endnode_hello *)(rp))->trans_id;
		break; 
	    case PKT_LONG_FORMAT:
		rpf.rpf_src = ((RP_long_msg *)(rp))->s_id;
		rpf.rpf_dst = ((RP_long_msg *)(rp))->d_id;
		break; 
	    case PKT_SHORT_FORMAT:
		rpf.rpf_srcnode = MakeShort(((RP_short_msg *)(rp))->srcnode);
		rpf.rpf_dstnode = MakeShort(((RP_short_msg *)(rp))->dstnode);
		break;
	}

	/* Evaluate the rest of the path expression.
	 */
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
} /* rp_match() */

/*
 * What order should the fetched data be in? network or host?
 * Since this is most likely a fetch to match, let's try for host order
 * and see what happens.
 *
 * NOTE: On fetches of DECnet/Ethernet ids, the two bytes DECnet address is
 * extracted from the six bytes Ethernet address and converted to host order. 
 */
static int
rp_fetch_long(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	RP_long_msg *rp;

	rp = (RP_long_msg *) ds_inline(ds, sizeof *rp, sizeof(char));
	if (rp == 0)
		return 0;
	switch (RPFID(pf)) {
	  case FLAGS:
		rex->ex_val = rp->flags;
		break;
	  case D_AREA:
		rex->ex_val = rp->d_area;
		break;
	  case D_SUBAREA:
		rex->ex_val = rp->d_subarea;
		break;
	  case D_ID:
		rex->ex_val = MakeShort(&rp->d_id.ea_vec[4]);
		break;
	  case S_AREA:
		rex->ex_val = rp->s_area;
		break;
	  case S_SUBAREA:
		rex->ex_val = rp->s_subarea;
		break;
	  case S_ID:
		rex->ex_val = MakeShort(&rp->s_id.ea_vec[4]);
		break;
	  case NL2R:
		rex->ex_val = rp->nl2;
		break;
	  case VISIT:
		rex->ex_val = rp->visit_ct;
		break;
	  case S_CLASS:
		rex->ex_val = rp->s_class;
		break;
	  case PT:
		rex->ex_val = rp->pt;
		break;
	  default:
		return 0;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
} /* rp_fetch_long() */

static int
rp_fetch_short(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	RP_short_msg *rp;

	rp = (RP_short_msg *) ds_inline(ds, sizeof *rp, sizeof(char));
	if (rp == 0)
		return 0;
	switch (RPFID(pf)) {
	  case FLAGS:
		rex->ex_val = rp->flags;
		break;
	  case DSTNODE:
		rex->ex_val = MakeShort(rp->dstnode);
		break;
	  case SRCNODE:
		rex->ex_val = MakeShort(rp->srcnode);
		break;
	  case FORWARD:
		rex->ex_val = rp->forward;
		break;
	  default:
		return 0;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
} /* rp_fetch_short() */

static int
rp_fetch_rhello(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	RP_router_hello *rp;

	rp = (RP_router_hello *) ds_inline(ds, sizeof *rp, sizeof(char));
	if (rp == 0)
		return 0;
	switch (RPFID(pf)) {
	  case FLAGS:
		rex->ex_val = rp->flags;
		break;
	  case VER:
		rex->ex_val = rp->ver_number;
		break;
	  case ECO:
		rex->ex_val = rp->eco_number;
		break;
	  case USER_ECO:
		rex->ex_val = rp->user_eco_number;
		break;
	  case ID:
		rex->ex_val = MakeShort(&rp->trans_id.ea_vec[4]);
		break;
	  case IINFO:
		rex->ex_val = rp->enet_iinfo;
		break;
	  case BLKSIZE:
		rex->ex_val = MakeShort(rp->blksize);
		break;
	  case PRIORITY:
		rex->ex_val = rp->priority;
		break;
	  case R_AREA:
		rex->ex_val = rp->area;
		break;
	  case R_TIMER:
		rex->ex_val = MakeShort(rp->timer);
		break;
	  case R_MPD:
		rex->ex_val = rp->mpd;
		break;
	  default:
		return 0;
	}
	rex->ex_op = EXOP_NUMBER;
	return 1;
} /* rp_fetch_rhello() */

static int
rp_fetch_ehello(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	RP_endnode_hello *rp;

	rp = (RP_endnode_hello *) ds_inline(ds, sizeof *rp, sizeof(char));
	if (rp == 0)
		return 0;
	switch (RPFID(pf)) {
	  case FLAGS:
		rex->ex_val = rp->flags;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case VER:
		rex->ex_val = rp->ver_number;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case ECO:
		rex->ex_val = rp->eco_number;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case USER_ECO:
		rex->ex_val = rp->user_eco_number;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case ID:
		rex->ex_val = MakeShort(&rp->trans_id.ea_vec[4]);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case IINFO:
		rex->ex_val = rp->enet_iinfo;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case BLKSIZE:
		rex->ex_val = MakeShort(rp->blksize);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case E_AREA:
		rex->ex_val = rp->area;
		rex->ex_op = EXOP_NUMBER;
		break;
	  case SEED:
		*A_CAST(&rex->ex_addr, Seed) = rp->ver_seed;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case NEIGHBOR:
		*A_CAST(&rex->ex_addr, Ether_addr) = rp->neigh_id;
		rex->ex_op = EXOP_ADDRESS;
		break;
	  case E_TIMER:
		rex->ex_val = MakeShort(rp->timer);
		rex->ex_op = EXOP_NUMBER;
		break;
	  case E_MPD:
		rex->ex_val = rp->mpd;
		rex->ex_op = EXOP_NUMBER;
		break;
	  default:
		return 0;
	}
	return 1;
} /* rp_fetch_ehello() */

/*
 * Retrieve the value of the specified field. Since there are several
 * RP headers, look ahead before doing ds_inline to determine the right
 * message to examine.
 */
static int
rp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	int pkt;
	u_short size;
	int plength = 0;
	u_char flags;			/* pad and message flag */

	if (!ds_u_short(ds, &size))	/* special ethernet/decnet stuff */
		return 0;

	if (ds->ds_count < 1)		/* lookahead 1 byte to pad/flag */
		return 0;

	flags = *ds->ds_next;

	if ( flags & ROUT_PAD_TYPE ) {
		/*
		 * Discard pad bytes
		 */
		plength = (flags & ROUT_PADDING);
		if ( !ds_seek(ds, plength, DS_RELATIVE)
		    || (ds->ds_count < 1) )	/* flag must exist*/
			return 0;
		flags = *ds->ds_next;
	}

	/*
	 * Fetching header prefix field
	 */
	switch (RPFID(pf)) {
	  case SIZE:
		rex->ex_val = size;
		rex->ex_op = EXOP_NUMBER;
		return 1;
	  case PADDING:
		rex->ex_val = plength;
		rex->ex_op = EXOP_NUMBER;
		return 1;
	}

	RP_MSG_TYPE (pkt, flags);
	switch (pkt) {
	    case PKT_LEVEL_1_ROUTER:
	    case PKT_LEVEL_2_ROUTER:
		return 0;	       /* don't handle for now */
	    case PKT_ROUTER_HELLO:
		return (rp_fetch_rhello(pf, ds, ps, rex));
	    case PKT_ENDNODE_HELLO:
		return (rp_fetch_ehello(pf, ds, ps, rex));
	    case PKT_LONG_FORMAT:
		return (rp_fetch_long(pf, ds, ps, rex));
	    case PKT_SHORT_FORMAT:
		return (rp_fetch_short(pf, ds, ps, rex));
	}

	return 0;
} /* rp_fetch() */


void
rp_decode_prefix(PacketView *pv, u_short size, void *paddr, int plen)
{
	pv_showfield(pv, &RPFIELD(SIZE), &size, "%-4d", size);
	pv_showvarfield(pv, &RPFIELD(PADDING), paddr, plen,
			"%-2d", plen);
} /* rp_decode_prefix() */


static char flagformat[] = "%#-02x=(RES:%d,TYPE:%s)";

/*
 * Translate the Ethernet address to a DECnet address and its
 * corresponding node name.
 */
char *
name_addr_fromether(Ether_addr *eaddr)
{
	static char	name_addr_buf[20];
	char		*name;
	dn_addr		addr;

	name = rp_hostnamefromether(eaddr);
	if (rp_addrfromether(eaddr, RP_NET, &addr)) {
		(void) nsprintf(name_addr_buf, sizeof name_addr_buf, "%s (%s)",
				decnet_ntoa(addr), name);
	} else {
		(void) nsprintf(name_addr_buf, sizeof name_addr_buf, "%s",
				name);
	}
	return name_addr_buf;
} /* name_addr_fromether() */

static void
rp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	char *rp;			/* cast to the correct header */
	struct rpframe rpf;
	int pkt;			/* RP packet type */
	u_char flags;
	u_short size;
	void *paddr = 0;
	int plen = 0;
	Protocol *pr = 0;		/* next layer protocol */

	(void) ds_u_short(ds, &size);

	if (ds->ds_count < 1)		/* lookahead 1 byte to pad or flag? */
		return;

	/*
	 * Skip any leading pads
	 */
	flags = *ds->ds_next;
	if ( flags & ROUT_PAD_TYPE ) {
		paddr = ds->ds_next;
		plen = (flags & ROUT_PADDING);
		if (!ds_seek(ds, plen, DS_RELATIVE) || (ds->ds_count < 1))
			return;
		flags = *ds->ds_next;
	}

	/*
	 * Display fields preceeding the Routing Layer Protocol packet.
	 */
	rp_decode_prefix(pv, size, paddr, plen);

	RP_MSG_TYPE(pkt, flags);		/* get message type */
	RP_ds_inline(pkt, rp, ds);		/* set up ds for match */

	if (rp == 0)
		return;

	/*
	 * Decode this Routing Protocol message header.
	 */
	rpf.rpf_type = pkt;
	switch (pkt) {
	    case PKT_LEVEL_1_ROUTER:
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
			     flagformat, flags, (flags & FLAG_RES)>>4,
			     en_name(&msgcode, pkt));
		/* TBD */
		return;
	    case PKT_LEVEL_2_ROUTER:
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
	    		     flagformat, flags, (flags & FLAG_RES)>>4,
			     en_name(&msgcode, pkt));
		/* TBD */
		return;
	    case PKT_ROUTER_HELLO:
		{
		RP_router_hello *msg = (RP_router_hello *)rp;
		rpf.rpf_src = msg->trans_id;
		rp_addrfromether(&msg->trans_id, RP_HOST, &rpf.rpf_srcnode);
		/*
		 */
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
	    		     flagformat, flags, (flags & FLAG_RES)>>4,
			     en_name(&msgcode, pkt));
		pv_showfield(pv, &RPFIELD(VER), &msg->ver_number,
			     "%-3d", msg->ver_number);
		pv_showfield(pv, &RPFIELD(ECO), &msg->eco_number,
			     "%-3d", msg->eco_number);
		pv_showfield(pv, &RPFIELD(USER_ECO), &msg->user_eco_number,
			     "%-3d", msg->user_eco_number);
		pv_showfield(pv, &RPFIELD(ID), &msg->trans_id,
			     "%-20.20s", name_addr_fromether(&msg->trans_id));
		pv_showfield(pv, &RPFIELD(IINFO), &msg->enet_iinfo,
			     "%#-04x", msg->enet_iinfo);
		pv_showfield(pv, &RPFIELD(BLKSIZE), &msg->blksize[0],
			     "%-5d", MakeShort(msg->blksize));
		/* ----- router specific ----- */
		pv_showfield(pv, &RPFIELD(PRIORITY), &msg->priority,
			     "%-3d", msg->priority);
		pv_showfield(pv, &RPFIELD(R_AREA), &msg->area,
			     "%-3d", msg->area);
		pv_showfield(pv, &RPFIELD(R_TIMER), &msg->timer[0],
			     "%-3d", MakeShort(msg->timer));
		pv_showfield(pv, &RPFIELD(R_MPD), &msg->mpd,
			     "%-3d", msg->mpd);
		break; 
		} /* router hello */
	    case PKT_ENDNODE_HELLO:
		{
		RP_endnode_hello *msg = (RP_endnode_hello *)(rp);
		rpf.rpf_src = msg->trans_id;
		rp_addrfromether(&msg->trans_id, RP_HOST, &rpf.rpf_srcnode);
		/*
		 */
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
			     flagformat, flags, (flags & FLAG_RES)>>4,
			     en_name(&msgcode, pkt));
		pv_showfield(pv, &RPFIELD(VER), &msg->ver_number,
			     "%-3d", msg->ver_number);
		pv_showfield(pv, &RPFIELD(ECO), &msg->eco_number,
			     "%-3d", msg->eco_number);
		pv_showfield(pv, &RPFIELD(USER_ECO), &msg->user_eco_number,
			     "%-3d", msg->user_eco_number);
		pv_showfield(pv, &RPFIELD(ID), &msg->trans_id,
			     "%-20.20s", name_addr_fromether(&msg->trans_id));
		pv_showfield(pv, &RPFIELD(IINFO), &msg->enet_iinfo,
			     "%#-04x", msg->enet_iinfo);
		pv_showfield(pv, &RPFIELD(BLKSIZE), &msg->blksize[0],
			     "%-5d", MakeShort(msg->blksize));
		/* ----- endnode specific ----- */
		pv_showfield(pv, &RPFIELD(E_AREA), &msg->area,
			     "%-3d", msg->area);
		pv_showfield(pv, &RPFIELD(SEED), &msg->ver_seed,
			     "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ",
			     msg->ver_seed.b[0], msg->ver_seed.b[1],
			     msg->ver_seed.b[2], msg->ver_seed.b[3],
			     msg->ver_seed.b[4], msg->ver_seed.b[5],
			     msg->ver_seed.b[6], msg->ver_seed.b[7]);
		pv_showfield(pv, &RPFIELD(NEIGHBOR), &msg->neigh_id,
			     "%-25.25s", ether_ntoa(&msg->neigh_id));
		pv_showfield(pv, &RPFIELD(E_TIMER), &msg->timer[0],
			     "%-5d", MakeShort(msg->timer));
		pv_showfield(pv, &RPFIELD(E_MPD), &msg->mpd,
			     "%-3d", msg->mpd);
		break; 
		} /* endnode hello */
	    case PKT_LONG_FORMAT:
		{
		RP_long_msg *msg = (RP_long_msg *)(rp);
		rpf.rpf_src = msg->s_id;
		rpf.rpf_dst = msg->d_id;
		rp_addrfromether(&msg->s_id, RP_HOST, &rpf.rpf_srcnode);
		rp_addrfromether(&msg->d_id, RP_HOST, &rpf.rpf_dstnode);
		/*
		 */
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
			     "%#-02x=(I-E:%d,RTS:%d,RQR:%d,LFDP:%s)",
			     flags, (flags & FLAG_IE)>>5, (flags & FLAG_RTS)>>4,
			     (flags & FLAG_RQR)>>3, en_name(&msgcode, pkt));
		pv_showfield(pv, &RPFIELD(D_AREA), &msg->d_area,
			     "%-3d", msg->d_area);
		pv_showfield(pv, &RPFIELD(D_SUBAREA), &msg->d_subarea,
			     "%-3d", msg->d_subarea);
		pv_showfield(pv, &RPFIELD(D_ID), &msg->d_id,
			     "%-20.20s", name_addr_fromether(&msg->d_id));
		pv_showfield(pv, &RPFIELD(S_AREA), &msg->s_area,
			     "%-3d", msg->s_area);
		pv_showfield(pv, &RPFIELD(S_SUBAREA), &msg->s_subarea,
			     "%-3d", msg->s_subarea);
		pv_showfield(pv, &RPFIELD(S_ID), &msg->s_id,
			     "%-20.20s", name_addr_fromether(&msg->s_id));
		pv_showfield(pv, &RPFIELD(NL2R), &msg->nl2,
			     "%-3d", msg->nl2);
		pv_showfield(pv, &RPFIELD(VISIT), &msg->visit_ct,
			     "%-3d", msg->visit_ct);
		pv_showfield(pv, &RPFIELD(S_CLASS), &msg->s_class,
			     "%-3d", msg->s_class);
		/*
		 * Set up for the transport layer protocol decoding.
		 * If the next layer cannot be decoded, promote the
		 * packet type level to terse and print a description
		 * of the protocol (i.e. protocol number).
		 */
		pr = in_match(rpprotos, &msg->pt);
		if (pr) {
			pv_showfield(pv, &RPFIELD(PT), &msg->pt,
				     "%-6s", pr->pr_name);
		} else {
			pv_showfield(pv, &RPFIELD(PT), &msg->pt,
				     "@t%#-6x", msg->pt);
		}
		break; 
		} /* long format data */
	    case PKT_SHORT_FORMAT:
		{
		RP_short_msg *msg = (RP_short_msg *)(rp);
		rpf.rpf_srcnode = MakeShort(msg->srcnode);
		rpf.rpf_dstnode = MakeShort(msg->dstnode);
		/*
		 */
		pv_showfield(pv, &RPFIELD(FLAGS), &flags,
			     "%#-02x=(RTS:%d,RQR:%d,SFDP:%s)",
			     flags, (flags & FLAG_RTS)>>4,
			     (flags & FLAG_RQR)>>3, en_name(&msgcode, pkt));
		pv_showfield(pv, &RPFIELD(DSTNODE), &msg->dstnode[0],
			     "%-20.20s", rp_hostname(rpf.rpf_dstnode, RP_HOST));
		pv_showfield(pv, &RPFIELD(SRCNODE), &msg->srcnode[0],
			     "%-20.20s", rp_hostname(rpf.rpf_srcnode, RP_HOST));
		pv_showfield(pv, &RPFIELD(FORWARD), &msg->forward,
			     "%-3d", msg->forward);
		break;
		}; /* short format data */
	    default:
		pv_printf(pv, "unknown DECnet Routing Protocol message");
	}

	/*
	 * Push a RP protocol stack frame.
	 */
	PS_PUSH(ps, &rpf.rpf_frame, &rp_proto);

	/*
	 * Continue decoding by a higher layer.
	 */
	pv_decodeframe(pv, pr, ds, ps);
	PS_POP(ps);
} /* rp_decode() */
