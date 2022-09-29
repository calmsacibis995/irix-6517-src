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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cap_net.h>

extern int errno;
struct	ifreq ifr;
struct  ifaliasreq  addreq, ridreq;
struct	sockaddr_in netmask = { AF_INET };

void	Perror(char *);
void	printb(char *, __uint32_t, char *);
void	printall(void);

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifra_addr), SIN(addreq.ifra_addr),
SIN(addreq.ifra_mask), SIN(addreq.ifra_broadaddr)};

/* XXX this hack needs to be removed post 6.5 */
int siocsifflags = SIOCSIFFLAGS;
int siocgifflags = SIOCGIFFLAGS;

#undef ALIAS_DEBUG
char	name[30];
int	all = 0;
int	flags;
int	metric;
int	setmask;
int	setbroadaddr;
int	setprimary;
int	setaddr;
int	setipdst;
int	doalias;
int	clearaddr;
int	newaddr = 1;
int	s;
extern	int errno;

#define RIDADDR 0
#define ADDR 	1
#define MASK	2
#define DSTADDR	3

void	setifflags(char *, int), setifaddr(char *, int), 
	setifdstaddr(char *, int), setifnetmask(char *, int);
void	setifmetric(char *, int), setifbroadaddr(char *, int), 
	setifipdst(char *, int), setifprimary(char *, int),
	setifsspace(char *, int), setifrspace(char *, int);
void	notealias(char *, int), get_aliases(char *, int);

int 	Ioctl(int, int, struct ifreq *);

#ifdef ALIAS_DEBUG
void	print_sockaddr(struct sockaddr *);
#endif

#define	NEXTARG		0xffffff

struct	cmd {
	char	*c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func)(char *, int);
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
#ifndef sgi
	{ "trailers",	-IFF_NOTRAILERS, setifflags },
#else
	{ "primary",	1,		setifprimary },
#endif
	{ "-trailers",	IFF_NOTRAILERS,	setifflags },
	{ "arp",	-IFF_NOARP,	setifflags },
	{ "-arp",	IFF_NOARP,	setifflags },
	{ "debug",	IFF_DEBUG,	setifflags },
	{ "-debug",	-IFF_DEBUG,	setifflags },
	{ "link0",	IFF_LINK0,	setifflags },
	{ "-link0",	-IFF_LINK0,	setifflags },
	{ "link1",	IFF_LINK1,	setifflags },
	{ "-link1",	-IFF_LINK1,	setifflags },
	{ "link2",	IFF_LINK2,	setifflags },
	{ "-link2",	-IFF_LINK2,	setifflags },
	{ "alias",	IFF_UP,		notealias },
	{ "-alias",	-IFF_UP,	notealias },
	{ "delete",	-IFF_UP,	notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	setifflags },
	{ "-swabips",	-EN_SWABIPS,	setifflags },
#endif
	{ "netmask",	NEXTARG,	setifnetmask },
	{ "metric",	NEXTARG,	setifmetric },
	{ "broadcast",	NEXTARG,	setifbroadaddr },
	{ "sspace",	NEXTARG,	setifsspace },
	{ "rspace",	NEXTARG,	setifrspace },
	{ "ipdst",	NEXTARG,	setifipdst },
	{ "list",	1,		get_aliases },
	{ "cksum",	IFF_CKSUM,	setifflags } ,
	{ "nocksum",	-IFF_CKSUM,	setifflags } ,
	{ 0,		0,		setifaddr },
	{ 0,		0,		setifdstaddr },
};

void	in_status(int), in_getaddr(char *, int);
void	status(void);

/* Known address families */
struct afswtch {
	char *af_name;
	short af_af;
	void (*af_status)(int);
	void (*af_getaddr)(char *, int);
	int af_difaddr;
	int af_aifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet",	AF_INET,	in_status,	in_getaddr,
		SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
	{ 0,		0,		0,		0 }
};

struct afswtch *afp;	/*the address family being set or asked about*/
int verbose = 0;

void
usage(void)
{
	fprintf(stderr, 
	    "usage: ifconfig [-v] interface | -a\n%s%s%s%s%s%s%s%s%s%s%s%s",
	    "\t[ af [ address [ dest_addr ] ] [ up ] [ down ]",
		    "[ netmask mask ] ]\n",
	    "\t[ metric n ]\n",
	    "\t[ primary ]\n",
	    "\t[ rspace n ]\n",
	    "\t[ sspace n ]\n",
	    "\t[ alias | -alias | delete ]\n",
	    "\t[ arp | -arp ]\n",
	    "\t[ link0 | -link0 ]\n",
	    "\t[ link1 | -link1 ]\n",
	    "\t[ link2 | -link2 ]\n",
	    "\t[ list ]\n");
	exit(1);
}

main(int argc, char **argv)
{
	int af = AF_INET;
	register struct afswtch *myafp;
	cap_t ocap;
	int c;
	extern int optind;

	if (argc < 2) {
		usage();
	}

	while ((c = getopt(argc, argv, "va")) != -1) {
		switch (c) {
		case 'a':
			all = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((argc == 0) && !all) {
		usage();
	} else if (!all) {
		strncpy(name, *argv, sizeof(name));
		strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	}
	argc--, argv++;
	if (argc > 0) {
		for (myafp = afp = afs; myafp->af_name; myafp++)
			if (strcmp(myafp->af_name, *argv) == 0) {
				afp = myafp; argc--; argv++;
				break;
			}
		myafp = afp;
		af = ifr.ifr_addr.sa_family = afp->af_af;
	}
	s = socket(af, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("ifconfig: socket");
		exit(1);
	}
	if (all) {
		printall();
		exit(0);
	}
	if (Ioctl(s, siocgifflags, &ifr) < 0) {
		Perror("ioctl (SIOCGIFFLAGS)");
		exit(1);
	}
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	flags = ifr.ifr_flags;
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (SIOCGIFMETRIC)");
	else
		metric = ifr.ifr_metric;
	if (argc == 0) {
		status();
		exit(0);
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
				if (argc <= 1){
					fprintf(stderr, "ifconfig: missing argument\n");
					exit(1);
				}
				(*p->c_func)(argv[1], 0);
				argc--, argv++;
			} else {
				/* 
				 * Make sure users specify an address with
				 * all alias commands
				 */
				if (p->c_name &&
					(!strcmp(p->c_name, "alias") 
					|| !strcmp(p->c_name, "-alias")
					|| !strcmp(p->c_name, "delete"))){
					if (argc <= 1){
						fprintf(stderr,"ifconfig: alias|-alias|delete: must specify address\n");
						exit(1);
					}
				}
				(*p->c_func)(*argv, p->c_parameter);
		}
		}
		argc--, argv++;
	}

	if ((doalias == 0) && (setmask) && (af == AF_INET)) {
#ifdef sgi
		/*
		 * It should be impossible to get an interface
		 * UP and RUNNING with address 0.0.0.0.
		 */
		if (setmask && (sintab[MASK]->sin_addr.s_addr == 0)) {
			errno = EINVAL;
			Perror("ioctl (SIOCSIFNETMASK)");
		}
#endif
		/*
		 * If setting the address and not the mask,
		 * clear any existing mask and the kernel will then
		 * assign the default.  If setting both,
		 * set the mask first, so the address will be
		 * interpreted correctly.
		 */
		ifr.ifr_addr = *(struct sockaddr *)sintab[MASK];
		ifr.ifr_addr.sa_family = AF_INET;
		if (cap_network_ioctl(s, SIOCSIFNETMASK, (caddr_t)&ifr) < 0)
			Perror("ioctl (SIOCSIFNETMASK)");
	}

	if (doalias) {	/* Process switches -alias or alias cases */

		int ret;
		struct	ifreq *ifreqp;
		struct  ifaliasreq  *ifaliasreqp;

		strncpy(myafp->af_ridreq, name, sizeof ifr.ifr_name);
		/*
		 * in_control(struct socket *so, __psint_t cmd, caddr_t data,
		 * struct ifnet *ifp)
		 * struct in_aliasreq *ifra = (struct in_aliasreq *)data;
		 * struct ifreq *ifr = (struct ifreq *)data;
		 */
		ifreqp = (struct ifreq *)myafp->af_ridreq;

		if (clearaddr) { /* handle -alias switch case */
#ifdef ALIAS_DEBUG
			printf(" IP alias processing; clearaddr case\n");
		    printf(" ioctl cmd SIOCDIFADDR 0x%x\n", myafp->af_difaddr);
			printf(" af_ridreq(ifreq) 0x%x, ifr_name %s\n",
			       ifreqp, ifreqp->ifr_name);
			print_sockaddr(&(ifreqp->ifr_addr));
			printf(" ifru_flags 0x%x, ifru_metric %d\n",
				ifreqp->ifr_flags, ifreqp->ifr_metric);
#endif
			ret = ioctl(s, myafp->af_difaddr, myafp->af_ridreq);
			if (ret < 0) {
				if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
					/* means no previous address
					 * for interface
					 */
				} else
					Perror("ioctl (SIOCDIFADDR)");
			}
		}

		strncpy(myafp->af_addreq, name, sizeof ifr.ifr_name);
		ifaliasreqp = (struct ifaliasreq *)myafp->af_addreq;

		if (newaddr) { /* add new alias address */

#ifdef ALIAS_DEBUG
			printf(" IP alias processing; newaddr case\n");
			printf(" ioctl cmd SIOCAIFADDR 0x%x\n",
				myafp->af_aifaddr);

			printf(" ifaliasreq 0x%x, ifra_name %s\n",
			       ifaliasreqp, ifaliasreqp->ifra_name);
			printf("  ifaliasreq: ifra_addr ");
			print_sockaddr(&(ifaliasreqp->ifra_addr));

			printf("  ifra_broadaddr ");
			print_sockaddr(&(ifaliasreqp->ifra_broadaddr));

			printf("  ifra_mask ");
			print_sockaddr(&(ifaliasreqp->ifra_mask));
#endif
			if (setaddr) {
				if (sintab[ADDR]->sin_addr.s_addr == 0) {
					fprintf(stderr,
					  "ifconfig: bad alias address\n");
					errno = EINVAL;
					Perror("ioctl (SIOCAIFADDR)");
				}
					
				ifaliasreqp->ifra_addr.sa_family = AF_INET;
				ifaliasreqp->ifra_addr =
					*(struct sockaddr *) sintab[ADDR];
			}
			if (setbroadaddr) {
				if (sintab[DSTADDR]->sin_addr.s_addr == 0) {
					fprintf(stderr,
					  "ifconfig: bad broadcast address\n");
					errno = EINVAL;
					Perror("ioctl (SIOCAIFADDR)");
				}
				ifaliasreqp->ifra_mask.sa_family = AF_INET;
				ifaliasreqp->ifra_broadaddr =
					*(struct sockaddr *)sintab[DSTADDR];
			}

			if (setmask) {
				struct sockaddr_in_new *tsa_in;
				if (sintab[MASK]->sin_addr.s_addr == 0) {
					fprintf(stderr,
					  "ifconfig: bad network mask\n");
					errno = EINVAL;
					Perror("ioctl (SIOCAIFADDR)");
				}
				ifaliasreqp->ifra_mask.sa_family = AF_INET;
				tsa_in= (struct sockaddr_in_new *)sintab[MASK];
				tsa_in->sin_len = sizeof(struct sockaddr_in);

				ifaliasreqp->ifra_mask =
					*(struct sockaddr *)sintab[MASK];
			}
			if (ioctl(s, myafp->af_aifaddr, myafp->af_addreq) < 0)
				Perror("ioctl (SIOCAIFADDR)");
		}
	} else { /* Process normal switch cases */

		if (setaddr) {
			if (sintab[ADDR]->sin_addr.s_addr == 0) {
				errno = EINVAL;
				Perror("ioctl (SIOCSIFADDR)");
			}
			ifr.ifr_addr = *(struct sockaddr *) sintab[ADDR];
			if (cap_network_ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0)
				Perror("ioctl (SIOCSIFADDR)");
		}
		if (setbroadaddr) {
			if (sintab[DSTADDR]->sin_addr.s_addr == 0) {
				errno = EINVAL;
				Perror("ioctl (SIOCSIFBRDADDR)");
			}
			ifr.ifr_addr = *(struct sockaddr *)sintab[DSTADDR];
			if (cap_network_ioctl(s, SIOCSIFBRDADDR, (caddr_t)&ifr) < 0)
				Perror("ioctl (SIOCSIFBRDADDR)");
		}
		if (setprimary) {
			if (cap_network_ioctl(s, SIOCSIFHEAD, (caddr_t)&ifr) <0)
				Perror("ioctl (SIOCSIFHEAD)");
		}
	}
	exit(0);
}

struct ifseen {
	struct ifseen *next;
	char *name;
} *ifshead;

void
handle_ifreq(struct ifreq *ifr)
{
	struct ifseen *ifs;
	char	*strdup();

	/* LMXXX - this seems to be avoiding repeated calls on the
	 * same interface.  Needed?
	 */
	for(ifs=ifshead; ifs; ifs=ifs->next)
		if(strcmp(ifs->name, ifr->ifr_name)==0)
			return;

	strncpy(name, ifr->ifr_name, sizeof ifr->ifr_name);
	flags = ifr->ifr_flags;

	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)ifr) < 0) {
		perror("ioctl (SIOCGIFMETRIC)");
		metric = 0;
	} else {
		metric = ifr->ifr_metric;
	}
	status();
	
	ifs = (struct ifseen *)malloc(sizeof *ifs);
	ifs->name = strdup(ifr->ifr_name);
	ifs->next = ifshead;
	ifshead = ifs;
}

void
printall(void)
{
	char inbuf[8192];
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	int i;

	ifc.ifc_len = sizeof inbuf;
	ifc.ifc_buf = inbuf;
	if( ioctl(s, SIOCGIFCONF, &ifc) < 0) {
		Perror("ioctl(SIOCGIFCONF)");
	}
	ifr = ifc.ifc_req;
	ifreq.ifr_name[0] = '\0';
	for (i = ifc.ifc_len / sizeof (struct ifreq); i > 0; i--, ifr++) {
		ifreq = *ifr;
		if( Ioctl(s, siocgifflags, &ifreq) < 0) {
			perror("ioctl(SIOCGIFFLAGS)");
			continue;
		}
		handle_ifreq(&ifreq);
	}
}


/*ARGSUSED*/
void
setifaddr(char *addr, int param)
{
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias == 0)
		clearaddr = 1;
	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

/* ARGSUSED */
void
setifnetmask(char *addr, int val)
{
	(*afp->af_getaddr)(addr, MASK);
	setmask++;
}

/* ARGSUSED */
void
setifbroadaddr(char *addr, int val)
{
	(*afp->af_getaddr)(addr, DSTADDR);
	setbroadaddr++;
}

/* ARGSUSED */
void
setifipdst(char *addr, int val)
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}

#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))
/*ARGSUSED*/
void
notealias(char *addr, int param)
{
	if (setaddr && doalias == 0 && param < 0)
		bcopy((caddr_t)rqtosa(af_addreq),
			(caddr_t)rqtosa(af_ridreq), 14);
	doalias = param;
	if (param < 0) { /* delete case aka '-alias' */
		clearaddr = 1;
		newaddr = 0;
	} else { /* add case aka 'alias' */
		clearaddr = 0;
	}
}

/*ARGSUSED*/
void
setifdstaddr(char *addr, int param)
{
	(*afp->af_getaddr)(addr, DSTADDR);
	ifr.ifr_addr = *(struct sockaddr *)sintab[DSTADDR];
	if (cap_network_ioctl(s, SIOCSIFDSTADDR, (caddr_t)&ifr) < 0)
		Perror("ioctl (SIOCSIFDSTADDR)");
}

/*ARGSUSED*/
void
setifrspace(char *val, int val2)
{
	ifr.ifr_perf = strtoul(val, (char *)0, 0);
	if (cap_network_ioctl(s, SIOCSIFRECV, (caddr_t)&ifr) < 0)
		Perror("ioctl (SIOCSIFRECV)");
}

/*ARGSUSED*/
void
setifsspace(char *val, int val2)
{
	ifr.ifr_perf = strtoul(val, (char *)0, 0);
	if (cap_network_ioctl(s, SIOCSIFSEND, (caddr_t)&ifr) < 0)
		Perror("ioctl (SIOCSIFSEND)");
}

void
setifflags(char *vname, int value)
{
	cap_t ocap;

 	if (Ioctl(s, siocgifflags, &ifr) < 0) {
 		Perror("ioctl (SIOCGIFFLAGS)");
 		exit(1);
 	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
 	flags = ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
#ifdef sgi
		if (setaddr && value == IFF_UP) {
                      	if (setmask) {
				ifr.ifr_addr = *(struct sockaddr *)sintab[MASK];
             			ifr.ifr_addr.sa_family = AF_INET;
                             	if (ioctl(s, SIOCSIFNETMASK, (caddr_t)&ifr) < 0)
                                	Perror("ioctl (SIOCSIFNETMASK)");
                                setmask = 0;
                        }

			/*
			 * special case setting an address as well as
			 * the down bit on the same line - addr goes FIRST
			 */
			ifr.ifr_addr = *(struct sockaddr *)sintab[ADDR];
			if (cap_network_ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0)
				Perror("ioctl (SIOCSIFADDR)");
			setaddr = 0;
#endif
		}
	} else
		flags |= value;
	ifr.ifr_flags = flags;
	if (cap_network_ioctl(s, siocsifflags, (caddr_t)&ifr) < 0)
		Perror(vname);
}

/* ARGSUSED */
void
setifmetric(char *val, int val2)
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (cap_network_ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		perror("ioctl (set metric)");
}

void
setifprimary(char *vname, int value)
{
	setprimary = value;
}

#ifdef sgi
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\11PROMISC\12ALLMULTI\13FILTMULTI\14MULTICAST\15CKSUM\16ALLCAST\17DRVRLOCK\20PRIVATE\21LINK0\22LINK1\23LINK2\25L2IPFRAG\26L2TCPSEG\27IPALIAS"
#else
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
"
#endif

char *table[] = {
	"bit", "Kbit", "Mbit", "Gbit", "Tbit"
};
int n_units = sizeof(table) / sizeof(char *);

char *
fancy(__uint64_t speed)
{
	int i = 0;
	static char sbuf[25];
	float f = speed;
	while (f > 1000.00) {
		f /= 1000.00;
		i++;
	}
	if (i >= n_units) {
		sprintf(sbuf, "%f bit/s", (float)speed);
	} else {
		sprintf(sbuf, "%3.2f %s/s", f, table[i]);
	}
	return sbuf;
}

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status(void)
{
	register struct afswtch *p = afp;
	short af = ifr.ifr_addr.sa_family;
	int r;
	u_long recvspace = 0;
	u_long sendspace = 0;
	struct ifspeed speed = {0,0};
	__uint64_t ispeed;
	struct ifdatareq ifd;

	printf("%s: ", name);
	if (siocgifflags == OSIOCGIFFLAGS) {
		flags >>= 16;	/* XXX hack, hack, hack */
	}
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

	if (!verbose)
		return;

	bzero(&ifr, sizeof(ifr));
	strcpy(ifr.ifr_name, name);
	r = ioctl(s, SIOCGIFRECV, &ifr);
	if (r < 0) {
		perror("ioctl (SIOCGIFRECV)");
	} else {
		recvspace = ifr.ifr_perf;
	}
	r = ioctl(s, SIOCGIFSEND, &ifr);
	if (r < 0) {
		perror("ioctl (SIOCGIFSEND)");
	} else {
		sendspace = ifr.ifr_perf;
	}
	bzero(&ifd, sizeof(ifd));
	strcpy(ifd.ifd_name, name);
	r = ioctl(s, SIOCGIFDATA, &ifd);
	if (r < 0) {
		ispeed = 0;
	} else {
		ispeed = if_getbaud(ifd.ifd_ifd.ifi_baudrate);
	}
	if (recvspace || sendspace) {
		printf("\t");
		if (recvspace) {
			printf("recvspace %d ", recvspace);
		}
		if (sendspace) {
			printf("sendspace %d ", sendspace);
		}
		printf("\n");
	}
	if (ispeed) {
		printf("\tspeed %s", fancy(ispeed));
		if (ifd.ifd_ifd.ifi_type == IFT_ETHER) {
			if (flags & IFF_LINK0)
				printf(" full-duplex");
			else
				printf(" half-duplex");
		}
		printf("\n");
	}
}

void
in_status(int force)
{
	struct sockaddr_in *sin;
	char *inet_ntoa();

	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
		} else
			perror("ioctl (SIOCGIFADDR)");
	}
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			perror("ioctl (SIOCGIFNETMASK)");
		bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
	} else
		sintab[MASK]->sin_addr = 
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFDSTADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_dstaddr;
		printf("--> %s ", inet_ntoa(sin->sin_addr));
	}
	printf("netmask %#x ", ntohl(sintab[MASK]->sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		if (ioctl(s, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    bzero((char *)&ifr.ifr_addr, sizeof(ifr.ifr_addr));
			else
			    perror("ioctl (SIOCGIFADDR)");
		}
		strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		if (sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');

	get_aliases((char *)0, 0);
}

/* ARGSUSED */
void
get_aliases(char *a1, int a2)
{
	struct ifaliasreq ifalias;
	struct sockaddr_in *sin;
	char *inet_ntoa();

	bzero (&ifalias, sizeof(struct ifaliasreq));
	strncpy(ifalias.ifra_name, name, sizeof ifalias.ifra_name); 

	ifalias.ifra_addr.sa_family = AF_INET;
	/*
	 * A cookie of zero means to return the primary address first
	 * and subsequent calls will return the aliases as the cookie
	 * value increase linearly. When a -1 is returned we've exhausted
	 * the alias'es for this interface.
	 */
	ifalias.cookie = 1;

	while (1){
		if ((ioctl(s, SIOCLIFADDR, (caddr_t)&ifalias)) < 0) { 
			perror("SIOCLIFADDR failed");
			break;
		}
		if (ifalias.cookie < 0) {
			break;
		}
		sin = (struct sockaddr_in *)&ifalias.ifra_addr;
		printf("\tinet %s ", inet_ntoa(sin->sin_addr));

		sin = (struct sockaddr_in *)&ifalias.ifra_mask;
		printf("netmask %#x ", ntohl(sin->sin_addr.s_addr));

		if (flags & IFF_BROADCAST) {
			sin = (struct sockaddr_in *)&ifalias.ifra_broadaddr;
			if (sin->sin_addr.s_addr != 0)
				printf("broadcast %s",
				       inet_ntoa(sin->sin_addr));
		}
		putchar('\n');
	}
}

void
Perror(char *cmd)
{
	extern int errno;

	fprintf(stderr, "ifconfig: ");
	switch (errno) {

	case ENXIO:
		fprintf(stderr, "%s: no such interface\n", cmd);
		break;

	case EPERM:
		fprintf(stderr, "%s: permission denied\n", cmd);
		break;

	default:
		perror(cmd);
	}
	exit(1);
}

void
in_getaddr(char *s, int which)
{
	register struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;
	int val;

	if (which != MASK)
		sin->sin_family = AF_INET;

	val = inet_addr(s);
	if (val != -1) {
		sin->sin_addr.s_addr = val;
		return;
	}
#ifdef sgi
	/*
	 * accept 0xffffffff as a valid address
	 */
	if (!strcmp("255.255.255.255",s) ||
		(strtoul(s, (char **)NULL, 0) == (unsigned long)0xffffffff)) {
			sin->sin_addr.s_addr = val;
			return;
	}
#endif /* sgi */
	hp = gethostbyname(s);
	if (hp) {
		sin->sin_family = hp->h_addrtype;	/* What about this? */
		bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
		return;
	}
	np = getnetbyname(s);
	if (np) {
		sin->sin_family = np->n_addrtype;	/* What about this? */
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
		return;
	}
	fprintf(stderr, "%s: bad value\n", s);
	exit(1);
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(char *s, __uint32_t v, char *bits)
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
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
					;
		}
		putchar('>');
	}
}

#ifdef ALIAS_DEBUG

static char *affamily[] = {
	"AF_UNSPEC",	/* 0 unspecified */
	"AF_UNIX",	/* 1 local to host (pipes, portals) */
	"AF_INET",	/* 2 internetwork: UDP, TCP, etc. */
	"AF_IMPLINK",	/* 3 arpanet imp addresses */
	"AF_PUP",	/* 4 pup protocols: e.g. BSP */
	"AF_CHAOS",	/* 5 mit CHAOS protocols */
	"AF_NS",	/* 6 XEROX NS protocols */
	"AF_ISO",	/* 7 ISO protocols */
	"AF_ECMA",	/* 8 European computer manufacturers */
	"AF_DATAKIT",	/* 9 datakit protocols */
	"AF_CCITT",	/* 10 CCITT protocols, X.25 etc */
	"AF_SNA",	/* 11 IBM SNA */
	"AF_DECnet",	/* 12 DECnet */
	"AF_DLI",	/* 13 DEC Direct data link interface */
	"AF_LAT",	/* 14 LAT */
	"AF_HYLINK",	/* 15 NSC Hyperchannel */
	"AF_APPLETALK",	/* 16 Apple Talk */
	"AF_ROUTE",	/* 17 Internal Routing Protocol */
	"AF_RAW",	/* 18 Raw link layer interface */
	"pseudo_AF_XTP", /* 19 eXpress Transfer Protocol (no AF) */

			 /* MIPS ABI VALUES - unimplemented */
	 "AF_NIT",	/* 17 Network Interface Tap */
	 "AF_802",	/* 18 IEEE 802.2, also ISO 8802 */
	 "AF_OSI",	/* 19 umbrella for all families used */
	 "AF_X25",	/* 20 CCITT X.25 in particular */
	 "AF_OSINET",	/* 21 AFI = 47, IDI = 4 */
	 "AF_GOSIP",	/* 22 U.S. Government OSI */
	 "AF_SDL",	/* 23 SGI Data Link for DLPI */
	 0
};
#define SA_MAX_FAMILY	24	/* size of affamily string array of pointers */

/*
 * Stores a printable string version of an internet address
 * into the buffer 'b' of lenth 'len'.
 */
void
in_addr_to_string(struct in_addr *in, char *b)
{
	u_char n, *p;
	int i;

	p = (u_char *)in;
	for (i = 0; i < 4; i++) {
		if (i)
			*b++ = '.';
		n = *p;
		if (n > 99) {
			*b++ = '0' + (n / 100);
			n %= 100;
		}
		if (*p > 9) {
			*b++ = '0' + (n / 10);
			n %= 10;
		}
		*b++ = '0' + n;
		p++;
	}
	*b = 0;
	return;
}

void
print_sockaddr(struct sockaddr *sap)
{
	register u_short i, port;
	char buf[20];

	printf("    sockaddr 0x%x sa_family 0x%x ", sap, sap->sa_family);
	if (sap->sa_family < SA_MAX_FAMILY) {
			printf("%s\n", affamily[sap->sa_family]);
	} else {
			printf("Invalid sa_family\n");
	}

	printf("    sa_data ");
	for (i=0; i< (sizeof(struct sockaddr) - sizeof(u_short)); i++) {
		printf("%x", sap->sa_data[i]);
	}
	in_addr_to_string((&((struct sockaddr_in *)sap)->sin_addr), buf);
	port = ((struct sockaddr_in *)sap)->sin_port;
	printf(" port 0x%x(%d), ip addr %s\n", port, port, buf);
}
#endif /* ALIAS_DEBUG */


/* XXX this hack needs to be removed post 6.5 */
/*
 * Handle failure of new ioctls when running on old kernel, possibly due to
 * autoconfig failure.  We need to allow the administrator to bring the 
 * network up correctly for recovery purposes.
 *
 * Due to capabilities, this doesn't really work for SIOCSIFFLAGS, so we
 * rely on an SIOCGIFFLAGS ioctl being performed first.  A massive
 * restructuring of ifconfig might break this, but that seems unlikely to
 * occur.
 */
int
Ioctl(int fd, int cmd, struct ifreq *ifr)
{
	int r;

retry:
	r = ioctl(fd, cmd, (char *)ifr);
	if (r == -1) {
		if (errno == EINVAL) {
			switch (cmd) {
			case SIOCSIFFLAGS:
				cmd = OSIOCSIFFLAGS;
				goto swiz;
			case SIOCGIFFLAGS:
				cmd = OSIOCGIFFLAGS;
swiz:
				siocsifflags = OSIOCSIFFLAGS;
				siocgifflags = OSIOCGIFFLAGS;
				goto retry;
			default:
				break;
			}
		}
	}
	return r;
}
