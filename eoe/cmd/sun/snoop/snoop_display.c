/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_display.c,v 1.2 1998/04/03 19:37:18 sca Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#ifndef sgi
#include <sys/bufmod.h>
#endif
#include <setjmp.h>
#include <varargs.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/dlpi.h>

#include "snoop.h"

char *dlc_header;
char *src_name, *dst_name;
int pi_frame;
int pi_time_hour;
int pi_time_min;
int pi_time_sec;
int pi_time_usec;

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static void hexdump(char *, int);

/*
 * This routine invokes the packet interpreters
 * on a packet.  There's some messing around
 * setting up a few packet-externals before
 * starting with the ethernet interpreter.
 * Yes, we assume here that all packets will
 * be ethernet packets.
 */
void
process_pkt(hdrp, pktp, num, flags)
	struct sb_hdr *hdrp;
	char *pktp;
	int num, flags;
{
	int drops, pktlen;
	struct timeval *tvp;
	struct tm *tm;
	extern int x_offset;
	extern int x_length;
	int offset, length;
	static struct timeval ptv;

	if (hdrp == NULL)
		return;

	tvp    = &hdrp->sbh_timestamp;
	if (ptv.tv_sec == 0)
		ptv = *tvp;
	drops  = hdrp->sbh_drops;
	pktlen = hdrp->sbh_msglen;
	if (pktlen <= 0)
		return;

	/* set up externals */
	dlc_header = pktp;
	pi_frame = num;
	tm = localtime(&tvp->tv_sec);
	pi_time_hour = tm->tm_hour;
	pi_time_min  = tm->tm_min;
	pi_time_sec  = tm->tm_sec;
	pi_time_usec = tvp->tv_usec;

	src_name = "?";
	dst_name = "*";

	click(hdrp->sbh_origlen);

	(*interface->interpreter)(flags, dlc_header, hdrp->sbh_msglen,
							hdrp->sbh_origlen);

	show_pktinfo(flags, num, src_name, dst_name, &ptv, tvp, drops,
							hdrp->sbh_origlen);

	if (x_offset >= 0) {
		offset = MIN(x_offset, hdrp->sbh_msglen);
		offset -= (offset % 2);  /* round down */
		length = MIN(hdrp->sbh_msglen - offset, x_length);

		hexdump(dlc_header + offset, length);
	}

	ptv = *tvp;
}


/*
 * *************************************************************
 * The following routines constitute a library
 * used by the packet interpreters to facilitate
 * the display of packet data.  This library
 * of routines helps provide a consistent
 * "look and feel".
 */


/*
 * Display the value of a flag bit in
 * a byte together with some text that
 * corresponds to its value - whether
 * true or false.
 */
char *
getflag(val, mask, s_true, s_false)
	int val, mask;
	char *s_true, *s_false;
{
	static char buff[80];
	char *p;
	int set;

	(void) strcpy(buff, ".... .... = ");
	if (s_false == NULL)
		s_false = s_true;

	for (p = &buff[8]; p >= buff; p--) {
		if (*p == ' ')
			p--;
		if (mask & 0x1) {
			set = val & mask & 0x1;
			*p = set ? '1':'0';
			(void) strcat(buff, set ? s_true: s_false);
			break;
		}
		mask >>= 1;
		val  >>= 1;
	}
	return (buff);
}

XDR xdrm;
jmp_buf xdr_err;
int xdr_totlen;
char *prot_prefix;
char *prot_nest_prefix = "";
char *prot_title;

void
show_header(pref, str, len)
	char *pref, *str;
	int len;
{
	prot_prefix = pref;
	prot_title = str;
	(void) sprintf(get_detail_line(0, len),
		"%s%s----- %s -----",
		prot_nest_prefix, pref, str);
}

void
xdr_init(addr, len)
	char *addr;
	int len;
{
	xdr_totlen = len;
	xdrmem_create(&xdrm, addr, len, XDR_DECODE);
}

char *
get_line(begin, end)
	int begin, end;
{
	char *line;

	line = get_detail_line(begin, end);
	(void) strcpy(line, prot_nest_prefix);
	(void) strcat(line, prot_prefix);
	return (line + strlen(line));
}

void
show_line(str)
	char *str;
{
	(void) strcpy(get_line(0, 0), str);
}

char
getxdr_char()
{
	char s;

	if (xdr_char(&xdrm, &s))
		return (s);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

char
showxdr_char(fmt)
	char *fmt;
{
	int pos; char val;

	pos = getxdr_pos();
	val = getxdr_char();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

u_char
getxdr_u_char()
{
	u_char s;

	if (xdr_u_char(&xdrm, &s))
		return (s);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

u_char
showxdr_u_char(fmt)
	char *fmt;
{
	int pos; u_char val;

	pos = getxdr_pos();
	val = getxdr_u_char();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

short
getxdr_short()
{
	short s;

	if (xdr_short(&xdrm, &s))
		return (s);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

short
showxdr_short(fmt)
	char *fmt;
{
	int pos; short val;

	pos = getxdr_pos();
	val = getxdr_short();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

u_short
getxdr_u_short()
{
	u_short s;

	if (xdr_u_short(&xdrm, &s))
		return (s);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

u_short
showxdr_u_short(fmt)
	char *fmt;
{
	int pos; u_short val;

	pos = getxdr_pos();
	val = getxdr_u_short();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

long
getxdr_long()
{
	long l;

	if (xdr_long(&xdrm, &l))
		return (l);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

long
showxdr_long(fmt)
	char *fmt;
{
	int pos; long val;

	pos = getxdr_pos();
	val = getxdr_long();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

u_long
getxdr_u_long()
{
	u_long l;

	if (xdr_u_long(&xdrm, &l))
		return (l);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

u_long
showxdr_u_long(fmt)
	char *fmt;
{
	int pos; u_long val;

	pos = getxdr_pos();
	val = getxdr_u_long();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

#ifndef sgi
/*
 * Not included in the IRIX 6.2 xdr_simple(3N) routines but that's ok
 * because they're not even used by snoop..
 */
longlong_t
getxdr_longlong()
{
	longlong_t l;

	if (xdr_longlong_t(&xdrm, &l))
		return (l);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

longlong_t
showxdr_longlong(fmt)
	char *fmt;
{
	int pos; longlong_t val;

	pos = getxdr_pos();
	val = getxdr_longlong();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}
#endif

u_longlong_t
getxdr_u_longlong()
{
	u_longlong_t l;

	if (xdr_u_longlong_t(&xdrm, &l))
		return (l);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

u_longlong_t
showxdr_u_longlong(fmt)
	char *fmt;
{
	int pos; u_longlong_t val;

	pos = getxdr_pos();
	val = getxdr_u_longlong();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, val);
	return (val);
}

bool_t
getxdr_bool()
{
	bool_t b;

	if (xdr_bool(&xdrm, &b))
		return (b);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

bool_t
showxdr_bool(fmt)
	char *fmt;
{
	int pos; bool_t val;

	pos = getxdr_pos();
	val = getxdr_bool();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt,
		val ? "True" : "False");
	return (val);
}

char *
getxdr_opaque(p, len)
	char *p; int len;
{
	if (xdr_opaque(&xdrm, p, len))
		return (p);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

char *
getxdr_string(p, len)
	char *p; int len;
{
	if (xdr_string(&xdrm, &p, len))
		return (p);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

char *
showxdr_string(len, fmt)
	int len; char *fmt;
{
	static char buff[1024];
	int pos;

	pos = getxdr_pos();
	getxdr_string(buff, len);
	(void) strcpy(buff+60, "...");
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, buff);
	return (buff);
}

char *
getxdr_bytes(lenp)
	u_int *lenp;
{
	static char buff[1024];
	char *p = buff;

	if (xdr_bytes(&xdrm, &p, lenp, 1024))
		return (buff);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

char *
getxdr_context(
	char *p,
	int len)
{
	u_short size;

	size = getxdr_u_short();
	if (((int)size > 0) && ((int)size < len) && getxdr_opaque(p, size))
		return (p);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

char *
showxdr_context(
	char *fmt)
{
	u_short size;
	static char buff[1024];
	int pos;

	pos = getxdr_pos();
	size = getxdr_u_short();
	if (((int)size > 0) && ((int)size < 1024) &&
	    getxdr_opaque(buff, size)) {
		(void) sprintf(get_line(pos, getxdr_pos()), fmt, buff);
		return (buff);
	}
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

enum_t
getxdr_enum()
{
	enum_t e;

	if (xdr_enum(&xdrm, &e))
		return (e);
	longjmp(xdr_err, 1);
	/* NOTREACHED */
}

void
xdr_skip(delta)
	int delta;
{
	if (delta % 4 != 0)
		longjmp(xdr_err, 1);
	xdr_setpos(&xdrm, xdr_getpos(&xdrm) + delta);
}

int
getxdr_pos()
{
	return (xdr_getpos(&xdrm));
}

void
setxdr_pos(pos)
	int pos;
{
	xdr_setpos(&xdrm, pos);
}

void
show_space()
{
	(void) get_line(0, 0);
}

void
show_trailer()
{
	show_space();
}

char *
getxdr_date()
{
	long sec, usec;
	static char buff[64];
	char *p;
	struct tm my_time;	/* private buffer to avoid collision */
				/* between gmtime and strftime */
	struct tm *tmp;

	sec  = getxdr_long();
	usec = getxdr_long();
	if (sec == -1)
		return ("-1 ");

	if (sec < 3600 * 24 * 365) {	/* assume not a date */
		(void) sprintf(buff, "%d.%06d", sec, usec);
	} else {
		tmp = gmtime(&sec);
		memcpy(&my_time, tmp, sizeof (struct tm));
		strftime(buff, sizeof (buff), "%d-%h-%y %T.", &my_time);
		p = buff + strlen(buff);
		(void) sprintf(p, "%06d GMT", usec);
	}
	return (buff);
}

char *
showxdr_date(fmt)
	char *fmt;
{
	int pos;
	char *p;

	pos = getxdr_pos();
	p = getxdr_date();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, p);
	return (p);
}

char *
getxdr_date_ns()
{
	long sec, nsec;
	static char buff[64];
	char *p;
	struct tm my_time;	/* private buffer to avoid collision */
				/* between gmtime and strftime */
	struct tm *tmp;

	sec  = getxdr_long();
	nsec = getxdr_long();
	if (sec == -1)
		return ("-1 ");

	if (sec < 3600 * 24 * 365) {	/* assume not a date */
		(void) sprintf(buff, "%d.%06d", sec, nsec);
	} else {
		tmp = gmtime(&sec);
		memcpy(&my_time, tmp, sizeof (struct tm));
		strftime(buff, sizeof (buff), "%d-%h-%y %T.", &my_time);
		p = buff + strlen(buff);
		(void) sprintf(p, "%09d GMT", nsec);
	}
	return (buff);
}

char *
showxdr_date_ns(fmt)
	char *fmt;
{
	int pos;
	char *p;

	pos = getxdr_pos();
	p = getxdr_date_ns();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, p);
	return (p);
}

char *
getxdr_time()
{
	long sec;
	static char buff[64];
	struct tm my_time;	/* private buffer to avoid collision */
				/* between gmtime and strftime */
	struct tm *tmp;

	sec  = getxdr_long();
	if (sec == -1)
		return ("-1 ");

	if (sec < 3600 * 24 * 365) {	/* assume not a date */
		(void) sprintf(buff, "%d", sec);
	} else {
		tmp = gmtime(&sec);
		memcpy(&my_time, tmp, sizeof (struct tm));
		strftime(buff, sizeof (buff), "%d-%h-%y %T", &my_time);
	}
	return (buff);
}

char *
showxdr_time(fmt)
	char *fmt;
{
	int pos;
	char *p;

	pos = getxdr_pos();
	p = getxdr_time();
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, p);
	return (p);
}

char *
getxdr_hex(len)
	int len;
{
	int i, j;
	static char hbuff[1024];
	char rbuff[1024];
	static char *hexstr = "0123456789ABCDEF";

	len = len < 1024 ? len : 1024;

	if (xdr_opaque(&xdrm, rbuff, len) == 0) {
		longjmp(xdr_err, 1);
	}
	j = 0;
	for (i = 0; i < len; i++) {
		hbuff[j++] = hexstr[rbuff[i] >> 4 & 0x0f];
		hbuff[j++] = hexstr[rbuff[i] & 0x0f];
	}
	hbuff[j] = '\0';

	return (hbuff);
}

char *
showxdr_hex(len, fmt)
	int len; char *fmt;
{
	int pos;
	char *p;

	pos = getxdr_pos();
	p = getxdr_hex(len);
	(void) sprintf(get_line(pos, getxdr_pos()), fmt, p);
	return (p);
}

static void
hexdump(data, datalen)
	char *data;
	int datalen;
{
	char *p;
	u_short *p16 = (u_short *) data;
	char *p8 = data;
	int i, left, len;
	int chunk = 16;  /* 16 bytes per line */

	printf("\n");

	for (p = data; p < data + datalen; p += chunk) {
		printf("\t%4d: ", p - data);
		left = (data + datalen) - p;
		len = MIN(chunk, left);
		for (i = 0; i < (len / 2) + (len % 2); i++)
			printf("%04x ", ntohs(*p16++) & 0xffff);
		for (i = 0; i < (chunk - left) / 2; i++)
			printf("     ");

		printf("   ");
		for (i = 0; i < len; i++, p8++)
			printf("%c", isprint(*p8) ? *p8 : '.');
		printf("\n");
	}

	printf("\n");
}
