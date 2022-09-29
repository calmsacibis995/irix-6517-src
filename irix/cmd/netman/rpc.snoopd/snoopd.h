#ifndef SNOOPD_H
#define SNOOPD_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoopd include file
 *
 *	$Revision: 1.9 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/xdr.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/svc.h>
#include "packetbuf.h"
#include "snoopd_rpc.h"

/*
 * Options
 */
struct options {
	u_int		opt_queuelimit;
	u_int		opt_usenameservice;
	u_int		opt_packetbufsize;
	u_int		opt_rawsequence;
	u_int		opt_loglevel;
	struct timeval	opt_deadtime;
};

/*
 * Interfaces and Filters
 */
struct iftab {
	struct iftab	*if_next;
	struct iftab	*if_prev;
	char		if_ifname[RAW_IFNAMSIZ+1];
	char		*if_hostname;
	struct in_addr	if_addr;
	u_int		if_clients;
	u_int		if_errclients;
	u_int		if_started;
	struct snooper	*if_snooper;
	struct protocol	*if_rawproto;
	u_int		if_mtu;
	u_int		if_snooplen;
	u_short		if_errflags;
	u_short		if_file;
	PacketBuf	if_pb;
};

#define if_filename if_hostname

struct filter {
	u_short		f_promisc;
	u_short		f_errflags;
	struct expr	*f_expr;
};

void iftable_init(void);
struct iftab *iftable_match(char *);
enum snoopstat errno_to_snoopstat(int);
enum snoopstat snoop_status(struct iftab *);
int snoop_open(struct iftab *);
void snoop_start(struct iftab *);
void snoop_setsnooplen(struct iftab *, u_int);
void snoop_seterrflags(struct iftab *, u_short);
void snoop_freefilter(struct iftab *, struct filter *);
int snoop_update(struct iftab *, struct expr *, struct exprerror *,
		 struct filter *);
int snoop_test(struct iftab *, u_short, struct filter *,
	       struct snooppacket *, int);
int snoop_read(struct iftab *, u_int);
void snoop_stop(struct iftab *);
void snoop_close(struct iftab *);

/*
 * Authentication Rules
 */
struct authrule {
	struct authrule		*ar_next;
	struct authentry	*ar_hosts;
	struct authentry	*ar_users;
	struct authentry	*ar_groups;
	struct authentry	*ar_services;
};

struct authentry {
	struct authentry	*ae_next;
	unsigned int		ae_wildcard;
	union {
	    struct in_addr	aeu_address;
	    uid_t		aeu_uid;
	    gid_t		aeu_gid;
	    struct authentry	*aeu_glist;
	    enum svctype	aeu_service;
	} ae_u;
};

#define ae_address	ae_u.aeu_address
#define ae_uid		ae_u.aeu_uid
#define ae_gid		ae_u.aeu_gid
#define ae_glist	ae_u.aeu_glist
#define ae_service	ae_u.aeu_service

void authorize_init(void);
int authorize(struct in_addr, struct authunix_parms *, enum svctype);

/*
 * Clients
 */
enum scstate {
	STOPPED = 0,
	STARTED = 1,
	BLOCKED = 2,
	ZOMBIE = 3
};

typedef struct snoopclient {
	void			*sc_private;
	struct snoopclient	*sc_next;
	struct snoopclient	*sc_prev;
	SVCXPRT			*sc_xprt;
	enum scstate		sc_state;
	struct iftab		*sc_if;
	int			(*sc_blkproc)();
	void			*sc_blkarg;
	struct timeval		sc_starttime;
	u_int			sc_snooplen;
	u_short			sc_errflags;
	u_short			sc_service;
	struct snoopstats	sc_stats;
	u_long			sc_sddrops;
} SnoopClient;

#define sc_seq			sc_stats.ss_seq
#define sc_ifdrops		sc_stats.ss_ifdrops
#define sc_sbdrops		sc_stats.ss_sbdrops

/*
 * Timer Frequency
 */
#define TICKTIME	100000
#define TICKS_PER_SEC	(1000000 / TICKTIME)

#endif /* !SNOOPD_H */
