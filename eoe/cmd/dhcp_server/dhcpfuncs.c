#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include "dhcp.h"
#include "dhcpdefs.h"
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <ndbm.h>
#include <syslog.h>

extern char	*ether_ntoa(EtherAddr *);
extern struct	dhcp_request_list *dhcp_rlist;
extern struct	dhcp_timelist *dhcp_tmlist_first;
extern struct	dhcp_timelist *dhcp_tmlist_last;
extern char	*myname;
extern int	dh0_timeouts_cnt;
extern int	ping_number_pending;
extern int	getdomainname(char *, int);
extern int	sys0_addToHostsFile(u_long, char *, char *);
extern int	sys0_addToEthersFile(EtherAddr *, char *);
extern u_long	loc0_get_ipaddr_from_cid(EtherAddr *, char *, int, DBM *);
extern int	sys0_removeFromHostsFile(u_long);
extern int	sys0_removeFromEthersFile(EtherAddr *);

char	*dh0_getNisDomain(void);
int	dh0_create_system_mapping(u_long, EtherAddr *, char *, char *);
int	dh0_remove_system_mapping(char *, EtherAddr *, int, DBM *);
char	*dh0_getDnsDomain(void);
u_long	*dh0_get_dnsServers(void);
struct dhcp_request_list*
	dh0_add_to_dhcp_reqlist(char *, int, EtherAddr *, u_long, char *,
				struct getParams *, u_long, u_int,
				dhcpConfig *, ping_status_t, int, int,
				char *, int, struct bootp*, struct bootp*,
				struct dhcp_client_info*);
int	dh0_upd_dhcp_rq_entry(struct dhcp_request_list *,
			      struct getParams *, u_int);
int	dh0_remove_from_dhcp_reqlist(char *, int);
int	dh0_addto_timelist(char *, int, EtherAddr *, time_t, 
			   struct dhcp_request_list*);
int	dh0_free_timelist_entry(struct dhcp_timelist *);
int	dh0_remove_from_timelist(char *, int);
int	dh0_set_timeval(struct timeval *, long);
long	dh0_update_and_get_next_timeout(time_t);
struct dhcp_timelist	 *dh0_find_timelist_entry(char *, int);
struct dhcp_timelist	 *dh0_pop_timelist_timer(void);
struct dhcp_request_list *dh0_find_dhcp_rqlist_entry(char *, int);
static int free_dhcp_rq(struct dhcp_request_list *);
extern	ping_send_DHCPOFFER(struct dhcp_request_list*);

char *
dh0_getNisDomain(void)
{
    char dmname[64];
    char *dm;

    if(getdomainname(dmname, sizeof(dmname)) == 0) {
	if(dmname[0] == '\0') {
	    return (char *)0;
	}
	else {
	    dm = strdup(dmname);
	    return(dm);
	}
    }
    return (char *)0;
}

/* Add entry to the /etc/hosts and /etc/ethers files */
int
dh0_create_system_mapping(u_long ipaddr, EtherAddr *eth, char *hsname, char *nisdm)
{
    int rval;

    rval = sys0_addToHostsFile(ipaddr, hsname, nisdm);
    if(rval){
      return(1);
    }
    rval = sys0_addToEthersFile(eth, hsname);
    if(rval){
      return(2);
    }
    return(0);
}

/* Delete the entry from the /etc/hosts and /etc/ethers files */
int
dh0_remove_system_mapping(char *cid_ptr, EtherAddr *eth, int cid_length,
			  DBM *db)
{
    int rval;
    u_long addrfromCid;

    addrfromCid = loc0_get_ipaddr_from_cid(eth, cid_ptr, cid_length, db);
    if(addrfromCid == 0)
	return 1;

    rval = sys0_removeFromHostsFile(addrfromCid);
    if(rval)
	return rval;
    
    rval = sys0_removeFromEthersFile(eth);
    return rval;
}

char *
dh0_getDnsDomain(void)
{
    char *dm;

    res_init();
    dm = _res.defdname;
    if((dm == 0) || (*dm == '\0'))
	return (char *)0;
    dm = strdup(_res.defdname);
    return dm;
}

u_long *
dh0_get_dnsServers(void)
{
    u_long *dnssrvs;
    int i;

    res_init();
    if(_res.nscount == 0)
	return 0;
    dnssrvs = (u_long *)malloc(sizeof(u_long) * (_res.nscount + 1));
    for(i = 0; i < _res.nscount; i++)
	dnssrvs[i] = _res.nsaddr_list[i].sin_addr.s_addr;
    dnssrvs[i] = 0;
    return dnssrvs;
}

struct dhcp_request_list*
dh0_add_to_dhcp_reqlist(char *cid_ptr, int cid_length, EtherAddr *ead,
			u_long ipa, char *hsname, struct getParams *reqparams,
			u_long srv, u_int lease, dhcpConfig *cfg,
			ping_status_t ping_stat, int ident, int seqnum,
			char *dh_msg, int has_sname,
			struct bootp* pingrp, struct bootp* pingrq,
			struct dhcp_client_info *dcip_data)
{
    struct dhcp_request_list *rq;

    /* check whether an entry exists to avoid duplicates */
    if (rq = dh0_find_dhcp_rqlist_entry(cid_ptr, cid_length)) {
	bcopy(cid_ptr, rq->cid_ptr, cid_length);
	/*rq->cid_ptr = strdup(cid_ptr); */
	
	rq->cid_length = cid_length;
	rq->dh_ipaddr = ipa;
	if (rq->dh_hsname)
	    free(rq->dh_hsname);
	if(hsname) 
	    rq->dh_hsname = strdup(hsname);
	else
	    rq->dh_hsname = 0;
	bcopy(reqparams, &rq->dh_reqParams, sizeof(struct getParams));
	rq->dh_reqServer = srv;
	rq->dh_reqLease = lease;
	rq->dh_configPtr = cfg;
	rq->ping_status = ping_stat;
	if (ping_stat == PING_SENT) {
	    memcpy(&rq->ping_rp, pingrp, sizeof (struct bootp));
	    memcpy(&rq->ping_rq, pingrq, sizeof (struct bootp));
	    rq->ping_seqnum = seqnum;
	    rq->ping_ident = ident;
	    rq->ping_dh_msg = dh_msg;
	    rq->ping_has_sname = has_sname;
	    if (dcip_data)
		memcpy(&rq->dci_data, dcip_data, 
		       sizeof( struct dhcp_client_info));
	    else
		bzero(&rq->dci_data, sizeof(struct dhcp_client_info));
	}
	return rq;
    }
    rq = (struct dhcp_request_list *)malloc(sizeof (struct dhcp_request_list));
    if (rq == (struct dhcp_request_list*)0)
	return 0;
    rq->cid_ptr = (char *)malloc(cid_length);
    if (rq->cid_ptr == (char*)0) {
	free(rq);
	return 0;
    }
    bcopy(cid_ptr, rq->cid_ptr, cid_length);
    rq->cid_length = cid_length;
    rq->dh_eaddr = (EtherAddr *)malloc(sizeof (EtherAddr));
    bcopy(ead->ea_addr, rq->dh_eaddr->ea_addr, 6);
    rq->dh_ipaddr = ipa;
    if(hsname)
	rq->dh_hsname = strdup(hsname);
    else
	rq->dh_hsname = 0;
    bcopy(reqparams, &rq->dh_reqParams, sizeof(struct getParams));
    rq->dh_reqServer = srv;
    rq->dh_reqLease = lease;
    rq->dh_configPtr = cfg;
    rq->ping_status = ping_stat;
    if (ping_stat == PING_SENT) {
	memcpy(&rq->ping_rp, pingrp, sizeof (struct bootp));
	memcpy(&rq->ping_rq, pingrq, sizeof (struct bootp));
	rq->ping_seqnum = seqnum;
	rq->ping_ident = ident;
	rq->ping_dh_msg = dh_msg;
	rq->ping_has_sname = has_sname;
	if (dcip_data)
	    memcpy(&rq->dci_data, dcip_data, sizeof( struct dhcp_client_info));
	else
	    bzero(&rq->dci_data, sizeof(struct dhcp_client_info));
    }
    
    rq->dh_prev = 0;
    rq->dh_next = 0;

    if(dhcp_rlist) {
	dhcp_rlist->dh_prev = rq;
	rq->dh_next = dhcp_rlist;
    }
    dhcp_rlist = rq;
    return rq;
}

int
dh0_upd_dhcp_rq_entry(struct dhcp_request_list* rq,
		      struct getParams *reqparams, u_int lease)
{
    bcopy(reqparams, &rq->dh_reqParams, sizeof(struct getParams));
    rq->dh_reqLease = lease;
    return 0;
}

static int
free_dhcp_rq(struct dhcp_request_list *rq)
{
    if(rq->dh_hsname)
	free(rq->dh_hsname);
    free(rq->dh_eaddr);
    free(rq->cid_ptr);
    free(rq);
    return 0;
}

int
dh0_remove_from_dhcp_reqlist(char *cid_ptr, int cid_length)
{
    struct dhcp_request_list *rq, *pq;

    rq = dhcp_rlist;
    if(!rq)
	return 1;
    while(rq) {
	if((cid_length == rq->cid_length) &&
	   (bcmp(rq->cid_ptr, cid_ptr, cid_length) == 0)) {
	    pq = rq->dh_prev;
	    if(pq) {
		pq->dh_next = rq->dh_next;
		if(rq->dh_next)
		    rq->dh_next->dh_prev = pq;
	    }
	    else {
		dhcp_rlist = rq->dh_next;
		if(dhcp_rlist)
		    dhcp_rlist->dh_prev = 0;
	    }
	    free_dhcp_rq(rq);
	    return 0;
	}
	rq = rq->dh_next;
    }
    return 0;	/* XXX error condition ??? */
}

struct dhcp_request_list *
dh0_find_dhcp_rqlist_entry(char *cid_ptr, int cid_length)
{
    struct dhcp_request_list *rq;

    rq = dhcp_rlist;
    if(!rq)
	return 0;
    while(rq) {
	/*syslog(LOG_DEBUG,"Request list entry:cid = %s",rq->cid_ptr);*/
        if (cid_length == rq->cid_length) {
	    if(bcmp(rq->cid_ptr, cid_ptr, cid_length) == 0)
		return rq;
	}
	rq = rq->dh_next;
    }
    return 0;
}

struct dhcp_request_list *
dh0_find_dhcp_rqlist_entry_by_ipaddr(u_long ipaddr)
{
    struct dhcp_request_list *rq;

    rq = dhcp_rlist;
    if(!rq)
	return 0;
    while(rq) {
	if (ipaddr == rq->dh_ipaddr)
	    return rq;
	rq = rq->dh_next;
    }
    return 0;
}

int
dh0_addto_timelist(char *cid_ptr, int cid_length, EtherAddr *ead, time_t tval,
		   struct dhcp_request_list *dhcp_rq)
{
    struct dhcp_timelist *tmq, *pmq;

    /* check whether there exists an entry to avoid duplicates */
    if (tmq = dh0_find_timelist_entry(cid_ptr,cid_length)) {
	tmq->tm_value = tval;
	if (tmq->tm_dhcp_rq && (tmq->tm_dhcp_rq->ping_status == PING_SENT))
	    ping_number_pending--;
	if (dhcp_rq && (dhcp_rq->ping_status == PING_SENT))
	    ping_number_pending++;
	/* printf("UPD: tq=0x%x, dhcp_rq=0x%x(0x%x), ping_status=%d(%d), number=%d(%d)\n", 
       tmq, tmq->tm_dhcp_rq, dhcp_rq, 
       (tmq->tm_dhcp_rq)?tmq->tm_dhcp_rq->ping_status: -1,
       (dhcp_rq)?dhcp_rq->ping_status: -1, ping_number_pending,
       dh0_timeouts_cnt); */
	tmq->tm_dhcp_rq = dhcp_rq;
	return 0;		/* update existing entry */
    }
    tmq = (struct dhcp_timelist *)malloc(sizeof (struct dhcp_timelist));
    tmq->cid_ptr = (char *)malloc(cid_length);
    if (cid_ptr != 0)
	bcopy(cid_ptr,tmq->cid_ptr,cid_length);
    tmq->cid_length = cid_length;
    tmq->tm_eaddr = (EtherAddr *)malloc(sizeof (EtherAddr));
    if(ead != 0) 
	bcopy(ead->ea_addr, tmq->tm_eaddr->ea_addr, 6);
    tmq->tm_value = tval;
    
    tmq->tm_dhcp_rq = dhcp_rq;
    tmq->tm_prev = 0;
    tmq->tm_next = 0;
    if (dhcp_rq && (dhcp_rq->ping_status == PING_SENT))
        ping_number_pending++;


    if(dhcp_tmlist_last) {
	pmq = dhcp_tmlist_last->tm_prev;
	dhcp_tmlist_last->tm_prev = tmq;
	tmq->tm_next = dhcp_tmlist_last;
	if(pmq) {
	    pmq->tm_next = tmq;
	    tmq->tm_prev = pmq;
	}
	else
	    dhcp_tmlist_first = tmq;
    }
    else {
	dhcp_tmlist_first = tmq;
	dhcp_tmlist_last = tmq;
    }
    dh0_timeouts_cnt++;
    /*printf("ADD: tm=0x%x, dhcp_rq=0x%x, ping_status=%d, number=%d(%d)\n", 
       tmq, dhcp_rq, (dhcp_rq)?dhcp_rq->ping_status: -1, ping_number_pending,
       dh0_timeouts_cnt);*/
    return 0;
}


int
dh0_free_timelist_entry(struct dhcp_timelist *tq)
{
    free(tq->cid_ptr);
    free(tq->tm_eaddr);
    free(tq);
    return 0;
}

int
dh0_remove_from_timelist(char *cid_ptr, int cid_length)
{
    struct dhcp_timelist *tq, *pq;

    tq = dhcp_tmlist_first;
    if(tq == 0)
	return 1;
    while(tq) {
	if((tq->cid_length == cid_length) && 
	   bcmp(tq->cid_ptr, cid_ptr, cid_length) == 0) {
	    pq = tq->tm_prev;
	    if(pq) {
		pq->tm_next = tq->tm_next;
		tq->tm_next->tm_prev = pq;
	    }
	    else {
		dhcp_tmlist_first = tq->tm_next;
		if(dhcp_tmlist_first)
		    dhcp_tmlist_first->tm_prev = 0;
	    }
	    if (tq->tm_dhcp_rq && (tq->tm_dhcp_rq->ping_status == PING_SENT))
		ping_number_pending--;
	    dh0_timeouts_cnt--;
	    /*printf("RM:tm=0x%x, dhcp_rq=0x%x, ping_status=%d, number=%d(%d)\n", 
	      tq, tq->tm_dhcp_rq, (tq->tm_dhcp_rq)?tq->tm_dhcp_rq->ping_status: -1, ping_number_pending, dh0_timeouts_cnt);*/
	    dh0_free_timelist_entry(tq);
	    return 0;
	}
	tq = tq->tm_next;
    }
    return 1;
}

struct dhcp_timelist *
dh0_find_timelist_entry(char *cid_ptr, int cid_length)
{
    struct dhcp_timelist *tq;

    tq = dhcp_tmlist_first;
    if(tq == 0)
	return 0;
    while(tq) {
      if (cid_length == tq->cid_length) {
	if(bcmp(tq->cid_ptr, cid_ptr, cid_length) == 0)
	  return tq;
      }
      tq = tq->tm_next;
    }
    return 0;
}

struct dhcp_timelist *
dh0_pop_timelist_timer(void)
{
    struct dhcp_timelist *tq;

    tq = dhcp_tmlist_first;
    if(!tq)
	return 0;
    dhcp_tmlist_first = dhcp_tmlist_first->tm_next;
    dhcp_tmlist_first->tm_prev = 0;

    dh0_timeouts_cnt--;
    if (tq->tm_dhcp_rq && (tq->tm_dhcp_rq->ping_status == PING_SENT))
        ping_number_pending--;
    /*printf("POP:tm=0x%x, dhcp_rq=0x%x, ping_status=%d, number=%d(%d)\n", 
      tq, tq->tm_dhcp_rq, (tq->tm_dhcp_rq)?tq->tm_dhcp_rq->ping_status: -1, ping_number_pending, dh0_timeouts_cnt);*/

    return tq;
}

int
dh0_set_timeval(struct timeval *tmo, long tval)
{
    tmo->tv_sec = tval;
    tmo->tv_usec = 0;
    return 0;
}

long
dh0_update_and_get_next_timeout(time_t timenow)
{
    long time_left;

    if(dh0_timeouts_cnt == 1) {
	time_left = dhcp_tmlist_last->tm_value - timenow;
	return time_left;
    }
    time_left = dhcp_tmlist_first->tm_value - timenow;
    if(time_left < 0)
	return 1;
    return time_left;
}

