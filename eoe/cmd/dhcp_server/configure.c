#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <string.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <assert.h>
#include <ndbm.h>
#include "dhcpdefs.h"
#include "dhcp_types.h"
#include "dhcp_optvals.h"

extern int sys0_removeFromHostsFile(u_long);
extern int dh0_remove_system_mapping(char *,EtherAddr *,int, DBM *);
extern int send_blocking_ping(int sd, u_long addr, int timeoutSecs, 
			      int timeoutMsecs, int tries);
extern int cf0_initialize_vendor_class(void);
extern void free_vendor_options(void);
extern void mk_str(int cid_flag, char *cid_ptr, int cid_length, 
		   char *str);

extern int alt_naming_flag;

#define	ulong_error	0xffffffff

static char *serve_this_network = "Serve_This_Network";
static char *lb_pro_addr_counter = "pro_address_counter";
static char *lb_pro_host_pfxcnt = "pro_host_pfx_counter";
static char *lb_pro_netmask = "pro_netmask";
static char *lb_pro_lease = "pro_lease";
static char *lb_pro_propel_server = "pro_propel_server";
static char *lb_pro_host_prefix = "pro_host_prefix";
static char *lb_pro_choose_name = "pro_choose_name";
static char *lb_pro_ipaddress_range = "pro_ipaddress_range";
static char *lb_pro_router_addr = "pro_router_addr";
static char *lb_pro_timeserver_addr = "pro_timeserver_addr";
static char *lb_pro_dnsserver_addr = "pro_dnsserver_addr";
static char *lb_pro_nisserver_addr = "pro_nisserver_addr";
static char *lb_pro_dns_domain = "pro_dns_domain";
static char *lb_pro_nis_domain = "pro_nis_domain";
static char *lb_pro_mtu = "pro_mtu";
static char *lb_pro_allnets_local = "pro_allnets_local";
static char *lb_pro_broadcast = "pro_broadcast";
static char *lb_pro_domask_disc = "pro_domask_disc";
static char *lb_pro_resp_mask_req = "pro_resp_mask_req";
static char *lb_pro_do_router_disc = "pro_do_router_disc";
static char *lb_pro_router_solicit_addr = "pro_router_solicit_addr";
static char *lb_pro_static_routes = "pro_static_routes";

static char *lb_pro_logserver_addr = "pro_logserver_addr";
static char *lb_pro_cookieserver_addr = "pro_cookieserver_addr";
static char *lb_pro_LPRserver_addr = "pro_LPRserver_addr";
static char *lb_pro_resourceserver_addr = "pro_resourceserver_addr";
static char *lb_pro_bootfile_size = "pro_bootfile_size";
static char *lb_pro_swapserver_addr = "pro_swapserver_addr";
static char *lb_pro_IPforwarding = "pro_IPforwarding";
static char *lb_pro_source_routing = "pro_source_routing";
static char *lb_pro_policy_filter = "pro_policy_filter";
static char *lb_pro_max_reassy_size = "pro_max_reassy_size";
static char *lb_pro_IP_ttl = "pro_IP_ttl";
static char *lb_pro_pathmtu_timeout = "pro_pathmtu_timeout";
static char *lb_pro_pathmtu_table = "pro_pathmtu_table";
static char *lb_pro_trailer_encaps = "pro_trailer_encaps";
static char *lb_pro_arpcache_timeout = "pro_arpcache_timeout";
static char *lb_pro_ether_encaps = "pro_ether_encaps";
static char *lb_pro_TCP_ttl = "pro_TCP_ttl";
static char *lb_pro_TCP_keepalive_intrvl = "pro_TCP_keepalive_intrvl";
static char *lb_pro_TCP_keepalive_garbage = "pro_TCP_keepalive_garbage";
static char *lb_pro_NetBIOS_nameserver_addr = "pro_NetBIOS_nameserver_addr";
static char *lb_pro_NetBIOS_distrserver_addr = "pro_NetBIOS_distrserver_addr";
static char *lb_pro_NetBIOS_nodetype = "pro_NetBIOS_nodetype";
static char *lb_pro_NetBIOS_scope = "pro_NetBIOS_scope";
static char *lb_pro_X_fontserver_addr = "pro_X_fontserver_addr";
static char *lb_pro_X_displaymgr_addr = "pro_X_displaymgr_addr";
static char *lb_pro_nisplus_domain = "pro_nisplus_domain";
static char *lb_pro_nisplusserver_addr = "pro_nisplusserver_addr";
static char *lb_pro_mobileIP_homeagent_addr = "pro_mobileIP_homeagent_addr";
static char *lb_pro_SMTPserver_addr = "pro_SMTPserver_addr";
static char *lb_pro_POP3server_addr = "pro_POP3server_addr";
static char *lb_pro_NNTPserver_addr = "pro_NNTPserver_addr";
static char *lb_pro_WWWserver_addr = "pro_WWWserver_addr";
static char *lb_pro_fingerserver_addr = "pro_fingerserver_addr";
static char *lb_pro_IRCserver_addr = "pro_IRCserver_addr";
static char *lb_pro_StreetTalkserver_addr = "pro_StreetTalkserver_addr";
static char *lb_pro_STDAserver_addr = "pro_STDAserver_addr";

static char *lb_pro_time_offset = "pro_time_offset";
static char *lb_pro_nameserver116_addr = "pro_nameserver116_addr";
static char *lb_pro_impressserver_addr = "pro_impressserver_addr";
static char *lb_pro_meritdump_pathname = "pro_meritdump_pathname";
static char *lb_pro_root_pathname = "pro_root_pathname";
static char *lb_pro_extensions_pathname = "pro_extensions_pathname";
static char *lb_pro_NTPserver_addr = "pro_NTPserver_addr";
static char *lb_pro_TFTPserver_name = "pro_TFTPserver_name";
static char *lb_pro_bootfile_name = "pro_bootfile_name";

/* end new options added */
char *rfc1533configFile = "/etc/config/bootp-rfc1533.options";

dhcpConfig *configListPtr;

static char bufx[1024];
static int found_default = 0;

extern struct in_addr myhostaddr;
extern char *useConfigDir;
extern char *nha0_get_primary_interface_name(u_long);
extern u_long *dh0_get_dnsServers(void);
extern u_long *nha0_get_nis_servers(void);
extern char *dh0_getNisDomain(void);
extern char *dh0_getDnsDomain(void);
extern char *loc0_get_hostname_from_cid(char *, int, EtherAddr *, DBM *);
extern u_long nha0_getSubnetmask(char *);
extern int nha0_get_kmem_data(char *, int *);
extern int nha0_haveNfsOption(void);
extern u_long nha0_getBroadcastAddr(char *);
extern int loc0_is_hostname_assigned(char *,DBM *);
extern int loc0_is_ipaddress_assigned(u_long,DBM *);
extern u_long loc0_get_ipaddr_from_cid(EtherAddr *, char *, int, DBM *);
extern int loc0_scan_find_lru_entry(dhcpConfig *, EtherAddr *, u_long *, 
				    char **, char *, int *, DBM *);
extern int loc0_expire_entry(EtherAddr *, char *, int, u_long,  DBM *);
extern int loc0_remove_entry(EtherAddr *, int, char *, int, DBM *);
extern int loc0_create_new_entry_l(char *, int, EtherAddr *, u_long, char *, 
				   time_t,DBM *);
extern int loc0_is_etherToIP_duplicate(char *, int, EtherAddr *, u_long , 
				       char *, DBM *);
extern long loc0_get_lease(u_long ipa, DBM*);

extern FILE *log_fopen(char *, char *);
extern int debug;
extern u_long mynetmask;
extern char *mydomain;
extern struct netaddr *np_recv;
extern int ifcount;
extern int ProclaimServer;
extern int ping_blocking_check;
extern int ping_sd;
extern u_long retry_stolen_timeout;

void
indexPastColonAndRemoveSpaces(char *str)
{
    char *p;
    int len, i;

    p = str;
    len = strlen(p);
    if(p[len-1] == '\n')
	p[len-1] = '\0';
    for (i = 0; *p ; p++) {
	if((i == 0) && (*p == ':')) 
	    continue;
	if(isspace(*p))
	    continue;
	bufx[i++] = *p;
    }
    bufx[i] = '\0';
}

static u_long *
getAddressList(char *str)
{
    char *p, *q;
    int cnt, i;
    u_long *ulptr;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;
    if(*p == '\0')
	return NULL;

    cnt = 1;
    while(p && *p) {
	if(*p == ',')
	    cnt++;
	p++;
    }
    ulptr = (u_long *)malloc((cnt+1) * sizeof(u_long));
    ulptr[cnt] = 0;
    p = bufx;
    for(i = 0; i < cnt; i++) {
	q = strtok(p, ",");
        if( (*q == '0') && (*(q+1) == 'x'))	/* hex number */
	    ulptr[i] = strtoul(q, NULL, 16);
	else
	    ulptr[i] = inet_addr(q);
	p = NULL;

    }
    return ulptr;
}

struct addr_range *
getAddressRanges(char *str)
{
    char *p, *q, *r;
    int cnt, i;
    struct addr_range *addrptr;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;

    if(*p == '\0')
	return (struct addr_range *)0;

    cnt = 1;
    while(p && *p) {
	if(*p == ',')
	    cnt++;
	p++;
    }
    addrptr = (struct addr_range *)malloc((cnt+1) * sizeof(struct addr_range));
    addrptr[cnt].r_low = addrptr[cnt].r_high = 0;
    p = bufx;
    for(i = 0; i < cnt; i++) {
	q = strtok(p, ",");
	r = q;
	while(r && (*r != '\0') && (*r != '-') ) r++;
	if(*r == '-') {
	     /* It is a range n-m */
	    *r = '\0';
	    r++;
	    addrptr[i].r_low = atoi(q);	
	    addrptr[i].r_high = atoi(r);
	}
	else {
	     /* It is a single value n */
	    addrptr[i].r_low = addrptr[i].r_high = atoi(q);
	}
	p = NULL;
    }
    return addrptr;
}

static struct addr_pair *
getAddressPairs(char *str)
{
    char *p, *q, *r;
    int cnt, i;
    struct addr_pair *apptr;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;

    if(*p == '\0')
	return (struct addr_pair *)0;

    cnt = 1;
    while(p && *p) {
	if(*p == ',')
	    cnt++;
	p++;
    }
    apptr = (struct addr_pair *)malloc((cnt+1) * sizeof(struct addr_pair));
    apptr[cnt].r_dest = apptr[cnt].r_router = 0;
    p = bufx;
    for(i = 0; i < cnt; i++) {
	q = strtok(p, ",");
	r = q;
	while(r && (*r != '\0') && (*r != '-') ) r++;
	if(*r == '-') {
	    *r = '\0';
	    r++;
	}
        if( (*q == '0') && (*(q+1) == 'x'))       /* specified as 0x... */
	    apptr[i].r_dest = strtoul(q, NULL, 16);
	else
	    apptr[i].r_dest = inet_addr(q);
        if( (*r == '0') && (*(r+1) == 'x'))       /* specified as 0x... */
	    apptr[i].r_router = strtoul(r, NULL, 16);
	else
	    apptr[i].r_router = inet_addr(r);
	p = NULL;
    }
    return apptr;
}

static int
getInt(char *str)
{
    char *p;
    int p_int;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;

    if(*p == '\0')
	return 0;
    p_int = atoi(p);

    return p_int;
}

static int*
getIntList(char *str)
{
    char *p, *q;
    int *p_int;
    int cnt, i;
    
    indexPastColonAndRemoveSpaces(str);
    p = bufx;

    if(*p == '\0')
	return 0;

    cnt = 1;
    while(p && *p) {
	if(*p == ',')
	    cnt++;
	p++;
    }
    p_int = (int*)malloc((cnt+1) * sizeof(int));
    p_int[cnt] = 0;
    p = bufx;
    for(i = 0; i < cnt; i++) {
	q = strtok(p, ",");
        if( (*q == '0') && (*(q+1) == 'x'))	/* hex number */
	    p_int[i] = strtol(q, NULL, 16);
	else
	    p_int[i] = atoi(q);
	p = NULL;
    }
    return p_int;
}

static u_long
getUnsignedLong(char *str, int is_addr)
{
    char *p;
    u_long uladdr;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;
    if(*p == '\0')
	return 0;

    if(is_addr) {
	if( (*p == '0') && (*(p+1) == 'x'))	/* hex number */
	    uladdr = strtoul(p, NULL, 16);
	else
	    uladdr = inet_addr(p);
    }
    else {
	if( (*p == '0') && (*(p+1) == 'x'))
	    uladdr = strtoul(p, NULL, 16);
	else
	    uladdr = strtoul(p, NULL, 10);
    }

    return uladdr;
}

static char *
getString(char *str)
{
    char *p;

    indexPastColonAndRemoveSpaces(str);
    p = bufx;

    if(*p == '\0')
	 return (char *)0;
    return strdup(p);
}

static void			/* CHANGE - to update myhostaddr early */
set_myhostaddr(void)
{
    register struct hostent *hp;
    char name[64];

    gethostname(name, sizeof(name));
    if ((hp = gethostbyname(name)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", name, hstrerror(h_errno));
	exit(1);
    }
    myhostaddr.s_addr = *(u_long *)(hp->h_addr_list[0]);
}


static int
free_entry(dhcpConfig *dfg)
{
    if(dfg == 0)
	return 0;
    if(dfg->p_hostpfx)
	free(dfg->p_hostpfx);
    if(dfg->p_addr_range)
	free(dfg->p_addr_range);
    if(dfg->p_router_addr)
	free(dfg->p_router_addr);
    if(dfg->p_tmsserver_addr)
	free(dfg->p_tmsserver_addr);
    if(dfg->p_dnsserver_addr)
	free(dfg->p_dnsserver_addr);
    if(dfg->p_nisserver_addr)
	free(dfg->p_nisserver_addr);
    if(dfg->p_dnsdomain)
	free(dfg->p_dnsdomain);
    if(dfg->p_nisdomain)
	free(dfg->p_nisdomain);
    if(dfg->p_routes)
	free(dfg->p_routes);
    if(dfg->p_logserver_addr)
	free(dfg->p_logserver_addr);
    if(dfg->p_cookieserver_addr)
	free(dfg->p_cookieserver_addr);
    if(dfg->p_LPRserver_addr)
	free(dfg->p_LPRserver_addr);
    if(dfg->p_resourceserver_addr)
	free(dfg->p_resourceserver_addr);
    if(dfg->p_policy_filter)
	free(dfg->p_policy_filter);
    if(dfg->p_pathmtu_table)
	free(dfg->p_pathmtu_table);
    if(dfg->p_NetBIOS_nameserver_addr)
	free(dfg->p_NetBIOS_nameserver_addr);
    if(dfg->p_NetBIOS_distrserver_addr)
	free(dfg->p_NetBIOS_distrserver_addr);
    if(dfg->p_NetBIOS_scope)
	free(dfg->p_NetBIOS_scope);
    if(dfg->p_X_fontserver_addr)
	free(dfg->p_X_fontserver_addr);
    if(dfg->p_X_displaymgr_addr)
	free(dfg->p_X_displaymgr_addr);
    if(dfg->p_nisplusdomain)
	free(dfg->p_nisplusdomain);
    if(dfg->p_nisplusserver_addr)
	free(dfg->p_nisplusserver_addr);
    if(dfg->p_mobileIP_homeagent_addr)
	free(dfg->p_mobileIP_homeagent_addr);
    if(dfg->p_SMTPserver_addr)
	free(dfg->p_SMTPserver_addr);
    if(dfg->p_POP3server_addr)
	free(dfg->p_POP3server_addr);
    if(dfg->p_NNTPserver_addr)
	free(dfg->p_NNTPserver_addr);
    if(dfg->p_WWWserver_addr)
	free(dfg->p_WWWserver_addr);
    if(dfg->p_fingerserver_addr)
	free(dfg->p_fingerserver_addr);
    if(dfg->p_IRCserver_addr)
	free(dfg->p_IRCserver_addr);
    if(dfg->p_StreetTalkserver_addr)
	free(dfg->p_StreetTalkserver_addr);
    if(dfg->p_STDAserver_addr)
	free(dfg->p_STDAserver_addr);
    if(dfg->p_nameserver116_addr)
	free(dfg->p_nameserver116_addr);
    if(dfg->p_impressserver_addr)
	free(dfg->p_impressserver_addr);
    if(dfg->p_merit_dump_pathname)
	free(dfg->p_merit_dump_pathname);
    if(dfg->p_root_disk_pathname)
	free(dfg->p_root_disk_pathname);
    if(dfg->p_extensions_pathname)
	free(dfg->p_extensions_pathname);
    if(dfg->p_NTPserver_addr)
	free(dfg->p_NTPserver_addr);
    if(dfg->p_tftp_server_name)
	free(dfg->p_tftp_server_name);
    if(dfg->p_bootfile_name)
	free(dfg->p_bootfile_name);
    if(dfg->vop_options) 
	dfg->vop_options = NULL; /* freed in free_vop_opts */
#ifdef EDHCP
    if (dfg->ddns_conf)
	dfg->ddns_conf = NULL;	/* freed in free_ddns_root */
#endif

    return 0;
}

int
cf0_free_config_list(dhcpConfig *delptr)
{
    dhcpConfig *cfg, *dfg;

    if(delptr) {
	/* Only delete one individual entry */
	free_entry(delptr);
	free(delptr);
	delptr = NULL;
	return 0;
    }
    cfg = configListPtr;
    while(cfg) {
	dfg = cfg->p_next;
	free_entry(cfg);
	free(cfg);
	cfg = dfg;
    }
    configListPtr = 0;
    return 0;
}

/*
 * parameter l specifies whether to return the new config or to
 * add it to the config list. The returned config is in retptr
 */
static int
add_to_config_list(char *fname, char *netname, int l, dhcpConfig **retptr)
{
    FILE *fp;
    char linebuf[1024];
    dhcpConfig *dhptr;
    dhcpConfig *cfgPtr, **end;
    u_long tul;

    fp = log_fopen(fname, "r");
    if(fp == NULL)
	return ERR_CANNOT_OPEN_CONFIG_FILE;

    if(strcmp(netname, "Default") == 0) {
	dhptr = (dhcpConfig *)malloc(sizeof(dhcpConfig));
	bzero(dhptr, sizeof(dhcpConfig));
	dhptr->p_netnum = 0;
	found_default = 1;
    }
    else if( (tul = inet_addr(netname)) == INADDR_NONE) {
	fclose(fp);
	syslog(LOG_ERR,
	       "Network Number %s incorrect in filename %s",
	       netname, fname);
	return ERR_ILLEGAL_NETWORK_NUMBER;
    }
    else {
	dhptr = (dhcpConfig *)malloc(sizeof(dhcpConfig));
	bzero(dhptr, sizeof(dhcpConfig));
	dhptr->p_netnum = tul;
    }
    dhptr->p_next = NULL;

    while(fgets(linebuf, 1024, fp) != NULL) {
	if( (*linebuf == '\n') || (*linebuf == '#') )
	    continue;
	if(strncmp(linebuf, serve_this_network, 18) == 0) {
	    dhptr->serve_me = getInt(&linebuf[18]);
	    if (dhptr->serve_me == 0) {
		cf0_free_config_list(dhptr);
		return 0;
	    }
	}
	if(strncmp(linebuf, lb_pro_addr_counter, 19) == 0)
	    dhptr->p_addrcnt = getInt(&linebuf[19]);
	if(strncmp(linebuf, lb_pro_netmask, 11) == 0) {
	    dhptr->p_netmask = getUnsignedLong(&linebuf[11], 1);
	    if( (dhptr->p_netmask == INADDR_NONE) ||
	        (dhptr->p_netmask == INADDR_ANY) ) {
		/* Use default netmask for class. */
		if (IN_CLASSA(dhptr->p_netnum)) {
		    dhptr->p_netmask = IN_CLASSA_NET;
		} else if (IN_CLASSB(dhptr->p_netnum)) {
		    dhptr->p_netmask = IN_CLASSB_NET;
		} else if (IN_CLASSC(dhptr->p_netnum)) {
		    dhptr->p_netmask = IN_CLASSC_NET;
		} else if (IN_CLASSD(dhptr->p_netnum)) {
		    dhptr->p_netmask = IN_CLASSD_NET;
		} else {
		    dhptr->p_netmask = 0;
		}
		if (debug && dhptr->p_netnum)
		    syslog(LOG_DEBUG,
			"null netmask in config file %s, using 0x%x",
			fname, dhptr->p_netmask);

	    }
	}
	else if(strncmp(linebuf, lb_pro_lease, 9) == 0)
	    dhptr->p_lease = getUnsignedLong(&linebuf[9], 0);
	else if(strncmp(linebuf, lb_pro_propel_server, 17) == 0) {
	    dhptr->p_propel_addr = getUnsignedLong(&linebuf[17], 1);
	    if(dhptr->p_propel_addr == INADDR_NONE)
		dhptr->p_propel_addr = 0;
	}
	else if(strncmp(linebuf, lb_pro_host_prefix, 15) == 0)
	    dhptr->p_hostpfx = getString(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_host_pfxcnt, 20) == 0)
	    dhptr->p_hstpfxcnt = getInt(&linebuf[20]);
	else if(strncmp(linebuf, lb_pro_choose_name, 15) == 0)
	    dhptr->p_choose_name = getInt(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_ipaddress_range, 19) == 0)
	    dhptr->p_addr_range = getAddressRanges(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_router_addr, 15) == 0)
	    dhptr->p_router_addr = getAddressList(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_timeserver_addr, 19) == 0)
	    dhptr->p_tmsserver_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_dnsserver_addr, 18) == 0)
	    dhptr->p_dnsserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_nisserver_addr, 18) == 0)
	    dhptr->p_nisserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_dns_domain, 14) == 0)
	    dhptr->p_dnsdomain = getString(&linebuf[14]);
	else if(strncmp(linebuf, lb_pro_nis_domain, 14) == 0)
	    dhptr->p_nisdomain = getString(&linebuf[14]);
	else if(strncmp(linebuf, lb_pro_mtu, 7) == 0)
	    dhptr->p_mtu = getInt(&linebuf[7]);
	else if(strncmp(linebuf, lb_pro_allnets_local, 17) == 0)
	    dhptr->p_allnetsloc = getInt(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_broadcast, 13) == 0)
	    dhptr->p_broadcast = getUnsignedLong(&linebuf[13], 1);
	else if(strncmp(linebuf, lb_pro_domask_disc, 15) == 0)
	    dhptr->p_domaskdisc = getInt(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_resp_mask_req, 17) == 0)
	    dhptr->p_respmaskreq = getInt(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_do_router_disc, 18) == 0)
	    dhptr->p_router_disc = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_router_solicit_addr, 23) == 0)
	    dhptr->p_router_solicit_addr = getUnsignedLong(&linebuf[23], 1);
	else if(strncmp(linebuf, lb_pro_static_routes, 17) == 0)
	    dhptr->p_routes = getAddressPairs(&linebuf[17]);
	
	/* process new options added */
	else if(strncmp(linebuf, lb_pro_logserver_addr, 18) == 0)
	    dhptr->p_logserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_cookieserver_addr, 21) == 0)
	    dhptr->p_cookieserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_LPRserver_addr, 18) == 0)
	    dhptr->p_LPRserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_resourceserver_addr, 23) == 0)
	    dhptr->p_resourceserver_addr = getAddressList(&linebuf[23]);
	else if(strncmp(linebuf, lb_pro_bootfile_size, 17) == 0)
	    dhptr->p_bootfile_size = getInt(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_swapserver_addr, 19) == 0)
	    dhptr->p_swapserver_addr = getUnsignedLong(&linebuf[19], 1);
	else if(strncmp(linebuf, lb_pro_IPforwarding, 16) == 0)
	    dhptr->p_IPforwarding = getInt(&linebuf[16]);
	else if(strncmp(linebuf, lb_pro_source_routing, 18) == 0)
	    dhptr->p_source_routing = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_policy_filter, 17) == 0)
	    dhptr->p_policy_filter = getAddressPairs(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_max_reassy_size, 19) == 0)
	    dhptr->p_max_reassy_size = getInt(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_IP_ttl, 11) == 0)
	    dhptr->p_IP_ttl = getInt(&linebuf[11]);
	else if(strncmp(linebuf, lb_pro_pathmtu_timeout, 19) == 0)
	    dhptr->p_pathmtu_timeout = getUnsignedLong(&linebuf[19], 0);
	else if(strncmp(linebuf, lb_pro_pathmtu_table, 17) == 0)
	    dhptr->p_pathmtu_table = getIntList(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_trailer_encaps, 18) == 0)
	    dhptr->p_trailer_encaps = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_arpcache_timeout, 20) == 0)
	    dhptr->p_arpcache_timeout = getUnsignedLong(&linebuf[19], 0);
	else if(strncmp(linebuf, lb_pro_ether_encaps, 16) == 0)
	    dhptr->p_ether_encaps = getInt(&linebuf[16]);
	else if(strncmp(linebuf, lb_pro_TCP_ttl, 11) == 0)
	    dhptr->p_TCP_ttl = getInt(&linebuf[11]);
	else if(strncmp(linebuf, lb_pro_TCP_keepalive_intrvl, 24) == 0)
	    dhptr->p_TCP_keepalive_intrvl = getUnsignedLong(&linebuf[24], 0);
	else if(strncmp(linebuf, lb_pro_TCP_keepalive_garbage, 25) == 0)
	    dhptr->p_TCP_keepalive_garbage = getInt(&linebuf[25]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_nameserver_addr, 27) == 0)
	    dhptr->p_NetBIOS_nameserver_addr = getAddressList(&linebuf[27]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_distrserver_addr, 28) == 0)
	    dhptr->p_NetBIOS_distrserver_addr = getAddressList(&linebuf[28]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_nodetype, 20) == 0)
	    dhptr->p_NetBIOS_nodetype = getInt(&linebuf[20]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_scope, 17) == 0)
	    dhptr->p_NetBIOS_scope = getString(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_X_fontserver_addr, 21) == 0)
	    dhptr->p_X_fontserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_X_displaymgr_addr, 21) == 0)
	    dhptr->p_X_displaymgr_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_nisplus_domain, 18) == 0)
	    dhptr->p_nisplusdomain = getString(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_nisplusserver_addr, 23) == 0)
	    dhptr->p_nisplusserver_addr = getAddressList(&linebuf[23]);
	else if(strncmp(linebuf, lb_pro_mobileIP_homeagent_addr, 27) == 0)
	    dhptr->p_mobileIP_homeagent_addr = getAddressList(&linebuf[27]);
	else if(strncmp(linebuf, lb_pro_SMTPserver_addr, 19) == 0)
	    dhptr->p_SMTPserver_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_POP3server_addr, 19) == 0)
 	    dhptr->p_POP3server_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_NNTPserver_addr, 19) == 0)
	    dhptr->p_NNTPserver_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_WWWserver_addr, 18) == 0)
	    dhptr->p_WWWserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_fingerserver_addr, 21) == 0)
	    dhptr->p_fingerserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_IRCserver_addr, 18) == 0)
	    dhptr->p_IRCserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_StreetTalkserver_addr, 25) == 0)
	    dhptr->p_StreetTalkserver_addr = getAddressList(&linebuf[25]);
	else if(strncmp(linebuf, lb_pro_STDAserver_addr, 19) == 0)
	    dhptr->p_STDAserver_addr = getAddressList(&linebuf[19]);
	else if (strncmp(linebuf, lb_pro_time_offset, 15) == 0)
	    dhptr->p_time_offset = getInt(&linebuf[15]);
	else if (strncmp(linebuf, lb_pro_nameserver116_addr, 22) == 0)
	    dhptr->p_nameserver116_addr = getAddressList(&linebuf[22]);
	else if (strncmp(linebuf, lb_pro_impressserver_addr, 22) == 0)
	    dhptr->p_impressserver_addr = getAddressList(&linebuf[22]);
	else if (strncmp(linebuf, lb_pro_meritdump_pathname, 22) == 0)
	    dhptr->p_merit_dump_pathname = getString(&linebuf[22]);
	else if (strncmp(linebuf, lb_pro_root_pathname, 17) == 0)
	    dhptr->p_root_disk_pathname = getString(&linebuf[17]);
	else if (strncmp(linebuf, lb_pro_extensions_pathname, 23) == 0)
	    dhptr->p_extensions_pathname = getString(&linebuf[23]);
	else if (strncmp(linebuf, lb_pro_NTPserver_addr, 18) == 0)
	    dhptr->p_NTPserver_addr = getAddressList(&linebuf[18]);
	else if (strncmp(linebuf, lb_pro_TFTPserver_name, 19) == 0)
	    dhptr->p_tftp_server_name = getString(&linebuf[19]);
	else if (strncmp(linebuf, lb_pro_bootfile_name, 17) == 0)
	    dhptr->p_bootfile_name = getString(&linebuf[17]);
    }
    fclose(fp);
    if(strcmp(netname, "Default") == 0) { /* CHANGE to set netnum */
	if (!myhostaddr.s_addr)
	    set_myhostaddr();
	if(dhptr->p_netmask == 0)
	    dhptr->p_netmask = mynetmask;
	dhptr->p_netnum = dhptr->p_netmask & myhostaddr.s_addr;
	if(configListPtr == 0) {
	    configListPtr = dhptr;
	    return 0;
	}
	cfgPtr = configListPtr;
	while(cfgPtr) {
	    if(cfgPtr->p_netnum == dhptr->p_netnum ) {
		/* Free up memory and return */
		cf0_free_config_list(dhptr);
		return 0;
	    }
	    cfgPtr = cfgPtr->p_next;
	}
    }
    /*
    ** Must be appended to list instead of prepended since we specifically
    ** place default at end.
    */
    if (l) {			/* add to list */
	for (end = &configListPtr, cfgPtr = configListPtr; cfgPtr;
	     end = &cfgPtr->p_next, cfgPtr = cfgPtr->p_next);
	*end = dhptr;
    }
    else {			/* return it in argument */
	if (retptr)
	    *retptr = dhptr;
    }
    if ( (dhptr->p_lease > 0) && 
	 (dhptr->p_lease < retry_stolen_timeout) )
	retry_stolen_timeout = dhptr->p_lease;
    return 0;
}

/*
 * set the default values in a new config structure
 */
void
set_default_config_values(dhcpConfig *dhptr)
{
    char *nisdm;

    dhptr->serve_me = 1;
    dhptr->p_addrcnt = 1;
    dhptr->p_hstpfxcnt = 1;
    dhptr->p_lease = 0x5a39a80;				/* 3 years */
    dhptr->p_propel_addr = 0;
    dhptr->p_hostpfx = strdup("iris");
    dhptr->p_choose_name = 0;
    dhptr->p_addr_range = (struct addr_range *)
	malloc(sizeof(struct addr_range)*2);
    bzero(dhptr->p_addr_range, sizeof(struct addr_range)*2);
    dhptr->p_addr_range->r_low = 1;
    dhptr->p_addr_range->r_high = 254;
    dhptr->p_router_addr = (u_long *)0;		
    dhptr->p_tmsserver_addr = (u_long *)0;		
    dhptr->p_dnsserver_addr = dh0_get_dnsServers();
    dhptr->p_nisserver_addr = 0;
    dhptr->p_nisdomain = (char *)0;
    if(nha0_haveNfsOption()) {
	if(nisdm = dh0_getNisDomain()) {
	    dhptr->p_nisdomain = nisdm;
	}
    }
    dhptr->p_nisserver_addr = 0;
    dhptr->p_dnsdomain = dh0_getDnsDomain();
    dhptr->p_mtu = 0;
    dhptr->p_allnetsloc = 0;
    dhptr->p_domaskdisc = 0;
    dhptr->p_respmaskreq = 0;
    dhptr->p_routes = 0;
}

static int
create_default_entry(void)
{
    dhcpConfig *dhptr, *cfgPtr, **end;
    char *ifname;
    int mtu;
    int rc;
    u_long myaddr[2];

    /* CHANGES - myhostaddr was set much later*/
    if (!myhostaddr.s_addr)
	set_myhostaddr();
    ifname = nha0_get_primary_interface_name(myhostaddr.s_addr);
    rc = nha0_get_kmem_data(ifname, &mtu);
    myaddr[0] = myhostaddr.s_addr;
    myaddr[1] = 0;

    dhptr = (dhcpConfig *)malloc(sizeof(dhcpConfig));
    bzero(dhptr, sizeof(dhcpConfig));
    set_default_config_values(dhptr);
    dhptr->p_netmask = nha0_getSubnetmask(ifname);
    dhptr->p_netnum = dhptr->p_netmask & myhostaddr.s_addr;
    dhptr->p_router_addr = (u_long *)malloc(sizeof(u_long) *2);
    bcopy(myaddr, dhptr->p_router_addr, sizeof(u_long)*2);
    dhptr->p_tmsserver_addr = (u_long *)malloc(sizeof(u_long)*2);
    bcopy(myaddr, dhptr->p_tmsserver_addr, sizeof(u_long)*2);
    if(rc == 0) {
	dhptr->p_mtu = mtu;
    }
    dhptr->p_broadcast = nha0_getBroadcastAddr(ifname);

    /*
    ** Must be appended to list instead of prepended since we specifically
    ** place default at end.
    */
    for (end = &configListPtr, cfgPtr = configListPtr; cfgPtr;
      end = &cfgPtr->p_next, cfgPtr = cfgPtr->p_next);
    *end = dhptr;
    return 0;
}

int
cf0_initialize_config(void)
{
    DIR *dirp;
    struct direct *dp;
    int rc;
    char nbuf[512];
    char *p;
    int default_config_found = 0;

    dirp = opendir(useConfigDir);
    if(dirp == NULL)
	return ERR_CANNOT_OPEN_CONFIG_DIR;

    configListPtr = (dhcpConfig *)NULL;

    rc = 0;
    while((dp = readdir(dirp)) != NULL) {
	if( (dp->d_namlen > 9) && (strncmp(dp->d_name, "config.", 7) == 0) ) {
	    sprintf(nbuf, "%s/%s", useConfigDir, dp->d_name);
	    p = dp->d_name;
	    if(strcmp(p, "config.Default") == 0) {
		default_config_found = 1;
		continue;
	    }
	    if(rc = add_to_config_list(nbuf, p+7, 1, NULL)) {
		cf0_free_config_list(0);
		break;
	    }
	}
    }
    if (rc != 0) {
        closedir(dirp);
	return rc;
    }

    /* Add it at the end, when the list is complete so you can traverse it */
    if(default_config_found) {
	sprintf(nbuf, "%s/config.Default", useConfigDir);
	if(rc = add_to_config_list(nbuf, "Default", 1, NULL)) {
	    cf0_free_config_list(0);
	}
    }
    closedir(dirp);
    if( (configListPtr == 0) && (found_default == 0) )
	create_default_entry();
    /* if there is no configuration then don't be a DHCP server */
    if (configListPtr == 0) {
	ProclaimServer = 0;
	syslog(LOG_ERR, "DHCP Server disabled, check config files!");
    }
    /* initialize configuration from LDAP backend */
#ifdef EDHCP_LDAP
    cf0_initialize_config_ldap();
#endif
    cf0_initialize_vendor_class();
    return rc;
}

int
cf0_reread_config()
{
    free_vendor_options();
    cf0_free_config_list(0);
    return(cf0_initialize_config());
}



dhcpConfig *
cf0_get_config(u_long addr, int flag)
{
    dhcpConfig *cfgPtr, *defPtr;
#ifdef EDHCP_LDAP
    dhcpConfig *ldap_config_ptr;
#endif
    u_long mltaddr;
    int found_cfg = 0;
    
    defPtr = cfgPtr = configListPtr;
    while(cfgPtr) {
	if((addr & cfgPtr->p_netmask)==(cfgPtr->p_netnum & cfgPtr->p_netmask)){
	    defPtr = cfgPtr;
	    found_cfg = 1;
	    break;
	}
	cfgPtr = cfgPtr->p_next;
    }
    if (0 == found_cfg) {
	/*
	 * there must be a matching configuration (a default is not
	 * enough) - clients that got addresses via relays from this
	 * server will continue to renew/rebind even if the server
	 * has stopped serving the network of the client
	 * for compatibility the default network use is retained
	 */
	mltaddr = MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
	defPtr = cfgPtr = configListPtr;
	while(cfgPtr) {
	    if (cfgPtr->p_netnum == (cfgPtr->p_netmask & mltaddr)) {
		defPtr = cfgPtr;
		found_cfg = 1;
		break;
	    }
	    cfgPtr = cfgPtr->p_next;
	}
    }
    if(defPtr->serve_me == 0) /*an extra check*/
	return 0;
    else{			/*only the configuration for the network*/ 
	if(found_cfg) {		/*on which the request came in on should*/
				/*be returned; else return NULL         */
#ifdef EDHCP_LDAP	    
	    if (1 == flag) {
		ldap_config_ptr = get_dhcpConfig_ldap(defPtr, addr);
		if (ldap_config_ptr)
		    return ldap_config_ptr;
	    }
#endif
	    return defPtr;    
	}
	return 0;
    }
}

int
req_addr_in_range(dhcpConfig *cfgPtr, u_long req_addr)
{
    struct addr_range *adr;
    u_long nnum;
    u_long hnum;
    int i;
    int ret;
    ret = 0;
    adr = cfgPtr->p_addr_range;
    if (adr != 0) {		/* if no ranges return 0 */
	if(cfgPtr->p_netnum == 0)
	    nnum = req_addr & cfgPtr->p_netmask;
	else {
	    nnum = cfgPtr->p_netnum;
	}
	hnum = req_addr - nnum + (cfgPtr->p_addr_range->r_low & cfgPtr->p_netmask);	/* get host number */

	i = 0;
	while( (adr[i].r_low != 0) && (adr[i].r_high != 0) ) {
	    if (hnum >= adr[i].r_low && hnum <= adr[i].r_high) {
		ret = 1;
		break;
	    }
	    i++;
	}
    }
    return ret;
}
    
    
int
cf0_is_req_addr_valid(u_long req_addr, u_long netaddr)
{
    dhcpConfig *cfgPtr;

    cfgPtr = configListPtr;
    while(cfgPtr) { /* change at HP */
      if(((netaddr & cfgPtr->p_netmask)==(cfgPtr->p_netnum & cfgPtr->p_netmask))&&
	 ((req_addr & cfgPtr->p_netmask)==(cfgPtr->p_netnum & cfgPtr->p_netmask))){
	/* we must still check if its in the range */
	if (req_addr_in_range(cfgPtr, req_addr))
	    return 0;		/* Valid */
	else
	    return 2;		/* Not in range but in subnet */
      }
      cfgPtr = cfgPtr->p_next;
    }
    return 1;
}

static char *
get_next_available_name(dhcpConfig *cfgPtr,char *cid_ptr, int cid_length,
			EtherAddr *eth, char *desired_name, DBM *db)
{
    char nm[MAXHOSTNAMELEN+1], *nm1;
    char hsnm[MAXHOSTNAMELEN];
    char *client_domain = (char *)0;
    int cnt, found, first_time;

    if (cfgPtr->p_dnsdomain) {
	client_domain = cfgPtr->p_dnsdomain;
    }
    else if (cfgPtr->p_nisdomain){
	client_domain = cfgPtr->p_nisdomain;
    }
    else{
	client_domain = mydomain;
    }
    first_time = 1;
    found = 0;
    cnt = cfgPtr->p_hstpfxcnt;
    while(!found) {
	if (first_time && desired_name && *desired_name) {
	    strcpy(nm, desired_name);
	}
	else
	    sprintf(nm, "%s%d", cfgPtr->p_hostpfx, cnt);
	if (client_domain) {
	    sprintf(hsnm,"%s.%s", nm, client_domain);
	}
	if(loc0_is_hostname_assigned(nm, db)) {
	    if (!first_time) 
	        cnt++;
	    else
	        first_time = 0;
	    continue;
	}
	if (client_domain && loc0_is_hostname_assigned(hsnm, db)) {
	    if (!first_time) 
	        cnt++;
	    else
	        first_time = 0;
	    continue;
	}
	nm1 = loc0_get_hostname_from_cid(cid_ptr, cid_length, eth, db);
	if(nm1 && *nm1 && (strcmp(nm, nm1) == 0) ) {
	    if (!first_time) 
	        cnt++;
	    else
	        first_time = 0;
	    if (nm1)
		free(nm1);
	    continue;
	}
	if (client_domain && nm1 && *nm1 && (strcmp(hsnm, nm1) ==0) ) {
	    if (!first_time) 
	        cnt++;
	    else
	        first_time = 0;
	    if (nm1)
		free(nm1);
	    continue;
	}
	found = 1;
	if (nm1)
	    free(nm1);
    }
    if (desired_name) {
	if (strcmp(nm, desired_name) != 0)
	    cfgPtr->p_hstpfxcnt = cnt + 1;
    }
    else
	cfgPtr->p_hstpfxcnt = cnt + 1;

    if (client_domain)	/* Make sure it is freed later by the callee */
	return(strdup(hsnm));
    else
	return(strdup(nm));
}

static u_long
get_next_available_address(dhcpConfig *cfgPtr, u_long nnum, DBM *db)
{
    int found_r, wrap_flag, cnt, indx, i;
    struct addr_range *adr;
    u_long tmp_ipa;
    struct in_addr ipn;

    found_r = 0;
    wrap_flag = 0;
    cnt = indx = cfgPtr->p_addrcnt;
    if(cnt <= 0)
	cnt = 1;
    adr = cfgPtr->p_addr_range;
    if(adr == 0) {
	ipn.s_addr = nnum;
	syslog(LOG_ERR, "Address range for network %s not specified",
		inet_ntoa(ipn));
	return 0;
    }

    for(;;) {
	i = 0;
	while( (adr[i].r_low != 0) && (adr[i].r_high != 0) ) {
	    if(cnt < adr[i].r_low) {
		cnt++;
		continue;
	    }
	    if(cnt >= adr[i].r_low && cnt <= adr[i].r_high) {
		found_r = 1;
		while(cnt <= adr[i].r_high) {
		    tmp_ipa = nnum - (cfgPtr->p_netmask & cnt) + cnt;
		    if(loc0_is_ipaddress_assigned(tmp_ipa, db)) {
			cnt++;
			if(wrap_flag && (cnt == indx))
			    return 0;
			continue;
		    }
		    cfgPtr->p_addrcnt = cnt+1;
		    return tmp_ipa;
		}
		break;	/* cnt is now out of range, find the range again */
	    }
	    i++;
	}
	if(!found_r) {	/* wrap around */
	    if(!wrap_flag) {
		cnt = 1;
		wrap_flag = 1;
	    }
	    else
		return 0;	/* not found */
	}
	else {
	    found_r = 0;
	}
    }
}

static char*
get_hostname_client(char *cid_ptr, int cid_length, EtherAddr *eth, 
		    u_long ipa, int dont_need_new_hostname, 
		    char *desired_name,
		    dhcpConfig *cfgPtr, DBM *db)
{
    struct hostent *he;
    char *hsname;
    
    if(dont_need_new_hostname) {
	hsname = (char *)
	    loc0_get_hostname_from_cid(cid_ptr, cid_length, eth, db);
	if( (hsname == 0) || (*hsname == '\0') )
	    hsname = 
		get_next_available_name(cfgPtr, cid_ptr, cid_length, 
					eth, desired_name, db);
    }
    else {
	he = gethostbyaddr(&ipa, sizeof(ipa), AF_INET);
	if (he) {
	    hsname = strdup(he->h_name);
	} else {
	    hsname = 
		get_next_available_name(cfgPtr, cid_ptr, cid_length,
					eth, desired_name, db);
	}
    }
    return hsname;
}

int
cf0_get_new_ipaddress(EtherAddr *eth, u_long src_addr, u_long desired,
		      u_long *newaddr, dhcpConfig **cfgPPtr, char *cid_ptr, 
		      int cid_length, DBM *db, char *desired_name)
{
  dhcpConfig *cfgPtr;
  u_long ipa = 0;
  char *hsname, *buf;
  u_long nnum;
  EtherAddr ethlru;
  int lru_cid_len = 0;
  char lru_cid[MAXCIDLEN];
  int dont_need_new_hostname = 0;
  struct in_addr ipn;
  char *client_domain = (char *)0;
  char hostname[MAXHOSTNAMELEN];
  char str[MAXCIDLEN];
  int two_tries = 0;  /* if we find a duplicate entry in etherToIP
		       * for the address we are trying to assign
		       * we try another time. If the second time
		       * fails we return error (1) */
  long lease_or_inf;
  hsname = (char*)0;
  
  cfgPtr = cf0_get_config(src_addr, 0);
  *cfgPPtr = cfgPtr;
  if(cfgPtr == 0) {
    if (debug >= 2) {
      ipn.s_addr = src_addr;
      syslog(LOG_DEBUG, "Configuration for address %s not found.", 
	     inet_ntoa(ipn));
    }
    return 1;
  }
  if(ipa = loc0_get_ipaddr_from_cid(eth, cid_ptr, cid_length, db)) {
    if(cf0_is_req_addr_valid(ipa, src_addr) == 0) {
      *newaddr = ipa;
      /* we have a valid mapping but we have to make sure there is
       * a mapping in etherToIP */
      dont_need_new_hostname = 1;
      goto validate_etherToIP;
    }
    if (!alt_naming_flag) /* want to retain the ethers map for non -x */
      dont_need_new_hostname = 1;
  }
try_another_time:
  if (desired && ! loc0_is_ipaddress_assigned(desired, db) &&
      (cf0_is_req_addr_valid(desired, src_addr) == 0)) {
    ipa = desired;
  }
  else {
    if(cfgPtr->p_netnum == 0)
      nnum = src_addr & cfgPtr->p_netmask;
    else
      nnum = cfgPtr->p_netnum;
    ipa = get_next_available_address(cfgPtr, nnum, db);
    if (alt_naming_flag)
      dont_need_new_hostname = 0;
  }
  *newaddr = ipa;
validate_etherToIP:
  if(ipa == 0) {		/* try to use expired entry */
    bzero(&ethlru, sizeof(EtherAddr));
    if (loc0_scan_find_lru_entry(cfgPtr, &ethlru, &ipa, &hsname, 
				 lru_cid, & lru_cid_len, db) == 0) {
      /* found, need to comment out this entry in the etherToIP,
       * hosts, and ethers file.
       */
      if (debug) {
	ipn.s_addr = ipa;
	syslog(LOG_DEBUG,
	       "Lease for address %s has expired. Reusing address.",
	       inet_ntoa(ipn));
      }
      loc0_expire_entry(&ethlru, lru_cid, lru_cid_len, ipa, db);
      if(cf0_is_req_addr_valid(ipa, src_addr) == 0) {
	  *newaddr = ipa;
      }
      else {
	  ipa = 0;
	  goto validate_etherToIP;
      }
      if ( (hsname == NULL) || (*hsname == '\0') ) 
	  hsname = get_hostname_client(cid_ptr, cid_length, eth, ipa,
				       dont_need_new_hostname, 
				       desired_name,
				       cfgPtr, db);
    }
    else {
	mk_str(1, cid_ptr, cid_length, str);
	ipn.s_addr = cfgPtr->p_netnum;
	syslog(LOG_WARNING, "All IP addresses in use - Network %s."
	       "No addr for Cid=%s, mac=%s", 
	       inet_ntoa(ipn), str, ether_ntoa(eth));
	return 1;
    }
  }
  else {
      hsname = get_hostname_client(cid_ptr, cid_length, eth, ipa,
				   dont_need_new_hostname, 
				   desired_name,
				   cfgPtr, db);
  }
  if (alt_naming_flag) {
    dh0_remove_system_mapping(cid_ptr, eth, cid_length, db);
    sys0_removeFromHostsFile(ipa);
  }

  lease_or_inf = loc0_get_lease(ipa, db);
  if (lease_or_inf >= 0)
    lease_or_inf = 0;
  loc0_remove_entry(eth, 3, cid_ptr, cid_length, db);/* remove it if one exists already */
  if (debug) {
    if (loc0_is_etherToIP_duplicate(cid_ptr, cid_length, eth, ipa, hsname, db)){
      if (two_tries == 0) {
	desired = 0;	/* a fresh approach makes this 0 */
	ipa = 0;
	dont_need_new_hostname = 0;
	if (hsname)
	  free(hsname);
	two_tries = 1;
	goto try_another_time;
      }
      else {
	if (hsname)
	  free(hsname);
	return 1;
      }
    }
  }
  if (ping_blocking_check) {
      if (send_blocking_ping(ping_sd, ipa, 1, 0, 1) >= 0) {
	  ipn.s_addr = ipa;
	  two_tries++;
	  syslog(LOG_WARNING, "Address %s appears to be STOLEN: tries(%d)", 
		 inet_ntoa(ipn), two_tries);
	  if (two_tries < 10) {
	      desired = 0;	/* a fresh approach makes this 0 */
	      ipa = 0;
	      dont_need_new_hostname = 0;
	      if (hsname)
		  free(hsname);
	      goto try_another_time;
	  }
	  else {
	      if (hsname)
		  free(hsname);
	      return 1;
	  }
      }
  }
  if (debug){
      buf = getRecByCid(0, cid_ptr, cid_length, db);
      if ((buf != NULL) && (lease_or_inf != STATIC_LEASE))
	  syslog(LOG_DEBUG,"Trying to create new entry with used addr: %s", buf);
  }
  /* process requested hostname from client */
  if (!alt_naming_flag && desired_name && *desired_name && 
      (lease_or_inf != STATIC_LEASE) &&
      (strcmp(hsname, desired_name) != 0)) {
      if (debug >= 3) {
	  syslog(LOG_DEBUG, "Requested name is %s", desired_name);
      }
      if (cfgPtr->p_dnsdomain) {
	  client_domain = cfgPtr->p_dnsdomain;
      }
      else if (cfgPtr->p_nisdomain){
	  client_domain = cfgPtr->p_nisdomain;
      }
      else{
	  client_domain = mydomain;
      }
      if (client_domain) {
	  sprintf(hostname,"%s.%s", desired_name, client_domain);
      }
      if (strcmp(hsname, hostname) != 0) {
	  if (loc0_is_hostname_assigned(hostname, db) == 0) {
	      free(hsname);
	      hsname = strdup(desired_name);
	  }
	  else {
	      if (debug >= 3) {/* name is taken */
		  syslog(LOG_DEBUG, "Requested name %s is already taken",
			 hostname);
	      }
	  }
      }
  }
  loc0_create_new_entry_l(cid_ptr, cid_length, eth, ipa, hsname, 
			  lease_or_inf, db);
  free(hsname);
  return 0;
}

rfc1533opts *
get_rfc1533_config(void)
{
    FILE *fp;
    char linebuf[1024];
    rfc1533opts *rfcptr;

    fp = log_fopen(rfc1533configFile, "r");
    if(fp == NULL)
	return 0;

    rfcptr = (rfc1533opts *)malloc(sizeof(rfc1533opts));
    bzero(rfcptr, sizeof(rfc1533opts));

    while(fgets(linebuf, 1024, fp) != NULL) {
	if( (*linebuf == '\n') || (*linebuf == '#') )
	    continue;
	if(strncmp(linebuf, lb_pro_netmask, 11) == 0) {
	    rfcptr->p_netmask = getUnsignedLong(&linebuf[11], 1);
	    if(rfcptr->p_netmask == INADDR_NONE)
		rfcptr->p_netmask = 0;
	}
	else if (strncmp(linebuf, lb_pro_time_offset, 15) == 0)
	    rfcptr->p_time_offset = getInt(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_router_addr, 15) == 0)
	    rfcptr->p_router_addr = getAddressList(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_timeserver_addr, 19) == 0)
	    rfcptr->p_tmsserver_addr = getAddressList(&linebuf[19]);
	else if (strncmp(linebuf, lb_pro_nameserver116_addr, 22) == 0)
	    rfcptr->p_nameserver116_addr = getAddressList(&linebuf[22]);
	else if(strncmp(linebuf, lb_pro_dnsserver_addr, 18) == 0)
	    rfcptr->p_dnsserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_logserver_addr, 18) == 0)
	    rfcptr->p_logserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_cookieserver_addr, 21) == 0)
	    rfcptr->p_cookieserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_LPRserver_addr, 18) == 0)
	    rfcptr->p_LPRserver_addr = getAddressList(&linebuf[18]);
	else if (strncmp(linebuf, lb_pro_impressserver_addr, 22) == 0)
	    rfcptr->p_impressserver_addr = getAddressList(&linebuf[22]);
	else if(strncmp(linebuf, lb_pro_resourceserver_addr, 23) == 0)
	    rfcptr->p_resourceserver_addr = getAddressList(&linebuf[23]);
	else if(strncmp(linebuf, lb_pro_bootfile_size, 17) == 0)
	    rfcptr->p_bootfile_size = getInt(&linebuf[17]);
	else if (strncmp(linebuf, lb_pro_meritdump_pathname, 22) == 0)
	    rfcptr->p_merit_dump_pathname = getString(&linebuf[22]);
	else if(strncmp(linebuf, lb_pro_dns_domain, 14) == 0)
	    rfcptr->p_dnsdomain = getString(&linebuf[14]);
	else if(strncmp(linebuf, lb_pro_swapserver_addr, 19) == 0)
	    rfcptr->p_swapserver_addr = getUnsignedLong(&linebuf[19], 1);
	else if (strncmp(linebuf, lb_pro_root_pathname, 17) == 0)
	    rfcptr->p_root_disk_pathname = getString(&linebuf[17]);
	else if (strncmp(linebuf, lb_pro_extensions_pathname, 23) == 0)
	    rfcptr->p_extensions_pathname = getString(&linebuf[23]);
	else if(strncmp(linebuf, lb_pro_IPforwarding, 16) == 0)
	    rfcptr->p_IPforwarding = getInt(&linebuf[16]);
	else if(strncmp(linebuf, lb_pro_source_routing, 18) == 0)
	    rfcptr->p_source_routing = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_policy_filter, 17) == 0)
	    rfcptr->p_policy_filter = getAddressPairs(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_max_reassy_size, 19) == 0)
	    rfcptr->p_max_reassy_size = getInt(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_IP_ttl, 11) == 0)
	    rfcptr->p_IP_ttl = getInt(&linebuf[11]);
	else if(strncmp(linebuf, lb_pro_pathmtu_timeout, 19) == 0)
	    rfcptr->p_pathmtu_timeout = getUnsignedLong(&linebuf[19], 0);
	else if(strncmp(linebuf, lb_pro_pathmtu_table, 17) == 0)
	    rfcptr->p_pathmtu_table = getIntList(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_mtu, 7) == 0)
	    rfcptr->p_mtu = getInt(&linebuf[7]);
	else if(strncmp(linebuf, lb_pro_allnets_local, 17) == 0)
	    rfcptr->p_allnetsloc = getInt(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_broadcast, 13) == 0)
	    rfcptr->p_broadcast = getUnsignedLong(&linebuf[13], 1);
	else if(strncmp(linebuf, lb_pro_domask_disc, 15) == 0)
	    rfcptr->p_domaskdisc = getInt(&linebuf[15]);
	else if(strncmp(linebuf, lb_pro_resp_mask_req, 17) == 0)
	    rfcptr->p_respmaskreq = getInt(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_do_router_disc, 18) == 0)
	    rfcptr->p_router_disc = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_router_solicit_addr, 23) == 0)
	    rfcptr->p_router_solicit_addr = getUnsignedLong(&linebuf[23], 1);
	else if(strncmp(linebuf, lb_pro_static_routes, 17) == 0)
	    rfcptr->p_routes = getAddressPairs(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_trailer_encaps, 18) == 0)
	    rfcptr->p_trailer_encaps = getInt(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_arpcache_timeout, 20) == 0)
	    rfcptr->p_arpcache_timeout = getUnsignedLong(&linebuf[19], 0);
	else if(strncmp(linebuf, lb_pro_ether_encaps, 16) == 0)
	    rfcptr->p_ether_encaps = getInt(&linebuf[16]);
	else if(strncmp(linebuf, lb_pro_TCP_ttl, 11) == 0)
	    rfcptr->p_TCP_ttl = getInt(&linebuf[11]);
	else if(strncmp(linebuf, lb_pro_TCP_keepalive_intrvl, 24) == 0)
	    rfcptr->p_TCP_keepalive_intrvl = getUnsignedLong(&linebuf[24], 0);
	else if(strncmp(linebuf, lb_pro_TCP_keepalive_garbage, 25) == 0)
	    rfcptr->p_TCP_keepalive_garbage = getInt(&linebuf[25]);
	else if(strncmp(linebuf, lb_pro_nis_domain, 14) == 0)
	    rfcptr->p_nisdomain = getString(&linebuf[14]);
	else if(strncmp(linebuf, lb_pro_nisserver_addr, 18) == 0)
	    rfcptr->p_nisserver_addr = getAddressList(&linebuf[18]);
	else if (strncmp(linebuf, lb_pro_NTPserver_addr, 18) == 0)
	    rfcptr->p_NTPserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_nameserver_addr, 27) == 0)
	    rfcptr->p_NetBIOS_nameserver_addr = getAddressList(&linebuf[27]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_distrserver_addr, 28) == 0)
	    rfcptr->p_NetBIOS_distrserver_addr = getAddressList(&linebuf[28]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_nodetype, 20) == 0)
	    rfcptr->p_NetBIOS_nodetype = getInt(&linebuf[20]);
	else if(strncmp(linebuf, lb_pro_NetBIOS_scope, 17) == 0)
	    rfcptr->p_NetBIOS_scope = getString(&linebuf[17]);
	else if(strncmp(linebuf, lb_pro_X_fontserver_addr, 21) == 0)
	    rfcptr->p_X_fontserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_X_displaymgr_addr, 21) == 0)
	    rfcptr->p_X_displaymgr_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_nisplus_domain, 18) == 0)
	    rfcptr->p_nisplusdomain = getString(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_nisplusserver_addr, 23) == 0)
	    rfcptr->p_nisplusserver_addr = getAddressList(&linebuf[23]);
	else if(strncmp(linebuf, lb_pro_mobileIP_homeagent_addr, 27) == 0)
	    rfcptr->p_mobileIP_homeagent_addr = getAddressList(&linebuf[27]);
	else if(strncmp(linebuf, lb_pro_SMTPserver_addr, 19) == 0)
	    rfcptr->p_SMTPserver_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_POP3server_addr, 19) == 0)
	    rfcptr->p_POP3server_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_NNTPserver_addr, 19) == 0)
	    rfcptr->p_NNTPserver_addr = getAddressList(&linebuf[19]);
	else if(strncmp(linebuf, lb_pro_WWWserver_addr, 18) == 0)
	    rfcptr->p_WWWserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_fingerserver_addr, 21) == 0)
	    rfcptr->p_fingerserver_addr = getAddressList(&linebuf[21]);
	else if(strncmp(linebuf, lb_pro_IRCserver_addr, 18) == 0)
	    rfcptr->p_IRCserver_addr = getAddressList(&linebuf[18]);
	else if(strncmp(linebuf, lb_pro_StreetTalkserver_addr, 25) == 0)
	    rfcptr->p_StreetTalkserver_addr = getAddressList(&linebuf[25]);
	else if(strncmp(linebuf, lb_pro_STDAserver_addr, 19) == 0)
	    rfcptr->p_STDAserver_addr = getAddressList(&linebuf[19]);
    }
    fclose(fp);
    return rfcptr;
}

#ifdef EDHCP_LDAP
/* get value of an option from the LDAP configuration
 */
void getOptVals(char *option, int *optID, char *optVal)
{
    char *tmp;

    *optID = -1;
    *optVal = NULL;

    tmp = strtok(option, " \t");
    if (!tmp) 
	return;
    *optID = atoi(tmp);
    tmp = strtok(0, " \t");
    if (!tmp) 
	return;
    strcpy(optVal, tmp); 
}

/*
 * update SUBnet configuration from LDAP options
 */
int
update_subnet_options(dhcpConfig *dhptr, char **goptions)
{
    char **opt;
    int optID;
    char optVal[256];
    char option[256];

    if (dhptr == NULL) {
	syslog(LOG_DEBUG, "update_subnet_options: dhcpConfig* NULL");
	return 1;
    }
    if ((goptions == NULL) || (*goptions == NULL))
	return 0;
    for (opt = goptions; *opt; opt++) {
	strcpy(option, *opt);
	getOptVals(option, &optID, optVal);
	switch(optID) {
	case PAD_TAG:/*			0*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;
	case SUBNETMASK_TAG:/*		1*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;
	case TIME_OFFSET_TAG:/*		2*/
	    dhptr->p_time_offset = getInt(optVal);
	    break;
	case ROUTER_TAG:/*		3*/
	    if (dhptr->p_router_addr)
		free(dhptr->p_router_addr);
	    dhptr->p_router_addr = getAddressList(optVal);
	    break;
	case TIMESERVERS_TAG:/*		4*/
	    if (dhptr->p_tmsserver_addr)
		free(dhptr->p_tmsserver_addr);
	    dhptr->p_tmsserver_addr = getAddressList(optVal);
	    break;
	case NAME_SERVER116_TAG:/*	5*/
	    if (dhptr->p_nameserver116_addr)
		free(dhptr->p_nameserver116_addr);
	    dhptr->p_nameserver116_addr = getAddressList(optVal);
	    break;
	case DNSSERVERS_TAG:/*		6*/
	    if (dhptr->p_dnsserver_addr)
		free(dhptr->p_dnsserver_addr);
	    dhptr->p_dnsserver_addr = getAddressList(optVal);
	    break;
	case LOGSERVERS_TAG:/*		7*/
	    if (dhptr->p_logserver_addr)
		free(dhptr->p_logserver_addr);
	    dhptr->p_logserver_addr = getAddressList(optVal);
	    break;
	case COOKIESERVERS_TAG:/*	8*/
	    if (dhptr->p_cookieserver_addr)
		free(dhptr->p_cookieserver_addr);
	    dhptr->p_cookieserver_addr = getAddressList(optVal);
	    break;
	case LPRSERVERS_TAG:/*		9*/
	    if (dhptr->p_LPRserver_addr)
		free(dhptr->p_LPRserver_addr);
	    dhptr->p_LPRserver_addr = getAddressList(optVal);
	    break;
	case IMPRESSSERVERS_TAG:/*	10*/
	    if (dhptr->p_impressserver_addr)
		free(dhptr->p_impressserver_addr);
	    dhptr->p_impressserver_addr = getAddressList(optVal);
	    break;
	case RESOURCESERVERS_TAG:/*	11*/
	    if (dhptr->p_resourceserver_addr)
		free(dhptr->p_resourceserver_addr);
	    dhptr->p_resourceserver_addr = getAddressList(optVal);
	    break;
	case HOSTNAME_TAG:/*		12*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;
	case BOOTFILE_SIZE_TAG:/*	13*/
	    dhptr->p_bootfile_size = getInt(optVal);
	    break;
	case MERITDUMP_FILE_TAG:/*	14*/
	    if (dhptr->p_merit_dump_pathname)
		free(dhptr->p_merit_dump_pathname);
	    dhptr->p_merit_dump_pathname = getString(optVal);
	    break;
	case DNSDOMAIN_NAME_TAG:/*	15*/
	    if (dhptr->p_dnsdomain)
		free(dhptr->p_dnsdomain);
	    dhptr->p_dnsdomain = getString(optVal);
	    break;
	case SWAPSERVER_ADDRESS_TAG:/*	16*/
	    dhptr->p_swapserver_addr = getUnsignedLong(optVal, 1);
	    break;
	case ROOT_PATH_TAG:/*		17*/
	    if (dhptr->p_root_disk_pathname)
		free(dhptr->p_root_disk_pathname);
	    dhptr->p_root_disk_pathname = getString(optVal);
	    break;
	case EXTENSIONS_PATH_TAG:/*	18*/
	    if (dhptr->p_extensions_pathname)
		free(dhptr->p_extensions_pathname);
	    dhptr->p_extensions_pathname = getString(optVal);
	    break;

	    /* IP Layer params per host */

	case IP_FORWARDING_TAG:/*	19*/
	    dhptr->p_IPforwarding = getInt(optVal);
	    break;
	case NON_LOCAL_SRC_ROUTE_TAG:/*	20*/
	    dhptr->p_source_routing = getInt(optVal);
	    break;
	case POLICY_FILTER_TAG:/*	21*/
	    if (dhptr->p_policy_filter)
		free(dhptr->p_policy_filter);
	    dhptr->p_policy_filter = getAddressPairs(optVal);
	    break;
	case MAX_DGRAM_REASSEMBL_TAG:/*	22*/
	    dhptr->p_max_reassy_size = getInt(optVal);
	    break;
	case DEF_IP_TIME_LIVE_TAG:/*	23*/
	    dhptr->p_IP_ttl = getInt(optVal);
	    break;
	case PATH_MTU_AGE_TMOUT_TAG:/*	24*/
	    dhptr->p_pathmtu_timeout = getUnsignedLong(optVal, 0);
	    break;
	case PATH_MTU_PLT_TABLE_TAG:/*	25*/
	    if (dhptr->p_pathmtu_table)
		free(dhptr->p_pathmtu_table);
	    dhptr->p_pathmtu_table = getIntList(optVal);
	    break;

	    /* IP Layer parameters per interface */
	case IF_MTU_TAG:/*		26*/
	    dhptr->p_mtu = getInt(optVal);
	    break;
	case ALL_SUBNETS_LOCAL_TAG:/*	27*/
	    dhptr->p_allnetsloc = getInt(optVal);
	    break;
	case BROADCAST_ADDRESS_TAG:/*	28*/
	    dhptr->p_broadcast = getUnsignedLong(optVal, 1);
	    break;
	case CLIENT_MASK_DISC_TAG:/*	29*/
	    dhptr->p_domaskdisc = getInt(optVal);
	    break;
	case MASK_SUPPLIER_TAG:/*	30*/
	    dhptr->p_respmaskreq = getInt(optVal);
	    break;
	case ROUTER_DISC_TAG:/*		31*/
	    dhptr->p_router_disc = getInt(optVal);
	    break;
	case ROUTER_SOLICIT_ADDR_TAG:/* 32*/
	    dhptr->p_router_solicit_addr = getUnsignedLong(optVal, 1);
	    break;
	case STATIC_ROUTE_TAG:/*	33*/
	    if (dhptr->p_routes)
		free(dhptr->p_routes);
	    dhptr->p_routes = getAddressPairs(optVal);
	    break;
	    /* Link layer params per interface */
	case TRAILER_ENCAPS_TAG:/*	34*/
	    dhptr->p_trailer_encaps = getInt(optVal);
	    break;
	case ARP_CACHE_TIMEOUT_TAG:/*	35*/
	    dhptr->p_arpcache_timeout = getUnsignedLong(optVal, 0);
	    break;
	case ETHERNET_ENCAPS_TAG:/*	36*/
	    dhptr->p_ether_encaps = getInt(optVal);
	    break;


	    /* TCP Parameters */
	case TCP_DEFAULT_TTL_TAG:/*	37*/
	    dhptr->p_TCP_ttl = getInt(optVal);
	    break;

	case TCP_KEEPALIVE_INTVL_TAG:/*	38*/
	    dhptr->p_TCP_keepalive_intrvl = getUnsignedLong(optVal, 0);
	    break;
	case TCP_KEEPALIVE_GARBG_TAG:/*	39*/
	    dhptr->p_TCP_keepalive_garbage = getInt(optVal);
	    break;
	    /* Applicatopn and service parameters */
	case NISDOMAIN_NAME_TAG:/*	40*/
	    if (dhptr->p_nisdomain)
		free(dhptr->p_nisdomain);
	    dhptr->p_nisdomain = getString(optVal);
	    break;
	case NIS_SERVERS_TAG:/*		41*/
	    if (dhptr->p_nisserver_addr)
		free(dhptr->p_nisserver_addr);
	    dhptr->p_nisserver_addr = getAddressList(optVal);
	    break;
	case NTP_SERVERS_TAG:/*		42*/
	    if (dhptr->p_NTPserver_addr)
		free(dhptr->p_NTPserver_addr);
	    dhptr->p_NTPserver_addr = getAddressList(optVal);
	    break;

	    /* Vendor specific extensions */
	case SGI_VENDOR_TAG:/*		43*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;

	    /* NetBIOS */
	case NETBIOS_NMSERV_ADDR_TAG:/* 44*/
	    if (dhptr->p_NetBIOS_nameserver_addr)
		free(dhptr->p_NetBIOS_nameserver_addr);
	    dhptr->p_NetBIOS_nameserver_addr = getAddressList(optVal);
	    break;
	case NETBIOS_DISTR_ADDR_TAG:/*  45*/
	    if (dhptr->p_NetBIOS_distrserver_addr)
		free(dhptr->p_NetBIOS_distrserver_addr);
	    dhptr->p_NetBIOS_distrserver_addr = getAddressList(optVal);
	    break;
	case NETBIOS_NODETYPE_TAG:/*    46*/
	    dhptr->p_NetBIOS_nodetype = getInt(optVal);
	    break;
	case NETBIOS_SCOPE_TAG:/*       47*/
	    if (dhptr->p_NetBIOS_scope)
		free(dhptr->p_NetBIOS_scope);
	    dhptr->p_NetBIOS_scope = getString(optVal);
	    break;
	    /* X window system */
	case X_FONTSERVER_ADDR_TAG:/*   48*/
	    if (dhptr->p_X_fontserver_addr)
		free(dhptr->p_X_fontserver_addr);
	    dhptr->p_X_fontserver_addr = getAddressList(optVal);
	    break;
	case X_DISPLAYMGR_ADDR_TAG:/*   49*/
	    if (dhptr->p_X_displaymgr_addr)
		free(dhptr->p_X_displaymgr_addr);
	    dhptr->p_X_displaymgr_addr = getAddressList(optVal);
	    break;
	    /* DHCP extensions */

	case IPADDR_TAG:/*		50*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;
	case IP_LEASE_TIME_TAG:/*	51*/
	    dhptr->p_lease = getUnsignedLong(optVal, 0);
	    break;
	case OPTION_OVERLOAD_TAG:/*	52*/
	case DHCP_MSG_TAG:/*		53*/
	case DHCP_SERVER_TAG:/*		54*/
	case DHCP_PARAM_REQ_TAG:/*	55*/
	case MAX_DHCP_MSG_SIZE_TAG: /*	57*/
	case RENEW_LEASE_TIME_TAG:/*	58*/
	case REBIND_LEASE_TIME_TAG:/*	59*/
	case DHCP_CLASS_TAG:/*		60*/
	case DHCP_CLIENT_ID_TAG:/*	61*/
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;

	    /* NIS+ */
	case NISPLUS_DOMAIN_TAG:/*	64*/
	    if (dhptr->p_nisplusdomain)
		free(dhptr->p_nisplusdomain);
	    dhptr->p_nisplusdomain = getString(optVal);
	    break;
	case NISPLUS_SERVER_ADDR_TAG:/* 65*/
	    if (dhptr->p_nisplusserver_addr)
		free(dhptr->p_nisplusserver_addr);
	    dhptr->p_nisplusserver_addr = getAddressList(optVal);
	    break;

	case TFTP_SERVER_NAME_TAG:/*    66*/
	    if (dhptr->p_tftp_server_name)
		free(dhptr->p_tftp_server_name);
	    dhptr->p_tftp_server_name = getString(optVal);
	    break;
	case BOOTFILE_NAME_TAG:/*       67*/
	    if (dhptr->p_bootfile_name)
		free(dhptr->p_bootfile_name);
	    dhptr->p_bootfile_name = getString(optVal);
	    break;

	case MBLEIP_HMAGENT_ADDR_TAG:/* 68*/
	    if (dhptr->p_mobileIP_homeagent_addr)
		free(dhptr->p_mobileIP_homeagent_addr);
	    dhptr->p_mobileIP_homeagent_addr = getAddressList(optVal);
	    break;
	case SMTP_SERVER_ADDR_TAG:/*	69*/
	    if (dhptr->p_SMTPserver_addr)
		free(dhptr->p_SMTPserver_addr);
	    dhptr->p_SMTPserver_addr = getAddressList(optVal);
	    break;
	case POP3_SERVER_ADDR_TAG:/*	70*/
	    if (dhptr->p_POP3server_addr)
		free(dhptr->p_POP3server_addr);
 	    dhptr->p_POP3server_addr = getAddressList(optVal);
	    break;
	case NNTP_SERVER_ADDR_TAG:/*	71*/
	    if (dhptr->p_NNTPserver_addr)
		free(dhptr->p_NNTPserver_addr);
	    dhptr->p_NNTPserver_addr = getAddressList(optVal);
	    break;
	case WWW_SERVER_ADDR_TAG:/*	72*/
	    if (dhptr->p_WWWserver_addr)
		free(dhptr->p_WWWserver_addr);
	    dhptr->p_WWWserver_addr = getAddressList(optVal);
	    break;
	case FINGER_SERVER_ADDR_TAG:/*	73*/
	    if (dhptr->p_fingerserver_addr)
		free(dhptr->p_fingerserver_addr);
	    dhptr->p_fingerserver_addr = getAddressList(optVal);
	    break;
	case IRC_SERVER_ADDR_TAG:/*	74*/
	    if (dhptr->p_IRCserver_addr)
		free(dhptr->p_IRCserver_addr);
	    dhptr->p_IRCserver_addr = getAddressList(optVal);
	    break;
	case STTALK_SERVER_ADDR_TAG:/*	75*/
	    if (dhptr->p_StreetTalkserver_addr)
		free(dhptr->p_StreetTalkserver_addr);
	    dhptr->p_StreetTalkserver_addr = getAddressList(optVal);
	    break;
	case STDA_SERVER_ADDR_TAG:/*	76*/
	    if (dhptr->p_STDAserver_addr)
		free(dhptr->p_STDAserver_addr);
	    dhptr->p_STDAserver_addr = getAddressList(optVal);
	    break;

/* SGI */
	case SDIST_SERVER_TAG:/*	80*/
	case RESOLVE_HOSTNAME_TAG:/*	81*/
	default:	    
	    syslog(LOG_DEBUG, "Tag %d ???", optID);
	    break;
	}
    } /* for */
    if ( (dhptr->p_lease > 0) && 
	 (dhptr->p_lease < retry_stolen_timeout) )
	retry_stolen_timeout = dhptr->p_lease;
    return 0;
}

/*
 * make a copy of an existing configuration and return it without adding
 * it to the list
 */
dhcpConfig*
cf0_get_subnet_config_copy(dhcpConfig* cfgsubnet)
{
    char nbuf[512];
    char *ipc;
    struct in_addr ia;
    dhcpConfig* cfg;

    ia.s_addr = cfgsubnet->p_netnum;
    ipc = inet_ntoa(ia);
    sprintf(nbuf, "%s/%s%s", useConfigDir, "config.", ipc);
    if (add_to_config_list(nbuf, ipc, 0, &cfg))
	return NULL;
    return cfg;
}
/*
 * used to obtain dhcpConfig to update configuration from LDAP backend
 * not used to identify configuration when a packet arrives
 */
dhcpConfig *
cf0_get_config_ldap(u_long addr)
{
    dhcpConfig *cfgPtr, *defPtr;
    int found_cfg = 0;
    
    defPtr = cfgPtr = configListPtr;
    while(cfgPtr) {
	if((addr & cfgPtr->p_netmask)==(cfgPtr->p_netnum & cfgPtr->p_netmask)){
	    defPtr = cfgPtr;
	    found_cfg = 1;
	    break;
	}
	cfgPtr = cfgPtr->p_next;
    }
    if(found_cfg)         /*on which the request came in on should*/
	return defPtr;    /*be returned; else return NULL         */
    return 0;
}
#endif
