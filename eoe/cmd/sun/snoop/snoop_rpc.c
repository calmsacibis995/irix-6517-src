/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_rpc.c,v 1.2 1996/07/06 17:51:45 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>

#include <netinet/in.h>
#include <netdb.h>

#include <sys/tiuser.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/auth_des.h>
#ifdef sgi
#include "auth_kerb.h"
#else
#include <rpc/auth_kerb.h>
#endif
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/pmap_clnt.h>
#include <rpc/svc.h>
#include <rpcsvc/yp_prot.h>
#include <rpc/pmap_prot.h>
#include "snoop.h"

#define	AUTH_KERB 4	/* XXXX remove when its in auth.h */

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

int pos;
struct cache_struct *find_xid();
extern jmp_buf xdr_err;
void protoprint();
char *nameof_prog();
char *nameof_astat();
char *nameof_why();

#define	LAST_FRAG ((u_long)1 << 31)

void rpc_detail_call(int flags, int xid, int rpcvers, int prog, int vers, int proc,
	char *data, int len);
void print_creds(void);
void print_verif(int direction);
void stash_xid(u_long xid, int frame, int prog, int vers, int proc);
void rpc_detail_reply(int flags, int xid, struct cache_struct *x, char *data, int len);

interpret_rpc(flags, rpc, fraglen, type)
	int flags;
	char *rpc;
	int fraglen, type;
{
	u_long xid;
	int direction;
	struct cache_struct *x;
	int rpcvers, prog, vers, proc;
	int status, astat, rstat, why;
	char *lp;
	unsigned recmark;
	int markpos;
	extern int pi_frame;
	int lo, hi;

	xdr_init(rpc, fraglen);

	if (setjmp(xdr_err)) {
		if (flags & F_DTAIL)
			(void) sprintf(get_line(0, 0),
				"----  short frame ---");
		return (fraglen);
	}

	if (type == IPPROTO_TCP) {	/* record mark */
		markpos = getxdr_pos();
		recmark = getxdr_long();
	}

	xid	  = getxdr_u_long();
	direction = getxdr_long();

	if (direction == CALL) {
		rpcvers = getxdr_long();
		pos = getxdr_pos();
		prog = getxdr_long();
		vers = getxdr_long();
		proc = getxdr_long();
		stash_xid(xid, pi_frame, prog, vers, proc);
		if (!(flags & (F_SUM | F_DTAIL)))
			protoprint(flags, CALL, xid, prog, vers, proc,
				rpc, fraglen);
	} else {
		x = find_xid(xid);
	}

	if (flags & F_SUM) {
		switch (direction) {
		case CALL:
			(void) sprintf(get_sum_line(),
				"RPC C XID=%lu PROG=%d (%s) VERS=%d PROC=%d",
				xid,
				prog, nameof_prog(prog),
				vers, proc);
			xdr_skip(4);			/* Cred auth type */
			xdr_skip(getxdr_long());	/* Cred auth data */
			xdr_skip(4);			/* Verf auth type */
			xdr_skip(getxdr_long());	/* Verf auth data */

			protoprint(flags, CALL, xid, prog, vers, proc,
				rpc, fraglen);
			break;

		case REPLY:
			lp = get_sum_line();
			if (x == NULL)
				(void) sprintf(lp, "RPC R XID=%lu", xid);
			else
				(void) sprintf(lp, "RPC R (#%d) XID=%lu",
					x->xid_frame, xid);

			lp += strlen(lp);
			status = getxdr_long();
			switch (status) {
			case MSG_ACCEPTED:
				(void) getxdr_long();
				xdr_skip(getxdr_long());
				astat = getxdr_long();
				(void) sprintf(lp, " %s",
					nameof_astat(astat));
				lp += strlen(lp);

				switch (astat) {
				case SUCCESS:
					if (x) {
						protoprint(flags, REPLY,
							xid,
							x->xid_prog,
							x->xid_vers,
							x->xid_proc,
							rpc, fraglen);
					}
					break;

				case PROG_UNAVAIL :
				case PROG_MISMATCH:
				case PROC_UNAVAIL :
					lo = getxdr_long();
					hi = getxdr_long();
					(void) sprintf(lp,
						" (low=%d, high=%d)",
						lo, hi);
					break;

				case GARBAGE_ARGS:
				case SYSTEM_ERR:
				default:
					;
				}
				break;

			case MSG_DENIED:
				rstat = getxdr_long();

				switch (rstat) {
				case RPC_MISMATCH:
					lo = getxdr_long();
					hi = getxdr_long();
					(void) sprintf(lp,
					" Vers mismatch (low=%d, high=%d)",
						lo, hi);
					break;

				case AUTH_ERROR:
					why = getxdr_u_long();
					(void) sprintf(lp,
						" Can't authenticate (%s)",
						nameof_why(why));
					break;
				}
			}
			break;
		}
	}

	if (flags & F_DTAIL) {
		show_header("RPC:  ", "SUN RPC Header", fraglen);
		show_space();
		if (type == IPPROTO_TCP) {	/* record mark */
			(void) sprintf(get_line(markpos, markpos+4),
				"Record Mark: %s fragment, length = %d",
				recmark & LAST_FRAG ? "last" : "",
				recmark & ~LAST_FRAG);
		}

		(void) sprintf(get_line(0, 0),
			"Transaction id = %lu",
			xid);
		(void) sprintf(get_line(0, 0),
			"Type = %d (%s)",
			direction,
			direction == CALL ? "Call":"Reply");

		switch (direction) {
		case CALL:
			rpc_detail_call(flags, xid, rpcvers,
				prog, vers, proc, rpc, fraglen);
			break;
		case REPLY:
			rpc_detail_reply(flags, xid, x, rpc, fraglen);
			break;
		}
	}

	return (fraglen);
}

void
rpc_detail_call(flags, xid, rpcvers, prog, vers, proc, data, len)
	int flags, xid, rpcvers, prog, vers, proc;
	char *data;
	int len;
{
	char *nameof_flavor();
	char *nameof_prog();

	(void) sprintf(get_line(pos, getxdr_pos()),
		"RPC version = %d",
		rpcvers);
	(void) sprintf(get_line(pos, getxdr_pos()),
		"Program = %d (%s), version = %d, procedure = %d",
		prog, nameof_prog(prog), vers, proc);
	print_creds();
	print_verif(CALL);
	show_trailer();
	protoprint(flags, CALL, xid, prog, vers, proc, data, len);
}

char *
nameof_flavor(flavor)
	int flavor;
{
	switch (flavor) {
	case AUTH_NONE : return ("None");
	case AUTH_UNIX : return ("Unix");
	case AUTH_SHORT: return ("Unix short");
	case AUTH_DES  : return ("DES");
	case AUTH_KERB : return ("Kerberos");
	default: return ("unknown");
	}
}

char *
tohex(p, len)
	char *p; int len;
{
	int i, j;
	static char hbuff[1024];
	static char *hexstr = "0123456789ABCDEF";

	j = 0;
	for (i = 0; i < len; i++) {
		hbuff[j++] = hexstr[p[i] >> 4	& 0x0f];
		hbuff[j++] = hexstr[p[i]	& 0x0f];
	}
	hbuff[j] = '\0';

	return (hbuff);
}

void
print_creds()
{
	int pos, flavor, authlen;
	int uid, gid, len;
	int tlen, idlen;
	int i, namekind;
	char *p, *line;

	pos = getxdr_pos();
	flavor  = getxdr_long();
	authlen = getxdr_long();
	(void) sprintf(get_line(pos, getxdr_pos()),
		"Credentials: Flavor = %d (%s), len = %d bytes",
		flavor, nameof_flavor(flavor), authlen);
	if (authlen == 0)
		return;

	switch (flavor) {
	case AUTH_UNIX:
		(void) showxdr_time("   Time = %s");
		(void) showxdr_string(MAX_MACHINE_NAME, "   Hostname = %s");
		pos = getxdr_pos();
		uid = getxdr_u_long();
		gid = getxdr_u_long();
		(void) sprintf(get_line(pos, getxdr_pos()),
			"   Uid = %d, Gid = %d",
			uid, gid);
		len = getxdr_u_long();
		line = get_line(pos, len * 4);
		if (len == 0)
			(void) sprintf(line, "   Groups = (none)");
		else {
			(void) sprintf(line, "   Groups = ");
			line += strlen(line);
			while (len--) {
				gid = getxdr_u_long();
				(void) sprintf(line, "%d ", gid);
				line += strlen(line);
			}
		}
		break;

	case AUTH_DES:
		namekind = getxdr_u_long();
		(void) sprintf(get_line(pos, getxdr_pos()),
			"   Name kind = %d (%s)",
			namekind,
			namekind == ADN_FULLNAME ?
				"fullname" : "nickname");
			switch (namekind) {
			case ADN_FULLNAME:
				(void) showxdr_string(64,
					"   Network name = %s");
				(void) showxdr_hex(8,
				"   Conversation key = 0x%s (DES encrypted)");
				(void) showxdr_hex(4,
				"   Window = 0x%s (DES encrypted)");
				break;

			case ADN_NICKNAME:
				(void) showxdr_hex(4, "   Nickname = 0x%s");
				break;
			};
		break;

	case AUTH_KERB:
		namekind = getxdr_u_long();
		(void) sprintf(get_line(pos, getxdr_pos()),
			"   Name kind = %d (%s)",
			namekind,
			namekind == ADN_FULLNAME ? "fullname" : "nickname");
			switch (namekind) {
			case AKN_FULLNAME:
				(void) sprintf(get_line(0, 0),
					"   Ticket");
				p = getxdr_bytes((u_int *)&len);
				(void) sprintf(get_line(0, 0),
					"      Version = %d",
					*p++);
				(void) sprintf(get_line(0, 0),
					"      Message type = 0x%02x",
					*p++);
				(void) sprintf(get_line(0, 0),
					"      Key version = %d",
					*p++);
				(void) sprintf(get_line(0, 0),
					"      Realm = %s",
					p);
				p += strlen(p) + 1;
				tlen = *p++;
				idlen = *p++;
				(void) sprintf(get_line(0, 0),
				"      Ticket length = %d, ID length = %d",
					tlen, idlen);
				(void) sprintf(get_line(0, 0),
					"      Server ticket (DES encrypted)");
				for (i = 0; i < tlen; i += 16) {
					len = MIN(16, tlen - i);
					(void) sprintf(get_line(0, 0),
						"         %s",
						tohex(p, len));
					p += len;
				}
				(void) sprintf(get_line(0, 0),
					"      Request ID (DES encrypted)");
				for (i = 0; i < idlen; i += 16) {
					len = MIN(16, idlen - i);
					(void) sprintf(get_line(0, 0),
						"         %s",
						tohex(p, len));
					p += len;
				}

				(void) showxdr_hex(4,
				"   Window = 0x%s (DES encrypted)");
				break;

			case AKN_NICKNAME:
				(void) showxdr_hex(4, "   Nickname = 0x%s");
				break;
			};
		break;

	default:
		(void) showxdr_hex(authlen, "[%s]");
		break;
	}
}

void
print_verif(direction)
	int direction;
{
	int pos, flavor, verlen;

	pos = getxdr_pos();
	flavor = getxdr_long();
	verlen = getxdr_long();
	(void) sprintf(get_line(pos, getxdr_pos()),
		"Verifier   : Flavor = %d (%s), len = %d bytes",
		flavor, nameof_flavor(flavor), verlen);
	if (verlen == 0)
		return;

	switch (flavor) {
	case AUTH_DES:
		(void) showxdr_hex(8, "   Timestamp = 0x%s (DES encrypted)");
		if (direction == CALL)
			(void) showxdr_hex(4,
				"   Window    = 0x%s (DES encrypted)");
		else
			(void) showxdr_hex(4, "   Nickname  = 0x%s");
		break;

	case AUTH_KERB:
		(void) showxdr_hex(8, "   Timestamp = 0x%s (DES encrypted)");
		if (direction == CALL)
			(void) showxdr_hex(4,
			"   Window    = 0x%s (DES encrypted)");
		else
			(void) showxdr_hex(4, "   Nickname  = 0x%s");
		break;

	default:
		(void) showxdr_hex(verlen, "[%s]");
		break;
	}
}

struct rpcnames {
	int   rp_prog;
	char *rp_name;
} rpcnames[] = {
100000, "PMAP",			/* Portmapper */
100001, "RSTAT",		/* Remote stats */
100002, "RUSERS",		/* Remote users */
100003, "NFS",			/* Nfs */
100004, "NIS",			/* Network Information Service */
100005, "MOUNT",		/* Mount demon */
100006, "DBX",			/* Remote dbx */
100007, "NISBIND",		/* NIS binder */
100008, "WALL",			/* Shutdown msg */
100009, "NISPASSWD",		/* Yppasswd server */
100010, "ETHERSTAT",		/* Ether stats */
100011, "RQUOTA",		/* Disk quotas */
100012, "SPRAY",		/* Spray packets */
100013, "IBM3270",		/* 3270 mapper */
100014, "IBMRJE",		/* RJE mapper */
100015, "SELNSVC",		/* Selection service */
100016, "RDATABASE",		/* Remote database access */
100017, "REX",			/* Remote execution */
100018, "ALICE",		/* Alice Office Automation */
100019, "SCHED",		/* Scheduling service */
100020, "LLM",			/* Local lock manager */
100021, "NLM",			/* Network lock manager */
100022, "X25INR",		/* X.25 inr protocol */
100023, "STATMON1",		/* Status monitor 1 */
100024, "STATMON2",		/* Status monitor 2 */
100025, "SELNLIB",		/* Selection library */
100026, "BOOTPARAM",		/* Boot parameters service */
100027, "MAZEPROG",		/* Mazewars game */
100028, "NISUPDATE",		/* NIS update */
100029, "KEYSERVE",		/* Key server */
100030, "SECURECMD",		/* Secure login */
100031, "NETFWDI",		/* NFS net forwarder init */
100032, "NETFWDT",		/* NFS net forwarder trans */
100033, "SUNLINKMAP",		/* Sunlink MAP */
100034, "NETMON",		/* Network monitor */
100035, "DBASE",		/* Lightweight database */
100036, "PWDAUTH",		/* Password authorization */
100037, "TFS",			/* Translucent file svc */
100038, "NSE",			/* NSE server */
100039, "NSE_ACTIVATE",		/* NSE activate daemon */
100040, "SUNVIEW_HELP",		/* Sunview help */
100041, "PNP",			/* PNP install */
100042, "IPADDR_ALLOC",		/* IP addr allocator */
100043, "FILEHANDLE",		/* Show filehandle */
100044, "MVSNFS",		/* MVS NFS mount  */
100045, "REM_FILEOP_USER",	/* Remote user file operations */
100046, "BATCH_NISUPDATE",	/* Batched ypupdate */
100047, "NEM",			/* Network execution mgr */
100048, "RAYTRACE_RD",		/* Raytrace/mandelbrot remote daemon */
100049, "RAYTRACE_LD",		/* Raytrace/mandelbrot local daemon */
100050, "REM_FILEOP_GROUP",	/* Remote group file operations */
100051, "REM_FILEOP_SYSTEM",	/* Remote system file operations */
100052, "REM_SYSTEM_ROLE",	/* Remote system role operations */
100055, "IOADMD",		/* Ioadmd */
100056, "FILEMERGE",		/* Filemerge */
100057, "NAMEBIND",		/* Name Binding Program */
100058, "NJE",			/* Sunlink NJE */
100059, "MVSATTR",		/* MVSNFS get attribute service */
100060, "RMGR",			/* SunAccess/SunLink resource manager */
100061, "UIDALLOC",		/* UID allocation service */
100062, "LBSERVER",		/* License broker */
100063, "LBBINDER",		/* NETlicense client binder */
100064, "GIDALLOC",		/* GID allocation service */
100065, "SUNISAM",		/* SunIsam */
100066, "RDBSRV",		/* Remote Debug Server */
100067, "NETDIR",		/* Network directory daemon */
100068, "CMSD",			/* Network calendar program */
100069, "NISXFR",		/* NIS transfer */
100070, "TIMED",		/* RPC.timed */
100071, "BUGTRAQ",		/* Bugtraqd */
100072, "NeFS",			/* Internal use only */
100073, "BILLBOARD",		/* Connectathon Billboard - NFS */
100074, "BILLBOARD",		/* Connectathon Billboard - X */
100075, "SCHEDROOM",		/* Sun meeting room scheduler */
100076, "AUTHNEGOTIATE",	/* Authentication negotiation */
100077, "ATTRPROG",		/* Database manipulation */
100078, "KERBPROG",		/* Kerberos authentication daemon */
100080, "AUTODUMP",		/* Sun consulting special */
100081, "EVENT_SVC",		/* Event protocol */
100085,	"ARM_PSD",		/* ARM policy */
100086,	"ARMTOD",		/* ARM TOD */
100087, "NA.ADMIN",		/* Sun (SNAG) administration agent */
100099, "PLD",			/* Genesil 8.1 hot plot */
100101, "NA.EVENT",		/* SNM (SunNet Manager) event dispatcher */
100102, "NA.LOGGER",		/* SNM report logger */
100103, "NA.DISCOVER",		/* SNM network discovery agent */
100104, "NA.SYNC",		/* SNM sync interface agent */
100105, "NA.DISKINFO",		/* SNM disk info agent */
100106, "NA.IOSTAT",		/* SNM iostat agent */
100107, "NA.HOSTPERF",		/* SNM rstat proxy agent */
100108, "NA.CONFIG",		/* SNM host configuration agent */
100109, "NA.ACTIVITY",		/* SNM activity daemon */
100111, "NA.LPSTAT",		/* SNM printer agent */
100112, "NA.HOSTMEM",		/* SNM host network memory agent */
100113, "NA.SAMPLE",		/* SNM sample agent */
100114, "NA.X25",		/* SNM X.25 agent */
100115,	"NA.PING",		/* SNM ping proxy agent */
100116,	"NA.RPCNFS",		/* SNM rpc and nfs agent */
100117,	"NA.HOSTIF",		/* SNM host interface agent */
100118,	"NA.ETHERIF",		/* SNM ethernet interface agent */
100119,	"NA.IPPATH",		/* SNM traceroute proxy agent */
100120,	"NA.IPROUTES",		/* SNM routing table agent */
100121,	"NA.LAYERS",		/* SNM protocol layers gent */
100122,	"NA.SNMP",		/* SNM SNMP proxy agent */
100123,	"NA.TRAFFIC",		/* SNM network traffic agent */
100124, "NA.DNI",		/* DNI (DECnet) proxy agent */
100125, "NA.CHAT",		/* IBM Channel attach proxy agent */
100126, "NA.FDDI",		/* FDDI agent */
100127, "NA.FDDISMT",		/* FDDI SMT proxy agent */
100128, "NA.MHS",		/* MHS agent */
100130, "SNM_GRAPHER",		/* SNM 3D grapher */
100132, "NA.TR",		/* Token Ring agent */
100134, "NA.TOKENRING",		/* Token Ring agent */
100136, "NA.FRAMERELAY",	/* Frame Relay agent */
100175, "NA.SNMPTRAP",		/* SNM SNMP trap daemon */
100180, "NA.MIPROUTES",		/* SNM multicast routing table agent */
100201, "MVSNFSSTAT",		/* MVS/NFS Memory usage statistic server */
100227, "NFS_ACL",		/* NFS ACL support */
100300, "NIS+",			/* NIS+ name service */
100302, "NIS+ CB",		/* NIS+ callbacks */
101002, "NSELINKTOOL",		/* NSE link daemon */
101003, "NSELINKAPP",		/* NSE link application */
110001, "GOLABEL",		/* SunOS MLS  */
110002, "PUC",			/* SunOS MLS  */
150001, "PCNFSD",		/* PC passwd authorization */
150002, "TOPS",			/* TOPS name mapping */
150003, "TOPS",			/* TOPS external attribute storage */
150004, "TOPS",			/* TOPS hierarchical file system */
150005, "TOPS",			/* TOPS NFS transparency extensions */
150006, "SOLARNET_FW",		/* SolarNet Framework protocol */
160001, "CM",			/* Nihon Sun - Japanese Input system */
300004, "FRAME 1",		/* Frame program 1 */
300009, "FRAME 2",		/* Frame program 2 */
390101, "RAP",			/* Legato RAP protocol */
390102, "RAPRD",		/* Legato RAP resource dir protocol */
500021, "ZNS",			/* Zeus Network Service */
};

compare(a, b)
	register struct rpcnames *a, *b;
{
	return (a->rp_prog - b->rp_prog);
}

char *
nameof_prog(prog)
	int prog;
{
	struct rpcnames *r;

	int elems = sizeof (rpcnames) / sizeof (*r);

	r = bsearch(&prog, rpcnames, elems, sizeof (*r),
		(int (*)(const void *, const void *))compare);
	if (r)
		return (r->rp_name);

	if (prog >= 0x40000000 && prog <= 0x5fffffff)
		return ("transient");

	return ("?");
}

char *
nameof_astat(status)
	int status;
{
	switch (status) {
	case SUCCESS	  : return ("Success");
	case PROG_UNAVAIL : return ("Program unavailable");
	case PROG_MISMATCH: return ("Program number mismatch");
	case PROC_UNAVAIL : return ("Procedure unavailable");
	case GARBAGE_ARGS : return ("Garbage arguments");
	case SYSTEM_ERR   : return ("System error");
	default: return ("unknown");
	}
}

/* XXX The following defines should go when <rpc/auth.h> gets updated */
#define	AUTH_KERB_GENERIC	8	/* kerberos generic error */
#define	AUTH_TIMEEXPIRE		9	/* time of credential expired */
#define	AUTH_TKT_FILE		10	/* something wrong with ticket file */
#define	AUTH_DECODE		11	/* can't decode authenticator */
#define	AUTH_NET_ADDR		12	/* net address in ticket wrong */

char *
nameof_why(why)
	int why;
{
	switch (why) {
	case AUTH_BADCRED:	return ("bogus credentials (seal broken)");
	case AUTH_REJECTEDCRED:	return ("client should begin new session");
	case AUTH_BADVERF:	return ("bogus verifier (seal broken)");
	case AUTH_REJECTEDVERF:	return ("verifier expired or was replayed");
	case AUTH_TOOWEAK:	return ("too weak");
	case AUTH_INVALIDRESP:	return ("bogus response verifier");
	case AUTH_KERB_GENERIC:	return ("kerberos generic error");
	case AUTH_TIMEEXPIRE:	return ("time of credential expired");
	case AUTH_TKT_FILE:	return ("something wrong with ticket file");
	case AUTH_DECODE:	return ("can't decode authenticator");
	case AUTH_NET_ADDR:	return ("net address in ticket wrong");
	case AUTH_FAILED:
	default:
		return ("unknown reason");
	}
}

void
rpc_detail_reply(flags, xid, x, data, len)
	int flags, xid;
	struct cache_struct *x;
	char *data;
	int len;
{
	int status;
	int astat, rstat, why;
	int pos;

	if (x) {
		(void) sprintf(get_line(0, 0),
			"This is a reply to frame %d",
			x->xid_frame);
	}
	pos = getxdr_pos();
	status = getxdr_long();
	(void) sprintf(get_line(pos, getxdr_pos()),
		"Status = %d (%s)",
		status, status ? "Denied" : "Accepted");

	switch (status) {
	case MSG_ACCEPTED:
		print_verif(REPLY);
		pos = getxdr_pos();
		astat = getxdr_long();
		(void) sprintf(get_line(pos, getxdr_pos()),
			"Accept status = %d (%s)",
			astat, nameof_astat(astat));

		switch (astat) {
		case SUCCESS:
			if (x) {
				show_trailer();
				protoprint(flags, REPLY, xid,
					x->xid_prog, x->xid_vers, x->xid_proc,
					data, len);
			}
			break;
		case PROG_UNAVAIL :
			break;
		case PROG_MISMATCH:
		case PROC_UNAVAIL :
			showxdr_long("   Low  = %d");
			showxdr_long("   High = %d");
			break;
		case GARBAGE_ARGS:
		case SYSTEM_ERR:
		default:
			;
		}

		break;

	case MSG_DENIED:
		pos = getxdr_pos();
		rstat = getxdr_long();
		(void) sprintf(get_line(pos, getxdr_pos()),
			"Reject status = %d (%s)",
			rstat,
			rstat ? "can't authenticate"
				: "version mismatch");

		switch (rstat) {
		case RPC_MISMATCH:
			showxdr_long("   Low  = %d");
			showxdr_long("   High = %d");
			break;
		case AUTH_ERROR:
			why = getxdr_u_long();
			(void) sprintf(get_line(pos, getxdr_pos()),
				"   Why = %d (%s)",
				why, nameof_why(why));
			break;
		}
		break;
	}
}

/*
 * Return true if this is a valid RPC packet
 */
valid_rpc(rpc, rpclen)
	char *rpc;
	int rpclen;
{
	XDR	xdrm;
	struct rpc_msg msg;

	if (rpclen < 12)
		return (0);

	xdrmem_create(&xdrm, rpc, rpclen, XDR_DECODE);
	if (xdr_u_long(&xdrm, &msg.rm_xid) &&
	    xdr_u_long(&xdrm, (u_long *) &msg.rm_direction)) {
		switch (msg.rm_direction) {
		case CALL:
			if (xdr_u_long(&xdrm, &msg.rm_call.cb_rpcvers) &&
				msg.rm_call.cb_rpcvers == 2)
				return (1);
			break;
		case REPLY:
			if (xdr_u_long(&xdrm,
				(u_long *) &msg.rm_reply.rp_stat) &&
				(msg.rm_reply.rp_stat == MSG_ACCEPTED ||
				msg.rm_reply.rp_stat == MSG_DENIED))
				return (1);
			break;
		}
	}

	return (0);
}

struct cache_struct *xcpfirst	= &xid_cache[0];
struct cache_struct *xcp	= &xid_cache[0];
struct cache_struct *xcplast	= &xid_cache[XID_CACHE_SIZE - 1];

struct cache_struct *
find_xid(xid)
	u_long xid;
{
	struct cache_struct *x;

	for (x = xcp; x >= xcpfirst; x--)
		if (x->xid_num == xid)
			return (x);
	for (x = xcplast; x > xcp; x--)
		if (x->xid_num == xid)
			return (x);
	return (NULL);
}

void
stash_xid(xid, frame, prog, vers, proc)
	u_long xid;
	int frame, prog, vers, proc;
{
	struct cache_struct *x;

	x = find_xid(xid);
	if (x == NULL) {
		x = xcp++;
		if (xcp > xcplast)
			xcp = xcpfirst;
		x->xid_num = xid;
		x->xid_frame = frame;
	}
	x->xid_prog = prog;
	x->xid_vers = vers;
	x->xid_proc = proc;
}

void
check_retransmit(line, xid)
	char *line;
	u_long xid;
{
	struct cache_struct *x;
	extern int pi_frame;

	x = find_xid(xid);
	if (x && x->xid_frame != pi_frame)
		(void) strcat(line, " (retransmit)");
}
