/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.33 $
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ifconfig.c	4.21 (Berkeley) 7/1/88";
#endif /* not lint */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

#include <sys/param.h>
#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smt_snmp_clnt.h>
#include <smt_snmp_api.h>
#include <sm_map.h>
#include <ma_str.h>

extern struct hostent *_gethtbyname(const char*);

static struct	ifreq ifr;
static struct	sockaddr_in sin = { AF_INET };
static struct	sockaddr_in broadaddr;
static struct	sockaddr_in netmask = { AF_INET };
static struct	sockaddr_in ipdst = { AF_INET };
char	name[30];
int	flags;
int	orig_flags;
int	metric;
int	setaddr;
int	setmask;
int	setbroadaddr;
int	setipdst;
int	setprimary;
int	s;
int	rawsoc;
extern	int errno;

static void	setifflags(char*, int);
static void	setifaddr(char*, int);
static void	setifdstaddr(char*, int);
static void	setifnetmask(char*, int);
static void	setifmetric(char*, int);
static void	setifbroadaddr(char*, int);
static void	setifipdst(char*, int);
static void	setifprimary(char*, int);
static void	setphycnt(char*, int);
static void	ck_firm(char*, int);

static int	fddi_snmp(char *, int);
static void	kick_smt();
static void	spawn_smt(void);
static void	printb(char*, unsigned short, char*);
static void	Perror(char *);
static void	status(void);

static int	phy_ct = 0;
const int	smtlog = LOG_ERR;

#define	NEXTARG		0xffffff

struct	cmd {
	char	*c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func)(char*,int);
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
	{ "primary",	1,		setifprimary },
	{ "single",	1,		setphycnt  },
	{ "dual",	2,		setphycnt  },
	{ "load",	1,		ck_firm },
	{ "-trailers",	IFF_NOTRAILERS,	setifflags },
	{ "arp",	-IFF_NOARP,	setifflags },
	{ "-arp",	IFF_NOARP,	setifflags },
	{ "debug",	IFF_DEBUG,	setifflags },
	{ "-debug",	-IFF_DEBUG,	setifflags },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	setifflags },
	{ "-swabips",	-EN_SWABIPS,	setifflags },
#endif
	{ "netmask",	NEXTARG,	setifnetmask },
	{ "metric",	NEXTARG,	setifmetric },
	{ "broadcast",	NEXTARG,	setifbroadaddr },
	{ "ipdst",	NEXTARG,	setifipdst },
	{ 0,		0,		setifaddr },
	{ 0,		0,		setifdstaddr },
};

/*
 * XNS support liberally adapted from
 * code written at the University of Maryland
 * principally by James O'Toole and Chris Torek.
 */

static void in_status(int);
static void in_getaddr(char*, struct sockaddr*);


/* Known address families */
struct afswtch {
	char *af_name;
	short af_af;
	void (*af_status)();
	void (*af_getaddr)();
} afs[] = {
	{ "inet",	AF_INET,	in_status,	in_getaddr },
	{ 0,		0,		0,		0 }
};

struct afswtch *afp;	/*the address family being set or asked about*/

main(argc, argv)
	int argc;
	char *argv[];
{
	int af = AF_INET;

	if (argc < 2) {
		fprintf(stderr, "usage: smtconfig interface\n%s%s%s%s%s",
		    "\t[ af [ address [ dest_addr ] ] [ up ] [ down ]",
			    "[ netmask mask ] ]\n",
		    "\t[ metric n ]\n",
		    "\t[ primary ] [ single | dual ]  [ load ]\n",
		    "\t[ arp | -arp ]\n");
		exit(1);
	}
	argc--, argv++;

	sm_openlog(SM_LOGON_STDERR, smtlog, 0, 0, 0);

	/* get interface name */
	sm_log(LOG_DEBUG, 0, "name = %s\n", *argv);
	strncpy(name, *argv, sizeof(name));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	argc--, argv++;
	if (argc > 0) {
		struct afswtch *myafp;

		for (myafp = afp = afs; myafp->af_name; myafp++)
			if (strcmp(myafp->af_name, *argv) == 0) {
				afp = myafp; argc--; argv++;
				break;
			}
		af = ifr.ifr_addr.sa_family = afp->af_af;
	}
	s = socket(af, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("smtconfig: socket");
		exit(1);
	}
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		Perror("ioctl (SIOCGIFFLAGS)");
		exit(1);
	}
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	flags = ifr.ifr_flags;
	orig_flags = flags;
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (SIOCGIFMETRIC)");
	else
		metric = ifr.ifr_metric;
	if (argc == 0) {
		status();
		map_exit(0);
	}
	while (argc > 0) {
		register struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_name == 0 && setaddr)
			p++;	/* got src, do dst */
		if (p->c_func) {
			if (p->c_parameter == NEXTARG) {
				(*p->c_func)(argv[1],0);
				argc--, argv++;
			} else
				(*p->c_func)(*argv, p->c_parameter);
		}
		argc--, argv++;
	}
	if ((setmask || setaddr) && (af == AF_INET)) {
		/*
		 * It should be impossible to get an interface
		 * UP and RUNNING with address 0.0.0.0.
		 */
		if (setaddr && (sin.sin_addr.s_addr == 0)) {
			errno = EINVAL;
			Perror("ioctl (SIOCSIFADDR)");
		}
		/*
		 * If setting the address and not the mask,
		 * clear any existing mask and the kernel will then
		 * assign the default.  If setting both,
		 * set the mask first, so the address will be
		 * interpreted correctly.
		 */
		ifr.ifr_addr = *(struct sockaddr *)&netmask;
		if (ioctl(s, SIOCSIFNETMASK, (caddr_t)&ifr) < 0)
			Perror("ioctl (SIOCSIFNETMASK)");
	}
	if (setipdst && af==AF_NS) {
		struct nsip_req rq;
		int size = sizeof(rq);

		rq.rq_ns = *(struct sockaddr *) &sin;
		rq.rq_ip = *(struct sockaddr *) &ipdst;

		if (setsockopt(s, 0, SO_NSIP_ROUTE, &rq, size) < 0)
			Perror("Encapsulation Routing");
		setaddr = 0;
	}
	if (setaddr) {
		ck_firm(0,0);
		ifr.ifr_addr = *(struct sockaddr *) &sin;
		if (ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0)
			Perror("ioctl (SIOCSIFADDR)");
		flags |= IFF_UP;	/* side effect of SOICSIFADDR */
	}
	if (setbroadaddr) {
		ifr.ifr_addr = *(struct sockaddr *)&broadaddr;
		if (ioctl(s, SIOCSIFBRDADDR, (caddr_t)&ifr) < 0)
			Perror("ioctl (SIOCSIFBRDADDR)");
	}
	if (setprimary) {
		if (ioctl(s, SIOCSIFHEAD, (caddr_t)&ifr) <0)
			Perror("ioctl (SIOCSIFHEAD)");
	}

	kick_smt();			/* attach to smt */
	map_exit(0);
}

/*ARGSUSED*/
static void
setifaddr(char *addr, int param)
{
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	(*afp->af_getaddr)(addr, &sin);
}

/*ARGSUSED*/
static void
setifnetmask(char *addr, int param)
{
	in_getaddr(addr, (struct sockaddr*)&netmask);
	setmask++;
}

/*ARGSUSED*/
static void
setifbroadaddr(char *addr, int param)
{
	(*afp->af_getaddr)(addr, &broadaddr);
	setbroadaddr++;
}

/*ARGSUSED*/
static void
setifipdst(char *addr, int param)
{
	in_getaddr(addr, (struct sockaddr*)&ipdst);
	setipdst++;
}

/*ARGSUSED*/
static void
setifdstaddr(char *addr, int param)
{

	(*afp->af_getaddr)(addr, &ifr.ifr_addr);
	if (ioctl(s, SIOCSIFDSTADDR, (caddr_t)&ifr) < 0)
		Perror("ioctl (SIOCSIFDSTADDR)");
}

static void
setifflags(char *vname, int value)
{
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		Perror("ioctl (SIOCGIFFLAGS)");
	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	flags = ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else {
		flags |= value;
	}
	if (!geteuid() && !(flags & ifr.ifr_flags & IFF_UP))
	    ck_firm(0,0);

	ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
		Perror(vname);
}

/*ARGSUSED*/
static void
setifmetric(char *val, int param)
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (set metric)");
}

/* ARGSUSED */
static void
setifprimary(char *vname, int value)
{
	setprimary = value;
}

#ifdef sgi
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\11PROMISC\12ALLMULTI\13FILTMULTI\14MULTICAST\15CKSUM"
#else
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
"
#endif

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
static void
status(void)
{
	register struct afswtch *p = afp;

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (metric)
		printf(" metric %d", metric);
	putchar('\n');
	if ((p = afp) != NULL) {
		(*p->af_status)(1);
	} else for (p = afs; p->af_name; p++) {
		ifr.ifr_addr.sa_family = p->af_af;
		(*p->af_status)(0);
	}
}

static void
in_status(int force)
{
	struct sockaddr_in *stsin;

	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		} else
			perror("ioctl (SIOCGIFADDR)");
	}
	stsin = (struct sockaddr_in *)&ifr.ifr_addr;
	printf("\tinet %s ", inet_ntoa(stsin->sin_addr));
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			perror("ioctl (SIOCGIFNETMASK)");
		bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
	} else
		netmask.sin_addr =
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFDSTADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		stsin = (struct sockaddr_in *)&ifr.ifr_dstaddr;
		printf("--> %s ", inet_ntoa(stsin->sin_addr));
	}
#ifdef sgi
	printf("netmask %#lx ", ntohl(netmask.sin_addr.s_addr));
#else
	printf("netmask %x ", ntohl(netmask.sin_addr.s_addr));
#endif
	if (flags & IFF_BROADCAST) {
		if (ioctl(s, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		stsin = (struct sockaddr_in *)&ifr.ifr_addr;
		if (stsin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(stsin->sin_addr));
	}
	putchar('\n');

	(void)fddi_snmp(name, FDDI_SNMP_INFO);
}



static void
Perror(char *cmd)
{
	switch (errno) {

	case ENXIO:
		fprintf(stderr, "smtconfig: %s: no such interface\n", cmd);
		break;

	case EPERM:
		fprintf(stderr, "smtconfig: %s: permission denied\n", cmd);
		break;

	default:
		perror(cmd);
	}
	map_exit(1);
}


static void
in_getaddr(char *str,
	   struct sockaddr *saddr)
{
	struct sockaddr_in *insin = (struct sockaddr_in *)saddr;
	struct hostent *hp;
	struct netent *np;
	int val;

	insin->sin_family = AF_INET;
	val = inet_addr(str);
	if (val != -1) {
		insin->sin_addr.s_addr = val;
		return;
	}
#ifdef sgi
	if (!strcmp("255.255.255.255",str)) {
		insin->sin_addr.s_addr = val;
		return;
	}
	/* Get hostnames from /etc/hosts only */
	hp = _gethtbyname(str);
#else
	hahahahahahaha
	hp = gethstbyname(str);
#endif /* sgi */
	if (hp) {
		insin->sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, &insin->sin_addr, hp->h_length);
		return;
	}
	np = getnetbyname(str);
	if (np) {
		insin->sin_family = np->n_addrtype;
		insin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
		return;
	}
	fprintf(stderr, "%s: bad value\n", str);
	map_exit(1);
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
static void
printb(char *str,
       unsigned short v,
       char *bits)
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", str, v);
	else
		printf("%s=%x", str, v);
	bits++;
	if (bits) {
		putchar('<');
		while (i = *bits++) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					continue;
		}
		putchar('>');
	}
}

/*ARGSUSED*/
static void
setphycnt(char *vname, int value)
{
	phy_ct = value;
}

static void
kick_smt()
{
	int cmd;

	/* Quit if the interface is not being changed,
	 * and if either we cannot do anything (not root) or the interface
	 *	is already down.
	 * This is to restart the daemon if it has died, while the interface
	 * is still up.
	 */
	if (((orig_flags ^ flags) & IFF_UP) == 0
	    && (geteuid() !=0 || !(flags & IFF_UP)))
		return;

	if (geteuid()) {
		fprintf(stderr,
			"smtconfig: cannot turn on %s except as root\n",
			name);
		return;
	}
	setuid(0);

	if (flags & IFF_UP) {
		spawn_smt();
		if (phy_ct == 1)
			cmd = FDDI_SNMP_SINGLE;
		else if (phy_ct == 2)
			cmd = FDDI_SNMP_DUAL;
		else
			cmd = FDDI_SNMP_UP;
		if (fddi_snmp(name, cmd) != STAT_SUCCESS)
			Perror("SMT_UP");

	} else {
		if (fddi_snmp(name, FDDI_SNMP_DOWN) != STAT_SUCCESS)
			Perror("SMT_DOWN");
	}
}


/*
 * Get 1 variable each time.
 */
static int
fddi_snmp(char *ifname, int action)
{
	int stat;
	PORT_INFO info;

	map_settimo(10, 0);
	stat = map_smt(ifname, action, (char *)&info, sizeof(info), 0);
	return(stat);
}

/*
 * reap parent of the SMT daemon.
 */
/* ARGSUSED */
static void
reapchild(int i)
{
	int stat;

	wait(&stat);
}

/* shut down the board
 */
static void
board_down()
{
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		Perror("ioctl (SIOCGIFFLAGS)");
	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	orig_flags= ifr.ifr_flags;

	ifr.ifr_flags = orig_flags & ~IFF_UP;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
		Perror(name);
	if (orig_flags & IFF_UP) {
		if (fddi_snmp(name, FDDI_SNMP_DOWN) != STAT_SUCCESS)
			Perror("SMT_DOWN");
	}
}

/*
 *	Start the SMT daemon.
 */
static void
spawn_smt(void)
{
	int pid;
	SMT_FSSTAT fsq;

	map_settimo(10, 0);
	if (smtlog == LOG_ERR)
		sm_openlog(SM_LOGON_STDERR, 0, 0, 0, 0);
	if (map_smt(0, FDDI_SNMP_FSSTAT, (char*)&fsq, sizeof(fsq), 0)
	    == STAT_SUCCESS) {
		sm_openlog(SM_LOGON_STDERR, smtlog, 0, 0, 0);
		return;
	}

	sm_openlog(SM_LOGON_STDERR, smtlog, 0, 0, 0);

	if (pid = fork()) {
		if (pid < 0) {
			fprintf(stderr, "smtconfig: smtd failed: %s\n",
				strerror(errno));
			map_exit(1);
		}
		/* Wait for smtd to fork&exit.  The SIGCLD will awaken us.
		 * This is much faster than simply waiting a fixed period.
		 */
		(void)signal(SIGCLD,reapchild);
		(void)sleep(15);
		(void)signal(SIGCLD,SIG_DFL);
		return;
	}

	(void) execl("/usr/etc/smtd", SMTD_SERVICE, 0);
	perror("smtconfig: can't exec smtd");
}





/* download to a GIO bus FDDI board
 */
#include "if_xpi.h"
#include "xpi/lc.firm"
#include "xpi/lc_flash.firm"
#include "xpi/mez_s.firm"
#include "xpi/mez_d.firm"
#include "xpi/mez_flash.firm"

static struct xpi_dwn xpi_dwn;
static int xpi_failed = 0;

static void
xpi_downflush()
{
	if (!xpi_failed
	    && 0 > ioctl(rawsoc, SIOC_XPI_STO, &xpi_dwn)) {
		perror("\nsmtconfig SIOC_XPI_STO");
		xpi_failed = 1;
	}
	xpi_dwn.addr += xpi_dwn.cnt*4;
	xpi_dwn.cnt = 0;
}


static void
xpi_download(u_long firm_min,
	     u_long addr,		/* start here */
	     u_long firm_max,		/* fill this much */
	     u_char* fp,		/* bytes of firmware */
	     u_char* fp_lim)
{
	int j;
	u_char op;
	u_long v;


	xpi_dwn.addr = firm_min;
	xpi_dwn.cnt = 0;

	/* fill initial part with zeros
	 */
	while (xpi_dwn.addr+xpi_dwn.cnt*4 < addr) {
		if (xpi_dwn.cnt >= XPI_DWN_LEN)
			xpi_downflush();
		xpi_dwn.val.l[xpi_dwn.cnt++] = 0;
	}

	/* install the firmware
	 */
	while (fp != fp_lim) {
		op = *fp++;
		if (op < lc_DZERO) {
			j = op-lc_DDATA+1;
			while (j-- != 0) {
				if (xpi_dwn.cnt >= XPI_DWN_LEN)
					xpi_downflush();
				xpi_dwn.val.c[xpi_dwn.cnt*4+0] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+1] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+2] = *fp++;
				xpi_dwn.val.c[xpi_dwn.cnt*4+3] = *fp++;
				xpi_dwn.cnt++;
			}
		} else {
			if (op < lc_DNOP) {
				j = op-lc_DZERO+1;
				v = 0;
			} else {
				j = op-lc_DNOP+1;
				v = 0x70400101;
			}
			while (j-- != 0) {
				if (xpi_dwn.cnt >= XPI_DWN_LEN)
					xpi_downflush();
				xpi_dwn.val.l[xpi_dwn.cnt++] = v;
			}
		}
	}

	/* fill rest with 0s to make checksum right
	 */
	while (xpi_dwn.addr+xpi_dwn.cnt*4 < firm_max) {
		if (xpi_dwn.cnt >= XPI_DWN_LEN)
			xpi_downflush();
		xpi_dwn.val.l[xpi_dwn.cnt++] = 0;
	}

	if (0 != xpi_dwn.cnt)
		xpi_downflush();
}



/* check the firmware on the FDDI board
 *	If it is old or bad, download a new version
 */
/* ARGSUSED */
static void
ck_firm(char *vname, int force)
{
	struct sockaddr_raw sr;
	struct xpi_vers vers;
	__uint32_t sig;
	int l;

	l = strlen(name);
	if (l == 4 && (!strncmp(name, "ipg", l-1)
		       || !strncmp(name, "rns", l-1)))
		return;			/* nothing to do */

	if (l < 4 || strncmp(name, "xpi", 3)) {
		fprintf(stderr, "smtconfig: unrecognized interface: %s\n",
			name);
		return;
	}

	rawsoc = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
	if (rawsoc < 0) {
		perror("smtconfig socket");
		return;
	}
	bzero((char*)&sr, sizeof(sr));
	(void)strncpy(sr.sr_ifname, name, sizeof(sr.sr_ifname));
	sr.sr_family = AF_RAW;
	sr.sr_port = 0;
	if (0 > bind(rawsoc, &sr, sizeof(sr))) {
		perror("smtconfig bind");
		return;
	}

	/* Get and compare versions.
	 *	Update the board if it differs from us,
	 *	even if it is newer.  This minimizes problems
	 *	with version skew.
	 */
	vers.type = -1;
	if (0 > ioctl(rawsoc, SIOC_XPI_VERS, &vers)) {
		l = errno;
		board_down();
		errno = l;
		Perror("smtconfig SIOC_XPI_VERS");
	}
	if (vers.type != XPI_TYPE_LC
	    && vers.type != XPI_TYPE_MEZ_S
	    && vers.type != XPI_TYPE_MEZ_D) {
		fprintf(stderr,
			"smtconfig: unrecognized XPI board type %d\n",
			vers.type);
		return;
	}
	vers.vers &= (XPI_VERS_CKSUM | XPI_VERS_M | XPI_VERS_FAM_M);
	if (!force
	    && ((vers.type == XPI_TYPE_LC && lc_vers == vers.vers)
		|| (vers.type == XPI_TYPE_MEZ_S && mez_s_vers == vers.vers)
		|| (vers.type == XPI_TYPE_MEZ_D && mez_d_vers == vers.vers)))
		return;

	board_down();			/* shut down the board */

	fflush(stderr);
	fprintf(stdout, "smtconfig: WARNING: Writing %s EEPROM.\n", name);
	fprintf(stdout,"\tDo not reset the system until it is finished");
	/* pause in case the operator does try to abort the process */
	fflush(stdout);
	for (l = 0; l < 2*2; l++) {
		sginap(HZ/2);
		fputc("   ."[l],stdout);
		fflush(stdout);
	}

	if (vers.type == XPI_TYPE_LC) {
		xpi_download(XPI_FIRM + XPI_DOWN,
			     XPI_FIRM + XPI_DOWN + lc_minaddr - XPI_SRAM,
			     XPI_FIRM + XPI_FIRM_SIZE + XPI_DOWN,
			     &lc_txt[0],
			     &lc_txt[sizeof(lc_txt)]);

		/* download stand-alone driver ... someday
		 */
		xpi_download(XPI_DRV + XPI_DOWN,
			     XPI_DRV + XPI_DOWN,
			     XPI_DRV + XPI_DRV_SIZE + XPI_DOWN, 0,0);

		/* download the code to program the EEPROM
		 */
		xpi_download(lc_flash_minaddr,
			     lc_flash_minaddr,
			     lc_flash_maxaddr,
			     &lc_flash_txt[0],
			     &lc_flash_txt[sizeof(lc_flash_txt)]);
	} else {
		if (vers.type == XPI_TYPE_MEZ_S) {
			xpi_download(XPI_FIRM + XPI_DOWN,
				     XPI_FIRM + XPI_DOWN
				     + mez_s_minaddr - XPI_SRAM,
				     XPI_FIRM + XPI_FIRM_SIZE + XPI_DOWN,
				     &mez_s_txt[0],
				     &mez_s_txt[sizeof(mez_s_txt)]);
		} else {
			xpi_download(XPI_FIRM + XPI_DOWN,
				     XPI_FIRM + XPI_DOWN
				     + mez_d_minaddr - XPI_SRAM,
				     XPI_FIRM + XPI_FIRM_SIZE + XPI_DOWN,
				     &mez_d_txt[0],
				     &mez_d_txt[sizeof(mez_d_txt)]);
		}

		/* download stand-alone driver ... someday
		 */
		xpi_download(XPI_DRV + XPI_DOWN,
			     XPI_DRV + XPI_DOWN,
			     XPI_DRV + XPI_DRV_SIZE + XPI_DOWN, 0,0);

		/* download the code to program the EEPROM
		 */
		xpi_download(mez_flash_minaddr,
			     mez_flash_minaddr,
			     mez_flash_maxaddr,
			     &mez_flash_txt[0],
			     &mez_flash_txt[sizeof(mez_flash_txt)]);
	}

	if (xpi_failed) {
		flags &= ~IFF_UP;
		return;
	}

	/* zap it */
	xpi_dwn.addr = XPI_PROG;
	if (0 > ioctl(rawsoc, SIOC_XPI_EXEC, &xpi_dwn)) {
		perror("\nsmtconfig SIOC_XPI_EXEC");
		flags &= ~IFF_UP;
		return;
	}

	/* wait until the chip is written
	 */
	for (l = (20*HZ)/(HZ/4); l != 0; l--) {
		sig = 0;
		if (0 <= ioctl(rawsoc,SIOC_XPI_SIGNAL,&sig))
			break;
		if (errno != EIO) {
			perror("\nsmtconfig SIOC_XPI_SIGNAL");
			break;
		}
		fputc('.',stdout);
		fflush(stdout);
		sginap(HZ/4);
	}
	fputc('\n',stdout);
	if (!l) {
		fprintf(stdout, "\t%s failed to start: %s\n",
			name, strerror(errno));
		flags &= ~IFF_UP;
	}
	(void)fprintf(stdout,"\tFinished.\n");

	if (0 != (flags & orig_flags & IFF_UP)) {
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		ifr.ifr_flags = flags;
		if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
			Perror(name);
	}
}
