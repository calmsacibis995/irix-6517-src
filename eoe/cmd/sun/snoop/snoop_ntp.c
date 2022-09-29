/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_ntp.c,v 1.3 1998/11/25 20:09:56 jlan Exp $"

#include <fcntl.h>
#include <netinet/in.h>
#include "snoop.h"
#include "ntp.h"
#include <stdio.h>

extern char *dlc_header;
extern char *addrtoname();

char *show_leap();
char *show_mode();
char *show_ref();
char *show_time();
double s_fixed_to_double();

interpret_ntp(flags, ntp, fraglen)
	int flags;
	struct ntpdata *ntp;
	int fraglen;
{
	long net_time;

	if (flags & F_SUM) {
		net_time = ntohl(ntp->xmt.int_part) - JAN_1970;
		(void) sprintf(get_sum_line(),
			"NTP  %s (%s)",
			show_mode(ntp->status & NTPMODEMASK),
			show_time(net_time));
	}

	if (flags & F_DTAIL) {

	show_header("NTP:  ", "Network Time Protocol", fraglen);
	show_space();
	(void) sprintf(get_line((char *)ntp->status - dlc_header, 1),
		"Leap = 0x%x (%s)",
		(int) (ntp->status & LEAPMASK) >> 6,
		show_leap(ntp->status & LEAPMASK));
	(void) sprintf(get_line((char *)ntp->status - dlc_header, 1),
		"Version = %d",
		(int) (ntp->status & VERSIONMASK) >> 3);
	(void) sprintf(get_line((char *)ntp->status - dlc_header, 1),
		"Mode    = %d (%s)",
		ntp->status & NTPMODEMASK,
		show_mode(ntp->status & NTPMODEMASK));
	(void) sprintf(get_line((char *)ntp->stratum - dlc_header, 1),
		"Stratum = %d (%s)",
		ntp->stratum,
		ntp->stratum == 0 ? "unspecified" :
		ntp->stratum == 1 ? "primary reference" :
				    "secondary reference");
	(void) sprintf(get_line((char *)ntp->ppoll - dlc_header, 1),
		"Poll = %d",
		ntp->ppoll);
	(void) sprintf(get_line((char *)ntp->precision - dlc_header, 1),
		"Precision = %d seconds",
		ntp->precision);
	(void) sprintf(get_line((char *)ntp->distance.int_part - dlc_header, 1),
		"Synchronizing distance   = 0x%04x.%04x  (%f)",
		ntohs(ntp->distance.int_part),
		ntohs(ntp->distance.fraction),
		s_fixed_to_double(&ntp->distance));
	(void) sprintf(get_line((char *)ntp->dispersion.int_part - dlc_header,
			1),
		"Synchronizing dispersion = 0x%04x.%04x  (%f)",
		ntohs(ntp->dispersion.int_part),
		ntohs(ntp->dispersion.fraction),
		s_fixed_to_double(&ntp->dispersion));
	(void) sprintf(get_line((char *)ntp->refid - dlc_header, 1),
		"Reference clock = %s",
		show_ref(ntp->stratum, ntp->refid));

	net_time = ntohl(ntp->reftime.int_part) - JAN_1970;
	(void) sprintf(get_line((char *)ntp->reftime.int_part - dlc_header, 1),
		"Reference time = 0x%08lx.%08lx (%s)",
		ntohl(ntp->reftime.int_part),
		ntohl(ntp->reftime.fraction),
		show_time(net_time));

	net_time = ntohl(ntp->org.int_part) - JAN_1970;
	(void) sprintf(get_line((char *)ntp->org.int_part - dlc_header, 1),
		"Originate time = 0x%08lx.%08lx (%s)",
		ntohl(ntp->org.int_part),
		ntohl(ntp->org.fraction),
		show_time(net_time));

	net_time = ntohl(ntp->rec.int_part) - JAN_1970;
	(void) sprintf(get_line((char *)ntp->rec.int_part - dlc_header, 1),
		"Receive   time = 0x%08lx.%08lx (%s)",
		ntohl(ntp->rec.int_part),
		ntohl(ntp->rec.fraction),
		show_time(net_time));

	net_time = ntohl(ntp->xmt.int_part) - JAN_1970;
	(void) sprintf(get_line((char *)ntp->xmt.int_part - dlc_header, 1),
		"Transmit  time = 0x%08lx.%08lx (%s)",
		ntohl(ntp->xmt.int_part),
		ntohl(ntp->xmt.fraction),
		show_time(net_time));
	}

	return (fraglen);
}

char *
show_leap(leap)
	int leap;
{
	switch (leap) {
	case NO_WARNING: return ("OK");
	case PLUS_SEC:	return ("add a second (61 seconds)");
	case MINUS_SEC:	return ("minus a second (59 seconds)");
	case ALARM:	return ("alarm condition (clock unsynchronized)");
	}
	return ("?");
}

char *
show_mode(mode)
	int mode;
{
	switch (mode) {
	case MODE_UNSPEC:	return ("unspecified");
	case MODE_SYM_ACT:	return ("symmetric active");
	case MODE_SYM_PAS:	return ("symmetric passive");
	case MODE_CLIENT:	return ("client");
	case MODE_SERVER:	return ("server");
	case MODE_BROADCAST:	return ("broadcast");
	case MODE_RES1:		return ("");
	case MODE_RES2:		return ("");
	}
	return ("?");
}

char *
show_ref(mode, refid)
	int mode;
	u_long refid;
{
	static char buff[128];
	struct in_addr host;
	extern char *inet_ntoa();

	switch (mode) {
	case 0:
	case 1:
		(void) strncpy(buff, (char *)&refid, 4);
		buff[4] = '\0';
		break;

	default:
		host.s_addr = refid;
		(void) sprintf(buff, "%s (%s)",
			inet_ntoa(host),
			addrtoname(host));
		break;
	}

	return (buff);
}

/*
 *  Here we have to worry about the high order bit being signed
 */
double
s_fixed_to_double(t)
	struct s_fixedpt *t;
{
	double a;

	if (ntohs(t->int_part) & 0x8000) {
		a = ntohs((int) (~t->fraction) & 0xFFFF);
		a = a / 65536.0;	/* shift dec point over by 16 bits */
		a +=  ntohs((int) (~t->int_part) & 0xFFFF);
		a = -a;
	} else {
		a = ntohs(t->fraction);
		a = a / 65536.0;	/* shift dec point over by 16 bits */
		a += ntohs(t->int_part);
	}
	return (a);
}

char *
show_time(t)
	long t;
{
	static char buff[64];

	(void) strcpy(buff, ctime(&t));
	buff[strlen(buff) - 1] = '\0';
	return (buff);
}
