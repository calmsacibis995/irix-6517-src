/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_nis.c,v 1.2 1996/07/06 17:51:30 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/tiuser.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpcsvc/yp_prot.h>
#include "snoop.h"

extern char *dlc_header;
extern jmp_buf xdr_err;
char *ypbind_error();
char *sum_ypxfrstat();
char *sum_ypmaplist();
void detail_ypmaplist();
void niscall(int proc);
void nisreply(int proc);

/*
 * Defines missing from 5.0 yp_prot.h
 */
#define	YPBINDPROG		((u_long)100007)
#define	YPBINDVERS		((u_long)2)
#define	YPBINDVERS_ORIG		((u_long)1)

/* Procedure symbols */

#define	YPBINDPROC_NULL		((u_long)0)
#define	YPBINDPROC_DOMAIN	((u_long)1)
#define	YPBINDPROC_SETDOM	((u_long)2)

#define	YPBIND_ERR_ERR 1		/* Internal error */
#define	YPBIND_ERR_NOSERV 2		/* No bound server for passed domain */
#define	YPBIND_ERR_RESC 3		/* System resource allocation failure */


static char *procnames_bind_short[] = {
	"NULL",			/*  0 */
	"DOMAIN",		/*  1 */
	"SETDOMAIN",		/*  2 */
};

static char *procnames_bind_long[] = {
	"Null procedure",		/*  0 */
	"Get domain name",		/*  1 */
	"Set domain name",		/*  2 */
};

static char *procnames_short[] = {
	"NULL",			/*  0 */
	"DOMAIN",		/*  1 */
	"DOMAIN_NONACK",	/*  2 */
	"MATCH",		/*  3 */
	"FIRST",		/*  4 */
	"NEXT",			/*  5 */
	"XFR",			/*  6 */
	"CLEAR",		/*  7 */
	"ALL",			/*  8 */
	"MASTER",		/*  9 */
	"ORDER",		/* 10 */
	"MAPLIST",		/* 11 */
};

#define	MAXPROC	2

static char *procnames_long[] = {
	"Null procedure",			/*  0 */
	"Verify domain support",		/*  1 */
	"Verify domain support (broadcast)",	/*  2 */
	"Return value of a key",		/*  3 */
	"Return first key-value pair in map",	/*  4 */
	"Return next key-value pair in map",	/*  5 */
	"Request map update",			/*  6 */
	"Close current map on server",		/*  7 */
	"Get all key-value pairs in map",	/*  8 */
	"Get master server",			/*  9 */
	"Get order",				/* 10 */
	"Return list of supported maps",	/* 11 */
};

void
interpret_nisbind(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[1024];
	unsigned int status;

	if (proc < 0 || proc > MAXPROC)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NISBIND C %s",
				procnames_bind_short[proc]);
			line += strlen(line);
			switch (proc) {
			case YPBINDPROC_NULL:
				break;
			case YPBINDPROC_DOMAIN:
				(void) sprintf(line, " %s",
					getxdr_string(buff, 1024));
				break;
			case YPBINDPROC_SETDOM:
				(void) sprintf(line, " %s",
					getxdr_string(buff, 1024));
				break;
			default:
				break;
			}
			check_retransmit(line, xid);
		} else {
			(void) sprintf(line, "NISBIND R %s ",
				procnames_bind_short[proc]);
			line += strlen(line);
			switch (proc) {
			case YPBINDPROC_NULL:
				break;
			case YPBINDPROC_DOMAIN:
				status = getxdr_long();
				if (status == 1) {	/* success */
					(void) strcat(line, "OK");
				} else {		/* failure */
					status = getxdr_long();
					(void) sprintf(line, "ERROR=%s",
						ypbind_error(status));
				}
				break;
			case YPBINDPROC_SETDOM:
				break;
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NISBIND:",
			"Network Information Service Bind", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_bind_long[proc]);
		if (type == CALL) {
			switch (proc) {
			case YPBINDPROC_NULL:
				break;
			case YPBINDPROC_DOMAIN:
				(void) showxdr_string(YPMAXDOMAIN,
					"Domain = %s");
				break;
			case YPBINDPROC_SETDOM:
				(void) showxdr_string(YPMAXDOMAIN,
					"Domain = %s");
				(void) showxdr_hex(4, "Address=%s");
				(void) showxdr_hex(2, "Port=%s");
				(void) showxdr_u_long("Version=%lu");
				break;
			default:
				break;
			}
		} else {
			switch (proc) {
			case YPBINDPROC_NULL:
				break;
			case YPBINDPROC_DOMAIN:
				status = getxdr_u_long();
				(void) sprintf(get_line(0, 0),
					"Status = %lu (%s)",
					status,
					status == 1 ? "OK":"Fail");
				if (status == 1) {
					(void) showxdr_hex(4,
						"Address=%s");
					(void) showxdr_hex(2,
						"Port=%s");
				} else {
					status = getxdr_u_long();
					(void) sprintf(get_line(0, 0),
						"Error = %lu (%s)",
						status,
						ypbind_error(status));
				}
				break;
			case YPBINDPROC_SETDOM:
				break;
			default:
				break;
			}
		}
		show_trailer();
	}
}

void
interpret_nis(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff1[1024], buff2[1024], buff3[1024];
	char *dom, *map, *key;
	int transid, status;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"NIS C %s",
				procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case YPPROC_NULL:
				break;
			case YPPROC_DOMAIN:
			case YPPROC_DOMAIN_NONACK:
			case YPPROC_MAPLIST:
				(void) sprintf(line, " %s",
					getxdr_string(buff1, 1024));
				break;
			case YPPROC_MATCH:
			case YPPROC_FIRST:
			case YPPROC_NEXT:
				dom = getxdr_string(buff1, 1024);
				map = getxdr_string(buff2, 1024);
				key = getxdr_string(buff3, 1024);
				(void) sprintf(line,
					" %s in %s",
					key, map);
				break;
			case YPPROC_XFR:
				dom = getxdr_string(buff1, 1024);
				map = getxdr_string(buff2, 1024);
				(void) sprintf(line,
					" map %s in %s",
					map, dom);
				break;
			case YPPROC_CLEAR:
				break;
			case YPPROC_ALL:
			case YPPROC_MASTER:
			case YPPROC_ORDER:
				dom = getxdr_string(buff1, 1024);
				map = getxdr_string(buff2, 1024);
				(void) sprintf(line,
					" map %s in %s",
					map, dom);
				break;
			default:
				break;
			}
			check_retransmit(line, xid);
		} else {
			(void) sprintf(line, "NIS R %s ",
				procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case YPPROC_NULL:
				break;
			case YPPROC_DOMAIN:
			case YPPROC_DOMAIN_NONACK:
				(void) sprintf(line, "%s",
					getxdr_long() ? "OK":"Fail");
				break;
			case YPPROC_MATCH:
				(void) sum_ypstat(line);
				break;
			case YPPROC_FIRST:
			case YPPROC_NEXT:
				if (sum_ypstat(line) == YP_TRUE) {
					line += strlen(line);
					(void) getxdr_string(buff1,
							YPMAXRECORD);
					(void) sprintf(line, " key=%s",
						getxdr_string(buff1, 1024));
				}
				break;
			case YPPROC_XFR:
				transid = getxdr_u_long();
				status  = getxdr_long();
				(void) sprintf(line, "transid=%lu %s",
					transid,
					sum_ypxfrstat(status));
				break;
			case YPPROC_CLEAR:
				break;
			case YPPROC_ALL:
				if (getxdr_u_long()) {
					(void) sum_ypstat(line);
					line += strlen(line);
					(void) sprintf(line, " key=%s",
					    getxdr_string(buff1, 1024));
				} else {
					(void) sprintf(line,
						"No more");
				}
				break;
			case YPPROC_MASTER:
				if (sum_ypstat(line) == YP_TRUE) {
					line += strlen(line);
					(void) sprintf(line, " peer=%s",
						getxdr_string(buff1, 1024));
				}
				break;
			case YPPROC_ORDER:
				if (sum_ypstat(line) == YP_TRUE) {
					line += strlen(line);
					(void) sprintf(line, " order=%lu",
						getxdr_u_long());
				}
				break;
			case YPPROC_MAPLIST:
				if (sum_ypstat(line) == YP_TRUE) {
					line += strlen(line);
					(void) sprintf(line, " %s",
						sum_ypmaplist());
				}
				break;
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NIS:  ", "Network Information Service", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long[proc]);
		if (type == CALL)
			niscall(proc);
		else
			nisreply(proc);
		show_trailer();
	}
}

/*
 *  Print out version 2 NIS call packets
 */
void
niscall(proc)
	int proc;
{
	switch (proc) {
	case YPPROC_NULL:
		break;
	case YPPROC_DOMAIN:
	case YPPROC_DOMAIN_NONACK:
	case YPPROC_MAPLIST:
		(void) showxdr_string(YPMAXDOMAIN, "Domain = %s");
		break;
	case YPPROC_MATCH:
	case YPPROC_FIRST:
	case YPPROC_NEXT:
		(void) showxdr_string(YPMAXDOMAIN, "Domain = %s");
		(void) showxdr_string(YPMAXMAP, "Map = %s");
		(void) showxdr_string(YPMAXRECORD, "Key = %s");
		break;
	case YPPROC_XFR:
		(void) showxdr_string(YPMAXDOMAIN, "Domain = %s");
		(void) showxdr_string(YPMAXMAP, "Map = %s");
		(void) showxdr_u_long("Order = %lu");
		(void) showxdr_string(YPMAXPEER, "Peer = %s");
		(void) showxdr_u_long("Transid = %lu");
		(void) showxdr_u_long("Prog = %lu");
		(void) showxdr_u_long("Port = %lu");
		break;
	case YPPROC_CLEAR:
		break;
	case YPPROC_ALL:
	case YPPROC_MASTER:
	case YPPROC_ORDER:
		(void) showxdr_string(YPMAXDOMAIN, "Domain = %s");
		(void) showxdr_string(YPMAXMAP, "Map = %s");
		break;
	default:
		break;
	}
}

/*
 *  Print out version 2 NIS reply packets
 */
void
nisreply(proc)
	int proc;
{
	unsigned int xfrstat, more;

	switch (proc) {
	case YPPROC_NULL:
		break;
	case YPPROC_DOMAIN:
	case YPPROC_DOMAIN_NONACK:
		(void) sprintf(get_line(0, 0),
			"Result=%s",
			getxdr_u_long() ? "OK":"Fail");
		break;
	case YPPROC_MATCH:
		(void) detail_ypstat();
		(void) showxdr_string(YPMAXRECORD, "Value = %s");
		break;
	case YPPROC_FIRST:
	case YPPROC_NEXT:
		(void) detail_ypstat();
		(void) showxdr_string(YPMAXRECORD, "Value = %s");
		(void) showxdr_string(YPMAXRECORD, "Key = %s");
		break;
	case YPPROC_XFR:
		(void) showxdr_u_long("Transid = %lu");
		xfrstat = getxdr_u_long();
		(void) sprintf(get_line(0, 0),
			"Transfer status = %lu (%s)",
			xfrstat, sum_ypxfrstat(xfrstat));
		break;
	case YPPROC_CLEAR:
		break;
	case YPPROC_ALL:
		more = getxdr_u_long();
		(void) sprintf(get_line(0, 0),
			"More = %s",
			more ? "true" : "false");
		if (more) {
			(void) detail_ypstat();
			(void) showxdr_string(YPMAXRECORD, "Key = %s");
			(void) showxdr_string(60, "Value = %s");
		}
		break;
	case YPPROC_MASTER:
		(void) detail_ypstat();
		(void) showxdr_string(YPMAXPEER, "Peer = %s");
	case YPPROC_ORDER:
		(void) detail_ypstat();
		(void) showxdr_u_long("Order=%lu");
		break;
	case YPPROC_MAPLIST:
		(void) detail_ypstat();
		detail_ypmaplist();
		break;
	default:
		break;
	}
}

char *
sum_ypxfrstat(status)
	int status;
{
	static char buff [16];

	switch (status) {
	case   1:	return ("Success");
	case   2:	return ("Master's version not newer");
	case  -1:	return ("Can't find server for map");
	case  -2:	return ("No such domain");
	case  -3:	return ("Resource allocation failure");
	case  -4:	return ("RPC failure talking to server");
	case  -5:	return ("Can't get master address");
	case  -6:	return ("NIS server/map db error");
	case  -7:	return ("Bad arguments");
	case  -8:	return ("Local dbm operation failed");
	case  -9:	return ("Local file I/O operation failed");
	case -10:	return ("Map version skew during transfer");
	case -11:	return ("Can't send clear req to local ypserv");
	case -12:	return ("No local order number in map");
	case -13:	return ("Transfer error");
	case -14:	return ("Transfer request refused");
	default:
		(void) sprintf(buff, "(%d)", status);
		return (buff);
	}
	/* NOTREACHED */
}

int
sum_ypstat(line)
	char *line;
{
	u_long status;
	char *str;
	char buff[16];

	status = getxdr_u_long();
	switch (status) {
	case YP_TRUE:	str = "OK";			break;
	case YP_NOMORE:	str = "No more entries";	break;
	case YP_FALSE:	str = "Fail";			break;
	case YP_NOMAP:	str = "No such map";		break;
	case YP_NODOM:	str = "No such domain";		break;
	case YP_NOKEY:	str = "No such key";		break;
	case YP_BADOP:	str = "Invalid operation";	break;
	case YP_BADDB:	str = "Bad database";		break;
	case YP_YPERR:	str = "Server error";		break;
	case YP_BADARGS:str = "Bad args";		break;
	case YP_VERS:	str = "Version mismatch";	break;
	default:	(void) sprintf(buff, "(%lu)", status);
			str = buff;
			break;
	}
	(void) strcpy(line, str);
	return ((int) status);
}

int
detail_ypstat()
{
	u_long status;
	char buff[32];


	status = sum_ypstat(buff);
	(void) sprintf(get_line(0, 0),
		"Status = %d (%s)",
		status, buff);

	return ((int) status);
}

char *
sum_ypmaplist()
{
	static char buff[1024];
	int maps = 0;

	if (setjmp(xdr_err)) {
		(void) sprintf(buff, "%d+ maps", maps);
		return (buff);
	}

	while (getxdr_long()) {
		(void) getxdr_string(buff, 1024);
		maps++;
	}

	(void) sprintf(buff, "%d maps", maps);
	return (buff);
}

void
detail_ypmaplist()
{
	int maps = 0;

	if (setjmp(xdr_err)) {
		(void) sprintf(get_line(0, 0),
			" %d+ maps. (Frame is incomplete)",
			maps);
		return;
	}

	(void) sprintf(get_line(0, 0), "Map list");

	while (getxdr_long()) {
		(void) showxdr_string(YPMAXMAP, "  %s");
		maps++;
	}

	(void) sprintf(get_line(0, 0), "%d maps", maps);
}

char *
ypbind_error(err)
	int err;
{
	static char buff[16];

	switch (err) {
	case YPBIND_ERR_ERR:	return ("Internal error");
	case YPBIND_ERR_NOSERV:	return ("Internal error");
	case YPBIND_ERR_RESC:	return ("Resource allocation fail");
	default:
		(void) sprintf(buff, "(%d)", err);
		return (buff);
	}
	/* NOTREACHED */
}
