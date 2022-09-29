/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_rport.c,v 1.1 1996/06/19 20:34:14 nn Exp $"

#include <ctype.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include "snoop.h"

#ifndef sgi
#define	NULL 0
#endif

char *show_string();

struct porttable {
	int	pt_num;
	char	*pt_short;
	char	*pt_long;
};

struct porttable pt_udp[] = {
	7,	"ECHO",		"Echo",
	9,	"DISCARD",	"Discard",
	13,	"DAYTIME",	"Daytime",
	19,	"CHARGEN",	"Character generator",
	37,	"TIME",		"Time",
	42,	"NAME",		"Host name server",
	53,	"DNS",		"Domain Name Server",
	67,	"BOOTPS",	"Bootstrap Protocol Server",
	68,	"BOOTPC",	"Boostrap Protocol Client",
	69,	"TFTP",		"Trivial File Transfer Protocol",
	79,	"FINGER",	"Finger",
/*	111,	"PORTMAP",	"Portmapper", Just Sun RPC */
	123,	"NTP",		"Network Time Protocol",
	512,	"BIFF",		"BIFF",
	513,	"WHO",		"WHO",
	514,	"SYSLOG",	"SYSLOG",
	517,	"TALK",		"TALK",
	520,	"RIP",		"Routing Information Protocol",
	550,	"NEW-RWHO",	"NEW-RWHO",
	560,	"RMONITOR",	"RMONITOR",
	561,	"MONITOR",	"MONITOR",
	0, NULL,		"",
};

struct porttable pt_tcp[] = {
	1,	"TCPMUX",	"TCPMUX",
	7,	"ECHO",		"Echo",
	9,	"DISCARD",	"Discard",
	11,	"SYSTAT",	"Active users",
	13,	"DAYTIME",	"Daytime",
	15,	"NETSTAT",	"Who is up",
	19,	"CHARGEN",	"Character generator",
	20,	"FTP-DATA",	"File Transfer Protocol (data)",
	21,	"FTP",		"File Transfer Protocol",
	23,	"TELNET",	"Terminal connection",
	25,	"SMTP",		"Simple Mail Transport Protocol",
	37,	"TIME",		"Time",
	39,	"RLP",		"Resource Location Protocol",
	42,	"NAMESERVER",	"Host Name Server",
	43,	"NICNAME",	"Who is",
	53,	"DNS",		"Domain Name Server",
	67,	"BOOTPS",	"Bootstrap Protocol Server",
	68,	"BOOTPC",	"Bootstrap Protocol Client",
	69,	"TFTP",		"Trivial File Transfer Protocol",
	77,	"RJE",		"RJE service (private)",
	79,	"FINGER",	"Finger",
	87,	"LINK",		"Link",
	95,	"SUPDUP",	"SUPDUP Protocol",
	101,	"HOSTNAME",	"NIC Host Name Server",
	102,	"ISO-TSAP",	"ISO-TSAP",
	103,	"X400",		"X400 Mail service",
	104,	"X400-SND",	"X400 Mail service",
	105,	"CSNET-NS",	"CSNET-NS",
	109,	"POP-2",	"POP-2",
/*	111,	"PORTMAP",	"Portmapper", Just Sun RPC */
	113,	"AUTH",		"Authentication Service",
	117,	"UUCP-PATH",	"UUCP Path Service",
	119,	"NNTP",		"Network News Transfer Protocol",
	123,	"NTP",		"Network Time Protocol",
	144,	"NeWS",		"Network extensible Window System",
	512,	"EXEC",		"EXEC",
	513,	"RLOGIN",	"RLOGIN",
	514,	"RSHELL",	"RSHELL",
	515,	"PRINTER",	"PRINTER",
	530,	"COURIER",	"COURIER",
	540,	"UUCP",		"UUCP",
	600,	"PCSERVER",	"PCSERVER",
	1524,	"INGRESLOCK",	"INGRESLOCK",
	6000,	"XWIN",		"X Window System",
	0, NULL,		"",
};

char *
getportname(proto, port)
	u_short port;
{
	struct porttable *p, *pt;

	switch (proto) {
	case IPPROTO_TCP: pt = pt_tcp; break;
	case IPPROTO_UDP: pt = pt_udp; break;
	default: return (NULL);
	}

	for (p = pt; p->pt_num; p++) {
		if (port == p->pt_num)
			return (p->pt_short);
	}
	return (NULL);
}

int
reservedport(proto, port)
	int proto, port;
{
	struct porttable *p, *pt;

	switch (proto) {
	case IPPROTO_TCP: pt = pt_tcp; break;
	case IPPROTO_UDP: pt = pt_udp; break;
	default: return (NULL);
	}
	for (p = pt; p->pt_num; p++) {
		if (port == p->pt_num)
			return (1);
	}
	return (0);
}

/*
 * Need to be able to register an
 * interpreter for transient ports.
 * See TFTP interpreter.
 */
#define	MAXTRANS 64
struct ttable {
	int t_port;
	int (*t_proc)();
} transients [MAXTRANS];

int
add_transient(port, proc)
	int port;
	int (*proc)();
{
	struct ttable *p;

	for (p = transients; p->t_port > 0; p++)
		;
	p->t_port = port;
	p->t_proc = proc;
	return (1);
}

struct ttable *
is_transient(port)
	int port;
{
	struct ttable *p;

	for (p = transients; p->t_port; p++) {
		if (port == p->t_port)
			return (p);
	}

	return (NULL);
}

void
del_transient(port)
	int port;
{
	struct ttable *p;

	for (p = transients; p->t_port; p++) {
		if (port == p->t_port)
			p->t_port = -1;
	}
}

int src_port, dst_port;

int
interpret_reserved(flags, proto, src, dst, data, dlen)
	int flags, proto, src, dst;
	char *data;
	int dlen;
{
	char *pn;
	int dir, port, which;
	char pbuff[16], hbuff[32];
	struct ttable *ttabp;

	src_port = src;
	dst_port = dst;

	pn = getportname(proto, src);
	if (pn != NULL) {
		dir = 'R';
		port = dst;
		which = src;
	} else {
		pn = getportname(proto, dst);
		if (pn == NULL) {
			ttabp = is_transient(src);
			if (ttabp) {
				(ttabp->t_proc)(flags, data, dlen);
				return (1);
			}
			ttabp = is_transient(dst);
			if (ttabp) {
				(ttabp->t_proc)(flags, data, dlen);
				return (1);
			}
			return (0);
		}

		dir = 'C';
		port = src;
		which = dst;
	}

	if (dlen > 0) {
		switch (which) {
		case  67:
		case  68:
			interpret_dhcp(flags, data, dlen);
			return (1);
		case  69:
			interpret_tftp(flags, data, dlen);
			return (1);
		case 123:
			interpret_ntp(flags, data, dlen);
			return (1);
		case 520:
			interpret_rip(flags, data, dlen);
			return (1);
		}
	}

	if (flags & F_SUM) {
		(void) sprintf(get_sum_line(),
			"%s %c port=%d %s",
			pn, dir, port,
			show_string(data, dlen, 20));
	}

	if (flags & F_DTAIL) {
		(void) sprintf(pbuff, "%s:  ", pn);
		(void) sprintf(hbuff, "%s:  ", pn);
		show_header(pbuff, hbuff, dlen);
		show_space();
		(void) sprintf(get_line(0, 0),
			"\"%s\"",
			show_string(data, dlen, 60));
		show_trailer();
	}
	return (1);
}

char *
show_string(str, dlen, maxlen)
	char *str;
	int dlen, maxlen;
/*
 *   Prints len bytes from str enclosed in quotes.
 *   If len is negative, length is taken from strlen(str).
 *   No more than maxlen bytes will be printed.  Longer
 *   strings are flagged with ".." after the closing quote.
 *   Non-printing characters are converted to C-style escape
 *   codes or octal digits.
 */
{
	static char tbuff[128];
	char *p, *pp;
	int printable = 0;
	int c, len;

	len = dlen > maxlen ? maxlen : dlen;
	dlen = len;

	for (p = str, pp = tbuff; len; p++, len--) {
		switch (c = *p & 0xFF) {
		case '\n': (void) strcpy(pp, "\\n"); pp += 2; break;
		case '\b': (void) strcpy(pp, "\\b"); pp += 2; break;
		case '\t': (void) strcpy(pp, "\\t"); pp += 2; break;
		case '\r': (void) strcpy(pp, "\\r"); pp += 2; break;
		case '\f': (void) strcpy(pp, "\\f"); pp += 2; break;
		default:
			if (isascii(c) && isprint(c)) {
				*pp++ = c;
				printable++;
			} else {
				(void) sprintf(pp,
					isdigit(*(p + 1)) ?
					"\\%03o" : "\\%o", c);
				pp += strlen(pp);
			}
			break;
		}
		*pp = '\0';
	}
	return (printable > dlen / 2 ? tbuff : "");
}
