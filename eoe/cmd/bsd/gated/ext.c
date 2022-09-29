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

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/ext.c,v 1.3 1994/10/17 18:46:34 sfc Exp $";
#endif	not lint

#define	RIPCMDS

#include "include.h"

/*
 * Socket declarations for each routing protocol and ioctl's
 */
int egp_socket;			/* egp raw socket descriptor */
#ifndef	NSS
int rip_socket;			/* rip raw socket descriptor */
int hello_socket;		/* hello raw socket descriptor */
int icmp_socket;		/* icmp raw socket descriptor */
#endif	NSS
int s;				/* socket for kernel ioctl calls */

/*
 * Routing tables and restriction table.
 */
struct rthash nethash[ROUTEHASHSIZ];
struct rthash hosthash[ROUTEHASHSIZ];
struct restricthash rt_restrict[ROUTEHASHSIZ];

/*
 * EGP specific variables
 */
u_short	mysystem;		/* autonomous system number */
struct as_list *my_aslist = NULL;	/* list of autonomous systems to announce to my AS */
int	nneigh;			/* number of trusted egp neighbors */
int	maxacq;			/* maximum number neighbors to be acquired */
int	n_acquired;		/* number neighbors acquired */
int	egpsleep;		/* seconds between egpjob() wakeups */
struct	egpngh	*egpngh;	/* start egp neighbor state table linked list */
u_short	egprid_h;		/* sequence number of received egp packet
				   in host byte order - all ids in internal
				   tables are in host byte order */
/*
 * configuratrion specification variables.
 */
int	conv_factor;		/* default EGP metric with no restrictions */
int	doing_rip;		/* Are we running RIP protocols? */
int	doing_hello;		/* Are we running HELLO protocols? */
int	doing_egp;		/* Are we running EGP protocols? */
int	rip_pointopoint;	/* Are we ONLY doing pointopoint RIP? */
int	hello_pointopoint;	/* Are we ONLY doing pointopoint HELLO? */
int	rip_supplier;		/* Are we broadcasting RIP protocols? */
int	hello_supplier;		/* Are we broadcasting HELLO protocols? */
int	rip_gateway;		/* Are we broadcasting a RIP default? */
int	hello_gateway;		/* Are we broadcasting a HELLO default? */
int	rip_default;		/* Default metric for RIP default */
int	hello_default;		/* Default metric for HELLO default */
int	announcethesenets;	/* is there an announce restrict list? */
int	donotannounce;		/* is there a forbid restrict list? */
int	glob_announcethesenets;	/* is there a global announce restrict list? */
int	glob_donotannounce;	/* is there a global forbid restrict list? */
int	donotlisten;		/* is there a listen black list? */
int	islisten;		/* is there a source listen black list? */

/*
 * Miscellaneous variables.
 */
char *my_name;                  /* name we were invoked as */
char *version_kernel;		/* OS version of the kernel */
char *my_hostname;		/* Hostname of this system */
int my_pid;                     /* my process ID */
struct interface *ifnet;	/* direct internet interface list */
#ifdef sgi
struct  interface **ifnext = &ifnet;	/* pointer to pointer to end of list */
#endif
int n_interfaces;		/* # internet interfaces */
int n_remote_nets;		/* # remote nets via internal static routes */
int n_routes;			/* # nets in routing table */
int tracing;			/* log errors, route changes &/or packets */
int savetrace;			/* save trace flags */
int are_tracing;		/* are we tracing? */
char *logfile;			/* the log file */
int terminate;			/* terminate EGP process */
int install;			/* if TRUE install route in kernel */
int rt_default_active = FALSE;	/* TRUE if gateway default is active */
int rt_maxage;			/* maximum allowed age of any route */
int maxpollint;			/* max poll interval of acquired neighbors */
time_t gatedtime;		/* time of day in secs of current interrupt */
char	*strtime;		/* time of day as an ASCII string */
long last_time;			/* time of day rt_time() was last called */
#ifdef	NSS
int	rttable_changed = 0;	/* Routing table has been changed */
#endif	NSS

int sched_a_dump;		/* flag to schedule an information dump */
int do_reinit;			/* Flag to indicate a reinit should be done */
FILE *ftrace = NULL;
struct advlist *adlist;		/* nets to go into NR update */
struct advlist *srcriplist;	/* gateways to speak RIP directly to */
struct advlist *srchellolist;	/* gateways to speak HELLO directly to */
struct advlist *trustedripperlist;	/* trusted RIP gateways */
struct advlist *trustedhelloerlist;	/* trusted HELLO gateways */
struct advlist *martians;	/* List of Martian nets */
struct as_list *sendas;		/* List of AS's to propogate */
struct as_valid *validas;	/* List of nets vs AS */
struct rt_entry *default_gateway;      /* Pointer to default gateway entry for dynamic restore */
char message[128];		/* For sprintfing error messages into */
char *err_message = &message[0];
#ifndef	vax11c
char *Gated_Configuration_File = INITFILE;	/* the configuration file */
#endif	vax11c

/*
 * HELLO protocol, default route specification.
 */
struct sockaddr_in hello_dfltnet;

/*
 *	EGP statistics
 */
struct egpstats_t egp_stats = { 0, 0, 0, 0 };

/*
 *	Names for the various bits
 */

struct bits flagbits[] = {
		{ RTF_UP,		"UP" },
		{ RTF_GATEWAY,		"GATEWAY" },
		{ RTF_HOST,		"HOST" },
		{ 0 }
	};
	
struct bits statebits[] = {
		{ RTS_PASSIVE,		"PASSIVE" },
		{ RTS_REMOTE,		"REMOTE" },
		{ RTS_INTERFACE,	"INTERFACE" },
		{ RTS_CHANGED,		"CHANGED" },
		{ RTS_NOTINSTALL,	"NOTINSTALL" },
		{ RTS_NOTADVISENR,	"NOTADVISENR" },
		{ RTS_SUBNET,		"SUBNET" },
		{ RTS_POINTOPOINT,	"POINTOPOINT" },
		{ RTS_STATIC,		"STATIC" },
		{ RTS_HOSTROUTE,	"HOSTROUTE" },
		{ RTS_INTERIOR,		"INTERIOR" },
		{ RTS_EXTERIOR,		"EXTERIOR" },
		{ 0 }
	};
	
struct bits intfbits[] = {
		{ IFF_UP,		"UP" },
		{ IFF_BROADCAST,	"BROADCAST" },
		{ IFF_DEBUG,		"DEBUG" },
		{ IFF_ROUTE,		"ROUTE" },
		{ IFF_POINTOPOINT,	"POINTOPOINT" },
#ifdef sgi
		{ IFF_RUNNING,		"RUNNING" },
#endif
		{ IFF_SUBNET,		"SUBNET" },
		{ IFF_PASSIVE,		"PASSIVE" },
		{ IFF_INTERFACE,	"INTERFACE" },
		{ IFF_REMOTE,		"REMOTE" },
                { IFF_NORIPOUT,		"NORIPOUT" },
                { IFF_NORIPIN,		"NORIPIN" },
                { IFF_NOHELLOOUT,	"NOHELLOOUT" },
                { IFF_NOHELLOIN,	"NOHELLOIN" },
                { IFF_NOAGE,		"NOAGE" },
                { IFF_ANNOUNCE,		"ANNOUNCE" },
                { IFF_NOANNOUNCE,	"NOANNOUNCE" },
		{ 0 }
	};
	
struct bits protobits[] = {
		{ RTPROTO_EGP,		"EGP" },
#ifndef	NSS
		{ RTPROTO_RIP,		"RIP" },
#else	NSS
		{ RTPROTO_IGP,		"IGP" },
#endif	NSS
		{ RTPROTO_HELLO,	"HELLO" },
		{ RTPROTO_KERNEL,	"KERNEL" },
		{ RTPROTO_REDIRECT,	"REDIRECT" },
		{ RTPROTO_DIRECT,	"DIRECT" },
		{ RTPROTO_DEFAULT,	"DEFAULT" },
		{ 0 }
	};
	
struct bits egp_states[] = {
		{ NGS_IDLE,		"IDLE" },
		{ NGS_ACQUISITION,	"ACQUISITION" },
		{ NGS_DOWN,		"DOWN" },
		{ NGS_UP,		"UP" },
		{ NGS_CEASE,		"CEASE" },
		{ -1 }
	};
	
struct bits egp_flags[] = {
		{ NG_BAD,		"BAD" },
		{ NG_METRICIN,		"METRICIN" },
		{ NG_METRICOUT,		"EGPMETRICOUT" },
		{ NG_ASIN,		"ASIN" },
		{ NG_ASOUT,		"ASOUT" },
		{ NG_NOGENDEFAULT,	"NOGENDEFAULT" },
		{ NG_DEFAULTIN,		"ACCEPTDEFAULT" },
		{ NG_DEFAULTOUT,	"DEFAULTOUT" },
		{ NG_VALIDATE,		"VALIDATE" },
		{ NG_INTF,		"INTF" },
		{ NG_SADDR,		"SADDR" },
		{ NG_GATEWAY,		"GATEWAY" },
		{ NG_AS,		"AS" },
		{ 0,			0 }
	};

struct bits trace_types[] = {
		{ TR_INT|TR_EXT|TR_RT|TR_EGP,	"general" },
		{ TR_ALL, "all" },
		{ TR_INT,		"internal" },
		{ TR_EXT,		"external" },
		{ TR_RT,		"route" },
		{ TR_EGP,		"egp" },
		{ TR_UPDATE,		"update" },
		{ TR_RIP,		"rip" },
		{ TR_HELLO,		"hello" },
		{ TR_ICMP,		"icmp" },
		{ TR_JOB,		"job" },
		{ TR_STAMP,		"stamp" },
#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)
		{ TR_SNMP,		"sgmp" },
		{ TR_SNMP,		"snmp" },
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)
#ifdef	NSS
		{ TR_ISIS,		"is-is" },
		{ TR_ESIS,		"es-is" },
		{ TR_SPF,		"spf" },
		{ TR_NSSRT,		"nssrt" },
#endif	NSS
		{ 0,			0 }
	};

#if	defined(AGENT_SNMP) || defined(AGENT_SGMP)
struct bits snmp_types[] = {
		{ AGENT_REG,	"REGISTER" },
		{ AGENT_REQ,	"REQUEST" },
		{ AGENT_ERR,	"ERROR" },
		{ AGENT_RSP,	"RESPONSE" },
		{ AGENT_REQN,	"REQNEXT" },
		{ 0,		0 }
	};
#endif	defined(AGENT_SNMP) || defined(AGENT_SGMP)


/*
 *	Names for EGP states and bits
 */

char *egp_acq_codes[5] = {
	"REQUEST",
	"CONFIRM",
	"REFUSE",
	"CEASE",
	"CEASE-ACK" };
	
char *egp_reach_codes[2] = {
	"HELLO",
	"I-H-U" };
	
char *egp_nr_status[3] = {
	"INDETERMINATE",
	"UP",
	"DOWN" };
	
char *egp_acq_status[8] = {
	"UNSPECIFIED",
	"ACTIVE MODE",
	"PASSIVE MODE",
	"INSUFFICIENT RESOURCES",
	"PROHIBITED",
	"GOING DOWN",
	"PARAMETER PROBLEM",
	"PROTOCOL VIOLATION" };
	
char *egp_reasons[7] = {
	"unspecified",
	"bad EGP header format",
	"bad EGP data field format",
	"reachability info unavailable",
	"excessive polling rate",
	"no response",
	"unsupported version" };
 
/*
 *	Definitions for interfacing with NYSERnet's SGMP daemon
 */
 
#ifdef AGENT_SNMP

int snmp_socket;

#endif AGENT_SNMP


#ifdef	AGENT_SGMP

int sgmp_socket;

#endif	AGENT_SGMP
