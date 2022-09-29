/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/dhcp_gen.h,v 1.2 1996/07/06 17:42:51 nn Exp $"

#ifndef	_DHCP_GEN_H
#define	_DHCP_GEN_H

/*
 * dhcp_gen.h - Generic DHCP definitions shared between the 'net dhcp' and
 * 'nsclient' client implementations.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DHCP state machine states.
 */
typedef enum {	INIT,		/* EVERYBODY starts out in this state */
		SELECTING,	/* No config. Discover one. */
		REQUESTING,	/* Respond to offered configurations */
		VERIFY,		/* Verify an existing config, renegotiate */
		BOUND,		/* Set internal settings based on config. */
		CONFIGURED,	/* Write configuration to permanent store */
		RELINQUISH	/* Relinquish configuration. */
} DHCPSTATE;

/*
 * Number of seconds to wait and collect OFFER replies to our DISCOVER.
 */
#define	DHCP_WAIT	4		/* Wait 4 secs for OFFERS */
#define	DHCP_FOREVER	((u_int)0xffff)	/* "forever" means 64k tries. */
#define	DHCP_T1_FACT	2		/* factor used for default t1 */
#define	DHCP_T2_FACT	8		/* factor used for default t2 */
#define	DHCP_LEASE_TIME	7		/* Default lease extension in days */
#define	DHCP_PERM	((u_long)0xffffffffL) /* a "permanent" lease time */
#define	DHCP_SN_TRIES	5 		/* # of tries ICMP -> subnetmask */
#define	DHCP_SCRATCH	128		/* Size of static scratch buffer */
#define	DHCP_RESOLV	"RESOLV.DHC"	/* Name of DHCP DNS file */

/*
 * Lease info.
 */
typedef struct {
	time_t	lease;	/* abs time when the lease expires */
	time_t	t1;	/* abs time when renegotiation starts */
	time_t	t2;	/* abs time when lease expiration warnings start */
} DHCPLI;

/*
 * Client id for a pcnfs client.
 */
#define	DHCP_CLIENT_ID_LEN	17
typedef struct {
	u_char	id[DHCP_CLIENT_ID_LEN];	/* Type, Hardware address */
	u_char	len;			/* Len of JUST the hardware addr */
} DHCPCLNT;

#define	PKT_LIST_SAVE	0
#define	PKT_LIST_DEL	1

#define	IPPORT_DHCP_SERV	67		/* server udp port */
#define	IPPORT_DHCP_CLNT	68		/* client udp port */
#define	BOOTREQUEST		1		/* BOOTP REQUEST opcode */
#define	BOOTREPLY		2		/* BOOTP REPLY opcode */
#define	BOOTMAGIC	{ 99, 130, 83, 99 }	/* rfc1048 magic cookie */

/*
 * Ugh. A workaround for the Nfsi update local vs far problems. We REALLY
 * ought to fix this. XXXX
 */
#define	NFSI_UD(xn, xv, xl) \
	(void) memcpy((char *)&(___nfsi.xn), (char *)(xv), (xl)); \
	(void) _fmemcpy((char far *)&(___nfsi_p->xn), (char far *)(xv), (xl))
#define	NFSI_SET(xn, xy) \
	___nfsi.xn = xy; \
	___nfsi_p->xn = xy
#define	NFSI_ZERO(xn, xy) \
	(void) memset((char *)&(___nfsi.xn), 0, (xy)); \
	(void) _fmemset((char far *)&(___nfsi_p->xn), 0, (xy))

/* globals */
extern time_t	dhcp_start_time;
extern time_t	dhcp_secs;
extern int	dhcp_max_size;
extern int	dhcp_max_opt_size;
extern int	dhcp_retries;
extern char	dhcp_already_configured;
extern int	dchp_pkt_counter;
extern u_char	dhcp_snd_buf[];
extern u_char	dhcp_rcv_buf[];
extern PKT_LIST	*list_hd, *state_pl;
extern struct in_addr servers;
extern u_char	opt_param_req[];
extern u_char	opt_class_id[];
extern DHCPCLNT	client_id;
extern DHCPLI	li;
extern char	*s_n;		/* dhcp_gen.c */
extern DHCPSTATE dhcp_state;	/* dhcp_gen.c */

extern int dhcp_configure(char *, char *);
extern int dhcp_selecting(int);
extern int dhcp_requesting(void);
extern int dhcp_verify(int, int *);
extern int dhcp_relinquish(char *);
extern int dhcp_bound(void);
extern int dhcp_configured(void);
extern int try_servers(DHCP_OPT **, char *, int (*)(u_long), int);
extern void process_vendor(DHCP_OPT **);
extern int bootp_collect(int);
extern int bootp_success(void);

extern void remove_list(PKT_LIST *, int);
extern void flush_list(void);
extern void reset_timeout(void);
extern u_char *init_msg(PKT *, u_char *);
extern void set_hw_spec_data(PKT *, u_char **);
extern PKT_LIST *select_best(void);
extern void send_decline(char *, PKT_LIST *);
extern void send_release(char *, PKT_LIST *);
extern void process_dns(PKT_LIST *);
extern void prt_server_msg(DHCP_OPT *);
extern int stat_hostconf(void);
extern int load_hostconf(PKT_LIST **);
extern int write_hostconf(PKT_LIST *);
extern void remove_hostconf(void);
extern int inet(u_int, u_int, struct in_addr *, u_short, struct in_addr *,
    u_short, u_int, time_t, time_t (*)(void), int (*)(int), int(*)(void));
extern time_t	set_timeout(void);
extern int lease_expired(void);
extern int use_time_serv(u_long);
extern int set_tz_env_var(int);
extern int setdostime(u_long);
extern int arp_request(u_long);
extern int arp_reply(u_long);
extern void arp_error(PKT_LIST *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_GEN_H */
