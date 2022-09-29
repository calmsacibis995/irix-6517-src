/*
 * @(#) $Id: rsvp_main.c,v 1.16 1998/12/09 19:22:33 michaelk Exp $
 */

/************************ rsvp_main.c  *******************************
 *                                                                   *
 *    RSVP daemon: main routine, initialization, and I/O control     *
 *		routines.  Generally, system-dependent code.         *
 *                                                                   *
 *********************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#define __MAIN__

#include "rsvp_daemon.h"
#include "rsvp_api.h"
#include <sys/stat.h>
#ifdef __sgi
#include <cap_net.h>
#endif
#include <fcntl.h>

#ifdef SOLARIS
#include <sys/sockio.h>         /* for SIOCGIFCONF (for SOLARIS) */
#include <sys/termios.h>        /* for TIOCNOTTY (for SOLARIS) */
#endif

#if defined(freebsd)
#include <sys/uio.h>
#include <net/if_dl.h>
#endif

/***  ??? why
#ifndef HOST_ONLY
#include <string.h>
#endif
***/

/* Data structure for main select() loop
 */
static fd_set	fds;			/* bit vector for select */
static int	fd_to_if[FD_SETSIZE];   /* fd -> phyint mapping array */
static int	fd_to_udpif[FD_SETSIZE];/* fd -> UDP int mapping array */
static int	fd_to_vif[FD_SETSIZE];  /* fd -> vif mapping array */

/* Sockets and addresses
 */
static int		api_socket;		/* Unix listen sock for API */
static int		kern_socket = -1; 	/* TC interface socket */
int			probe_socket;		/* Sock to rcv status probe */
#ifdef _MIB
int			mib_enabled = 0;	/* enable MIB if license found */
extern fd_set		peer_mgmt_mask;		/* for SNMP messages */
int			mib_entry_expire = 300; /* keep NOTINSERVICE entries for
						 * this many seconds */
#endif
static struct in_addr	encap_group;			/* Gu */
u_int16_t		encap_port;			/* Pu  */
u_int16_t		encap_portp;			/* Pu' */
u_char			encap_ttl = RSVP_TTL_ENCAP;	/* IP TTL */
struct in_addr		encap_router;
struct sockaddr_in	encap_sin;
u_char			sock_send_ttl[FD_SETSIZE];	/* TTL per socket */
u_char			sock_send_RA[FD_SETSIZE];	/* Send-RA flag */
int			mstat_ttl = 1;	/* IP TTL for multicasting status:
					 *  Note small default!
					 */
char            recv_buff[MAX_PKT + sizeof(struct ip)];
#if defined (freebsd) 
char            recv_ctrl_buff[MAX_SOCK_CTRL];
#endif

/*
 *	Test facility variables
 */
#define MAX_DEBUG_FILTER	10
int debug_filters[MAX_DEBUG_FILTER];
int debug_filter_num = 0;
int debug_filter = 0;
int Test_mode = 0;
u_long *tsttun_vec;
int	*tst_sock2if;

/* 
 * Global RSRR variables.
 */
struct rsrr_vif vif_list[RSRR_MAX_VIFS]; /* vifs received from routing */
#ifndef HOST_ONLY
char rsrr_recv_buf[RSRR_MAX_LEN];	/* RSRR receive buffer */
char rsrr_send_buf[RSRR_MAX_LEN];	/* RSRR send buffer */
struct sockaddr_un serv_addr;		/* Server address */
struct sockaddr_un cli_addr;		/* Client address */
int clilen, servlen;			/* Lengths */
#endif /* ! HOST_ONLY */

/* RSRR definitions. */
int             rsrr_socket;		/* interface to reservation protocol */
void	        rsrr_init();
#ifndef HOST_ONLY
int	        rsrr_read();
int		rsrr_send_iq();
int		rsrr_send();
int	        rsrr_get_ir();
extern int      rsrr_accept();
extern void     rsrr_qt_init();
#endif /* ! HOST_ONLY */

/* External declarations */
int		process_api_req(int, rsvp_req *, int);
int		api_status(rsvp_req *);
int		rsvp_pkt_process(struct packet *, struct sockaddr_in *, int);
extern char     vers_comp_date[];	/* version & compilation date/time */
void		hexf();
void		del_from_timer();
u_long		next_event_time();
void		init_route();
int		unicast_init();
void		init_probe_socket();
void		send_rsvp_state(char *, int, struct sockaddr_in *);
void		api_free_packet(struct packet *);
int		next_word(char **, char *), pfxcmp(char *, char *);
#ifdef RTAP
void		rtap_init(), rtap_loop(fd_set *), rtap_command();
void		rtap_dispatch();
extern int	rtap_insock, rtap_fd;		/* these are already defined - mwang */
#endif
char 		*fmt_hostname(struct in_addr *);
#define iptoname(p)	fmt_hostname(p)

/* forward declarations */
int		do_sendmsg(int, struct sockaddr_in *, struct in_addr *, int,
						u_char, u_char *, int);
int		do_sendto(int, struct sockaddr_in *, struct in_addr *, int,
						u_char, u_char *, int);
void		rsvp_api_close(int);
static int	init_rsvp(void);
#ifndef __sgi
int 		save_pid(void);
#endif
static void	sigpipe(int);
int		udp_input(int, int);
int		api_input(int);
int		raw_input(int, int);
int		status_probe(int ifd);
int		start_UDP_encap(int);
int		api_cmd(int, rapi_cmd_t *);
static void	sigint_handler(int);
static void	turn_off_rsvp_intercept();
static void	turn_off_rsvp_vif_intercept();
int		get_rsvp_socket(void);
int		send_socket(int, int);
int		recv_socket(int, int);
int		on_exit();
void		bind_socket(int, u_long, int);
#if defined (freebsd)
void	        enable_IP_RECVIF(int);
#endif
int		IsLocal(int, struct in_addr*);
u_long		Gateway(u_long);
int		find_sid(int fd, int pid, int client_sid);  
int		find_free_sid(int fd, int pid, int client_sid);
int		map_if(struct in_addr *);
u_int32_t	hostname_cnv(char *);
void		print_ifs();
void		read_config_file(char *);
void		cfig_action(int, char *, char *);
void		cfig_do(char **, char *);
KEY_ASSOC	*load_key(char *, char *);

void
Usage(char *namep)
	{
	fprintf(stderr,
"Usage: %s [-D] [-d debug_bits] [-l debug_level] [-t mstat_ttl] \n",
	 namep);
}

/*
 *	Define table giving syntax of configuration file
 */
typedef struct  {
	char	*cfig_keywd;
	char	cfig_flags;
#define CFF_CMDOP	0x01
#define CFF_HASPARM	0x02
#define CFF_HASPARM2	0x04
	char	cfig_actno;
}  Cfig_element;

enum {	CfAct_iface, CfAct_police,  CfAct_udp,    CfAct_udpttl,
	CfAct_intgr, CfAct_disable, CfAct_sndttl, CfAct_refresh,
	CfAct_passnormal, CfAct_passbreak,
	CfAct_sendkey,CfAct_recvkey,CfAct_neighbor
};

/*	interface <name> [police] [udp] [udpttl <#>] [integrity]
 *			[disable] [sendttl <#>] [refresh <#>]
 */
Cfig_element  Cfig_Table[] = {
	{"interface",	CFF_CMDOP+CFF_HASPARM,	CfAct_iface},
	{"police",	0,			CfAct_police},
	{"udpencap",	0,			CfAct_udp},
	{"udpttl",	CFF_HASPARM,		CfAct_udpttl},
	{"integrity",	0,			CfAct_intgr},
	{"disable",	0,			CfAct_disable},
	{"passnormal",	0,			CfAct_passnormal},
	{"passbreak",	0,			CfAct_passbreak},
	{"refresh",	CFF_HASPARM,		CfAct_refresh},
	{"sendkey",	CFF_HASPARM2,		CfAct_sendkey},
	{"sendttl",	CFF_HASPARM,		CfAct_sndttl},
	{"neighbor",	CFF_CMDOP+CFF_HASPARM,	CfAct_neighbor},
	{"recvkey",	CFF_HASPARM2,		CfAct_recvkey},
	{NULL,		0,			0} /* Fence: ends table */
};

int	Cfig_if=-1;		/* Global interface number for config file */
struct in_addr Cfig_neighbor;	/* Global neighbor address for config file */
Cfig_element *cfig_search(char *);
char	*Cfig_filename="/etc/config/rsvpd.conf"; /* default config file location */


/*	Protocol number for socket calls to create sockets to send RAW
 *	RSVP datagrams.  System dependent.
 */
#define IPPROTO_forRAW	IPPROTO_RSVP

/*
 *
 *	The main() routine for rsvpd, the RSVP daemon.
 *
 */
int
main(int argc, char *argv[]) {
	int             fd_wid, rc, fd_val, newsock, zero = 0;
	fd_set          tmp_fds;
	extern char    *optarg;
	u_int           c;
	int		inp_if;
	struct timeval	t_intvl;
	struct timeval	t_old, t_new;
	int		delta;
	int		Daemonize = 1;
	int		Rflag = 0;
	char		*R_arg;
#ifdef _MIB
	char		*license_msgp;
#endif

	/*
	 *      Set defaults
	 */
	debug = DEFAULT_DEBUG_MASK;
	l_debug = DEFAULT_LOGGING_LEVEL;
	encap_router.s_addr = INADDR_ANY;
	Max_rsvp_msg = MAX_PKT;

	/*
	 *	Process options
	 */
	while ((c = getopt(argc, argv, "c:Dd:e:l:t:R:")) != -1) {
		switch (c) {

		case 'c':
		        Cfig_filename = optarg;
			break;

		case 'D':	/* -D => debug mode, i.e. do not daemonize */
			Daemonize = 0;
			break;
			
		case 'd':
			debug = atoi(optarg);
			break;
#ifdef _MIB
		case 'e':
			mib_entry_expire = atoi(optarg);
			mib_entry_expire *= 60;
			break;
#endif

		case 'l':
			l_debug = atoi(optarg);
			break;

		case 't': /* -t: specify TTL for multicasting status info */
			mstat_ttl = atoi(optarg);
			break;

		case 'R': /* -R: specify name/address of RSVP-capable
			   *     router to receive encapsulated Path messages.
			   */
			Rflag = 1;
			R_arg = optarg;
			break;

#ifdef DEBUG
		case 's': /* max message size (for debugging) */
			Max_rsvp_msg = atoi(optarg);
			break;			

		case 'x': /* -x: test mode. */
			Test_mode = 1;
			break;
#endif

		default:
			Usage(argv[0]);
			exit(1);
		}
	}
#ifndef __sgi	/* test_alive() can't tell if it is sending a kill to itself
		   or some other pid that is not rsvpd.  Just skip it for now.
		   A fix will come from ISI in 4.2a2. mwang */
	/*
	 *  If there is already an RSVP daemon running (as shown by existence
	 *  of pid file), exit immediately.
	 */
	if ((rc = test_alive())) {
		fprintf(stderr,
			"RSVP daemon is already running, with PID=%d\n", rc);
		exit(-1);
	}
#endif
	/*
	 * 	Daemonize, if requested.
	 */
	if (Daemonize) {
#if BSD >= 199306
		daemon(0, 0);
#else
#ifdef __sgi
		_daemonize(0, -1, -1, -1);
#else
		int i;
		if (fork())
			exit(0);
		for (i = 0; i < getdtablesize(); i++)
			(void) close(i);
		(void) open("/", O_RDONLY);
			(void) dup2(0, 1);
		(void) dup2(0, 2);
		i = open("/dev/tty", O_RDWR);
		if (i >= 0) {
			(void) ioctl(i, (int) TIOCNOTTY, (char *)0);
			(void) close(i);
		}
#endif /* __sgi */
#endif /* BSD */
	}
	if (Rflag) {
		encap_router.s_addr = hostname_cnv(R_arg);
		if (encap_router.s_addr == INADDR_ANY)
			log(LOG_ERR, 0, "Unknown router %s", R_arg);
	}

	/*
	 *	Initialize RSVP
	 */
	signal(SIGPIPE, sigpipe);
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	reset_log(1);	/* Initialize log and print header */
#ifndef __sgi
	/*
	 * test_alive() looks for a file with a pid number.  Since rsvpd may
	 * be started from a startup script, the pid may be the same every time.
	 */
	if (save_pid() == -1) {
		log(LOG_ERR, 0, "save_pid() failed!\n");
		exit(-1);
	}
#endif

	if (init_rsvp() == -1) {
		log(LOG_ERR, 0, "RSVP initialization failed.\n", 0);
		exit(-1);
	}

	/*
	 * Print interface status.  Config file was read in init_rsvp
	 */
	print_ifs();

#ifdef _MIB
	/*
	 * The name of the license we are looking for is actually set in license.c
	 */
#ifdef _LICENSED
	if (!(getLicense("rsvpd-snmpagent", &license_msgp))) {
		log(LOG_ERR, 0, "%s", license_msgp);
		mib_enabled = 0;
	} else
#endif
             {
		if ((mib_init(0)) < 0) {
		     log(LOG_ERR, errno, "Could not initialize rsvpd-snmpagent.\n", 0);
		     mib_enabled = 0;
		} else {
		     log(LOG_INFO, 0, "rsvpd-snmpagent activated.\n");
		     mib_enabled = 1;
		}
	     }
#endif  /* _MIB */

	FD_SET(api_socket, &fds);
	FD_SET(probe_socket, &fds);
	if (UDP_Only) {
		FD_SET(rsvp_pUsocket, &fds);
		log(LOG_INFO, 0, "Using UDP encapsulation\n");
	}
	else
		FD_SET(rsvp_socket, &fds);
	FD_SET(rsvp_Usocket, &fds);
#ifndef HOST_ONLY
	if (!NoMroute) {
		FD_SET(rsrr_socket, &fds);
	}
#endif
#ifdef RTAP
	rtap_fd = rtap_insock = -1;
	if (!Daemonize)
		rtap_init();
#endif


	/* XXX Configure max # simultaneous API sessions (=> fd_wid) ??
	 */

	/*
	 *	Main control loop
	 */
#ifdef SOLARIS
	fd_wid = sysconf(_SC_OPEN_MAX);
#else
	fd_wid = getdtablesize();
#endif

#ifdef _MIB
	/*
	 * Transfer all the bits from the PEER management fd_set to the
	 * master fd_set.
	 */
	if (mib_enabled) {
	     for (fd_val=0; fd_val < fd_wid; fd_val++) {
		  if (FD_ISSET(fd_val, &peer_mgmt_mask)) {
		       FD_SET(fd_val, &fds);
		  }
	     }
	}
#endif

	for (;;) {
		memcpy((char *) &tmp_fds, (char *) &fds, sizeof(fds));
#ifdef RTAP
		if (!Daemonize)
			rtap_loop(&tmp_fds);
#endif
		gettimeofday(&t_old, NULL);
		delta = next_event_time();

		if (delta == -1)
			rc = select(fd_wid, &tmp_fds, (fd_set *) NULL, 
				(fd_set *) NULL, NULL);
		else {
			delta -= time_now;
			t_intvl.tv_sec = delta/1000;
			t_intvl.tv_usec = 1000*(delta%1000);
			rc = select(fd_wid, &tmp_fds, (fd_set *) NULL, 
				(fd_set *) NULL, &t_intvl);
		}
		if (rc == -1) {
			log(LOG_ERR, errno, "Bad return from select()\n");
			assert(errno == EINTR);
			continue;
		}

		gettimeofday(&t_new, NULL);
		delta = (t_new.tv_sec - t_old.tv_sec) * 1000 + 
			(t_new.tv_usec - t_old.tv_usec)/1000;
		time_now += delta;
		timer_wakeup(0);
		assert(rc >= 0);
		for (fd_val = 0; rc--; fd_val++) {
			/* Locate the next fd bit that is set, then dispatch
			 *  on it.
			 */
			while (!FD_ISSET(fd_val, &tmp_fds))
				fd_val++;
#ifndef HOST_ONLY
			if (fd_val == rsrr_socket) {
				rsrr_read(RSRR_ALL_TYPES);
			}
			else
#endif

			     if (fd_val == api_socket) {
				/*
				 * API connection request from client; get a
				 * new socket for this connection and add it
				 * to the select list.
				 */
				newsock = accept(api_socket, (struct sockaddr *) NULL, &zero);
				if (newsock == -1) {
					log(LOG_ERR, errno, "accept error\n");
					continue;
				}
				FD_SET(newsock, &fds);
			}
			else if (fd_val == probe_socket) {
				/* Status probe packet arrived.
				 */
				status_probe(fd_val);
			}
			else if (fd_val == rsvp_socket) {
			/*
			 * rsvp_socket:
 			 *  o Receive intercepted (RSVP_ON) raw multicast
			 *	and unicast Path messages; superceded by 
			 *	rvsock[].
			 *  o Receive intercepted (RSVP_ON) raw unicast
			 *	Resv messages; superceded by rvsock[].
			 */
				raw_input(fd_val, -1);
			}
			else if (fd_val == rsvp_Usocket) {
			/*
			 * rsvp_Usocket:
			 *  o Receive UDP-encap multicast Path messages, 
			 *	to G*, Pu.
			 */
				udp_input(fd_val, -1);
			}
			else if (fd_val == rsvp_pUsocket) {
			/*
			 * rsvp_pUsocket:
			 *  o UDP-only: Receive UDP-encap multicast Path 
			 *	messages to (D), Pu' through any interface.
			 */
				udp_input(fd_val, -1);
			}
#ifndef HOST_ONLY
			else if ((inp_if = fd_to_vif[fd_val]) >= 0) {
			/*
			 * rvsock[vif]: (Not UDP-only and mrouted is running)
			 *   o Receive intercepted (VIF_ON) raw multicast and 
			 *	unicast Path message on specific vif.
			 *   o Receive intercepted (VIF_ON) raw unicast Resv 
			 *	messages on specific vif.
			 */
				raw_input(fd_val, inp_if);
			}
#endif /* HOST_ONLY */
			else if ((inp_if = fd_to_udpif[fd_val]) >= 0) {
			/*
			 * Uisock[if]:
			 *   o Receive UDP-encap unicast Resv messages on
			 *	specific phyint.
			 *   o Not UDP-only: Receive UDP-encap unicast Path
			 *	message on specific phyint, to port Pu.
			 *   o UDP-only: Receive UDP-encap unicast Path message 
			 *	on specific phyint, to port Pu'.
			 */
				udp_input(fd_val, inp_if);
			}
#ifdef RTAP
			else if (fd_val == rtap_insock)
				rtap_command();
			else if (fd_val == rtap_fd)
				rtap_dispatch();				
#endif /* RTAP */
#ifdef _MIB
			else if (mib_enabled && (FD_ISSET(fd_val, &peer_mgmt_mask)))
				mib_process_req();
#endif
			else
				/*
				 *   RSVP API input
				 */
				api_input(fd_val);
		}
	}
}

static void
sigpipe(int sig) {
	log(LOG_DEBUG, 0, "Got a SIGPIPE\n");
	signal(SIGPIPE, sigpipe);
}


static void
sigint_handler(int sig)
{
	log(LOG_ERR, 0, "Exiting on signal %d\n", sig);
	if (!UDP_Only) turn_off_rsvp_intercept();
#ifndef HOST_ONLY
	/* Clean up the RSRR socket. */
	if (!NoMcast) {
		unlink(cli_addr.sun_path);
	}
#endif /* ! HOST_ONLY */
	_exit(sig);
}


/*
 * init_rsvp():  Initialize rsvpd.  Read the vif and phyint info from the 
 *	kernel, initialize the sockets, and start the timer.
 */
static int
init_rsvp(void)
{
	int	i,vif;
	extern RSVP_HOP	Prev_Hop_Obj, Next_Hop_Obj;
	extern FILTER_SPEC Wildcard_FilterSpec_Obj;
	extern STYLE	Style_Obj;

	/* Zero out and initialize headers of some global objects
	 */
	Init_Object(&Prev_Hop_Obj, RSVP_HOP, ctype_RSVP_HOP_ipv4);
	Init_Object(&Next_Hop_Obj, RSVP_HOP, ctype_RSVP_HOP_ipv4);
	Init_Object(&Style_Obj, STYLE, ctype_STYLE);
	Init_Object(&Wildcard_FilterSpec_Obj, FILTER_SPEC,
						ctype_FILTER_SPEC_ipv4);
	FD_ZERO(&fds);
	for (i= 0; i < FD_SETSIZE; i++) {
		fd_to_if[i] = fd_to_udpif[i] = fd_to_vif[i] = -1;
	}
	
	/*	Misc initializations
	 */
 	memset(session_hash, 0, SESS_HASH_SIZE*sizeof(Session *));
	memset(sock_send_ttl, 0, FD_SETSIZE);
	memset(sock_send_RA, 0, FD_SETSIZE);
	memset(key_assoc_table, 0, KEY_TABLE_SIZE * sizeof(KEY_ASSOC));
	Key_Assoc_Max = 0;

	/*
	 *	Initialize encapsulation group G* and ports Pu, Pu', and
	 *	set up encapsulation socket addr structure.
	 */
	if ((encap_group.s_addr= inet_addr(RSVP_ENCAP_GROUP)) == -1){
		log(LOG_ERR, errno, "Encap grp addr", 0);
		return(-1);
	}
	encap_port = hton16((u_int16_t) RSVP_ENCAP_PORT);
	Set_Sockaddr_in(&encap_sin, encap_group.s_addr, encap_port);
	encap_portp = hton16((u_int16_t) RSVP_ENCAP_PORTP);

	/*
	 * 	Build list of physical interfaces, set if_num to #.
	 *	Use first interface address as my local address.
	 */
	if_num = if_list_init();
	local_addr.s_addr = IF_toip(0);

#ifdef __sgi
	/*
	 * We need to read the config file after if_list_init but before
	 * init_sockets.
	 * This name should not be hard coded like this.
	 */
	read_config_file(Cfig_filename);
#endif

	/*	Try to set up raw socket rsvp_socket.  If we are not running
	 *  	as root or if raw socket call fails, set NoRawIO true. If fail 
	 *	to set multicast TTL (in get_rsvp_socket()), set NoMcast true,
	 * 	else initialize rsrr_socket.
	 */
	NoRawIO = NoMcast = 0;
	rsvp_socket = rsvp_pUsocket = -1;
	if (geteuid() || (rsvp_socket = get_rsvp_socket()) < 0) {
		NoRawIO = 1;
		UDP_Only = 1;
	}
	if (!NoMcast)
		rsrr_init();

	/*  	Get table of vifs from multicast routing.  But if there is
	 *	no multicast forwarding or if get_vifs returns 0, then
	 * 	turn on NoMroute and fake vifs to be identical to phyints.
	 */
	if (NoMcast || (get_vifs() < 0)) {
		NoMroute = 1;
		log(LOG_ALWAYS, 0, "No mrouted running\n");
		vif_num = if_num;
		/* Setup permanent vif structure. */
		for (vif=0; vif< RSRR_MAX_VIFS; vif++)
			BIT_SET(vif_list[vif].status, RSRR_DISABLED_BIT);
		for (i = 0; i < vif_num; i++) {
			vif_toif[i] = i;
			vif_list[i].id = i;
			/* Hack: not sure if we need threshold in this case. */
			vif_list[i].threshold = 255;
			vif_list[i].status = 0;
			vif_list[i].local_addr.s_addr = IF_toip(i);
			if (IsDebug(DEBUG_RSRR))
			    log(LOG_DEBUG, 0, "Configured vif %d with %s\n",
				vif_list[i].id,
				inet_ntoa(vif_list[i].local_addr));
		}
		/* Note that [vif_num] entry is for API */
		vif_toif[vif_num] = i;
	}

#ifndef HOST_ONLY
	/*	If there is multicast routing, will set up per-VIF receive 
	 *	socket (in init_sockets) later.  If not, enable single
	 *	intercept to rsvp_socket now and set to turn it off upon
	 *	exit.  But if enabling intercept fails, must do UDP 
	 *	encapsulation to receive RSVP messages => UDP_Only true.
	 */
	if (NoMroute) {
#if defined (freebsd) && defined(IPRECVIF)
	        enable_IP_RECVIF(rsvp_socket);
#endif
		if (setsockopt(rsvp_socket, IPPROTO_IP, IP_RSVP_ON,
			       (char *)NULL, 0) < 0) {
			log(LOG_INFO, 0, "Pre-3.3 multicast kernel\n");
			UDP_Only = 1;
		} else
#ifdef STANDARD_C_LIBRARY
			atexit(turn_off_rsvp_intercept);
#else
			on_exit(turn_off_rsvp_intercept, 0);
#endif
	}
#endif /* ! HOST_ONLY  */

	/*	Call routine to initialize the rest of the network I/O
	 *	sockets: rsvp_Usocket, rsvp_Rsocket, vsock[], and isock[].
	 */
	init_sockets(vif_num, if_num);

	/*	Initialize:
	 *	--  Unicast routing
	 *	--  probe_socket: socket for receiving status probes
	 *	--  api_socket: API listen socket
	 *	--  Timer routines
	 *	--  kernel traffic control
	 */
	if (unicast_init())
		NoUnicast = 0;
	else
		NoUnicast = 1;
	init_probe_socket();
	init_api();
	init_timer(MAX_TIMER_Q);
#ifdef SCHEDULE
	TC_init(kern_socket);
#endif /* SCHEDULE */
	return (0);
}

/*
 *  get_rsvp_socket():
 *	Create and initialize raw RSVP socket for receiving multicast
 *	pkts on protocol 46.  Returns -1 if cannot do raw I/O.  If
 *	cannot do multicast initialization, set NoMcast=1 as side effect.
 */
int
get_rsvp_socket()
	{
	int	newsock;
	u_char	loop = 0;		/* No mcast loopback */


#ifdef __sgi
	newsock = cap_socket(AF_INET, SOCK_RAW, IPPROTO_RSVP);
#else
	newsock = socket(AF_INET, SOCK_RAW, IPPROTO_RSVP);
#endif
	if (newsock<0){
		if (errno == EPROTONOSUPPORT)
			return(-1);
		else {
			log(LOG_ERR, errno, "raw socket", 0);
			exit(-1);
		}
	}
	if (cap_setsockopt(newsock, IPPROTO_IP, IP_MULTICAST_LOOP,
					(char *) &loop, sizeof(loop)) < 0) {
		NoMcast = 1;
	}
	return(newsock);
}


static void
turn_off_rsvp_intercept()
{
	if(rsvp_socket >= 0) {
		if (NoMroute) {
#if !defined(__sgi) && !defined(SOLARIS)
			setsockopt(rsvp_socket, IPPROTO_IP,
				   IP_RSVP_OFF, (char *)0, 0);
#endif
		}
		close(rsvp_socket);
		rsvp_socket = -1;
	}
	turn_off_rsvp_vif_intercept();
}

static void
turn_off_rsvp_vif_intercept()
	{
	int	vif;
	
	if (!NoMroute)
		return;
	for (vif = 0; vif < vif_num; vif++) {
		if (BIT_TST(vif_list[vif].status, RSRR_DISABLED_BIT))
			continue;
		if (fd_to_vif[rvsock[vif]] < 0)
			continue;		

		setsockopt(rvsock[vif], IPPROTO_IP, IP_RSVP_VIF_OFF,
					(char *) &vif, sizeof(int));
		FD_CLR(rvsock[vif], &fds);
	}
	memset(fd_to_vif, -1, FD_SETSIZE);
}


/*
 * init_sockets(): initialize sockets for real and virtual interfaces.
 *
 */
void
init_sockets(int vif_num, int if_num) {
	int	i, vif, handler;
	struct ip_mreq	mreq;
	char errmsg[IFNAMSIZ+60];

	vsock = (int *)calloc(vif_num, sizeof(int));
	rvsock = (int *)calloc(vif_num, sizeof(int));
	Uvsock = (int *)calloc(vif_num, sizeof(int));
	isock = (int *)calloc(if_num, sizeof(int));
	Uisock = (int *)calloc(if_num, sizeof(int));
	assert(vsock && rvsock && Uvsock && isock && Uisock);
	handler = 0;

	/*
	 *	Create rsvp_Usocket, a UDP socket for sending and 
	 *	receiving UDP-encapsulated multicast packets.
	 *	Also create rsvp_pUsocket, a UDP socket for a
	 *	UDP-only host to receive multicast Path messages.
	 */
	rsvp_Usocket = send_socket(SOCK_DGRAM, 0);
 	bind_socket(rsvp_Usocket, INADDR_ANY, encap_port);
#if defined (freebsd) && defined(IPRECVIF)
	enable_IP_RECVIF(rsvp_Usocket);
#endif
	if (!NoMcast) {
		/*
		 *	On each phyint, join encapsulation group
		 */
		for (i= 0; i < if_num; i++) {
#ifdef __sgi
		        if (if_vec[i].if_flags & IF_FLAG_Disable)
			       continue;
#endif
			mreq.imr_multiaddr = encap_group;
			mreq.imr_interface.s_addr = IF_toip(i);
			if (0 > cap_setsockopt(rsvp_Usocket, IPPROTO_IP, 
			    IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq))) {
			        sprintf(errmsg, "Cannot receive UDP-enacapsulated multicast on %s",
					if_vec[i].if_name);
				log(LOG_ERR, errno, errmsg, 0);
			}
		}
	}

	if (UDP_Only) {
		rsvp_pUsocket = recv_socket(SOCK_DGRAM, 0);
		bind_socket(rsvp_pUsocket, INADDR_ANY, encap_portp);
#if defined (freebsd) && defined(IPRECVIF)
		enable_IP_RECVIF(rsvp_pUsocket);
#endif
		for (i= 0; i < if_num; i++) {
			if (Test_mode && i > 0)
				break;
			mreq.imr_multiaddr = encap_group;
			mreq.imr_interface.s_addr = IF_toip(i);
			if (0 > cap_setsockopt(rsvp_pUsocket, IPPROTO_IP, 
			    IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq))) {
				log(LOG_ERR, errno, "pUsock join1", 0);
				exit(-1);
			}
		}
	}

	/*	Create Uisock[], vector of phyint-specific UDP S/R sockets,
	 *	and isock[], vector of phyint-specific raw send sockets.
	 */
	for (i = 0; i < if_num; i++) {
		struct in_addr ifaddr;
#ifdef __sgi
		if (if_vec[i].if_flags & IF_FLAG_Disable)
		       continue;
#endif
		Uisock[i] = send_socket(SOCK_DGRAM, 0);
		bind_socket(Uisock[i], IF_toip(i), 
			(UDP_Only)? encap_portp: encap_port );
		ifaddr.s_addr = IF_toip(i);
		cap_setsockopt(Uisock[i], IPPROTO_IP, IP_MULTICAST_IF,
			&ifaddr, sizeof(struct in_addr));
		FD_SET(Uisock[i], &fds);
		fd_to_udpif[Uisock[i]] = i;
		if (Test_mode)
			tst_sock2if[Uisock[i]] = i;

		if (NoRawIO)
			start_UDP_encap(i);
		else {
			isock[i] = send_socket(SOCK_RAW, IPPROTO_forRAW);
			bind_socket(isock[i], IF_toip(i), 0);
		}
	}

	if (UDP_Only)
		return;

	/*	Now set up raw sockets: vsock[], rvsock[].
	 */
	for (vif = 0; vif < vif_num; vif++) {
#ifdef __sgi
	        /* 
		 * Ugh, how do I tell if a vif is mapped onto a disabled
		 * physical interface?
		 */
	        if ((vif < if_num) && (if_vec[vif].if_flags & IF_FLAG_Disable)) {
		       vsock[vif] = -1;
		       Uvsock[vif] = -1;
		       continue;
		}
#endif
		/*	For this socket, need to be able to set the source
		 *	address.  Under SunOS, this requires a kernel change.
		 *	If this change is made to allow setting the IP header,
		 *	then don't need separate Uvsock[] series.
		 */
		vsock[vif] = send_socket(SOCK_RAW, IPPROTO_forRAW);
		Uvsock[vif] = send_socket(SOCK_RAW, IPPROTO_UDP);
		if (Test_mode)
			tst_sock2if[Uvsock[vif]] = vif;

		/*
		 *	Forward packets on this socket only to this interface
		 */
		if (!Test_mode) {
		    if ((cap_setsockopt(vsock[vif], IPPROTO_IP, IP_MULTICAST_IF,
				(char *) &(if_vec[vif].if_orig),
				sizeof(if_vec[vif].if_orig)) < 0)) {
		    	log(LOG_ERR, errno, "vsock IF", 0);
		   	exit(-1);
		    }
		    if ((cap_setsockopt(Uvsock[vif], IPPROTO_IP, IP_MULTICAST_IF,
				(char *) &(if_vec[vif].if_orig),
				sizeof(if_vec[vif].if_orig)) < 0)) {
		    	log(LOG_ERR, errno, "Uvsock IF", 0);
		   	exit(-1);
		    }
		}

#ifndef HOST_ONLY
		/*	If this is a multicast router (mrouted is running),
		 *	must disable normal multicast forwarding process
		 *	with new call added for RSVP.  But don't do it
		 *	if vif was disabled.
		 */
		/* XXX Re-initializing vifs requires re-starting RSVP.
		 */
		if (!NoMroute && !RSRR_is_disabled(vif) && !Test_mode &&
						(vif_flags(vif)&IFF_UP)) {
		    if ((cap_setsockopt(vsock[vif], IPPROTO_IP,IP_MULTICAST_VIF,
				    (char *) &vif, sizeof(int)) < 0)) {
			log(LOG_ERR, errno, "vsock VIF %d", vif);
			exit(-1);
		    }
		    if ((cap_setsockopt(Uvsock[vif], IPPROTO_IP,IP_MULTICAST_VIF,
				    (char *) &vif, sizeof(int)) < 0)) {
			log(LOG_ERR, errno, "vsock VIF %d", vif);
			exit(-1);
		    }
		}
#endif /* HOST_ONLY */

		rvsock[vif] = recv_socket(SOCK_RAW, IPPROTO_RSVP);
#ifndef HOST_ONLY
		if (!NoMroute && !RSRR_is_disabled(vif) && !Test_mode &&
						(vif_flags(vif)&IFF_UP)) {

		    if ((setsockopt(rvsock[vif], IPPROTO_IP, IP_RSVP_VIF_ON,
				    (char *) &vif, sizeof(int)) < 0)) {
			    UDP_Only = 1;
			    turn_off_rsvp_vif_intercept();
				/* Turn off any that succeeded. Ignore errors.
				*/
			} else {
			    /* Set handler for exit. */
			    if (!handler) {
#ifdef STANDARD_C_LIBRARY
				atexit(turn_off_rsvp_intercept);
#else
				on_exit(turn_off_rsvp_intercept, 0);
#endif
				handler = 1;
			    }
				/* Set to receive */
			    FD_SET(rvsock[vif], &fds);
			    fd_to_vif[rvsock[vif]] = vif;
			}
		 }
#endif /* ! HOST_ONLY */
	}

}

int
send_socket(int type, int protocol)
	{
	u_char	loop = 0;		/* No mcast loopback */
	int	sock;

#ifdef __sgi
	if (type == SOCK_RAW)
		sock = cap_socket(AF_INET, type, protocol);
	else
#endif
		sock = socket(AF_INET, type, protocol);
	if (sock < 0) {
		log(LOG_ERR, errno, "socket", 0);
		exit(-1);
	}
	if (!NoMcast) {
		if (cap_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
			(char *) &loop, sizeof(loop)) < 0) {
			log(LOG_ERR, errno, "mcast loop", 0);
			exit(-1);
		}
	}
	/* Note that TTL is set dynamically in do_sendto() */
	return(sock);
}


int
status_probe(int ifd)
	{
	char            recv_buf[MAX_PKT];
	struct sockaddr_in from;
	int             from_len, recv_len;

	from_len = sizeof(from);
	memset((char *) &from, 0, from_len);
	recv_len = recvfrom(ifd, recv_buf, sizeof(recv_buf),
			    0, (struct sockaddr *) & from, &from_len);
	if (recv_len < 0) {
		log(LOG_ERR, errno, "recvfrom", 0);
		return(-1);
	}
	send_rsvp_state(recv_buf, recv_len, &from);
	return(0);
}


int
recv_socket(int type, int protocol)
	{
	int	sock;

#ifdef __sgi
	if (type == SOCK_RAW)
		sock = cap_socket(AF_INET, type, protocol);
	else
#endif
		sock = socket(AF_INET, type, protocol);
	if (sock < 0) {
		log(LOG_ERR, errno, "socket", 0);
		exit(-1);
	}
	return(sock);
}

void
bind_socket(int sock, u_long addr, int port)
	{
	struct	sockaddr_in sin;
	int	one = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, 
							sizeof(one))) {
		log(LOG_ERR, errno, "REUSEADDR", 0);
		exit(-1);
	}		
	/*
	 *	Bind to encapsulation port.
	 */
	Set_Sockaddr_in(&sin, addr, port);
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin))) {
		log(LOG_ERR, errno, "bind", 0);
		exit(-1);
	}
}

#if defined(freebsd) && defined(IPRECVIF)
void enable_IP_RECVIF(int sock)
        {
	 int i;
	        if (setsockopt(sock, IPPROTO_IP, IP_RECVIF,
			       /* Need to use a dummy int for now */
			       (char *)&i, sizeof(int)) < 0) {
		  	log(LOG_INFO, 0, "No IP_RECVIF support in kernel\n");
		}
	}
#endif 
/*
 * start_UDP_encap(vif_no): Called to initialize for UDP encapsulation
 *	on specified vif number.
 */
int
start_UDP_encap(int vif)
	{	
	vif_flags(vif) |= IF_FLAG_UseUDP;
	return(0);
}


/*
 *  if_list_init():
 *		Initialize  table describing physical interfaces,
 *		if_vec[0...if_num].  Return number of interfaces if_num.
 *		The last element corresponds to the API.
 *
 *		As a side-effect, this routine defines kern_socket, which
 *		is used for communicating with the kernel traffic control
 *		code.
 */
int
if_list_init() {
	unsigned int    n;
	char            ifbuf[5000];
	struct ifconf   ifc;
	struct ifreq	*ifrp, ifr;
	char		*cifr, *cifrlim;
	int		if_num = 0;

	if ((kern_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log(LOG_ERR, errno, "kern_socket", 0);
		exit(-1);
	}
#if DEBUG
	if (Test_mode) {
		/* Test mode: read file named .rsvp.ifs containing:
		 *	<if name> <if address> <remote addr>
		 */
		FILE *fp = fopen(".rsvp.ifs", "r");
		char	buff[80], ipaddrstr[64], ifname[32], rmtaddrstr[64];
		
		if (fp == NULL) {
			fprintf(stderr, "Error reading .rsvpifs\n");
			exit(1);
		}
		if_vec = (if_rec *) calloc(RSRR_MAX_VIFS+1, sizeof(if_rec));
		tsttun_vec = (u_long *) calloc(RSRR_MAX_VIFS+1, sizeof(u_long));
		tst_sock2if = (int *) calloc(256, sizeof(int));
		memset(tst_sock2if, -1, 4*256);

		while (fgets(buff, sizeof(buff), fp)) {
			if (if_num > RSRR_MAX_VIFS) {
				fprintf(stderr, ".rsvpifs file too big\n");
				exit(1);
			}
			sscanf(buff, "%s %s %s", ifname, ipaddrstr, rmtaddrstr);
			if_vec[if_num].if_flags = 0;
			strncpy(if_vec[if_num].if_name, ifname, IFNAMSIZ);
			if_vec[if_num].if_orig.s_addr = hostname_cnv(ipaddrstr);
			tsttun_vec[if_num] = hostname_cnv(rmtaddrstr);
			if_num++;
		}
		strcpy(if_vec[if_num].if_name, "API"); 
		return(if_num);
	}
#endif	
	ifc.ifc_buf = ifbuf;
	ifc.ifc_len = sizeof(ifbuf);
	if (ioctl(kern_socket, SIOCGIFCONF, (char *) &ifc) < 0) {
		log(LOG_ERR, errno, "ioctl SIOCGIFCONF", 0);
		exit(1);
	}
	n = ifc.ifc_len / sizeof(struct sockaddr_in); /* just a guess */
	if_vec = (if_rec *) calloc(n+1, sizeof(if_rec));

	ifrp = ifc.ifc_req;
	cifr = (char *)ifrp;
	cifrlim = cifr + ifc.ifc_len;

	while(cifr < cifrlim) {
		if (!strcmp(ifrp->ifr_name, "lo0"))
			goto eek;
#ifdef NET2_STYLE_SIOCGIFCONF
		if (ifrp->ifr_addr.sa_family != AF_INET)
			goto eek;
#endif
		if_vec[if_num].if_orig.s_addr = ((struct sockaddr_in *) &
				   (ifrp->ifr_addr))->sin_addr.s_addr;
		if (IF_toip(if_num) == 0)
			goto eek;
		strncpy(if_vec[if_num].if_name, ifrp->ifr_name, IFNAMSIZ);
		ifr = *ifrp;
		if (ioctl(kern_socket, SIOCGIFFLAGS, (char *) &ifr) < 0) {
			log(LOG_ERR, errno, "ioctl SIOCGIFFLAGS", 0);
			exit(1);
		}
		if_vec[if_num].if_flags = (ifr.ifr_flags&IFF_UP);
		if_vec[if_num].if_udpttl = RSVP_TTL_ENCAP;
		if_num++;

eek:
#ifndef NET2_STYLE_SIOCGIFCONF
		cifr += sizeof(struct ifreq);
#else
		cifr += offsetof(struct ifreq, ifr_addr)+ifrp->ifr_addr.sa_len;
#endif
		ifrp = (struct ifreq *)cifr;
	}
	/* Add one more entry corresponding to API
	 */
	strcpy(if_vec[if_num].if_name, "API");
	if_vec[if_num].if_flags = IFF_UP;	/* Well, yes */

	return (if_num);
}


/*
 * get_vifs(): Read virtual interface (VIF) information from multicast
 *	routing and build vif_toif[] vector to map vif# -> if#.  Return
 *	# of vif's, or zero if multicast routing daemon doesn't respond
 *	to Initial Query.
 */
int
get_vifs()
{
#ifdef HOST_ONLY
	return -1;
#else
	int rc;

	/* To get the vifs, send a query to routing, and then wait for reply. */
	rc = rsrr_send_iq();
	
	if (rc > 0)
		return rsrr_get_ir();
	else
		return -1;
#endif

}


/* Initialize RSRR socket
 */
void
rsrr_init()
{
#ifndef HOST_ONLY

    if ((rsrr_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
	log(LOG_ERR, errno, "Can't create RSRR socket", 0);
	exit(-1);
    }

    /* Server's address. */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, RSRR_SERV_PATH);
#ifdef STANDARD_C_LIBRARY
    servlen = (offsetof(struct sockaddr_un, sun_path)
              + strlen(serv_addr.sun_path));
#else
    servlen = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);
#endif
#ifdef SOCKADDR_LEN
    serv_addr.sun_len = servlen;
#endif

    /* Client's address. */
    memset((char *) &cli_addr, 0, sizeof(cli_addr));
    cli_addr.sun_family = AF_UNIX;
    strcpy(cli_addr.sun_path, RSRR_CLI_PATH);
#ifdef STANDARD_C_LIBRARY
    clilen = (offsetof(struct sockaddr_un, sun_path)
             + strlen(cli_addr.sun_path));
#else
    clilen = sizeof(cli_addr.sun_family) + strlen(cli_addr.sun_path);
#endif
#ifdef SOCKADDR_LEN
    cli_addr.sun_len = clilen;
#endif

    /* Remove on the off-chance that it was left around. */
    unlink(cli_addr.sun_path);

    if (bind(rsrr_socket, (struct sockaddr *) &cli_addr, clilen) < 0) {
	log(LOG_ERR, errno, "Can't bind RSRR socket", 0);
	exit(-1);
    }

    rsrr_qt_init();
#endif /* ! HOST_ONLY */
}

#ifndef HOST_ONLY
/* Get an Initial Reply from routing.
 * Block internally here with timeout, in case routing isn't out there.
 */
int
rsrr_get_ir()
{

    fd_set fds_read;
    int fds_max,fds_ready;
    static struct timeval timeout;

    FD_ZERO(&fds_read);

    FD_SET(rsrr_socket, &fds_read);
    fds_max = rsrr_socket + 1;

    /* Timeout for routing to respond is 5 seconds. */
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    fds_ready = select(fds_max, &fds_read, (fd_set *) NULL,
			   (fd_set *) NULL, &timeout); 

    if (fds_ready < 0) {
	log(LOG_ERR, errno, "select");
	exit(-1);
    }
    
    if (fds_ready == 0) {
	if (IsDebug(DEBUG_RSRR))
	    log(LOG_DEBUG, 0, "Assuming routing isn't out there.\n");

	/* Assume routing isn't out there. */
	return -1;
    }

    if (FD_ISSET(rsrr_socket, &fds_read)) {
	/* Read RSRR Initial Reply*/
	return rsrr_read(RSRR_INITIAL_REPLY);
    }

    /* Shouldn't ever get here, but return error in case we do. */
    return -1;
}


/* Read a message from the RSRR socket, and call rsrr_accept to process it.
 * Return an error if the message is not of the type specified.
 */
int
rsrr_read(expected_type)
    u_char expected_type;
{
    register int rsrr_recvlen;
    int dummy = 0;

    rsrr_recvlen = recvfrom(rsrr_socket, rsrr_recv_buf, sizeof(rsrr_recv_buf),
			    0, (struct sockaddr *) 0, &dummy);
    if (rsrr_recvlen < 0) {	
	log(LOG_ERR, errno, "recvfrom", 0);
	return -1;
    }
    return rsrr_accept(rsrr_recvlen,expected_type);
}


/* Send Initial Query RSRR message
 */
int
rsrr_send0(int sendlen) {
	int error;

	error = sendto(rsrr_socket, rsrr_send_buf, sendlen, 0,
	       (struct sockaddr *)&serv_addr, servlen);

	if (errno == ECONNREFUSED)
		return 0;
	else if (error < 0) {
		log(LOG_ERR, errno, "Sending on RSRR socket", 0);
		return -1;
	}
	return 1;
}


/* Send an RSRR message
 */
int
rsrr_send(int sendlen) {
	int error;

	error = sendto(rsrr_socket, rsrr_send_buf, sendlen, 0,
	       (struct sockaddr *)&serv_addr, servlen);

	if (error < 0) {
		log(LOG_ERR, errno, "Sending on RSRR socket", 0);
		return -1;
	}
	if (error != sendlen) {
		log(LOG_ERR, 0,
	    "Sent only %d out of %d bytes on RSRR socket\n", error, sendlen);
		return -1;
   	}

	return 0;
}
#endif /* ! HOST_ONLY */


/*
 * init_api() initializes the api between the daemon and client applications.
 * The daemon listens for incoming requests (coming on a unix socket) from
 * application, and stores some local info about these requests. This info
 * includes a copy of an RSVP packet with the needed info, that will be
 * refreshed and throughn in every refresh period.
 */

void
init_api()
{
	int	i;
	struct sockaddr_un server;
	int 	addr_len;

	memset(api_table, 0, sizeof(api_table));

	(void) unlink(SNAME);	/* The unix socket name */
	if ((api_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		log(LOG_WARNING, errno, "opening socket\n");
		assert(NULL);	/* i.e. abort() */
	}
	server.sun_family = AF_UNIX;
	(void) strcpy(server.sun_path, SNAME);
#ifdef STANDARD_C_LIBRARY
	/*
	 * This is only necessary for Net/2 and later, but it's never
	 * incorrect on earlier systems, so do it.
	 */
	addr_len = (offsetof(struct sockaddr_un, sun_path)
		    + strlen(server.sun_path));
#else
	addr_len = sizeof(server.sun_family) + strlen(server.sun_path);
#endif
#ifdef SOCKADDR_LEN
	server.sun_len = addr_len;
#endif
	if (bind(api_socket, (struct sockaddr *) & server, addr_len) < 0) {
		log(LOG_WARNING, errno, "bind");
		assert(NULL);	/* i.e. abort() */
	}
	i = 1;
#ifdef SOLARIS
	if (fcntl(api_socket, F_SETFL, (fcntl(api_socket, F_GETFL) | O_NONBLOCK)) == -1) {
		log(LOG_ERR, errno, "Setting Non-blocking I/O\n");
		exit(-1);
	}
#else
	if (ioctl(api_socket, FIONBIO, &i) == -1) {
		log(LOG_ERR, errno, "Setting Non-blocking I/O\n");
		exit(-1);
	}
#endif /* SOLARIS */
	(void) listen(api_socket, 10);	/* Maximum pending req. at one time */
}

/*
 *	do_sendmsg(): Send buffer containing RSVP message:
 *		* To specified vif; -1 => unspecified (Resv msgs)
 *		* To destination (addr,port) = *toaddrp
 *		* From IP source address = *srcaddrp; NULL => unspecified
 *		* tp specified TTL (unless UDP encaps is used)
 *
 *	This routine figures out whether UDP and/or raw encapsualated copies
 *	are needed, and chooses the appropriate socket.  The socket choice
 *	is quite system-dependent.
 */
int
do_sendmsg(
	int vif,
	struct sockaddr_in *toaddrp,
	struct in_addr	*srcaddrp,
	int		 flags,
	u_char		 ttl,
	u_char		*data,
	int		 len)
	{
	struct sockaddr_in sin;
	int		mode = 0;
	u_char		encap_ttl;

	if (vif >= 0) {
		if (!(vif_flags(vif)&IF_FLAG_IFF_UP))
			return(mode); /* Should never happen? */
		encap_ttl = if_vec[vif_toif[vif]].if_udpttl;
	} else
		encap_ttl = RSVP_TTL_MAX;

	if (NoRawIO || UDP_Only) {
		/*	UDP-only host.
		 */
		if (IN_MULTICAST(ntoh32(toaddrp->sin_addr.s_addr)) ) {
			/*	Multicast Path msg: => (G*, P)
			 */
			sin = encap_sin;
		}
		else if (vif >= 0 && !IsLocal(AF_INET, &toaddrp->sin_addr)) {
			/*	Unicast Path msg to non-local D => (Ra, Pu)
			 */
			Set_Sockaddr_in(&sin,
				Gateway(toaddrp->sin_addr.s_addr), encap_port);
		}
		else {
			/* 	Unicast Path/Resv msg to local D => (D, Pu)
			 */
			Set_Sockaddr_in(&sin, toaddrp->sin_addr.s_addr, 
								encap_port);
		}
		if (vif >= 0)
			do_sendto(Uisock[vif], &sin, srcaddrp, PKTFLG_USE_UDP,
				encap_ttl, data, len);
		else
			do_sendto(rsvp_Usocket, &sin, srcaddrp, PKTFLG_USE_UDP,
				encap_ttl, data, len);
		mode = 1;
	}
	else if (((vif >= 0 && (vif_flags(vif) & IF_FLAG_UseUDP)) ||
		 (flags & PKTFLG_USE_UDP)) &&
				(IsLocal(AF_INET, &toaddrp->sin_addr))){

		/*	Need UDP encapsulation on this interface, but we
		 *	are capable of sending raw packets => (D, Pu')
		 */
		Set_Sockaddr_in(&sin, toaddrp->sin_addr.s_addr, encap_portp);
		flags |= PKTFLG_USE_UDP;

		if (IN_MULTICAST(ntoh32(toaddrp->sin_addr.s_addr))) {
			flags |= (PKTFLG_RAW_UDP|PKTFLG_SET_SRC);
			do_sendto(Uvsock[vif], &sin, srcaddrp, flags,
				encap_ttl, data, len);
		}
		else if (vif >= 0)
		    do_sendto(Uisock[vif], &sin, srcaddrp, flags,
				encap_ttl, data, len);
		else
		    do_sendto(rsvp_Usocket, &sin, srcaddrp, flags,
				encap_ttl, data, len);
		mode = 1;
	}
	flags &= ~(PKTFLG_USE_UDP|PKTFLG_RAW_UDP);
	if (!NoRawIO && !UDP_Only) {
		/*
		 *	Send raw encapsulation
		 */
		if (IN_MULTICAST(ntoh32(toaddrp->sin_addr.s_addr)) ) {
		    flags |= PKTFLG_SET_SRC;
		    do_sendto(vsock[vif], toaddrp, srcaddrp, flags,
				ttl, data, len);
		}
		else if (vif >= 0)
		    do_sendto(isock[vif], toaddrp, srcaddrp, flags,
				ttl, data, len);
		else
		    do_sendto(rsvp_socket, toaddrp, srcaddrp, flags, 
				ttl, data, len);
		mode += 2;
	}
	return(mode);
}


/*
 *	do_sendto(): Send packet with specific:
 *		* IP destination address = *tosockaddr
 *		* IP source address = *srcaddrp, if SET_SRC flag is on.
 *		* TTL = pkt_ttl
 *		* Router Alert option if Send_RA flag is on.
 *
 *	If required, build UDP and/or IP header in front of RSVP header,
 *	using space in buffer in front of rsvp_data.
 *
 *	This routine is unfortunately quite system-dependent.
 *
 */
int
do_sendto(
	int sock,
	struct sockaddr_in *tosockaddr,
	struct in_addr	*srcaddrp,
	int		 flags,
	u_char		 ttl,
	u_char		*data,
	int		 len)
	{
	int		 bytes, offset = 0;
	struct udphdr	*uhp;
	struct sockaddr_in sin = *tosockaddr;	/* Local copy */
	u_char		 Router_alert[4] = {148, 4, 0, 0};
	int ttl_int=ttl;


	if (flags & PKTFLG_RAW_UDP) {
		/*
		 *	Must build UDP header to send encapsulated pkt
		 *	through raw socket.
		 */
		offset = sizeof(struct udphdr);
		uhp = (struct udphdr *) ((char *)data - offset);
		uhp->uh_sport = sin.sin_port;
		uhp->uh_dport = sin.sin_port;
		sin.sin_port = 0;	/* Port goes in UDP header */
		uhp->uh_ulen = hton16(len + offset);
		uhp->uh_sum = 0;  	/* RSVP checksum is enough */
	}

#ifdef SET_SRC
	if (flags & PKTFLG_SET_SRC) {
		struct ip	*ipp;

		/*
		 *	Must set IP src address (maybe not my own), so build
		 *	IP header here.
		 *	Note that ip_output will move (UDP&)RSVP header(s)
		 *	if needed to make room for Router Alert option.
		 */
		offset += sizeof(struct ip);
		ipp = (struct ip *)((char *)data - offset);
		ipp->ip_v = IPVERSION;
		ipp->ip_hl = sizeof(struct ip) >> 2;
		ipp->ip_off = 0;
		ipp->ip_tos = 0;
		ipp->ip_p = (flags & PKTFLG_USE_UDP) ? IPPROTO_UDP:
								IPPROTO_RSVP;
		ipp->ip_len = len+offset;
		ipp->ip_ttl = ttl;
		ipp->ip_id = ipp->ip_sum = 0;  /* kernel will fill in */
		ipp->ip_src.s_addr = srcaddrp->s_addr;
		ipp->ip_dst.s_addr = sin.sin_addr.s_addr;
	}
	else
#endif
	if (ttl != sock_send_ttl[sock]) {
		/*
		 *	If we are not building our own IP header and if
		 *	this ttl != last one specified for this socket,
		 *	call setsockopt to set new ttl.
		 */
		if (IN_MULTICAST(ntoh32(tosockaddr->sin_addr.s_addr))) {
			if (cap_setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
				(char *) &ttl, sizeof(ttl)) < 0) {
					log(LOG_ERR, errno, "mcast TTL", 0);
				return(-1);
			}
		}
#ifdef SET_IP_TTL
		/* For systems that define IP_TTL socket option */
		 else {
			if (setsockopt(sock, IPPROTO_IP, IP_TTL,
				(char *) &ttl_int, sizeof(ttl_int)) < 0) {
					log(LOG_ERR, errno, "set ucast TTL", 0);
				return(-1);

			}
		}
#endif
	}
	sock_send_ttl[sock] = ttl;

	/*	If Send-RA flag differs from last one specified for this
	 *	socket, call setsockopt to impose/remove Router Alert option.
	 */
	if (flags & PKTFLG_USE_UDP) {
		/* Don't check flag if UDP encapsulation in use */
	}
	else if ((flags & PKTFLG_Send_RA) && !sock_send_RA[sock]) {
		if (setsockopt(sock, IPPROTO_IP, IP_OPTIONS, 
			(void *)Router_alert, sizeof(Router_alert)) < 0){
			log(LOG_ERR, errno, "Router alert");
			return(-1);
		}
		sock_send_RA[sock] = 1;
	}
	else if (!(flags & PKTFLG_Send_RA) && sock_send_RA[sock]) {
		if (setsockopt(sock, IPPROTO_IP, IP_OPTIONS, 
			(void *)Router_alert, 0) < 0){
			log(LOG_ERR, errno, "!Router alert");
			return(-1);
		}
		sock_send_RA[sock] = 0;
	}
	if (l_debug >= LOG_HEXD) {
		log(LOG_DEBUG, 0, "Output to %s on socket %d\n",
				iptoname(&sin.sin_addr), sock);
		hexf(stderr, (char *)data-offset, len+offset);
	}

	if (Test_mode) {
		int ifn = tst_sock2if[sock];

		if (ifn >= 0)
			sin.sin_addr.s_addr = tsttun_vec[ifn];
		else
			return(0);
	}
	bytes = sendto(sock, (char *)data-offset,
		len+offset, 0,
		(struct sockaddr *) &sin, sizeof(struct sockaddr_in));

	if (bytes == -1) {
		log(LOG_ERR, errno, "sendto", 0);
		return (-1);
	}
	return (0);
}

/*
 * raw_input(): receive an RSVP protocol message (protocol 46)
 *		from socket 'ifd', through interface 'inp_if'
 */
int
raw_input(
	int ifd,	/* File descriptor of input socket */
	int inp_if)	/* Input interface number (-1 => mcast) */
	{
	struct ip      *ip = (struct ip *) recv_buff;
	struct sockaddr_in from, sin;
	int		app_int = 0, i, rc;
	int             from_len, recv_len, iphdrlen, ipdatalen;		
	struct packet	packet;
	struct	 {
		packet_map	raw_map_base;
		union	{
	  	  SenderDesc Sender_list;	/* For Path-like messages */
	  	  FlowDesc   Resv_list;		/* For Resv-like messages */
		}  raw_map_ext[MAX_FLWDS-1];
	}		map;	

#if defined (freebsd) && defined(IPRECVIF)      /* For the recvmsg() call */
	struct msghdr mhdr; 
        struct iovec iohdr;
        struct cmsghdr *chdr;
        struct sockaddr_dl *sdl;

	 /* setup the iovec for receving raw datagrams */
        memset((char *) &iohdr, 0, sizeof(struct iovec));
        iohdr.iov_base = (void *)recv_buff;
        iohdr.iov_len  = sizeof(recv_buff);
        memset((char *) &mhdr, 0, sizeof(struct msghdr));
#endif
	from_len = sizeof(from);
	memset((char *) &from, 0, from_len);

#if !defined(freebsd) || !defined(IPRECVIF)
	recv_len = recvfrom(ifd, recv_buff, sizeof(recv_buff),
			    0, (struct sockaddr *) & from, &from_len);
	if (recv_len < 0) {
		log(LOG_ERR, errno, "recvfrom", 0);
		return(-1);
	}
#else   /* setup msghdr for freebsd */

	 mhdr.msg_name = (caddr_t)&from ;
	 mhdr.msg_namelen = from_len;
	 mhdr.msg_iov = &iohdr ;
	 mhdr.msg_iovlen = 1 ;     /* one iovec element in scatter/gather */
	 mhdr.msg_control = (caddr_t)recv_ctrl_buff ;
	                           /* IP_RECVIF support, will fill this with 
				      incoming phys. interface index */
	 mhdr.msg_controllen = MAX_SOCK_CTRL ;
	 mhdr.msg_flags = 0;
	 recv_len = recvmsg(ifd,&mhdr,0);
 	 if (recv_len < 0) {
		log(LOG_ERR, errno, "recvmsg", 0);
		return(-1);
	 }
  	 chdr = (struct cmsghdr *)recv_ctrl_buff ;
	 if (inp_if == -1 && chdr->cmsg_len != 0) {      
                                  /* IP_RECVIF support for VIFs excluded */
	                          /* for now */
	  if(chdr->cmsg_level == IPPROTO_IP && chdr->cmsg_type == IP_RECVIF){
           sdl = (struct sockaddr_dl *)CMSG_DATA(chdr);
           inp_if = sdl->sdl_index -1; 
          }
          else 
	   log(LOG_INFO, 0, "Unknown control info, retrieving inc. interface\n");
	 }
#endif  /* freebsd && IPRECVIF*/

#ifdef __alpha__
	iphdrlen = (ip->ip_vhl & 0xf) << 2;
#else
	iphdrlen = ip->ip_hl << 2;
#endif
	ipdatalen = ip->ip_len;
	if (iphdrlen + ipdatalen != recv_len) {
		log(LOG_ERR, 0, "Packet length wrong", 0);
		return(-1);
	}
	/*
	 *	If dest addr is multicast group, test for and drop the
	 *	packet if we sent it ourselves.  This "accidental" loopback
	 *	can only happen if there is an application running on this
	 *	(router) node, which has joined this multicast group; then
	 *	multicast Path, etc messages send to vsock[i] will be
	 *	received on rsvp_socket.
	 */
	if (IN_MULTICAST(ntoh32(ip->ip_dst.s_addr))) {
		for (i = 0; i < if_num; i++)
			if (ip->ip_src.s_addr == IF_toip(i)) {
				return(0);
			}
	}
	else
		/*	Unicast dest in IP header: map into apparent local
		 *	interface number, to be sure it was for me.
		 */
		app_int = map_if(&ip->ip_dst);


	packet.pkt_order = BO_NET;
	packet.pkt_len = ipdatalen;
	packet.pkt_data = (common_header *) &recv_buff[iphdrlen];
	packet.pkt_offset = 0;
	packet.pkt_map = (packet_map *) &map;
	packet.pkt_flags = 0;
	packet.pkt_ttl = ip->ip_ttl;
	/*
	 *	If the message type is not Path or PathTear or ResvConf and if
	 *	IP destination address does not match any of the
	 *	addresses of the local interfaces, then forward the
	 *	message to IP destination address and return.
	 */
	if ((app_int < 0) &&
	     packet.pkt_data->rsvp_type != RSVP_PATH &&
	     packet.pkt_data->rsvp_type != RSVP_CONFIRM &&
	     packet.pkt_data->rsvp_type != RSVP_PATH_TEAR) {

		Set_Sockaddr_in(&sin, ip->ip_dst.s_addr, 0);
		rc = sendto(rsvp_socket,
				(char *)&recv_buff[iphdrlen], ipdatalen, 0,
				(struct sockaddr *) &sin,
				 sizeof(struct sockaddr_in));

		if (rc < 0) {
			log(LOG_ERR, errno, "sendto", 0);
			return (-1);
		}
		return(0);
	}
	if (l_debug >= LOG_HEXD) {
		log(LOG_DEBUG, 0, "Raw input from %s on vif %d\n",
				iptoname(&from.sin_addr), inp_if);
		hexf(stderr, (char *)packet.pkt_data, packet.pkt_len);
	}
	/*
	 *  Call common routine to do initial processing of RSVP packet.
	 */
	rc = rsvp_pkt_process(&packet, &from, inp_if);
	if (rc < 0) {
		log(LOG_WARNING, 0, "\n%s Parse error %d in raw pkt from %s\n",
			rsvp_timestamp(), rc, iptoname(&from.sin_addr));
		hexf(stdout, (char *)packet.pkt_data, packet.pkt_len);
	}
	return(0);
}

/*
 * udp_input(): receive a UDP-encapsulated RSVP protocol message
 */
int
udp_input(
	int ifd,	/* File descriptor of UDP socket */
	int inp_if)	/* Input interface number (-1 => mcast) */
	{
	struct sockaddr_in from;
	int             from_len, recv_len, rc;
	struct packet	packet;
	struct	 {
		packet_map	raw_map_base;
		union	{
	  	  SenderDesc Sender_list;	/* For Path-like messages */
	  	  FlowDesc   Resv_list;		/* For Resv-like messages */
		}  raw_map_ext[MAX_FLWDS-1];
	}		map;
#if defined(freebsd) && defined(IPRECVIF)       /* For recvmsg() call */
        struct msghdr mhdr; 
        struct cmsghdr *chdr ;
        struct iovec iohdr;
        struct sockaddr_dl *sdl ; 
	 
        /* setup the iovec for receving raw datagrams */

        memset((char *) &iohdr, 0, sizeof(struct iovec));
        iohdr.iov_base = (void *)recv_buff;
        iohdr.iov_len  = sizeof(recv_buff);
        memset((char *) &mhdr, 0, sizeof(struct msghdr));
#endif
	from_len = sizeof(from);
	memset((char *) &from, 0, from_len);
#if !defined(freebsd) || !defined(IPRECVIF)
	recv_len = recvfrom(ifd, recv_buff, sizeof(recv_buff),
			    0, (struct sockaddr *) & from, &from_len);
	if (recv_len < 0) {
		log(LOG_ERR, errno, "recvfrom", 0);
		return (-1);
	}
#else   /* setup msghdr for freebsd */
	 mhdr.msg_name = (caddr_t)&from ;
	 mhdr.msg_namelen = from_len;
	 mhdr.msg_iov = &iohdr ;
	 mhdr.msg_iovlen = 1 ;     /* one iovec element in scatter/gather */
	 mhdr.msg_control = (caddr_t)recv_ctrl_buff ;
	                           /* IP_RECVIF support, will fill this with 
				      incoming phys. interface index */
	 mhdr.msg_controllen = MAX_SOCK_CTRL ;
	 mhdr.msg_flags = 0;
	 recv_len = recvmsg(ifd,&mhdr,0);
 	 if (recv_len < 0) {
		log(LOG_ERR, errno, "recvmsg", 0);
		return(-1);
	 }
  	 chdr = (struct cmsghdr *)recv_ctrl_buff ;
	 if (inp_if == -1 && chdr->cmsg_len != 0) {      
                                  /* IP_RECVIF support for VIFs excluded */
	                          /* for now */
	  if(chdr->cmsg_level == IPPROTO_IP && chdr->cmsg_type == IP_RECVIF){
           sdl = (struct sockaddr_dl *)CMSG_DATA(chdr);
           inp_if = sdl->sdl_index -1; 
          }
          else 
	   log(LOG_INFO, 0, "Unknown control info, retrieving inc. interface\n");
	 }
#endif   /* freebsd */
	packet.pkt_order = BO_NET;
	packet.pkt_len = recv_len;
	packet.pkt_data = (common_header *) recv_buff;
	packet.pkt_offset = 0;
	packet.pkt_map = (packet_map *) &map;
	packet.pkt_flags = PKTFLG_USE_UDP;
	packet.pkt_ttl = packet.pkt_data->rsvp_snd_TTL;
	if (l_debug >= LOG_HEXD) {
		log(LOG_DEBUG, 0, "UDP input from %s on vif %d\n",
				iptoname(&from.sin_addr), inp_if);
		hexf(stderr, (char *)packet.pkt_data, packet.pkt_len);
	}

	if (Test_mode) {
		int i;

		for (i= 0; i <if_num; i++)
			if (from.sin_addr.s_addr == tsttun_vec[i])
				break;
		if (i == vif_num) {
			fprintf(stderr,"Other Src: %s\n", inet_ntoa(from.sin_addr));
			return(0);
		}
		inp_if = i;
	}
	/*
	 *  Call common routine to do initial processing of RSVP packet.
	 */
	rc = rsvp_pkt_process(&packet, &from, inp_if);
	if (rc < 0) {
		log(LOG_WARNING, 0, "\n%s Parse error %d in UDP pkt from %s\n",
			rsvp_timestamp(), rc, iptoname(&from.sin_addr));
		hexf(stdout, (char *)packet.pkt_data, packet.pkt_len);
	}
	return (0);
}


/*
 * api_input(): reads an API request from UNIX pipe, and processes it.
 *		First 4 bytes are length of following request.
 */
int
api_input(int fd) {
	int             rc, len;
	int		sid;
	char            recv_buf[MAX_MSG];
	rsvp_req	*req = (rsvp_req *) recv_buf;
	int		ret_val = 0;

	rc = read(fd, &len, sizeof(int));
	if (rc != sizeof(int) ||
	   (rc = read(fd, (char *) req, len)) != len) {
		if (rc == 0) {
            		/* 
             		 *  Application quit or died rather than closed 
            		 *  the RSVP connection; not an error.
          		 */      
			ret_val = 0;
		}
		else {
			log(LOG_ERR, errno, "Error in API read\n");
			ret_val = -1;
		}
		/*	No message from which to derive the SID, but we 
		 *	have to close all sids that used that fd.
		 */ 
		for (sid = 0; sid < API_TABLE_SIZE; sid++) { 
			if (api_table[sid].api_fd == fd) {
				api_table[sid].api_fd = 0;
					/* prevent upcall */
				process_api_req(sid, NULL, 0);
				rsvp_api_close(sid); 
			}
		} 
           
		/* 	Now close the socket.
		 */          
		FD_CLR(fd, &fds); 
		(void) close(fd); 
		return ret_val; 
        }

	if (req->rq_type == API_DEBUG) {
		char *cmd = (char *)req + sizeof(rsvp_req);
		return(api_cmd(fd, (rapi_cmd_t *)cmd));
	}
	else if (req->rq_type == API2_STAT)
		/* Trigger event upcalls to return status */
		return(api_status(req));
		

	/*	Locate or create local sid for this request, given the
	 *	fd (Unix pipe) on which it arrived, the process from which
	 *	it came, and client's sid.
	 */
	sid = find_sid(fd, req->rq_pid, req->rq_a_sid);

	if (sid < 0 && req->rq_type == API2_REGISTER) { 
		/*  First message; look for available slot. 
		 */ 
		sid = find_free_sid(fd, req->rq_pid, req->rq_a_sid); 
             
		if (sid < 0) { /* XXX api error */ 
			log(LOG_WARNING, 0, "API: too many sessions\n"); 
			return (-1); 
		}
		memset(&api_table[sid], 0, sizeof(api_rec)); 
		api_table[sid].api_fd = fd; 
		api_table[sid].api_pid = req->rq_pid; 
		api_table[sid].api_a_sid = req->rq_a_sid; 
		num_sessions++; 
	}
	else if (sid < 0) {
 		log(LOG_ERR, 0, "API: req %d from ?? pid= %dAsid= %d\n",
				req->rq_type, req->rq_pid, req->rq_a_sid); 
		return(-1); 
	}

	if (process_api_req(sid, req, rc) < 0) {
		/* Return code = -1 => release the session.
		 */
		api_table[sid].api_fd = 0;
		rsvp_api_close(sid);
		if (req->rq_type != API_CLOSE) {
			/* Even though only one session had error,
			 * close the Unix socket to the process.
			 */
			(void) close(fd);
			FD_CLR(fd, &fds);
			return(-1);
		}
	}
	return(0);
}


int
sid_hash(int fd, int pid, int client_sid) { 
	u_int32_t ufd = fd; 
	u_int32_t upid = pid; 
	u_int32_t u_client_sid = client_sid;

	return ((((ufd*65599 + upid)*65599) + u_client_sid)*65599) % 
							API_TABLE_SIZE; 
} 
 
/*
 * find_sid(): Search API table to locate sid for this fd, pid, and client_sid
 *
 *	Use open hash table.  Return -1 if no match.
 */
int
find_sid(int fd, int pid, int client_sid) 
	{ 
	int first_sid = sid_hash(fd, pid, client_sid); 
	int test_sid, i; 
	api_rec * test_rec; 
 
	for (i=0, test_sid=first_sid; i<API_TABLE_SIZE;  test_sid++, i++) 
		{
		if (test_sid >= API_TABLE_SIZE) 
			test_sid -= API_TABLE_SIZE; 
 
		test_rec = &api_table[test_sid]; 
       
		if (test_rec->api_fd == fd 
			&& test_rec->api_pid == pid 
			&& test_rec->api_a_sid == client_sid) 
				return test_sid;
		else if (test_rec->api_fd == 0)
			return(-1);
	}
	return -1;                        /* no match */ 
} 

 
/*  find_free_sid():  Returns sid for a free slot in API table api_table[],
 *		for this fd, pid and client_sid.  Does not allocate.
 */
int
find_free_sid(int fd, int pid, int client_sid) 
	{ 
	int first_sid = sid_hash(fd, pid, client_sid); 
	int test_sid, i; 
	api_rec * test_rec; 
 
	/* do not allow hash table to get more than half full */ 
	if (num_sessions >= MAX_SESSIONS) 
		return -1; 
 
	for (i=0, test_sid=first_sid; i<API_TABLE_SIZE; test_sid++, i++) 
		{
		if (test_sid >= API_TABLE_SIZE) 
			test_sid -= API_TABLE_SIZE; 
 
		test_rec = &api_table[test_sid]; 
       
		if (test_rec->api_fd == 0) 
			return test_sid; 
	}
	return -1;                        /* no match */ 
} 

/* 
 *	Release API session
 */
void
rsvp_api_close(int sid) {
	api_rec *recp = &api_table[sid];

	del_from_timer((char *) (unsigned long)sid, TIMEV_API);
	api_free_packet(&recp->api_p_packet);
	api_free_packet(&recp->api_r_packet);
	num_sessions--;
}


/*
 * refresh_api():  Called from process_api_req when new request
 *	arrives from API, and periodically from timer.  It injects
 *	stored API packets into the processing as if they had arrived
 *	from the network (except API packets are in host byte order).
 *
 *      Returns 0 normally to restart timer, -1 to kill timer.
 */
int
refresh_api(int sid) {
	api_rec		*recp = &api_table[sid];
	int		rc;

	/*
	 *  Make sure the process still exists before keeping the message
	 *  alive.
	 */
	if (kill(recp->api_pid, 0) == -1)
		if (errno == ESRCH)
			return(-1);

	/* Also make sure we still have record for it
	 */
	if (recp->api_p_packet.pkt_data == NULL &&
	    recp->api_r_packet.pkt_data == NULL)
		return(-1);

	if ((recp->api_p_packet.pkt_data)) {
		rc = rsvp_pkt_process(&recp->api_p_packet, NULL, vif_num);
		if (rc < 0) {  /* Internal error */
			log(LOG_ERR, 0, "\nParse error %d in API pkt\n", rc);
			hexf(stdout, (char *)recp->api_p_packet.pkt_data, 
					recp->api_p_packet.pkt_len);
		}
	}
	if ((recp->api_r_packet.pkt_data)) {
		rc = rsvp_pkt_process(&recp->api_r_packet, NULL, vif_num);
		if (rc < 0) {  /* Internal error */
			log(LOG_ERR, 0, "\nParse error %d in API pkt\n", rc);
			hexf(stdout, (char *)recp->api_r_packet.pkt_data, 
					recp->api_r_packet.pkt_len);
		}
	}
	return (0);
}


/*
 * reset_log() resets a log, if it is too long. It renames it to another name
 * and opens a new log. This way there is a limit on the size of the logs,
 * and at most 2 logs exists at any given time.
 *
 * The date and time are written in the header of each logfile.
 */

void
reset_log(int init) {
	char            header[81];
	time_t          clock;

	if (!init) {
		fprintf(logfp, ".\n.\n= END OF LOG, ... start a new log =\n");
		(void) fclose(logfp);
	}
	(void) unlink(RSVP_LOG_PREV);
	(void) rename(RSVP_LOG, RSVP_LOG_PREV);
	if ((logfp = fopen(RSVP_LOG, "w")) == NULL) {
		fprintf(stderr, "RSVP: can\'t open log file %s\n", RSVP_LOG);
		assert(!init);
		return;
	}
	setbuf(logfp, (char *) NULL);
	sprintf(header, "%s\n", vers_comp_date);
	log(LOG_ALWAYS, 0, header);
	clock = time((time_t *) NULL);
        log(LOG_ALWAYS, 0, "Log level:%d  Debug mask:%d  Start time: %s", 
			l_debug, debug,	ctime(&clock));
}

/*
 *  api_cmd() takes a file descriptor and a command message from the API
 *	executes the command in the message.
 *
 *	DON'T FORGET to set the response code, 'old'
 */
int
api_cmd(int fd, rapi_cmd_t *cmd) {
	int old;
	if (cmd->rapi_cmd_type == RAPI_CMD_DEBUG_MASK) {
		old = debug;
		if (cmd->rapi_filler != (u_short)-1) {
			debug = cmd->rapi_filler;
			log(LOG_INFO, 0, "Debug Mask changed from %d to %d\n",
				old, debug);
		} else
			log(LOG_INFO, 0, "Debug Mask unchanged at %d\n", old);
	} else if (cmd->rapi_cmd_type == RAPI_CMD_DEBUG_LEVEL) {
		old = l_debug;
		if (cmd->rapi_filler != (u_short)-1) {
			l_debug = cmd->rapi_filler;
			log(LOG_INFO, 0, "Debug Level changed from %d to %d\n",
				old, l_debug);
		} else
			log(LOG_INFO, 0, "Debug Level unchanged at %d\n", old);
	} else if (cmd->rapi_cmd_type == RAPI_CMD_FILTER_ON) {
		old = debug_filter;
		if (cmd->rapi_filler != (u_short)-1) {
			debug_filter = 1;
			log(LOG_INFO, 0, "Debug filter now 1, was %d\n", old);
		} else
			log(LOG_INFO, 0, "Debug filter unchanged at %d\n", old);
	} else if (cmd->rapi_cmd_type == RAPI_CMD_FILTER_OFF) {
		old = debug_filter;
		if (cmd->rapi_filler != (u_short)-1) {
			debug_filter = 0;
			debug_filter_num = 0;
			log(LOG_INFO, 0, "Debug filter now 0, was %d\n", old);
		} else
			log(LOG_INFO, 0, "Debug filter unchanged at %d\n", old);
	} else if (cmd->rapi_cmd_type == RAPI_CMD_ADD_FILTER) {
		if (debug_filter != 1) {
			log(LOG_INFO, 0, "debug_filter should be 1\n");
			old = -1;
		} else {
			int i;
			int s1 = cmd->rapi_data[0];
			for (i = 0; i < debug_filter_num; i++)
				if (debug_filters[i] == s1)
					break;
			if (i == debug_filter_num) {
				if (debug_filter_num >= MAX_DEBUG_FILTER) {
					log(LOG_ERR, 0, "Too many filters\n");
					old = -1;
				} else {
					debug_filters[debug_filter_num++] = s1;
					old = debug_filter_num;
				}
			}
		}
	} else if (cmd->rapi_cmd_type == RAPI_CMD_DEL_FILTER) {
		if (debug_filter != 1) {
			log(LOG_INFO, 0, "debug_filter should be 1\n");
			old = -1;
		} else {
			int i;
			int s1 = cmd->rapi_data[0];
			for (i = 0; i < debug_filter_num; i++)
				if (debug_filters[i] == s1)
					break;
			if (i == debug_filter_num) {
				log(LOG_ERR, 0, "No filter to delete\n");
				old = -1;
			} else {
				old = --debug_filter_num;
				debug_filters[i] = debug_filters[old];
			}
		}
	}
/****  Don't tell...
	 Ack the application with the old debug value
	if (write(fd, (char *) &old, sizeof(old)) == -1)
		log(LOG_ERR, errno, "write() error\n");
****/
	FD_CLR(fd, &fds);
	(void) close(fd);
	return (0);
}

#ifndef __sgi		
/*
 * test_alive(): Check whether daemon is already running, return its pid;
 *	else create file containing my pid and return 0.
 */
int
test_alive() {
	int             pid, rc;
	FILE           *fp;

	if (((fp = fopen(RSVP_PID_FILE, "r")) != NULL) &&
	    (fscanf(fp, "%u", &pid) == 1)) {
		rc = kill(pid, 0);
		if (rc == 0 || errno != ESRCH) {
			if (errno != ESRCH)
				perror("kill");
			(void) fclose(fp);
			return (pid);
		}
		(void) fclose(fp);
	}
	return(0);
}

int
save_pid() {
	FILE *fp;
	int pid = getpid();
	if (unlink(RSVP_PID_FILE) == -1) {
		if (errno != ENOENT)
			perror("unlink");
	}
	if ((fp = fopen(RSVP_PID_FILE, "w")) == NULL) {
		perror("fopen");
		return (-1);
	}
	fprintf(fp, "%u\n", pid);
	(void) fclose(fp);
	return (0);
}
#endif /* __sgi */


int
IsLocal(int domain, struct in_addr *adrp)
	{
	int i;

	assert(domain == AF_INET);
	if (IN_MULTICAST(ntoh32(adrp->s_addr)))
		return(1);

	for (i=0; i<if_num; i++)
		if (inet_netof(*adrp) == inet_netof(if_vec[i].if_orig))
			return(1);
	return(0);
}

u_long
Gateway(u_long addr)
	{
	return encap_router.s_addr;
}


u_int32_t
hostname_cnv(char *cp)
	{
	struct hostent *hp;

	if (isdigit(*cp))
		return inet_addr(cp);
	else if ((hp = gethostbyname(cp)))
		return *(u_int32_t *) hp->h_addr;
	else	return(NULL);
}


/*	Map IP address into local interface number, or -1
 */
int
map_if(struct in_addr *adrp)
	{
	int	ifn;

	for (ifn= if_num-1; ifn >= 0; ifn--)
		if (if_vec[ifn].if_orig.s_addr == adrp->s_addr)
			break;
	return (ifn);
}

void
print_ifs()
	{
	int i;
	char  flgs[40];

        log(LOG_ALWAYS, 0, "Physical interfaces are:\n");
	for (i=0; i < if_num+1; i++) {
		flgs[0] = '\0';
		if (!(if_vec[i].if_flags & IFF_UP))
			strcat(flgs, "DOWN ");
		else if (if_vec[i].if_up)
			strcat(flgs, "TCup ");
		else
			strcat(flgs, "NoIS ");
		if (if_vec[i].if_flags & IF_FLAG_UseUDP)
			strcat(flgs, "UDP ");
		if (if_vec[i].if_flags & IF_FLAG_Intgrty)
			strcat(flgs, "Intgry ");
		if (if_vec[i].if_flags & IF_FLAG_Police)
			strcat(flgs, "Police ");	

 		if (if_vec[i].if_flags & IF_FLAG_Disable)
			strcpy(flgs, "RSVP Disabled!");
 		if (if_vec[i].if_flags & IF_FLAG_PassNormal)
			strcpy(flgs, "Bypass TC, process RSVP normally");
 		if (if_vec[i].if_flags & IF_FLAG_PassBreak)
			strcpy(flgs, "Bypass TC, set Break Bit");

        	log(LOG_ALWAYS, 0, "%d %-8.8s %-16.16s %s\n", i,
			if_vec[i].if_name, inet_ntoa(if_vec[i].if_orig), flgs);
	}
}

/*	Read and parse a configuration file.
 */
void
read_config_file(char *fname)
        {
        FILE            *fd;
	char		 cfig_line[80], word[64];
	char		*cp;


        fd = fopen(fname, "r");
        if (!fd) {
                if (errno != ENOENT)
                        log(LOG_ERR, 0, "Cannot open config file\n");
                return;
        }

	log(LOG_INFO, 0, "Reading configuration file %s\n", fname);

        while (fgets(cfig_line, sizeof(cfig_line), fd)) {

		cp = cfig_line;
		if (*cp == '#' || !next_word(&cp, word))
			continue;
		cfig_do(&cp, word);

		/*	Loop to parse keywords in line
		 */
		while (*cp != '\n' && *cp != '#'&& *cp != '\0') {
		    next_word(&cp, word);
		    cfig_do(&cp, word);
		}
		if (*cp == '\0') {
		    log(LOG_ERR, 0, "Config file line 2 long: %s\n",cfig_line);
			exit(1);
		}
	}
}
			
/* This does not do as much syntax-checking as one might like... */				    		
void
cfig_do(char **cpp, char *word)
	{
	Cfig_element	*cep, *tcep;
	char		parm1[64], parm2[64];

	for (cep = Cfig_Table; cep->cfig_keywd; cep++)
		if (!pfxcmp(word, cep->cfig_keywd))
			break;
	if (!cep->cfig_keywd) {
		log(LOG_ERR, 0, "Unknown config verb: %s\n", word);
		exit(1);
	}
	for (tcep = cep+1; tcep->cfig_keywd; tcep++)
		if (!pfxcmp(word, tcep->cfig_keywd)) {
			log(LOG_ERR, 0, "Ambiguous keyword: %s\n", word);
			exit(1);
		}

	if (cep->cfig_flags & (CFF_HASPARM|CFF_HASPARM2)) {
		if (!next_word(cpp, parm1)) {
			log(LOG_ERR, 0, "Missing parm for %s\n", word);
			exit(1);
		}
	}
	if (cep->cfig_flags & CFF_HASPARM2) {
		if (!next_word(cpp, parm2)) {
			log(LOG_ERR, 0, "Missing parm for %s\n", word);
			exit(1);
		}
	}
	cfig_action(cep->cfig_actno, parm1, parm2);
}

void
cfig_action(int act, char *parm1, char *parm2)
	{
	int i;
	KEY_ASSOC *kap;

	switch (act) {
	
	case CfAct_iface:
		for (i = 0; i < if_num; i++) {
			if (!strcmp(parm1, if_vec[i].if_name))
				break;
		}
		if (i >= if_num) {
			log(LOG_WARNING, 0, "Unknown interface %s, skipping rest of this line\n", parm1);
			Cfig_if = -1;
			break;
		}
		Cfig_if = i;
		break;

	case CfAct_police:
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_Police;
		break;	

	case CfAct_udp:
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_UseUDP;
		break;	

	case CfAct_udpttl:
		if (Cfig_if != -1) {
		     if_vec[Cfig_if].if_flags |= IF_FLAG_UseUDP;
		     if_vec[Cfig_if].if_udpttl = atoi(parm1);
		}
		break;		

	case CfAct_intgr:
		/* integrity  -- require INTEGRITY object in message
		 *		received through this interface
		 */
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_Intgrty;
		break;

	case CfAct_disable:
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_Disable;
		break;

	case CfAct_passnormal:
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_PassNormal;
		break;

	case CfAct_passbreak:
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_flags |= IF_FLAG_PassBreak;
		break;

	case CfAct_sndttl:
		/* Set default TTL for Resv messages, etc. */
		log(LOG_WARNING, 0, "sndttl config parm not yet supported\n");
		break;

	case CfAct_sendkey:
		/* sendkey <int: keyid> <hex: key>  --
		 *		keyid and key for sending message to this
		 *		interface.
		 */
		kap = load_key(parm1, parm2);
		if (!kap)
			exit(1);
		kap->kas_if = Cfig_if;
		kap->kas_sender.s_addr = INADDR_ANY;
		break;

	case CfAct_neighbor:
		Cfig_neighbor.s_addr = hostname_cnv(parm1);
		if (Cfig_neighbor.s_addr == INADDR_ANY) {
			log(LOG_ERR, 0, "Unknown config host %s\n", parm1);
			exit(1);
		}
		break;

	case CfAct_recvkey:
		/* recvkey <int: keyid> <hex: key>  --
		 *		keyid and key for receiving message to this
		 *		interface.
		 */
		kap = load_key(parm1, parm2);
		if (!kap)
			exit(1);
		kap->kas_sender = Cfig_neighbor;
		kap->kas_if = -1;
		break;
	
	case CfAct_refresh:
		log(LOG_WARNING, 0, "refresh config parm not yet supported\n");
		if (Cfig_if != -1)
		     if_vec[Cfig_if].if_Rdefault = atoi(parm1);
		break;

	default:
		assert(0);
	}
}

#ifndef RTAP
/*
 * Skip leading blanks, then copy next word (delimited by blank or zero, but
 * no longer than 63 bytes) into buffer b, set scan pointer to following 
 * non-blank (or end of string), and return 1.  If there is no non-blank text,
 * set scan ptr to point to 0 byte and return 0.
 */
int 
next_word(char **cpp, char *b)
{
	char           *tp;
	int		L;

	*cpp += strspn(*cpp, " \t");
	if (**cpp == '\0' || **cpp == '\n' || **cpp == '#')
		return(0);

	tp = strpbrk(*cpp, " \t\n#");
	L = MIN((tp)?(tp-*cpp):strlen(*cpp), 63);
	strncpy(b, *cpp, L);
	*(b + L) = '\0';
	*cpp += L;
	*cpp += strspn(*cpp, " \t");
	return (1);
}

/*
 * Prefix string comparison: Return 0 if s1 string is prefix of s2 string, 1
 * otherwise.
 */
int 
pfxcmp(s1, s2)
	register char  *s1, *s2;
{
	while (*s1)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

#endif

#ifndef SECURITY
KEY_ASSOC *
load_key(char *keyidstr, char *keystr)
	{
	log(LOG_ERR, 0, "Keys unsupported\n");
	return(NULL);
}
#endif
