/* Silicon Graphics PPP
 */

#ident "$Revision: 1.58 $"


#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stropts.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <values.h>

#include <netdb.h>
#include <net/if.h>
#include <net/raw.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

#define DEFINE
#include "ppp.h"
#include "pppinfo.h"
#include "keyword.h"

static struct ppp *newest, *oldest;
static enum connmodes callmode;

/* when the bundle will be busy, and time to add bandwidth */
static struct timeval busy_time;

/* when the bundle will be idle, and time to reduce bandwidth */
static struct timeval idle_time;
static int idle;

/* ignore busy or idle indications from the kernel for a while after
 *	changing the available bandwidth
 */
static struct timeval beepok_time;

/* slow down attemps to add lines as consecutive attempts fail */
static struct timeval add_time;
#define BACKOFF0    5			/* initial value in seconds */
#define BACKOFF_MAX (15*60)
static time_t add_backoff = BACKOFF0;

/* to infer speeds of individual links */
static float hi_bps;			/* add links when load above this */
static float lo_bps;			/* too many links if load below this */
static float avail_bps;
static int avail_bps_ok;		/* 0=unknown, 1=computed, 2=config */
static float prev_avail_bps;
static int prev_avail_bps_ok;
static time_t beep_tstamp;		/* previous counts to get rates */
u_int beep_ibytes, beep_obytes;


/* info given `pppstat` */
static struct ppp_status ppp_status;
static struct timeval ppp_status_time;	/* status file should be updated */


/* initial packets entangled with banner on async links */
#define BANSIZE 512
#define BANPAD 5
static char banbuf[BANSIZE+BANPAD];
static int banlen, banoff;

static flg seen_framer;			/* >=1 HDLC frame through framer */
static flg all_sync;			/* all links in bundle are sync */

#define mpsize 512
static struct ppp_buf mp_buf;
static int mp_sn = -2;
static int mp_len;


static struct ppp *rcv_msg_ppp;		/* set by rcv_msg() */
struct timeval rcv_msg_ts;
static char bad_beep[20];
static char *beep_name;
static char *beeps[] = {
	"TCP/IP",			/* BEEP_ACTIVE */
	"IP",				/* BEEP_WEAK */
	"line off",			/* BEEP_DEAD */
	"CP bad",			/* BEEP_CP_BAD */
	"Frame",			/* BEEP_FRAME */
	"bad FCS",			/* BEEP_FCS */
	"aborted frame",		/* BEEP_ABORT */
	"tiny frame",			/* BEEP_TINY */
	"dropped",			/* BEEP_BABBLE */
	"MP",				/* BEEP_MP */
	"MP error"			/* BEEP_MP_ERROR */
};
#define NUM_BEEPS (sizeof(beeps)/sizeof(beeps[0]))


static void usage(void);
static void newest_oldest(void);
static void init_ppp(struct ppp*);
static void update_acct(int);
static void ck_rend(char*);
static int make_ip(struct ppp*);
static void update_status(void);
static int rcv_msg(int);
static void ipkt(struct ppp*);
static void log_pkt(int lvl, char*, char*, char*, int, u_char*);
static void log_beep_act(char*, char*, char*, char*, struct beep*);
static int log_mp_ipkt_str(int, char*, char*);
static int addline(void);
static int fork_addline(int);
static void unfork(char *);
static void die(struct ppp *);
static void last_dev(void);
static void get_hi_lo_bps(void);
static void numdevs_inc(struct ppp*);
static void deadline(struct ppp*);
static void reap_child(void);
static void devs_off(int, char*);
static void stopint(int);
static void moredebug();
static void lessdebug();


/* FCS table from RFC-1331 */
u_short ppp_fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


void
main(int argc, char **argv)
{
	register int i;
	register struct ppp *ppp;
	char *p;
	fd_set err, in;
	int nfds;
	struct timeval timer;
	time_t c_dly;
	long ms, ms1, ms2;

	if (0 != (p = strrchr(argv[0], '/'))) {
		pgmname = p+1;
	} else {
		pgmname = argv[0];
		if (!strcmp(pgmname,"-ppp"))
			pgmname++;
	}

	ctty = open("/dev/tty", O_RDONLY|O_NDELAY);
	if (isatty(stderrfd)) {
		interact = 1;
	} else {
		no_interact();
	}
	openlog(pgmname, LOG_PID | LOG_ODELAY | LOG_NOWAIT, LOG_DAEMON);

	opterr = 0;
	while (i = getopt(argc,argv,"dLf:r:"), i != EOF)
		switch (i) {
		case 'd':
			debug = ++arg_debug;
			SET_Debug();
			break;
		case 'L':
			(void)printf("keywords:\n");
			for (i = 0; i < KEYTBL_LEN-1; i++)
				(void)printf("\t%s\n",keytbl[i].str);
			exit(0);
			break;
		case 'f':
			cfilename = optarg;
			break;
		case 'r':
			remote = optarg;
			break;
		default:
			usage();
	}
	if (argc != optind)
		usage();

	if (remote) {
		assume_callee = 0;
	} else {
		assume_callee = 1;
		connmode = CALLEE;
		no_interact();

		remote = getenv("USER");
		if (!remote) {
			remote = "";
			log_complain("","-r not specified and $USER not set");
			exit(1);
		}
	}
	if (remote)
		remote = strdup(remote);
	if ((int)strlen(remote) > SYSNAME_LEN) {
		log_complain("","\"%s\" is too long", remote);
		remote[SYSNAME_LEN] = '\0';
	}

	if (geteuid() != 0) {
		log_complain("","requires UID 0");
		exit(1);
	}

	/* from now on, send complaints to the system log */
	if (stderrpid <= 0)
		stderrfd = -1;

	/* avoid being affected by odinary processes */
	(void)nice(-40);

	init_rand();
	ppp_status.ps_version = PS_VERSION;
	ppp_status.ps_initial_date = time(0);
	get_clk();
	clr_acct(&ppp0.dv);
	if (assume_callee)
		ppp0.dv.acct.answered++;
	last_dev();

	idle_time.tv_sec = TIME_NEVER;
	busy_time.tv_sec = TIME_NEVER;
	beepok_time.tv_sec = TIME_NOW;
	add_time.tv_sec = TIME_NOW;

	init_ppp(&ppp0);
	(void)parse_conf(0);

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, stopint);
	(void)signal(SIGTERM, stopint);
	(void)signal(SIGPIPE, stopint);
	(void)signal(SIGUSR1, moredebug);
	(void)signal(SIGUSR2, lessdebug);
	(void)signal(SIGCHLD, reap_child);

	if (connmode == CALLEE) {
		ppp0.dv.callmode = CALLEE;
		grab_dev("starting PPP", &ppp0.dv);
		numdevs_inc(&ppp0);
		closefds();		/* avoid signals from the terminal */
		if (!rdy_tty_dev(&ppp0.dv))
			cleanup(1);
		if (0 > reset_accm(&ppp0,1))
			cleanup(1);
		if (ppp0.dv.sync == SYNC_ON)
			ppp0.nowait = 1;    /* assume no getty on sync line */
	} else {
		if (lochost.sin_addr.s_addr != 0
		    && !def_lochost
		    && remhost.sin_addr.s_addr != 0
		    && !def_remhost) {
			if (!add_rend_name("",remhost_str))
				cleanup(1);
			ck_rend("existing caller connected to %s");
			if (!make_ip(&ppp0))
				cleanup(1);
		}
	}

	for (;;) {
restart:
		/* Come here either at the very start, after a child has
		 * been forked via fork_addline(), or after a camping
		 * CALLER decides to try again.
		 *
		 * Here there are 0 or 1 active lines.
		 */

		closefds();		/* avoid signals from the terminal */

		if (connmode == Q_CALLER) {
			if (numdevs == 0)
				goto shut;

		} else if (connmode ==  CALLER) {
			/* The second time around or when we know the remote
			 * IP address, use a child to make the link.
			 * This lets us listen for incoming connections.
			 */
			if (rendnode >= 0) {
				if (fork_addline(1))
					goto shut;
			} else if (!addline()) {
				goto shut;
			}
		}

		/* if we have a device and are just starting with it,
		 * then get LCP going.
		 */
		if (ppp0.dv.devfd >= 0
		    && ppp0.lcp.fsm.state <= FSM_CLOSED_2) {
			log_debug(1,"", "starting to use %s", ppp0.dv.line);
			lcp_event(&ppp0,FSM_OPEN);
			lcp_event(&ppp0,FSM_UP);
		}

		/* While in this loop there is at least one active line
		 * and more might be added.
		 */
		idle = 0;
		act_devs(2,0);
		for (; numdevs > 0; get_clk()) {
			FD_ZERO(&in);
			nfds = 0;

			ms = HEARTBEAT*1000;

			if (numdevs > mindevs) {
				ms1 = cktime(&idle_time);
				ms = MIN(ms, ms1);
			}
			if (add_pid < 0 && numdevs < outdevs) {
				ms1 = cktime(&busy_time);
				ms2 = cktime(&add_time);
				ms1 = MAX(ms1,ms2);
				ms = MIN(ms, ms1);
			}

			if (banlen != 0)
				ms = 0;	/* get frame saved from banner */

			/* run state machines for the RFC 1717 bundle */
			if (ms != 0 && mp_ppp != 0) {
				ms1 = cktime(&mp_ppp->ipcp.fsm.timer);
				ms = MIN(ms, ms1);

				ms1 = cktime(&mp_ppp->ccp.fsm.timer);
				ms = MIN(ms, ms1);
			}

			for (ppp = &ppp0;
			     ppp != 0;
			     ppp = (struct ppp*)ppp->dv.next) {
				if (ppp->dv.line[0] == '\0')
				    continue;

				if (ms != 0) {
					/* note earliest PPP protocol timer */
					ms1 = cktime(&ppp->lcp.fsm.timer);
					ms = MIN(ms, ms1);

					ms1 = cktime(&ppp->auth.timer);
					ms = MIN(ms, ms1);

					if (mp_ppp == 0) {
					    ms1 = cktime(&ppp->ipcp.fsm.timer);
					    ms = MIN(ms, ms1);

					    ms1 = cktime(&ppp->ccp.fsm.timer);
					    ms = MIN(ms, ms1);
					}

					if (ppp->phase == NET_PHASE) {
					    ms1 = cktime(&ppp->lcp.echo_timer);
					    ms = MIN(ms, ms1);
					}
				}

				/* Watch lines that are not under the module.
				 * Allow only one packet at a time so that
				 * none bypass the module.
				 */
				if (ppp->dv.devfd >= 0) {
				    if (ppp->in_stop != 1) {
					(void)do_strioctl(ppp,
							  SIOC_PPP_1_RX, 0,
							  "ioctl(_1_RX)");
					ppp->in_stop = 1;
				    }
				    FD_SET(ppp->dv.devfd, &in);
				    nfds = MAX(nfds,ppp->dv.devfd);
				}
			}

			/* Awaken for the expiration of the inactivity timer
			 * if we are going to sleep at all.
			 * Do not worry about the inactivity timer if the
			 * bundle was started by the peer.  Extra lines
			 * are handled separately.
			 */
			if (sactime != 0 && oldest != 0) {
				c_dly = oldest->dv.active;
			} else {
				c_dly = TIME_NEVER;
			}
			if (c_dly == TIME_NEVER) {
				ppp_status.ps_atime = TIME_NEVER;
			} else {
				ppp_status.ps_atime = (c_dly-clk.tv_sec);
				if (ms != 0) {
					ms1 = ppp_status.ps_atime*1000;
					ms =  (ms1 > 0) ? MIN(ms, ms1) : 0;
				}
			}

			timer.tv_sec = ms/1000;
			timer.tv_usec = (ms%1000)*1000;

			/* A CALLEE might not have finished IPCP chatting and
			 * received its IP address and so might not yet have
			 * the PPP module or rendezvous socket set up.
			 */
			if (rendnode >= 0 && add_pid != 0) {
				ck_rend("link to %s stolen");
				FD_SET(rendnode, &in);
				nfds = MAX(nfds,rendnode);
			}
			if (modfd >= 0) {
				FD_SET(modfd, &in);
				nfds = MAX(nfds,modfd);
			}
			err = in;
			if (status_poke_fd >= 0) {
				FD_SET(status_poke_fd, &in);
				nfds = MAX(nfds,status_poke_fd);
			}

			nfds = select(nfds+1, &in,0,&err, &timer);
			if (nfds < 0) {
				if (errno == EINTR)
					continue;
				if (errno != EAGAIN)
					bad_errno("select()","");
				nfds = 0;
			}

			update_acct(0);

			if (banlen != 0 && nfds == 0) {
				/* get frame saved from banner */
				FD_SET(ppp0.dv.devfd, &in);
				nfds = 1;
			} else if (nfds == 0) {
				/* If things are quiet, maybe shut down.
				 * Stop incomplete calls.
				 * If we originated this call, shut it all
				 * down.  Otherwise, shut down our part
				 * of it.
				 */
				if (clk.tv_sec > c_dly) {
					unfork("activity timer expired");
					devs_off((callmode != CALLEE
						  || ppp0.conf.mp_callee)
						 ? 0 : 1,
						     "activity timer expired");
				}
			}

			/* See what the kernel has to say from the PPP driver.
			 */
			if (modfd >= 0 && FD_ISSET(modfd,&in)
			    && rcv_msg(modfd)) {
				switch (ibuf.type) {
				case BEEP_DEAD:
					if (!rcv_msg_ppp) {
					    log_debug(1,"",
						      "odd %s from index %d",
						      beep_name,
						      ibuf.dev_index);
					} else {
					    deadline(rcv_msg_ppp);
					}
					break;
				case BEEP_CP_BAD:
					/* Problem with compression.
					 * Blip CCP if active.
					 */
					ppp = mp_ppp ? mp_ppp : rcv_msg_ppp;
					if (!ppp) {
					    log_debug(1,"",
						      "odd %s from index %d",
						      beep_name,
						      ibuf.dev_index);
					} else {
					    ppp->ccp.cnts.bad_ccp++;
					    ccp_blip(ppp,0);
					}
					break;
				case BEEP_ACTIVE:
					act_devs(0,1);
					update_acct(1);
					ccp_blip_age(rcv_msg_ppp);
					break;
				case BEEP_WEAK:
					act_devs(1,1);
					update_acct(1);
					ccp_blip_age(rcv_msg_ppp);
					break;
				case BEEP_FRAME:
					if (!rcv_msg_ppp) {
					    log_debug(1,"",
						      "odd %s from index %d",
						      beep_name,
						      ibuf.dev_index);
					} else {
					    ipkt(rcv_msg_ppp);
					}
					break;
				case BEEP_FCS:
				case BEEP_ABORT:
				case BEEP_TINY:
				case BEEP_BABBLE:
				case BEEP_MP:
				case BEEP_MP_ERROR:
					break;
				default:
					log_complain(rcv_msg_ppp
						     ? rcv_msg_ppp->link_name
						     : "",
						     "wild %s from index %d",
						     beep_name,
						     ibuf.dev_index);
					break;
				}
			}

			/* Deal with non-IP input or other PPP events.
			 */
			for (ppp = &ppp0;
			     ppp != 0;
			     ppp = (struct ppp*)ppp->dv.next) {
				if (ppp->dv.line[0] == '\0')
					continue;

				if (ppp->dv.devfd >= 0) {
					if (FD_ISSET(ppp->dv.devfd, &err)) {
					    log_debug(1, ppp->link_name,
						      "%s off/error",
						      ppp->dv.line);
					    hangup(ppp);
					    continue;
					}
					if (FD_ISSET(ppp->dv.devfd, &in)) {
					    if (ppp->in_stop != 0)
						ppp->in_stop = 2;
					    if (rcv_msg(ppp->dv.devfd)) {
						switch (ibuf.type) {
						case BEEP_FRAME:
						    ipkt(ppp);
						    break;
						case BEEP_DEAD:
						    deadline(ppp);
						    continue;
						}
					    }
					}
				}

				if (cktime(&ppp->lcp.fsm.timer) == TIME_NOW)
					lcp_event(ppp,FSM_TO_P);

				if (cktime(&ppp->auth.timer) == TIME_NOW)
					auth_time(ppp);

				if (mp_ppp == 0) {
					if (cktime(&ppp->ipcp.fsm.timer)
					    == TIME_NOW)
						ipcp_event(ppp,FSM_TO_P);

					if (cktime(&ppp->ccp.fsm.timer)
					    == TIME_NOW)
						ccp_event(ppp,FSM_TO_P);
				}

				if (ppp->phase == NET_PHASE
				    && cktime(&ppp->lcp.echo_timer)==TIME_NOW)
					lcp_send_echo(ppp);
			}
			if (mp_ppp != 0) {
				if (cktime(&mp_ppp->ipcp.fsm.timer)
				    == TIME_NOW)
					ipcp_event(mp_ppp,FSM_TO_P);

				if (cktime(&mp_ppp->ccp.fsm.timer)
				    == TIME_NOW)
					ccp_event(mp_ppp,FSM_TO_P);
			}

			/* If we are the child, join the resident daemon
			 * only when all of the banner has been processed
			 * and at least one sync HDLC frame has gone
			 * through the framer.
			 */
			if (add_pid == 0 && banlen == 0 && !reconfigure
			    && (ppp0.dv.sync == SYNC_OFF || seen_framer)) {
				(void)make_rend(1);
				do_rend(&ppp0);
			}

			if (rendnode >= 0 && FD_ISSET(rendnode,&in))
				(void)rendezvous(1);

			/* Add or delete extra lines.  This is separate
			 * from the inactivity timer on the whole bundle.
			 *  add_time delays adding the first line,
			 *  effectively helping to implement demand dialing.
			 *  Otherwise, we would restore the line immediately
			 *  after turning it off.
			 */
			if (numdevs < outdevs
			    && (numdevs < mindevs
				|| TIME_NOW == cktime(&busy_time))) {
				if (TIME_NOW == cktime(&add_time)
				    && !fork_addline(ppp0.conf.mp_callee))
					goto restart;

			} else if (numdevs > mindevs
				   && TIME_NOW == cktime(&idle_time)) {
				/* Turn off most recent new device. */
				devs_off(2,"inactivity timeout");
			}

			/* update status for pppstat */
			if (status_poke_fd >= 0
			    && FD_ISSET(status_poke_fd,&in))
				update_status();
		}

		/* all lines are off */

		update_acct(1);
		if (!demand_dial && !camping) {
			/* Must be simple, one-shot CALLEE or CALLER
			 * that handled one call.
			 */
			cleanup(0);
		}


shut:
		/* Here there are 0 active lines.
		 *
		 * Come here after all lines have shut down, when
		 * a quiet caller has not yet started a line, or after
		 * a simple caller has failed to make a connection.
		 *
		 * A camping caller that has not yet made the connection
		 * might not know the IP addresses and so the rendezvous
		 * socket might not be ready.
		 */

		if (add_pid == 0) {
			/* must be a helpful child that failed.
			 */
			cleanup(1);
		}

		if (rendnode >= 0
		    && (demand_dial || camping || add_pid > 0)) {
			if (connmode == CALLER)
				connmode = Q_CALLER;

			for (ppp = &ppp0;
			     ppp != 0;
			     ppp = (struct ppp*)ppp->dv.next) {
				if (ppp->dv.devfd >= 0
				    || ppp->dv.dev_index != -1)
					hangup(ppp);
			}

			/* Flush pending output,
			 * unless we have a child trying to connect.
			 */
			if (add_pid < 0
			    && 0 > ioctl(modfd, TCFLSH, 2))
				log_errno("ioctl(modfd,TCFLSH,2)","");

			do {
				ppp0.dv.acct.sconn = get_clk();

				/* Check that the FIFO has not been removed.
				 * Quit if some other process now owns
				 * the link.
				 */
				ck_rend("link to %s stolen");

				ck_acct(&ppp0.dv,0);

				FD_ZERO(&in);
				FD_SET(rendnode, &in);
				nfds = rendnode;
				ms = cktime(&add_time);
				if (ms > 0) {
					/* If the modem is still hot,
					 * let it cool.
					 */
					ppp_status.ps_atime = ms/1000;
					if (ms/1000 > HEARTBEAT)
					    ms = HEARTBEAT*1000;
				} else {
					/* If not demand dialing, redial now.
					 */
					if (!demand_dial
					    && !fork_addline(1))
					    goto restart;

					/* else, wait until the kernel
					 * says something is happening
					 */
					FD_SET(modfd, &in);
					nfds = MAX(nfds,modfd);
					ppp_status.ps_atime = TIME_NEVER;
					ms = HEARTBEAT*1000;
				}
				timer.tv_sec = ms/1000;
				timer.tv_usec = (ms%1000)*1000;
				if (status_poke_fd >= 0) {
					FD_SET(status_poke_fd, &in);
					nfds = MAX(nfds,status_poke_fd);
				}
				nfds = select(nfds+1,&in,0,0, &timer);
				if (nfds < 0) {
					if (errno == EINTR)
						continue;
					if (errno != EAGAIN)
						bad_errno("select()","");
					nfds = 0;
				}

				ppp0.dv.acct.sconn = get_clk();

				if (nfds > 0) {
					/* see what kernel has to say */
					if (FD_ISSET(modfd,&in)
					    && rcv_msg(modfd)) {
					    if (ibuf.type == BEEP_ACTIVE
						|| ibuf.type == BEEP_WEAK) {
						    if (!fork_addline(1))
							    goto restart;
					    } else {
						    log_complain("",
								 "odd \"%s\"",
								 beep_name);
					    }
					}

					if (FD_ISSET(rendnode,&in)
					    && rendezvous(1)) {
						break;
					}
				}

				/* update status for pppstat */
				if (status_poke_fd >= 0
				    && FD_ISSET(status_poke_fd, &in))
					update_status();
			} while (demand_dial || camping || add_pid > 0);
		}

		if (!demand_dial && !camping
		    && add_pid <= 0 && numdevs == 0) {
			/* must be a one-shot CALLEE or CALLER that failed.
			 */
			cleanup(0);
		}
	}
}



static void
usage(void)
{
	kludge_stderr();
	(void)fputs("usage: [-d] [-f cfile] [-r remote]\n", stderr);
	exit(1);
}


u_long
newmagic(void)
{
	register u_long m;
	struct tms tms;

	for (;;) {
		m = random() ^ times(&tms);
		if ((m & 0xff) != 0)
			return m;
	}
}


/* find the newest and oldest links
 */
static void
newest_oldest(void)
{
	struct ppp *ppp;
	int new_age, old_age;

	newest = 0;
	oldest = 0;
	callmode = UNKNOWN_CALL;
	new_age = 0;
	old_age = 0;
	all_sync = 1;
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.line[0] == '\0')
			continue;
		if (!ppp->dv.sync)
			all_sync = 0;
		if ( ppp->age >= new_age) {
			new_age = ppp->age;
			newest = ppp;
		}
		if (ppp->age <= old_age || old_age == 0) {
			old_age = ppp->age;
			oldest = ppp;
			callmode = ppp->dv.callmode;
		}
	}
}


static void
fix_name(struct ppp *ppp)
{
	bzero(ppp->link_name, sizeof(ppp->link_name));
	if (maxdevs > 1 && add_pid != 0 && rendnode >= 0) {
		int i = ppp->link_num;
		(void)sprintf(ppp->link_name," %d", i);
		(void)sprintf(ppp->lcp.fsm.name, "  LCP%d", i);
		(void)sprintf(ppp->auth.name,    " AUTH%d", i);
		if (ppp == mp_ppp) {
			(void)strcpy(ppp->ipcp.fsm.name, "  IPCP");
			(void)strcpy(ppp->ccp.fsm.name,  "   CCP");
		} else {
			(void)sprintf(ppp->ipcp.fsm.name," IPCP%d", i);
			(void)sprintf(ppp->ccp.fsm.name, "  CCP%d", i);
		}
	} else {
		(void)strcpy(ppp->lcp.fsm.name, "  LCP");
		(void)strcpy(ppp->auth.name,    " AUTH");
		(void)strcpy(ppp->ipcp.fsm.name," IPCP");
		(void)strcpy(ppp->ccp.fsm.name, "  CCP");
	}
}


static void
fix_names(void)
{
	struct ppp *ppp;

	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next)
		fix_name(ppp);
}


static void
clear_ppp(struct ppp *ppp)
{
	int save_num;
	struct dev *save_next;

	if (ppp == &ppp0) {
		ppp->link_num = 1;
	} else {
		save_next = ppp->dv.next;
		save_num = ppp->link_num;
		bzero(ppp, sizeof(*ppp));
		ppp->dv.next = save_next;
		ppp->link_num = save_num;
		ppp->dv.acct = ppp0.dv.acct;
		ppp->dv.acct.call_start = 0;
	}
	ppp->mypid = getpid();
	ppp->dv.line[0] = '\0';
	ppp->dv.devfd = -1;
	ppp->dv.devfd_save = -1;
	ppp->dv.dev_index = -1;
	ppp->dv.dev_sbuf.st_ino = 0;
	ppp->dv.rendpid = -1;
	ppp->dv.active = TIME_NEVER;
	ppp->lcp.fsm.timer.tv_sec = TIME_NEVER;
	ppp->ipcp.fsm.timer.tv_sec = TIME_NEVER;
	ppp->ccp.fsm.timer.tv_sec = TIME_NEVER;
	ppp->utmp_id[0] = '\0';
	ppp->utmp_has_host = 0;
	ppp->utmp_type = NO_UTMP;
	ppp->bps = ppp0.conf_bps;
}


static void
init_ppp(struct ppp *ppp)
{
	clear_ppp(ppp);
	fix_name(ppp);

	ppp->age = clk.tv_sec;

	lcp_init(ppp);
	auth_init(ppp);
	ipcp_init(ppp);
	ccp_init(ppp);
}



/* account for the connect-time */
static void
update_acct(int reset_idle)		/* 1=now busy */
{
	struct ppp *ppp;
	int total_secs, idle_secs;

	total_secs = get_clk() - ppp0.dv.acct.sconn;

	if (idle) {
		idle_secs = total_secs;
	} else if (total_secs >= BEEP/HZ) {
		/* If enough time has elapsed for the regular notifications
		 * from the kernel, then the link must be idle.
		 * The link is considered busy for BEEP/HZ seconds after the
		 * kernel beeps.
		 */
		idle_secs = total_secs-BEEP/HZ;
		idle = 1;
	} else if (reset_idle) {
		/* The kernel has said something, so the preceding time
		 * can be counted.
		 */
		idle_secs = idle ? total_secs : 0;
	} else {
		/* Fewer than BEEP/HZ seconds have elapsed since the
		 * kernel said the link was busy, so nothing can be
		 * counted yet.
		 */
		return;
	}

	if (reset_idle)
		idle = 0;

	/* count link-seconds, accumulating the connect time for
	 * all of the links.
	 */
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.line[0] == '\0')
			continue;
		if (ppp->dv.callmode != CALLEE) {
			ppp0.dv.acct.orig_conn += total_secs;
			ppp0.dv.acct.orig_idle += idle_secs;
		} else {
			ppp0.dv.acct.ans_conn += total_secs;
			ppp0.dv.acct.ans_idle += idle_secs;
		}
	}
	ppp0.dv.acct.sconn = clk.tv_sec;
	ck_acct(&ppp0.dv, 0);
}



/* Try get get a frame from the among the banner
 *	Do not bother with address or protocol compression, because
 *	this code should only see initial LCP frames.
 */
static int				/* <0 if failed */
get_banframe(void)
{
	u_char c, *cp;
	u_short fcs;
	char esc;

	if (banlen == 0)
		return 0;

	ibuf.dev_index = ppp0.dv.dev_index;
	cp = &ibuf.frame;
	*cp++ = PPP_FLAG;
	fcs = PPP_FCS_INIT;
	esc = 0;
	while (banlen != 0) {
		if (cp >= &ibuf.un.info[PPP_MAX_MTU-2]) {
			ibuf.type = BEEP_BABBLE;
			break;
		}

		banlen--;
		c = banbuf[banoff++];
		if (esc) {
			/* do not bother after an aborted frame */
			if (c == PPP_FLAG) {
				log_debug(6,"",
					  "abort: discard %d salvaged bytes",
					  banlen+cp-&ibuf.frame);
				banlen = 0;
				return 0;
			}
			c ^= PPP_ESC_BIT;
			esc = 0;

		} else if (c == PPP_FLAG) {
			/* ignore null frames */
			if (cp == &ibuf.addr)
				continue;

			/* skip bad frames */
			if (cp < ibuf.un.info
			    || fcs != PPP_FCS_GOOD) {
				log_debug(6,"",
					  "bad FCS: discard %d salvaged bytes",
					  cp-&ibuf.frame);
				cp = &ibuf.frame;
				continue;
			}

			/* got a good one */
			ibuf.type = BEEP_FRAME;
			log_debug(3, "","salvage input packet from banner");
			return (cp-(u_char*)&ibuf)-2;	/* length less FCS */

		} else if (c == PPP_ESC) {
			esc = 1;
			continue;
		}

		fcs = PPP_FCS(fcs,c);
		*cp++ = c;
	}

	if (cp != &ibuf.frame)
		log_debug(6,"","missing end flag: discard %d salvaged bytes",
			  cp-&ibuf.frame);

	return 0;
}


/* update status for pppstat */
static void
update_status(void)
{
	struct ppp *ppp;
	struct ppp_info *info_p;
	struct ps_dev *psl;
	struct strioctl strioctl;
	struct stat st;
	char buf[32];
	int fd;

	/* flush the pipe that poked us */
	if (0 > read(status_poke_fd,buf,sizeof(buf))) {
		log_errno("read(status_poke_fd)","");
		(void)close(status_poke_fd);
		status_poke_fd = -1;
		return;
	}

	/* skip it if the file is recent */
	get_clk();
	if (ppp_status.ps_cur_date == cur_date.tv_sec
	    && cktime(&ppp_status_time) != TIME_NOW)
		return;
	fsm_settimer(&ppp_status_time, 750);

	/* punt if the module is not open */
	if (modfd < 0)
		return;

	/* get the data from the kernel */
	info_p = &ppp_status.ps_pi;
	strioctl.ic_cmd = SIOC_PPP_INFO;
	strioctl.ic_timout = 0;
	strioctl.ic_len = sizeof(info_p);
	strioctl.ic_dp = (char*)&info_p;
	if (0 > ioctl(modfd, I_STR, &strioctl)) {
		log_errno("_INFO","");
		ppp_status_time.tv_sec += 60;
		return;
	}

	if (ppp_status.ps_pi.pi_version != PI_VERSION) {
		log_complain("","wrong _PPP_INFO version %d instead of %d",
			     ppp_status.ps_pi.pi_version, PI_VERSION);
		ppp_status_time.tv_sec += 60;
		return;
	}
	if (ppp_status.ps_pi.pi_len != sizeof(ppp_status.ps_pi)) {
		log_complain("","wrong _PPP_INFO size %d instead of %d",
			     ppp_status.ps_pi.pi_len,
			     sizeof(ppp_status.ps_pi));
		ppp_status_time.tv_sec += 60;
		return;
	}
	ppp_status.ps_cur_date = cur_date.tv_sec;
	ppp_status.ps_pid = ppp0.mypid;
	ppp_status.ps_add_pid = add_pid;
	ppp_status.ps_debug = debug;
	ppp_status.ps_maxdevs = maxdevs;
	ppp_status.ps_outdevs = outdevs;
	ppp_status.ps_mindevs = mindevs;
	ppp_status.ps_numdevs = numdevs;
	ppp_status.ps_idle_time = cktime(&idle_time)/1000;
	ppp_status.ps_busy_time = cktime(&busy_time)/1000;
	ppp_status.ps_beepok_time = cktime(&beepok_time)/1000;
	ppp_status.ps_add_time = cktime(&add_time)/1000;
	ppp_status.ps_add_backoff = add_backoff;
	ppp_status.ps_avail_bps = avail_bps_ok ? avail_bps : 0;
	ppp_status.ps_prev_avail_bps = prev_avail_bps_ok ? prev_avail_bps : 0;
	bcopy(&ppp0.dv.acct, &ppp_status.ps_acct, sizeof(ppp_status.ps_acct));
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.line[0] == '\0' || ppp->dv.dev_index < 0)
			continue;
		psl = &ppp_status.ps_devs[ppp->link_num-1];
		psl->callee = (ppp->dv.callmode == CALLEE);
		psl->ps_active = clk.tv_sec - ppp->age;
		psl->bps = ppp->bps;
		psl->lcp_echo_rtt = ppp->lcp.echo_rtt;
		psl->lcp_neg_mtru = (mp_ppp ? mp_ppp : ppp)->lcp.neg_mtru;
		psl->lcp_neg_mrru = (mp_ppp ? mp_ppp : ppp)->lcp.neg_mrru;
		psl->lcp_neg_mtu = ppp->lcp.neg_mtu;
		psl->lcp_neg_mru = ppp->lcp.neg_mru;
		psl->bad_ccp = ppp->ccp.cnts.bad_ccp;
		psl->ccp_rreq_sent = ppp->ccp.cnts.rreq_sent;
		psl->ccp_rreq_rcvd = ppp->ccp.cnts.rreq_rcvd;
		psl->ccp_rack_rcvd = ppp->ccp.cnts.rack_rcvd;
		bcopy(&ppp->lcp.ident_rcvd, &psl->ident,
		      (psl->ident_len = ppp->lcp.ident_rcvd_len));
	}

	/* prevent some fun and games */
	if (0 > lstat(status_path, &st)) {
		if (errno != ENOENT) {
			log_errno("open() ", status_path);
			ppp_status_time.tv_sec += 60;
			return;
		}
	} else if (st.st_uid != 0 && 0 > unlink(status_path)) {
		log_errno("unlink() ", status_path);
		ppp_status_time.tv_sec += 60;
		return;
	}

	/* re-open the file every time in case it has disappeared */
	fd = open(status_path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		log_errno("open() ", status_path);
		ppp_status_time.tv_sec += 60;
		return;
	}
	if (sizeof(ppp_status) != write(fd,&ppp_status, sizeof(ppp_status)))
		log_errno("write() ", status_path);
	(void)close(fd);
}


int hack;
/* get an input packet
 */
static int				/* 0=bad */
rcv_msg(int fd)
{
	int sn, ticks, i;
	float load_bps, bps;
	struct ppp *ppp;
	int flags;
	struct strbuf imsg, cmsg;
	char *nstr;
	char dbgmsg[100];
	char cbuf[32];
	char istr[sizeof("index=xxx ")+8];
	static char *acts[] = {
		"rx lo/tx lo",		/* 0 */
		"rx med/tx lo",		/* 1 BEEP_ST_RX_ACT */
		"rx hi/tx lo",		/* 2 BEEP_ST_RX_BUSY */
		"rx ???/tx lo",		/* 3 ? */
		"rx hi/tx med",		/* 4 BEEP_ST_TX_ACT */
		"rx med/tx med",	/* 5 BEEP_ST_RX_ACT|BEEP_ST_TX_ACT */
		"rx hi/tx med",		/* 6 BEEP_ST_RX_BUSY|BEEP_ST_TX_ACT */
		"rx ???/tx med",	/* 7 ?|BEEP_ST_TX_ACT */
		"rx lo/tx hi",		/* 8 BEEP_ST_TX_BUSY */
		"rx med/tx hi",		/* 9 BEEP_ST_RX_ACT|BEEP_ST_TX_BUSY */
		"rx hi/tx hi",		/* a BEEP_ST_RX_BUSY|BEEP_ST_TX_busy */
		"rx ???/tx hi",		/* b ?|BEEP_ST_TX_busy */
		"rx lo/tx ???",		/* c ? */
		"rx med/tx ???",	/* d BEEP_ST_RX_ACT */
		"rx hi/tx ???",		/* e BEEP_ST_RX_BUSY */
		"???"			/* f ? */
	};


	bzero(&ibuf,sizeof(ibuf));

	/* Read from the file descriptor or from the frame saved
	 * from the banner or an assembled MP packet.
	 */
	if (fd != ppp0.dv.devfd
	    || 0 == (imsg.len = get_banframe())) {
		imsg.buf = (char*)&ibuf;
		imsg.maxlen = sizeof(ibuf);
		cmsg.buf = cbuf;
		cmsg.maxlen = sizeof(cbuf);
		bzero(cbuf,sizeof(cbuf));
		flags = 0;

		i = getmsg(fd,&cmsg,&imsg,&flags);
		if (i < 0) {
			if (errno != EINTR && errno != EAGAIN)
				log_errno("getmsg()","");
			return 0;
		}

		if (cmsg.len > 0) {
			if (imsg.len+cmsg.len >= sizeof(ibuf)) {
				log_complain("","discarding buffer with %d"
					     " control and %d data bytes",
					     cmsg.len, imsg.len);
				return 0;
			}

			(void)sprintf(dbgmsg,
				      "discard buffer with %d control and"
				      " %d data bytes",
				      cmsg.len, imsg.len);
			bcopy(cmsg.buf,&imsg.buf[imsg.len], cmsg.len);
			log_pkt(1, "", dbgmsg,0,
				cmsg.len+imsg.len, (u_char*)imsg.buf);
			return 0;
		}

		if (i != 0) {
			log_complain("","getmsg() returned %d for %d bytes",
				     i, imsg.len);
			return 0;
		}
		if (imsg.len < PPP_BUF_HEAD_FRAME) {
			(void)sprintf(dbgmsg,
				      "getmsg() returned 0 for %d bytes",
				      imsg.len);
			log_pkt(0,"",dbgmsg,0,imsg.len,(u_char*)imsg.buf);
			return 0;
		}
	}

	rcv_msg_ts = clk;
	ibuf_info_len = imsg.len - PPP_BUF_MIN;

	/* Recognize HDLC frames that arrive on a sync link before the
	 * PPP framing STREAMS module was pushed onto the stack.
	 *
	 * Do that only until the first valid buffer is received.
	 */
	if (!seen_framer
	    && imsg.buf == (char*)&ibuf) {
		if (imsg.buf[0] == PPP_ADDR
		    && imsg.buf[1] == PPP_CNTL
		    && ppp0.dv.sync == SYNC_ON) {
			for (i = imsg.len-1; i >= 0; i--)
				imsg.buf[i+PPP_BUF_HEAD] = imsg.buf[i];
			ibuf.type = BEEP_FRAME;
			ibuf.dev_index = -1;
			ibuf.bits = 0;
			ibuf.frame = 0;
			ibuf_info_len += PPP_BUF_HEAD;
			log_debug(3, "","salvage early HDLC frame");
		} else {
			seen_framer = 1;
		}
	}

	/* Find the corresponding MP or BF&I MP link
	 */
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.line[0] == '\0')
			continue;
		/* Messages with bad link numbers are either MP packets
		 * or are from streams not under the mux.
		 * They cannot be non-MP packets from links under the mux.
		 */
		if (ibuf.dev_index == -1
		    && ppp->dv.dev_index == -1
		    && ibuf.bits == 0)
			continue;
		if (ppp->dv.dev_index == ibuf.dev_index)
			break;
	}
	rcv_msg_ppp = ppp;


	istr[0] = '\0';
	nstr = "";
	if (ppp0.dv.next != 0
	    || mindevs > 1
	    || (ppp == 0 || ibuf.dev_index != -1)) {
		/* Forget the label if we can be certain only one link
		 * is involved.
		 *
		 * Generate the "index=XXX" string if possible.
		 * Use it only if there is not a better label from the
		 * main structure.
		 */
		if (ppp != 0)
			nstr = ppp->link_name;
		if (nstr[0] == '\0'
		    && ibuf.dev_index != -1)
			(void)sprintf(istr,"index=%d ", ibuf.dev_index);
	}

	if (ibuf.type <= NUM_BEEPS) {
		beep_name = beeps[ibuf.type-1];
	} else {
		(void)sprintf(bad_beep, "%#x", ibuf.type);
		beep_name = bad_beep;
	}

	switch (ibuf.type) {
	default:
		(void)sprintf(dbgmsg, "read %#x odd bytes:", imsg.len);
		log_pkt(1, nstr, dbgmsg,0, imsg.len, (u_char*)imsg.buf);
		break;

	case BEEP_ACTIVE:
	case BEEP_WEAK:
		ppp_status.ps_beep_st = ibuf.un.beep.st;
		nstr = (ibuf.un.beep.st < (sizeof(acts)/sizeof(acts[0]))
			? acts[ibuf.un.beep.st]
			: "???");
		/* if this beep is too soon after a change, then ignore it.
		 */
		get_clk();
		if (TIME_NOW != cktime(&beepok_time)) {
			beep_tstamp = ibuf.un.beep.tstamp;
			beep_ibytes = ibuf.un.beep.raw_ibytes;
			beep_obytes = ibuf.un.beep.raw_obytes;
			log_beep_act("ignore ", beep_name, istr, nstr,
				     &ibuf.un.beep);
			break;
		}

		/* compute link speed */
		if (beep_tstamp == 0) {
			ppp_status.ps_cur_ibps = 0;
			ppp_status.ps_cur_obps = 0;
			load_bps = 0;

		} else if ((ticks = (ibuf.un.beep.tstamp-beep_tstamp)) != 0) {
			ppp_status.ps_cur_ibps = ((ibuf.un.beep.raw_ibytes
						   - beep_ibytes) * HZ * 8.0
						  / ticks);
			ppp_status.ps_cur_obps = ((ibuf.un.beep.raw_obytes
						   - beep_obytes) * HZ * 8.0
						  / ticks);

			load_bps = MAX(ppp_status.ps_cur_ibps,
				       ppp_status.ps_cur_obps);
		}
		beep_tstamp = ibuf.un.beep.tstamp;
		beep_ibytes = ibuf.un.beep.raw_ibytes;
		beep_obytes = ibuf.un.beep.raw_obytes;


		/* "busy" means at least one instant there was more output
		 *	on the interface queue than could be sent down
		 *	the STREAMS.
		 * "idle" means that the interface queue drained completely.
		 * Each message means the business, idleness, or whatever
		 * has prevailed for the preceding "BEEP" duration.
		 *
		 * If we think we know the speed of the current link,
		 * do not drop it if the recent traffic rate would not
		 * fit on the bundle without the link.
		 *
		 * The ISDN driver does not tell the truth about the incoming
		 * load.  Under 100% saturation by TCP traffic, it often does
		 * not say a link is saturated, although it is supposed to
		 * say the link is saturated at or above 90%.  So just guess
		 * if we have the the right load and the ISDN driver said
		 * something.
		 */
		if (0 != (ibuf.un.beep.st & (BEEP_ST_RX_BUSY|BEEP_ST_TX_BUSY))
		    || load_bps >= hi_bps
		    || (all_sync
			&& !avail_bps_ok
			&& (ibuf.un.beep.st & BEEP_ST_RX_ACT)
			&& load_bps >= 0.9*56.0*numdevs)) {
			idle_time.tv_sec = clk.tv_sec+idle_delay;
			if (busy_time.tv_sec > clk.tv_sec+busy_delay)
				busy_time.tv_sec = clk.tv_sec+busy_delay;

			/* Attribute the difference between the current and
			 * previous maximum rates (when the bundle is
			 * saturated) to the bandwidth of the newest link.
			 */
			if (avail_bps_ok < 2) {
				if (!avail_bps_ok || avail_bps < load_bps) {
					avail_bps = load_bps;
					avail_bps_ok = 1;
					get_hi_lo_bps();
				}

				if (prev_avail_bps_ok
				    && newest != 0
				    && newest->conf_bps == 0) {
					bps = avail_bps - prev_avail_bps;
					/* make estimate more real */
					if (newest->dv.sync) {
					    if (bps < 60000)
						bps = 56000;
					    else if (bps < 66000)
						bps = 64000;
					    else
						bps = trunc(bps/8000.0)*8000.0;
					    avail_bps = prev_avail_bps + bps;
					    get_hi_lo_bps();
					}
					newest->bps = bps;
				}
			}

		} else {
			/* Ensure that the clock is ticking to turn off the
			 * link if the kernel stops saying there is traffic.
			 *
			 * Ensure the clock does not run out and turn off
			 * the link if there is more load than capacity.
			 */
			if (idle_time.tv_sec > clk.tv_sec+idle_delay
			    || load_bps >= lo_bps)
				idle_time.tv_sec = clk.tv_sec+idle_delay;

			busy_time.tv_sec = TIME_NEVER;
		}
		log_beep_act("", beep_name, istr, nstr, &ibuf.un.beep);
		break;

	case BEEP_DEAD:
	case BEEP_CP_BAD:
		(void)sprintf(dbgmsg, "%s: %s", beep_name, istr);
		log_ipkt(2, nstr, dbgmsg);
		break;

	case BEEP_FRAME:
		/* Deal with MP frames received before the device
		 * is under the STREAMS mux.  This can happen if
		 * the peer gets ahead of us.
		 */
		if (fd == ppp0.dv.devfd
		    && ibuf.proto == PPP_MP) {
			(void)sprintf(dbgmsg, "read %#x MP bytes:%s",
				      ibuf_info_len, istr);
			log_ipkt(6, nstr, dbgmsg);
			if (ibuf.bits != 0) {
				log_ipkt(log_mp_ipkt_str(BEEP_MP_ERROR,
							 istr, dbgmsg),
					 nstr, dbgmsg);
				return 0;
			}
			log_ipkt(log_mp_ipkt_str(BEEP_MP, istr, dbgmsg),
				 nstr, dbgmsg);
			if (mp_recv_ssn) {
				sn = ibuf.un.mp_s.sn;
				i = sizeof(struct mp_s);
			} else {
				sn = ibuf.un.mp_l.sn;
				i = sizeof(struct mp_s);
			}
			if (ibuf.un.mp_s.mp_bits & MP_B_BIT) {
				if (mp_sn != -2)
					log_debug(1,"",
						  "missing MP fragment"
						  " after %d",
						  mp_sn);
				mp_len = 0;
				bzero(&mp_buf,sizeof(mp_buf));
				mp_buf.type = BEEP_FRAME;
				mp_sn = sn;
			} else if (mp_sn+1 != sn) {
				log_debug(1,"",
					  "missing MP fragment before %d", sn);
				mp_sn = -2;
				return 0;
			} else {
				mp_sn = sn;
			}
			/* expand compressed protocol field */
			while (mp_len == 0
			       && !(mp_buf.proto & 1)
			       && ibuf_info_len > i) {
				mp_buf.proto <<= 8;
				mp_buf.proto |= ibuf.un.info[i++];
			}
			ibuf_info_len -= i;
			if (ibuf_info_len < 0
			    || ibuf_info_len + mp_len >= PPP_MAX_MTU) {
				log_debug(1,"",
					  "bad early MP packet %d bytes long",
					  ibuf_info_len + mp_len);
				mp_sn = -2;
				return 0;
			}
			bcopy(&ibuf.un.info[i],&mp_buf.un.info[mp_len],
			      ibuf_info_len);
			mp_len += ibuf_info_len;
			if (!(ibuf.un.mp_s.mp_bits & MP_E_BIT))
				return 0;

			mp_buf.bits = MP_B_BIT|MP_E_BIT;
			ibuf_info_len = mp_len;
			bcopy(&mp_buf,&ibuf,sizeof(ibuf));
			mp_len = 0;
			mp_sn = -2;

			/* forget MP null frames */
			if (ibuf_info_len == 0) {
				(void)sprintf(dbgmsg, "ignore MP null:%s",
					      istr);
				return 0;
			}
			(void)sprintf(dbgmsg,
				      "assemble %#x bytes: %sproto=%#04x",
				      ibuf_info_len, istr,
				      ibuf.proto);
		} else {
			(void)sprintf(dbgmsg, "read %#x bytes: %sproto=%#04x",
				      ibuf_info_len, istr,
				      ibuf.proto);
		}
		log_ipkt(3, nstr, dbgmsg);
		break;

	case BEEP_FCS:
	case BEEP_ABORT:
	case BEEP_TINY:
	case BEEP_BABBLE:
		(void)sprintf(dbgmsg, "read %#x bytes: %s %sproto=%#04x",
			      imsg.len-PPP_BUF_HEAD-4,
			      beep_name, istr, ibuf.proto);
		log_ipkt(2, nstr, dbgmsg);
		break;

	case BEEP_MP:
	case BEEP_MP_ERROR:
		log_ipkt(log_mp_ipkt_str(ibuf.type, istr, dbgmsg),
			 nstr, dbgmsg);
		break;
	}

	return 1;
}


/* Do not change the number of lines for awhile.
 */
static void
no_beep(int delta)			/* 1=added line, -1=lost line,
					 * 0=no change */
{
	/* No more discretionary lines for a while, so reset the idle
	 * and busy timers, and ignore the kernel for a while.
	 */
	beepok_time.tv_sec = clk.tv_sec+BEEP/HZ;
	idle_time.tv_sec = ((numdevs == 0)
			    ? TIME_NEVER
			    : clk.tv_sec+MAX(idle_delay,BEEP/HZ*2));
	busy_time.tv_sec = TIME_NEVER;

	if (delta > 0) {
		/*  Reset overall failure backoff after success. */
		add_time.tv_sec = TIME_NOW;
		add_backoff = BACKOFF0;

	} else if (delta < 0) {
		/* Do not add a line soon after losing one. */
		add_time.tv_sec = clk.tv_sec + add_backoff;
	}

}


/* send a PPP packet
 */
void
ppp_send(struct ppp *ppp,
	 struct ppp_buf *buf,
	 int info_len)			/* length of the INFO field */
{
#define SEND_LOG_LVL 3
#define ADDBYTE(c) {if (PPP_ASYNC_MAP((c), ppp->lcp.tx_accm)) {		\
			*wptr++ = PPP_ESC; *wptr++ = (c) ^ PPP_ESC_BIT;	\
		} else {						\
			*wptr++ = (c);					\
		}							\
	}
	struct ppp_buf wbuf;
	struct strbuf omsg;
	u_short fcs;
	u_char c, *wptr;
	int i;
	char *mpstr, dbgmsg[50];

	if (ppp->dv.devfd >= 0) {
		if (ppp->dv.sync == SYNC_DEFAULT)
			log_complain(ppp->lcp.fsm.name,
				     "sync undecided before output");

		wptr = &wbuf.frame;

		/* Start every async frame with a probably redundant
		 * framing byte.
		 */
		if (ppp->dv.sync == SYNC_OFF)
			*wptr++ = PPP_FLAG;

		fcs = PPP_FCS_INIT;
		ADDBYTE(PPP_ADDR);
		fcs = PPP_FCS(fcs,PPP_ADDR);
		ADDBYTE(PPP_CNTL);
		fcs = PPP_FCS(fcs,PPP_CNTL);

		/* We cannot send MP packets until the link is bundled
		 */
		if (buf->bits != 0)
			log_complain("","sending bogus MP bits %#x",buf->bits);

		/* start sending the frame */
		c = buf->proto>>8;
		ADDBYTE(c);
		fcs = PPP_FCS(fcs,c);
		c = buf->proto;
		ADDBYTE(c);
		fcs = PPP_FCS(fcs,c);
		for (i = 0; i < info_len; i++) {
			ADDBYTE(buf->un.info[i]);
			fcs = PPP_FCS(fcs,buf->un.info[i]);
		}
		/* end the frame */
		if (ppp->dv.sync == SYNC_OFF) {
			fcs ^= PPP_FCS_INIT;
			ADDBYTE(fcs & 0xff);
			ADDBYTE(fcs >> 8);
			*wptr++ = PPP_FLAG;
		}
		omsg.buf = (char*)&wbuf.frame;
		omsg.len = wptr-&wbuf.frame;

		if (debug >= SEND_LOG_LVL) {
			(void)sprintf(dbgmsg, "write %#x bytes: proto=%#04x",
				      info_len, buf->proto);
			log_pkt(SEND_LOG_LVL, ppp->link_name, dbgmsg,0,
				info_len, buf->un.info);
			(void)sprintf(dbgmsg, "      %#x raw bytes:",omsg.len);
			log_pkt(SEND_LOG_LVL+1, ppp->link_name, dbgmsg,0,
				omsg.len, (u_char*)omsg.buf);
		}

		if (0 > putmsg(ppp->dv.devfd, 0,&omsg,0))
			log_errno("putmsg()","");

	} else {
		wbuf.type = BEEP_FRAME;
		wbuf.dev_index = ppp->dv.dev_index;
		if (buf->bits != 0) {
			mpstr = "(MP)";
			wbuf.bits = MP_B_BIT|MP_E_BIT;
		} else {
			mpstr = "";
			wbuf.bits = 0;
		}
		wbuf.frame = PPP_FLAG;
		wbuf.addr = PPP_ADDR;
		wbuf.cntl = PPP_CNTL;
		bcopy(&buf->proto, &wbuf.proto, info_len+sizeof(wbuf.proto));
		omsg.buf = (char*)&wbuf;
		omsg.len = &wbuf.un.info[info_len]-(u_char*)&wbuf;

		if (debug >= SEND_LOG_LVL) {
			(void)sprintf(dbgmsg,
				      "send %#x bytes: index=%d proto=%#04x%s",
				      info_len, wbuf.dev_index,
				      buf->proto, mpstr);
			log_pkt(SEND_LOG_LVL, ppp->link_name, dbgmsg,0,
				info_len, buf->un.info);
		}

		if (0 > putmsg(modfd, 0,&omsg,0))
			log_errno("putmsg()","");
	}

#undef ADDBYTE
#undef SEND_LOG_LVL
}



/* process an incoming packet.
 *	We already know that the buffer is a PPP packet and not a message
 *	from the driver.
 *
 * The driver uncompresses the address, control, and protocol fields
 *	on input, and strips the flag, address, control, and FCS fields
 */
static void
ipkt(struct ppp *ppp)
{
	static struct {
	    u_short act;
	    u_short proto;
	    char    *msg;
	} *tp, t[] = {
	    {2, PPP_IP,	    "stray IP packet"},
	    {2, PPP_VJC_COMP,"stray IP packet"},
	    {2, PPP_VJC_UNCOMP, "stray IP packet"},
	    {2, PPP_MP,	    "stray MP packet"},
	    {2, PPP_CP,	    "stray compressed packet"},
	    {1, PPP_CP_MULTI,"compression below multi-link"},
	    {1, PPP_CCP_MULTI,"compression below multi-link"},
	    {1, PPP_LQR,    "LQR packet"},
	    {1, PPP_BCP,    "Bridging Control Protocol (BCP)"},
	    {1, PPP_BP,	    "Bridging Protocol (BP)"},
	    {1, PPP_BACP,   "Bandwidth Allocation Control Protocol (BACP)"},
	    {1, PPP_BAP,    "Bandwidth Allocation Protocol (BAP)"},
	    {1, PPP_IPXCP,  "IPX Control Protocol"},
	    {1, PPP_IPXP,   "IPX Protocol"},
	    {1, 0x3f,	    "NETBIOS Framing"},
	    {1, 0x803f,	    "NETBIOS Framing Control Protocol"},
	    {1, 0x29,	    "Appletalk"},
	    {1, 0x8029,	    "Appletalk Control Protocol"},
	    {1, 0x53,	    "Encryption"},
	    {1, 0x8053,	    "Encryption Control Protocol"},
	    {1, 0x55,	    "Individual Link Encryption"},
	    {1, 0x8055,	    "Individual Link Encryption Control Protocol"},
	    {1,	0,	    "unrecognized protocol"}
	};

	switch (ibuf.proto) {
	case PPP_LCP:
		lcp_ipkt(ppp);
		break;
	case PPP_PAP:
		pap_ipkt(ppp);
		break;
	case PPP_CHAP:
		chap_ipkt(ppp);
		break;
	case PPP_IPCP:
		ipcp_ipkt(ppp);
		break;
	case PPP_CCP:
		ccp_ipkt(mp_ppp ? mp_ppp : ppp);
		break;

	default:
		for (tp = t; tp->proto != 0 && tp->proto != ibuf.proto; tp++)
			continue;
		switch (tp->act) {
		case 1:
			log_debug(tp->act, ppp->link_name,
				  "Protocol-Rejecting %s %#x",
				  tp->msg, ibuf.proto);
			lcp_pj(ppp, ppp->lcp.fsm.name);
			break;
		case 2:
			log_ipkt(2, ppp->link_name, tp->msg);
			break;
		}
	}
}


/* log the INFO field of a packet
 */
static void
log_pkt(int lvl,
	char* name,
	char *msg1, char *msg2,
	int buf_len,
	u_char *buf)
{
#	define PAT_SIZE 200
	char str[PAT_SIZE+3+4+2];
	int pos, b, i, cmode, cmode_ok, c;

	if (debug < lvl)
		return;

	(void)strcpy(str, msg1);
	if (msg2)
		(void)strcat(str, msg2);
	pos = strlen(str);

	/* See if there is an ASCII string in the packet.
	 * Unless there is at least one string of 4 characters, do not
	 * decode any of the packet as ASCII.
	 */
	i = 0;
	cmode_ok = 0;
	b = 0;
	while (b < buf_len) {
		c = buf[b++];
		if (isalnum(c) || c == ' ') {
			if (++i >= 4) {
				cmode_ok = 1;
				break;
			}
		} else {
			i = 0;
		}
	}

	b = 0;
	cmode = 0;
	while (b < buf_len) {
		if (pos >= PAT_SIZE) {
			(void)strcpy(&str[pos],
				     cmode ? "\" ..." : " ...");
			cmode = 0;
			break;
		}
		c = buf[b++];
		/* It is too bad that ctype has been "improved" so
		 * that isprint() no longer gives reliable answers.
		 *
		 * Display the current character as printable if it
		 * is and if previous characters were or if at least
		 * two more printable characters follow it.
		 */
		if (cmode_ok
		    && (c >= ' ' && c < 0x7f)
		    && (cmode
			|| (b+2 < buf_len
			    && (buf[b] >= ' ' && buf[b] < 0x7f)
			    && (buf[b+1] >= ' ' && buf[b+1] < 0x7f)))) {
			i = sprintf(&str[pos],
				    cmode ? "%c" : " \"%c", c);
			cmode = 1;
		} else {
			i = sprintf(&str[pos],
				    cmode ? "\" %02x" : " %02x", c);
			cmode = 0;
		}
		if (i < 0)
			break;
		pos += i;
	}
	if (cmode)
		(void)strcat(str,"\"");
	log_debug(lvl, name, "%s", str);
}


/* log the most recent input packet
 */
void
log_ipkt(int lvl, char* name, char *msg)
{
	char *bstr;

	switch (ibuf.bits) {
	case 0:
		bstr = 0;
		break;
	case MP_B_BIT:
		bstr = "(MP_B)";
		break;
	case MP_E_BIT:
		bstr = "(MP_E)";
		break;
	case MP_B_BIT|MP_E_BIT:
		bstr = "(MP)";
		break;
	default:
		bstr = "(MP?)";		/* (in case RFC 1717 changes) */
		break;
	}
	log_pkt(lvl, name, msg,bstr, ibuf_info_len, ibuf.un.info);
}


/* log an activity indication
 */
static void
log_beep_act(char *act,
	     char *beep_name,
	     char *istr,
	     char *nstr,
	     struct beep *bp)
{
	log_debug(2,"", "%s%s: %s%-4s %d %d",
		  act, beep_name, istr, nstr, bp->raw_ibytes, bp->raw_obytes);
}


static int
log_mp_ipkt_str(int type,
		char *istr,
		char *dbgmsg)
{
	(void)sprintf(dbgmsg, "%sMP %s%s-%#x", istr,
		      (type == BEEP_MP) ? "" : "Error ",
		      ((ibuf.un.mp_s.mp_bits == MP_B_BIT) ? "B-"
		       : (ibuf.un.mp_s.mp_bits == MP_E_BIT) ? ".E"
		       : (ibuf.un.mp_s.mp_bits != 0) ? "BE"
		       : ".."),
		      (mp_recv_ssn) ? ibuf.un.mp_s.sn : ibuf.un.mp_l.sn);

	ibuf.bits &= ~(MP_B_BIT|MP_E_BIT);

	if (type == BEEP_MP) {
		if ((!mp_recv_ssn && ibuf.un.mp_l.rsvd != 0)
		    ||  ibuf.un.mp_s.rsvd != 0)
			return 1;
		return 5;
	} else {
		return 2;
	}
}


/* do a STREAMS ioctl
 */
int					/* <0 if error, and message logged */
do_strioctl(struct ppp *ppp,
	    int cmd,
	    struct ppp_arg *arg,
	    char *msg)
{
	struct strioctl strioctl;
	struct ppp_arg arg0;
	int res;
	int fd;

	strioctl.ic_cmd = cmd;
	strioctl.ic_timout = 0;
	if (!arg) {
		arg = &arg0;
		bzero(&arg0,sizeof(arg0));
	}
	strioctl.ic_dp = (char*)arg;
	strioctl.ic_len = sizeof(*arg);
	if (!ppp) {
		/* do something to the entire multilink bundle */
		arg->dev_index = (__uint32_t)-1;
		fd = modfd;
	} else if (ppp->dv.devfd >= 0) {
		/* do something to a link not yet part of the bundle */
		arg->dev_index = (__uint32_t)-1;
		fd = ppp->dv.devfd;
	} else {
		/* do something to a link that is part of the bundle */
		arg->dev_index = ppp->dv.dev_index;
		fd = modfd;
	}
	arg->version = 0;

	res = 1;
	if (0 > (res = ioctl(fd, I_STR, &strioctl))) {
		log_errno(msg,"");
		log_complain("","%s--fd=%d dev_index=%d",
			     msg, fd, arg->dev_index);

	} else if (arg->version != IF_PPP_VERSION) {
		log_complain("","%s--wrong kernel module version %d, not %d",
			     msg, arg->version, IF_PPP_VERSION);
		res = -2;

	} else {
		log_debug(7,"","%s--success, fd=%d dev_index=%d",
			  msg, fd, arg->dev_index);
	}

	return res;
}


/* do a boolean STREAMS ioctl
 */
int					/* <0 if error, and message logged */
do_strioctl_ok(struct ppp *ppp,
	    int cmd,
	    u_char ok,
	    char *msg)
{
	struct ppp_arg arg;

	bzero(&arg,sizeof(arg));
	arg.v.ok = ok;
	return do_strioctl(ppp, cmd, &arg, msg);
}



/* see about the file and pipe for `pppstat` */
static void
ck_status_poke(void)
{
	struct stat st1, st2;

	if (status_poke_path[0] == '\0')
		return;			/* too early */

	if (0 > lstat(status_poke_path, &st1)) {
		if (errno == ENOENT) {
			if (status_poke_fd >= 0)
				log_complain("","%s disappeared",
					     status_poke_path);
		} else {
			log_errno("stat() ", status_poke_path);
			(void)close(status_poke_fd);
			status_poke_fd = -1;
			return;
		}
		st1.st_mode = 0;

	} else if (!S_ISFIFO(st1.st_mode)) {
		(void)unlink(status_poke_path);
		st1.st_mode = 0;
	}

	if (!S_ISFIFO(st1.st_mode)) {
		if (status_poke_fd >=  0) {
			(void)close(status_poke_fd);
			status_poke_fd = -1;
		}
		if (0 > mknod(status_poke_path, S_IFIFO, 0)) {
			log_errno("mknod() ", status_poke_path);
			return;
		}
		if (0 > chmod(status_poke_path, (S_IRUSR|S_IWUSR
						 | S_IRGRP|S_IWGRP
						 | S_IROTH|S_IWOTH))) {
			log_errno("chmod() ", status_poke_path);
			return;
		}
	}

	if (status_poke_fd >= 0
	    && (0 > fstat(status_poke_fd, &st2)
		|| st1.st_dev != st2.st_dev
		|| st1.st_ino != st2.st_ino)) {
		(void)close(status_poke_fd);
		status_poke_fd = -1;
	}

	if (status_poke_fd < 0) {
		status_poke_fd = open(status_poke_path,O_RDWR|O_NDELAY,0);
		if (status_poke_fd < 0) {
			log_errno("open() ", status_poke_path);
			return;
		}

		strcpy(ppp_status.ps_rem_sysname, rem_sysname);
		ppp_status.ps_lochost = lochost.sin_addr;
		ppp_status.ps_remhost = remhost.sin_addr;

		update_status();
	}
}



/* see about the rendezvous point
 */
static void
ck_rend(char* msg_pat)
{
	if (make_rend(1)) {
		log_complain("", msg_pat, remote);
		unmade_rend();
		cleanup(1);
	}

	ck_status_poke();
}


/* Poke the existing daemon (if there is one) and wait for it to kill us.
 */
void
do_rend(struct ppp *ppp)
{
	ppp->lochost.sin_addr.s_addr = (def_lochost
					? 0
					: lochost.sin_addr.s_addr);
	ppp->remhost.sin_addr.s_addr = (def_remhost
					? 0
					: remhost.sin_addr.s_addr);
	(void)strcpy(ppp->loc_epdis, loc_epdis);
	(void)strcpy(ppp->rem_epdis, rem_epdis);

	unmade_rend();			/* do not remove the node if we die */

	log_debug(1,"","pass device");

	no_signals(killed);

	if (0 > write(rendnode, &ppp0, sizeof(ppp0)))
		bad_errno("write(caller poke) ",ppp->dv.line);

	/* quit immediately if only reporting failure
	 */
	if (ppp->nowait == 2)
		exit(0);

	log_debug(4,"","waiting to be killed");

	/* After the existing daemon has had time to grab the devices,
	 * close them.  They main device must not be closed too soon or
	 * the STREAMS stack will be messed up.
	 */
	sginap(5*HZ);
	if (ppp->nowait != 0) {
		log_complain("","no signal received");
		exit(0);
	}

	die(ppp);
}


/* set MTU in the kernel.
 */
static int				/* 0=failed */
set_mtu(struct ppp *ppp)
{
	struct ppp_arg arg;

	bzero(&arg,sizeof(arg));

	kern_ip_mtu = (ppp->conf.ip_mtu < PPP_MIN_MTU
		       ? PPP_DEF_MRU
		       : ppp->conf.ip_mtu);
	if (kern_ip_mtu > ppp->lcp.neg_mtru)
		kern_ip_mtu = ppp->lcp.neg_mtru;

	arg.v.mtu.mtru = ppp->lcp.neg_mtru;
	arg.v.mtu.ip_mtu = kern_ip_mtu;

	if (qmax >= 0) {
		arg.v.mtu.qmax = qmax;
	} else {
		/* Pick a queue length that will avoid dropping packets
		 * if the user picks a tiny MTU.  Make it big enough
		 * to handle one full stream of data transmitted
		 * and ACKs for one full stream of data received.
		 */
		arg.v.mtu.qmax = 1+((DEF_WINDOW*3/2)
				    /(kern_ip_mtu - sizeof(struct ip)));
		if (arg.v.mtu.qmax < IFQ_MAXLEN)
			arg.v.mtu.qmax = IFQ_MAXLEN;
	}

	if (bigxmit == -1) {
		arg.v.mtu.bigxmit = kern_ip_mtu/4;
		if (arg.v.mtu.bigxmit < PPP_MIN_MTU)
			arg.v.mtu.bigxmit = PPP_MIN_MTU;
	} else {
		arg.v.mtu.bigxmit = bigxmit;
	}

	arg.v.mtu.telnettos = telnettos;

	if (0 > do_strioctl(0, SIOC_PPP_MTU, &arg, "_MTU"))
		return 0;

	return 1;
}


/* Configure the IP part of the link.
 */
static int				/* 0=failed */
make_ip(struct ppp *ppp)
{
	static char remoteaddr[sizeof("REMOTEADDR=111.222.333.444")+1];
	struct stat sbuf;
	char *p, *p1;
	int i;
#	define RCMDV "route -n "
#	define RCMD  "route -n -q "
#	define SIZE_RCMD sizeof(RCMD "111.222.333.444")	/* worst case */
	char *rcmd;


	/* Poke the existing daemon and wait for it to kill us
	 * if it exists.
	 */
	(void)add_rend_name("",remhost_str);
	if (make_rend(1) || add_pid == 0) {
		if (reconfigure) {
			log_complain("","premature make_ip()");
			return 0;
		}
		do_rend(ppp);
	}

	/* set up the interface */
	if (modfd < 0) {
		modfd = open("/dev/if_ppp", O_RDWR | O_NONBLOCK);
		if (0 > modfd)
			bad_errno("open() ","/dev/if_ppp)");

		/* get the unit number */
		if (0 > fstat(modfd, &sbuf)) {
			log_errno("fstat(unit#)","");
			return 0;
		}
		ppp->dv.unit = minor(sbuf.st_rdev);
		sprintf(ifname, "ppp%d", ppp->dv.unit);

		(void)sprintf(status_path, RENDDIR_STATUS_PAT,
			      ppp0.dv.unit);
		(void)sprintf(status_poke_path, RENDDIR_STATUS_POKE_PAT,
			      ppp0.dv.unit);
		ck_status_poke();

		proxy_arp();
		set_ip();
		def_lochost = 0;	/* addresses cannot change now */
		def_remhost = 0;

		if (kern_ip_mtu <= 0
		    && !set_mtu(ppp))
			return 0;

		if (noicmp)
			(void)do_strioctl(0, SIOC_PPP_NOICMP, 0, "_NOICMP");

		if (afilter != 0) {
			struct strioctl strioctl;

			strioctl.ic_cmd = SIOC_PPP_AFILTER;
			strioctl.ic_timout = 0;
			strioctl.ic_len = sizeof(afilter);
			strioctl.ic_dp = (char*)&afilter;
			if (0 > ioctl(modfd, I_STR, &strioctl))
				log_errno("_AFILTER","");
		}

		/* put the remote hostname and string into the environment
		 * for the scripts and route commands.
		 */
		sprintf(remoteaddr, "REMOTEADDR=%s", remhost_str);
		(void)putenv(remoteaddr);

		/* run optional initial script */
		if (start_cmd != 0)
			call_system("", "start_cmd: ", start_cmd, 1);

		/* Install a default route if asked.
		 *	Decide each time whether to be verbose in case
		 *	debugging has been turned on or off.
		 */
		if (add_route != 0) {
			if (add_route[0] == '\0'
			    || (add_route[0] == '-'
				&& add_route[1] == '\0')) {
				free(add_route);
#ifdef PPP_IRIX_53
				add_route=strdup("add default $REMOTEADDR 1");
#else
				add_route = strdup("add default $REMOTEADDR");
#endif
			}

			/* build `route delete ...` command */
			if (conf_del_route
			    && (del_route || !strncmp(add_route,"add ",4))) {
				if (!del_route) {
					/* find first 2 or 3 words after
					 * "add" in add-route command.
					 */
					p1 = &add_route[4];
					while (*p1 == ' ')
					    p1++;
					p = p1;
					if (!strncmp(p,"net ",4)
					    || !strncmp(p,"host ",5)) {
					    while (*p != ' ')
						p++;
					    while (*p == ' ')
						p++;
					}
					while (*p != ' ' && *p != '\0')
					    p++;
					while (*p == ' ')
					    p++;
					while (*p != ' ' && *p != '\0')
					    p++;
					if (del_route)
					    free(del_route);
					i = sizeof("delete ") +strlen(p1) +1;
					del_route = malloc(i);
					strcpy(del_route, "delete ");
					strncat(del_route, p1, p-p1);
					del_route[i-1] = '\0';
				}
				p = malloc(SIZE_RCMD + strlen(del_route));
				strcpy(p, debug ? RCMDV : RCMD);
				strcat(p, del_route);
				free(del_route);
				del_route = p;
			}

			/* build `route add ...` command
			 * Preface it with the `route delete` command to
			 * deal with any previous stray routes.
			 */
			if (del_route != 0) {
				rcmd = malloc(strlen(del_route)+2
					      +SIZE_RCMD
					      +strlen(add_route)+1);
				sprintf(rcmd, "%s; %s%s",
					del_route,
					debug ? RCMDV : RCMD, add_route);
			} else {
				rcmd = malloc(SIZE_RCMD+strlen(add_route)+1);
				sprintf(rcmd, "%s%s",
					debug ? RCMDV : RCMD, add_route);
			}

			/* execute the `route add` command */
			call_system("", "add_route: ", rcmd, 1);
		}

#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62)
		/* try to get the routes restored faster */
		if (debug) {
			kludge_stderr();
			(void)system("set -x; killall -v -ALRM routed 1>&2");
			(void)system("set -x; killall -v -USR1 gated 1>&2");
		} else {
			(void)system("killall -ALRM routed 1>&2");
			(void)system("killall -USR1 gated 1>&2");
		}
#endif
	}

	return 1;
}


/* Turn MP on or off in the kernel.
 */
int					/* >=0 on success */
set_mp(struct ppp *ppp)
{
	struct ppp *ppp1;
	struct ppp_arg arg;


	/* Do nothing if MP could not yet be on.
	 * If we have not happy with LCP, we cannot know if MP is
	 * going to be used or not.  However, we may have previously
	 * been completed the Establish phase and only recently received
	 * a new LCP Configure-Request or Authenication packet.
	 */
	if (ppp->phase <= AUTH_PHASE
	    && !mp_known)
		return 0;

	mp_on = ppp->lcp.neg_mp;
	if (!mp_known) {
		log_debug(4,ppp->link_name,
			  mp_on ? "MP known on" : "MP known off");
		mp_known = 1;
	}

	if (mp_on) {
		if (mp_ppp != ppp) {
			if (mp_ppp != 0) {
				log_debug(6,ppp->link_name,
					  "switch mp_ppp from %s",
					  mp_ppp->link_name);
				ppp->ccp = mp_ppp->ccp;
				ppp->ipcp = mp_ppp->ipcp;
			}
			mp_ppp = ppp;
			fix_names();
		}
	} else {
		if (mp_ppp != 0) {
			mp_ppp = 0;
			mp_ncp_bits = 0;
			fix_names();
		}
	}

	/* We can only note MP if the link is not yet part of the bundle.
	 */
	if (ppp->dv.devfd >= 0) {
		log_debug(6,ppp->link_name,
			  "postpone setting MP since devfd=%d dev_index=%d",
			  ppp->dv.devfd, ppp->dv.dev_index);
		return 0;
	}

	bzero(&arg,sizeof(arg));
	if (ppp->lcp.neg_mp) {
		mp_ncp_bits = MP_B_BIT|MP_E_BIT;
		arg.v.mp.on = 1;
		mp_send_ssn = ppp->lcp.neg_send_ssn;
		mp_recv_ssn = ppp->lcp.neg_recv_ssn;
		arg.v.mp.min_frag = (ppp->conf.frag_size != 0
				     ? ppp->conf.frag_size
				     : PPP_MIN_MTU);
		if (arg.v.mp.min_frag > PPP_MIN_MTU)
			arg.v.mp.min_frag = PPP_MIN_MTU;
		arg.v.mp.max_frag = ppp->lcp.neg_mtu;
		if (mp_send_ssn) {
			arg.v.mp.max_frag -= sizeof(struct mp_s);
			arg.v.mp.min_frag -= sizeof(struct mp_s);
		} else {
			arg.v.mp.max_frag -= sizeof(struct mp_l);
			arg.v.mp.min_frag -= sizeof(struct mp_l);
		}
		arg.v.mp.reasm_window = ((ppp->conf.reasm_window > 0)
					 ? ppp->conf.reasm_window
					 : 32);
		arg.v.mp.send_ssn = mp_send_ssn;
		arg.v.mp.recv_ssn = mp_recv_ssn;

		/* We must always use MP headers if MTRU > MTU.
		 * So find the smallest MTU and compare that to the MTRU.
		 */
		if (!(arg.v.mp.must_use_mp = ppp->conf.mp_headers)) {
			for (ppp1 = &ppp0;
			     ppp1 != 0;
			     ppp1 = (struct ppp*)ppp1->dv.next) {
				if (ppp1->dv.line[0] == '\0')
					continue;
				if (ppp1->lcp.neg_mtru > ppp1->lcp.neg_mtu) {
					arg.v.mp.must_use_mp = 1;
					break;
				}
			}
		}
		arg.v.mp.mp_nulls = ppp->conf.mp_nulls;
		if (debug >=4)
			arg.v.mp.debug = 2;
		else if (debug >= 2)
			arg.v.mp.debug = 1;
		else
			arg.v.mp.debug = 0;
	} else {
		arg.v.mp.max_frag = PPP_MAX_MTU;
	}

	if (!bcmp(&arg.v.mp, &ppp->lcp.arg_mp_set,
		  sizeof(ppp->lcp.arg_mp_set)))
		return 0;

	log_debug(6,ppp->link_name,
		  mp_on?"turn on MP: headers %s, mp.debug=%d" : "turn off MP",
		  arg.v.mp.must_use_mp ? "required" : "optional",
		  arg.v.mp.debug);
	if (0 > do_strioctl(ppp, SIOC_PPP_MP, &arg, "_MP"))
		return -1;
	bcopy(&arg.v.mp, &ppp->lcp.arg_mp_set,
	      sizeof(ppp->lcp.arg_mp_set));
	return 0;
}


/* Connect this line to the others
 */
int					/* >=0 on success */
link_mux(struct ppp *l_ppp)
{
	struct ppp *ppp = 0;
	struct ppp_arg arg;
	int i;


	/* If the IP addresses are not yet known, then we cannot link
	 * the line to the STREAMS mux.
	 */
	if (lochost.sin_addr.s_addr == 0
	    || def_lochost) {
		log_debug(6,l_ppp->link_name,
			  "postpone I_LINK: local IP address=%s"
			  " and def_lochost=%d",
			  ip2str(lochost.sin_addr.s_addr),
			  def_lochost);
		return set_mp(l_ppp);
	}
	if (remhost.sin_addr.s_addr == 0
	    || def_remhost) {
		log_debug(6,l_ppp->link_name,
			  "postpone I_LINK: remote IP address=%s"
			  " and def_remhost=%d",
			  ip2str(remhost.sin_addr.s_addr),
			  def_remhost);
		return set_mp(l_ppp);
	}

	/* ensure that the IP interface (and so mux/driver) exists
	 */
	if (!make_ip(l_ppp))
		return -1;

	/* Link the device STREAMS stack to the driver or mux.
	 */
	if (l_ppp->dv.devfd >= 0) {
		/* We cannot put the link under the STREAMS mux until the RX
		 * ACCM is known, since switching the RX ACCM requires
		 * stepping input one packet at time, and since that facility
		 * is available only to links not under the mux.  It does not
		 * work for links under the mux, because IP packets do not go
		 * to the daemon, so the daemon cannot know when to trigger
		 * another input packet.
		 */
		if (l_ppp->phase < AUTH_PHASE) {
			log_debug(6,l_ppp->link_name,
				  "postpone I_LINK: since in %s Phase",
				  phase_name(l_ppp->phase));
			return set_mp(l_ppp);
		}

		i = ioctl(modfd,I_LINK,l_ppp->dv.devfd);
		if (i < 0) {
			log_errno("I_LINK","");
			return -1;
		}
		l_ppp->dv.dev_index = i;
		l_ppp->dv.devfd_save = l_ppp->dv.devfd;
		l_ppp->dv.devfd = -1;
		log_debug(6,l_ppp->link_name,"I_LINK to MUX with index %d",
			  l_ppp->dv.dev_index);

		/* ensure the mux knows about the tx ACCM, MTU, etc. */
		if (0 > lcp_set(l_ppp,1))
			return -1;
	}

	/* Turn MP on or off in the kernel.
	 * A side effect of turning on MP is enabling IP input for the
	 * individual link, and	disabling/turning on the compression gates
	 * for the individual link in the PPP framer, since the module is in
	 * charge with MP.
	 * Note that this can change mp_ppp;
	 */
	if (0 > set_mp(l_ppp))
		return -1;
	ppp = mp_ppp ? mp_ppp : l_ppp;

	/* Turn on input in the framer if we just now put the link
	 * under the mux.  This is redundant if MP is on.
	 */
	if (l_ppp->in_stop != 0) {
		log_debug(6,l_ppp->link_name,
			  "SIOC_PPP_RX_OK=1 since in_stop=%d", l_ppp->in_stop);
		if (0 > do_strioctl_ok(l_ppp, SIOC_PPP_RX_OK, 1,
				       "link_mux ioctl(mod, RX_OK on)"))
			return -1;
		l_ppp->in_stop = 0;
	}

	/* That is all for now if it is not yet time to turn on IP.
	 */
	if (l_ppp->phase != NET_PHASE || ppp->phase != NET_PHASE)
		return 1;

	/* Start IP header compression mechanisms for this link.
	 * This is necessary if we just created the mux module
	 * (so it does not know about VJ compression) or if
	 * IPCP renegotiated.
	 *
	 * If VJ header compression is turned off, then use the
	 * machinery only to count active TCP connections.
	 */
	bzero(&arg,sizeof(arg));
	if (!ppp->ipcp.tx_comp) {
		arg.v.vj.tx_slots = DEF_VJ_SLOT;
		arg.v.vj.rx_slots = MIN_VJ_SLOT;
	} else {
		arg.v.vj.tx_comp = ppp->ipcp.tx_comp;
		arg.v.vj.tx_compslot = ppp->ipcp.tx_compslot;
		arg.v.vj.tx_slots = ppp->ipcp.tx_slots;
		arg.v.vj.rx_slots = ppp->ipcp.rx_slots;
	}
	arg.v.vj.link_num = l_ppp->link_num-1;
	if (0 > do_strioctl(l_ppp, SIOC_PPP_VJ, &arg, "_VJ"))
		return -1;

	/* let the bundle use the link */
	return do_strioctl_ok(l_ppp, SIOC_PPP_TX_OK,
			      1, "ioctl(link_mux() TX_OK on)");
}


/* reject a rendezvous
 */
static void
rend_rej(struct ppp *ppp, char *pat, ...)
{
	char pbuf[200];

	va_list args;

	va_start(args, pat);
	vsprintf(pbuf, pat, args);
	va_end(args);

	log_complain(ppp->link_name, "reject %s: %s", ppp->dv.line, pbuf);
	ppp->dv.active = TIME_NEVER;
	set_tr_msg(&ppp->lcp.fsm, "%.*s", TR_MSG_MAX-1, pbuf);
	lcp_event(ppp,FSM_CLOSE);
}


char *
epdis_msg(char *type,
	  char *new_epdis,
	  char *old_epdis)
{
	static char buf[60+2*sizeof(loc_epdis)+1];

	if (new_epdis[0] == '\0') {
		sprintf(buf,
			"no %s End Point Discrimitor instead of %s",
			type, old_epdis);
	} else {
		sprintf(buf,
			"%s End Point Discrimitor %s instead of %s",
			type, new_epdis, old_epdis);
	}
	return buf;
}


/* see what the other daemon wants
 */
int					/* 1=took his device */
rendezvous(int mode)			/* 0=kill him.  1=take his device */
{
	struct ppp *ppp, *ppp2;
	struct ppp nppp;
	int newfd;
	FILE *lf;
	pid_t ckpid;
	int i;

	/* We are probably about to add a second link to a multilink bundle.
	 * This new link might last longer than the previous link(s).
	 *
	 * If this daemon was originally started by an incoming phone
	 * call, then it would be bad if when that original line died,
	 * `init` continued to wait for the original process to exit.
	 * The original process would remain to control the second line,
	 * and so `init` would never realize the line is free.
	 *
	 * So start a child to be the main process, and leave the
	 * original process sitting around undead.  As long as the original
	 * process exists, `init` will know the line is busy and not
	 * create a new getty that would mess things up.  When the phone
	 * line is shut down, the original, now undead main process is
	 * killed, which releases `init` to restart `getty` on the line.
	 */
	if (ppp0.dv.line[0] != '\0'
	    && ppp0.nowait == 0
	    && ppp0.dv.rendpid < 0
	    && ppp0.dv.callmode == CALLEE
	    && numdevs == 1
	    && maxdevs > 1) {
		switch (ckpid = fork()) {
		case -1:
			log_errno("demote fork() ","");
			break;
		case 0:
			ppp0.dv.rendpid = ppp0.mypid;
			ppp0.mypid = ppp_status.ps_pid = getpid();
			if (ppp0.dv.lockfile[0] != '\0') {
				lf = fopen(ppp0.dv.lockfile,"r+");
				if (!lf) {
					log_errno("fopen(demote) lockfile",
						  ppp0.dv.lockfile);
				} else {
					(void)fprintf(lf, LOCKPID_PAT,
						      ppp0.mypid);
					(void)fclose(lf);
				}
			}
			break;
		default:
			/* the parent is treated like a helping child */
			log_debug(2,"",
				  "demote PID %d to watch %s and promote %d",
				  ppp0.mypid, ppp0.dv.line, ckpid);
			add_pid = 0;
			no_signals(killed);
			die(&ppp0);
			break;
		}
	}

	/* Read more than the largest possible structure in case the
	 * binary changes.
	 */
	i = read(rendnode,&nppp,sizeof(nppp));
	if (i != sizeof(nppp)) {
		if (i < 0)
			log_errno("read(rendnode) rendezvous", "");
		else
			log_complain("","bogus %d byte rendezvous message",i);
		/* flush junk from the FIFO */
		while (0 < read(rendnode,&nppp,sizeof(nppp)))
			continue;
		return 0;
	}

	if (nppp.version != ppp0.version) {
		log_complain("","incompatible structure version %d",
			     nppp.version);
		/* flush junk from the FIFO */
		while (0 < read(rendnode,&nppp,sizeof(nppp)))
			continue;
		return 0;
	}

	/* combine accounting */
	ppp0.dv.acct.calls += nppp.dv.acct.calls;
	ppp0.dv.acct.attempts += nppp.dv.acct.attempts;
	ppp0.dv.acct.failures += nppp.dv.acct.failures;
	ppp0.dv.acct.answered += nppp.dv.acct.answered;
	ppp0.dv.acct.setup += nppp.dv.acct.setup;
	if (nppp.dv.acct.min_setup != 0) {
		if (ppp0.dv.acct.min_setup > nppp.dv.acct.min_setup
		    || ppp0.dv.acct.min_setup == 0)
			ppp0.dv.acct.min_setup = nppp.dv.acct.min_setup;
		if (ppp0.dv.acct.max_setup < nppp.dv.acct.max_setup
		    || ppp0.dv.acct.max_setup == 0)
			ppp0.dv.acct.max_setup = nppp.dv.acct.max_setup;
	}

	/* Kill the process if it was only reporting failure,
	 */
	if (nppp.dv.lockfile[0] == '\0') {
		if (add_pid == nppp.mypid
		    && modfd >= 0
		    && numdevs == 0
		    && 0 > ioctl(modfd, TCFLSH, 2))
			log_errno("ioctl(modfd,TCFLSH,2)","");
		(void)kill(nppp.mypid, SIGINT);
		log_debug(1,"","accept failed call info from pid %d",
			  nppp.mypid);
		return 0;
	}

	lf = fopen(nppp.dv.lockfile,"r+");
	if (!lf) {
		log_errno("fopen(rendezvous) lockfile ",
			  nppp.dv.lockfile);
		(void)kill(nppp.mypid,SIGINT);
		return 0;
	}
	if (1 != fscanf(lf, "%ld", &ckpid)) {
		log_complain("","bogus rendezvous lock file: %s",
			     nppp.dv.lockfile);
		(void)kill(nppp.mypid,SIGINT);
		(void)fclose(lf);
		return 0;
	}
	if (nppp.mypid != ckpid || ckpid <= 0) {
		log_complain("","wrong PID %ld instead of %ld in lockfile %s",
			     ckpid, nppp.mypid,
			     nppp.dv.lockfile);
		(void)kill(nppp.mypid,SIGINT);
		if (ckpid > 0 && ckpid != ppp0.mypid)
			(void)kill(ckpid,SIGINT);
		(void)fclose(lf);
		return 0;
	}

	if (!mode) {			/* should never happen with PPP */
		ppp = 0;
	} else {
		/* find a free structure */
		ppp2 = &ppp0;
		for (;;) {
			ppp = ppp2;

			/* Use the first free structure, provided
			 * the configuration values are the same.
			 * Assume they are the same if the initial
			 * control file line number is the same.
			 */
			if (ppp->dv.line[0] == '\0'
			    && (ppp->conf_lnum == nppp.conf_lnum
				|| ppp != &ppp0))
				break;
			ppp2 = (struct ppp*)ppp->dv.next;

			/* create a new structure if everything is busy.
			 */
			if (!ppp2) {
				ppp2 = malloc(sizeof(*ppp2));
				bzero(ppp2,sizeof(*ppp2));
				ppp2->link_num = ppp->link_num+1;
				ppp->dv.next = (struct dev*)ppp2;
			}
		}
	}
	if (!ppp) {
		(void)kill(ckpid, SIGINT);
		(void)unlink(nppp.dv.lockfile);
		(void)fclose(lf);
		log_debug(1,"","refuse to rendezvous with pid %ld for %s",
			  ckpid, nppp.dv.line);
		return 0;
	}

	/* Steal his device.
	 */
	newfd = open(nppp.dv.line,O_RDWR|O_NDELAY,0);
	if (newfd < 0) {
		log_errno("failed to steal ", nppp.dv.line);
		(void)kill(ckpid, SIGINT);
		return 0;
	}
	sethup(newfd);

	/* steal the lock */
	(void)rewind(lf);
	(void)fprintf(lf, LOCKPID_PAT, getpid());
	stlock(nppp.dv.lockfile);
	(void)fclose(lf);

	/* use some of the state from the parent */
	init_ppp(ppp);
	nppp.dv.next = ppp->dv.next;
	nppp.dv.rendpid = ckpid;
	nppp.mypid = ppp->mypid;
	nppp.in_stop = 2;
	nppp.dv.devfd = newfd;
	nppp.dv.devfd_save = -1;
	nppp.dv.dev_index = -1;
	nppp.dv.acct = ppp0.dv.acct;
	nppp.dv.acct.sconn = clk.tv_sec;
	nppp.age = ppp->age;
	nppp.auth.recv_names.nxt = 0;
	nppp.link_num = ppp->link_num;
	bzero(&nppp.lcp.arg_lcp_set,sizeof(nppp.lcp.arg_lcp_set));
	bzero(&nppp.lcp.arg_mp_set,sizeof(nppp.lcp.arg_mp_set));

	/* use the rest of the state from the child */
	bcopy(&nppp,ppp,sizeof(*ppp));
	fix_name(ppp);			/* restore names after bcopy() */

	numdevs_inc(ppp);

	/* the other process should have handled any premature HDLC frames.
	 */
	seen_framer = 1;

	log_debug(1,ppp->link_name,"took %s from process %d",
		  ppp->dv.line, ckpid);

	dologin(ppp);

	if (ppp->nowait != 0) {
		/* If we do not need to keep getty from grabbing the device
		 * (because we originated the call), then get rid of the
		 * process.
		 */
		if (ppp->nowait != 2
		    && 0 > kill(ckpid, SIGTERM)
		    && (errno != ESRCH || debug >= 1))
			log_errno("kill(ckpid)","");
		ppp->dv.rendpid = -1;
	}

	if (numdevs > maxdevs) {
		ppp->dv.active = TIME_NEVER;
		if (maxdevs == 0) {
			log_debug(1,ppp->link_name, "operator shutdown");
			set_tr_msg(&ppp->lcp.fsm, "operator shutdown");
		} else {
			log_complain(ppp->link_name, "more than maxdevs=%d"
				     " links/devices with %s",
				     maxdevs, ppp->dv.line);
			set_tr_msg(&ppp->lcp.fsm, "more than maxdevs=%d"
				   " links/devices with %.40s",
				   maxdevs, ppp->dv.line);
		}
		lcp_event(ppp,FSM_CLOSE);
		return 1;		/* act as if it worked */
	}

	if (kern_ip_mtu > ppp->lcp.neg_mtru) {
		rend_rej(ppp, "incompatible negotiated MTRU %d instead of %d",
			 ppp->lcp.neg_mtru, kern_ip_mtu);
		return 1;
	}

	if (ppp->lochost.sin_addr.s_addr != 0) {
		if (ppp->lochost.sin_addr.s_addr != lochost.sin_addr.s_addr) {
			rend_rej(ppp, "bad local IP address %s instead of %s",
				 ip2str(ppp->lochost.sin_addr.s_addr),
				 lochost_str);
			return 1;
		}
		bzero(&ppp->lochost, sizeof(ppp->lochost));
	}

	if (ppp->remhost.sin_addr.s_addr != 0) {
		if (ppp->remhost.sin_addr.s_addr != remhost.sin_addr.s_addr) {
			rend_rej(ppp,"bad remote IP address %s instead of %s",
				 ip2str(ppp->remhost.sin_addr.s_addr),
				 remhost_str);
			return 1;
		}
		bzero(&ppp->remhost, sizeof(ppp->remhost));
	}

	if (mp_known) {
		if (mp_on != ppp->lcp.neg_mp) {
			rend_rej(ppp, "incompatible MP parameters;"
				 " mp_on=%d neg_mp=%d",
				 mp_on, ppp->lcp.neg_mp);
			return 1;
		}
		if (mp_on) {
			if (ppp->lcp.neg_recv_ssn != mp_recv_ssn) {
				rend_rej(ppp, "incompatible MP parameters;"
					 " mp_recv_ssn=%d neg_recv_ssn=%d",
					 mp_recv_ssn, ppp->lcp.neg_recv_ssn);
				return 1;
			}
			if (ppp->lcp.neg_send_ssn != mp_send_ssn) {
				rend_rej(ppp, "incompatible MP parameters;"
					 " mp_send_ssn=%d neg_send_ssn=%d",
					 mp_send_ssn, ppp->lcp.neg_send_ssn);
				return 1;
			}
			if (ppp->ccp.fsm.state >= FSM_REQ_SENT_6) {
				rend_rej(ppp, "CCP started prematurely");
				return 1;
			}
			if (ppp->ipcp.fsm.state >= FSM_REQ_SENT_6) {
				rend_rej(ppp, "IPCP started prematurely");
				return 1;
			}
		}
	}

	if (strcmp(rem_epdis, ppp->rem_epdis)) {
		/* Allow the peer to change its discriminator
		 * only when starting the first link in a bundle.
		 */
		if (rem_epdis[0] != '\0') {
			char *msg = epdis_msg("remote",
					      ppp->rem_epdis, rem_epdis);
			if ((add_pid > 0 && add_pid != ckpid) || numdevs > 1) {
				rend_rej(ppp, "inconsistent %s", msg);
				if (ppp->rem_epdis[0] != '\0')
					rm_rend("ep-", ppp->rem_epdis);
				return 1;
			}
			log_debug(1,"","change to %s", msg);
			rm_rend("ep-", rem_epdis);
		}
		strcpy(rem_epdis, ppp->rem_epdis);
		if (rem_epdis[0] != '\0')
			(void)add_rend_name("ep-", rem_epdis);
	}

	if (strcmp(loc_epdis, ppp->loc_epdis)) {
		/* If we might have switched endpoint discriminators,
		 * then do it.
		 */
		if (loc_epdis[0] != '\0') {
			char *msg = epdis_msg("local",
					      ppp->loc_epdis, loc_epdis);
			if ((add_pid > 0 && add_pid != ckpid) || numdevs > 0) {
				rend_rej(ppp, "inconsistent %s", msg);
				return 1;
			}
			log_debug(1,"","change to %s", msg);
		}
		strcpy(loc_epdis, ppp->loc_epdis);
	}

	if (ppp->rem_sysname[0] != '\0') {
		if (rem_sysname[0] == '\0') {
			strcpy(rem_sysname,ppp->rem_sysname);
		} else if (strcmp(ppp->rem_sysname, rem_sysname)) {
			static complained = 0;
			if (rem_epdis[0] == '\0' && !complained) {
				complained = 1;
				log_complain(ppp->link_name,
					     "use REM_SYSNAME=%s or %s"
					     " to resolve multiple remote"
					     " names",
					     ppp->rem_sysname,
					     rem_sysname);
			}
		}
		if (!add_rend_name("",ppp->rem_sysname)) {
			rend_rej(ppp, "%s makes %d rendezvous names",
				 ppp->rem_sysname, num_rends);
			return 1;
		}
	}

	if (link_mux(ppp) < 0) {
		rend_rej(ppp, "failed to LINK to mux");
		return 1;
	}

	/* continue or finish authentication if it had not finished */
	if (ppp->phase == AUTH_PHASE)
		auth_done(ppp);

	if (ppp->phase == NET_PHASE) {
		ipcp_go(ppp);
		if (!ccp_go(ppp)) {	/* let data start moving */
			/* give up on failure */
			ipcp_event(ppp,FSM_CLOSE);
			set_tr_msg(&ppp->lcp.fsm, "system failure");
			lcp_event(ppp,FSM_CLOSE);
			return 1;
		}
	}

	/* No more discretionary lines for a while after adding one. */
	no_beep(1);

	return 1;
}


/* Add a line by calling the other machine.
 *
 * This always works on the first slot, on ppp0, because it is working on
 * the first line or because it has been fork()ed to start another line.
 */
static int				/* 0=failed */
addline(void)
{
	int i, j, k, l;
	int seenflag;
	fd_set in;
	struct timeval timer;
	char c;

	/* get rid of old PPP structures in case this is a forked child */
	while (ppp0.dv.next != 0) {
		struct ppp *oppp = (struct ppp*)ppp0.dv.next;
		ppp0.dv.next = oppp->dv.next;
		free(oppp);
	}
	init_ppp(&ppp0);
	mp_sn = -2;
	mp_len = 0;
	mp_buf.proto = 0;
	seen_framer = 0;

	ppp0.dv.callmode = CALLER;
	j = 0;
	for (;;) {
		if (j >= num_uucp_nams) {
			if (num_uucp_nams == 0) {
				log_complain("","no UUCP name known");
				return 0;
			}
			j = 0;
		}
		(void)strncpy(ppp0.dv.uucp_nam, uucp_names[j],
			      sizeof(ppp0.dv.uucp_nam));
		i = get_conn(&ppp0.dv,0);

		/* Do not add or delete discretionary lines for a while.
		 */
		no_beep(0);

		if (i == 2)		/* let caller rendezvous */
			return 0;
		if (i == 1)		/* success */
			break;

		/* dialing failed */
		if (++j >= num_uucp_nams) {
			log_complain("", "giving up for now");
			if (modfd >= 0) {
				/* flush attention getters */
				while (rcv_msg(modfd))
					continue;
			}
			return 0;
		}
	}
	numdevs_inc(&ppp0);

	if (!set_tty_modes(&ppp0.dv)) {
		rel_dev(&ppp0.dv);
		return 0;
	}

	/* Read the banner from the remote machine.  Look for any
	 * packets mixed up in it.
	 */
	if (ppp0.dv.sync == SYNC_OFF) {
		k = 0;
		seenflag = 0;
		for (;;) {
			if (!seenflag) {
				banlen = 0;
				banoff = 0;
				(void)get_clk();
				rcv_msg_ts = clk;
			}
			timer.tv_sec = 0;
			timer.tv_usec = 1000000/20;
			FD_ZERO(&in);
			FD_SET(ppp0.dv.devfd, &in);
			i = select(ppp0.dv.devfd+1, &in,0,0, &timer);
			if (i <= 0) {
				if (i == 0)
					break;
				if (errno == EINTR || errno == EAGAIN)
					continue;
				log_errno("banner select()","");
				break;
			}
			j = banoff+banlen;
			i = read(ppp0.dv.devfd, &banbuf[j], sizeof(banbuf)-j);
			if (i == 0) {
				log_complain("", "EOF while reading banner");
				break;
			}
			if (i < 0) {
				log_errno("read(banner)","");
				break;
			}
			if (Debug && k == 0) {
				kludge_stderr();
				(void)fputs("banner: ",stderr);
			}
			l = i+j;
			while (j < l) {
				c = banbuf[j];
				if (c == PPP_FLAG) {
					/* note possible start of frame */
					if (!seenflag) {
						seenflag = 1;
						banoff = j+1;
						banlen = -banoff;
					}
				}
				c = toascii(c);
				if (Debug) {
					if (iscntrl((c))) {
						c += '@';
						(void)fprintf(stderr,"^%c",c);
					} else {
						(void)fprintf(stderr,"%c",c);
					}
				}
				j++;
				k++;
			}
			if (seenflag) {
				banlen += i;
				if (banlen >= BANSIZE) {
					banlen = BANSIZE;
					break;
				}
			}
		}
		if (Debug && k != 0) {
			putc('\n',stderr);
			log_debug(1,"","saving %d bytes to salvage",
				  banlen);
		}
	}

	if (0 >= rdy_tty_dev(&ppp0.dv)
	    || 0 > reset_accm(&ppp0,1)) {
		rel_dev(&ppp0.dv);
		return 0;
	}

	return 1;
}



/* Start a child to add a line.
 *	This can only be used when the IP address of the peer is known.
 */
static int				/* 0=are the child, 1=the parent */
fork_addline(int force)
{
	pid_t pid;


	/* only one child at a time */
	if (add_pid >= 0)
		return 1;

	/* no children until there is a place to rendezvous.
	 */
	if (rendnode <= 0)
		return 1;

	/* If we are not sure if we have the right to add a link to the
	 * bundle, add the link only if we created it the oldest link in
	 * the bundle and so the bundle itself.
	 */
	if (!force && callmode == CALLEE)
		return 1;

	ck_rend("link to %s stolen");

	/* Children cannot negotiate our address, because they
	 * might agree to the wrong ones.
	 */
	if (lochost.sin_addr.s_addr == 0
	    || def_lochost
	    || remhost.sin_addr.s_addr == 0
	    || def_remhost)
		return 1;

	/* Children cannot negotiate MP parameters until the parent is
	 * sure of them.  So do not create a child to create a second
	 * line while the MP-state of a first line is unknown.
	 */
	if (!mp_known && numdevs != 0)
		return 1;

	(void)sighold(SIGCHLD);
	pid = fork();
	add_pid = pid;
	(void)sigrelse(SIGCHLD);

	switch (pid) {
	case 0:				/* this is the child */
		log_debug(1,"","add line #%d", numdevs+1);
		ppp0.nowait = 1;
		modfd = -1;
		status_poke_fd = -1;
		unmade_rend();
		connmode = RE_CALLER;
		del_route = 0;
		conf_del_route = 0;
		conf_proxy_arp = 0;
		numdevs = 0;
		clr_acct(&ppp0.dv);
		if (!addline())
			do_rend(&ppp0);	/* tell the parent of failure */
		return 0;

	case -1:
		log_errno("fork()","");
		break;
	}

	/* This is the parent. */

	/* As soon as we know we are going to use multilink,
	 * switch the names in the log.
	 */
	fix_names();

	/* Do not try too soon again if this attempt fails */
	add_time.tv_sec = clk.tv_sec + add_backoff;
	if (add_backoff < BACKOFF_MAX)
		add_backoff *= 2;

	return 1;
}


/* stop looking for another line
 */
static void
unfork(char *msg)
{
	pid_t pid;

	if ((pid = add_pid) > 0) {
		log_debug(1,"", "abort dialing by process %d: %s", pid, msg);
		(void)kill(pid,SIGTERM);
	}
}


/* go to sleep forever
 */
static void
die(struct ppp *ppp)
{
	/* It is worth closing everything in case there is no existing
	 * daemon or the daemon crashes, to minimize keeping the device
	 * turned on.
	 *
	 * Do not simply exit, because if getty is being used, init will start
	 * another getty which will mess up the STREAMS stack.
	 */
	rendnode = -1;
	modfd = -1;
	status_poke_fd = -1;
	ppp->dv.devfd = -1;
	ppp->dv.devfd_save = -1;
	closefds();
	ppp->utmp_type = NO_UTMP;

	/* sleep here forever */
	for (;;)
		(void)pause();
}


/* clear things after the last device is turned off
 */
static void
last_dev(void)
{
	numdevs = 0;

	oldest = 0;
	newest = 0;

	beep_tstamp = 0;
	prev_avail_bps = 0;
	prev_avail_bps_ok = 2;
	avail_bps = 0;
	avail_bps_ok = 2;

	mp_ppp = 0;
	if (ppp0.conf.mp != 0)
		mp_known = 0;
	mp_ncp_bits = 0;
}


/* (re)compute the available bandwidth in the bundle
 */
static void
get_hi_lo_bps(void)
{
	/* lo_bps = prev_avail_bps if known
	 *	  = avail_bps * (numdevs-1)/numdevs avail_bps known
	 *	  = avail_bps - newest->bps
	 */
	lo_bps = 0;
	if (prev_avail_bps_ok) {
		lo_bps = prev_avail_bps;
	} else if (avail_bps_ok && newest && newest->bps != 0) {
		lo_bps = avail_bps - newest->bps;
	}

	/* hi_bps <= 0.9*sum of sum of links, if known */
	hi_bps = (avail_bps_ok) ? (0.9*avail_bps) : MAXFLOAT;
}


/* housekeeping when adding a new device
 */
static void
numdevs_inc(struct ppp *ppp)
{
	numdevs++;
	newest_oldest();

	if (lactime != 0)
		ppp->dv.active = clk.tv_sec + lactime;

	beep_tstamp = 0;
	prev_avail_bps_ok = avail_bps_ok;
	prev_avail_bps = avail_bps;
	if (ppp->bps == 0) {
		avail_bps_ok = 0;
	} else {
		avail_bps += ppp->bps;
		if (avail_bps_ok == 2 && ppp->conf_bps == 0)
			avail_bps_ok = 1;
	}

	get_hi_lo_bps();
}


/* hang up a phone
 */
void
hangup(struct ppp *ppp)
{
	struct ppp *ppp1;

	if (ppp->dv.devfd >= 0
	    || ppp->dv.dev_index != -1) {
		numdevs--;

		/* No more discretionary lines for a while after losing one.
		 * Delay adding even lines required by mindevs
		 */
		no_beep(-1);

		beep_tstamp = 0;
		if (numdevs == 0) {
			last_dev();
		} else if (numdevs == 1) {
			prev_avail_bps = 0;
			prev_avail_bps_ok = 2;
		} else if (ppp->bps != 0) {
			prev_avail_bps -= ppp->bps;
		} else {
			prev_avail_bps_ok = 0;
		}

		/* recompute available bandwidth */
		avail_bps_ok = 2;
		avail_bps = 0;
		for (ppp1 = &ppp0;
		     ppp1 != 0;
		     ppp1 = (struct ppp*)ppp1->dv.next) {
			if (ppp1->dv.line[0] == '\0'
			    || ppp1 == ppp)
				continue;
			if (mp_on && mp_ppp == ppp) {
				ppp1->ccp = mp_ppp->ccp;
				ppp1->ipcp = mp_ppp->ipcp;
				mp_ppp = ppp1;
			}
			if (ppp1->bps == 0) {
				avail_bps = 0;
				avail_bps_ok = 0;
			} else if (avail_bps_ok != 0) {
				avail_bps += ppp1->bps;
				if (ppp1->conf_bps == 0)
					avail_bps_ok = 1;
			}
		}
		get_hi_lo_bps();
	}

	rel_dev(&ppp->dv);
	clear_ppp(ppp);
	newest_oldest();
}


/* The kernel says a line is dead.
 */
static void
deadline(struct ppp *ppp)
{
	log_debug(1,ppp->link_name,"%s off", ppp->dv.line);

	hangup(ppp);
}


/* see if helper has finished
 */
static void
reap_child(void)
{
	struct ppp *ppp;
	int st;
	pid_t pid;

	for (;;) {
		pid = waitpid(-1, &st, WNOHANG);
		if (pid <= 0) {
			if (pid < 0 && errno != ECHILD)
				log_errno("waitpid()","");
			break;
		}
		if (WIFSIGNALED(st)
		    && WTERMSIG(st) != SIGTERM
		    && WTERMSIG(st) != SIGINT) {
			log_complain("","pid %d killed by signal %d",
				     pid, WTERMSIG(st));
		}

		if (pid == stderrpid) {
			stderrpid = -1;
			stderrfd = -1;
			continue;
		}

		/* notice when the slave disappears */
		if (pid == add_pid) {
			add_pid = -1;
			continue;
		}

		for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
			if (ppp->dv.rendpid == pid) {
				ppp->dv.rendpid = -1;
				break;
			}
		}
	}

	(void)signal(SIGCHLD, reap_child);
}


/* Turn off one device
 */
static void
dev_off(struct ppp *ppp,
	char *msg)
{
	log_debug(1,ppp->link_name,"%s; %s", msg, ppp->dv.line);
	ppp->dv.active = TIME_NEVER;
	ppp->dv.acct.call_start = 0;
	set_tr_msg(&ppp->lcp.fsm, "%.40s", msg);
	lcp_event(ppp,FSM_CLOSE);
}


/* Turn off some devices
 */
static void
devs_off(int new,			/* 0=all, 1=ours, 2=newest of ours */
	 char *msg)
{
	struct ppp *ppp, *tgt;
	int age;

	age = 0;
	tgt = 0;
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.line[0] == '\0')
			continue;

		if (new == 0) {
			/* close all links i the bundle */
			dev_off(ppp, msg);

		} else if (ppp->dv.callmode != CALLEE) {
			/* Close the newest link we created.
			 */
			if (new == 1) {
				dev_off(ppp, msg);
			} else if (ppp->age >= age) {
				age = ppp->age;
				tgt = ppp;
			}
		}
	}
	if (tgt != 0)
		dev_off(tgt, msg);

	/* No more discretionary lines for a while. */
	no_beep(0);
}


/* set MP debugging
 */
static void
set_mpdebug(void)
{
	struct ppp *ppp;

	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->dv.devfd >= 0 && ppp->dv.dev_index != -1) {
			(void)set_mp(mp_ppp);
			return;
		}
	}
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

	kludge_stderr();

	/* tell the kernel to change debugging */
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

	if (mp_ppp != 0)
		set_mpdebug();

	log_complain("", "debug=%d", debug);

	/* reset call backoff timer */
	add_time.tv_sec = TIME_NOW;

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

	kludge_stderr();

	/* tell the kernel to change debugging */
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

	if (mp_ppp != 0)
		set_mpdebug();
	if (debug == 0)
		ck_acct(&ppp0.dv, 1);	/* dump connect info */

	log_complain("", "debug=%d", debug);

	/* reset call backoff timer */
	add_time.tv_sec = TIME_NOW;

	(void)signal(SIGUSR2, lessdebug);
}


/* Shut down after SIGINT or SIGTERM
 */
static void
stopint(int sig)
{
	struct ppp *ppp;
	int numact;

	if (sig == SIGPIPE)
		stderrfd = -1;

	log_debug(interact ? 0 : 1, "", "received signal %d", sig);

	/* do this only on the first signal */
	(void)signal(SIGINT, cleanup);
	(void)signal(SIGTERM,cleanup);
	(void)signal(SIGPIPE,cleanup);

	/* if this is a child, then pass call info to parent */
	if (add_pid == 0) {
		ppp0.nowait = 2;
		do_rend(&ppp0);
	}

	/* do not start any new connections */
	maxdevs = 0;
	outdevs = 0;
	mindevs = 0;
	idle_time.tv_sec = TIME_NEVER;
	busy_time.tv_sec = TIME_NEVER;
	beepok_time.tv_sec = TIME_NEVER;

	/* count the active PPP state machines */
	numact = 0;
	for (ppp = &ppp0; ppp != 0; ppp = (struct ppp*)ppp->dv.next) {
		if (ppp->lcp.fsm.state >= FSM_REQ_SENT_6)
			numact++;
	}

	unfork("operator shutdown");
	devs_off(0, "operator shutdown");

	if (numact == 0	)		/* if nothing active */
		cleanup(sig);		/* then quit now */

	demand_dial = 0;
	sactime = 0;			/* quit when things are over */
	camping = 0;
}


/*
 * Record login in wtmp file.
 */

#define	UTYPE		struct utmpx
#include <utmpx.h>

#define	SCPYN(a, b)	(void) strncpy(a, b, sizeof (a))

static void
clr_ut(struct ppp *ppp,
       UTYPE *ut,
       int type)
{
	char *p;

	bzero(ut,sizeof(*ut));
	ut->ut_type = type;
	ut->ut_pid = getpid();
	ut->ut_xtime = time(0);
	if (ppp->utmp_id[0] == '\0') {
		(void)sprintf(ut->ut_id, "p%02X%X",
			      ppp->dv.unit, ppp->link_num);
	} else {
		bcopy(ppp->utmp_id,ut->ut_id,sizeof(ut->ut_id));
	}
	p = strrchr(ppp->dv.lockfile, '.');
	strncpy(ut->ut_line, p ? (p+1) : ppp->dv.nodename,
		sizeof(ut->ut_line)-1);
	strncpy(ut->ut_name, ppp->utmp_name, sizeof(ut->ut_name)-1);
	if (remhost.sin_addr.s_addr != 0
	    && !def_remhost)
		strncpy(ut->ut_host,remhost_str, sizeof(ut->ut_host)-1);
}


void
dologin(struct ppp *ppp)
{
	UTYPE *ufound, utmp;

	/* Do not fiddle with the files more than necessary.
	 * If the utmp entry even has the hostname, then stop now.
	 * If it does not and if we know it, then update it.
	 * Add the record even if we do not know the host name
	 * to log efforts to log in.
	 */
	if (ppp->utmp_has_host
	    || (ppp->utmp_type != NO_UTMP
		&& (remhost.sin_addr.s_addr == 0
		    || def_remhost)))
		return;

	if (ppp->utmp_name[0] == '\0')
		strncpy(ppp->utmp_name,
			(ppp->auth.recvd_name[0] != '\0'
			 ? ppp->auth.recvd_name
			 : remote),
			sizeof(ppp->utmp_name));
	clr_ut(ppp,&utmp,USER_PROCESS);

	/* Look for an existing slot for us.
	 */
	setutxent();
	ufound = getutxline(&utmp);
	if (!ufound) {
		setutxent();
		ufound = getutxid(&utmp);
	}

	if (ufound != 0) {
		/* update the existing slot. */
		SCPYN(ppp->utmp_id, ufound->ut_id);
		SCPYN(utmp.ut_id, ufound->ut_id);

		if (ufound->ut_type == USER_PROCESS) {
			if (ufound->ut_name[0] != '\0') {
				SCPYN(ppp->utmp_name, ufound->ut_name);
				SCPYN(utmp.ut_name, ufound->ut_name);
			}
			if (ufound->ut_host[0] != '\0') {
				ppp->utmp_has_host = 1;
				(void)strcpy(utmp.ut_host, ufound->ut_host);
			}
		}

		if (no_utmp && ppp->utmp_type == NO_UTMP) {
			endutxent();
			return;
		}

		if (ppp->utmp_type == NO_UTMP
		    || ufound->ut_pid != utmp.ut_pid
		    || (!ppp->utmp_has_host && utmp.ut_host[0] != '\0')) {
			if (ppp->utmp_type == NO_UTMP)
				ppp->utmp_type = PPP_UTMP;
			if (utmp.ut_host[0] != '\0')
				ppp->utmp_has_host = 1;
			pututxline(&utmp);
			log_debug(6,"","updated utmp: ut_host=\"%s\" type=%d",
				  utmp.ut_host,ppp->utmp_type);
		} else {
			log_debug(6,"","do not update utmp:"
				  " remhost=%s def=%d ut_host=\"%s\"",
				  ip2str(remhost.sin_addr.s_addr),
				  def_remhost,
				  ufound->ut_host);
		}

	} else if (no_utmp) {
		ppp->utmp_type = NO_UTMP;
		endutxent();
		return;

	} else {
		/* create a new entry */
		ppp->utmp_type = PPP_UTMP;
		setutxent();
		(void)pututxline(&utmp);
	}

	/*
	 * updwtmpx attempts to update both wtmp and wtmpx,
	 * which must be kept in sync for 'last' to produce
	 * any helpful remote-host info.
	 */
	updwtmpx(WTMPX_FILE, &utmp);
	endutxent();
}


/* Record logout in wtmp file
 */
void
dologout(struct dev *dp)
{
	struct ppp *ppp = (struct ppp*)dp;
	UTYPE *ufound, utmp;

	/* Do it only if we are in charge of the utmp entry.
	 */
	if (ppp->utmp_type != PPP_UTMP)
		return;

	clr_ut(ppp,&utmp,DEAD_PROCESS);
	setutxent();
	ufound = getutxid(&utmp);
	if (!ufound)
		return;

	utmp = *ufound;
	utmp.ut_type = DEAD_PROCESS;
	utmp.ut_user[0] = '\0';
	utmp.ut_xtime = time(0);
	(void)pututxline(&utmp);
	endutxent();
	updwtmpx(WTMPX_FILE, &utmp);

	ppp->utmp_type = NO_UTMP;
}
