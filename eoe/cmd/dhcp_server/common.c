#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <sys/sysmacros.h>
#include <ctype.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <ndbm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "dhcp.h"
#include "dhcpdefs.h"
#include "dhcp_types.h"
#include "dhcp_optvals.h"

char	*RealHostFile =    "/etc/hosts";
char	*tmpHostFile =     "/etc/hosts.tmp";
char	*RealEtherFile =   "/etc/ethers";
char	*tmpEtherFile =    "/etc/ethers.tmp";

char addl_options_to_send[128]; /* these are options specified in the
				       * options file
				       */
int addl_options_len = 0;

extern iaddr_t	myhostaddr;
extern struct netaddr *np_recv;
extern int ifcount;
extern int ProclaimServer;
extern int always_return_netmask;
rfc1533opts *rfc1533ptr = 0;

extern int	sys0_removeFromHostsFile(u_long);
extern int	sys0_removeFromEthersFile(EtherAddr *);
extern int	ether_line(char *, struct ether_addr *, char *);
extern int	efputs(char*, FILE*);
extern int	erename(char*, char*);
extern void	setparams(struct getParams*, int, u_char*);

char *  alt_hosts_file  = (char *)0;
char *  alt_ethers_file = (char *)0;
int	dont_update_hosts = 0;
int	dont_update_ethers = 0;

#define LINE_SIZE 128

#define CONFIG_DIR  "/etc/config/"

FILE *
log_fopen(char *file, char *options)
{
	FILE *fp;

	fp = fopen(file, options);
	if (! fp) {
		syslog(LOG_ERR, "fopen failed for file %s: %m", file);
	}
	return fp;
}

DBM *
log_dbmopen(char *file){
  DBM *db;
  db = dbm_open(file, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IROTH);
  if (db==NULL)
    syslog(LOG_ERR, "dbm_open failed for file %s: %m", file);
  return db;
}

void
log_dbmclose(DBM *db){
  dbm_close(db);
  
}

int
dh0_get_dhcp_msg_type(u_char *dhbuf)
{
    u_char *p, *p_end;
    int len;
    int msg_type = -1;

    p = dhbuf;
    p_end = p + DHCP_OPTS_SIZE;

    while( (p < p_end) && (*p != END_TAG) ) {
	switch(*p++) {
	    case PAD_TAG:
		continue;
	    case DHCP_MSG_TAG:
		len = *p++;
		if(len == 1)
		    msg_type = *p++;
		return msg_type;
	    case SGI_VENDOR_TAG:
		len = *p++;
		continue;
	    default:
		len = *p++;
		p += len;
	}
    }
    return msg_type;
}

/*  dhcp_client_info added to support Dynmaic DNS and vendor options
 * we should actually collect most of the parameters passed and place
 * them into dhcp_client_info
 */
int
dh0_decode_dhcp_client_packet(u_char dhbuf[], struct getParams *reqParams,
			      u_long *ipaddr,
			      u_int *lease, u_long *server, char **req_hsname,
			      char **cid_ptr, int *cid_flag_ptr, 
			      int *cid_length, int *sgi_resolv_name,
			      struct dhcp_client_info* dcip_data)
{
  char nbuf[MAXHOSTNAMELEN+1];
  u_char *p, *p_end;
  int len;
  int msg_type = -1;
  int hostname_tag_flag = 0;
  int encaps_options_len = 0; /* distinguish between client fqdn and 
			       * sgi hostname which have the same option code 
			       */

  bzero(reqParams, sizeof(struct getParams));
  if (addl_options_len) {
    setparams(reqParams, addl_options_len, (u_char*)addl_options_to_send);
  }
  *ipaddr = *server = 0;
  *lease = 0;
  p = dhbuf;
  p_end = p + DHCP_OPTS_SIZE;
  
  while( (p < p_end) && (*p != END_TAG) ) {
    switch(*p++) {
    case PAD_TAG:
      continue;
    case DHCP_MSG_TAG:
      len = *p++;
      if(len == 1)
	msg_type = *p++;
      else
	p += len;
      if( (msg_type < DHCPDISCOVER) || (msg_type > DHCPPOLL) )
	msg_type = -1;
      continue;
    case DHCP_PARAM_REQ_TAG:
      len = *p++;
      setparams(reqParams, len, p);
      p += len;
      continue;
    case IPADDR_TAG:
      len = *p++;
      if(len == 4) {
	bcopy(p, ipaddr, 4);
	p += len;
      }
      continue;
    case IP_LEASE_TIME_TAG:
      len = *p++;
      if(len == 4) {
	bcopy(p, lease, 4);
	p += len;
      }
      continue;
    case DHCP_SERVER_TAG:
      len = *p++;
      if(len == 4) {
	bcopy(p, server, 4);
	p += len;
      }
      continue;
    case SGI_VENDOR_TAG:
      len = *p++;
      encaps_options_len = len;
      continue;
    case RESOLVE_HOSTNAME_TAG:
      /*    case CLIENT_FQDN_TAG:*/
      /* Windows NT clients also set this - but our clients use either 
	 HOSTNAME_TAG or RESOLVE_HOSTNAME_TAG. If HOSTNAME_TAG is set or if
         the first byte is null in the value field we ignore this TAG 
	 In the long term we should stop using this hack */
      len = *p++;
      bcopy(p, nbuf, len);
      nbuf[len] = '\0';
      if (encaps_options_len > 0) { /* SGI hostname */
	if ((nbuf[0] != '\0') && (0 == hostname_tag_flag)) {
	    *req_hsname = strdup(nbuf);
	    *sgi_resolv_name = 1;
	}
	encaps_options_len -= len; /* may have client fqdn also */
      } 
      else {
	  /* CLIENT_FQDN_TAG - if its not encapsulated then it must be this */
	  bcopy(p, dcip_data->client_fqdn, len); /* flag, rcode1, rcode2, domain name */
	  reqParams->GET_CLIENT_FQDN = 1;
      }
      p += len;
      continue;
    case DHCP_CLASS_TAG:	/* 60 */
	/* clients send the vendor class using this tag */
	len = *p++;
	bcopy(p, dcip_data->vendor_tag, len);
	p += len;
	continue;
    case HOSTNAME_TAG:
      len = *p++;
      bcopy(p, nbuf, len);
      nbuf[len] = '\0';
      if (*sgi_resolv_name == 1) {
	*sgi_resolv_name = 0;
	free(*req_hsname);
	*req_hsname = (char*)0;
      }
      if (*req_hsname)
	  free(*req_hsname);
      *req_hsname = strdup(nbuf);
      hostname_tag_flag = 1;
      p += len;
      continue;
    case DHCP_CLIENT_ID_TAG:
      len = *p;    /*the Len field includes both the h/w type & CID*/
      p += 1;         
      if (len >= 2){
	*cid_length = len;
	*cid_ptr = (char *)malloc(len * sizeof(char));
	bzero(*cid_ptr, len);
	bcopy(p, *cid_ptr, len); /*copying type + cid*/
	/* *(cid_ptr + len) = NULL; */
	p += len;
	*cid_flag_ptr = 1;
      }
      continue;
    default:
      len = *p++;
      p += len;
    }
  }
  return msg_type;
}

static int
encode_ptr_tag(char **pb, int tag, char *value, int len)
{
    char *p;

    p = *pb;
    if(value && *value) {
	*p++ = tag;
	*p++ = len;
	bcopy(value, p, len);
	p += len;
	*pb = p;
	return 0;
    }
    return 1;
}

static int
encode_value_tag(char **pb, int tag, unsigned value, int len)
{
    char *p;
    unsigned short val;

    p = *pb;
    if(value) {
	*p++ = tag;
	*p++ = len;
	if(len == 1)
	    *p = value;
	else if (len == 2) {
	    val = (unsigned short) value;
	    bcopy(&val, p, len);
	}
	else
	    bcopy(&value, p, len);
	p += len;
	*pb = p;
	return 0;
    }
    return 1;
}

static int
encode_ptrvalue_array_tag(char **pb, int tag, u_long *value)
{
    char *p, *q;
    int i;
    int tlen;

    if(!value)
	return 1;
    p = *pb;

    tlen = 0;
    *p++ = tag;
    q = p++;

    i = 0;
    while(value[i]) {
	bcopy(&value[i], p, 4);
	p += 4;
	tlen += 4;
	i++;
    }
    *q = tlen;
    *pb = p;
    if(i == 0)
	return 1;
    return 0;
}

/*
 * encode vendor options onto the packet sent to the client
 * all applicable vendor options for the vendor class are
 * sent regardsless of whether they were asked for or not
 */
int
dh0_encode_vendor_options(struct getParams *reqP, char *offered_hsname,
			  dhcpConfig *cfg, char **cpp, 
			  struct dhcp_client_info *dcip_data)
{
    int tlen;
    char *p, *q;
    struct dhcp_optval *dop_opt;
    struct vendor_options *vop_opt;
    int len, i;
    char *optlen;

    tlen = 0;
    p = *cpp;
    /* this is retained for compatibility with old clients
     * the clients must be changed to send a sgi tag instead */
    if( (reqP->RESOLVE_HOSTNAME) || (reqP->GET_SDIST_SERVER) ) {
	if(offered_hsname || cfg->p_propel_addr) {
	    *p++ = SGI_VENDOR_TAG;
	    q = p++;
	    if((reqP->GET_SDIST_SERVER) && cfg->p_propel_addr) {
		encode_value_tag(&p, SDIST_SERVER_TAG, cfg->p_propel_addr, 4);
		tlen += (4+2);
	    }
	    if((reqP->RESOLVE_HOSTNAME) && offered_hsname) {
		len = strlen(offered_hsname);
		encode_ptr_tag(&p, RESOLVE_HOSTNAME_TAG, offered_hsname, len);
		tlen += (len+2);
	    }
	}
    }
    if (dcip_data->vendor_tag[0] != NULL) { /* add the applicable 
					       vendor class options */
	/* check if this vendor tag is in cfg */
	vop_opt = cfg->vop_options;
	while (vop_opt) {
	    if (strcmp(vop_opt->cp_vendor_tag, 
		       dcip_data->vendor_tag) == 0) {
		dop_opt = vop_opt->dop_optval;
		while (dop_opt && (dop_opt->i_opt_num != 0)) {
		    for (i = 0; i < MAX_OPT_TYPES; i++) {
			if (dop_opt->dof_fields[i].i_len) {
			    if (tlen == 0) {
				*p++ = SGI_VENDOR_TAG;
				q = p++;
			    }
			    if (i == 0) {
				*p++ = (uint8_t)dop_opt->i_opt_num;
				optlen = p++;
				*optlen = (uint8_t)0;
			    }
			    dop_opt->dof_fields[i].doep_encoder
				(dop_opt->i_opt_num, 
				 dop_opt->dof_fields[i].vp_value, &p,
				 dop_opt->dof_fields[i].i_len);
			    *optlen += (uint8_t)(dop_opt->dof_fields[i].i_len);
			    if (i == 0)
				tlen += (dop_opt->dof_fields[i].i_len + 2);
			    else
				tlen += dop_opt->dof_fields[i].i_len;
			}
			else 
			   break;
		    }
		    dop_opt = dop_opt->dop_next;
		}
	    }
	    vop_opt = vop_opt->vop_next;
	}
    }
    if (tlen)
	*q = tlen;
    *cpp = p;
    return 0;
}
							       
		
/*
 * used dhcp_client_info instead of multiple fields	    
 */
int
dh0_encode_dhcp_server_packet(u_char dhbuf[], int msgtype,
			      struct getParams *reqP, u_long req_lease,
			      char *msg, char *hsname, char *offered_hsname,
			      dhcpConfig *cfg, 
			      struct dhcp_client_info *dcip_data)
{
    char *p, *q;
    int i;
    int tlen, len;
    unsigned short tmpshort;
    u_long mltaddr;
    
    if( (msgtype < DHCPDISCOVER) || (msgtype > DHCPPRPL) )
	return 1;
    if( (msgtype != DHCPNAK) && (msgtype != DHCPPRPL) && (cfg == 0) )
	return 2;

    p = (char *)dhbuf;
    encode_value_tag(&p, DHCP_MSG_TAG, msgtype, 1);

    /* IP Address lease time is a MUST item */
    if( (msgtype == DHCPOFFER) || (msgtype == DHCPACK) ) {
	/* lease value is computed elsewhere. If lease is zero then
	   don't send it (response to DHCPINFORM) */
	if (req_lease)
	    encode_value_tag(&p, IP_LEASE_TIME_TAG, req_lease, 4);
    }

    /* The server identifier is a MUST field */
    mltaddr = MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
    encode_value_tag(&p, DHCP_SERVER_TAG, mltaddr, 4);
    if(msgtype == DHCPNAK) {
	if(msg) {
	    len = strlen(msg);
	    encode_ptr_tag(&p, DHCP_SERVER_MSG, msg, len);
	}
	*p = END_TAG;
	return 0;
    }

    if( (reqP->GET_SUBNETMASK) || (always_return_netmask) ) {
	encode_value_tag(&p, SUBNETMASK_TAG, cfg->p_netmask, 4);
    }
    if(reqP->GET_HOSTNAME) {
	if(hsname && *hsname) {
	    len = strlen(hsname);
	    encode_ptr_tag(&p, HOSTNAME_TAG, hsname, len);
	}
    }
    if(reqP->GET_NISDOMAIN) {
	if(cfg->p_nisdomain && *(cfg->p_nisdomain)) {
	    len = strlen(cfg->p_nisdomain);
	    encode_ptr_tag(&p, NISDOMAIN_NAME_TAG, cfg->p_nisdomain, len);
	}
    }
    if(reqP->GET_DNSDOMAIN) {
	if(cfg->p_dnsdomain && *(cfg->p_dnsdomain)) {
	    len = strlen(cfg->p_dnsdomain);
	    encode_ptr_tag(&p, DNSDOMAIN_NAME_TAG, cfg->p_dnsdomain, len);
	}
    }

    dh0_encode_vendor_options(reqP, offered_hsname, cfg, &p, dcip_data);
    
    if(reqP->GET_ROUTER) {
	encode_ptrvalue_array_tag(&p, ROUTER_TAG, cfg->p_router_addr);
    }
    if(reqP->GET_TIMESERVERS) {
	encode_ptrvalue_array_tag(&p, TIMESERVERS_TAG, cfg->p_tmsserver_addr);
    }
    if(reqP->GET_DNSSERVERS) {
	encode_ptrvalue_array_tag(&p, DNSSERVERS_TAG, cfg->p_dnsserver_addr);
    }
    if(reqP->GET_NIS_SERVERS) {
	encode_ptrvalue_array_tag(&p, NIS_SERVERS_TAG, cfg->p_nisserver_addr);
    }
    if(reqP->GET_IF_MTU) {
	encode_value_tag(&p, IF_MTU_TAG, cfg->p_mtu, 2);
    }
    if(reqP->GET_ALL_SUBNETS_LOCAL) {
	if(cfg->p_allnetsloc)
	    encode_value_tag(&p, ALL_SUBNETS_LOCAL_TAG, 1, 1);
	else
	    encode_value_tag(&p, ALL_SUBNETS_LOCAL_TAG, 0, 1);
    }
    if(reqP->GET_BROADCAST_ADDRESS) {
	encode_value_tag(&p, BROADCAST_ADDRESS_TAG, cfg->p_broadcast, 4);
    }
    if(reqP->GET_CLIENT_MASK_DISC) {
	if(cfg->p_domaskdisc)
	    encode_value_tag(&p, CLIENT_MASK_DISC_TAG, 1, 1);
	else
	    encode_value_tag(&p, CLIENT_MASK_DISC_TAG, 0, 1);
    }
    if(reqP->GET_MASK_SUPPLIER) {
	if(cfg->p_respmaskreq)
	    encode_value_tag(&p, MASK_SUPPLIER_TAG, 1, 1);
	else
	    encode_value_tag(&p, MASK_SUPPLIER_TAG, 0, 1);
    }
    if( (reqP->GET_STATIC_ROUTE) && cfg->p_routes) {
	tlen = 0;
	*p++ = STATIC_ROUTE_TAG;
	q = p++;
	i = 0;
	while(cfg->p_routes[i].r_dest && cfg->p_routes[i].r_router) {
	    bcopy(&cfg->p_routes[i].r_dest, p, 4);
	    p += 4;
	    bcopy(&cfg->p_routes[i].r_router, p, 4);
	    p += 4;
	    tlen += 8;
	    i++;
	}
	*q = tlen;
    }
    if(reqP->GET_RENEW_LEASE_TIME) {
	if(req_lease && (req_lease < cfg->p_lease))
	    encode_value_tag(&p, RENEW_LEASE_TIME_TAG, req_lease/2, 4);
	else
	    encode_value_tag(&p, RENEW_LEASE_TIME_TAG, cfg->p_lease/2, 4);
    }
    if(reqP->GET_REBIND_LEASE_TIME) {
	if(req_lease && (req_lease < cfg->p_lease))
	    encode_value_tag(&p, REBIND_LEASE_TIME_TAG, (7/8)*req_lease, 4);
	else
	    encode_value_tag(&p, REBIND_LEASE_TIME_TAG, (7/8)*cfg->p_lease, 4);
    }

    /* new options added here */
    if (reqP->GET_LOGSERVERS)
	encode_ptrvalue_array_tag(&p, LOGSERVERS_TAG, cfg->p_logserver_addr);
    if (reqP->GET_COOKIESERVERS)
	encode_ptrvalue_array_tag(&p, COOKIESERVERS_TAG,
				  cfg->p_cookieserver_addr);
    if (reqP->GET_LPRSERVERS)
	encode_ptrvalue_array_tag(&p, LPRSERVERS_TAG, cfg->p_LPRserver_addr);
    if (reqP->GET_RESOURCESERVERS)
	encode_ptrvalue_array_tag(&p, RESOURCESERVERS_TAG,
				  cfg->p_resourceserver_addr);
    if((reqP->GET_BOOTFILE_SIZE) &&  (cfg->p_bootfile_size))
	encode_value_tag(&p, BOOTFILE_SIZE_TAG, cfg->p_bootfile_size, 2);
    if(reqP->GET_SWAPSERVER_ADDRESS) 
	encode_value_tag(&p, SWAPSERVER_ADDRESS_TAG,
			 cfg->p_swapserver_addr, 4);
    if(reqP->GET_IP_FORWARDING) 
	encode_value_tag(&p, IP_FORWARDING_TAG, cfg->p_IPforwarding, 1);
    if(reqP->GET_NON_LOCAL_SRC_ROUTE) 
	encode_value_tag(&p, NON_LOCAL_SRC_ROUTE_TAG,
			 cfg->p_source_routing, 1);
    if( (reqP->GET_POLICY_FILTER) && cfg->p_policy_filter) {
	tlen = 0;
	*p++ = POLICY_FILTER_TAG;
	q = p++;
	i = 0;
	while(cfg->p_policy_filter[i].r_dest && cfg->p_routes[i].r_router) {
	    bcopy(&cfg->p_policy_filter[i].r_dest, p, 4); /* address */
	    p += 4;
	    bcopy(&cfg->p_policy_filter[i].r_router, p, 4); /* mask */
	    p += 4;
	    tlen += 8;
	    i++;
	}
	*q = tlen;
    }
    if(reqP->GET_MAX_DGRAM_REASSEMBL) 
	encode_value_tag(&p, MAX_DGRAM_REASSEMBL_TAG,
			 cfg->p_max_reassy_size, 2);
    if(reqP->GET_DEF_IP_TIME_LIVE) 
	encode_value_tag(&p, DEF_IP_TIME_LIVE_TAG, cfg->p_IP_ttl, 1);
    if(reqP->GET_PATH_MTU_AGE_TMOUT) 
	encode_value_tag(&p, PATH_MTU_AGE_TMOUT_TAG,
			 cfg->p_pathmtu_timeout, 4);
    if( (reqP->GET_PATH_MTU_PLT_TABLE) && (cfg->p_pathmtu_table)) {
	tlen = 0;
	*p++ = PATH_MTU_PLT_TABLE_TAG;
	q = p++;
	i = 0;
	while (tmpshort = cfg->p_pathmtu_table[i]) {
	    bcopy(&tmpshort, p, 2);
	    p += 1;
	    tlen += 2;
	    i++;
	}
	*q = tlen;
    }
    if(reqP->GET_ROUTER_DISC) 
	encode_value_tag(&p, ROUTER_DISC_TAG, cfg->p_router_disc, 1);
    if(reqP->GET_ROUTER_SOLICIT_ADDR) 
	encode_value_tag(&p, ROUTER_SOLICIT_ADDR_TAG,
			 cfg->p_router_solicit_addr, 4);
    if(reqP->GET_TRAILER_ENCAPS) 
	encode_value_tag(&p, TRAILER_ENCAPS_TAG, cfg->p_trailer_encaps, 1);
    if( (reqP->GET_ARP_CACHE_TIMEOUT) && (cfg->p_arpcache_timeout))
	encode_value_tag(&p, ARP_CACHE_TIMEOUT_TAG, cfg->p_arpcache_timeout, 4);
    if(reqP->GET_ETHERNET_ENCAPS) 
	encode_value_tag(&p, ETHERNET_ENCAPS_TAG, cfg->p_ether_encaps, 1);
    if( (reqP->GET_TCP_DEFAULT_TTL) && (cfg->p_TCP_ttl))
	encode_value_tag(&p, TCP_DEFAULT_TTL_TAG, cfg->p_TCP_ttl, 1);
    if( (reqP->GET_TCP_KEEPALIVE_INTVL) && (cfg->p_TCP_keepalive_intrvl))
	encode_value_tag(&p, TCP_KEEPALIVE_INTVL_TAG,
			 cfg->p_TCP_keepalive_intrvl, 4);
    if(reqP->GET_TCP_KEEPALIVE_GARBG) 
	encode_value_tag(&p, TCP_KEEPALIVE_GARBG_TAG,
			 cfg->p_TCP_keepalive_garbage, 1);
    if (reqP->GET_NETBIOS_NMSERV_ADDR)
	encode_ptrvalue_array_tag(&p, NETBIOS_NMSERV_ADDR_TAG,
				  cfg->p_NetBIOS_nameserver_addr);
    if (reqP->GET_NETBIOS_DISTR_ADDR)
	encode_ptrvalue_array_tag(&p, NETBIOS_DISTR_ADDR_TAG,
				  cfg->p_NetBIOS_distrserver_addr);
    if( (reqP->GET_NETBIOS_NODETYPE) && (cfg->p_NetBIOS_nodetype))
	encode_value_tag(&p, NETBIOS_NODETYPE_TAG,
			 cfg->p_NetBIOS_nodetype, 1);
    if(reqP->GET_NETBIOS_SCOPE) { /* is this a string ???? */
	if(cfg->p_NetBIOS_scope && *(cfg->p_NetBIOS_scope)) {
	    len = strlen(cfg->p_NetBIOS_scope);
	    encode_ptr_tag(&p, NETBIOS_SCOPE_TAG, cfg->p_NetBIOS_scope, len);
	}
    }
    if (reqP->GET_NETBIOS_DISTR_ADDR)
	encode_ptrvalue_array_tag(&p, NETBIOS_DISTR_ADDR_TAG,
				  cfg->p_NetBIOS_distrserver_addr);
    if (reqP->GET_X_FONTSERVER_ADDR)
	encode_ptrvalue_array_tag(&p, X_FONTSERVER_ADDR_TAG,
				  cfg->p_X_fontserver_addr);
    if (reqP->GET_X_DISPLAYMGR_ADDR)
	encode_ptrvalue_array_tag(&p, X_DISPLAYMGR_ADDR_TAG,
				  cfg->p_X_displaymgr_addr);
    if(reqP->GET_NISPLUS_DOMAIN) {
	if(cfg->p_nisplusdomain && *(cfg->p_nisplusdomain)) {
	    len = strlen(cfg->p_nisplusdomain);
	    encode_ptr_tag(&p, NISPLUS_DOMAIN_TAG, cfg->p_nisplusdomain, len);
	}
    }
    if (reqP->GET_NISPLUS_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, NISPLUS_SERVER_ADDR_TAG,
				  cfg->p_nisplusserver_addr);
    if (reqP->GET_MBLEIP_HMAGENT_ADDR)
	encode_ptrvalue_array_tag(&p, MBLEIP_HMAGENT_ADDR_TAG,
				  cfg->p_mobileIP_homeagent_addr);
    if (reqP->GET_SMTP_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, SMTP_SERVER_ADDR_TAG,
				  cfg->p_SMTPserver_addr);
    if (reqP->GET_POP3_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, POP3_SERVER_ADDR_TAG,
				  cfg->p_POP3server_addr);
    if (reqP->GET_NNTP_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, NNTP_SERVER_ADDR_TAG,
				  cfg->p_NNTPserver_addr);
    if (reqP->GET_WWW_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, WWW_SERVER_ADDR_TAG,
				  cfg->p_WWWserver_addr);
    if (reqP->GET_FINGER_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, FINGER_SERVER_ADDR_TAG,
				  cfg->p_fingerserver_addr);
    if (reqP->GET_IRC_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, IRC_SERVER_ADDR_TAG,
				  cfg->p_IRCserver_addr);
    if (reqP->GET_STTALK_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, STTALK_SERVER_ADDR_TAG,
				  cfg->p_StreetTalkserver_addr);
    if (reqP->GET_STDA_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, STDA_SERVER_ADDR_TAG,
				  cfg->p_STDAserver_addr);
    
    if (reqP->GET_TIME_OFFSET)
	encode_value_tag(&p, TIME_OFFSET_TAG, cfg->p_time_offset, 4);
    if (reqP->GET_NAMESERVER116_ADDR)
	encode_ptrvalue_array_tag(&p, NAME_SERVER116_TAG,
				  cfg->p_nameserver116_addr);
    if (reqP->GET_IMPRESS_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, IMPRESSSERVERS_TAG,
				  cfg->p_impressserver_addr);
    if (reqP->GET_MERITDUMP_FILE) {
	if (cfg->p_merit_dump_pathname && *(cfg->p_merit_dump_pathname)) {
	    len = strlen(cfg->p_merit_dump_pathname);
	    encode_ptr_tag(&p, MERITDUMP_FILE_TAG, cfg->p_merit_dump_pathname,
			   len);
	}
    }
    if (reqP->GET_ROOT_PATH) {
	if (cfg->p_root_disk_pathname && *(cfg->p_root_disk_pathname)) {
	    len = strlen(cfg->p_root_disk_pathname);
	    encode_ptr_tag(&p, ROOT_PATH_TAG, cfg->p_root_disk_pathname,
			   len);
	}
    }
    if (reqP->GET_EXTENSIONS) {
	if (cfg->p_extensions_pathname && *(cfg->p_extensions_pathname)) {
	    len = strlen(cfg->p_extensions_pathname);
	    encode_ptr_tag(&p, EXTENSIONS_PATH_TAG, cfg->p_extensions_pathname,
			   len);
	}
    }
    if (reqP->GET_NTP_SERVER_ADDR)
	encode_ptrvalue_array_tag(&p, NTP_SERVERS_TAG, cfg->p_NTPserver_addr);
    if (reqP->GET_TFTP_SERVER_NAME) {
	if (cfg->p_tftp_server_name && *(cfg->p_tftp_server_name)) {
	    len = strlen(cfg->p_tftp_server_name);
	    encode_ptr_tag(&p, TFTP_SERVER_NAME_TAG, cfg->p_tftp_server_name,
			   len);
	}
    }
    if (reqP->GET_BOOTFILE_NAME) {
	if (cfg->p_bootfile_name && *(cfg->p_bootfile_name)) {
	    len = strlen(cfg->p_bootfile_name);
	    encode_ptr_tag(&p, BOOTFILE_NAME_TAG, cfg->p_bootfile_name,
			   len);
	}
    }
			   
    if(msg) {
	len = strlen(msg);
	encode_ptr_tag(&p, DHCP_SERVER_MSG, msg, len);
    }

    *p = END_TAG;

    return 0;
}


int
sys0_addToHostsFile(u_long ipa, char *hname, char *nisdm)
{
    FILE *fp;
    struct in_addr ipn;
    char *ipchar;
    int rval;

    if (dont_update_hosts) 
	return 0;
    if(rval = sys0_removeFromHostsFile(ipa))
      return(rval);
    
    if(alt_hosts_file)
	fp = log_fopen(alt_hosts_file, "a");
    else
	fp = log_fopen(RealHostFile, "a");
    if(!fp)
	return(1);
    ipn.s_addr = ipa;
    ipchar = inet_ntoa(ipn);
    if (nisdm && *nisdm) {
	if (fprintf(fp, "%s\t%s.%s\t%s\n", ipchar, hname, nisdm, hname) < 0) {
	    fclose(fp);
	    return(1);
	}
    }
    else {
	if (fprintf(fp, "%s\t%s\n", ipchar, hname) < 0) {
	    fclose(fp);
	    return(1);
	}
    }
    fclose(fp);
    return(0);
}

int
sys0_addToEthersFile(EtherAddr *eth, char *hname)
{
    FILE *fp;
    char *ebuf;
    int rval;

    if (dont_update_ethers) 
	return 0;
    if (rval = sys0_removeFromEthersFile(eth))
      return rval;
    if(alt_ethers_file)
	fp = log_fopen(alt_ethers_file, "a");
    else
	fp = log_fopen(RealEtherFile, "a");
    if(!fp)
	return(1);
    ebuf = ether_ntoa(eth);
    if (fprintf(fp, "%s\t%s\n", ebuf, hname) < 0) {
	fclose(fp);
	return(1);
    }
    fclose(fp);
    return(0);
}

int
sys0_removeFromHostsFile(u_long ipaddr)
{
    FILE *fp, *fp1;
    char buf[256], zbuf[256];
    u_long curip;
    char *p;

    if (dont_update_hosts) 
	return 0;
    if(alt_hosts_file)
	fp = log_fopen(alt_hosts_file, "r");
    else
	fp = log_fopen(RealHostFile, "r");
    if(!fp)
	return(1);
    fp1 = log_fopen(tmpHostFile, "w");
    if(!fp1) {
	fclose(fp);
	return(2);
    }

    while(fgets(buf, 256, fp) != NULL) {
	if( (buf[0] == '\n') || (buf[0] == '#') ) {
	    if(efputs(buf, fp1)) {
		fclose(fp);
		fclose(fp1);
		return(2);
	    }
	    continue;
	}
	if(isdigit(buf[0])) {
	    strcpy(zbuf, buf);
	    p = buf;
	    while( (*p != ' ') && (*p != '\t') )
		p++;
	    *p = '\0';
	    curip = inet_addr(buf);
	    if(curip == ipaddr)
		continue;
	}
	if (efputs(zbuf, fp1)) {
	    fclose(fp);
	    fclose(fp1);
	    return(2);
	}
    }
    fclose(fp);
    fclose(fp1);
    if(alt_hosts_file) {
	if (erename(tmpHostFile, alt_hosts_file))
	    return 1;
    }
    else {
	if (erename(tmpHostFile, RealHostFile))
	    return 1;
    }
    return(0);
}

int
sys0_removeFromEthersFile(EtherAddr *eth)
{
    FILE *fp, *fp1;
    char buf[256], hsname[64];
    EtherAddr ehs;
    int len;
    int cr_found;

    if (dont_update_ethers) 
	return 0;
    if(alt_ethers_file)
	fp = log_fopen(alt_ethers_file, "r");
    else
	fp = log_fopen(RealEtherFile, "r");
    if(!fp)
	return(1);
    fp1 = log_fopen(tmpEtherFile, "w");
    if(!fp1) {
	fclose(fp);
	return(2);
    }

    while(fgets(buf, 256, fp) != NULL) {
	if( (buf[0] == '\n') || (buf[0] == '#') ) {
	    if (efputs(buf, fp1)) {
		fclose(fp);
		fclose(fp1);
		return(2);
	    }
	    continue;
	}
	if(isxdigit(buf[0])) {
	    len = strlen(buf);
	    cr_found = 0;
	    if(buf[len-1] == '\n') {
		buf[len-1] = '\0';
		cr_found = 1;
	    }
	    if(ether_line(buf, &ehs, hsname) == 0) {
		if(bcmp(ehs.ea_addr, eth->ea_addr, 6) == 0) {
		    cr_found = 0;
		    continue;
		}
	    }
	}
	if(cr_found) {
	   buf[len-1] = '\n';
	   cr_found = 0;
	}
	if (efputs(buf, fp1)) {
	    fclose(fp);
	    fclose(fp1);
	    return(2);
	}
    }
    fclose(fp);
    fclose(fp1);
    if(alt_ethers_file) {
	if (erename(tmpEtherFile, alt_ethers_file))
	    return 1;
    }
    else {
	if (erename(tmpEtherFile, RealEtherFile))
	    return 1;
    }
    return(0);
}

char *
sys0_name_errmsg(char *nm, int code)
{
    char xbuf[256];
    char *msg;

    switch(code) {
	case NAME_INVALID:
	    sprintf(xbuf, "%d:The selected hostname %s is not valid.",
		    NAME_INVALID, nm);
	    break;
	case NAME_INUSE:
	    sprintf(xbuf, "%d:The selected hostname %s is already in use.",
		    NAME_INUSE, nm);
	    break;
	default:
	    return (char *)0;
    }
    msg = strdup(xbuf);
    return msg;
}

char *
sys0_addr_errmsg(u_long addr, int code)
{
    char xbuf[256];
    char *msg;
    iaddr_t ipn;
    
    ipn.s_addr = addr;
    switch(code) {
	case ADDR_INVALID:
	    sprintf(xbuf, "%d:The selected address %s is not served by this server.",
		    ADDR_INVALID, inet_ntoa(ipn));
	    break;
	case ADDR_INUSE:
	    sprintf(xbuf, "%d:The selected address %s is already in use.",
		    ADDR_INUSE, inet_ntoa(ipn));
	    break;
	default:
	    return (char *)0;
    }
    msg = strdup(xbuf);
    return msg;
}


int
sys0_hostnameIsValid(char* hname)
{
    char tn[255];
    char c1, cn, c;
    int len, i;

    if(hname && (*hname == NULL))
	return 0;
    if((len = strlen(hname)) > MAXHOSTNAMELEN)
	return 0;
    strcpy(tn, hname);
    c1 = tn[0];
    cn = tn[len-1];
    if(!isalpha(c1))
	return 0;
    if(!isalnum(cn))
	return 0;
    for(i=0; i < len; i++) {
	c = tn[i];
	if(isalnum(c) || (c == '.') || (c == '-') )
	    continue;
	else
	    return 0;
    }
    for(i=1; i < len-2; i++) {
	if( ((tn[i] == '.')   || (tn[i] == '-')) &&
	    ((tn[i+1] == '.') || (tn[i+1] == '-')) )
	    return 0;
    }
    return 1;
}

static int
set_flag_state(char *fname, int fstate)
{
    FILE *fp;

    if ((fp = fopen(fname, "w")) == NULL)
	return(1);
    if (fstate == CHK_ON)
	fwrite("on\n", 3, 1, fp);
    else
	fwrite("off\n", 4, 1, fp);
    fclose(fp);
    return(0);
}

static int
get_flag_state(char *namep)
{
    char line[LINE_SIZE];
    FILE *fp;
    int  state = CHK_OFF;	/* assume off */

    if ((fp = log_fopen(namep, "r")) == NULL) {
	state = CHK_ERROR;
    } else {
	if ((fgets(line, LINE_SIZE, fp) != NULL) &&
	    (strncasecmp(line, "on", 2) == 0)) {
	    state = CHK_ON;
	}
	fclose(fp);
    }
    return(state);
}

int
chkconf(char *flag_file, char *flag_state)
{
    char flag_name[LINE_SIZE];
    int  state;

    if (flag_state == NULL) {
	strcpy(flag_name, CONFIG_DIR);
	strcat(flag_name, flag_file);
	if(get_flag_state(flag_name) == CHK_ON) {
	    state = CHK_ON;
	}
	else {
	    state = CHK_OFF;	/* it's off or can't read the file */
	}
    } else {
	strcpy(flag_name, CONFIG_DIR);
	strcat(flag_name, flag_file);
	if (strncasecmp("on", flag_state, 2) == 0)
	    state = CHK_ON;
	else if (strncasecmp("off", flag_state, 3) == 0)
	    state = CHK_OFF;
	else {
	    fprintf(stderr,"Error: state can only be \"on\" or \"off\"\n");
	    state = CHK_OTHER;
	}
	if(set_flag_state(flag_name, state)) {
	    fprintf(stderr, "Error: cannot set flag \"%s\": %s\n",
		    flag_file, flag_state);
	    state = CHK_OTHER;
	}
    }
    return(state);
}

/*
 * Convert Ethernet address to printable representation.
 */
char *
ether_sprintf(u_char *ap)
{
    char xbuf[64];
#define C(i)    (*(ap+i) & 0xff)
    sprintf(xbuf, "%x:%x:%x:%x:%x:%x\n", C(0), C(1), C(2), C(3), C(4), C(5));
#undef  C
    return xbuf;
}

int
cf0_encode_rfc1533_opts(u_char dhbuf[])
{
    char *p, *q;
    int i;
    int tlen, len;
    unsigned short tmpshort;

    p = (char *)dhbuf;

    if(rfc1533ptr->p_netmask)
	encode_value_tag(&p, SUBNETMASK_TAG, rfc1533ptr->p_netmask, 4);
    if(rfc1533ptr->p_time_offset)
	encode_value_tag(&p, TIME_OFFSET_TAG, rfc1533ptr->p_time_offset, 4);
    if(rfc1533ptr->p_router_addr)
	encode_ptrvalue_array_tag(&p, ROUTER_TAG, rfc1533ptr->p_router_addr);
    if(rfc1533ptr->p_tmsserver_addr)
	encode_ptrvalue_array_tag(&p, TIMESERVERS_TAG,
				  rfc1533ptr->p_tmsserver_addr);
    if(rfc1533ptr->p_nameserver116_addr)
	encode_ptrvalue_array_tag(&p, NAME_SERVER116_TAG,
				  rfc1533ptr->p_nameserver116_addr);
    if(rfc1533ptr->p_dnsserver_addr)
	encode_ptrvalue_array_tag(&p, DNSSERVERS_TAG,
				  rfc1533ptr->p_dnsserver_addr);
    if(rfc1533ptr->p_logserver_addr)
	encode_ptrvalue_array_tag(&p, LOGSERVERS_TAG,
				  rfc1533ptr->p_logserver_addr);
    if(rfc1533ptr->p_cookieserver_addr)
	encode_ptrvalue_array_tag(&p, COOKIESERVERS_TAG,
				  rfc1533ptr->p_cookieserver_addr);
    if(rfc1533ptr->p_LPRserver_addr)
	encode_ptrvalue_array_tag(&p, LPRSERVERS_TAG,
				  rfc1533ptr->p_LPRserver_addr);
    if(rfc1533ptr->p_impressserver_addr)
	encode_ptrvalue_array_tag(&p, IMPRESSSERVERS_TAG,
				  rfc1533ptr->p_impressserver_addr);
    if(rfc1533ptr->p_resourceserver_addr)
	encode_ptrvalue_array_tag(&p, RESOURCESERVERS_TAG,
				  rfc1533ptr->p_resourceserver_addr);
    if(rfc1533ptr->p_bootfile_size)
	encode_value_tag(&p, BOOTFILE_SIZE_TAG, rfc1533ptr->p_bootfile_size, 2);
    if(rfc1533ptr->p_merit_dump_pathname) {
	if( rfc1533ptr->p_merit_dump_pathname &&
	    *(rfc1533ptr->p_merit_dump_pathname)) {
	    len = strlen(rfc1533ptr->p_merit_dump_pathname);
	    encode_ptr_tag(&p, MERITDUMP_FILE_TAG,
			   rfc1533ptr->p_merit_dump_pathname, len);
	}
    }
    if(rfc1533ptr->p_dnsdomain) {
	if(rfc1533ptr->p_dnsdomain && *(rfc1533ptr->p_dnsdomain)) {
	    len = strlen(rfc1533ptr->p_dnsdomain);
	    encode_ptr_tag(&p, DNSDOMAIN_NAME_TAG, rfc1533ptr->p_dnsdomain, len);
	}
    }
    if(rfc1533ptr->p_swapserver_addr)
	encode_value_tag(&p, SWAPSERVER_ADDRESS_TAG,
			 rfc1533ptr->p_swapserver_addr, 4);
    if(rfc1533ptr->p_root_disk_pathname) {
	if( rfc1533ptr->p_root_disk_pathname &&
	    *(rfc1533ptr->p_root_disk_pathname)) {
	    len = strlen(rfc1533ptr->p_root_disk_pathname);
	    encode_ptr_tag(&p, ROOT_PATH_TAG,
			   rfc1533ptr->p_root_disk_pathname, len);
	}
    }
    if(rfc1533ptr->p_extensions_pathname) {
	if( rfc1533ptr->p_root_disk_pathname &&
	    *(rfc1533ptr->p_root_disk_pathname)) {
	    len = strlen(rfc1533ptr->p_root_disk_pathname);
	    encode_ptr_tag(&p, ROOT_PATH_TAG,
			   rfc1533ptr->p_root_disk_pathname, len);
	}
    }
    if(rfc1533ptr->p_IPforwarding)
	encode_value_tag(&p, IP_FORWARDING_TAG, rfc1533ptr->p_IPforwarding, 1);
    if(rfc1533ptr->p_source_routing)
	encode_value_tag(&p, NON_LOCAL_SRC_ROUTE_TAG,
			 rfc1533ptr->p_source_routing, 1);
    if(rfc1533ptr->p_policy_filter) {
	tlen = 0;
	*p++ = POLICY_FILTER_TAG;
	q = p++;
	i = 0;
	while(rfc1533ptr->p_policy_filter[i].r_dest &&
	      rfc1533ptr->p_routes[i].r_router) {
	    bcopy(&rfc1533ptr->p_policy_filter[i].r_dest, p, 4); /* address */
	    p += 4;
	    bcopy(&rfc1533ptr->p_policy_filter[i].r_router, p, 4); /* mask */
	    p += 4;
	    tlen += 8;
	    i++;
	}
	*q = tlen;
    }
    if(rfc1533ptr->p_max_reassy_size)
	encode_value_tag(&p, MAX_DGRAM_REASSEMBL_TAG,
			 rfc1533ptr->p_max_reassy_size, 2);
    if(rfc1533ptr->p_IP_ttl)
	encode_value_tag(&p, DEF_IP_TIME_LIVE_TAG, rfc1533ptr->p_IP_ttl, 1);
    if(rfc1533ptr->p_pathmtu_timeout)
	encode_value_tag(&p, PATH_MTU_AGE_TMOUT_TAG,
			 rfc1533ptr->p_pathmtu_timeout, 4);
    if(rfc1533ptr->p_pathmtu_table) {
	tlen = 0;
	*p++ = PATH_MTU_PLT_TABLE_TAG;
	q = p++;
	i = 0;
	while (tmpshort = rfc1533ptr->p_pathmtu_table[i]) {
	    bcopy(&tmpshort, p, 2);
	    p += 1;
	    tlen += 2;
	    i++;
	}
	*q = tlen;
    }
    if(rfc1533ptr->p_mtu)
	encode_value_tag(&p, IF_MTU_TAG, rfc1533ptr->p_mtu, 2);
    if(rfc1533ptr->p_allnetsloc)
	encode_value_tag(&p, ALL_SUBNETS_LOCAL_TAG, 1, 1);
    if(rfc1533ptr->p_broadcast)
	encode_value_tag(&p, BROADCAST_ADDRESS_TAG, rfc1533ptr->p_broadcast, 4);
    if(rfc1533ptr->p_domaskdisc)
	encode_value_tag(&p, CLIENT_MASK_DISC_TAG, 1, 1);
    if(rfc1533ptr->p_respmaskreq)
	encode_value_tag(&p, MASK_SUPPLIER_TAG, 1, 1);
    if(rfc1533ptr->p_router_disc)
	encode_value_tag(&p, ROUTER_DISC_TAG, rfc1533ptr->p_router_disc, 1);
    if(rfc1533ptr->p_router_solicit_addr)
	encode_value_tag(&p, ROUTER_SOLICIT_ADDR_TAG,
			 rfc1533ptr->p_router_solicit_addr, 4);
    if(rfc1533ptr->p_routes) {
	tlen = 0;
	*p++ = STATIC_ROUTE_TAG;
	q = p++;
	i = 0;
	while(rfc1533ptr->p_routes[i].r_dest &&
	      rfc1533ptr->p_routes[i].r_router) {
	    bcopy(&rfc1533ptr->p_routes[i].r_dest, p, 4);
	    p += 4;
	    bcopy(&rfc1533ptr->p_routes[i].r_router, p, 4);
	    p += 4;
	    tlen += 8;
	    i++;
	}
	*q = tlen;
    }
    if(rfc1533ptr->p_trailer_encaps)
	encode_value_tag(&p, TRAILER_ENCAPS_TAG,
			 rfc1533ptr->p_trailer_encaps, 1);
    if(rfc1533ptr->p_arpcache_timeout)
	encode_value_tag(&p, ARP_CACHE_TIMEOUT_TAG,
			 rfc1533ptr->p_arpcache_timeout, 4);
    if(rfc1533ptr->p_ether_encaps)
	encode_value_tag(&p, ETHERNET_ENCAPS_TAG, rfc1533ptr->p_ether_encaps, 1);
    if(rfc1533ptr->p_TCP_ttl)
	encode_value_tag(&p, TCP_DEFAULT_TTL_TAG, rfc1533ptr->p_TCP_ttl, 1);
    if(rfc1533ptr->p_TCP_keepalive_intrvl)
	encode_value_tag(&p, TCP_KEEPALIVE_INTVL_TAG,
			 rfc1533ptr->p_TCP_keepalive_intrvl, 4);
    if(rfc1533ptr->p_TCP_keepalive_garbage)
	encode_value_tag(&p, TCP_KEEPALIVE_GARBG_TAG,
			 rfc1533ptr->p_TCP_keepalive_garbage, 1);
    if(rfc1533ptr->p_nisdomain) {
	if(rfc1533ptr->p_nisdomain && *(rfc1533ptr->p_nisdomain)) {
	    len = strlen(rfc1533ptr->p_nisdomain);
	    encode_ptr_tag(&p, NISDOMAIN_NAME_TAG, rfc1533ptr->p_nisdomain, len);
	}
    }
    if(rfc1533ptr->p_nisserver_addr)
	encode_ptrvalue_array_tag(&p, NIS_SERVERS_TAG,
				  rfc1533ptr->p_nisserver_addr);
    if(rfc1533ptr->p_NTPserver_addr)
	encode_ptrvalue_array_tag(&p, NTP_SERVERS_TAG,
				  rfc1533ptr->p_NTPserver_addr);
    if(rfc1533ptr->p_NetBIOS_nameserver_addr)
	encode_ptrvalue_array_tag(&p, NETBIOS_NMSERV_ADDR_TAG,
				  rfc1533ptr->p_NetBIOS_nameserver_addr);
    if(rfc1533ptr->p_NetBIOS_distrserver_addr)
	encode_ptrvalue_array_tag(&p, NETBIOS_DISTR_ADDR_TAG,
				  rfc1533ptr->p_NetBIOS_distrserver_addr);
    if(rfc1533ptr->p_NetBIOS_nodetype)
	encode_value_tag(&p, NETBIOS_NODETYPE_TAG,
			 rfc1533ptr->p_NetBIOS_nodetype, 1);
    if(rfc1533ptr->p_NetBIOS_scope) {
	if(rfc1533ptr->p_NetBIOS_scope && *(rfc1533ptr->p_NetBIOS_scope)) {
	    len = strlen(rfc1533ptr->p_NetBIOS_scope);
	    encode_ptr_tag(&p, NETBIOS_SCOPE_TAG,
			   rfc1533ptr->p_NetBIOS_scope, len);
	}
    }
    if(rfc1533ptr->p_X_fontserver_addr)
	encode_ptrvalue_array_tag(&p, X_FONTSERVER_ADDR_TAG,
				  rfc1533ptr->p_X_fontserver_addr);
    if(rfc1533ptr->p_X_displaymgr_addr)
	encode_ptrvalue_array_tag(&p, X_DISPLAYMGR_ADDR_TAG,
				  rfc1533ptr->p_X_displaymgr_addr);
    if(rfc1533ptr->p_nisplusdomain) {
	if(rfc1533ptr->p_nisplusdomain && *(rfc1533ptr->p_nisplusdomain)) {
	    len = strlen(rfc1533ptr->p_nisplusdomain);
	    encode_ptr_tag(&p, NISPLUS_DOMAIN_TAG,
			   rfc1533ptr->p_nisplusdomain, len);
	}
    }
    if(rfc1533ptr->p_nisplusserver_addr)
	encode_ptrvalue_array_tag(&p, NISPLUS_SERVER_ADDR_TAG,
				  rfc1533ptr->p_nisplusserver_addr);
    if(rfc1533ptr->p_mobileIP_homeagent_addr)
	encode_ptrvalue_array_tag(&p, MBLEIP_HMAGENT_ADDR_TAG,
				  rfc1533ptr->p_mobileIP_homeagent_addr);
    if(rfc1533ptr->p_SMTPserver_addr)
	encode_ptrvalue_array_tag(&p, SMTP_SERVER_ADDR_TAG,
				  rfc1533ptr->p_SMTPserver_addr);
    if(rfc1533ptr->p_POP3server_addr)
	encode_ptrvalue_array_tag(&p, POP3_SERVER_ADDR_TAG,
				  rfc1533ptr->p_POP3server_addr);
    if(rfc1533ptr->p_NNTPserver_addr)
	encode_ptrvalue_array_tag(&p, NNTP_SERVER_ADDR_TAG,
				  rfc1533ptr->p_NNTPserver_addr);
    if(rfc1533ptr->p_WWWserver_addr)
	encode_ptrvalue_array_tag(&p, WWW_SERVER_ADDR_TAG,
				  rfc1533ptr->p_WWWserver_addr);
    if(rfc1533ptr->p_fingerserver_addr)
	encode_ptrvalue_array_tag(&p, FINGER_SERVER_ADDR_TAG,
				  rfc1533ptr->p_fingerserver_addr);
    if(rfc1533ptr->p_IRCserver_addr)
	encode_ptrvalue_array_tag(&p, IRC_SERVER_ADDR_TAG,
				  rfc1533ptr->p_IRCserver_addr);
    if(rfc1533ptr->p_StreetTalkserver_addr)
	encode_ptrvalue_array_tag(&p, STTALK_SERVER_ADDR_TAG,
				  rfc1533ptr->p_StreetTalkserver_addr);
    if(rfc1533ptr->p_STDAserver_addr)
	encode_ptrvalue_array_tag(&p, STDA_SERVER_ADDR_TAG,
				  rfc1533ptr->p_STDAserver_addr);
    *p = END_TAG;
    return 0;
}

/*
 * error checking fputs
 * return 0 if no error otherwise 1
 * fputs does not check/set errno hence using fwrite
 */

int efputs(char *buf, FILE *fp)
{
    if (fwrite(buf, strlen(buf), 1, fp) != 1) {
	syslog(LOG_ERR, "File write error %s", strerror(errno));
	return 1;
    }
    return 0;
}
/*
 * error checking rename
 * return 0 if no error otherwise 1
 * fputs does not check/set errno hence using fwrite
 */

int erename(char *old, char *new)
{
    if (rename(old, new) < 0) {
	syslog(LOG_ERR, "Rename error %s", strerror(errno));
	return 1;
    }
    return 0;
}


void setparams( struct getParams *reqParams, int len, u_char *p)
{
      int i;

      for(i = 0; i < len; i++) {
	switch(*p++) {
	case IP_LEASE_TIME_TAG:
	  reqParams->GET_IPLEASE_TIME = 1;
	  break;
	case SUBNETMASK_TAG:
	  reqParams->GET_SUBNETMASK = 1;
	  break;
	case HOSTNAME_TAG:
	  reqParams->GET_HOSTNAME = 1;
	  break;
	case NISDOMAIN_NAME_TAG:
	  reqParams->GET_NISDOMAIN = 1;
	  break;
	case DNSDOMAIN_NAME_TAG:
	  reqParams->GET_DNSDOMAIN = 1;
	  break;
	case DHCP_SERVER_TAG:
	  reqParams->GET_DHCP_SERVER = 1;
	  break;
	case RESOLVE_HOSTNAME_TAG:
	  reqParams->RESOLVE_HOSTNAME = 1;
	  break;
	case SDIST_SERVER_TAG:
	  reqParams->GET_SDIST_SERVER = 1;
	  break;
	case IPADDR_TAG:
	  reqParams->GET_IPADDRESS = 1;
	  break;
	case ROUTER_TAG:
	  reqParams->GET_ROUTER = 1;
	  break;
	case TIMESERVERS_TAG:
	  reqParams->GET_TIMESERVERS = 1;
	  break;
	case DNSSERVERS_TAG:
	  reqParams->GET_DNSSERVERS = 1;
	  break;
	case NIS_SERVERS_TAG:
	  reqParams->GET_NIS_SERVERS = 1;
	  break;
	case IF_MTU_TAG:
	  reqParams->GET_IF_MTU = 1;
	  break;
	case ALL_SUBNETS_LOCAL_TAG:
	  reqParams->GET_ALL_SUBNETS_LOCAL = 1;
	  break;
	case BROADCAST_ADDRESS_TAG:
	  reqParams->GET_BROADCAST_ADDRESS = 1;
	  break;
	case CLIENT_MASK_DISC_TAG:
	  reqParams->GET_CLIENT_MASK_DISC = 1;
	  break;
	case MASK_SUPPLIER_TAG:
	  reqParams->GET_MASK_SUPPLIER = 1;
	  break;
	case STATIC_ROUTE_TAG:
	  reqParams->GET_STATIC_ROUTE = 1;
	  break;
	case OPTION_OVERLOAD_TAG:
	  reqParams->HAS_OPTION_OVERLOAD = 1;
	  break;
	case MAX_DHCP_MSG_SIZE_TAG:
	  reqParams->HAS_MAX_DHCP_MSG_SIZE = 1;
	  break;
	case RENEW_LEASE_TIME_TAG:
	  reqParams->GET_RENEW_LEASE_TIME = 1;
	  break;
	case REBIND_LEASE_TIME_TAG:
	  reqParams->GET_REBIND_LEASE_TIME = 1;
	  break;
	case LOGSERVERS_TAG:
	  reqParams->GET_LOGSERVERS = 1;
	  break;
	case COOKIESERVERS_TAG:
	  reqParams->GET_COOKIESERVERS = 1;
	  break;
	case LPRSERVERS_TAG:
	  reqParams->GET_LPRSERVERS = 1;
	  break;
	case RESOURCESERVERS_TAG:
	  reqParams->GET_RESOURCESERVERS = 1;
	  break;
	case BOOTFILE_SIZE_TAG:
	  reqParams->GET_BOOTFILE_SIZE = 1;
	  break;
	case SWAPSERVER_ADDRESS_TAG:
	  reqParams->GET_SWAPSERVER_ADDRESS = 1;
	  break;
	case IP_FORWARDING_TAG:
	  reqParams->GET_IP_FORWARDING = 1;
	  break;
	case NON_LOCAL_SRC_ROUTE_TAG:
	  reqParams->GET_NON_LOCAL_SRC_ROUTE = 1;
	  break;
	case POLICY_FILTER_TAG:
	  reqParams->GET_POLICY_FILTER = 1;
	  break;
	case MAX_DGRAM_REASSEMBL_TAG:
	  reqParams->GET_MAX_DGRAM_REASSEMBL = 1;
	  break;
	case DEF_IP_TIME_LIVE_TAG:
	  reqParams->GET_DEF_IP_TIME_LIVE = 1;
	  break;
	case PATH_MTU_AGE_TMOUT_TAG:
	  reqParams->GET_PATH_MTU_AGE_TMOUT = 1;
	  break;
	case PATH_MTU_PLT_TABLE_TAG:
	  reqParams->GET_PATH_MTU_PLT_TABLE = 1;
	  break;
	case ROUTER_DISC_TAG:
	  reqParams->GET_ROUTER_DISC = 1;
	  break;
	case ROUTER_SOLICIT_ADDR_TAG:
	  reqParams->GET_ROUTER_SOLICIT_ADDR = 1;
	  break;
	case TRAILER_ENCAPS_TAG:
	  reqParams->GET_TRAILER_ENCAPS = 1;
	  break;
	case ARP_CACHE_TIMEOUT_TAG:
	  reqParams->GET_ARP_CACHE_TIMEOUT = 1;
	  break;
	case ETHERNET_ENCAPS_TAG:
	  reqParams->GET_ETHERNET_ENCAPS = 1;
	  break;
	case TCP_DEFAULT_TTL_TAG:
	  reqParams->GET_TCP_DEFAULT_TTL = 1;
	  break;
	case TCP_KEEPALIVE_INTVL_TAG:
	  reqParams->GET_TCP_KEEPALIVE_INTVL = 1;
	  break;
	case TCP_KEEPALIVE_GARBG_TAG:
	  reqParams->GET_TCP_KEEPALIVE_GARBG = 1;
	  break;
	case NETBIOS_NMSERV_ADDR_TAG:
	  reqParams->GET_NETBIOS_NMSERV_ADDR = 1;
	  break;
	case NETBIOS_DISTR_ADDR_TAG:
	  reqParams->GET_NETBIOS_DISTR_ADDR = 1;
	  break;
	case NETBIOS_NODETYPE_TAG:
	  reqParams->GET_NETBIOS_NODETYPE = 1;
	  break;
	case NETBIOS_SCOPE_TAG:
	  reqParams->GET_NETBIOS_SCOPE = 1;
	  break;
	case X_FONTSERVER_ADDR_TAG:
	  reqParams->GET_X_FONTSERVER_ADDR = 1;
	  break;
	case X_DISPLAYMGR_ADDR_TAG:
	  reqParams->GET_X_DISPLAYMGR_ADDR = 1;
	  break;
	case NISPLUS_DOMAIN_TAG:
	  reqParams->GET_NISPLUS_DOMAIN = 1;
	  break;
	case NISPLUS_SERVER_ADDR_TAG:
	  reqParams->GET_NISPLUS_SERVER_ADDR = 1;
	  break;
	case MBLEIP_HMAGENT_ADDR_TAG:
	  reqParams->GET_MBLEIP_HMAGENT_ADDR = 1;
	  break;
	case SMTP_SERVER_ADDR_TAG:
	  reqParams->GET_SMTP_SERVER_ADDR = 1;
	  break;
	case POP3_SERVER_ADDR_TAG:
	  reqParams->GET_POP3_SERVER_ADDR = 1;
	  break;
	case NNTP_SERVER_ADDR_TAG:
	  reqParams->GET_NNTP_SERVER_ADDR = 1;
	  break;
	case WWW_SERVER_ADDR_TAG:
	  reqParams->GET_WWW_SERVER_ADDR = 1;
	  break;
	case FINGER_SERVER_ADDR_TAG:
	  reqParams->GET_FINGER_SERVER_ADDR = 1;
	  break;
	case IRC_SERVER_ADDR_TAG:
	  reqParams->GET_IRC_SERVER_ADDR = 1;
	  break;
	case STTALK_SERVER_ADDR_TAG:
	  reqParams->GET_STTALK_SERVER_ADDR = 1;
	  break;
	case STDA_SERVER_ADDR_TAG:
	  reqParams->GET_STDA_SERVER_ADDR = 1;
	  break;
	case TIME_OFFSET_TAG:
	  reqParams->GET_TIME_OFFSET = 1;
	  break;
	case NAME_SERVER116_TAG:
	  reqParams->GET_NAMESERVER116_ADDR = 1;
	  break;
	case IMPRESSSERVERS_TAG:
	  reqParams->GET_IMPRESS_SERVER_ADDR = 1;
	  break;
	case MERITDUMP_FILE_TAG:
	  reqParams->GET_MERITDUMP_FILE = 1;
	  break;
	case ROOT_PATH_TAG:
	  reqParams->GET_ROOT_PATH = 1;
	  break;
	case EXTENSIONS_PATH_TAG:
	  reqParams->GET_EXTENSIONS = 1;
	  break;
	case NTP_SERVERS_TAG:
	  reqParams->GET_NTP_SERVER_ADDR = 1;
	  break;
	case TFTP_SERVER_NAME_TAG:
	  reqParams->GET_TFTP_SERVER_NAME = 1;
	  break;
	case BOOTFILE_NAME_TAG:
	  reqParams->GET_BOOTFILE_NAME = 1;
	  break;
	default:
	  break;
	}
      }
}

void
set_addl_options_to_send(char *pstr)
{
    int tmp;

    pstr = strtok(pstr, ":,");
    addl_options_len = 0;
    while (pstr) {
      sscanf(pstr, " %d", &tmp);
      addl_options_to_send[addl_options_len] = (char)tmp;
      addl_options_len++;
      pstr = strtok(0, ":,");
    }
}

/*
 * Append the client fqdn onto the DHCPACK reply
 * Its is not possible to do this in the encode function because the
 * DNS may have to be update and the return codes are needed
 */
int
dh0_append_client_fqdn(u_char dhbuf[], char *client_fqdn)
{
    /* find the end of the options by walking down */
    u_char *p = dhbuf;
    u_char *p_end = p + DHCP_OPTS_SIZE;
    int len;

				/* skip until end */
    while ( (p < p_end) && (*p != END_TAG) ) {
	switch (*p++) {
	case PAD_TAG:
	    continue;
	default:
	    len = *p++;
	    p += len;
	    continue;
	}
    }
    if (*p != END_TAG) {
	syslog(LOG_ERR, "Did not find end tag when adding the client_fqdn");
	return 1;
    }
				/* encode client fqdn */
    if ( (*client_fqdn != NULL) ) {
	len = 4 + strlen(client_fqdn+3);
	encode_ptr_tag((char **)&p, CLIENT_FQDN_TAG, client_fqdn, len);
    }
    
    *p = END_TAG;

    return 0;
}
