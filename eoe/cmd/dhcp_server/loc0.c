#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <syslog.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <limits.h>
#include <ndbm.h>
#ifdef EDHCP
#include "ddns/dhcp_dnsupd.h"
extern int dhcp_dnsupd_on;
#endif

#include "dhcpdefs.h"

#ifdef EDHCP_LDAP
/* #include "ldap_dhcp.h"  */
#endif

extern EtherAddr *ether_aton(char *);
extern int alt_naming_flag;
extern int ether_ntohost(char *, struct ether_addr *);
extern int sys0_removeFromHostsFile(u_long);
extern int sys0_removeFromEthersFile(EtherAddr *);
extern	u_long		dh0_lease_time(u_long, dhcpConfig *);

extern int efputs(char *, FILE*);
extern int erename(char *, char *);
extern int debug;
extern DBM *log_dbmopen(DBM *);
extern void mk_str(int cid_flag, char *cid_ptr, int cid_length, char *str);

char	*EtherIpFile =     "/var/dhcp/etherToIP";
char	*tmpEtherIpFile =  "/var/dhcp/etherToIP.tmp";
char    *lockEtherIpFile = "/var/dhcp/etherToIP.lock";
int	dblock;			/* fd for dblock */

char *script_file_to_run;
state_change_t script_state_changes[MAXSTATECHANGES];
int script_state_change_index;
extern int script_state_change(char *cid_ptr, int cid_length, EtherAddr *eth, 
			       char *ipc, char *hna, unsigned long lease, 
			       state_change_op);
#ifdef EDHCP
extern int ping_timeout;
extern int ping_dns;
#endif
extern u_long retry_stolen_timeout;
/****************************************************************************
 * dhETHER_IP file related functions: This file is used for the  purpose of *
 * keeping a dhcp internal mapping for each of the dhcp client systems. The *
 * format of the file is:                                                   *
 * ethernet_address  ip_address(dot notation)  hostname   Lease(seconds)    *
 ****************************************************************************
 * changes made on 6/19/95 in format
 * ethernet_address  ip_address(dot notation)  hostname   EndLease(seconds) *
 * EndLease of 0 implies this is only offered
 */


/*
 * Take a line from the dhETHER_IP file and break it up into its components
 * of Ethernet Address, IP Address, and hostname, and end_lease.
 */
int
parse_ether_entry_l(char *buf, char **beh, char **bipc, char **bhna,char **blease)
{
    char *p;

    p = buf;
    *beh = strtok(p, " \t");
    if (! *beh) {
	return 1;
    }
    p = (char *)0;
    *bipc = strtok(p, " \t");
    if (! *bipc) {
	return 1;
    }
    *bhna = strtok(p, " \t");
    if (! *bhna) {
	return 1;
    }
    *blease = strtok(p, " \t");
    if (! *blease) {
	return 1;
    }

    return 0;
}

u_long
loc0_get_ipaddr_from_cid(EtherAddr *eth, char *cid_ptr, int cid_length, DBM *db){
    
    char *buf;
    char sbuf[256];
    char buffer[256];
    
    char *eh, *ipc, *lend, *hna;
    u_long ipcl;
    struct hostent *hp;

    bzero(buffer, 256);
    buf = getRecByCid(sbuf, cid_ptr, cid_length, db);
    if (buf != NULL){
	if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend))
	    return 0;
	ipcl = inet_addr(ipc);
	if(ipcl == INADDR_NONE)
	    return 0;
	return ipcl;
    }
    
    if(ether_ntohost(buffer, eth) == 0) {
	if(hp = gethostbyname(buffer)) {
	    ipcl = *(u_long *)(hp->h_addr_list[0]);
	    /* verify that this address is not assigned to some other cid */
	    if (getRecByIpAddr(0, &ipcl, db)) {
		/* if this returns a value it must be for some other cid */
		ipcl = 0;
	    }
	    return ipcl;
	}
    }
    return 0;
}

  

  
int
loc0_is_ipaddress_for_cid(EtherAddr *eth, u_long ipaddr,char *cid_ptr, int cid_length, DBM *db){
 
  u_long ipaddr_cid;

  ipaddr_cid = loc0_get_ipaddr_from_cid(eth, cid_ptr, cid_length, db);
  if (ipaddr_cid == 0)       /* Not Found */
    return 1;
  if(ipaddr == ipaddr_cid)
    return 0;
  return 2;	/* Found, but has a different mapping */
}



int
loc0_remove_entry(EtherAddr *eth, int expire_it, char *cid_ptr, int cid_length, DBM *db)
{
  /* the expire_it parameter also decides whether to remove the
   * entry or not
   * 1 - Expire and Reuse
   * 2 - Release and Reuse
   * 0 - Remove only if lease is zero
   * 3 - Remove in any case
   */
  char *buf;
  char *eh, *ipc, *hna, *lend;
  char sbuf[256];
  u_long ipcl;
  long lease;
  /* EtherAddr *ehs;*/

  LOCKDHCPDB(dblock);
 
  buf = getRecByCid(sbuf, cid_ptr, cid_length, db);

  if (buf == NULL) {
      UNLOCKDHCPDB(dblock);
      return 0;
  }

  if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend)) {
      UNLOCKDHCPDB(dblock);
      return 1;
  }
  
  ipcl = inet_addr(ipc);
  if (ipcl == INADDR_NONE) {
      UNLOCKDHCPDB(dblock);
      return 1;
  }

  /*Possibility in case of cid that the ethernet card is moved and m/c retains
    it's cid;so then cid and mac.addr will differ in the dbase on server. 
    
    ehs = ether_aton(eh);
    if (bcmp(ehs->ea_addr, eth->ea_addr, 6)){
    syslog(LOG_DEBUG, "Key and MacAddr mismatch");
    return 1;
    }  else
    */
  lease = atol(lend);
  if (lease == STATIC_LEASE) {
#ifdef EDHCP_LDAP
    /* static leases are available as reservations in LDAP  */
	  deleteLeaseLdap(cid_ptr, cid_length, ipcl);
#endif
    UNLOCKDHCPDB(dblock);	/* static lease don't remove */
    return 0;
  }
  if (expire_it) {
    if (script_file_to_run)
      script_state_change(cid_ptr, cid_length, eth, ipc, hna, 0, STATE_DELETE);
#ifdef EDHCP_LDAP
    if  (lease != 0) {
	deleteLeaseLdap(cid_ptr, cid_length, ipcl);
    }
#endif
    deleteRec(cid_ptr, cid_length, eth, ipcl, hna, db);
    if (debug >= 2) {
      switch (expire_it) {
      case 1:
	syslog(LOG_DEBUG, "Expire: %s\t%s\t%s\t%s\n",eh,ipc,hna,lend);
	break;
      case 2:
	syslog(LOG_DEBUG, "Release: %s",eh,ipc,hna,lend);
	break;
      }
    }
    UNLOCKDHCPDB(dblock);
    return 0;
    /* expire_it = 3 when we remove an entry just before adding it
     * and in DHCPDECLINE */
  }
  else {
    if (atoi(lend) != 0) {
      if (debug >= 2)
	syslog(LOG_DEBUG,"Ether Rm -> (%s) Lease not zero *NOT REMOVED*", buf);
    }
    else{
      if (script_file_to_run)
	script_state_change(cid_ptr, cid_length, eth, ipc, hna, 
			    0, STATE_DELETE);
#ifdef EDHCP_LDAP
      if  (lease != 0) {
	  deleteLeaseLdap(cid_ptr, cid_length, ipcl);
      }
#endif
      deleteRec(cid_ptr, cid_length, eth, ipcl, hna, db);
      if (debug >= 2)
	syslog(LOG_DEBUG, "Ether Rm -> (%s)", buf);
    }
  }
  UNLOCKDHCPDB(dblock);
  return 0;
}

char *
loc0_get_hostname_from_cid(char *cid_ptr, int cid_length, EtherAddr *eth, DBM *db)
{
  char *buf;
  char *eh, *ipc, *hna, *lend;
  EtherAddr *ehs;
  char sbuf[1024];
  char buffer[1024];
  
  bzero(buffer, 1024);
  buf = getRecByCid(sbuf, cid_ptr, cid_length, db);
  if (buf != NULL){
      if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend)) {
	  return (char *)0;
      }
      ehs = ether_aton(eh);
      if(!ehs) {
	  return (char *)0;
      }
      if(bcmp(ehs->ea_addr, eth->ea_addr, 6) == 0) {
	  return strdup(hna);
      }
      if(ether_ntohost(buffer, eth) == 0)   /*Poss. in case of cid that the
					    client has a new interface card*/
	  return strdup(buffer);
  }
  
  if(ether_ntohost(buffer, eth) == 0) {  /*in case of any static entries*/ 
      /* make sure that this hostname is not assigned to some other cid */
      if (getRecByHostName(0, buffer, db)) {
	  return (char *)0;
      }
      return strdup(buffer);
  }
  return (char *)0;
}


int
loc0_is_hostname_assigned(char *nm, DBM *db)
{
  char *buf;
  char *eh, *ipc, *hna, *lend;
  char sbuf[256];
  
  if(!alt_naming_flag && gethostbyname(nm))
    return 1;

  buf = getRecByHostName(sbuf, nm, db);
  if (buf == NULL)
    return 0;
  
  if (parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend) == 0) {
    if(strcmp(nm, hna) == 0) {
      return 1;
    }
  }
  return 0;
}

int
loc0_is_ipaddress_assigned(u_long ipa, DBM *db)
{
  char *buf;
#ifdef EDHCP
  /* if ping_dns is on it means that if the hostaddress is in DNS
   * it is assumed to be taken */
  if(!alt_naming_flag && ping_dns && 
     gethostbyaddr(&ipa, sizeof(ipa), AF_INET))
#else
  if(!alt_naming_flag && gethostbyaddr(&ipa, sizeof(ipa), AF_INET))
#endif
    return 1;
  
  buf = getRecByIpAddr(0, &ipa, db);
  if (buf == NULL)
    return 0;  /*IP addr. not assigned */
  else 
    return 1;
}


/*
 * if we are going to have an expired loose atthis time or soon
 * then we make the lease 0. This is used when an offer is being sent
 * to a client which had an existing but expired lease
 */
int
loc0_zero_expired_lease(EtherAddr *eth, char *cid_ptr, int cid_length, DBM *db)
{
    char *buf;
    char *eh, *ipc, *hna;
    char *lend;
    char *p;
    int ret = 2;		/* not found */
    char sbuf[256];
    char abuf[256];
    EtherAddr *ehs;
    long lease_or_inf;
    time_t lease_end, curr_time;
    
    
    LOCKDHCPDB(dblock);
    buf = getRecByCid(abuf, cid_ptr, cid_length,db);
    if (buf == NULL) {
	UNLOCKDHCPDB(dblock);
	return 1;
    }
    strcpy(sbuf, abuf);
    if(parse_ether_entry_l(abuf, &eh, &ipc, &hna, &lend)){
	UNLOCKDHCPDB(dblock);
	return 1;
    }
    lease_end = atol(lend);
    if (time(&curr_time) == (time_t)-1)
	return 1;		/* error */
    if ( (lease_end <= 0) || ((long)lease_end == INFINITE_LEASE) || 
	 ((long)lease_end == STATIC_LEASE) )
	return 0;			/* no need to change */
#ifdef EDHCP
    if ( (lease_end > (curr_time + ping_timeout + DHCPOFFER_TIMEOUT)) )
	return 0;
#else
    if ( (lease_end > (curr_time + DHCPOFFER_TIMEOUT)) )
	return 0;
#endif
    lease_or_inf = 0;
    ehs = ether_aton(eh);
    if (!(bcmp(ehs->ea_addr, eth->ea_addr, 6))){
	ret = 0;
	p = &sbuf[lend-buf];
	sprintf(p, "%d\0", lease_or_inf);
    }
    if (script_file_to_run) 
	script_state_change(cid_ptr, cid_length, ehs, ipc, hna, 
			    lease_or_inf, STATE_LEASE);
    putRecByCid(cid_ptr, cid_length, db, sbuf);
    UNLOCKDHCPDB(dblock);
    
    return ret;
}

long
loc0_get_lease(u_long ipa, DBM *db)
{
  char *buf;
  char sbuf[256];
  char *eh, *ipc, *hna, *lend;

  buf = getRecByIpAddr(0, &ipa, db);
  if (buf == NULL)
    return 0;  /*IP addr. not assigned */
  else {
    strcpy(sbuf, buf);
  
    if (parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend))
      return 0;
    else {
      return (atol(lend));
    }
  }
}
    

long
loc0_get_leasebycid(void* cid_ptr, int cid_length, DBM *db)
{
    char *buf;
    char sbuf[256];
    char *eh, *ipc, *hna, *lend;
    
    buf = getRecByCid(sbuf, cid_ptr, cid_length, db);
    if (buf == NULL)
	return 0;  /* not assigned */
    else {
	if (parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend))
	    return 0;
	else {
	    return (atol(lend));
	}
    }
}


/* functions to handle lease through the "etherToIP File" */

int
loc0_create_new_entry_l(char *cid_ptr, int cid_length, EtherAddr *eth,
			u_long ipaddr, char *hname, time_t lease_end,
			DBM *db)
{
  char *ebuf;
  char *ipchar;
  char tmp1[256];
  char tmp2[256];
  struct in_addr ipn;
  
  ebuf = ether_ntoa(eth);
  ipn.s_addr = ipaddr;
  ipchar = inet_ntoa(ipn);

  if (sprintf(tmp1,"%s\t%s\t%s\t%d\0", ebuf, ipchar, hname, lease_end)<0){
    return 1;
  }
  strcpy(tmp2, tmp1);

  if (script_file_to_run) 
    script_state_change(cid_ptr, cid_length, eth, ipchar, hname, 
			lease_end, STATE_CREATE);

  LOCKDHCPDB(dblock);
#ifdef EDHCP_LDAP
  if  (lease_end != 0) { 
      addLeaseLdap(cid_ptr, cid_length, tmp2);
  }
#endif
  putRecByCid(cid_ptr, cid_length, db, tmp2);
  putRecByEtherAddr(eth, db, cid_ptr, cid_length);
  putRecByHostName(hname, db, cid_ptr, cid_length);
  putRecByIpAddr(ipaddr, db, cid_ptr, cid_length);
  UNLOCKDHCPDB(dblock);
  return 0;
}


/* update the lease time
   returns 0 if ok, 2 if not found
   1 other error
   */
int
loc0_update_lease(EtherAddr *eth, char *cid_ptr, int cid_length, time_t lease_start,
		  u_long lease, DBM *db)
{
  char *buf;
  char *eh, *ipc, *hna;
  char *lend;
  char *p;
  int ret = 2;		/* not found */
  char sbuf[256];
  char abuf[256];
  EtherAddr *ehs;
  long lease_or_inf;

  
  LOCKDHCPDB(dblock);
  buf = getRecByCid(abuf, cid_ptr, cid_length,db);
  if (buf == NULL) {
    UNLOCKDHCPDB(dblock);
    return 1;
  }
  strcpy(sbuf, abuf);
  if(parse_ether_entry_l(abuf, &eh, &ipc, &hna, &lend)){
    UNLOCKDHCPDB(dblock);
    return 1;
  }
  if (atol(lend) == STATIC_LEASE) 
      lease_or_inf = STATIC_LEASE;
  else {
      if ((atol(lend) == -1) || ((long)lease == -1) || (lease == INFINITE_LEASE))
	  lease_or_inf = -1;
      else
	  lease_or_inf = (long)(lease_start+lease);
  }
  ehs = ether_aton(eh);
  if (!(bcmp(ehs->ea_addr, eth->ea_addr, 6))){
      ret = 0;	/* ok */
      p = &sbuf[lend-buf];
      sprintf(p, "%d\0", lease_or_inf);
  }
  if (script_file_to_run) 
    script_state_change(cid_ptr, cid_length, ehs, ipc, hna, 
			lease_or_inf, STATE_LEASE);
#ifdef EDHCP_LDAP
  if  (lease_or_inf > 0) { 
      updateLeaseLdap(cid_ptr, cid_length, ipc, lease_or_inf);
  }
#endif
  putRecByCid(cid_ptr, cid_length, db, sbuf);
  UNLOCKDHCPDB(dblock);

  return ret;
}


/* find oldest expired ethernet & ipaddress pair
   returns 0 if found, 1 if error, or 2 if not found
   */
   
int
loc0_scan_find_lru_entry(dhcpConfig *cfgPtr, EtherAddr *eth, u_long *ipa, 
			 char **hname, char *lru_cid, int *lru_cid_len, 
			 DBM *db)
{
  datum key;
  datum data;
  char *eh, *ipc, *hna, *hlease;
  time_t lease_end;
  EtherAddr *ehs;
  time_t curr_time;
  time_t oldest_lease = LONG_MAX;
  char sbuf[256];
  u_long ipatmp;
  time_t stolen_lease_time = -2;

  if (time(&curr_time) == (time_t)-1)
    return 1;		/* error */
  
  
  for (key = dbm_firstkey(db); key.dptr != NULL; key = dbm_nextkey(db)){
    if (*(char*)key.dptr != '3') /*Acessing the dbase with cid as the key to the hash 
			   function*/
      continue;
    bzero(sbuf, sizeof(sbuf));
    data = dbm_fetch(db, key);
    if (data.dptr)
	strcpy(sbuf, data.dptr);
    else {
	mk_str(1, key.dptr, key.dsize, sbuf);
	syslog(LOG_ERR, "No data in DB for key %s", sbuf);
	continue;
    }
    if(parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &hlease)) {
      return 1; /*error*/
    }
    if (strncmp(hna, "STOLEN-", 7) == 0) {
	if (sscanf(hna, "STOLEN-%d", &stolen_lease_time) != 1)
	    continue;
	if ( (curr_time - stolen_lease_time) > retry_stolen_timeout) {
	    lease_end = stolen_lease_time;
	    hna[0] = NULL;
#ifdef DEBUG
	    syslog(LOG_DEBUG, "Recycling LEASE %s", data.dptr);
#endif
	}
	else 
	    continue;
    }
    else
	lease_end = atol(hlease);
    if ((lease_end <= curr_time) && (lease_end > 0)) { /* expired entry */
      ipatmp = inet_addr(ipc);
      if(ipatmp == INADDR_NONE)
	  return 1;
      if (cf0_get_config(ipatmp, 0) != cfgPtr) {
	  continue;		/* make sure its part of this config */
      }
	/* -1 is static lease, -2 is stolen address  -3 is static */
      if (lease_end < oldest_lease) {
	ehs = ether_aton(eh);
	if(!ehs)
	  return 1;
	bcopy(ehs, eth, sizeof(EtherAddr));
	*ipa = ipatmp;
	if (*hname)
	    free(*hname);
       	*hname = strdup(hna);
	*lru_cid_len = key.dsize -1;
	if (*lru_cid_len > (MAXCIDLEN-1)) {
	  syslog(LOG_ERR, "Cid too long len = %d", *lru_cid_len);
	  return 1;
	}
	bcopy((char*)key.dptr+1, lru_cid, *lru_cid_len);
	oldest_lease = lease_end;
      }
    }
  }
  if (oldest_lease == LONG_MAX)
    return 2;
  else
    return 0;
}


/* updates entry if found otherwise creates a new entry
   */

int
loc0_create_update_entry(char *cid_ptr, int cid_length, EtherAddr *eth,
			 u_long ipaddr,char *hname,time_t lease_start,
			 u_long lease, DBM *db)
{
  int ret;
  char *buf;
  long lease_or_inf;

  ret = loc0_remove_entry(eth, 3, cid_ptr, cid_length, db);
  if (ret == 0){		/* static leases are never removed */
    if  (loc0_get_leasebycid(cid_ptr, cid_length, db) == STATIC_LEASE)
	lease_or_inf = STATIC_LEASE;
    else {
	if (lease == (u_long)INFINITE_LEASE) 
	    lease_or_inf = INFINITE_LEASE;
	else
	    lease_or_inf = (long)(lease_start+lease);
    }
    if (debug >= 2){
      buf = getRecByCid(0, cid_ptr, cid_length, db);
      if ((buf!=NULL) && (lease_or_inf != STATIC_LEASE))
	syslog(LOG_DEBUG,"Trying to create new entry with used addr :%s\n",
	       buf);
    }
    ret = loc0_create_new_entry_l(cid_ptr, cid_length, eth, ipaddr, hname,
				  lease_or_inf, db);
  }
  return ret;
}

int
loc0_expire_entry(EtherAddr *ethl, char *cid_ptr, int cid_length, u_long ipa,
		  DBM *db)
{
    loc0_remove_entry(ethl, 1, cid_ptr, cid_length, db); /* Expire it */
    sys0_removeFromHostsFile(ipa);  /*pt to check for pre-existence in /hosts*/
    sys0_removeFromEthersFile(ethl);/*and /ethers file (to do with extra 
				      fields in EtherIpFile)*/
#ifdef EDHCP
    if ( (dhcp_dnsupd_on == 1) && (0 == alt_naming_flag) ) {
	dhcpConfig *cfgPtr;
	cfgPtr = cf0_get_config(ipa, 0);
	/* this should check if the 
	 record was added before deleting */
	dhcp_dnsdel(ipa, cfgPtr->ddns_conf);
    }
#endif
    return 0;
}


/* return 1 when duplicate is found or error */
int
loc0_is_etherToIP_duplicate(char *cid_ptr, int cid_length, EtherAddr *ethl,
			    u_long ipa, char *hsname, DBM *db)
{
  char *buf;
  char *eh, *ipc, *hna, *lend;
  struct in_addr ipn;
  int ret = 0;
  u_long ipcl;
  EtherAddr *ehs;
  char sbuf[256];
  
  buf = getRecByCid(sbuf, cid_ptr, cid_length, db);
  if (buf != NULL){
    if (parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend)) {
	syslog(LOG_ERR, "Parsing database entry error(dup): %s", buf);
	return ret;
    }
    if (atol(lend) == STATIC_LEASE) {	/* static lease */
	return ret;
    }
    ipcl = inet_addr(ipc);
    if ((ipcl != INADDR_NONE) && (ipcl == ipa)) {     
      if (debug) {
	ipn.s_addr = ipa;
	syslog(LOG_DEBUG, "Duplicate IP %s(%s) %s(%s) %s(%s) %s\n",
	       eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
	       hna, hsname, lend);
      }
      ret = 1;
    }
    ehs = ether_aton(eh);
    if ((ehs != NULL) && (bcmp(ehs->ea_addr, ethl->ea_addr, 6) == 0)) {
      if (debug) {
	ipn.s_addr = ipa;
	syslog(LOG_DEBUG, "Duplicate Ether %s(%s) %s(%s) %s(%s) %s\n",
	       eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
	       hna, hsname, lend);
      }
      ret = 1;
    }
    if (strcmp(hna, hsname) == 0) {
      if (debug) {
	ipn.s_addr = ipa;
	syslog(LOG_DEBUG, "Duplicate HostName %s(%s) %s(%s) %s(%s) %s\n",
	       eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
	       hna, hsname, lend);
      }
      ret = 1;
    }
  }
  return ret;
}

/* return 1 when  error, 0 otherwise */
int
loc0_is_etherToIP_inconsistent(char *cid_ptr, int cid_length, EtherAddr *ethl,
			       u_long ipa, char *hsname, DBM *db)
{
    char *buf;
    char *eh, *ipc, *hna, *lend;
    struct in_addr ipn;
    u_long ipcl;
    EtherAddr *ehs;
    int hnalen = 0;
    int hsnamelen = 0;
    int complen;
    char sbuf[256];
    datum key, data;
    char keybuf[256];
    char str_in[MAXCIDLEN];
    char str_out[MAXCIDLEN];
    int i;

    hsnamelen = strlen(hsname);
    for (i = 0; i < 4; i++) {
	if (i == 0) {		/* cid */
	    buf = getRecByCid(sbuf, cid_ptr, cid_length, db);
	    if (buf == NULL)		/* no record for this cid */
		continue;
	}
	else {
	    if (i == 1) {
		sprintf(keybuf, "1%s", hsname);
		key.dsize = strlen(keybuf);
	    }
	    else if (i == 2) {
		keybuf[0] = '2';
		memcpy (&keybuf[1], &ipa, sizeof(u_long));
		key.dsize = 1 + sizeof(u_long);
	    }
	    else {
		keybuf[0] = '0';
		memcpy (&keybuf[1], ethl, sizeof(EtherAddr));
		key.dsize = 1 + sizeof(EtherAddr);
	    }
	    key.dptr = keybuf;
	    data = dbm_fetch(db, key);
	    if (data.dptr != NULL) {
		if ((data.dsize != cid_length) ||
		    (memcmp(data.dptr, cid_ptr, data.dsize) != 0)) {
		    if (debug) {
			ipn.s_addr = ipa;
			mk_str(1, cid_ptr, cid_length, str_in);
			mk_str(1, data.dptr, data.dsize, str_out);
			syslog(LOG_DEBUG, "Inconsistent RevMap i=%d %s %s %s %s(%s)\n",
			       i, ether_ntoa(ethl), inet_ntoa(ipn), hsname,
			       str_in, str_out);
		    }
		    return 1;
		}
	    }
	    continue;
	}
	
	if (parse_ether_entry_l(sbuf, &eh, &ipc, &hna, &lend)) {
	    syslog(LOG_ERR, "Parsing database entry error(incons): %s", buf);
	    return 0;
	}
	hnalen = strlen(hna);
	complen = (hnalen < hsnamelen) ? hnalen : hsnamelen;
	ipcl = inet_addr(ipc);
	ehs = ether_aton(eh);
	if ((ipcl != INADDR_NONE) && (ipcl == ipa)) {	/* match */
	    if ((ehs == NULL) ||(bcmp(ehs->ea_addr, ethl->ea_addr, 6) != 0) ||
		(strncmp(hna, hsname, complen) != 0)) { /* error */
		if (debug) {
		    ipn.s_addr = ipa;
		    syslog(LOG_DEBUG, "Inconsistent IP %s(%s) %s(%s) %s(%s) %s\n",
			   eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
			   hna, hsname, lend);
		}
		return 1;
	    }
	}
	else if ((ehs != NULL) && (bcmp(ehs->ea_addr, ethl->ea_addr, 6) == 0)) {
	    if (debug) {
		ipn.s_addr = ipa;
		syslog(LOG_DEBUG, "Inconsistent Ether %s(%s) %s(%s) %s(%s) %s\n",
		       eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
		       hna, hsname, lend);
	    }
	    return 1;
	}
	else if (strncmp(hna, hsname, complen) == 0) {
	    if (debug) {
		ipn.s_addr = ipa;
		syslog(LOG_DEBUG, "Inconsistent HostName %s(%s) %s(%s) %s(%s) %s\n",
		       eh, ether_ntoa(ethl), ipc, inet_ntoa(ipn),
		       hna, hsname, lend);
	    }
	    return 1;
	}
    }
    return 0;
}

    
/* 
 *   returns 0 if no error 
 */
int 
script_state_change(char *cid_ptr, int cid_length, EtherAddr *eth, 
		    char *ipc, char *hna, unsigned long lease, 
		    state_change_op op)
{

    state_change_t *p_stch;
    int len;

    if (script_state_change_index >= MAXSTATECHANGES) {
	syslog(LOG_ERR, "More than %d state changes ?", 
	       script_state_change_index);
	return 1;
    }

    p_stch = &script_state_changes[script_state_change_index];

    len = (cid_length > MAXCIDLEN) ? MAXCIDLEN : cid_length;
    p_stch->cid_length = len;
    memcpy(p_stch->cid, cid_ptr, len);
    memcpy(&p_stch->mac, eth, sizeof(EtherAddr));
    strcpy(p_stch->ipc, ipc);
    strcpy(p_stch->hostname, hna);
    p_stch->lease = lease;
    p_stch->op = op;
    script_state_change_index++;
    return 0;
}

void mk_str(int cid_flag, char *cid_ptr, int cid_length, char *str){
    int i = 0;
    int k = 0;


    if (cid_flag){
      sprintf(&str[k], "%1x;",(u_int)cid_ptr[i]); /*printing just the htype)*/
      if ((u_int)cid_ptr[i] < 16)
	k += 2;
      else
	k += 3;
      i++;
    }
    else{
      sprintf(&str[k], "%1x;", 0);/*Since there is no h/w type provided using 
				    type=0 which basically means that the cid is 
				    not necessarily a chaddr*/
      k += 2;
    }
    for(; i < (cid_length - 1); i++){ 
	sprintf(&str[k], "%x:",(u_int)cid_ptr[i]);
	if ((u_int)cid_ptr[i] < 16)
	    k += 2;
	else
	    k += 3;
    }
    sprintf(&str[k], "%x",(u_int)cid_ptr[i]);
}

/*
 * the next two routines are used essentially by LDAP
 * ther are supposed to behave similar to ether_ntoa and ether_aton
 */
void mk_cid(char *cidstr, char *cid_ptr, int *cid_len)
{
    char *byte;
    char buf[MAXCIDLEN];
    int i;

    strcpy(buf, cidstr);

    for (i = 0, byte = strtok(buf,":"); byte; byte = strtok(0, ":")) {
	*cid_ptr = (char) strtol(byte, 0, 16);
	cid_ptr++;
	i++;
    }
    *cid_len = i;
}

void mk_cidstr(char *cid_ptr, int cid_len, char *str)
{
    int i, k;
    for(i = 0, k = 0; i < (cid_len - 1); i++){ 
	sprintf(&str[k], "%x:",(u_int)cid_ptr[i]);
	if ((u_int)cid_ptr[i] < 16)
	    k += 2;
	else
	    k += 3;
    }
    sprintf(&str[k], "%x",(u_int)cid_ptr[i]);
}
