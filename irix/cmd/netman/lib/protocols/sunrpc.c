/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Sun Remote Procedure Call (RPC), defined in RFC 1050.
 *
 * TODO:
 *	Resolve authentication strings formatted a la pv_show_opaque_auth
 */
#include <bstring.h>
#include <debug.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <stdio.h>
#include "cache.h"
#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protocols/ip.h"
#include "protocols/sunrpc.h"
#include "strings.h"

char sunrpcname[] = "sunrpc";

#define	COMMON_HDRLEN	8	/* # of leading bytes common to call & reply */

/*
 * SunRPC field identifiers and descriptors.
 */
enum sunrpcfid {
	XID, DIRECTION,
	    RPCVERS, PROG, VERS, PROC, CREDTYPE, CREDLEN, CRED,
	    VERFTYPE, VERFLEN, VERF,
	    STAT, ACCEPTSTAT, REJECTSTAT, LOW, HIGH, AUTHSTAT
};

/*
 * Selected SunRPC fields.
 */
static ProtoField sunrpcfields[] = {
/* common initial fields */
	PFI_UINT("xid",     "Transaction ID",     XID,      u_long, PV_TERSE),
	PFI_UINT("direction","Message Direction", DIRECTION,enum_t, PV_TERSE),

/* call message fields */
	PFI_UINT("rpcvers", "RPC Version Number", RPCVERS,  u_long, PV_VERBOSE),
	PFI_UINT("prog",    "Program Number",     PROG,     u_long, PV_DEFAULT),
	PFI_UINT("vers",    "Version Number",     VERS,     u_long, PV_VERBOSE),
	PFI_UINT("proc",    "Procedure Number",   PROC,     u_long, PV_DEFAULT),
	PFI_UINT("credtype","Credential Flavor",  CREDTYPE, enum_t, PV_TERSE),
	PFI_UINT("credlen", "Credential Length",  CREDLEN,  u_long, PV_VERBOSE),
	PFI_VAR("cred", "Call Credentials",   CRED, MAX_AUTH_BYTES, PV_DEFAULT),

/* call & reply fields */
	PFI_UINT("verftype","Verifier Flavor",    VERFTYPE, enum_t, PV_DEFAULT),
	PFI_UINT("verflen", "Verifier Length",    VERFLEN,  u_long, PV_VERBOSE),
	PFI_VAR("verf", "Credential Verifier",VERF, MAX_AUTH_BYTES, PV_DEFAULT),

/* reply message fields */
	PFI("stat", "Reply Status", STAT,   DS_ZERO_EXTEND, sizeof(enum_t),
	    COMMON_HDRLEN,  EXOP_NUMBER,    PV_TERSE),
	PFI_UINT("acceptstat","Accepted Status",  ACCEPTSTAT,enum_t,PV_TERSE),
	PFI_UINT("rejectstat","Rejected Status",  REJECTSTAT,enum_t,PV_TERSE),
	PFI_UINT("low",     "Low Version",        LOW,      u_long, PV_TERSE),
	PFI_UINT("high",    "High Version",       HIGH,     u_long, PV_TERSE),
	PFI_UINT("authstat","Authentication Status",AUTHSTAT,enum_t,PV_TERSE),
};

#define	SUNRPCFID(pf)		((enum sunrpcfid) (pf)->pf_id)
#define	SUNRPCFIELD(fid)	sunrpcfields[(int) fid]

/*
 * Nicknames for SunRPC and its protocols.
 */
ProtoMacro sunrpcnicknames[] = {
	PMI("SUNRPC",	sunrpcname),
	PMI("nfs",	"sunrpc.nfs"),
	PMI("NFS",	"sunrpc.nfs"),
#ifdef NOTYET
	PMI("mount",	"sunrpc.mount"),
	PMI("MOUNT",	"sunrpc.mount"),
	PMI("pmap",	"sunrpc.pmap"),
	PMI("PMAP",	"sunrpc.pmap"),
#endif
};

/*
 * SunRPC enumerated type names.
 */
static Enumeration msg_type, reply_stat, accept_stat, reject_stat,
		   auth_flavor, auth_stat;

static Enumerator
	msg_type_vec[]      = { EI(CALL), EI(REPLY) },
	reply_stat_vec[]    = { EI(MSG_ACCEPTED), EI(MSG_DENIED) },
	accept_stat_vec[]   = { EI(SUCCESS), EI(PROG_UNAVAIL),
				EI(PROG_MISMATCH), EI(PROC_UNAVAIL),
				EI(GARBAGE_ARGS), EI(SYSTEM_ERR) },
	reject_stat_vec[]   = { EI(RPC_MISMATCH), EI(AUTH_ERROR) },
	auth_flavor_vec[]   = { EI(AUTH_NULL), EI(AUTH_UNIX), EI(AUTH_SHORT),
				EI(AUTH_DES) },
	auth_stat_vec[]	    = { EI(AUTH_BADCRED), EI(AUTH_REJECTEDCRED),
				EI(AUTH_BADVERF), EI(AUTH_REJECTEDVERF),
				EI(AUTH_TOOWEAK), EI(AUTH_INVALIDRESP),
				EI(AUTH_FAILED) };

/*
 * SunRPC protocol options.
 */
static ProtOptDesc sunrpcoptdesc[] = {
	POD("portmap", SUNRPC_PROPT_PORTMAP,
		       "Query server portmappers during capture"),
};

/*
 * SunRPC protocol interface.
 */
#define	sunrpc_compile	pr_stub_compile

DefineProtocol(sunrpc, sunrpcname, "Sun Remote Procedure Call", PRID_SUNRPC,
	       DS_BIG_ENDIAN, 0, 0, PR_MATCHNOTIFY,
	       sunrpcoptdesc, lengthof(sunrpcoptdesc), 0);

/*
 * Caches that associate (xid,ipproto,client) with (prog,vers,proc), so that
 * sunrpc_match and sunrpc_decode can figure out what's in a reply.
 */
#define	MAXCALLS	32	/* maximum cache size */

static Cache *callmatches;
static Cache *calldecodes;

struct callkey {
	u_long		xid;
	u_int		ipproto;
	struct in_addr	client;
};

static unsigned int
sunrpc_callhash(struct callkey *ck)
{
	return ck->xid ^ ck->client.s_addr;
}

static int
sunrpc_callcmp(struct callkey *ck1, struct callkey *ck2)
{
	return ck1->xid != ck2->xid
	    || ck1->ipproto != ck2->ipproto
	    || ck1->client.s_addr != ck2->client.s_addr;
}

struct callval {
	u_long		prog;	/* called program */
	u_long		vers;	/* protocol version */
	u_long		proc;	/* procedure number */
	struct pmap	pmap;	/* args/result if PMAP_GETPORT */
};

static int
xdr_callval(XDR *xdr, struct callval **cvp)
{
	struct callval *cv;

	cv = *cvp;
	switch (xdr->x_op) {
	  case XDR_DECODE:
		if (cv == 0)
			*cvp = cv = new(struct callval);
		/* FALL THROUGH */
	  case XDR_ENCODE:
		return xdr_u_long(xdr, &cv->prog)
		    && xdr_u_long(xdr, &cv->vers)
		    && xdr_u_long(xdr, &cv->proc)
		    && xdr_pmap(xdr, &cv->pmap);
	  case XDR_FREE:
		delete(cv);
		*cvp = 0;
	}
	return TRUE;
}

static void
sunrpc_calldump(struct callkey *ck, struct callval *cv)
{
	printf("(%lu,%u,%s) -> (%lu,%lu,%lu)\n",
		ck->xid, ck->ipproto, ip_hostname(ck->client, IP_HOST),
		cv->prog, cv->vers, cv->proc);
}

static struct cacheops callcacheops =
    { { sunrpc_callhash, sunrpc_callcmp, xdr_callval }, 0, sunrpc_calldump };

/*
 * SunRPC protocol operations.
 */
static Index *sunrpcprograms;

static int
sunrpc_init()
{
	int nested;
	struct servent *sp;

	if (!pr_register(&sunrpc_proto, sunrpcfields, lengthof(sunrpcfields),
			 lengthof(msg_type_vec) + lengthof(reply_stat_vec)
			 + lengthof(accept_stat_vec) + lengthof(reject_stat_vec)
			 + lengthof(auth_flavor_vec) + lengthof(auth_stat_vec)
			 + 10)) {
		return 0;
	}

	nested = 0;
	sp = getservbyname(sunrpcname, "tcp");
	if (pr_nest(&sunrpc_proto, PRID_TCP, sp ? sp->s_port : PMAPPORT,
		    sunrpcnicknames, lengthof(sunrpcnicknames))) {
		nested = 1;
	}
	sp = getservbyname(sunrpcname, "udp");
	if (pr_nest(&sunrpc_proto, PRID_UDP, sp ? sp->s_port : PMAPPORT,
		    sunrpcnicknames, lengthof(sunrpcnicknames))) {
		nested = 1;
	}
	if (!nested)
		return 0;

	c_create("sunrpc.matches", MAXCALLS, sizeof(struct callkey), 2 MINUTES,
		 &callcacheops, &callmatches);
	c_create("sunrpc.decodes", MAXCALLS, sizeof(struct callkey), 2 MINUTES,
		 &callcacheops, &calldecodes);
	in_create(10, sizeof(u_long), 0, &sunrpcprograms);

	en_init(&msg_type, msg_type_vec, lengthof(msg_type_vec),
		&sunrpc_proto);
	en_init(&reply_stat, reply_stat_vec, lengthof(reply_stat_vec),
		&sunrpc_proto);
	en_init(&accept_stat, accept_stat_vec, lengthof(accept_stat_vec),
		&sunrpc_proto);
	en_init(&reject_stat, reject_stat_vec, lengthof(reject_stat_vec),
		&sunrpc_proto);
	en_init(&auth_flavor, auth_flavor_vec, lengthof(auth_flavor_vec),
		&sunrpc_proto);
	en_init(&auth_stat, auth_stat_vec, lengthof(auth_stat_vec),
		&sunrpc_proto);
	setrpcent(1);
	return 1;
}

static int
sunrpc_setopt(int id, char *val)
{
	extern int _sunrpcportmap;

	if ((enum sunrpc_propt) id != SUNRPC_PROPT_PORTMAP)
		return 0;
	_sunrpcportmap = (*val != '0');
	return 1;
}

/* ARGSUSED */
static void
sunrpc_embed(Protocol *pr, long prog, long qual)
{
	(void) in_enter(sunrpcprograms, &prog, pr);
}

/* ARGSUSED */
static Expr *
sunrpc_resolve(char *name, int len, struct snooper *sn)
{
	struct rpcent *rp;
	Expr *ex;

	rp = getrpcbyname(name);
	if (rp == 0)
		return 0;
	ex = expr(EXOP_NUMBER, EX_NULLARY, name);
	ex->ex_val = rp->r_number;
	return ex;
}

int	sunrpc_get_msg(DataStream *, XDR *, struct rpc_msg *);
void	sunrpc_free_msg(XDR *, struct rpc_msg *);
void	sunrpc_spy_on_getport(enum msg_type, struct in_addr, DataStream *,
			      struct callval *);
int	sunrpc_register(struct in_addr, struct pmap *);

static int
sunrpc_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	int procoff, matched;
	XDR xdr;
	struct rpc_msg msg;
	struct ipframe *ipf;
	struct callkey ck;
	struct callval *cv;
	struct sunrpcframe srf;

	procoff = DS_TELL(ds);
	if (!sunrpc_get_msg(ds, &xdr, &msg))
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	ck.xid = msg.rm_xid;
	ck.ipproto = ipf->ipf_prototype;
	switch (msg.rm_direction) {
	  case CALL:
		procoff += SUNRPCFIELD(PROC).pf_off;
		ck.client = ipf->ipf_src;
		cv = new(struct callval);
		cv->prog = msg.rm_call.cb_prog;
		cv->vers = msg.rm_call.cb_vers;
		cv->proc = msg.rm_call.cb_proc;
		c_enter(callmatches, &ck, cv);
		break;

	  case REPLY:
		procoff = -1;
		ck.client = ipf->ipf_rdst;
		cv = c_match(callmatches, &ck);
	}

	if (cv && cv->prog == PMAPPROG && cv->vers == PMAPVERS
	    && cv->proc == PMAPPROC_GETPORT) {
		sunrpc_spy_on_getport(msg.rm_direction, ipf->ipf_src, ds, cv);
	}

	if (pex == 0)
		matched = 1;
	else if (cv == 0 || cv->prog != pex->ex_prsym->sym_prototype)
		matched = 0;
	else {
		srf.srf_prog = cv->prog;
		srf.srf_vers = cv->vers;
		srf.srf_proc = cv->proc;
		srf.srf_direction = msg.rm_direction;
		srf.srf_procoff = procoff;
		srf.srf_morefrags = ipf->ipf_morefrags;
		srf.srf_xdr = &xdr;
		PS_PUSH(ps, &srf.srf_frame, &sunrpc_proto);
		matched = ex_match(pex, ds, ps, rex);
		PS_POP(ps);
	}

	sunrpc_free_msg(&xdr, &msg);
	return matched;
}

static int
sunrpc_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	XDR xdr;
	struct rpc_msg msg;
	struct ipframe *ipf;
	int ok;

	if (!sunrpc_get_msg(ds, &xdr, &msg))
		return 0;
	ipf = PS_TOP(ps, struct ipframe);
	ok = 1;
	rex->ex_op = EXOP_NUMBER;
	switch (msg.rm_direction) {
	  case CALL:
		switch (SUNRPCFID(pf)) {
		  case XID:
			rex->ex_val = msg.rm_xid;
			break;
		  case DIRECTION:
			rex->ex_val = (long) msg.rm_direction;
			break;
		  case RPCVERS:
			rex->ex_val = msg.rm_call.cb_rpcvers;
			break;
		  case PROG:
			rex->ex_val = msg.rm_call.cb_prog;
			break;
		  case VERS:
			rex->ex_val = msg.rm_call.cb_vers;
			break;
		  case PROC:
			rex->ex_val = msg.rm_call.cb_proc;
			break;
		  case CREDTYPE:
			rex->ex_val = msg.rm_call.cb_cred.oa_flavor;
			break;
		  case CREDLEN:
			rex->ex_val = msg.rm_call.cb_cred.oa_length;
			break;
		  case VERFTYPE:
			rex->ex_val = msg.rm_call.cb_verf.oa_flavor;
			break;
		  case VERFLEN:
			rex->ex_val = msg.rm_call.cb_verf.oa_length;
			break;
		  default:
			ok = 0;
		}
		break;

	  case REPLY:
		switch (SUNRPCFID(pf)) {
		  case XID:
			rex->ex_val = msg.rm_xid;
			break;
		  case DIRECTION:
			rex->ex_val = (long) msg.rm_direction;
			break;
		  case STAT:
			rex->ex_val = (long) msg.rm_reply.rp_stat;
			break;
		  case VERFTYPE:
			switch (msg.rm_reply.rp_stat) {
			  case MSG_ACCEPTED:
				rex->ex_val =
					msg.rm_reply.rp_acpt.ar_verf.oa_flavor;
				break;
			  default:
				ok = 0;
			}
			break;
		  case ACCEPTSTAT:
			switch (msg.rm_reply.rp_stat) {
			  case MSG_ACCEPTED:
				rex->ex_val = msg.rm_reply.rp_acpt.ar_stat;
				break;
			  default:
				ok = 0;
			}
			break;
		  case REJECTSTAT:
			switch (msg.rm_reply.rp_stat) {
			  case MSG_DENIED:
				rex->ex_val = msg.rm_reply.rp_rjct.rj_stat;
				break;
			  default:
				ok = 0;
			}
			break;
		  case LOW:
		  case HIGH:
			switch (msg.rm_reply.rp_stat) {
			  case MSG_ACCEPTED:
				switch (msg.rm_reply.rp_acpt.ar_stat) {
				  case PROG_MISMATCH:
					rex->ex_val = (SUNRPCFID(pf) == LOW)
					    ? msg.rm_reply.rp_acpt.ar_vers.low
					    : msg.rm_reply.rp_acpt.ar_vers.high;
					break;
				  default:
					ok = 0;
				}
				break;
			  case MSG_DENIED:
				switch (msg.rm_reply.rp_rjct.rj_stat) {
				  case RPC_MISMATCH:
					rex->ex_val = (SUNRPCFID(pf) == LOW)
					    ? msg.rm_reply.rp_rjct.rj_vers.low
					    : msg.rm_reply.rp_rjct.rj_vers.high;
					break;
				  default:
					ok = 0;
				}
				break;
			  default:
				ok = 0;
			}
			break;
		  case AUTHSTAT:
			switch (msg.rm_reply.rp_stat) {
			  case MSG_DENIED:
				switch (msg.rm_reply.rp_rjct.rj_stat) {
				  case AUTH_ERROR:
					rex->ex_val = 
						msg.rm_reply.rp_rjct.rj_why;
					break;
				  default:
					ok = 0;
				}
				break;
			  default:
				ok = 0;
			}
			break;
		  default:
			ok = 0;
		}
	}
	sunrpc_free_msg(&xdr, &msg);
	return ok;
}

static char *
sunrpc_progname(u_long prog)
{
	struct rpcent *rp;
	static char buf[10+1];

	rp = getrpcbynumber(prog);
	if (rp)
		return rp->r_name;
	(void) sprintf(buf, "%lu", prog);
	return buf;
}

void	pv_show_opaque_auth(PacketView *, enum sunrpcfid, struct opaque_auth *);
void	pv_show_version_mismatch(PacketView *, u_long, u_long);

static char u_long_format[] = "%-10lu";

static void
sunrpc_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	int procoff;
	XDR xdr;
	struct rpc_msg msg;
	struct ipframe *ipf;
	struct callkey ck;
	struct callval *cv;
	struct sunrpcframe srf;
	Protocol *pr;

	procoff = DS_TELL(ds);
	if (!sunrpc_get_msg(ds, &xdr, &msg))
		return;
	pv_showfield(pv, &SUNRPCFIELD(XID), &msg.rm_xid,
		     u_long_format, msg.rm_xid);
	pv_showfield(pv, &SUNRPCFIELD(DIRECTION), &msg.rm_direction,
		     "%-8.8s", en_name(&msg_type, (int)msg.rm_direction));

	ipf = PS_TOP(ps, struct ipframe);
	ck.xid = msg.rm_xid;
	ck.ipproto = ipf->ipf_prototype;
	switch (msg.rm_direction) {
	  case CALL:
		procoff += SUNRPCFIELD(PROC).pf_off;
		ck.client = ipf->ipf_src;
		cv = new(struct callval);
		cv->prog = msg.rm_call.cb_prog;
		cv->vers = msg.rm_call.cb_vers;
		cv->proc = msg.rm_call.cb_proc;
		c_enter(calldecodes, &ck, cv);

		pv_showfield(pv, &SUNRPCFIELD(RPCVERS), &msg.rm_call.cb_rpcvers,
			     u_long_format, msg.rm_call.cb_rpcvers);
		pr = in_match(sunrpcprograms, &cv->prog);
		pv_showfield(pv, &SUNRPCFIELD(PROG), &cv->prog,
			     pr ? "%-14.14s" : "@t%-14.14s",
			     pr ? pr->pr_name : sunrpc_progname(cv->prog));
		pv_showfield(pv, &SUNRPCFIELD(VERS), &cv->vers,
			     u_long_format, cv->vers);
		pv_showfield(pv, &SUNRPCFIELD(PROC), &cv->proc,
			     pr ? u_long_format : "@t%-10lu", cv->proc);
		pv_show_opaque_auth(pv, CRED, &msg.rm_call.cb_cred);
		pv_show_opaque_auth(pv, VERF, &msg.rm_call.cb_verf);
		break;

	  case REPLY:
		procoff = -1;
		ck.client = ipf->ipf_rdst;
		cv = c_match(calldecodes, &ck);
		pr = (cv == 0) ? 0 : in_match(sunrpcprograms, &cv->prog);

		pv_showfield(pv, &SUNRPCFIELD(STAT), &msg.rm_reply.rp_stat,
			     "%-12.12s",
			     en_name(&reply_stat, (int)msg.rm_reply.rp_stat));
		switch (msg.rm_reply.rp_stat) {
		  case MSG_ACCEPTED:
			pv_show_opaque_auth(pv, VERF,
					    &msg.rm_reply.rp_acpt.ar_verf);
			pv_showfield(pv, &SUNRPCFIELD(ACCEPTSTAT),
				     &msg.rm_reply.rp_acpt.ar_stat,
				     (msg.rm_reply.rp_acpt.ar_stat == SUCCESS) ?
					"@v%-13.13s" : "%-13.13s",
				     en_name(&accept_stat,
					     msg.rm_reply.rp_acpt.ar_stat));
			switch (msg.rm_reply.rp_acpt.ar_stat) {
			  case PROG_MISMATCH:
				pv_show_version_mismatch(pv,
					msg.rm_reply.rp_acpt.ar_vers.low,
					msg.rm_reply.rp_acpt.ar_vers.high);
			}
			break;
		  case MSG_DENIED:
			pv_showfield(pv, &SUNRPCFIELD(REJECTSTAT),
				     &msg.rm_reply.rp_rjct.rj_stat, "%-12.12s",
				     en_name(&reject_stat,
					     msg.rm_reply.rp_rjct.rj_stat));
			switch (msg.rm_reply.rp_rjct.rj_stat) {
			  case RPC_MISMATCH:
				pv_show_version_mismatch(pv,
					msg.rm_reply.rp_rjct.rj_vers.low,
					msg.rm_reply.rp_rjct.rj_vers.high);
				break;
			  case AUTH_ERROR:
				pv_showfield(pv, &SUNRPCFIELD(AUTHSTAT),
					     &msg.rm_reply.rp_rjct.rj_why,
					     "%-17.17s", en_name(&auth_stat,
						msg.rm_reply.rp_rjct.rj_why));
			}
		}
	}

	if (cv) {
		if (cv->prog == PMAPPROG && cv->vers == PMAPVERS
		    && cv->proc == PMAPPROC_GETPORT) {
			sunrpc_spy_on_getport(msg.rm_direction, ipf->ipf_src,
					      ds, cv);
		}

		srf.srf_prog = cv->prog;
		srf.srf_vers = cv->vers;
		srf.srf_proc = cv->proc;
		srf.srf_direction = msg.rm_direction;
		srf.srf_procoff = procoff;
		srf.srf_morefrags = ipf->ipf_morefrags;
		srf.srf_xdr = &xdr;
		PS_PUSH(ps, &srf.srf_frame, &sunrpc_proto);
	}

	pv_decodeframe(pv, pr, ds, ps);
	if (cv)
		PS_POP(ps);
	sunrpc_free_msg(&xdr, &msg);
}

static void
pv_show_opaque_auth(PacketView *pv, enum sunrpcfid fid, struct opaque_auth *oa)
{
	char buf[128];

	pv_showfield(pv, &sunrpcfields[(int)fid - 2], &oa->oa_flavor,
		     "%-10.10s", en_name(&auth_flavor, oa->oa_flavor));
	pv_showfield(pv, &sunrpcfields[(int)fid - 1], &oa->oa_length,
		     "%-3lu", oa->oa_length);
	if (oa->oa_base == 0)
		return;
	switch (oa->oa_flavor) {
	  case AUTH_NULL:
		return;

	  case AUTH_UNIX: {
		XDR xdr;
		struct authunix_parms aup;
		int cc, len;
		gid_t *grp;

		xdrmem_create(&xdr, oa->oa_base, oa->oa_length, XDR_DECODE);
		aup.aup_machname = 0;
		aup.aup_gids = 0;
		if (!xdr_authunix_parms(&xdr, &aup)) {
			/* XXX */
			break;
		}
		cc = nsprintf(buf, sizeof buf, "%s:%d.%d",
			      aup.aup_machname, aup.aup_uid, aup.aup_gid);
		len = MIN(aup.aup_len, NGRPS);
		for (grp = aup.aup_gids; --len >= 0; grp++)
			cc += nsprintf(&buf[cc], sizeof buf - cc, ",%d", *grp);
		xdr.x_op = XDR_FREE;
		(void) xdr_authunix_parms(&xdr, &aup);
		xdr_destroy(&xdr);
		break;
	  }

	  case AUTH_SHORT: {
		int cc, rem;
		char *bp;

		/*
		 * Shorthand UNIX verifiers have been obsolete for a while,
		 * so just print an initial substring in hex.
		 */
		cc = 0;
		rem = MIN(oa->oa_length, 20);
		for (bp = oa->oa_base; --rem >= 0; bp++)
			cc += nsprintf(&buf[cc], sizeof buf - cc, "%02x", *bp);
		break;
	  }

	  /*
	  ** Gone. - jes
	  ** RPC Version one support has been removed from libc.
	  **
	  case AUTH_DES: {
		XDR xdr;

		xdrmem_create(&xdr, oa->oa_base, oa->oa_length, XDR_DECODE);
		if (fid == CRED) {
			struct authdes_cred adc;

			adc.adc_fullname.name = 0;
			if (!xdr_authdes_cred(&xdr, &adc)) {
				break;
			}
			switch (adc.adc_namekind) {
			  case ADN_FULLNAME:
				(void) nsprintf(buf, sizeof buf,
						"%s:%08lx%08lx[%08lx]",
						adc.adc_fullname.name,
						adc.adc_fullname.key.key.high,
						adc.adc_fullname.key.key.low,
						adc.adc_fullname.window);
				xdr.x_op = XDR_FREE;
				(void) xdr_authdes_cred(&xdr, &adc);
				break;
			  case ADN_NICKNAME:
				(void) nsprintf(buf, sizeof buf, "%08lx",
						adc.adc_nickname);
				break;
			  default:
			}
		} else {
			struct authdes_verf adv;

			if (!xdr_authdes_verf(&xdr, &adv)) {
				break;
			}
			(void) sprintf(buf, "%08lx%08lx[%08lx]",
				       adv.adv_xtimestamp.key.high,
				       adv.adv_xtimestamp.key.low,
				       adv.adv_winverf);
		}
		xdr_destroy(&xdr);
		break;
	  }
	  */
	}
	pv_showvarfield(pv, &SUNRPCFIELD(fid), oa->oa_base, oa->oa_length,
			"%-20s", buf);
	pv_break(pv);
}

static void
pv_show_version_mismatch(PacketView *pv, u_long low, u_long high)
{
	static char versformat[] = "%-2lu";

	pv_showfield(pv, &SUNRPCFIELD(LOW), &low, versformat, low);
	pv_showfield(pv, &SUNRPCFIELD(HIGH), &low, versformat, high);
}

static int
sunrpc_get_msg(DataStream *ds, XDR *xdr, struct rpc_msg *rm)
{
	enum msg_type direction;

	if (ds->ds_count < COMMON_HDRLEN)	/* can we lookahead? */
		return 0;
	xdrdata_create(xdr, ds);
	bzero((char *) rm, sizeof *rm);

#define	RM	((struct rpc_msg *)ds->ds_next)
	if ((unsigned long)RM % sizeof direction != 0)
		bcopy(&RM->rm_direction, &direction, sizeof direction);
	else
		direction = RM->rm_direction;
#undef RM

	switch (direction) {
	  case CALL:
		(void) xdr_callmsg(xdr, rm);
		break;
	  case REPLY:
		rm->acpted_rply.ar_results.where = 0;
		rm->acpted_rply.ar_results.proc = xdr_void;
		(void) xdr_replymsg(xdr, rm);
		break;
	  default:
		return 0;
	}
	return 1;
}

static void
sunrpc_free_msg(XDR *xdr, struct rpc_msg *rm)
{
	xdr->x_op = XDR_FREE;
	switch (rm->rm_direction) {
	  case CALL:
		(void) xdr_callmsg(xdr, rm);
		break;
	  case REPLY:
		(void) xdr_replymsg(xdr, rm);
	}
	xdr_destroy(xdr);
}

static void
sunrpc_spy_on_getport(enum msg_type direction, struct in_addr server,
		      DataStream *ds, struct callval *cv)
{
	if ((u_long)ds->ds_next % sizeof(u_long) != 0)
		return;
	if (direction == CALL) {
		if (ds->ds_count >= sizeof cv->pmap)
			cv->pmap = *(struct pmap *)ds->ds_next;
		else
			cv->pmap.pm_prot = 0;
	} else {
		if (ds->ds_count >= sizeof cv->pmap.pm_port) {
			cv->pmap.pm_port = *(u_long *)ds->ds_next;
			(void) sunrpc_register(server, &cv->pmap);
		}
	}
}

/*
 * This function is exported to pmap.c.
 */
int
sunrpc_register(struct in_addr server, struct pmap *pm)
{
	int prid;

	sunrpc_addmap(server, pm);
	switch (pm->pm_prot) {		/* determine protocol id */
	  case IPPROTO_TCP:
		prid = PRID_TCP;
		break;
	  case IPPROTO_UDP:
		prid = PRID_UDP;
		break;
	  default:
		prid = PRID_NULL;
	}
	return pr_nestqual(&sunrpc_proto, prid, pm->pm_port, server.s_addr,
			   sunrpcnicknames, lengthof(sunrpcnicknames));
}
