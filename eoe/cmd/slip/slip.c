/* IRIX SLIP deamon
 */

#ident "$Revision: 1.50 $"


#include <unistd.h>
#include <stdlib.h>
#include <values.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stropts.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>

#include <sys/if_sl.h>

#include <netdb.h>
#include <net/if.h>
#include <net/raw.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <protocols/routed.h>

#define DEFINE
#include "pputil.h"

struct dev dv;
struct dev *dp0 = &dv;


/* This program runs in one of two modes.  As the caller, it looks for
 *	the callee in the UUCP control files, dials it, identifies itself,
 *	and starts SLIPing.  As the callee, when it is often run as if it
 *	were a 'shell,' it is given the address of caller either as
 *	an argument or in a single line of text from the caller.  The
 *	callee then attaches its standard input and starts to SLIP.
 *
 *	This code includes the necessary equivalent of ifconfig(1M).
 */



static int noicmp;			/* >0 if dropping ICMP */

static int ccount;			/* count of connections */

static struct afilter *afilter;

static int proto = SC_STD;

static int mtu = 0;			/* limit interactive latency */
static int cur_mtu;

static char *remhost_nam;
static char *lochost_nam;


static void usage(void);
static void badhost(char*, char*);
static void make_mod(void);
static void init_afilter(void);
static void moredebug();
static void lessdebug();


void
main(int argc, char **argv)
{
	int i;
	long l;
	char *p;
	struct hostent *hp;
	struct timeval timer;
	time_t c_dly;
	int was_idle;
	fd_set in;
	int fds;
	char b;
	struct servent *sp;
	char ansbuf[128];


	if (0 != (p = strrchr(argv[0], '/')))
		pgmname = p+1;
	else
		pgmname = argv[0];

	dv.callmode = UNKNOWN_CALL;
	dv.devfd = -1;
	dv.dev_index = -1;
	dv.devfd_save = -1;
	dv.rendpid = -1;
	clr_acct(&dv);
	dv.modwait = DEFAULT_ASYNC_MODWAIT;
	dv.modtries = DEFAULT_MODTRIES;

	ctty = open("/dev/tty", O_RDONLY|O_NDELAY);
	if (isatty(stderrfd))
		interact = 1;
	else
		no_interact();
	openlog(pgmname, LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_DAEMON);

	opterr = 0;
	while (i = getopt(argc,argv,"dcIm:M:s:A:P:T:R:S:p:u:l:r:ioq"),
	       i != EOF)
		switch (i) {
		case 'd':
			++debug;
			SET_Debug();
			break;

		case 'c':
			if (dv.callmode == CALLEE) {
				log_complain("","cannot camp as callee");
			} else {
				camping = 1;
			}
			break;

		case 'I':
			noicmp = 1;
			break;

		case 'm':
			if (inet_aton(optarg, &netmask.sin_addr) <= 0) {
				log_complain("","bad netmask \"%s\"",
					     optarg);
				netmask.sin_addr.s_addr = 0;
			}
			break;

		case 'M':
			i = strtol(optarg,&p,0);
			if (*p != '\0'
			    || i < 0
			    || i >= HOPCNT_INFINITY) {
				log_complain("","bad metric \"%s\"",
					     optarg);
			} else {
				metric = i;
			}
			break;

		case 's':
			i = strtol(optarg,&p,0);
			if (*p != '\0' || i < 256 || i > 16384) {
				log_complain("","bad MTU \"%s\"", optarg);
			} else {
				cur_mtu = mtu = i;
			}
			break;

		case 'A':
			init_afilter();
			sactime = strtol(optarg,&p,0);
			if (*p == ',')
				lactime = strtol(p+1,&p,0);
			else
				lactime = sactime;
			if (*p != '\0' || sactime < MIN(DEFAULT_ASYNC_MODWAIT,
							BEEP)
			    || lactime < sactime) {
				log_complain("","bad activity timer \"%s\"",
					     optarg);
				lactime = DEFAULT_LACTIME;
				sactime = DEFAULT_SACTIME;
			}
			break;

		case 'P':
			init_afilter();
			if (optarg[0] == '-' && optarg[1] == '\0') {
				bzero(afilter->port,sizeof(afilter->port));
			} else if (0 != (sp = getservbyname(optarg, "tcp"))) {
				SET_PORT(afilter, sp->s_port);
			} else if (0 != (sp = getservbyname(optarg, "udp"))) {
				SET_PORT(afilter, sp->s_port);
			} else {
				i = strtol(optarg,&p,0);
				if (*p != '\0' || i < 1 || i > 65535) {
					log_complain("","bad port \"%s\"",
						     optarg);
				} else {
					SET_PORT(afilter, i);
				}
			}
			break;

		case 'T':
			init_afilter();
			if (optarg[0] == '-' && optarg[1] == '\0') {
				bzero(afilter->icmp,sizeof(afilter->icmp));
			} else {
				i = strtol(optarg,&p,0);
				if (*p != '\0' || i < 0 || i > ICMP_MAXTYPE) {
					log_complain("",
						     "bad ICMP type \"%s\"",
						     optarg);
				} else {
					SET_ICMP(afilter, i);
				}
			}
			break;

		case 'R':
			if (add_route) {
				log_complain("","only one -R permitted");
			} else {
				add_route = optarg;
			}
			break;

		case 'S':
			if (num_smods >= MAX_SMODS) {
				log_complain("","too many -S arguments");
			} else {
				smods[num_smods++] = optarg;
			}
			break;

		case 'p':
			if (!strcmp(optarg,"std")) {
				proto = SC_STD;
			} else if (!strcmp(optarg,"cksum")) {
				proto = SC_CKSUM;
			} else if (!strcmp(optarg,"comp")) {
				proto = SC_COMP;
			} else if (!strcmp(optarg,"cslip")) {
				proto = SC_CSLIP;
			} else {
				proto = -1;
			}
			switch (proto) {
			case SC_STD:
			case SC_CKSUM:
			case SC_COMP:
			case SC_CSLIP:
				break;
			default:
				log_complain("","bad protocol \"%s\"",
					     optarg);
			}
			break;

		case 'u':
			if (dv.uucp_nam[0] != '\0')
				usage();
			(void)strncpy(dv.uucp_nam, optarg,
				      sizeof(dv.uucp_nam)-1);
			break;

		case 'l':
			if (lochost_nam != 0)
				usage();
			lochost_nam = optarg;
			break;

		case 'r':
			if (remote != 0)
				usage();
			remote = optarg;
			break;

		case 'i':
			if (dv.callmode != UNKNOWN_CALL)
				usage();
			if (camping)
				log_complain("","cannot camp as callee");
			dv.callmode = CALLEE;
			no_interact();
			break;

		case 'o':
			if (dv.callmode != UNKNOWN_CALL)
				usage();
			dv.callmode = CALLER;
			break;

		case 'q':
			if (dv.callmode != UNKNOWN_CALL)
				usage();
			dv.callmode = Q_CALLER;
			init_afilter();
			break;

		default:
			usage();
	}
	if (argc != optind)
		usage();

	if (debug)
		kludge_stderr();
	if (geteuid() != 0) {
		log_complain("","requires UID 0");
		exit(1);
	}

	dv.sync = SYNC_OFF;
	dv.conf_sync = SYNC_OFF;

	if (dv.callmode == UNKNOWN_CALL)
		dv.callmode = (afilter != 0) ? Q_CALLER : CALLER;
	if (dv.callmode == Q_CALLER && sactime == 0) {
		lactime = DEFAULT_LACTIME;
		sactime = DEFAULT_SACTIME;
	}
	if (dv.callmode != Q_CALLER && afilter != 0) {
		log_complain("","cannot use -A, -P, or -T except with -q");
		free(afilter);
		afilter = 0;
	}
	demand_dial = (sactime != 0);	/* XXX */

	if (lochost_nam == 0) {
		/* use our main name for the near side */
		static char ourhost_nam[MAXHOSTNAMELEN];

		if (0 > gethostname(&ourhost_nam[0], sizeof(ourhost_nam))) {
			log_errno("gethostnam","");
			exit(1);
		}
		lochost_nam = &ourhost_nam[0];
	}
	hp = gethostbyname(lochost_nam);
	if (0 == hp)
		badhost("local", lochost_nam);
	lochost.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, &lochost.sin_addr, sizeof(lochost.sin_addr));

	if (dv.uucp_nam[0] == '\0') {
		if (remote == 0)
			usage();
		(void)strncpy(dv.uucp_nam, remote,
			      sizeof(dv.uucp_nam)-1);
	}
	if (remote == 0) {
		if (dv.uucp_nam[0] == '\0')
			usage();
		remote = dv.uucp_nam;
	}

	hp = gethostbyname(remhost_nam = remote);
	if (0 == hp)
		badhost("remote", remhost_nam);
	remhost.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, &remhost.sin_addr, sizeof(remhost.sin_addr));

	if (add_route != 0
	    && (add_route[0] == '\0'
		|| (add_route[0] == '-' && add_route[1] == '\0'))) {
#ifdef SLIP_IRIX_53
		add_route = malloc(sizeof("add default x 1")
				   +strlen(remhost_nam));
#else
		add_route = malloc(sizeof("add default x")
				   +strlen(remhost_nam));
#endif
		(void)strcpy(add_route, "add default ");
		(void)strcat(add_route, remhost_nam);
#ifdef SLIP_IRIX_53
		(void)strcat(add_route, " 1");
#endif
	}

	if (stderrpid <= 0)
		stderrfd = -1;		/* start complaining to system log */

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, cleanup);
	(void)signal(SIGTERM, cleanup);
	(void)signal(SIGUSR1, moredebug);
	(void)signal(SIGUSR2, lessdebug);

	for (;;) {
		make_mod();

		closefds();		/* avoid signals from the terminal */

		if (dv.callmode == Q_CALLER) {
			goto shut;

		} else if (dv.callmode == RE_CALLEE
			   || dv.callmode == CALLEE) {
			if (!rdy_tty_dev(&dv))	/* get the device ready */
				goto shut;

		} else if (dv.callmode == CALLER
			   || dv.callmode == RE_CALLER) {
			i = get_conn(&dv,1);
			if (!i) {
				log_complain("","giving up for now");
				if (0 > ioctl(modfd, TCFLSH, 2))
					log_errno("ioctl(modfd,TCFLSH,2)","");
				/* flush attention getters */
				for (;;) {
					i = read(modfd,ansbuf,sizeof(ansbuf));
					if (i <= 0) {
						if (errno != EAGAIN)
							log_errno("clear read(modfd)", "");
						goto shut;
					}
				}
			}
			if (i == 2) {	/* rendezvoused */
				continue;
			}

			if (!set_tty_modes(&dv))
				goto shut;

			/* If debugging, log the banner or anything else
			 * from the remote machine.  Read the banner even
			 * if not debugging to be consistent and predictable.
			 */
			for (l = 0; l <= 3*sizeof(ansbuf); ) {
				timer.tv_sec = 1;
				timer.tv_usec = 1000000/20;
				FD_ZERO(&in);
				FD_SET(dv.devfd, &in);
				if (0 > select(dv.devfd+1, &in,0,0, &timer)
				    && errno != EINTR)
					log_errno("banner select()","");
				i = read(dv.devfd,ansbuf,sizeof(ansbuf));
				if (i <= 0) {
					if (i < 0)
						log_errno("read(banner)","");
					break;
				}
				if (!Debug)
					continue;
				if (l == 0)
					fputs("banner: ",stderr);
				for (p = ansbuf; p < &ansbuf[i]; p++, l++) {
					*p = toascii(*p);
					if (iscntrl(*p))
						(void)fprintf(stderr,
							      "^%c",
							      (char)(*p+'@'));
					else
						(void)fprintf(stderr,"%c",*p);
				}
			}
			if (l != 0)
				putc('\n',stderr);
			(void)fflush(stderr);

			if (!rdy_tty_dev(&dv))	/* get the device ready */
				goto shut;
		}

		++ccount;
		if (demand_dial || debug > 0)
			log_complain("",
				     ((ccount > 1)
				      ? "restart %s on %s using %s protocol"
				      : "start %s on %s using %s protocol"),
				     remote, dv.line,
				     ((proto == SC_STD) ? "std"
				      : ((proto == SC_CKSUM) ? "cksum"
					 : ((proto == SC_COMP) ? "comp"
					    : "cslip"))));

		/* Link the device STREAMS stack to the driver.
		 */
		dv.dev_index = ioctl(modfd,I_LINK,dv.devfd);
		if (dv.dev_index < 0) {
			log_errno("I_LINK","");
			goto shut;
		}
		dv.devfd = -1;

		/* set SLIP parameters
		 */
		dv.devtio.c_lflag = proto;
		dv.devtio.c_oflag = (noicmp ? SC_NOICMP : 0);
#if defined(SLIP_IRIX_62) || defined(SLIP_IRIX_53)
		if (mtu == 0 && (dv.devtio.c_cflag&CBAUD) == B38400)
#else
		if (mtu == 0 && dv.devtio.c_ospeed >= B38400)
#endif
			cur_mtu = 1500;
		dv.devtio.c_cc[VMIN] = (cur_mtu>>8) & 0xff;
		dv.devtio.c_cc[VTIME] = cur_mtu & 0xff;
		if (0 > ioctl(modfd, TCSETA, (char*)&dv.devtio)) {
			log_errno("TCSETA-on","");
			goto shut;
		}

		/* Wait forever, or until the line dies.
		 */
		was_idle = 0;
		act_devs(0,0);
		fds = 1+MAX(modfd,rendnode);
		for (;;) {
			FD_ZERO(&in);
			FD_SET(modfd, &in);
			FD_SET(rendnode, &in);
			timer.tv_usec = 0;
			if (sactime == 0) {
				timer.tv_sec = HEARTBEAT;
			} else {
				timer.tv_sec = dv.active - get_clk();
				/* defend against time changes */
				if (timer.tv_sec < 0)
					timer.tv_sec = 0;
				else if (timer.tv_sec > HEARTBEAT)
					timer.tv_sec = HEARTBEAT;
			}
			i = select(fds, &in,0,0, &timer);
			if (i < 0) {
				if (errno == EINTR)
					continue;
				if (errno != EAGAIN)
					bad_errno("select()","");
				i = 0;
			}

			c_dly = get_clk() - dv.acct.sconn;
			if (dv.callmode == CALLEE
			    || dv.callmode == RE_CALLEE) {
				dv.acct.ans_conn += c_dly;
				if (i == 0 || was_idle)
					dv.acct.ans_idle += c_dly;
			} else {
				dv.acct.orig_conn += c_dly;
				if (i == 0 || was_idle)
					dv.acct.orig_idle += c_dly;
			}
			dv.acct.sconn = clk.tv_sec;
			ck_acct(&dv,0);

			/* If things are quiet, maybe we should shut down
			 * the device.
			 */
			if (i == 0) {
				was_idle = 1;
				if (sactime != 0
				    && clk.tv_sec > dv.active) {
					log_debug(1,"",
						  "%s activity timer expired",
						  dv.line);
					break;
				}
			} else if (was_idle) {
				/* count idleness in previous period */
				was_idle = 0;
			}

			if (FD_ISSET(modfd,&in)) {
				/* see what the kernel has to say */
				if (1 != read(modfd, &b, 1))
					bad_errno("active read()","");
				if (b == BEEP_DEAD) {
					log_debug(1,"","line off");
					break;
				} else if (b == BEEP_ACTIVE) {
					log_debug(2,"","TCP/IP active");
					act_devs(0,1);
				} else if (b == BEEP_WEAK) {
					log_debug(2,"","IP active");
					act_devs(1,1);
				} else {
					log_complain("","wild beep %#x", b);
				}
			}

			if (FD_ISSET(rendnode,&in))
				(void)rendezvous(0);
		}

		if (dv.callmode == CALLEE || dv.callmode == RE_CALLEE)
			dv.acct.ans_conn += clk.tv_sec - dv.acct.sconn;
		else
			dv.acct.orig_conn += clk.tv_sec - dv.acct.sconn;
shut:
		if (rendnode >= 0 && demand_dial) {
			rel_dev(&dv);

			c_dly = get_clk() + dv.modwait;
			for (;;) {
				(void)get_clk();

				/* Check that the socket has not been removed.
				 * Quit if some other process owns the link.
				 */
				if (make_rend(1)) {
					log_complain("","link stolen");
					unmade_rend();
					cleanup(1);
				}

				ck_acct(dp0,0);

				FD_ZERO(&in);
				FD_SET(rendnode, &in);
				timer.tv_usec = 0;
				if (clk.tv_sec >= c_dly) {
					/* listen for new traffic
					 * after the modem has cooled
					 */
					timer.tv_sec = HEARTBEAT;
					FD_SET(modfd, &in);
				} else {
					timer.tv_sec = c_dly - clk.tv_sec;
				}
				fds = 1+MAX(rendnode,modfd);
				i = select(fds,&in,0,0, &timer);
				if (i < 0) {
					if (errno == EINTR)
						continue;
					if (errno != EAGAIN)
						bad_errno("select()","");
					i = 0;
				}
				dv.acct.sconn = get_clk();

				if (i > 0) {
					if (FD_ISSET(modfd,&in)) {
					    /* see what kernel has to say */
						if (1 != read(modfd, &b, 1))
						    bad_errno("sleep read()",
							      "");
						log_debug(2,"",
							  "read %#02x", b);
					    if (b == BEEP_ACTIVE
						|| b == BEEP_WEAK) {
						dv.callmode = RE_CALLER;
						break;
					    } else {
						log_complain("",
							     "odd beep %#x",
							     b);
					    }
					}

					if (FD_ISSET(rendnode,&in)
					    && 0 != rendezvous(1)) {
						break;
					}
				}
			}

		} else if (camping) {
			/* If camping, try to reopen line.  First make the
			 * interface go away for the rest of the system
			 */
			modfd = -1;
			rel_dev(&dv);
			(void)sleep(dv.modwait);    /* let the modem cool */

		} else {
			/* must be simple, one-shot CALLEE */
			cleanup(0);
		}
	}
}



static void
usage(void)
{
	kludge_stderr();
	(void)fprintf(stderr, "usage: [-dcI] [-m netmask] "
		      "[-M metric] [-s mtu] [-A secs[,secs]]\n   "
		      "[-P port] [-T icmp_type] [-R \"routecmd\"] "
		      "[-S smods]\n   [-p proto] [-u uucp] [-l local] "
		      "-r remote {-i|-o|-q}\n");
	exit(1);
}


static void
badhost(char *msg, char *nam)
{
	kludge_stderr();
	(void)fprintf(stderr, "%s: %s %s: %s\n",
		      pgmname, msg, nam, hstrerror(h_errno));
	exit(1);
}


/* create the protocol streams module/driver */
static void
make_mod(void)
{
#	define RCMDV "route -n    "
#	define RCMD  "route -n -q "
	char *rcmd;
	struct stat sbuf;
	char newline[BUFSIZ];

	if (modfd >= 0)
		return;

	init_rand();

	if (dv.devfd < 0 && dv.callmode == CALLEE)
		grab_dev("starting SLIP", &dv);

	(void)add_rend_name("",inet_ntoa(remhost.sin_addr));
	if (make_rend(1)) {
		/* poke the existing daemon and wait for it
		 * to kill us
		 */
		log_debug(1,"","waiting to be killed");
		no_signals(killed);
		(void)sprintf(newline, "%.55s pid=%ld", dv.line, getpid());
		if (0 > write(rendnode, newline, strlen(newline)+1))
			bad_errno("write(caller poke) ",dv.line);
		rends[0].made = 0;
		rendnode = -1;
		closefds();
		for (;;)
			(void)pause();
	}

	modfd = open("/dev/if_sl", O_RDWR | O_NONBLOCK);
	if (0 > modfd)
		bad_errno("open()","/dev/if_sl");

	/* get the unit number */
	if (0 > fstat(modfd, &sbuf))
		bad_errno("fstat(unit#)","");
	dv.unit = minor(sbuf.st_rdev);
	(void)sprintf(ifname, "sl%d", dv.unit);

	set_ip();

	if (afilter != 0) {
		int soc;
		struct sockaddr_raw sr;

		soc = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP);
		if (soc < 0)
			bad_errno("raw socket()","");

		bzero(&sr, sizeof(sr));
		(void)strncpy(sr.sr_ifname, ifname, sizeof(sr.sr_ifname));
		sr.sr_family = AF_RAW;
		sr.sr_port = 0;
		if (0 > bind(soc, &sr, sizeof(sr))) {
			perror("raw bind()");
			cleanup(1);
		}

		if (0 > ioctl(soc, SIOC_SL_AFILTER, &afilter))
			bad_errno("SIOC_SL_AFILTER","");

		(void)close(soc);
	}

	/* Install a default route if asked.
	 *	Decide each time whether to be verbose.
	 */
	if (add_route != 0) {
		rcmd = malloc(strlen(RCMD)+strlen(add_route)+1);
		sprintf(rcmd, "%s%s", debug ? RCMDV : RCMD, add_route);
		call_system("", "", rcmd,0);
		free(rcmd);
	}
}


/* see what the other daemon wants
 */
int					/* 1=took his device */
rendezvous(int mode)			/* 0=kill him.  1=take his device */
{
	char newlockfile[MAXPATHLEN];	/* lockfile name */
	char newline[BUFSIZ];
	char newnode[sizeof(dv.nodename)];
	int newfd;
	FILE *lf;
	pid_t pid, ckpid;
	int i;
	extern void stlock(char*);
	extern void sethup(int);

	i = read(rendnode,newline,sizeof(newline));
	if (i < 0) {
		log_errno("read(rendnode)","");
		return 0;
	}

	if (i < 5+1+1+4+1) {
		log_complain("","bogus short rendezvous message \"%s\"",
			     newline);
		return 0;
	}

	if (2 != sscanf(newline, "/dev/%50s pid=%ld", &newnode[0], &pid)) {
		log_complain("","bogus rendezvous message \"%s\"",
			     newline);
		return 0;
	}
	newline[strlen(newnode)+5] = '\0';

	(void)lockname(newnode, newlockfile, sizeof(newlockfile));
	lf = fopen(newlockfile,"r+");
	if (!lf) {
		log_errno("fopen(rendezvous) lock file ", newlockfile);
		return 0;
	}
	if (1 != fscanf(lf, "%ld", &ckpid)
	    || ckpid != pid) {
		log_complain("","bogus rendezvous lock file: %s",
			     newlockfile);
		(void)kill(pid,SIGINT);
		(void)fclose(lf);
		return 0;
	}

	/* If we do not need another port, kill the other guy and unlock
	 * his device.
	 */
	if (!mode) {
		(void)kill(pid, SIGINT);
		(void)unlink(newlockfile);
		(void)fclose(lf);
		log_debug(1,"","refuse to rendezvous with pid %d for %s",
			  pid,newline);
		return 0;
	}

	/* Steal his device.
	 */
	newfd = open(newline,O_RDWR|O_NDELAY,0);
	if (newfd < 0) {
		log_errno("failed to steal ", newline);
		(void)kill(pid, SIGINT);
		return 0;
	}
	sethup(newfd);

	(void)rewind(lf);
	(void)fprintf(lf, LOCKPID_PAT, getpid());
	stlock(newlockfile);
	(void)fclose(lf);

	/* remember to kill him */
	dv.rendpid = pid;
	dv.devfd = newfd;
	dv.dev_index = -1;
	dv.devfd_save = -1;
	(void)strncpy(dv.line,newline,sizeof(dv.line));
	(void)strncpy(dv.nodename,newnode,sizeof(dv.nodename));
	(void)strncpy(dv.lockfile,newlockfile,sizeof(dv.lockfile));
	dv.acct.sconn = get_clk();
	dv.acct.answered++;
	dv.callmode = RE_CALLEE;

	log_debug(1,"","took %s from %d", dv.line, pid);
	return 1;
}


/* create an initial stab at the activity filter
 */
static void
init_afilter(void)
{
	static struct plist {
		char	*name;
		int	num;
	} ports[] = {
		{"daytime", 13},
		{"time",    37},
		{"ntp",	    123},
		{"route",   520},
		{"timed",   525},
		{"",	    0}
	};
	struct plist *pp;
	struct servent *sp;


	if (afilter != 0)
		return;

	afilter = (struct afilter*)malloc(sizeof(*afilter));
	bzero(afilter, sizeof(*afilter));

	SET_ICMP(afilter, ICMP_UNREACH);
	SET_ICMP(afilter, ICMP_SOURCEQUENCH);
	SET_ICMP(afilter, ICMP_TSTAMP);
	SET_ICMP(afilter, ICMP_TSTAMPREPLY);

	for (pp = &ports[0]; pp->name[0] != '\0'; pp++) {
		sp = getservbyname(pp->name, "tcp");
		if (!sp)
			sp = getservbyname(pp->name, "udp");
		if (!sp)
			SET_PORT(afilter, pp->num);
		else
			SET_PORT(afilter, sp->s_port);
	}
}

/* dummy since SLIP does not log in or out
 */
/* ARGSUSED */
void
dologout(struct dev *dp)
{
}


/* change debugging
 */
static void
moredebug()
{
	int soc;
	struct ifreq ifr;

	++debug;
	SET_Debug();

	if (debug != 0)
		kludge_stderr();

	if (debug > 1 &&ifname[0] != '\0') {
		soc = socket(AF_INET, SOCK_DGRAM, 0);
		if (soc < 0)
			log_errno("moredebug socket()","");
		(void)strncpy(ifr.ifr_name, ifname,
			      sizeof(ifr.ifr_name));
		if (0 > ioctl(soc, SIOCGIFFLAGS, (char*)&ifr))
			log_errno("moredebug SIOCGIFFLAGS","");
		ifr.ifr_flags |= IFF_DEBUG;
		(void)strncpy(ifr.ifr_name, ifname,
			      sizeof(ifr.ifr_name));
		if (0 > ioctl(soc, SIOCSIFFLAGS, (char*)&ifr))
			log_errno("moredebug SIOCSIFFLAGS","");
	}

	log_complain("","debug=%d", debug);
	(void)signal(SIGUSR1, moredebug);
}

static void
lessdebug()
{
	int soc;
	struct ifreq ifr;

	if (--debug < 1)
		debug = 0;
	SET_Debug();

	if (debug <= 1 && ifname[0] != '\0') {
		soc = socket(AF_INET, SOCK_DGRAM, 0);
		if (soc < 0)
			log_errno("lessdebug socket()","");
		(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (0 > ioctl(soc, SIOCGIFFLAGS, (char*)&ifr))
			log_errno("lessdebug SIOCGIFFLAGS","");
		ifr.ifr_flags &= ~IFF_DEBUG;
		(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (0 > ioctl(soc, SIOCSIFFLAGS, (char*)&ifr))
			log_errno("lessdebug SIOCSIFFLAGS","");
	}

	log_complain("","debug=%d", debug);
	(void)signal(SIGUSR2, lessdebug);
}
