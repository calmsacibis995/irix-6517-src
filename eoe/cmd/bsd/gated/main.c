/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 */

#ifndef sgi
#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/main.c,v 1.8 1997/02/27 10:52:16 bitbug Exp $";
#endif	not lint
#endif

/* main.c
 *
 * Main function of the EGP user process. Includes packet reception,
 * ICMP handler and timer interrupt handler.
 *
 * Functions: main, recvpkt, icmpin, timeout, getod, quit, p_error
 */
/*
 * Software Overview:
 *
 * At start up, the controlling function, main(), first sets tracing options
 * based on starting command arguments. It then calls the initialization
 * functions described in the next section, sets up signal handlers for
 * termination (SIGTERM) and timer interrupt (SIGALRM) signals, calls
 * timeout() to start periodic interrupt processing, and finally waits in a 
 * loop to receive incoming EGP or ICMP redirect packets
 *
 * When an EGP packet is received, egpin() is called to handle it. It in turn
 * calls a separate function for each EGP command type, which sends the
 * appropriate response. When an ICMP packet is received icmpin() is called.
 *
 * The timer interrupt routine, timeout(), calls egpjob() to perform periodic
 * EGP processing such as reachability determination, command sending and
 * retransmission. It in turn calls separate functions to format each command
 * type. Timeout() also periodically calls rt_time() to increment route ages 
 * and delete routes when too old.
 */

#include "include.h"
#include <sys/capability.h>

#ifndef	NSS
struct rip *ripmsg = (struct rip *)rip_packet;
SIGTYPE timeout();
SIGTYPE set_reinit_flag();
#endif	NSS
FILE *conf_open();
int no_config_file;

SIGTYPE setdumpflg();
SIGTYPE chgtrace();
extern SIGTYPE egpallcease();

#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)
extern int snmp_init();
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)

#ifdef	AGENT_SNMP
extern int snmpin();
extern int register_snmp_var();
#endif	AGENT_SNMP

#ifdef	AGENT_SGMP
extern int sgmpin();
extern int register_sgmp_var();
#endif	AGENT_SGMP

#ifndef vax11c
#ifdef	NSS
gated_init(argc, argv)
#else	NSS
main(argc, argv)
#endif	NSS
#else	vax11c
gw_main(argc,argv)
#endif	vax11c
	int   argc;
	char *argv[];
{
#ifndef vax11c
#if !defined(sgi) || (sgi && _BSD_SIGNALS)
  struct sigvec vec, ovec;
#endif
#ifdef sgi
  int daemon_arg = _DF_NOCHDIR;
#endif
  int	selectbits,
        forever = TRUE,
        error = FALSE,
        arg_n = 0;
#else	vax11c
  int	i;
#endif	vax11c
  char	*cp;
  FILE  *fp;
  cap_t ocap;
  cap_value_t cap_priv_port = CAP_PRIV_PORT;

  getod();		/* start time */

  if ( !(my_hostname = calloc(MAXHOSTNAMELENGTH+1, sizeof(char))) ) {
    p_error("main: calloc");
    quit();
  }
  if ( gethostname(my_hostname, MAXHOSTNAMELENGTH+1) ) {
    p_error("main: gethostname");
    quit();
  }
  	
  /* check arguments for turning on tracing and a trace file */

  my_name = argv[0];
  if (cp = rindex(my_name, '/')) {
    my_name = cp + 1;
  }
  tracing = savetrace = are_tracing = 0;
  logfile = NULL;
#ifndef vax11c
  while (--argc > 0 && !error) {
    argv++;
    arg_n++;
    switch (arg_n) {
      case 1:
        cp = *argv;
        if (*cp++ != '-') {
          if (argc > 1) {
            error = TRUE;
          } else {
            logfile = *argv;
          }
        } else if (*cp++ != 't') {
          error = TRUE;
        } else if (*cp == '\0') {
          savetrace = TR_INT | TR_EXT | TR_RT | TR_EGP;
        } else {
          while (*cp != '\0') {
            switch (*cp++) {
              case 'i':
                savetrace |= TR_INT;
                break;
              case 'e':
                savetrace |= TR_EXT;
                break;
              case 'r':
                savetrace |= TR_RT;
                break;
              case 'p':
                savetrace |= TR_EGP;
                break;
              case 'u':
                savetrace |= TR_UPDATE;
                break;
#ifdef	NSS
              case 'I':
                savetrace |= TR_ISIS;
                break;
              case 'E':
                savetrace |= TR_ESIS;
                break;
#else	NSS
              case 'R':
                savetrace |= TR_RIP;
                break;
              case 'H':
                savetrace |= TR_HELLO;
                break;
#endif	NSS
#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)
	      case 'M':
	        savetrace |= TR_SNMP;
	        break;
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)
#ifdef sgi
	      case 'B':
		daemon_arg = _DF_NOFORK | _DF_NOCHDIR;
		break;
#endif
              default:
                error = TRUE;
            }
          }
        }
        break;
      case 2:
        logfile = *argv;
        break;
      default:
        error = TRUE;
    }
  }
  if (error) {
    fprintf(stderr, "Usage: %s [i][e][r][p][u][R][H]] [logfile]\n", my_name);
    exit(1);
  }
  if ((savetrace == 0) || (logfile != NULL)) {
#ifdef sgi
    _daemonize(daemon_arg, -1,-1,-1);
#else /* sgi */
    int t;

    if (fork()) {
      exit(0);
    }
#if BSD42 || UTX32_1_X
    t = 20;
#else BSD42 || UTX32_1_X
    t = getdtablesize();
#endif BSD42 || UTX32_1_X
    for (t--; t >= 0; t--) {
      (void) close(t);
    }
    (void) open("/dev/null", O_RDONLY);
    (void) dup2(0, 1);
    (void) dup2(0, 2);
    t = open("/dev/tty", O_RDWR);
    if (t >= 0) {
      (void) ioctl(t, TIOCNOTTY, (char *)NULL);
      (void) close(t);
    }
#endif /* sgi */
  }
  my_pid = getpid();

#if	defined(SYSLOG_43)
  openlog(my_name, LOG_PID|LOG_CONS|LOG_NDELAY, LOG_FACILITY);
#ifndef	NSS
  (void) setlogmask(LOG_UPTO(LOG_NOTICE));
#endif	NSS
#else	defined(SYSLOG_43)
  openlog(my_name, LOG_PID);
#endif	defined(SYSLOG_43)

  if ( savetrace ) {
    (void) traceon(logfile, GEN_TRACE);
  } else if (savetrace = traceflags()) {
    (void) traceon(logfile, GEN_TRACE);
  }
#else	vax11c
  for(i = 1; i < argc; i++) {
	if (strcasecmp(argv[i],"bootfile") == 0) {
		if (i >= argc) {
			printf("ERROR: No GATED boot file specified!\n");
			return;
		}
		i++;
		EGPINITFILE = argv[i];
		continue;
	}
	if (strcasecmp(argv[i],"trace") == 0) {
		tracing = TR_INT | TR_EXT | TR_RT | TR_EGP;
		continue;
	}
	if (strcasecmp(argv[i],"trace-all") == 0) {
		tracing = TR_INT | TR_EXT | TR_RT | TR_EGP |
			  TR_UPDATE | TR_RIP | TR_HELLO;
		continue;
	}
	if (strcasecmp(argv[i],"trace-internal-errors") == 0) {
		tracing |= TR_INT;
		continue;
	}
	if (strcasecmp(argv[i],"trace-external-changes") == 0) {
		tracing |= TR_EXT;
		continue;
	}
	if (strcasecmp(argv[i],"trace-routing-changes") == 0) {
		tracing |= TR_RT;
		continue;
	}
	if (strcasecmp(argv[i],"trace-packets") == 0) {
		tracing |= TR_EGP;
		continue;
	}
	if (strcasecmp(argv[i],"trace-egp-updates") == 0) {
		tracing |= TR_UPDATE;
		continue;
	}
	if (strcasecmp(argv[i],"trace-rip-updates") == 0) {
		tracing |= TR_RIP;
		continue;
	}
	if (strcasecmp(argv[i],"trace-hello-updates") == 0) {
		tracing |= TR_HELLO;
		continue;
	}
#ifdef	defined(AGENT_SNMP) || defined(AGENT_SGMP)
	if (strcasecmp(argv[i],"trace-snmp") == 0) {
		tracing |= TR_SNMP;
		continue;
	}
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)
	printf("GATED bad arg \"%s\"\n",argv[i]);
	return;
  }
#endif	vax11c
  if (tracing) {
    are_tracing++;
  }

  syslog(LOG_NOTICE, "Start %s version %s", my_name, version);
  TRACE_TRC("Start %s[%d] version %s at %s\n", my_name, my_pid, version, strtime);

/* open initialization file */
  no_config_file = FALSE;
  if ( (fp = conf_open()) == NULL) {
    quit();
  }
#ifndef vax11c
  setnetent(1);
  sethostent(1);
#endif	vax11c
  martian_init();	/* initialize martian net table */
  rt_init();		/* initialize route hash tables */
  init_options(fp);	/* initialize protocol options */
#ifdef	NSS
  init_if(fp);		/* initialize interface tables */
#else	NSS
  init_if();  		/* initialize interface tables */
#endif	NSS
  rt_resinit(fp);	/* initialize routing restriction tables. */
  rt_ASinit(fp, TRUE);	/* initialize AS routing restrictions. */
  rt_NRadvise_init(fp);	/* initialize interior routes to be EGP advised */

#ifdef	NSS
  s = getsocket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    quit();
  }
#else	NSS
  /*
   * only need one socket per protocol type in 4.3bsd
   */
  ocap = cap_acquire(1, &cap_priv_port);
  icmp_socket = getsocket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  cap_surrender(ocap);
  if (icmp_socket < 0) {
    quit();
  }
  if (doing_egp) {
    ocap = cap_acquire(1, &cap_priv_port);
    egp_socket = get_egp_socket(AF_INET, SOCK_RAW, IPPROTO_EGP);
    cap_surrender(ocap);
    if (egp_socket < 0) {
      quit();
    }
  }
  if (doing_hello) {
#ifdef sgi
    int on;
#endif
    ocap = cap_acquire(1, &cap_priv_port);
    hello_socket = getsocket(AF_INET, SOCK_RAW, IPPROTO_HELLO);
    cap_surrender(ocap);
    if (hello_socket < 0) {
      quit();
    }
#ifdef sgi
    on = 48*1024;
    if (setsockopt(hello_socket, SOL_SOCKET, SO_RCVBUF, (char *)&on, sizeof(on)) < 0) {
      p_error("main: setsockopt SO_RCVBUF");
    } else {
      TRACE_TRC("main: HELLO receive buffer size set to %dK\n", on/1024);
    }
    if (setsockopt(hello_socket, SOL_SOCKET, SO_SNDBUF, (char *)&on, sizeof(on)) < 0) {
      p_error("main: setsockopt SO_RSNDBUF");
    } else {
      TRACE_TRC("main: HELLO send buffer size set to %dK\n", on/1024);
    }
#endif
  }
  if (doing_rip)
    rip_socket = rip_init();

  s = icmp_socket;   	/* socket desc. for ioctl calls to install routes */
#endif	NSS

#ifdef AGENT_SNMP
  if ((snmp_socket = snmp_init()) < 0) {
    syslog(LOG_ERR, "main: can't open snmp socket");
    quit();
  }
#endif AGENT_SNMP
#ifdef AGENT_SGMP
  if ((sgmp_socket = snmp_init()) < 0) {
    syslog(LOG_ERR, "main: can't open sgmp socket");
    quit();
  }
#endif AGENT_SGMP

  if (doing_egp) {
    init_egpngh(fp);	/* read egp neighbor list */
    init_egp();		/* initialize EGP neighbor tables */
  }
#ifndef	NSS
  rt_readkernel();	/* Initailize routing tables with kernel's */
  rt_ifoptinit(fp);	/* Initialize options for interfaces */
  rt_ifinit();		/* initialize interior routes for direct nets */

  if (doing_rip && rip_supplier < 0) {
    rip_supplier = FALSE;
  }
  if (doing_hello && hello_supplier < 0) {
    hello_supplier = FALSE;
  }

  if (no_config_file && !rip_supplier && rt_locate( (int)EXTERIOR, &hello_dfltnet, RTPROTO_KERNEL) ) {
    char *s = "No config file, one interface and a default route, gated exiting\n";
    TRACE_TRC(s);
    syslog(LOG_NOTICE,s);
    quit();
  }

  if (doing_rip) {
    ripmsg->rip_cmd = RIPCMD_REQUEST;
    ripmsg->rip_vers = RIPVERSION;
    ripmsg->rip_nets[0].rip_dst.sa_family = AF_UNSPEC;
    ripmsg->rip_nets[0].rip_metric = RIPHOPCNT_INFINITY;
    ripmsg->rip_nets[0].rip_dst.sa_family = htons(AF_UNSPEC);
    ripmsg->rip_nets[0].rip_metric = htonl((u_long)RIPHOPCNT_INFINITY);
    toall(sendripmsg);
  }
#else	NSS
  addrouteforbackbone();
#endif	NSS

  install = TRUE;	/* install routes from now on */

  if (install) {
    TRACE_TRC("\n***Routes are being installed in kernel\n\n");
  } else {
    TRACE_TRC("\n***Routes are not being installed in kernel\n\n");
  }

#ifndef	NSS
  rt_dumbinit(fp);	/* initialize static routes gateways */
#endif	NSS

  (void) fclose(fp);

#ifndef vax11c
#ifdef sgi
  /*
   * No need to create the pid and version file. The pid and version
   * are already logged to syslog. killall(1) does not require gated's pid
   */
#else
  fp = fopen(PIDFILE, "w");
  if (fp != NULL) {
    fprintf(fp, "%d\n", my_pid);
    (void) fclose(fp);
  }

  fp = fopen(VERSIONFILE, "w");
  if (fp != NULL) {
    fprintf(fp, "%s version %s built %s\n\tpid %d, started %s",
      my_name, version, build_date, my_pid, strtime);
    (void) fclose(fp);
  }
#endif /* sgi */

  /* Setup signal processing */
#if !defined(sgi) || (sgi && _BSD_SIGNALS)
  bzero((char *)&vec, sizeof(struct sigvec));
  vec.sv_mask = sigmask(SIGIO) | sigmask(SIGALRM) | sigmask(SIGTERM) | sigmask(SIGUSR1) | sigmask(SIGINT) | sigmask(SIGHUP);
#endif	/* bsd signals */

  /* SIGTERM to terminate */
#if !defined(sgi) || (sgi && _BSD_SIGNALS)
  vec.sv_handler = egpallcease;
  if (error = sigvec(SIGTERM, &vec, &ovec)) {
    p_error("main: sigvec SIGTERM");
    quit();
  }
#else	/* bsd signals */
  sigset(SIGTERM, egpallcease);
#endif	/* bsd signals */

#ifndef	NSS
  /* SIGALRM for route delete processing */
#if !defined(sgi) || (sgi && _BSD_SIGNALS)
  vec.sv_handler = timeout;
  if (error = sigvec(SIGALRM, &vec, &ovec)) {
    p_error("main: sigvec SIGALRM");
    quit();
  }
  /* SIGUSR1 for reconfiguration */
  vec.sv_handler = set_reinit_flag;
  if (error = sigvec(SIGUSR1, &vec, &ovec)) {
    p_error("main: sigvec SIGUSR1");
    quit();
  }
#else   /* bsd signals */
  sigset(SIGALRM, timeout);
  sigset(SIGUSR1, set_reinit_flag);
#endif  /* bsd signals */
#endif	NSS

  /* SIGINT for dumps */
#if !defined(sgi) || (sgi && _BSD_SIGNALS)
  vec.sv_handler = setdumpflg;
  if (error = sigvec(SIGINT, &vec, &ovec)) {
    p_error("main: sigvec SIGINT");
    quit();
  }
  /* SIGHUP for tracing */
  vec.sv_handler = chgtrace;
  if (error = sigvec(SIGHUP, &vec, &ovec)) {
    p_error("main: sigvec SIGHUP");
    quit();
  }
#else	/* bsd signals */
  sigset(SIGINT, setdumpflg);
  sigset(SIGHUP, chgtrace);
#endif  /* bsd signals */
#endif	vax11c

  TRACE_INT("\n");
  init_display_config("main: RIP", doing_rip, rip_supplier, rip_gateway, rip_pointopoint, rip_default);
  init_display_config("main: HELLO", doing_hello, hello_supplier, hello_gateway, hello_pointopoint, hello_default);
  init_display_config("main: EGP", doing_egp, -1, 0, 0, 0);
  TRACE_INT("main: commence routing updates:\n\n");

  timeout();

#ifndef vax11c
#ifndef	NSS
  /* wait to receive HELLO, RIP, EGP, ICMP or IMP messages */

  selectbits = 0;			/* select descriptor mask */
  if (doing_egp)
    selectbits |= 1 << egp_socket;	/* EGP socket */
  selectbits |= 1 << icmp_socket;	/* ICMP socket */
  if (doing_hello)
    selectbits |= 1 << hello_socket;	/* HELLO socket */
  if (doing_rip)
    selectbits |= 1 << rip_socket; 	/* RIP socket */
#ifdef AGENT_SNMP
  selectbits |= 1 << snmp_socket;	/* SNMP socket */
#endif AGENT_SNMP
#ifdef AGENT_SGMP
  selectbits |= 1 << sgmp_socket;	/* SGMP socket */
#endif AGENT_SGMP
#endif	NSS
	
  endnetent();
  endhostent();

#ifndef	NSS
  while (forever) {
    int ibits;
    register int n;

    ibits = selectbits;
    n = select(20, (struct fd_set *)&ibits, (struct fd_set *)0,
                          (struct fd_set *)0, (struct timeval *)0);

    if (n < 0) {
      if (errno != EINTR) {
        p_error("main: select");
        quit();
      }
      continue;
    }
    if (doing_egp && (ibits & (1 << egp_socket)))
      recvpkt(egp_socket, IPPROTO_EGP);
    if (ibits & (1 << icmp_socket))
      recvpkt(icmp_socket, IPPROTO_ICMP);
    if (doing_hello && (ibits & (1 << hello_socket)))
      recvpkt(hello_socket, IPPROTO_HELLO);
    if (doing_rip && (ibits & (1 << rip_socket)))
      recvpkt(rip_socket, IPPROTO_RIP);
#ifdef AGENT_SNMP
    if (ibits & (1 << snmp_socket))
      recvpkt(snmp_socket, IPPROTO_SNMP);
#endif AGENT_SNMP
#ifdef AGENT_SGMP
    if (ibits & (1 << sgmp_socket))
      recvpkt(sgmp_socket, IPPROTO_SGMP);
#endif AGENT_SGMP
  }
#endif	NSS
#else	vax11c
  /*
   *	Wait for data arrival on appropriate sockets
   */
  if (doing_egp) Setup_VMS_Receive(IPPROTO_EGP, egp_socket);
  Setup_VMS_Receive(IPPROTO_ICMP, icmp_socket);
  if (doing_hello) Setup_VMS_Receive(IPPROTO_HELLO, hello_socket);
  if (doing_rip) Setup_VMS_Receive(IPPROTO_RIP, rip_socket);
#ifdef AGENT_SNMP
  Setup_VMS_Receive(IPPROTO_SNMP, snmp_socket);
#endif AGENT_SNMP
#ifdef AGENT_SGMP
  Setup_VMS_Receive(IPPROTO_SGMP, sgmp_socket);
#endif AGENT_SGMP
#endif	vax11c
}

#ifndef	NSS
recvpkt(sock, protocol)
	int sock, protocol;
{
  char packet[SLOP(MAXPACKETSIZE)];  	/* packet buffer */
  struct sockaddr_in from;
  int fromlen = sizeof(from), count, omask;

  getod();			/* current time */
  count = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&from, &fromlen);
  if (count <= 0) {
    if (count < 0 && errno != EINTR)
      p_error("recvpkt: recvfrom");
    return;
  }
  if (fromlen != sizeof (struct sockaddr_in)) {
    syslog(LOG_ERR, "recvpkt: fromlen %d invalid\n\n", fromlen);
    return;
  }
  if (count > sizeof(packet)) {
    syslog(LOG_ERR, "recvfrom: packet discarded, length %d > %d",
                       count, sizeof(packet));
    syslog(LOG_ERR, ", from addr %s\n\n", inet_ntoa(from.sin_addr));
    return;
  }

#ifndef vax11c
#define	mask(s)	(1<<((s)-1))

  omask = sigblock(mask(SIGALRM | SIGTERM));
#endif	vax11c

  switch (protocol) {
    case IPPROTO_EGP:
      if (tracing & TR_EGP) 
        tracercv(sock, protocol, packet, count, &from);
      egpin(packet);
      break;
    case IPPROTO_ICMP:
      icmpin(packet, &from);
      break;
    case IPPROTO_HELLO:
      helloin(packet);
      break;
    case IPPROTO_RIP:
      ripin((struct sockaddr *)&from, count, packet);
      break;
#ifdef AGENT_SNMP
    case IPPROTO_SNMP:
      snmpin(&from, count, packet);
      break;
#endif AGENT_SNMP
#ifdef AGENT_SGMP
    case IPPROTO_SGMP:
      sgmpin(&from, count, packet);
      break;
#endif AGENT_SGMP
#ifndef sgi
    case IMPLINK_IP:
      break;  		/* not implemented - need care with different
                         * address family for imp?
                         */
#endif
  }
#ifndef vax11c
  (void) sigsetmask(omask);
#endif	vax11c
}
#endif	NSS


/*
 * timer control for periodic route-age and interface processing.
 * timeout() is called when the periodic interrupt timer expires.
 */

SIGTYPE timeout()
{
  static unsigned long
      		next_egpjob = 0,
		next_rttime = 0,
#ifndef	NSS
		next_ripjob = 0,
		next_hellojob = 0,
#endif	NSS
		next_timestamp = 0,
		next_iftime = 0;
  register unsigned long interval;
#ifndef	NSS
  struct itimerval  value, ovalue;
#endif	NSS

#if sgi && !defined(_BSD_SIGNALS)
  HOLD_MY_SIG;
#endif
  getod();
#ifndef NSS
  TRACE_JOB("JOB BEGIN:\ttime: %u egp: %u rip: %u hello: %u\n\t\trt: %u if: %u stamp: %u\n",
            gatedtime, next_egpjob, next_ripjob, next_hellojob,
            next_rttime, next_iftime, next_timestamp);
#else	NSS
  TRACE_JOB("JOB BEGIN:\ttime: %u egp: %u rt: %u if: %u\n",
            gatedtime, next_egpjob, next_rttime, next_iftime);
#endif  NSS

  if (next_rttime == 0)	{		/* initialization */
    next_rttime = gatedtime + RT_TIMERRATE;
  }

  interval = gatedtime+1000;	/* some large value */

  if (gatedtime >= next_timestamp) {
    TRACE_STAMP("TIMESTAMP: %s", strtime);
    next_timestamp = gatedtime+TIME_STAMP;
  }

  if (doing_egp && gatedtime >= next_egpjob) {
    TRACE_JOB("JOB CALL:\tegpjob()\n");
    egpjob();
    TRACE_JOB("JOB RETURN:\tegpjob()\n");
    next_egpjob = gatedtime + egpsleep;
  }
  if (doing_egp && interval > next_egpjob)
      interval = next_egpjob;
#ifndef	NSS
  if (doing_rip && gatedtime >= next_ripjob) {
    if (rip_supplier) {
      TRACE_JOB("JOB CALL:\ttoall()\n");
      toall(supply);
      TRACE_JOB("JOB RETURN:\ttoall()\n");
    }
    next_ripjob = gatedtime + RIP_INTERVAL;
  }
  if ( doing_rip && interval > next_ripjob) {
      interval = next_ripjob;
  }
  if (doing_hello && gatedtime >= next_hellojob) {
    if (hello_supplier) {
      TRACE_JOB("JOB CALL:\thellojob()\n");
      hellojob();
      TRACE_JOB("JOB RETURN:\thellojob()\n");
    }
    next_hellojob = gatedtime + HELLO_TIMERRATE;
  }
  if (doing_hello && interval > next_hellojob)
      interval = next_hellojob;
#endif	NSS
  if (gatedtime >= next_rttime) {
    TRACE_JOB("JOB CALL:\trt_time()\n");
    rt_time();
    TRACE_JOB("JOB RETURN:\trt_time()\n");
    next_rttime = gatedtime + RT_TIMERRATE;
    if (sched_a_dump == 1) {
      TRACE_JOB("JOB CALL:\tdumpinfo()\n");
      dumpinfo();
      TRACE_JOB("JOB RETURN:\tdumpinfo()\n");
#ifdef	NSS
      TRACE_JOB("JOB CALL:\tl2lsdb_dump()\n");
      l2lsdb_dump();
      TRACE_JOB("JOB RETURN:\tl2lsdb_dump()\n");
#endif	NSS
      sched_a_dump = 0;
    }
  }
  if (interval > next_rttime)
      interval = next_rttime;
#ifndef	NSS
  if (gatedtime >= next_iftime) {
    TRACE_JOB("JOB CALL:\tif_check()\n");
    if_check();
    TRACE_JOB("JOB RETURN:\tif_check()\n");
#ifdef	AGENT_SNMP
    register_snmp_vars();
#endif	AGENT_SNMP
#ifdef	AGENT_SGMP
    register_sgmp_vars();
#endif	AGENT_SGMP
    next_iftime = gatedtime + CHECK_INTERVAL;
  }
  if (interval > next_iftime) {
      interval = next_iftime;
  }

  if (do_reinit) {
    do_reinit = 0;
    reinit();
  }
#endif	NSS

#ifndef NSS
  TRACE_JOB("JOB END:\ttime: %u egp: %u rip: %u hello: %u\n\t\trt: %u if: %u stamp: %u\n",
            gatedtime, next_egpjob, next_ripjob, next_hellojob,
            next_rttime, next_iftime);
#else	NSS
  TRACE_JOB("JOB END:\ttime: %u egp: %u rt: %u if: %u\n",
            gatedtime, next_egpjob, next_rttime, next_iftime);
#endif  NSS
  TRACE_JOB("JOB TIME:\tinterval: %u delta: %u\n", interval, interval-gatedtime);

  interval -= gatedtime;
#ifndef	NSS
  value.it_interval.tv_sec = 0;		/* no auto timer reload */
  value.it_interval.tv_usec = 0;
  value.it_value.tv_sec = interval;
  value.it_value.tv_usec = 0;
#ifndef vax11c
  (void) setitimer(ITIMER_REAL, &value, &ovalue);
#else	vax11c
  Set_VMS_Timeout(&value, timeout, 0, "GATED_timeout()");
#endif	vax11c
#endif	NSS

#if sgi && !defined(_BSD_SIGNALS)
  RELSE_MY_SIG;
#endif
  return;
}

/*
 * get time of day in seconds and as an ASCII string.
 * Called at each interrupt and saved in external variables.
 */

getod()
{
  struct timeval tp;
  struct timezone tzp;

  if (gettimeofday (&tp, &tzp))
    p_error("getod: gettimeofday");
  gatedtime = tp.tv_sec;				/* time in seconds */
  strtime = (char *) ctime(&gatedtime);		 /* time as an ASCII string */
  return;
}


/* exit gated */

quit()
{
  if (rt_default_active == TRUE) {
    (void) rt_default("DELETE");
  }
  syslog(LOG_NOTICE, "Exit %s version %s", my_name, version);
  getod();
  TRACE_TRC("\nExit %s[%d] version %s at %s\n", my_name, my_pid, version, strtime);
  exit(1);
}

/*
 * Print error message to system logger.
 *
 * First flush stdout stream to preserve log order as perror writes directly
 * to file
 */

p_error(str)
	char *str;
{
  int error_number = errno;

  TRACE_TRC("%s: %s\n", str, gd_error(error_number));
#ifdef	notdef
  (void) fflush(stdout);
#endif	notdef
  syslog(LOG_ERR, "%s: %s", str, gd_error(error_number));
}

/*
 * set dump flag so the database may be dumped.
 */
SIGTYPE setdumpflg()
{
#if sgi && !defined(_BSD_SIGNALS)
	HOLD_MY_SIG;
#endif
	sched_a_dump = 1;
	getod();
	syslog(LOG_NOTICE, "setdumpflg: dump request received");
	TRACE_TRC("setdumpflg: dump request received at %s", strtime);
#if sgi && !defined(_BSD_SIGNALS)
	RELSE_MY_SIG;
#endif
}

/*
 * toggle the trace on/off on a HUP.
 */
SIGTYPE chgtrace()
{
  int new_flags;

#if sgi && !defined(_BSD_SIGNALS)
	HOLD_MY_SIG;
#endif
  if (logfile == NULL) {
    syslog(LOG_ERR, "chgtrace: can not toggle tracing to console");
    return;
  }
  if (are_tracing) {
    traceoff(GEN_TRACE);
    are_tracing = 0;
  } else {
    if ( (new_flags = traceflags()) ) {
      savetrace = new_flags;
    }
    traceon(logfile, GEN_TRACE);
    are_tracing = 1;
  }
#if sgi && !defined(_BSD_SIGNALS)
	RELSE_MY_SIG;
#endif
}

#ifndef	NSS
/*
 *	set_reinit_flag() - Set re-init flag to re-read config file
 */

SIGTYPE set_reinit_flag()
{
#if sgi && !defined(_BSD_SIGNALS)
	HOLD_MY_SIG;
#endif
	do_reinit = 1;
	getod();
	syslog(LOG_NOTICE, "set_reinit_flag: re-init request received");
	TRACE_TRC("set_reinit_flag: re-init request received at %s", strtime);
#if sgi && !defined(_BSD_SIGNALS)
	RELSE_MY_SIG;
#endif
}
#endif	NSS

reinit()
{
  FILE *fp;

  if ( (fp = conf_open()) == NULL) {
    syslog(LOG_WARNING, "main: initialization file %s missing", EGPINITFILE);
    TRACE_TRC("main: initialization file %s missing at %s", EGPINITFILE, strtime);
    return;
  }

  syslog(LOG_INFO, "reinit: reinitializing from %s started", EGPINITFILE);
  TRACE_TRC("reinit: reinitializing from %s started at %s", EGPINITFILE, strtime);

  /* Need to add code to set the tracing depending on traceflags */

  rt_ASinit(fp, FALSE);
#ifndef	NSS
  if_check();
#endif	NSS
#ifdef	AGENT_SNMP
  register_snmp_vars();
#endif	AGENT_SNMP
#ifdef	AGENT_SGMP
  register_sgmp_vars();
#endif	AGENT_SGMP

  (void) fclose(fp);

  syslog(LOG_INFO, "reinit: reinitializing from %s done", EGPINITFILE);
  TRACE_TRC("reinit: reinitializing from %s done at %s", EGPINITFILE, strtime);

  return;
}

FILE *conf_open()
{
  FILE *fp;

  if ( (fp = fopen(EGPINITFILE, "r")) == NULL) {
    syslog(LOG_WARNING, "conf_open: initialization file %s missing - using defaults\n", EGPINITFILE);
    TRACE_TRC("conf_open: initialization file %s missing - using defaults\n", EGPINITFILE);
    no_config_file = TRUE;
    if ( (fp = fopen(EGPINITFILE = "/dev/null", "r")) == NULL) {
      syslog(LOG_WARNING, "conf_open: error opening %s\n", EGPINITFILE);
      TRACE_TRC("conf_open: error opening %s\n", EGPINITFILE);
    }
  }
  return(fp);
}
