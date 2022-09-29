/*
 * Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 * HIPPI snoop module.
 */
#include <bstring.h>
#include <sys/types.h>
#include "index.h"
#include "enum.h"
#include "protodefs.h"
#include "strings.h"
#include "protocols/ether.h"


/* This should come from a header file somewhere, but where?
 */

#define HIPPI_ULA_SIZE 6
typedef unsigned char hippi_ula_t[HIPPI_ULA_SIZE];

typedef __uint32_t hippi_i_t;

typedef struct hippi_fp {
	u_char  hfp_ulp_id;
	u_char  hfp_flags;
#	define HFP_FLAGS_P     0x80
#	define HFP_FLAGS_B     0x40
	u_short hfp_d1d2off;
#	define HFP_D1SZ_MASK   0x07F8
#	define HFP_D1SZ_SHFT   3
#	define HFP_D2OFF_MASK  0x0007
	__uint32_t      hfp_d2size;
} hippi_fp_t;

typedef struct hippi_le {
	u_char  hle_fcwm;		/* Forwarding class, Width, M_Type */
#	define HLE_FC_MASK     0xE0
#	define HLE_FC_SHIFT    5
#	define HLE_W		0x10
#	define HLE_MT_MASK     0x0F
#	define HLE_MT_SHIFT    0
#	   define MAT_DATA	    0   /* data carrying PDU */
#	   define MAT_ARP_REQUEST   1   /* HIPPI ARP request */
#	   define MAT_ARP_REPLY	    2	/* HIPPI ARP response */
#	   define MAT_S_REQUEST	    3	/* switch address discovery request */
#	   define MAT_S_RESPONSE    4   /* switch address discovery response */
	u_char  hle_dest_swaddr[3];     /* dest switch address */
	u_char  hle_swat;		/* switch address types */
#	define HLE_DAT_MASK    0xF0
#	define HLE_DAT_SHIFT   4
#	define HLE_SAT_MASK    0x0F
#	define HLE_SAT_SHIFT   0
	u_char  src_switch_addr[3];     /* source switch address */
	u_short resvd;
	hippi_ula_t     dest;		/* Destination IEEE address */
	u_short local_admin;
	hippi_ula_t     src;		/* Source IEEE address */
} hippi_le_t;

struct hippi_head {
	hippi_i_t   ifield;
	hippi_fp_t  fp;
	hippi_le_t  le;
};

struct hippiframe {
	ProtoStackFrame	    hf_frame;	/* base class state */
	hippi_ula_t	    hf_dest;
	hippi_ula_t	    hf_src;
};


char hippiname[] = "hippi";

/* HIPPI field identifiers and descriptors.
 */
enum hippifid {
	IFIELD,

	HFP_ULP_ID,
	HFP_FLAGS,
	HFP_D1D2OFF,
	HFP_D2SIZE,

	HLE_FCWM,
	HLE_DEST_SW,
	HLE_SWAT,
	HLE_SRC_SW,
	HLE_RESVD,
	HLE_DEST,
	HLE_LOCAL_ADMIN,
	HLE_SRC,
};

static ProtoField hippifields[] = {
    PFI_UINT("ifield",	"ifield",	IFIELD,		__uint32_t,PV_TERSE),

    PFI_UINT("ulp_id",	"hfp_ulp_id",	HFP_ULP_ID,	u_char,	   PV_TERSE),
    PFI_UINT("hfp_flags","hfp_flags",	HFP_FLAGS,	u_char,	   PV_TERSE),
    PFI_UINT("d1d2off",	"hfp_d1d2off",	HFP_D1D2OFF,	u_short,   PV_TERSE),
    PFI_UINT("d2size",	"hfp_d2size",	HFP_D2SIZE,	__uint32_t,PV_TERSE),

    PFI_UINT("fcwm",	"hle_fcwm",	HLE_FCWM,	u_char,	   PV_DEFAULT),
    PFI_BYTE("dest_sw",	"hle_dest_swaddr",HLE_DEST_SW,	3,	   PV_DEFAULT),
    PFI_UINT("swat",	"hle_swat",	HLE_SWAT,	u_char,	   PV_DEFAULT),
    PFI_BYTE("src_sw",	"src_switch_addr",HLE_SRC_SW,	3,	   PV_DEFAULT),
    PFI_UINT("resvd",	"resvd",	HLE_RESVD,	u_short,   PV_VERBOSE),
    PFI_ADDR("dst",	"dest",		HLE_DEST,   HIPPI_ULA_SIZE,PV_DEFAULT),
    PFI_UINT("local_admin","local_admin",HLE_LOCAL_ADMIN,u_short,  PV_DEFAULT),
    PFI_ADDR("src",	"src",		HLE_SRC,    HIPPI_ULA_SIZE,PV_DEFAULT),
};


#define	HIPPIFID(pf)	((enum hippifid) (pf)->pf_id)
#define	HIPPIFIELD(fid)	hippifields[(int) fid]

Expr	*ether_resolve(char *, int, struct snooper *);
#define hippi_resolve ether_resolve

/*
 * HIPPI protocol macros and options.
 */
static ProtoMacro hippimacros[] = {
	PMI("arp",	"llc.arp"),
	PMI("arpip",	"llc.arpip"),
	PMI("ip",	"llc.ip"),
	PMI("hello",	"llc.hello"),
	PMI("icmp",	"llc.icmp"),
	PMI("igmp",	"llc.igmp"),
	PMI("rarp",	"llc.rarp"),
	PMI("rarpip",	"llc.rarpip"),
	PMI("rip",	"llc.rip"),
	PMI("sunrpc",	"llc.sunrpc"),
	PMI("nfs",	"llc.nfs"),
	PMI("tcp",	"llc.tcp"),
	PMI("udp",	"llc.udp"),
	PMI("bootp",	"llc.bootp"),
	PMI("dns",	"llc.dns"),
	PMI("snmp",	"llc.snmp"),
	PMI("tftp",	"llc.tftp"),
	PMI("idp",	"llc.idp"),
	PMI("ipx",	"llc.ipx"),
	PMI("echo",	"llc.echo"),
	PMI("error",	"llc.error"),
	PMI("spp",	"llc.spp"),
	PMI("xnsrip",	"llc.xnsrip"),
	PMI("pep",	"llc.pep"),
	PMI("decnet",	"llc.decnet"),
	PMI("nsp",	"llc.nsp"),
	PMI("lat",	"llc.lat"),
	PMI("xtp",	"llc.xtp"),
	PMI("ftp",	"llc.ftp"),
	PMI("netbios",	"llc.netbios"),
	PMI("scp",	"llc.scp"),
	PMI("smtp",	"llc.smtp"),
	PMI("spx",	"llc.spx"),
	PMI("telnet",	"llc.telnet"),
	PMI("timed",	"llc.timed"),
	PMI("tsp",	"llc.tsp"),
	PMI("ftp",	"llc.ftp"),
	PMI("novellecho","llc.novellecho"),
	PMI("novellerror","llc.novellerror"),
	PMI("novellrip","llc.novellrip"),
	PMI("osi",	"llc.osi"),
	PMI("rcp",	"llc.rcp"),
	PMI("rlogin",	"llc.rlogin"),
	PMI("sna",	"llc.sna"),
	PMI("vines",	"llc.vines"),
	PMI("x11",	"llc.x11"),
	PMI("ARP",	"llc.ARP"),
	PMI("ARPIP",	"llc.ARPIP"),
	PMI("HELLO",	"llc.HELLO"),
	PMI("IP",	"llc.IP"),
	PMI("ICMP",	"llc.ICMP"),
	PMI("IGMP",	"llc.IGMP"),
	PMI("RARP",	"llc.RARPIP"),
	PMI("RARPIP",	"llc.RARPIP"),
	PMI("RIP",	"llc.RIP"),
	PMI("SUNRPC",	"llc.SUNRPC"),
	PMI("NFS",	"llc.NFS"),
	PMI("TCP",	"llc.TCP"),
	PMI("UDP",	"llc.UDP"),
	PMI("BOOTP",	"llc.BOOTP"),
	PMI("DNS",	"llc.DNS"),
	PMI("SNMP",	"llc.SNMP"),
	PMI("TFTP",	"llc.TFTP"),
	PMI("IDP",	"llc.IDP"),
	PMI("IPX",	"llc.IPX"),
/*	PMI("ECHO",	"llc.ECHO"),	Don't cover icmp ECHO constant */
	PMI("ERROR",	"llc.ERROR"),
	PMI("SPP",	"llc.SPP"),
	PMI("XNSRIP",	"llc.XNSRIP"),
	PMI("PEP",	"llc.PEP"),
	PMI("DECNET",	"llc.DECNET"),
	PMI("NSP",	"llc.NSP"),
	PMI("LAT",	"llc.LAT"),
	PMI("XTP",	"llc.XTP"),
	PMI("FTP",	"llc.FTP"),
	PMI("NETBIOS",	"llc.NETBIOS"),
	PMI("SCP",	"llc.SCP"),
	PMI("SMTP",	"llc.SMTP"),
	PMI("SPX",	"llc.SPX"),
	PMI("TELNET",	"llc.TELNET"),
	PMI("TIMED",	"llc.TIMED"),
	PMI("TSP",	"llc.TSP"),
	PMI("FTP",	"llc.FTP"),
	PMI("NOVELLECHO","llc.NOVELLECHO"),
	PMI("NOVELLERROR","llc.NOVELLERROR"),
	PMI("NOVELLRIP","llc.NOVELLRIP"),
	PMI("OSI",	"llc.OSI"),
	PMI("RCP",	"llc.RCP"),
	PMI("RLOGIN",	"llc.RLOGIN"),
	PMI("SNA",	"llc.SNA"),
	PMI("VINES",	"llc.VINES"),
	PMI("X11",	"llc.X11"),
};

#define hippi_setopt pr_stub_setopt

DefineProtocol(hippi, hippiname, "HIgh-Performance Parallel Interface",
	       PRID_HIPPI, DS_BIG_ENDIAN, sizeof(struct hippi_head),
	       0, 0, 0, 0, 0);

static Protocol *llc;


static int
hippi_init(void)
{
	if (!pr_register(&hippi_proto, hippifields, lengthof(hippifields),
			 lengthof(hippimacros) + 100)) {
		return 0;
	}
	pr_addmacros(&hippi_proto, hippimacros, lengthof(hippimacros));
	pr_addconstnumber(&hippi_proto, "MTU", 65280);
	return 1;
}


/* ARGSUSED */
static void
hippi_embed(Protocol *pr, long prototype, long qualifier)
{
	if (pr->pr_id == PRID_LLC)
		llc = pr;
}


static ExprType
hippi_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	enum hippifid fid;
	u_char *bmask;
	long imask, match;

	fid = HIPPIFID(pf);
	switch (fid) {
	case HLE_DEST:
	case HLE_SRC:
		if (mex == 0)
			bmask = (u_char *)PC_ALLBYTES;
		else {
			if (mex->ex_op != EXOP_ADDRESS) {
				pc_badop(pc, mex);
				return ET_ERROR;
			}
			bmask = A_BASE(&mex->ex_addr, pf->pf_size);
		}
		if (tex->ex_op != EXOP_ADDRESS) {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_bytefield(pc, pf, (char *) bmask,
				  (char *)A_BASE(&tex->ex_addr, pf->pf_size),
				  ETHERHDRLEN)) {
			return ET_COMPLEX;
		}
		break;

	case IFIELD:
	case HFP_ULP_ID:
	case HFP_FLAGS:
	case HFP_D1D2OFF:
	case HFP_D2SIZE:
	case HLE_FCWM:
	case HLE_DEST_SW:
	case HLE_SWAT:
	case HLE_SRC_SW:
	case HLE_RESVD:
	case HLE_LOCAL_ADMIN: {
		if (!pc_intmask(pc, mex, &imask))
			return ET_ERROR;
		if (tex->ex_op == EXOP_NUMBER)
			match = tex->ex_val;
		else {
			pc_badop(pc, tex);
			return ET_ERROR;
		}
		if (!pc_intfield(pc, pf, imask, match, ETHERHDRLEN))
			return ET_COMPLEX;
		}
	}
	return ET_SIMPLE;
}


static int
hippi_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct hippi_head *h;
	struct hippiframe hf;
	int matched;

	h = (struct hippi_head *)ds_inline(ds, sizeof(*h), sizeof(__uint32_t));
	if (h == 0)
		return 0;
	PS_PUSH(ps, &hf.hf_frame, &hippi_proto);
	bcopy(&h->le.dest, hf.hf_dest, sizeof(hf.hf_dest));
	bcopy(&h->le.src, hf.hf_src, sizeof(hf.hf_src));
	matched = ex_match(pex, ds, ps, rex);
	PS_POP(ps);
	return matched;
}


/* ARGSUSED */
static int
hippi_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct hippi_head *h;

	h = (struct hippi_head *)ds_inline(ds, sizeof(*h), sizeof(__uint32_t));
	if (h == 0)
		return 0;
	switch (HIPPIFID(pf)) {
	case IFIELD:
		rex->ex_val = h->ifield;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HFP_ULP_ID:
		rex->ex_val = h->fp.hfp_ulp_id;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HFP_FLAGS:
		rex->ex_val = h->fp.hfp_flags;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HFP_D1D2OFF:
		rex->ex_val = h->fp.hfp_d1d2off;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HFP_D2SIZE:
		rex->ex_val = h->fp.hfp_d2size;
		rex->ex_op = EXOP_NUMBER;
		break;

	case HLE_FCWM:
		rex->ex_val = h->le.hle_fcwm;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_DEST_SW:
		rex->ex_val = ((h->le.hle_dest_swaddr[0] << 16)
			       + (h->le.hle_dest_swaddr[1] << 8)
			       + h->le.hle_dest_swaddr[2]);
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_SWAT:
		rex->ex_val = h->le.hle_swat;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_SRC_SW:
		rex->ex_val = ((h->le.src_switch_addr[0] << 16)
			       + (h->le.src_switch_addr[1] << 8)
			       + h->le.src_switch_addr[2]);
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_RESVD:
		rex->ex_val = h->le.resvd;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_DEST:
		bcopy(h->le.dest, A_CAST(&rex->ex_addr, hippi_ula_t),
		      sizeof(hippi_ula_t));
		rex->ex_op = EXOP_ADDRESS;
		break;
	case HLE_LOCAL_ADMIN:
		rex->ex_val = h->le.local_admin;
		rex->ex_op = EXOP_NUMBER;
		break;
	case HLE_SRC:
		bcopy(h->le.src, A_CAST(&rex->ex_addr, hippi_ula_t),
		      sizeof(hippi_ula_t));
		rex->ex_op = EXOP_ADDRESS;
		break;
	}
	return 1;
}

static void
hippi_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct hippi_head *h;
	Protocol *pr;
	struct hippiframe hf;
	u_short key;

	h = (struct hippi_head *)ds_inline(ds, sizeof(*h), sizeof(__uint32_t));
	if (h == 0)
		return;

	pv_showfield(pv, &HIPPIFIELD(IFIELD), &h->ifield,
		     "%#08x", h->ifield);

	pv_showfield(pv, &HIPPIFIELD(HFP_ULP_ID), &h->fp.hfp_ulp_id,
		     "%#04x", h->fp.hfp_ulp_id);
	pv_showfield(pv, &HIPPIFIELD(HFP_FLAGS), &h->fp.hfp_flags,
		     "%#04x", h->fp.hfp_flags);
	pv_showfield(pv, &HIPPIFIELD(HFP_D1D2OFF), &h->fp.hfp_d1d2off,
		     "%#-6x", h->fp.hfp_d1d2off);
	pv_showfield(pv, &HIPPIFIELD(HFP_D2SIZE), &h->fp.hfp_d2size,
		     "%8d", h->fp.hfp_d2size);

	pv_showfield(pv, &HIPPIFIELD(HLE_FCWM), &h->le.hle_fcwm,
		     "%#04x", h->le.hle_fcwm);
	pv_showfield(pv, &HIPPIFIELD(HLE_DEST_SW), &h->le.hle_dest_swaddr,
		     "%#08x", ((h->le.hle_dest_swaddr[0] << 16)
			       + (h->le.hle_dest_swaddr[1] << 8)
			       + h->le.hle_dest_swaddr[2]));
	pv_showfield(pv, &HIPPIFIELD(HLE_SWAT), &h->le.hle_swat,
		     "%#04x", h->le.hle_swat);
	pv_showfield(pv, &HIPPIFIELD(HLE_SRC_SW), &h->le.src_switch_addr,
		     "%#-6x", ((h->le.src_switch_addr[0] << 16)
			       + (h->le.src_switch_addr[1] << 8)
			       + h->le.src_switch_addr[2]));
	pv_showfield(pv, &HIPPIFIELD(HLE_RESVD), &h->le.resvd,
		     "%#-6x", h->le.resvd);
	pv_showfield(pv, &HIPPIFIELD(HLE_DEST), &h->le.dest,
		     "%-25.25s",
		     ether_hostname((struct etheraddr *)h->le.dest));
	pv_showfield(pv, &HIPPIFIELD(HLE_LOCAL_ADMIN), &h->le.local_admin,
		     "%#-6x", h->le.local_admin);
	pv_showfield(pv, &HIPPIFIELD(HLE_SRC), &h->le.src,
		     "%-25.25s",
		     ether_hostname((struct etheraddr *)h->le.src));

	PS_PUSH(ps, &hf.hf_frame, &hippi_proto);
	key = PRID_LLC;
	bcopy(&h->le.dest, hf.hf_dest, sizeof(hf.hf_dest));
	bcopy(&h->le.src, hf.hf_src, sizeof(hf.hf_src));
	pv_decodeframe(pv, llc, ds, ps);
	PS_POP(ps);
}
