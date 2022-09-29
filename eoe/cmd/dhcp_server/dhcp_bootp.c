/*
 * BOOTP (bootstrap protocol) server daemon.
 *
 * Answers BOOTP request packets from booting client machines.
 * See [SRI-NIC]<RFC>RFC951.TXT for a description of the protocol.
 */
/*
#define PERFORMANCE
# define EDHCP to compile in enhanced server features 
# current enhanced features are ping check, mac address filtering,
# ldap backend, dhcp server failsafe, dynamic dns updates
# note that some of these are not available yet
# EDHCP is defined in the Makefile to build teh enhanced product
*/
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <fcntl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dhcp.h"
#include "dhcpdefs.h"
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <setjmp.h>
#include <syslog.h>

#include <net/raw.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#include <sys/time.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>

#include <ndbm.h>

#include "bootptab.h"

#ifndef ISM_VERSION
#define ISM_VERSION "\"1.0\""
#endif
static char *_identString = "$MyProductVersion: "ISM_VERSION" $";
#define DHCP_CONFIG_DIR "/var/dhcp/config"

#ifdef EDHCP
#include "ddns/dhcp_dnsupd.h"
#include "dhcptab.h"
extern unsigned int getLicense(char *prog, char **msg);
#ifdef EDHCP_LDAP
#include <lber.h>
#include <ldap.h>
#include "ldap/ldap_api.h"
extern void cleanup_restabm(void);
extern void cleanup_caches(void);
extern void cleanup_DHCPServiceLdap(void);
extern void free_ld_root(void);
#endif
#endif

#ifndef MAXDNAME
#define MAXDNAME 1025		/* required for client fqdn option */
#endif

DBM *db;
extern int dblock;
char	reg_netmask[30], *reg_net;
char	reg_hostnet[30];

int	debug = 0;
int	sleepmode = 0;
int	ProclaimServer = 0;
char *	alt_sysname = (char *)0;
char *	alt_config_dir = (char *)0;
char *	useConfigDir = (char *)0;
int	using_nis = 0;
int	standalone_mode = 0;
int     dh0_timeouts_cnt = 0;
int	alt_naming_flag = 0;
int	always_return_netmask = 0;

/*
 * data to support the icmp ping check
 */
int	ping_blocking_check = 0;	/* off for now */
int	ping_nonblocking_check = 0;	/* on for now */
int	ping_dns = 0;		/* check dns to see if ipaddr assigned */
int	ping_sd = -1;			/* ping socket */
int	ping_number_pending = 0; /* pings sent - not recd/timed out */
int	ping_timeout = DHCPPING_TIMEOUT;
int	ping_maxout = DHCPPING_MAXOUT;
#ifdef DEBUG
#define RETRY_STOLEN_TIMEOUT 300
#else
#define RETRY_STOLEN_TIMEOUT 12*60*60
#endif
u_long     retry_stolen_timeout = RETRY_STOLEN_TIMEOUT;
static int ident = 0;
int	ntransmitted = 0;
extern int init_ping(void);
extern int send_ping(int sd, u_long ipaddr, u_short seqnum, u_short ident);
extern struct icmp *recv_icmp_msg( int sd, u_long *ipaddr, 
				   u_char *inbuf, int inbuflen);
extern int recv_echo_reply(int *seqnum, int *ident, struct icmp *inbuf);

/* changes to call a script whenever there is a state change */
extern char *	script_file_to_run;
extern int	script_state_change_index; /* if > 0 implies changes took place */
int	script_msgtype;		/* reply type or client message 
				   that cause the change */
extern state_change_t	script_state_changes[];
extern int execute_script(void);

int	s;		/* socket fd */
int	rsockout;	/* raw socket fd for broadcast */
int	rsockout_err;	/* Return Error code */
struct	sockaddr_raw braw_addr; /* raw socket addr struct */
u_char	buf[1024];	/* receive packet buffer */
time_t	TimeNow;	/* time of day */
struct	arpreq arpreq;	/* arp request ioctl block */
struct	sockaddr_in sin;
time_t	msg_recv_time;		/* time at which a message is received */

#ifdef PERFORMANCE
int     perf_flag;
u_long  discover_time = 0;/*performance calculations related stuff*/
u_long  request_time = 0;
long    discover_ctr=0;
long    request_ctr=0;
struct  timeval perf_time_start; 
struct  timeval perf_time_end;
struct  timezone perf_tz;
#define MIL 1000000
#endif

#ifdef EDHCP_LDAP
/* ldap related declarations  */
char *ldap_conf_file = (char*)0;
#endif

struct	dhcp_request_list	*dhcp_rlist = 0;
struct	dhcp_timelist		*dhcp_tmlist_first = 0;
struct	dhcp_timelist		*dhcp_tmlist_last = 0;

char	*DHCP_MASTER = "proclaim_server";
char	*INCORRECT_MAPPING_MSG = "Invalid Address: incorrect mapping";
char	*WRONG_SUBNET_MSG = "Invalid address: wrong subnet";
char	*WRONG_SERVER_MSG = "Wrong Server requested for renew/rebind";


extern	char		*alt_hosts_file;
extern	char		*alt_ethers_file;
extern	int		dont_update_hosts;
extern	int		dont_update_ethers;
extern	rfc1533opts	*rfc1533ptr;
extern	struct ifconf	ifconf;
extern	struct ifreq	ifreq[];
extern	struct netaddr	nets[];
extern	int		ifcount;
extern  char            *EtherIpFile; 
extern  char		*lockEtherIpFile;

extern	dhcpConfig	*cf0_get_config(u_long, int flag);
extern	rfc1533opts	*get_rfc1533_config(void);
extern	int		cf0_encode_rfc1533_opts(u_char []);
extern	int		sr_create_ip_send_packet(char **, int *,
						 struct bootp *, int);
extern	int		sr_initialize_nets(void);
extern	int		sr_initialize_broadcast_sockets(void);
extern	struct netaddr*	sr_match_address(u_long);
extern	int		sr_initialize_single_broadcast_socket(void);
extern	int		sr_get_interface_type(char *);
extern	int		cf0_initialize_config(void);
extern	int		cf0_reread_config(void);
#ifdef EDHCP_LDAP
extern  void		cf0_free_config_ldap(void);
#endif
extern	int		dh0_set_timeval(struct timeval *, long);
extern  int		dh0_free_timelist_entry(struct dhcp_timelist *);
extern	int		dh0_remove_from_dhcp_reqlist(char *, int);
extern	int		loc0_remove_entry(EtherAddr *, int, char *, int, DBM *);
extern  struct dhcp_request_list *dh0_find_dhcp_rqlist_entry(char *,int);
extern	long		dh0_update_and_get_next_timeout(time_t);
extern	int		dh0_get_dhcp_msg_type(u_char *);
extern struct dhcp_request_list * dh0_find_dhcp_rqlist_entry_by_ipaddr(u_long ipaddr);
extern	int		ether_hostton(char *, struct ether_addr *);
extern	int		ether_ntohost(char *, struct ether_addr *);
extern	int		registerinethost(char *, char *, char *,
					 struct in_addr *, char *);
extern	int		dh0_decode_dhcp_client_packet(u_char *,
						      struct getParams *,
						      u_long *, u_int *,
						      u_long *, char **,
						      char **, int *, 
						      int *, int *, struct dhcp_client_info *);
extern	int		loc0_is_ipaddress_for_cid(EtherAddr *, u_long, char *, int, DBM *);
extern	int		cf0_get_new_ipaddress(EtherAddr *, u_long, u_long,
					      u_long *, dhcpConfig **,char *,
					      int, DBM *, char *name);
extern	int		chkconf(char *, char *);
extern	int		dh0_addto_timelist(char *, int, EtherAddr *, time_t,
					   struct dhcp_request_list*);
extern	int		dh0_encode_dhcp_server_packet(u_char *, int,
						      struct getParams *,
						      u_long, char *, char *,
						      char *, dhcpConfig *,
						      struct dhcp_client_info*);
extern struct dhcp_request_list* 
dh0_add_to_dhcp_reqlist(char *, int, EtherAddr *, u_long, char *,
			struct getParams *, u_long, u_int,
			dhcpConfig *, ping_status_t, int, int,
			char *, int, struct bootp*, struct bootp*,
			struct dhcp_client_info*);
extern	int		sys0_hostnameIsValid(char *);
extern	int		loc0_is_hostname_assigned(char *, DBM *);
extern	int		dh0_remove_from_timelist(char *, int);
extern	int		cf0_is_req_addr_valid(u_long, u_long);
extern	int		dh0_upd_dhcp_rq_entry(struct dhcp_request_list *,
					      struct getParams *, u_int);
extern	int		dh0_create_system_mapping(u_long, EtherAddr *,
						  char *, char *);
extern	int		loc0_create_update_entry(char *, int, EtherAddr *, 
						 u_long, char *,
						 time_t, u_long, DBM *);
extern	int		loc0_update_lease(EtherAddr *,char *, int, 
					  time_t,u_long, DBM *);
extern long		loc0_get_lease(u_long ipaddr, DBM*);
extern	int		loc0_zero_expired_lease(EtherAddr *, char *, int, 
						DBM *db);
extern	int		dh0_remove_system_mapping(char *, EtherAddr *, int, 
						  DBM *);
extern	FILE		*log_fopen(char *, char *);
extern  DBM             *log_dbmopen(char *);
extern  void            log_dbmclose(DBM *);
extern  int		loc0_is_etherToIP_inconsistent(char *, int,EtherAddr *,
						       u_long, char *, DBM *);
static	int		check_selected_name_address(char *rhsname, char **dhcp_msg, u_long dhcp_ipaddr, struct bootp *rq, struct dhcp_request_list *req, char *cid_ptr, int cid_length);
extern char*		sys0_name_errmsg(char *, int);
extern char*		sys0_addr_errmsg(u_long, int);
extern char*		loc0_get_hostname_from_cid(char *, int, EtherAddr *, DBM *);
extern int		loc0_is_ipaddress_assigned(u_long ipa, DBM *);
extern void		set_addl_options_to_send(char *pstr);
extern void		mk_str(int cid_flag, char *cid_ptr, int cid_length, 
			       char *str);
extern void		cleanup_atexit(void);
extern void		free_vendor_options(void);
extern int		cf0_free_config_list(dhcpConfig *delptr);
extern u_long		loc0_get_ipaddr_from_cid(EtherAddr *, char *, 
						 int, DBM *);
#ifdef EDHCP
extern int		process_expired_ping_send(void);
extern int		dh0_append_client_fqdn(u_char dhbuf[], 
					       char *client_fqdn);
extern int		ping_send_DHCPOFFER(struct dhcp_request_list*);
extern int		handle_ping_nonblocking_reply(int);

/* globals associated with DHCP-DNS updates */
#define DHCP_DNS_DEFAULT_TTL 3600
#define DEFAULT_DDNS_CONF_FILE "/var/dhcp/config/dhcp_ddns.conf"
int dhcp_dnsupd_on = 0;
int dhcp_dnsupd_secure = 1;
char *ddns_conf_file = DEFAULT_DDNS_CONF_FILE;
int dhcp_dnsupd_beforeACK = 0;
int dhcp_dnsupd_clientARR = 0;
int dhcp_dnsupd_always = 0;
int dhcp_dnsupd_ttl = DHCP_DNS_DEFAULT_TTL;	/* 1 hour */
#endif
/*
 * Globals below are associated with the bootp database file (bootptab).
 */

char	VERS[] = "SGI bootp/dhcp Server V3.1.2";
char	*bootptab = "/etc/bootptab";
static  char *opts_file;

FILE	*fp;
char	line[256];	/* line buffer for reading bootptab */
char	*linep;		/* pointer to 'line' */
int	linenum;	/* current line number in bootptab */
char	homedir[64];	/* bootfile homedirectory */
char	defaultboot[64]; /* default file to boot */

int	nhosts;		/* current number of hosts */
long	modtime;	/* last modification time of bootptab */

int	max_addr_alloc = 64;
int	numaddrs;
u_int	*myaddrs;	/* save addresses of executing host */
char	myname[MAXHOSTNAMELEN+1]; /* my primary name */
char	*mydomain;	/* pointer into myname */
iaddr_t	myhostaddr;	/* save (main) internet address of executing host */
int	forwarding;	/* flag that controls cross network forwarding */
u_long	mynetmask;	/* Netmask of the primary interface - used by DHCP */
int dhcp_timeout, dhcp_timer_set = 0;

struct	netaddr *np_recv, *np_send;

static char *iaddr_ntoa(iaddr_t addr);

void request(struct bootp *);
void reply(struct bootp *);
void forward(struct bootp *, iaddr_t *, iaddr_t *);
void sendreply(struct bootp *, int, int, int, int);
void setarp(iaddr_t *, u_char *, int );
void readtab(void);
void getfield(char *, int );
void makenamelist(void);

/*
 * For sgi, bootp is a single threaded server invoked
 * by the inet master daemon (/usr/etc/inetd).  Once
 * a bootp has been started, it will handle all subsequent
 * bootp requests until it has been idle for TIMEOUT
 * seconds, at which point the bootp process terminates
 * and inetd will start listening to the IPPORT_BOOTPS
 * socket again.
 */
#define	TIMEOUT		300	/* number of idle seconds to wait */

extern int addl_options_len;

void
init_dhcp_opts(void)
{
    using_nis = 0;
    dont_update_hosts = 0;
    dont_update_ethers = 0;
    if (alt_hosts_file) { free(alt_hosts_file); alt_hosts_file = NULL; }
    if (alt_ethers_file) { free(alt_ethers_file); alt_ethers_file = NULL;}
    if (alt_sysname) { free(alt_sysname); alt_sysname = NULL; }
    if (alt_config_dir) { free(alt_config_dir); alt_config_dir = NULL; }
    alt_naming_flag = 0;
    always_return_netmask = 0;
    dhcp_timeout = TIMEOUT; dhcp_timer_set = 0;
    addl_options_len = 0;
    if (script_file_to_run) { 
	free(script_file_to_run); script_file_to_run = NULL; }
#ifdef EDHCP
    ping_blocking_check = 0;
    ping_nonblocking_check = 0;
    ping_timeout = DHCPPING_TIMEOUT;
    ping_maxout = DHCPPING_MAXOUT;
    ping_dns = 0;
#ifdef EDHCP_LDAP
    if (ldap_conf_file) { 
	free(ldap_conf_file); ldap_conf_file = NULL; 
    }
#endif
    dhcp_dnsupd_on = 0;
    dhcp_dnsupd_secure = 1;
    dhcp_dnsupd_beforeACK = 0;
    dhcp_dnsupd_clientARR = 0;
    dhcp_dnsupd_always = 0;
    dhcp_dnsupd_ttl = DHCP_DNS_DEFAULT_TTL;
#endif
}

static long	dhcpopts_modtime = 0; /* last modification time of dhcpmtab */

int
process_dhcp_opts(char *fname)
{
    FILE *fp;
    char dbuf[256];
    char *p, *str;
    char tbuf[256];
    struct stat st;

    if(!fname || (*fname == '\0') )
	return 1;
    fp = log_fopen(fname, "r");
    if(!fp)
	return 1;
    fstat(fileno(fp), &st);
    if (st.st_mtime == dhcpopts_modtime && st.st_nlink) {
	fclose(fp);
	return 0;
    }
    if (dhcpopts_modtime != 0)
	init_dhcp_opts();	/* only second time onwards */
    dhcpopts_modtime = st.st_mtime;
    while(fgets(dbuf, 256, fp) != NULL) {
	p = dbuf;

	for(;;) {
	    if(*p == '#')
		break;
	    while(*p && isspace(*p)) p++;
	    if(*p == '\0')
		break;
	    str = p;
	    while(*p && !isspace(*p)) p++;

	    if(strncmp(str, "-h", 2) == 0) {
		syslog(LOG_ERR, "The -h option is no longer supported");
		fclose(fp);
		return 2;
	    }
	    else
 	    if(strncmp(str, "-y", 2) == 0) {
		/* DHCP will use nis, this system better be the NIS master */
		using_nis++;
		p++;
	    }
	    else
	    if(strncmp(str, "-s", 2) == 0) {
		syslog(LOG_ERR, "The -s option is no longer supported");
		fclose(fp);
		return 2;
	    }
	    else
	    if(strncmp(str, "-W", 2) == 0) {
		dont_update_hosts++;
	    }
	    else
	    if(strncmp(str, "-w", 2) == 0) {
		/* DHCP: Optional location of the hosts map */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		alt_hosts_file = strdup(str);
	    }
	    else
	    if(strncmp(str, "-E", 2) == 0) {
		dont_update_ethers++;
	    }
	    else
	    if(strncmp(str, "-e", 2) == 0) {
		/* DHCP: Optional location of the ethers map */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		alt_ethers_file = strdup(str);
	    }
	    else
	    if(strncmp(str, "-u", 2) == 0) {
		/* DHCP: Optional sysname file, default is /unix */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		alt_sysname = strdup(str);
	    }
	    else
	    if(strncmp(str, "-c", 2) == 0) {
		/* DHCP: Optional DHCP Server config directory,
		 * default is /var/dhcp/config.
		 */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		alt_config_dir = strdup(str);
	    }
	    else
	    if(strncmp(str, "-x", 2) == 0) {
		alt_naming_flag++;
	    }
	    else
	    if(strncmp(str, "-n", 2) == 0) {
		/* This is a workaround for a bug in the windows
		 * dhcp client, where the windows client expects
		 * to get a valid netmask back from the server
		 * without asking for it.
		 */
		always_return_netmask++;
	    }
	    else
	    if(strncmp(str, "-t", 2) == 0) {
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		sscanf(str, "%d", &dhcp_timeout);
		dhcp_timer_set = 1;
	    }
	    else
	    if (strncmp(str, "-m", 2) == 0) {
		if (*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while (isspace(*p)) p++;
		str = p;
		while (*p && !isspace(*p)) p++;
		strncpy(tbuf,str,p-str);
		tbuf[p-str] = '\0';
		set_addl_options_to_send(tbuf);
	    }
	    else
	    if (strncmp(str, "-r", 2) == 0) {
	      if (*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while (isspace(*p)) p++;
		str = p;
		while (*p && !isspace(*p)) p++;
		*p++ = '\0';
		script_file_to_run = strdup(str);
		if (stat(script_file_to_run, &st) == -1) {
		  syslog(LOG_ERR, "Script file %s not found (%m)", 
			 script_file_to_run);
		  script_file_to_run = (char*)0;
		}
	    }
#ifdef EDHCP
	    else
	    if (strncmp(str, "-pn", 3) == 0) { /* non blocking ping */
	      ping_nonblocking_check = 1;
	      ping_blocking_check = 0;
	    }
	    else if (strncmp(str, "-pb", 3) == 0) { /* blocking ping */
	      ping_blocking_check = 1;
	      ping_nonblocking_check = 0;
	    }
	    else if (strncmp(str, "-pt", 3) == 0) { /* ping timeout */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		sscanf(str, "%d", &ping_timeout);
	    }
	    else if (strncmp(str, "-pl", 3) == 0) { 
		/* max pings pending to deactivate sending PING */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		sscanf(str, "%d", &ping_maxout);
	    }
	    else if (strncmp(str, "-pd", 3) == 0) {
		/* should we ping dns to check if an address is taken */
		ping_dns = 1;
	    }
#ifdef EDHCP_LDAP
	    else if (strncmp(str, "-l", 2) == 0) {
		/* ldap database */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		ldap_conf_file = strdup(str);
	    }
#endif
	    else if (strncmp(str, "-dn", 3) == 0) 
		dhcp_dnsupd_on = 1; /* do dynamic dns updates */
	    else if (strncmp(str, "-ds", 3) == 0) 
		dhcp_dnsupd_secure = 0;	/* don't use security */
	    else if (strncmp(str, "-db", 3) == 0)
		dhcp_dnsupd_beforeACK = 1;
	    else if (strncmp(str, "-dc", 3) == 0)
		dhcp_dnsupd_clientARR = 1;
	    else if (strncmp(str, "-da", 3) == 0)
		dhcp_dnsupd_always = 1;
	    else if (strncmp(str, "-dt", 3) == 0) { /* ttl */
		if(*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		sscanf(str, "%d", &dhcp_dnsupd_ttl);
	    }
	    else if (strncmp(str, "-df", 3) == 0) {
		if (*p == '\0') {
		    fclose(fp);
		    return 1;
		}
		while(isspace(*p)) p++;
		str = p;
		while(*p && !isspace(*p)) p++;
		*p++ = '\0';
		ddns_conf_file = strdup(str);
	    }
#endif
	    else {
		fclose(fp);
		return 1;
	    }
	}
    }
    fclose(fp);
    return 0;
}


static void
setIncrDbgFlg()
{
        (void)signal(SIGUSR1, (void (*)())setIncrDbgFlg);

	debug++;
        if (debug)
	  syslog(LOG_DEBUG, "Debug turned ON, Level %d\n", debug);
#ifdef EDHCP_LDAP
	ldhcp_level++;
#endif
}

static void
setNoDbgFlg()
{
        (void)signal(SIGUSR2, (void (*)())setNoDbgFlg);

        if (debug)
	  syslog(LOG_DEBUG, "Debug turned OFF\n");
	debug = 0;
#ifdef EDHCP_LDAP
	ldhcp_level = LDHCP_LOG_RESOURCE;
#endif
}


#ifdef EDHCP_LDAP
static int rereadLdapConf = 0;
#endif
static int rereadConfig = 0;

static void
setRereadConfFlg(int sig)
{

    (void)signal(sig, (void (*)())setRereadConfFlg);
    if (sig == SIGHUP) {
	rereadConfig = 1;
	syslog(LOG_INFO, "received SIGHUP");
    }

#ifdef EDHCP_LDAP
    if (sig == SIGALRM)
	rereadLdapConf = 1;
#endif
}

static void
check_options_config(void)
{
    int rc;

    syslog(LOG_INFO, "received SIGHUP: reconfiguring");
    process_dhcp_opts(opts_file);
    if(alt_config_dir)
	useConfigDir = alt_config_dir;
    else
	useConfigDir = DHCP_CONFIG_DIR;
#ifdef EDHCP_LDAP
    /* read the ldap configuration file */
    if (ldap_conf_file) {
	rc = parse_ldap_dhcp_config(ldap_conf_file);
	if (rc) {
	    exit(3);
	}
    }
    else 
	free_ld_root();
#ifdef EDHCP
    if ((ping_blocking_check) || 
	(ping_nonblocking_check)) {
	if (ping_sd < 0) {
	    ping_sd = init_ping();
	    if (ping_sd < 0) {
		syslog(LOG_ERR, "Error in opening ping socket");
		exit(1);
	    }
	    ident = htons(getpid()) & 0xFFFF;
	}
    }
    else {
	if (ping_sd > 0) {
	    close(ping_sd);
	    ping_sd = -1;
	}
    }
    read_dtab();
#endif
    cf0_free_config_ldap();
#endif
    if (rc = cf0_reread_config()) {
	syslog(LOG_ERR, "unable to re-read DHCP Master Configuration %d", rc);
	exit(1);
    }

#ifdef EDHCP
    if (dhcp_dnsupd_secure) {
	rc = parse_ddns_dhcp_config(ddns_conf_file);
	if (rc) 
	    exit(4);
    }
#endif
    rereadConfig = 0;
}

void
cleanup_atexit(void)
{
#ifdef EDHCP_LDAP
    (void) cleanup_restabm();
    (void) cleanup_caches();
    (void) cleanup_DHCPServiceLdap();
#endif
    free_vendor_options();
    cf0_free_config_list(0);
}


void
main(int argc, char *argv[])
{
    register struct bootp *bp;
    register int n;
    struct	sockaddr_in from;
    int	fromlen;
    int ch;
    int len, i;
    fd_set readfds;
    struct timeval tmout, *ptmout;
    long next_tm;
    struct dhcp_timelist *tm_entry;
    char sifname[IFNAMSIZ+1];
    iaddr_t ipn;
    int on = 1;
    int off = 0;
    int rc = 0;
#ifdef EDHCP
    struct dhcp_request_list* dhcp_rq;
#endif
    int maxfd;

    static int ignore_first_packet = 1;

    struct dhcp_timelist *dh0_pop_timelist_timer(void);
    extern char *optarg;


    if(chkconf(DHCP_MASTER, 0) == CHK_ON)
    	ProclaimServer = 1;

    /* Before we do anything we check for a valid Flexlm License. */
#ifdef EDHCP
    {
	char    *message;
	if ( ProclaimServer &&
	     (getLicense(argv[0], &message) == 0) ) { /* return 1 is ok */
	    openlog(argv[0], (LOG_PID | LOG_NDELAY), LOG_DAEMON);
	    syslog(LOG_NOTICE, "EDHCP %s: %s", argv[0], message);
	    exit(1);
	}
    }
#endif

    while ((ch = getopt(argc, argv, "dfpbo:")) != -1) {
	switch(ch) {
	    case 'd':
		/* Debug Info into syslog */
		debug++;
#ifdef EDHCP_LDAP
		ldhcp_level++;
#endif
		break;
	    case 'f':
		/* enable cross network forwarding */
		forwarding++;
		break;
	    case 'p':
		/* Sleep before processing the message, allows for easy
		 * debugging by dbx
		 */
		sleepmode++;
		break;
	    case 'b':
		/* Run in standalone mode */
		standalone_mode++;
		break;
	    case 'o':
		/* Options file specified for dhcp specific options */
		if(!ProclaimServer)
		    break;
		opts_file = strdup(optarg);
		rc = process_dhcp_opts(opts_file);
		break;
	    default:
		rc = 2;
	}
    }
    
    (void) signal(SIGUSR1, setIncrDbgFlg);
    (void) signal(SIGUSR2, setNoDbgFlg);
#ifdef EDHCP_LDAP
    (void) signal(SIGALRM, setRereadConfFlg);
#endif
    (void) signal(SIGHUP, setRereadConfFlg);

    time(&TimeNow);
    openlog("bootp", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "%s starting at %s", VERS, ctime(&TimeNow));
    switch(rc) {
	case 1:
	    syslog(LOG_ERR, "Cannot parse the DHCP options file %s",opts_file);
	    exit(1);
	case 2:
	    syslog(LOG_ERR, "Illegal command line option.");
	    exit(2);
    }

#ifdef EDHCP_LDAP
    /* read the ldap configuration file */
    if (ldap_conf_file) {
	rc = parse_ldap_dhcp_config(ldap_conf_file);
	if (rc) {
	    exit(3);
	}
    }
#endif

    rfc1533ptr = get_rfc1533_config();

    /*
     * It is not necessary to create a socket for input.
     * The inet master daemon passes the required socket
     * to this process as fd 0.  Bootp takes control of
     * the socket for a while and gives it back if it ever
     * remains idle for the timeout limit.
     */
    s = 0;
    if(standalone_mode) {
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
	    syslog(LOG_ERR,"Opening DGRAM stream socket(%m)");
	    exit(1);
	}
	dup2(s, 0);
	bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;       
	sin.sin_port = htons(IPPORT_BOOTPS);
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    syslog(LOG_ERR,"Binding DGRAM stream socket(%m)");
	    exit(1);
	}
    }
    /* 
     * initialize ping socket
     */
    maxfd = s;
#ifdef EDHCP
    if ((ping_blocking_check) || (ping_nonblocking_check)) {
	ping_sd = init_ping();
	if (ping_sd < 0) {
	    syslog(LOG_ERR, "Error in opening ping socket");
	    exit(1);
	}
	maxfd = (ping_sd > maxfd)? ping_sd: maxfd;
	ident = htons(getpid()) & 0xFFFF;
    }
#endif
    /*
     * Save the name of the executing host
     */
    makenamelist();

    ifcount = sr_initialize_nets();
    if(MULTIHMDHCP) {
	rsockout_err = sr_initialize_broadcast_sockets();
    }
    else if(ProclaimServer) {
	rsockout_err = sr_initialize_single_broadcast_socket();
    }

    db = log_dbmopen(EtherIpFile);
    dblock = open(lockEtherIpFile, O_RDONLY|O_CREAT, 0604);
    if (dblock < 0) {
        syslog(LOG_ERR, "Could not open %s (%m)", lockEtherIpFile);
	log_dbmclose(db);
	exit(1);
    }

    if(ProclaimServer) {
	mynetmask = nets[0].netmask;
	if(alt_config_dir)
	    useConfigDir = alt_config_dir;
	else
	    useConfigDir = DHCP_CONFIG_DIR;
	if(rc = cf0_initialize_config()) {
	    syslog(LOG_ERR, "unable to read DHCP Master Configuration %d", rc);
	    exit(1);
	}
    }

    if(MULTIHMDHCP) {
	if(setsockopt(s, SOL_SOCKET, SO_PASSIFNAME, &on, sizeof(on)) < 0) {
	    syslog(LOG_ERR, "Cannot set socket option SO_PASSIFNAME:(%m)");
	    exit(1);
	}
    }
    else if (ifcount > 1) {
	if(setsockopt(s, SOL_SOCKET, SO_PASSIFNAME, &off, sizeof(off)) < 0) {
	    syslog(LOG_ERR, "Cannot set socket option SO_PASSIFNAME:(%m)");
	    exit(1);
	}
    }

    if (ifcount < 2 && forwarding) {
	if (debug) 
	    syslog(LOG_DEBUG, "less than two interfaces, -f flag ignored");
	forwarding = 0;
    }
    if(debug && (ifcount>1 ^ forwarding))
	syslog(LOG_DEBUG, "%d network interface(s), forwarding is %s",
		ifcount, forwarding ? "ENABLED" : "DISABLED");
    len = sizeof (sin);
    if (getsockname(s, &sin, &len) < 0) {
	syslog(LOG_ERR, "getsockname failed (%m)");
	exit(1);
    }

#ifdef EDHCP
    if (dhcp_dnsupd_secure) {
	rc = parse_ddns_dhcp_config(ddns_conf_file);
	if (rc) 
	    exit(4);
    }
#endif

    if ((!ProclaimServer) || (!dhcp_timer_set)) {
	dh0_set_timeval(&tmout, TIMEOUT);
	dhcp_timeout = TIMEOUT;
    }
    else
	dh0_set_timeval(&tmout, dhcp_timeout);

    dh0_addto_timelist(0, 0, 0, TimeNow+dhcp_timeout, 0); 

#ifdef EDHCP
    /* read the mac filter file
     */
    read_dtab();
#endif

    for (;;) {
      
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);
#ifdef EDHCP
	if (ping_nonblocking_check) {
	    FD_SET(ping_sd, &readfds);
	}
#endif
	if (dhcp_timer_set && (dhcp_timeout == 0) && (dh0_timeouts_cnt == 1))
	    ptmout = (struct timeval*)0;
	else
	    ptmout = &tmout;
	switch (select(maxfd + 1, &readfds, (fd_set *)0, 
		       (fd_set *)0, ptmout)) {
	    case 0:
		/* Timeout happened : process timeouts */
		if(dh0_timeouts_cnt == 1) {/* idle timeout happened */
		  
#ifdef PERFORMANCE
		  if (discover_ctr > 0){
		    syslog(LOG_DEBUG,"The total time taken to process %lu DISCOVER messages = %lu usec.", discover_ctr, discover_time);
		    syslog(LOG_DEBUG,"The average time to process %lu DISCOVER messages = %lu usec.", discover_ctr, discover_time/discover_ctr);
		  }
		  if (request_ctr > 0){
		    syslog(LOG_DEBUG,"The total time taken to process %lu REQUESTS = %lu usec", request_ctr, request_time);
		    syslog(LOG_DEBUG,"The average time to process %lu REQUESTS = %lu usec", request_ctr, request_time/request_ctr);
		  }
#endif
		  
		  log_dbmclose(db);
		  cleanup_atexit();
		  exit(0);
		  _identString = _identString;  /*no complaints for ID string*/
		}

		
		/* Remove the First timeout and clean up, also set the next timeout
		 * from the list. Do not update the idle timeout.
		 */
		tm_entry = dh0_pop_timelist_timer();
		if(tm_entry) {
#ifdef EDHCP
				/* if this was an entry for which
				 * an ping was sent then process that */
		    if (tm_entry->tm_dhcp_rq) 
			dhcp_rq = tm_entry->tm_dhcp_rq;
		    else
			dhcp_rq = dh0_find_dhcp_rqlist_entry(tm_entry->cid_ptr,
							     tm_entry->cid_length);
		    if (dhcp_rq && (dhcp_rq->ping_status == PING_SENT)) {
			ping_send_DHCPOFFER(dhcp_rq);
		    }
		    else 
#endif		      
		      {
			dh0_remove_from_dhcp_reqlist(tm_entry->cid_ptr,
						     tm_entry->cid_length);
			
			loc0_remove_entry(tm_entry->tm_eaddr, 0,
					  tm_entry->cid_ptr, 
					  tm_entry->cid_length, db);
		    }
		    /* prevent major memory leak */
		    dh0_free_timelist_entry(tm_entry);
		}
		else {
		    syslog(LOG_ERR, "No timer entry found for processing.");
		}
		time(&TimeNow);
		next_tm = dh0_update_and_get_next_timeout(TimeNow);
		dh0_set_timeval(&tmout, next_tm);
		break;
	    case -1:
		time(&TimeNow);
		dhcp_tmlist_last->tm_value = TimeNow+dhcp_timeout;
		next_tm = dh0_update_and_get_next_timeout(TimeNow);
		dh0_set_timeval(&tmout, next_tm);
		if (errno == EINTR)
		    continue;
		syslog(LOG_ERR, "select error (%m)");
		break;
	    default:
#ifdef EDHCP
		if (FD_ISSET(ping_sd, &readfds)) {
		    handle_ping_nonblocking_reply(ping_sd);
		}
		if (ping_number_pending > 0) 
		    process_expired_ping_send();
#endif
		if (FD_ISSET(s, &readfds)) {
		    fromlen = sizeof (from);
		    n = recvfrom(s, buf, sizeof buf, 0, (caddr_t)&from, 
				 &fromlen);
		    if ( (n <= 0) || (n < BOOTPKTSZ) )
			continue;
		    
		    if(MULTIHMDHCP) {
			bzero(sifname, IFNAMSIZ);
			np_recv = 0;
			strncpy(sifname, (char *)buf, IFNAMSIZ);
			if(*sifname == '\0')
			    continue;
			for (i = 0; i < ifcount; i++) {
			    if(strcmp(nets[i].ifname, sifname) == 0) {
				np_recv = &nets[i];
				break;
			    }
			}
			/* The socket option SO_PASSIFNAME is not set for the first
			   packet beacuse inetd receives it before option was set*/
			if (ignore_first_packet) {
			    ignore_first_packet = 0;
			    continue;
			}
			if(!np_recv) {
			    syslog(LOG_ERR, "Cannot find interface %s", sifname);
			    continue;
			}
			bp = (struct bootp *)&buf[IFNAMSIZ];
		    }
		    else {
			bp = (struct bootp *)buf;
		    }
		    
		    time(&TimeNow);
		    dhcp_tmlist_last->tm_value = TimeNow+dhcp_timeout;
		    
		    if (time(&msg_recv_time) == (time_t)-1)
			syslog(LOG_ERR, "Error in time(): (%m)");
		    
		    readtab(); 	/* (re)read bootptab */
		    if (rereadConfig) {
			check_options_config();
			if (ping_sd > 0)
			    maxfd = (ping_sd > maxfd)? ping_sd: maxfd;
			else
			    maxfd = s;
		    }
#ifdef EDHCP_LDAP
		    if (rereadLdapConf) {
			check_ldap_subnet_refresh();
			rereadLdapConf = 0;
		    }
#endif

		    switch (bp->bp_op) {
		    case BOOTREQUEST:
			if(MULTIHMDHCP && np_recv) {
			    ipn.s_addr = np_recv->netmask;
			    reg_net = inet_ntoa(ipn);
			    strcpy(reg_netmask, reg_net);
			    reg_net = inet_ntoa(np_recv->myaddr);
			}
			else {
			    ipn.s_addr = nets[0].netmask;
			    reg_net = inet_ntoa(ipn);
			    strcpy(reg_netmask, reg_net);
			    reg_net = inet_ntoa(nets[0].myaddr);
			}
			strcpy(reg_hostnet, reg_net);
			request(bp);
			break;
		    case BOOTREPLY:
			reply(bp);
			break;
		    }
		}
		time(&TimeNow);
		next_tm = dh0_update_and_get_next_timeout(TimeNow);
		dh0_set_timeval(&tmout, next_tm);
	}
    }    
}

int
process_rfc1533_bootp_message(struct bootp *rp)
{
    int rval;

    if(rfc1533ptr == 0)
	return 1;
    rval = cf0_encode_rfc1533_opts(rp->dh_opts);
    return rval;
}

/*
 * Check the passed name against the current host's addresses.
 *
 * Return value
 *	TRUE if match
 *	FALSE if no match
 */
int
matchhost(char *name)
{
    register struct hostent *hp;
    int i;
    u_int requested_addr;

    if (hp = gethostbyname(name)) {
	requested_addr =  *(u_long *)(hp->h_addr_list[0]);
	for (i = 0; i < numaddrs; i++)
	    if (requested_addr == myaddrs[i])
		return 1;
    }
    return 0;
}

/*
 * Check whether two passed IP addresses are on the same wire.
 * The first argument is the IP address of the requestor, so
 * it must be on one of the wires to which the server is
 * attached.
 *
 * Return value
 *	TRUE if on same wire
 *	FALSE if not on same wire
 */
int
samewire(register iaddr_t *src, register iaddr_t *dest)
{
    register struct netaddr *np;

    /*
     * It may well happen that src is zero, since the datagram
     * comes from a PROM that may not yet know its own IP address.
     * In that case the socket layer doesnt know what to fill in
     * as the from address.  In that case, we have to assume that
     * the source and dest are on different wires.  This means
     * we will be doing forwarding sometimes when it isnt really
     * necessary.
     */
    if (src->s_addr == 0 || dest->s_addr == 0)
	return 0;

    /*
     * In order to take subnetworking into account, one must
     * use the netmask to tell how much of the IP address
     * actually corresponds to the real network number.
     *
     * Search the table of nets to which the server is connected
     * for the net containing the source address.
     */
    for (np = nets; np->netmask != 0; np++)
	if ((src->s_addr & np->netmask) == np->net)
	    return ((dest->s_addr & np->netmask) == np->net);

    return 0;
}


int execute_script(void)
{
    int i;
    char scriptbuf[256];
    char *ethc;
    state_change_t *p_stch;
    char str[MAXCIDLEN];

    for (i = 0; i < script_state_change_index; i++) {
	p_stch = &script_state_changes[i];
	mk_str(1, p_stch->cid, p_stch->cid_length, str);
	ethc = ether_ntoa(&p_stch->mac);
	sprintf(scriptbuf, "%s '-c %s -m %s -i %s -h %s -l %d -o %d -t %d' &",
		script_file_to_run, str, ethc, p_stch->ipc, p_stch->hostname,
		p_stch->lease, p_stch->op, script_msgtype);
	if (debug) 
	    syslog(LOG_DEBUG, "Script: %s", scriptbuf);
	if (system(scriptbuf) == -1) {
	    syslog(LOG_ERR, "Could not execute %s (%m)",
		   scriptbuf);
	    return 1;
	}
    }
    return 0;
}

static long
find_lease_to_offer(u_long newaddr, u_int dhcp_lease, 
		    dhcpConfig* newConfigPtr, DBM *db)
{
    long offered_lease;

    offered_lease = loc0_get_lease((u_long)newaddr, db);
    if ( (dhcp_lease == -1) && ( (offered_lease == INFINITE_LEASE) ||
				 (offered_lease == STATIC_LEASE) ) )
	offered_lease = -1;
    else {
	if ( (dhcp_lease > 0) && (dhcp_lease < newConfigPtr->p_lease) )
	    offered_lease = (long)dhcp_lease;
	else 
	    offered_lease = (long)newConfigPtr->p_lease;
    }
    return offered_lease;
}

int
process_dhcp_message(struct bootp *rq, struct bootp *rp)
{
    char *dh0_getNisDomain(void);
    char *dh0_getDnsDomain(void);
    struct dhcp_timelist *dh0_find_timelist_entry(char *, int);
    struct dhcp_request_list *dh0_find_dhcp_rqlist_entry(char *,int);

    char	npath[4];
    char 	hostname1[MAXHOSTNAMELEN];
    int		has_sname;	/* does rq have nonempty bp_sname? */
    struct	hostent *hostentp;
    iaddr_t	fromaddr, ipn;
    
    int		dhcp_msg_type;
    struct	getParams dhcp_reqParams;
    u_long	dhcp_ipaddr, dhcp_server;
    char	*dhcp_hsname, *dhcp_msg;
    u_int	dhcp_lease;
    char	*resolv_name;
    dhcpConfig	*newConfigPtr;
    
    u_long	newaddr, netaddr;
    int		nameaddr_err;
    int		rval, aval;
    struct dhcp_request_list *req;
    struct dhcp_timelist *tmq;
    
    char	*rhsname;
    u_long	mltaddr;
    int		typ;
    int		brk_flag;
    
    char	*client_domain;
    char        *cid_ptr = 0; /*CHECK*/
    int         cid_flag = 0;
    int         cid_length;
    char        str[1024];
    int		sgi_resolv_name = 0; /* not sgis proprietary resolv name */
    ping_status_t ping_status = PING_NONE;
    int		timeout;
#ifdef EDHCP
    int ret;
#endif
    struct	dhcp_client_info dci_data;
    long	offered_lease;	/* length of lease offered */

    offered_lease = 0;
    bzero(str, 1024);
    newaddr = 0;
    resolv_name = (char *)0;
    dhcp_hsname = dhcp_msg = (char *)0;
    dhcp_ipaddr = dhcp_server = 0;
    bzero(&dhcp_reqParams, sizeof(struct getParams));
    dhcp_lease = 0;
    bzero(&dci_data, sizeof(struct dhcp_client_info));
    
    npath[0] = '\0';
    strcpy((char *)rp->bp_file, npath);
    has_sname = rq->bp_sname[0] != '\0';
    if (!has_sname)
	strncpy((char *)rp->bp_sname, myname, sizeof(rp->bp_sname));
    else if (!matchhost((char *)rq->bp_sname)) {
	iaddr_t destaddr;
	/* Not for us.  */
	if (!forwarding)
	    return 0;
	fromaddr = rq->bp_ciaddr.s_addr ? rq->bp_ciaddr : rp->bp_yiaddr;
	/* The client should not be asking for a new IP address from
	 * a specific server.
	 */
	if(fromaddr.s_addr == 0)
	    return 0;
	/*
	 * Look up the host by name and decide whether
	 * we should forward the message to him.
	 */
	if ((hostentp = gethostbyname((const char *)rq->bp_sname)) == 0) {
	    syslog(LOG_INFO, "request for unknown server %s", rq->bp_sname);
	    return 0;
	}
	destaddr.s_addr = *(u_long *)(hostentp->h_addr_list[0]);
	/*
	 * If the other server is on a different cable from the
	 * requestor, then forward the request.  If on the same
	 * wire, there is no point in forwarding.  Note that in
	 * the case that we don't yet know the IP address of the
	 * client, there is no way to tell whether the client and
	 * server are actually on the same wire.  In that case
	 * we forward regardless.  It's redundant, but there's
	 * no way to get around it.
	 */
	if (!samewire(&fromaddr, &destaddr)) {
	    /*
	     * If we were able to compute the client's Internet
	     * address, pass that information along in case the
	     * other server doesn't have the info.
	     */
	    rq->bp_yiaddr = rp->bp_yiaddr;
	    forward(rq, &fromaddr, &destaddr);
	}
	return 0;
    }
#ifdef EDHCP
    /* if a matching mac address is found take appropriate actions */
    if (lookup_dtab(rq->bp_hlen, rq->bp_htype, rq->bp_chaddr))
	return 0;		/* drop packet */
#endif

    dhcp_msg_type = dh0_decode_dhcp_client_packet(rq->dh_opts, &dhcp_reqParams,
						  &dhcp_ipaddr, &dhcp_lease,
						  &dhcp_server, &resolv_name, 
						  &cid_ptr, &cid_flag, &cid_length, 
						  &sgi_resolv_name, 
						  &dci_data);
    
    
    if (!cid_flag){
      cid_length = sizeof(EtherAddr); 
      /*Assuming that the rq->bp_chaddr is not going to be longer than 
	sizeof(EtherAddr) i.e. 6 bytes(true for Ethernet & FDDI)*/
      cid_ptr = (char *)malloc(cid_length);
      bzero(cid_ptr,cid_length); 
      bcopy(rq->bp_chaddr, cid_ptr, cid_length);
    }
    
    mk_str(cid_flag, cid_ptr, cid_length, str);
    /*The 'str'(ing) is being used in displaying in the SYSLOG only. The key 
      used for the ndbm file in case of cid_flag=0 is just the cid (without the
      h/w type) whereas when cid_flag=1 the key used includes the h/w type and
      the cid. 
    */
    
    script_state_change_index = 0; /* signifies no changes as yet */
    
    switch(dhcp_msg_type){
    case DHCPDISCOVER:
#ifdef PERFORMANCE
	perf_flag = DHCPDISCOVER;
	gettimeofday(&perf_time_start,&perf_tz);
#endif 
	script_msgtype = DHCPOFFER;
	if (debug >= 3) {
	    syslog(LOG_DEBUG,
		   "DHCPDISCOVER: cid (%s)",str);
	}
		
	if(rq->bp_ciaddr.s_addr == 0) {
	    /* client doesn't know his IP address */
	    if(dhcp_ipaddr) {
		/* client is requesting a specific address */
		if(loc0_is_ipaddress_for_cid((EtherAddr *)rq->bp_chaddr,
					     dhcp_ipaddr, cid_ptr,
					     cid_length, db)== 0) {
		    newConfigPtr = cf0_get_config(dhcp_ipaddr, 1);
		    if (newConfigPtr) {
			mltaddr = MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
			netaddr = rq->bp_giaddr.s_addr ? rq->bp_giaddr.s_addr : mltaddr;
			if (cf0_is_req_addr_valid(dhcp_ipaddr, netaddr)== 0)
			    newaddr = dhcp_ipaddr;
		    }
		}
		if (debug) {
		    ipn.s_addr = dhcp_ipaddr;
		    syslog(LOG_DEBUG,
			   "DHCPDISCOVER: cid (%s) request address (%s)",str,
			   inet_ntoa(ipn));
		}
		
	    }
	retry_inconsistent:
	    if(newaddr == 0) {
		/* The local mapping will also get created here as a new */
		/* IP address and hostname is generated */
		if(rq->bp_giaddr.s_addr) {
		    if (debug >= 2)
			syslog(LOG_DEBUG,
			       "DHCPDISCOVER: cid (%s) thru gateway (%s)",
			       str, inet_ntoa(rq->bp_giaddr));
		    /* This is a forwarded message, use the gateway addr */
		    if(cf0_get_new_ipaddress((EtherAddr *)rq->bp_chaddr,
					     rq->bp_giaddr.s_addr,
					     dhcp_ipaddr,
					     &newaddr, &newConfigPtr, 
					     cid_ptr, cid_length, db, 
					     resolv_name))
			return 1;
		}
		else {
		    /* Generate using my IP Address */
		    mltaddr =
			MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
		    if(cf0_get_new_ipaddress((EtherAddr *)rq->bp_chaddr,
					     mltaddr, dhcp_ipaddr,
					     &newaddr,&newConfigPtr,
					     cid_ptr, cid_length, db,
					     resolv_name))
			return 1;
		    if (debug) {
			ipn.s_addr = newaddr;
			syslog(LOG_DEBUG,
			       "DHCPDISCOVER: cid (%s) new address (%s)", str,
			       inet_ntoa(ipn));
		    }
		}
	    }
	    else {
	      /* same address is being assigned as was earlier */
	      /* change lease to zero so that if this lease is expired it is
	       * not assigned to anyone else. In case pings are sent it is 
	       * possible to erroneously give out the same ip to two 
	       * clients unless the lease is zeroed in the database
	       */
		if (loc0_zero_expired_lease((EtherAddr *)rq->bp_chaddr, 
					    cid_ptr, cid_length, db)) {
		    syslog(LOG_DEBUG,
			   "DHCPDISCOVER: cid (%s) Record must exist",str);
		    return 1;
		}
	    }
	}
	else {
	    if (debug)
		syslog(LOG_DEBUG,
		       "DHCPDISCOVER: cid (%s) ciaddr is non zero",str);
	    /* ciaddr MUST be 0 in a DHCPDISCOVER message */
	    /* newaddr = rq->bp_ciaddr.s_addr; */
	    /* newConfigPtr = cf0_get_config(newaddr); */
	    return 1;
	}
	if (resolv_name) {
	    free(resolv_name);
	    resolv_name = (char*)0;
	}
	/* gets here if dhcp_ipaddr set to a valid mapping */
	dhcp_hsname =
	    loc0_get_hostname_from_cid(cid_ptr, cid_length,
				       (EtherAddr *)rq->bp_chaddr, db);
	if (debug >= 2)
	    syslog(LOG_DEBUG, "DHCPDISCOVER assigning hostname (%s)",dhcp_hsname);
	if(dhcp_hsname == (char *)0) {
	    return 1;
	}
	
	/* additional consistency check here for the case where a requested
	 * address is being given out */
	if (debug) {
	    if (dhcp_ipaddr &&
		loc0_is_etherToIP_inconsistent(cid_ptr, cid_length, 
					       (EtherAddr *)rq->bp_chaddr,
					       newaddr, dhcp_hsname, db)){
		/* inconsistency found - offer another address */
		dhcp_ipaddr = 0;/* new address */
		newaddr = 0;
		syslog(LOG_DEBUG, "DHCPDISCOVER retry inconsistent client(%s)",
		       str);
		goto retry_inconsistent;
	    }
	}
	/* send only host part */
	dhcp_hsname = strtok(dhcp_hsname, " .");
#ifdef EDHCP
	/* if ping checking is on then prepare to send the ICMP request
	 */
	if (ping_nonblocking_check && 
	    (ping_number_pending <= DHCPPING_MAXOUT)) {
	    ipn.s_addr = newaddr;
	    ret = send_ping( ping_sd, newaddr, ++ntransmitted, ident);
	    if ( (ret < 0) && (errno != EWOULDBLOCK) ) {
		syslog(LOG_ERR, "Error in send_ping - nonblocking errno=%d",
		       errno);
		return(-1);
	    }
	    if (debug) 
		syslog(LOG_DEBUG,"PING_SENT:cid (%s),ip (%s), name (%s)",
		       str,inet_ntoa(ipn), dhcp_hsname);
	    ping_status = PING_SENT;
	    timeout = ping_timeout;
	}
#endif
#ifdef EDHCP_LDAP
	/* get a specific config if one is available 
	 * this updates the configPtr if there was one in the ldap database */
	newConfigPtr = cf0_get_config((u_long)newaddr, 1);
#endif
	/* compute the lease  to offer */
	offered_lease = find_lease_to_offer((u_long)newaddr, dhcp_lease,
					    newConfigPtr, db);

	if ((ping_status == PING_NONE) || (ping_status == PING_RECV)) {
	    if(newConfigPtr->p_choose_name)
		dh0_encode_dhcp_server_packet(rp->dh_opts,
					      DHCPOFFER,
					      &dhcp_reqParams, offered_lease, 
					      dhcp_msg,
					      dhcp_hsname, dhcp_hsname, newConfigPtr, &dci_data);
	    else
		dh0_encode_dhcp_server_packet(rp->dh_opts,
					      DHCPOFFER,
					      &dhcp_reqParams, offered_lease, 
					      dhcp_msg,
					      dhcp_hsname, 0, newConfigPtr, &dci_data);
	}
	rp->bp_yiaddr.s_addr = (u_long)newaddr;
	
	req = dh0_add_to_dhcp_reqlist(cid_ptr, cid_length, 
				(EtherAddr *)rq->bp_chaddr, newaddr,
				dhcp_hsname, &dhcp_reqParams, dhcp_server,
				dhcp_lease, newConfigPtr, 
				ping_status, ident,  ntransmitted, 
				dhcp_msg, has_sname, rp, rq, &dci_data);
	
	
	if ((ping_status == PING_NONE) || (ping_status == PING_RECV)) {
	    if (debug)
		syslog(LOG_DEBUG,"DHCPOFFER:cid (%s),ip (%s), name (%s)",
		       str,inet_ntoa(rp->bp_yiaddr), dhcp_hsname);
	    if(rp->bp_flags & BROADCAST_BIT)
		sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
	    else
		sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
	    timeout = DHCPOFFER_TIMEOUT;
	}
	time(&TimeNow);
	dh0_addto_timelist(cid_ptr, cid_length, 
			   (EtherAddr *)rq->bp_chaddr,
			   TimeNow+timeout, req);
	break;
    case DHCPREQUEST:
#ifdef PERFORMANCE
	perf_flag = DHCPREQUEST;
	gettimeofday(&perf_time_start,&perf_tz);
#endif 	    
	script_msgtype = DHCPACK;
	if (debug >= 3) {
	    syslog(LOG_DEBUG,
		   "DHCPREQUEST: cid (%s)",str);
	}
	mltaddr = MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
	ipn.s_addr = dhcp_server;
	if( (dhcp_server == mltaddr) || matchhost(inet_ntoa(ipn)) ) {
	    if (debug) {
		syslog(LOG_DEBUG, "DHCPREQUEST:cid (%s) for server (%s)",
		       str,inet_ntoa(ipn));
	    }
	    /* This message is from a client responding to a DHCPOFFER
	     * message from us.
	     */
	    req = dh0_find_dhcp_rqlist_entry(cid_ptr, cid_length);
	    if (req == 0) {		    
		loc0_remove_entry((EtherAddr *)rq->bp_chaddr, 0, cid_ptr,
				  cid_length, db);
		break;
	    }
	    nameaddr_err = 0;
	    typ = 0;
	    brk_flag = 0;
	    if(resolv_name && sgi_resolv_name) {
	        script_msgtype = DHCPOFFER;
		if(resolv_name[1] == ':') {
		    rhsname = &resolv_name[2];
		    resolv_name[1] = '\0';
		    typ = atoi(resolv_name);
		}
		else {
		    rhsname = resolv_name;
		}
		switch(typ) {
		case 0:
		case SELECTED_NAME:
		    /* While adding the check for ip address
		     * have tried to retain the old logic
		     * even the part that interacts with
		     * alt_naming_flag! which is suspicious */
		    nameaddr_err = check_selected_name_address
			(rhsname, &dhcp_msg, 0, rq, req, cid_ptr,
			 cid_length);
		    
		    if (alt_naming_flag) {
			if (nameaddr_err == 2)
			    break;
		    }

		    if(nameaddr_err) {
			/* compute the lease  to offer */
			offered_lease = find_lease_to_offer(req->dh_ipaddr,
							    req->dh_reqLease,
							    req->dh_configPtr,
							    db);
		
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPOFFER, &(req->dh_reqParams),
						      offered_lease, dhcp_msg,
						      req->dh_hsname, req->dh_hsname,
						      req->dh_configPtr, &dci_data);
			rp->bp_yiaddr.s_addr = (u_long)req->dh_ipaddr;
			
			if(rp->bp_flags & BROADCAST_BIT)
			    sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
			else
			    sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
			tmq = dh0_find_timelist_entry(cid_ptr, cid_length);
			time(&TimeNow);
			tmq->tm_value = TimeNow+DHCPOFFER_TIMEOUT;
			brk_flag++;
			break;
		    }
		    else {
			free(req->dh_hsname);
			req->dh_hsname = strdup(rhsname);
			loc0_remove_entry((EtherAddr *)rq->bp_chaddr,0,
					  cid_ptr, cid_length, db);
			if(dhcp_msg) {
			    free(dhcp_msg);
			    dhcp_msg = (char *)0;
			}
		    }
		    break;
		case KEEP_OLD_NAMEADDR:
		case NEW_NAMEADDR:
		    req->dh_ipaddr = dhcp_ipaddr;
		    /* Fall through */
		case KEEP_OLD_NAMEONLY:
		    free(req->dh_hsname);
		    req->dh_hsname = strdup(rhsname);
		    nameaddr_err = check_selected_name_address
			(rhsname, &dhcp_msg, dhcp_ipaddr, rq, req,
			 cid_ptr, cid_length);
		    if (alt_naming_flag) {
			if (nameaddr_err == 2)
			    break;
		    }
		    if (nameaddr_err == 3) {
			u_long mltaddr1;
			dhcp_ipaddr = 0;
			/* must get a new address */
			if(rq->bp_giaddr.s_addr) 
			    mltaddr1 = rq->bp_giaddr.s_addr;
			else
			    mltaddr1 = mltaddr;
			if(cf0_get_new_ipaddress((EtherAddr *)rq->bp_chaddr,
						 mltaddr1, dhcp_ipaddr,
						 &newaddr, &newConfigPtr,
						 cid_ptr, cid_length, db, 
						 (char*)0))
			    return 1;
			req->dh_ipaddr = newaddr;
		    }
		    if (nameaddr_err) {
			/* compute the lease  to offer */
			offered_lease = find_lease_to_offer(req->dh_ipaddr,
							    req->dh_reqLease,
							    req->dh_configPtr,
							    db);
		
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPOFFER, &(req->dh_reqParams),
						      offered_lease, dhcp_msg,
						      req->dh_hsname, req->dh_hsname,
						      req->dh_configPtr, &dci_data);
			rp->bp_yiaddr.s_addr = (u_long)req->dh_ipaddr;

			if(rp->bp_flags & BROADCAST_BIT)
			    sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
			else
			    sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
			tmq = dh0_find_timelist_entry(cid_ptr, cid_length);
			time(&TimeNow);
			tmq->tm_value = TimeNow+DHCPOFFER_TIMEOUT;
			brk_flag++;
			break;
		    }
		    
		    loc0_remove_entry((EtherAddr *)rq->bp_chaddr, 0,
				      cid_ptr, cid_length,db);
		    /* Fall through */
		    if(dhcp_msg) {
			free(dhcp_msg);
			dhcp_msg = (char *)0;
		    }
		    break;
		case OFFERED_NAME:
		default:
		    syslog(LOG_ERR,
			   "Internal Error: Invalid type returned (%d)",
			   typ );
		    /* Fall through */
		case NO_DHCP:
		    loc0_remove_entry((EtherAddr *)rq->bp_chaddr, 0, 
				      cid_ptr, cid_length, db);
		    dh0_remove_from_dhcp_reqlist(cid_ptr, cid_length);
		    dh0_remove_from_timelist(cid_ptr, cid_length);
		    brk_flag++;
		    break;
		}
		if(brk_flag)
		    break;
	    }
	    /* Client has accepted the offer */
	    if(req) {
	        script_msgtype = DHCPACK;
		if (debug) {
		    ipn.s_addr = req->dh_ipaddr;
		    syslog(LOG_DEBUG, "DHCPACK:cid (%s),ip (%s) name (%s)",
			   str, inet_ntoa(ipn), req->dh_hsname);
		}
		if (req->dh_configPtr->p_dnsdomain) {
		    client_domain = req->dh_configPtr->p_dnsdomain;
		}
		else if (req->dh_configPtr->p_nisdomain){
		    client_domain = req->dh_configPtr->p_nisdomain;
		}
		else {
		    client_domain = mydomain;
		}
		dh0_upd_dhcp_rq_entry(req, &dhcp_reqParams, dhcp_lease);
		dh0_remove_system_mapping(cid_ptr, (EtherAddr *)rq->bp_chaddr, cid_length, db);
		dh0_create_system_mapping(req->dh_ipaddr,
					  (EtherAddr *)rq->bp_chaddr,
					  req->dh_hsname,
					  client_domain);
		/* compute the lease  to offer */
		offered_lease = find_lease_to_offer(req->dh_ipaddr,
						    req->dh_reqLease,
						    req->dh_configPtr,
						    db);
		dh0_encode_dhcp_server_packet(rp->dh_opts, 
					      DHCPACK, &(req->dh_reqParams),
					      offered_lease, dhcp_msg,
					      req->dh_hsname, 0,
					      req->dh_configPtr, &dci_data);
		
		rp->bp_yiaddr.s_addr = (u_long)req->dh_ipaddr;
#ifdef EDHCP
		/* do dns update and encode client_fqdn */
		if ( (1 == dhcp_dnsupd_on) && (alt_naming_flag == 0) &&
		     ( (req->dh_reqParams.GET_CLIENT_FQDN == 1) ||
		       (1 == dhcp_dnsupd_always) ) ) {
		    dhcp_dnsadd(0, req->dh_ipaddr, req->dh_hsname, 
				client_domain, req->dh_reqLease, 
				dci_data.client_fqdn, 
				req->dh_configPtr->ddns_conf);
		    dh0_append_client_fqdn(rp->dh_opts, dci_data.client_fqdn);
		}
#endif
		if(rp->bp_flags & BROADCAST_BIT)
		    sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
		else
		    sendreply(rp, 0, has_sname, 0, DHCP_REPLY);

#ifdef EDHCP
		/* do dns update and encode client_fqdn */
		if ( (1 == dhcp_dnsupd_on) && (dhcp_dnsupd_beforeACK == 0) &&
		     (0 == alt_naming_flag) &&
		     ( (req->dh_reqParams.GET_CLIENT_FQDN == 1) ||
		       (1 == dhcp_dnsupd_always) ) ) {
		    dhcp_dnsadd(1, req->dh_ipaddr, req->dh_hsname, 
				client_domain, req->dh_reqLease, 
				dci_data.client_fqdn,
				req->dh_configPtr->ddns_conf);
		}
#endif
		
		if (client_domain) {
		    sprintf(hostname1,"%s.%s", req->dh_hsname,
			    client_domain);
		} else {
		    strcpy(hostname1, req->dh_hsname);
		}
		loc0_create_update_entry(cid_ptr, cid_length, 
					 (EtherAddr *)rq->bp_chaddr,
					 req->dh_ipaddr,
					 hostname1, msg_recv_time,
					 offered_lease,
					 db);
		dh0_remove_from_timelist(cid_ptr, cid_length);
		dh0_remove_from_dhcp_reqlist(cid_ptr,cid_length);
	    }
	}
	else if(dhcp_server == 0) {
	    /* Client is renewing or rebinding the lease, OR
	     * it is from a client verifying its IP address
	     * in the case of INIT-REBOOT.
	     */
	    if( (dhcp_ipaddr) && (rq->bp_ciaddr.s_addr == 0) ) { 
		if (debug) {
		    ipn.s_addr = dhcp_ipaddr;
		    syslog(LOG_DEBUG,
			   "DHCPREQUEST: cid(%s) Init-Reboot from client (%s)",
			   str, inet_ntoa(ipn));
		}
		/* INIT-REBOOT State */
		mltaddr =
		    MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
		netaddr =
		    rq->bp_giaddr.s_addr ? rq->bp_giaddr.s_addr : mltaddr;

		if((aval = cf0_is_req_addr_valid(dhcp_ipaddr, netaddr)) == 0) {
		    rval = loc0_is_ipaddress_for_cid((EtherAddr *)rq->
						     bp_chaddr,dhcp_ipaddr,
						     cid_ptr, cid_length, db);
		    if(rval == 0) {
			newConfigPtr = cf0_get_config(dhcp_ipaddr, 1);
			if(newConfigPtr == 0)
			    return 1;
			offered_lease = find_lease_to_offer(dhcp_ipaddr,
							    dhcp_lease,
							    newConfigPtr,
							    db);
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPACK, &dhcp_reqParams,
						      offered_lease, dhcp_msg,
						      0, 0, newConfigPtr,
						      &dci_data);
			rp->bp_yiaddr.s_addr = dhcp_ipaddr;
			if (loc0_update_lease((EtherAddr *)rq->bp_chaddr,
					      cid_ptr, cid_length,
					      msg_recv_time, 
					      offered_lease,
					      db)!=0){
			    /* there was a DHCPNAK here that was unlikely
			     * to take place. Instead just place a error 
			     * message and remain silent */
			    ipn.s_addr = dhcp_ipaddr;
			    syslog(LOG_ERR,
				   "Init-Reboot from client (%s)"
				   "- Lease update error.",
				   inet_ntoa(ipn));
			    return 0;
			} 
			else if (debug)
			    syslog(LOG_DEBUG,
				   "DHCPACK to Init-Reboot from client (%s)",
				   inet_ntoa(rp->bp_yiaddr));
		    }
		    else if(rval == 2) {	/* Incorrect mapping */
			/* we should NAK only if this ipaddress is 
			 * assigned to someone else otherwise keep quiet
			 */
			char tmp_cid[MAXCIDLEN];
			int tmp_cidlen;
			if (getCidByIpAddr(tmp_cid, &tmp_cidlen, 
					   &dhcp_ipaddr, db)) {
			    dh0_encode_dhcp_server_packet(rp->dh_opts,
							  DHCPNAK, 0, 0,
							  INCORRECT_MAPPING_MSG,
							  0, 0, 0, &dci_data);
			    script_msgtype = DHCPNAK;
			    if (debug) {
				ipn.s_addr = dhcp_ipaddr;
				syslog(LOG_DEBUG,
				       "DHCPNAK to Init-Reboot from client (%s) - Incorrect Mapping.",
				       inet_ntoa(ipn));
			    }
			    /* rp->bp_yiaddr.s_addr = dhcp_ipaddr; */
			}
			else {
			    if (debug) {
				ipn.s_addr = dhcp_ipaddr;
				syslog(LOG_DEBUG,
				       "No reply to Init-Reboot from client (%s).",
				       inet_ntoa(ipn));
			    }
			    return 0; /* remain silent 
				       * we don't have a binding for this ip
				       * although there is a binding for 
				       * this cid */
			}
		    }
		    else {
			return 0;		/* Remain Silent */
		    }
		}
		else {	/* Address is invalid: on wrong subnet */
		    if (aval != 2) {
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPNAK, 0, 0,
						      WRONG_SUBNET_MSG,
						      0, 0, 0, &dci_data);
			if (debug) {
			    ipn.s_addr = dhcp_ipaddr;
			    syslog(LOG_DEBUG,
				   "DHCPNAK to Init-Reboot from client (%s) - Incorrect Subnet.",
				   inet_ntoa(ipn));
			}
			script_msgtype = DHCPNAK;
		    }
		    else {
			if (debug) {
			    ipn.s_addr = dhcp_ipaddr;
			    syslog(LOG_DEBUG, "No Reply to Init-Reboot from client (%s).",
				   inet_ntoa(ipn));
			}
			return 0;
		    }
		}
	    }
	    else if( (dhcp_ipaddr == 0) && (rq->bp_ciaddr.s_addr) ) {
		/* Lease Renewal/Rebind  message from the client */
		if (debug)
		    syslog(LOG_DEBUG,
			   "DHCPREQUEST: Renew/Rebind cid (%s) ciaddr (%s)",
			   str, inet_ntoa(rq->bp_ciaddr));
		rp->bp_yiaddr.s_addr = rq->bp_ciaddr.s_addr;
		
		if ((rval=loc0_is_ipaddress_for_cid((EtherAddr *)rq->
						    bp_chaddr, 
						    rq->bp_ciaddr.s_addr,
						    cid_ptr, cid_length, 
						    db)==0)) {
		    newConfigPtr = cf0_get_config(rq->bp_ciaddr.s_addr, 1);
		    if(newConfigPtr == 0)
			return 1;
		    if ((aval = cf0_is_req_addr_valid(rq->bp_ciaddr.s_addr,
					      newConfigPtr->p_netnum)) != 0) {
			/* the old address valid check with giaddr 
			   would not work but this one will work */
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPNAK, 0, 0,
						      INCORRECT_MAPPING_MSG,
						      0, 0, 0, &dci_data);
			script_msgtype = DHCPNAK;
			if (debug) {
			    syslog(LOG_DEBUG,
				   "DHCPNAK to Renew/Rebind from client (%s)"
				   "- Incorrect Range or Subnet.", 
				   inet_ntoa(rq->bp_ciaddr));
			}
		    }
		    else {
			offered_lease = 
			    find_lease_to_offer(rq->bp_ciaddr.s_addr,
						dhcp_lease,
						newConfigPtr,
						db);
			dh0_encode_dhcp_server_packet(rp->dh_opts,
						      DHCPACK, 
						      &dhcp_reqParams,
						      offered_lease, 
						      dhcp_msg,
						      0, 0, 
						      newConfigPtr,
						      &dci_data);
			if (loc0_update_lease((EtherAddr *)rq->bp_chaddr,cid_ptr, cid_length,
					      msg_recv_time,
					      offered_lease,
					      db) != 0) {
			    /* there was a DHCPNAK coded here that was unlikely
			     * to take place. Instead just place a debug 
			     * message and remain silent */
			    
			    syslog(LOG_ERR,
				   "Renew/Rebind from client (%s)"
				   "- Lease Update Error.", 
				   inet_ntoa(rp->bp_yiaddr));
			}
			else if (debug)
			    syslog(LOG_DEBUG,
				   "DHCPACK to Renew/Rebind from client (%s)",
				   inet_ntoa(rp->bp_yiaddr));
		    }
		}
		else {
		    /* I am not the orignal server who assigned this lease but
		     * need to check if I can give the requested ip address
		     * a new lease
		     * NAK only in case we have a different mapping - else
		     * if no mapping be silent
		     */
		    if (rval == 2) {
			char tmp_cid[MAXCIDLEN];
			int tmp_cidlen;
			if (getCidByIpAddr(tmp_cid, &tmp_cidlen, 
					   (ulong*)&rq->bp_ciaddr.s_addr, db)) {
			    dh0_encode_dhcp_server_packet(rp->dh_opts,
							  DHCPNAK, 0, 0,
							  WRONG_SERVER_MSG,
							  0, 0, 0, &dci_data);
			    script_msgtype = DHCPNAK;
			    if (debug) {
				syslog(LOG_DEBUG,
				       "DHCPNAK to Renew/Rebind from client (%s)"
				   "- Incorrect Server.", 
				       inet_ntoa(rq->bp_ciaddr));
			    }
			}
			else {
			    if (debug) {
				syslog(LOG_DEBUG,
				       "No Reply to Renew/Rebind from client (%s)",
				       inet_ntoa(rq->bp_ciaddr));
			    }
			    return 1;
			}
		    }
		    else
			return 1;
		}
	    } /* CHANGE added else when could not understand message -
		 earlier the received message was just sent back  */
	    else {		/* remain quiet */
		return 0;
	    }
	    if(rp->bp_flags & BROADCAST_BIT)
		sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
	    else
		sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
	}
	else if(dhcp_server != 0) {
	    /* Client has selected someone else */
	    dh0_remove_from_timelist(cid_ptr, cid_length);
	    dh0_remove_from_dhcp_reqlist(cid_ptr, cid_length);
	    loc0_remove_entry((EtherAddr *)rq->bp_chaddr, 0, cid_ptr, 
			      cid_length, db);
	}
	break;
    case DHCPDECLINE: 
	{
	    iaddr_t	ipn, ipn1;
	    char	tmp_cid[MAXCIDLEN];
	    u_long	res_node;
	    char	tmp_ipn[64];
	    
	    ipn.s_addr = loc0_get_ipaddr_from_cid((EtherAddr *)rq->bp_chaddr,
						  cid_ptr, cid_length, db);
	    if ( ipn.s_addr != dhcp_ipaddr ) {
		ipn1.s_addr = dhcp_ipaddr;
		strcpy(tmp_ipn, inet_ntoa(ipn1));
		syslog(LOG_DEBUG, 
		       "DHCPDECLINE: MISMATCH!Req IP is %s and IP in db is %s",
		       tmp_ipn, inet_ntoa(ipn));
		/* ipn = Zero means no record in the database  */
	    }
	    syslog(LOG_WARNING,
		   "DHCPDECLINE: cid (%s) marking stolen %s",str, 
		   dhcp_ipaddr);
	    /* instead of removing the lease mark the address as stolen
	     * so that it is not given out again */
	    /* the original record for this ipaddress must be removed */    
	    loc0_remove_entry((EtherAddr *)rq->bp_chaddr, 3, 
			      cid_ptr, cid_length, db);
	    res_node = dhcp_ipaddr;
	    sprintf(tmp_cid, "STOLEN-%d", ++msg_recv_time);
	    loc0_create_update_entry(tmp_cid, strlen(tmp_cid), 
				     (EtherAddr*)&res_node, res_node,
				     tmp_cid, 0, STOLEN_LEASE, db);
	    script_msgtype = DHCPDECLINE;
	}
	break;
    case DHCPRELEASE:
	if (debug)
	    syslog(LOG_DEBUG, "DHCPRELEASE: cid (%s)", str);
	script_msgtype = DHCPRELEASE;
#ifdef EDHCP
	if ( (dhcp_dnsupd_on == 1) && (0 == alt_naming_flag) ) { 
	    /* this should check if the 
	       record was added before deleting */
	    u_long addrfromCid;
	    dhcpConfig *cfgPtr;
	    
	    addrfromCid = loc0_get_ipaddr_from_cid((EtherAddr *)rq->bp_chaddr,
						   cid_ptr, cid_length, db);
	    if (addrfromCid != 0) {
		cfgPtr = cf0_get_config(addrfromCid, 0);
		dhcp_dnsdel(addrfromCid, cfgPtr->ddns_conf);
	    }
	}
#endif
	dh0_remove_system_mapping(cid_ptr, (EtherAddr *)rq->bp_chaddr, cid_length, db);
	/*
	 * Instead of removing the record just set the lease to expire
	 * some clients do an init-reboot after release (ISC clients)
	*/
	loc0_update_lease((EtherAddr *)rq->bp_chaddr, cid_ptr, cid_length,
			  msg_recv_time, 0, db);
	break;
	
    case DHCPINFORM:
	if (debug)
	    syslog(LOG_DEBUG, "DHCPINFORM: cid (%s)", str);
	/* respond with a DHCPACK as if a DHCPDISCOVER was received
	 * except don't send a lease and address
	 */
	if(rq->bp_ciaddr.s_addr != 0) {
	    dhcp_hsname =
		loc0_get_hostname_from_cid(cid_ptr, cid_length,
					   (EtherAddr *)rq->bp_chaddr, db);
	    dhcp_hsname = strtok(dhcp_hsname, " .");/*send only host part*/
	    
	    if(dhcp_hsname == (char *)0) {
		return 1;
	    }
	    newConfigPtr = cf0_get_config(rq->bp_ciaddr.s_addr, 1);
	    if(newConfigPtr == 0)
		return 1;
	    /* note that we are passing a negative value for lease time
	     * to inform the routine not to fill up lease time
	     */
	    if(newConfigPtr->p_choose_name)
		dh0_encode_dhcp_server_packet(rp->dh_opts,
					      DHCPACK, &dhcp_reqParams, 0,
					      dhcp_msg, dhcp_hsname,
					      dhcp_hsname, newConfigPtr, 
					      &dci_data);
	    else
		dh0_encode_dhcp_server_packet(rp->dh_opts,
					      DHCPACK, &dhcp_reqParams, 0,
					      dhcp_msg, dhcp_hsname, 0,
					      newConfigPtr, &dci_data);
	    /* should we create system_mapping */
	    if(rp->bp_flags & BROADCAST_BIT)
		sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
	    else
		sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
	}
	break;
	/* this is not part of the standard yet but this functionality has
	 * been added to support polling of the server to verify whether the
	 * server is running and serving addresses
	 */
    case DHCPPOLL:
	if (debug)
	    syslog(LOG_DEBUG, "DHCPPOLL: cid (%s)", str);
	rp->bp_yiaddr.s_addr = rq->bp_ciaddr.s_addr;
		
	dh0_encode_dhcp_server_packet(rp->dh_opts,
				      DHCPPRPL, &dhcp_reqParams,
				      0, 0,
				      0, 0, 0, &dci_data);
	if(rp->bp_flags & BROADCAST_BIT)
	    sendreply(rp, 0, has_sname, 1, DHCP_REPLY);
	else
	    sendreply(rp, 0, has_sname, 0, DHCP_REPLY);
	break;
    default:
	break;
    }
    if (script_file_to_run && (script_state_change_index > 0)) {
	execute_script();
    }
    if(dhcp_hsname) {
	free(dhcp_hsname);
	dhcp_hsname = (char *)0;
    }
    if (resolv_name)
	free(resolv_name);
    free(cid_ptr);/*CHECK*/
    return 0;
}

/*
 * Perform Reverse ARP lookup using /etc/ether and /etc/hosts (or
 * their NIS equivalents)
 *
 * Returns 1 on success, 0 on failure.
 */
int
reverse_arp(struct ether_addr *eap, iaddr_t *iap)
{
    register struct hostent *hp;
    char host[512];

    /*
     * Call routine to access /etc/ethers or its NIS equivalent
     */
    if (ether_ntohost(host, eap))
	return (0);
	
    /*
     * Now access the hostname database.
     */
    if ((hp = gethostbyname(host)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", host, hstrerror(h_errno));
	return (0);
    }

    /*
     * Return primary Internet address
     */
    iap->s_addr = *(u_long *)(hp->h_addr_list[0]);
    return (1);
}

char	hostmap[] = "hosts.byname";
/*
 *	A new diskless workstation/autoreg is asking for
 *		initial boot/IPADDR
 */
int
handle_diskless(struct bootp *rq, struct bootp *rp)
{
    char	*t_host, *domain, *ypmaster, *str_match, *t_domain;
    char	hostname[256], alias[256];
    int	err = 0;

    if (yp_get_default_domain(&domain)) {
	syslog(LOG_ERR, "Autoreg: can't get domain");
	return(-1);
    }

    t_host = (char *)rq->vd_clntname;
    if (debug)
	syslog(LOG_DEBUG, "Autoreg: domain=%s, host=%s", domain, t_host);

    gethostname(hostname, sizeof(hostname));
    if (gethostbyname(t_host) != NULL) {
	/* play safe */
	rp->vd_flags = VF_RET_IPADDR;
	return(0);
    }

    if (!reg_netmask) {
	if (str_match = strrchr(reg_hostnet, '.'))
	    *str_match = 0;
    }

    /* get ypmaster ipaddr */
    if (yp_master(domain, hostmap, &ypmaster)) {
	if (debug)
	    syslog(LOG_DEBUG, "no \"%s\" YPmaster found for \"%s\" domain",
		   hostmap, domain);
	return(-1);
    }

    /* check and prevent broadcast storm */
    if (!matchhost(ypmaster)) {
	if (!forwarding) {
	    if (debug)
		syslog(LOG_DEBUG, "Autoreg: I am neither YPMASTER nor GATEWAY");
	    return(-1);
	}
	if (debug)
	    syslog(LOG_DEBUG, "Autoreg: I am GATEWAY");
	/* XXX
	 *	finding out if the requestor and YPMASTER are
	 *	at same sub-net is impossible. So let's
	 *	allow gateway issue an extra broadcast msg.
	 */
    } else if (debug)
	syslog(LOG_DEBUG, "Autoreg: I am YPMASTER");

    /* check/create full host name */
    if ((t_domain = strchr((const char *)rq->vd_clntname, '.')) != NULL) {
	/* if it not for the domain i belong, then ignore */
	if (strcmp(domain, t_domain+1)) {
	    if (debug)
		syslog( LOG_DEBUG, "Autoreg: req_domain(%s)<>domain(%s)",
			t_domain+1, domain);
	    return(-1);
	}
	strcpy(hostname, t_host);
	*t_domain = 0;
	strcpy(alias, t_host);
	*t_domain = '.';
    } else {
	strcpy(hostname, t_host);
	strcat(hostname, ".");
	strcat(hostname, domain);
	strcpy(alias, t_host);
    }

    /* can't create, other bootp may be creating at the same time
       or, there is no NIS */
    err = registerinethost(hostname,reg_hostnet,reg_netmask,0,alias);
    if ((u_long)err <= YPERR_BUSY) {
	syslog(LOG_ERR, "Autoreg: REGISTER failed(0x%x)", err);
	return(-1);
    }

    rp->vd_flags = VF_NEW_IPADDR;
    rp->bp_yiaddr.s_addr = (u_long)(err);

    if (debug)
	syslog( LOG_DEBUG, "Autoreg: %s REGISTERED as %s",
		hostname, inet_ntoa(*(struct in_addr *)&err));
    return(0);
}

/*
 * Process BOOTREQUEST packet.
 *
 * <SGI CHANGES>
 *
 * Our version does implement the hostname processing
 * specified in RFC 951.  If the client specifies a hostname, then
 * only that hostname will respond.  We do one additional thing
 * that the RFC does not specify:  if the server name is not specified,
 * then we fill it in, so that the client knows the name as well
 * as the IP address of the server who responded.
 *
 * Our version also implements the forwarding algorithm specified
 * in RFC 951, but not used in the Stanford version of this code.
 * If a request is received that specifies a host other than this
 * one, check the network address of that host and forward the
 * request to him if he lives on a different wire than the one
 * from which the request was received.
 *
 * Another change from the RFC and the original version of the
 * code is that the BOOTP server will respond to the client even
 * if the client is not listed in the BOOTP configuration file,
 * provided that the client already knows his IP address.  The
 * reason for this change is to supply a bit more ease of use.
 * If there is a file out there that you want to boot, it seems
 * a shame to have to edit bootptab on the server before
 * you can boot the lousy file.  All the more so because if you
 * have already configured the IP address of your system, then
 * the bootptab information is redundant.  (This will make the
 * transition from XNS network boot to IP network boot a little
 * less painful).
 *
 * <END SGI CHANGES>
 */
void
request(struct bootp *rq)
{
    /* register struct bootp *rq = (struct bootp *)buf; */
    struct bootp rp;
    char path[MAXPATHLEN], file[128];
    iaddr_t fromaddr;
    register struct hosts *hp;
    register struct hostent *hostentp;
    register n;
    int has_sname;	/* does rq have nonempty bp_sname? */

    if(sleepmode)	/* allow debugging */
        sleep(10);
    rp = *rq;	/* copy request into reply */
    rp.bp_op = BOOTREPLY;

    /* Let's first check if it is a DHCP message */
    if(rq->dh_magic == VM_DHCP) {
	if(dh0_get_dhcp_msg_type(rq->dh_opts) == -1) {
	    /* This is a RFC1533 Only message */
	    process_rfc1533_bootp_message(&rp);
	    /* Fall thru to process the bootp stuff */
	}
	else {
	    /* This is a DHCP Message */
	    if(ProclaimServer) {
		process_dhcp_message(rq, &rp);
		return;
	    }
	    else {
		if (debug)
		    syslog(LOG_DEBUG,
		      "DHCP request from client (%s). DHCP server not enabled.",
		      ether_ntoa((struct ether_addr *)rq->bp_chaddr));
		return;
	    }
	}
    }

    /* prevent possible buffer overrun */
    rq->bp_file[sizeof(rq->bp_file)-1] = '\0';  

    /* This is a non dhcp, regular bootp packet */
    /*
     * Let's resolve client's ipaddr first.
     */
    if (rq->bp_yiaddr.s_addr != 0 && rq->bp_giaddr.s_addr != 0) { 
	/*
	 * yiaddr has already been filled in by forwarding bootp
	 */
	hp = (struct hosts *) 0;
    } else if (rq->bp_ciaddr.s_addr == 0) {
	/*
	 * client doesnt know his IP address, 
	 * search by hardware address.
	 */
	hp = lookup_btabm(rq->bp_htype, rq->bp_chaddr);
	if (hp == (struct hosts *)0) {
	    /*
	     * The requestor isn't listed in bootptab.
	     */
	    if( (rp.vd_magic == VM_SGI) && (rp.vd_flags == VF_GET_IPADDR) ) {
		if((hostentp = gethostbyname((const char *)rp.vd_clntname)) == 0) {
		    rp.bp_yiaddr.s_addr = 0;
		} else {
		    rp.bp_yiaddr.s_addr= *(u_long *)(hostentp->h_addr_list[0]);
		    rp.vd_flags = VF_RET_IPADDR;
		}
	    }

	    /*
	     * Try Reverse ARP using /etc/ethers or NIS before
	     * giving up.
	     */
	    if (!reverse_arp((struct ether_addr *)rq->bp_chaddr, &rp.bp_yiaddr)) {
		/*
		 * Don't trash the request at this point,
		 * since it may be for another server with
		 * better tables than we have.
		 */
		if (debug)
		    syslog(LOG_DEBUG,
			   "No IP address for %s found in /etc/ethers or ethers map, can't reply",
			    ether_ntoa((struct ether_addr *)rq->bp_chaddr));
		/* Play it safe */
		rp.bp_yiaddr.s_addr = 0;
	    }
	} else {
	    rp.bp_yiaddr = hp->iaddr;
	}
    } else {
	/* search by IP address */
	hp = lookup_btabi(rq->bp_ciaddr);
    }

    fromaddr = rq->bp_ciaddr.s_addr ? rq->bp_ciaddr : rp.bp_yiaddr;

    /*
     * Check whether the requestor specified a particular server.
     * If not, fill in the name of the current host.  If a
     * particular server was requested, then don't answer the
     * request unless the name matches our name or one of our
     * aliases.
     */
    has_sname = rq->bp_sname[0] != '\0';
    if (!has_sname)
	strncpy((char *)rp.bp_sname, myname, sizeof(rp.bp_sname));
    else if (!matchhost((char *)rq->bp_sname)) {
	iaddr_t destaddr;

	if (debug)
	    syslog(LOG_DEBUG, "%s: request for server %s",
		   iaddr_ntoa(fromaddr), rq->bp_sname);
	/*
	 * Not for us.
	 */
	if (!forwarding)
	    return;
	/*
	 * Look up the host by name and decide whether
	 * we should forward the message to him.
	 */
	if ((hostentp = gethostbyname((const char *)rq->bp_sname)) == 0) {
	    syslog(LOG_INFO, "%s: request for unknown server %s",
		   iaddr_ntoa(fromaddr), rq->bp_sname);
	    return;
	}
	destaddr.s_addr = *(u_long *)(hostentp->h_addr_list[0]);
	/*
	 * If the other server is on a different cable from the
	 * requestor, then forward the request.  If on the same
	 * wire, there is no point in forwarding.  Note that in
	 * the case that we don't yet know the IP address of the
	 * client, there is no way to tell whether the client and
	 * server are actually on the same wire.  In that case
	 * we forward regardless.  It's redundant, but there's
	 * no way to get around it.
	 */
	if (!samewire(&fromaddr, &destaddr)) {
		/*
		 * If we were able to compute the client's Internet
		 * address, pass that information along in case the
		 * other server doesn't have the info.
		 */
		rq->bp_yiaddr = rp.bp_yiaddr;
		forward(rq, &fromaddr, &destaddr);
	}
	return;
    }
    if ( (fromaddr.s_addr == 0) && (rq->vd_magic == VM_AUTOREG) &&
	 rq->vd_clntname && rq->vd_clntname[0] ) {
	if (debug)
	    syslog(LOG_DEBUG, "Autoreg starts");
	if (handle_diskless(rq, &rp))
	    return;
	fromaddr = rp.bp_yiaddr;
    }
    /*
     * If we get here and the 'from' address is still zero, that
     * means we don't recognize the client and we can't pass the buck.
     * So we have to give up.
     */
    if (fromaddr.s_addr == 0)
	return;

    if (rq->bp_file[0] == 0) {
	/* client didnt specify file */
	if (hp == (struct hosts *)0 || hp->bootfile[0] == 0)
	    strcpy(file, defaultboot);
	else
	    strcpy(file, hp->bootfile);
    } else {
	/* client did specify file */
	strcpy(file, (const char *)rq->bp_file);
    }
    if (file[0] == '/')	/* if absolute pathname */
	strcpy(path, file);
    else {			/* else look in boot directory */
	strcpy(path, homedir);
	strcat(path, "/");
	strcat(path, file);
    }
    /* try first to find the file with a ".host" suffix */
    n = strlen(path);
    if (hp != (struct hosts *)0 && hp->host[0] != 0) {
	strcat(path, ".");
	strcat(path, hp->host);
    }
    if (access(path, R_OK) < 0) {
	path[n] = 0;	/* try it without the suffix */
	if (access(path, R_OK) < 0) {
	    /*
	     * We don't have the file.  Don't respond unless
	     * the client asked for us by name, in case some
	     * other server does have the file.  If he asked
	     * for us by name, send him back a null pathname
	     * so that he knows we don't have his boot file.
	     */
	    if (rq->bp_sname[0] == 0) {
		if (debug)	/* Bug #767501 */
		    syslog(LOG_DEBUG,
			   "%s requested boot file %s: access failed (%m)",
			   iaddr_ntoa(fromaddr), path);
		
		return;		/* didnt ask for us */
	    }
	    syslog(LOG_ERR,
		"%s requested boot file %s: access failed (%m)",
			iaddr_ntoa(fromaddr), path);
	    path[0] = '\0';
	}
    }
    if (path[0] != '\0') {
	if (strlen(path) > sizeof(rp.bp_file)-1) {
	    syslog(LOG_ERR, "%s: reply boot file name %s too long",
		   iaddr_ntoa(fromaddr), path);
	    path[0] = '\0';
	} else
	    syslog(LOG_INFO, "reply to %s: boot file %s", 
		   iaddr_ntoa(fromaddr), path);
    }
    strcpy((char *)rp.bp_file, path);
    sendreply(&rp, 0, has_sname, 0, BOOTSTRAP_REPLY);
}

/*
 * Select the address of the interface on this server that is
 * on the same net as the 'from' address.
 *
 * Return value
 *	TRUE if match
 *	FALSE if no match
 */
int
bestaddr(iaddr_t *from, iaddr_t *answ)
{
    register struct netaddr *np;
    int match = 0;
    int i;

    if (from->s_addr == 0) {
		answ->s_addr = myhostaddr.s_addr;
    } else {
	/*
	 * Search the table of nets to which the server is connected
	 * for the net containing the source address.
	 */
	for (np = nets; np->netmask != 0; np++)
	    if ((from->s_addr & np->netmask) == np->net) {
		answ->s_addr = np->myaddr.s_addr;
		match = 1;
		break;
	    }
	    else {
		if ((np->first_alias != 0) && (np->last_alias != 0)) {
		    for (i = np->first_alias; i <= np->last_alias; i++) {
			if ((from->s_addr & np->netmask) == 
			    (myaddrs[i] & np->netmask)) {
			    answ->s_addr = np->myaddr.s_addr; /* ?? alias ?? */
			    match = 1;
			    break;
			}
		    }
		    if (match == 1)
			break;
		}
	    }
	
	/*
	 * If no match in table, default to our 'primary' address
	 */
	if (np->netmask == 0)
	    answ->s_addr = myhostaddr.s_addr;
    }
    return match;
}


/*
 * Process BOOTREPLY packet (something is using us as a gateway).
 */
void
reply(struct bootp *bp)
{
    /* struct bootp *bp = (struct bootp *)buf; */
    iaddr_t dst, gate;

    dst = bp->bp_yiaddr.s_addr ? bp->bp_yiaddr : bp->bp_ciaddr;

    if (debug)
	syslog(LOG_DEBUG, "forwarding BOOTP reply from %s to %s",
		bp->bp_sname, iaddr_ntoa(dst));

    /*
     * Try to compute a better giaddr in case we didn't know the
     * client's IP address when we forwarded the original request.
     * The remote server would not be returning the request unless
     * it contained the client's Internet address, so this time
     * we should get the right answer.
     */
    if (bestaddr(&dst, &gate))
	bp->bp_giaddr = gate;
    else
	syslog(LOG_ERR, "reply: can't find net for %s", iaddr_ntoa(dst));

    sendreply(bp, 1, 0, 0, BOOTSTRAP_REPLY);
}

/*
 * Forward a BOOTREQUEST packet to another server.
 *
 *	RFC 951 (7.3) implies no-forward in case
 *		bp_ciaddr = 0.
 */
void
forward(struct bootp *bp, iaddr_t *from, iaddr_t *dest)
{
    struct sockaddr_in to;

    /*
     * If hop >= 3, just discard(RFC951 says so).
     */
    if (bp->bp_hops >= 3)
		return;
    bp->bp_hops++;

    /*
     * If giaddr is 0, then I am the immediate gateway.
     * So, set myaddr for successing forwarders to send
     * reply to me directly.
     */
    if (!bp->bp_giaddr.s_addr) {
	(void) bestaddr(from, &bp->bp_giaddr);
    }

    to.sin_family = AF_INET;
    to.sin_port = htons(IPPORT_BOOTPS);
    to.sin_addr.s_addr = dest->s_addr;	/* already in network order */

    if (debug)
	syslog(LOG_DEBUG, "forwarding BOOTP request to %s(%s)",
		bp->bp_sname, iaddr_ntoa(*dest));

    if (sendto(s, (caddr_t)bp, sizeof *bp, 0, &to, sizeof to) < 0)
	syslog(LOG_ERR, "forwarding to %s failed (%m)", 
		iaddr_ntoa(to.sin_addr));
}

/*
 * Send a reply packet to the client.  'forward' flag is set if we are
 * not the originator of this reply packet.
 */

/* The performance metrics calculations are done at 3 different places 
   for precision purposes. It could be simply done once at the top but then 
   the time calculations wouldn't be precise (9/24/97)
   */


void
sendreply(struct bootp *bp, int forward,
	  int has_sname, int broadcast_it, int reply_type)
{
  iaddr_t dst;
  struct sockaddr_in to;
  char *bufp;
  int buflen;
  int  itype;
  
  to = sin;
  to.sin_port = htons(IPPORT_BOOTPC);
  /*
   * If the client IP address is specified, use that
   * else if gateway IP address is specified, use that
   * else make a temporary arp cache entry for the client's NEW 
   * IP/hardware address and use that.
   */
  if (bp->bp_ciaddr.s_addr) {
    dst = bp->bp_ciaddr;
    if (debug >= 2)
      syslog(LOG_DEBUG, "reply to client (ciaddr) %s", inet_ntoa(dst));
    
  } else if (bp->bp_giaddr.s_addr && forward == 0) {
    dst = bp->bp_giaddr;
    to.sin_port = htons(IPPORT_BOOTPS);
    if (debug >= 2)
      syslog(LOG_DEBUG, "reply via gateway (giaddr) %s", inet_ntoa(dst));
  } else {
    dst = bp->bp_yiaddr;
    if (debug >= 2)
      syslog(LOG_DEBUG, "reply that IP address (yiaddr) is %s", inet_ntoa(dst));
    if (dst.s_addr != 0) 
      setarp(&dst, bp->bp_chaddr, bp->bp_hlen);
  }
  
  if( (forward == 0) && (reply_type == BOOTSTRAP_REPLY) ) {
    /*
     * If we are originating this reply, we
     * need to find our own interface address to
     * put in the bp_siaddr field of the reply.
     * If this server is multi-homed, pick the
     * 'best' interface (the one on the same net
     * as the client).
     */
    int gotmatch = 0;
    gotmatch = bestaddr(&dst, &bp->bp_siaddr);
    if (bp->bp_giaddr.s_addr == 0) {
      if (gotmatch == 0) {
	syslog(LOG_ERR, "can't reply to %s (unknown net)", inet_ntoa(dst));
	return;
      }
      bp->bp_giaddr.s_addr = bp->bp_siaddr.s_addr;
    } else if (!gotmatch && has_sname) {
      struct hostent *hp;
      
      /*
       * Use the address corresponding to the name
       * specified by the client.
       */
      if ((hp = gethostbyname((const char *)bp->bp_sname))) {
	bp->bp_siaddr = *(struct in_addr *)hp->h_addr;
      }
    }
  }
  
  if (!(bp->bp_giaddr.s_addr && (forward == 0))) { 
    if(ProclaimServer && (broadcast_it || (dst.s_addr == 0))) { 
      if(rsockout_err)
	return;
      if(MULTIHMDHCP) {
	np_send =
	  forward ? sr_match_address(bp->bp_giaddr.s_addr): np_recv;
	if(np_send) {
	  itype = sr_get_interface_type(np_send->ifname);
	  if(itype == TYPE_UNSUPPORT) {
	    syslog(LOG_ERR, "Unsupported interface `%s`",
		   np_send->ifname);
	    return;
	  }
	  sr_create_ip_send_packet(&bufp, &buflen, bp, itype);
#ifdef PERFORMANCE
	  if (perf_flag == DHCPDISCOVER){
	    discover_ctr ++;
	    gettimeofday(&perf_time_end, &perf_tz);
	    discover_time += (perf_time_end.tv_usec - perf_time_start.tv_usec) + (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec));
	    if (!(discover_ctr % 100))
	      syslog(LOG_DEBUG,"The average time to process %lu DISCOVER messages = %lu usec.",discover_ctr, discover_time/discover_ctr);  
	  }
	  else if(perf_flag == DHCPREQUEST){
	    request_ctr ++;
	    gettimeofday(&perf_time_end, &perf_tz);
	    request_time += (perf_time_end.tv_usec - perf_time_start.tv_usec) + (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec)); 
	    if (!(request_ctr % 100))
	      syslog(LOG_DEBUG,"The average time to process %lu REQUESTS = %lu usec",request_ctr, request_time/request_ctr);  
	  }
#endif
	  
	  if ( (sendto(np_send->rsockout, bufp, buflen, 0,
		       &braw_addr, sizeof (SCIN))) != buflen ) {
	    syslog(LOG_ERR, "Unable to broadcast packet (%m)");
	  }
	}
	else {
	  syslog(LOG_ERR, "Cannot send broadcast (interface?)");
	}
      }
      else {
	itype = sr_get_interface_type(nets[0].ifname);
	if(itype == TYPE_UNSUPPORT) {
	  syslog(LOG_ERR, "Unsupported interface `%s`",
		 nets[0].ifname);
	  free(bufp);
	  return;
	}
	sr_create_ip_send_packet(&bufp, &buflen, bp, itype);
	
#ifdef PERFORMANCE
	if (perf_flag == DHCPDISCOVER){
	  discover_ctr ++;
	  gettimeofday(&perf_time_end, &perf_tz);
	  discover_time += (perf_time_end.tv_usec - perf_time_start.tv_usec)+ (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec));
	  if (!(discover_ctr % 100))
	    syslog(LOG_DEBUG,"The average time to process %lu DISCOVER messages = %lu usec.",discover_ctr, discover_time/discover_ctr);
	}
	else if(perf_flag == DHCPREQUEST){
	  request_ctr ++;
	  gettimeofday(&perf_time_end, &perf_tz);
	  request_time += (perf_time_end.tv_usec - perf_time_start.tv_usec)+
	    (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec)); 
	  if (!(request_ctr % 100))
	    syslog(LOG_DEBUG,"The average time to process %lu REQUESTS = %lu usec",request_ctr, request_time/request_ctr);  
	}
#endif
	
	if ( (sendto(rsockout, bufp, buflen, 0, &braw_addr,
		     sizeof (SCIN))) != buflen ) {
	  syslog(LOG_ERR, "Cannot send broadcast packet (%m)");
	}
      }
      free(bufp);
      return;
    }
  }
  
  to.sin_addr = dst;
#ifdef PERFORMANCE
  if (perf_flag == DHCPDISCOVER){
    discover_ctr ++;
    gettimeofday(&perf_time_end, &perf_tz);
    discover_time += (perf_time_end.tv_usec - perf_time_start.tv_usec) + 
     (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec));
    if (!(discover_ctr % 100))
      syslog(LOG_DEBUG,"The average time to process %lu DISCOVER messages = %lu usec.",discover_ctr, discover_time/discover_ctr);  
  }
  else if(perf_flag == DHCPREQUEST){
    request_ctr ++;
    gettimeofday(&perf_time_end, &perf_tz);
    request_time += (perf_time_end.tv_usec - perf_time_start.tv_usec) + 
      (MIL * (perf_time_end.tv_sec - perf_time_start.tv_sec)); 
    if (!(request_ctr % 100))
      syslog(LOG_DEBUG,"The average time to process %lu REQUESTS = %lu usec",request_ctr, request_time/request_ctr);
  }
#endif
  
  if (sendto(s, (caddr_t)bp, sizeof *bp, 0, &to, sizeof to) < 0)
    syslog(LOG_ERR, "send to %s failed (%m)", iaddr_ntoa(to.sin_addr));
}


/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 */
void
setarp(iaddr_t *ia, u_char *ha, int len)
{
    struct sockaddr_in *si;

    bzero(&arpreq, sizeof(struct arpreq));
    arpreq.arp_pa.sa_family = AF_INET;
    si = (struct sockaddr_in *)&arpreq.arp_pa;
    si->sin_addr = *ia;
    bcopy(ha, arpreq.arp_ha.sa_data, len);
    if (ioctl(s, SIOCSARP, (caddr_t)&arpreq) < 0)
	syslog(LOG_ERR, "set arp ioctl failed (%m)");
}

/* 
 * get an arp entry
 * this is used to check whether the reply to a ping was from the same 
 * mac address as the one for which the address was intended
 */
int
getarp(struct in_addr *ia, u_char *ha, int len)
{
    struct sockaddr_in *si;

    bzero(&arpreq, sizeof(struct arpreq));
    arpreq.arp_pa.sa_family = AF_INET;
    si = (struct sockaddr_in *)&arpreq.arp_pa;
    si->sin_addr = *ia;
    if (ioctl(s, SIOCGARP, (caddr_t)&arpreq) < 0) {
	syslog(LOG_ERR, "get arp ioctl failed (%m)");
	return -1;
    }
    bcopy(arpreq.arp_ha.sa_data, ha, len);
    return 0;
}

/*
 * Read bootptab database file.  Avoid rereading the file if the
 * write date hasnt changed since the last time we read it.
 */
void
readtab(void)
{
    struct stat st;
    register char *cp;
    int v;
    register u_long i;
    char temp[64], tempcpy[64];
    register struct hosts *hp;
    int skiptopercent;
    struct hosts hosts_ent;

    if (fp == 0) {
	if ((fp = log_fopen(bootptab, "r")) == NULL) {
	    syslog(LOG_ERR, "can't open bootptab %s (%m)", bootptab);
	    exit(1);
	}
    }
    fstat(fileno(fp), &st);
    if (st.st_mtime == modtime && st.st_nlink)
	return;	/* hasnt been modified or deleted yet */
    fclose(fp);
    if ((fp = log_fopen(bootptab, "r")) == NULL) {
	syslog(LOG_ERR, "can't reopen bootptab %s (%m)", bootptab);
	exit(1);
    }
    fstat(fileno(fp), &st);
    if (debug)
	syslog(LOG_DEBUG, "(re)reading %s", bootptab);
    modtime = st.st_mtime;
    homedir[0] = defaultboot[0] = 0;
    hp = &hosts_ent;
    nhosts = 0;
    cleanup_btabm();
    cleanup_btabi();
    linenum = 0;
    skiptopercent = 1;

    /*
     * read and parse each line in the file.
     */
    for (;;) {
	if (fgets(line, sizeof line, fp) == NULL)
	    break;	/* done */
	if ( (i = strlen(line)) && (line[i-1] == '\n') )
	    line[i-1] = 0;	/* remove trailing newline */
	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */
	/* fill in fixed leading fields */
	if (homedir[0] == 0) {
	    getfield(homedir, sizeof homedir);
	    continue;
	}
	if (defaultboot[0] == 0) {
	    getfield(defaultboot, sizeof defaultboot);
	    continue;
	}
	if (skiptopercent) {	/* allow for future leading fields */
	    if (line[0] != '%')
		continue;
	    skiptopercent = 0;
	    continue;
	}
	/* fill in host table */
	getfield(hp->host, sizeof hp->host);
	getfield(temp, sizeof temp);
	sscanf(temp, "%d", &v);
	hp->htype = v;
	getfield(temp, sizeof temp);
	strcpy(tempcpy, temp);
	cp = tempcpy;
	/* parse hardware address */
	for (i = 0 ; i < sizeof hp->haddr ; i++) {
	    char *cpold;
	    char c;
	    cpold = cp;
	    while (*cp != '.' && *cp != ':' && *cp != 0)
		cp++;
	    c = *cp;	/* save original terminator */
	    *cp = 0;
	    cp++;
	    if (sscanf(cpold, "%x", &v) != 1)
		goto badhex;
	    hp->haddr[i] = v;
	    if (c == 0)
		break;
	}
	if (hp->htype == 1 && i != 5) {
badhex:	    syslog(LOG_ERR, "bad hex address: %s at line %d of bootptab",
		   temp, linenum);
	    continue;
	}
	getfield(temp, sizeof temp);
	i = inet_addr(temp);
	if (i == -1) {
	    register struct hostent *hep;
	    hep = gethostbyname(temp);
	    if (hep != 0 && hep->h_addrtype == AF_INET)
		i = *(int *)(hep->h_addr_list[0]);
	}
	if (i == -1 || i == 0) {
	    syslog(LOG_ERR, "bad internet address: %s at line %d of bootptab",
		   temp, linenum);
	    continue;
	}
	hp->iaddr.s_addr = i;
	getfield(hp->bootfile, sizeof hp->bootfile);
	insert_btab(hp);
    }
}


/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 */
void
getfield(char *str, int len)
{
    register char *cp = str;

    for ( ; *linep && (*linep == ' ' || *linep == '\t') ; linep++)
	;	/* skip spaces/tabs */
    if (*linep == 0) {
	*cp = 0;
	return;
    }
    len--;	/* save a spot for a null */
    for ( ; *linep && *linep != ' ' && *linep != '\t' ; linep++) {
	*cp++ = *linep;
	if (--len <= 0) {
	    *cp = 0;
	    syslog(LOG_ERR, "string truncated: %s, on line %d of bootptab",
		   str, linenum);
	    return;
	}
    }
    *cp = 0;
}

/*
 * Build a list of all the names and aliases by which
 * the current host is known on all of its interfaces.
 */
void
makenamelist(void)
{
    register struct hostent *hp;
    char name[64];

    numaddrs = 0;
    myaddrs = (u_int *)malloc(max_addr_alloc*sizeof(u_int));

    /*
     * Get name of host as told to the kernel and look that
     * up in the hostname database.
     */
    gethostname(name, sizeof(name));
    if ((hp = gethostbyname(name)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", name, hstrerror(h_errno));
	exit(1);
    }
    strcpy(myname,name);

    /*
    ** Find domain name.
    */
    mydomain = strchr(myname, '.');
    if (mydomain) {
	mydomain++;
    }

    /*
     * Remember primary Internet address
     */
    myhostaddr.s_addr = *(u_long *)(hp->h_addr_list[0]);
}


static char *
iaddr_ntoa(iaddr_t addr)
{
    struct hostent *hp;

    if (hp = gethostbyaddr(&addr, sizeof(addr), AF_INET)) {
	return hp->h_name;
    } else {
	return inet_ntoa(addr);
    }
}

static int
check_selected_name_address(char *rhsname, char **dhcp_msg,
			    u_long dhcp_ipaddr,
			    struct bootp *rq, struct dhcp_request_list *req, char *cid_ptr, int cid_length)
{
    /* 1=invalid/inuse 2=alt_naming_flg error */

    u_long	mltaddr;
    u_long	netaddr;
    char	*client_domain;
    char	hostname1[MAXHOSTNAMELEN];

    if (req->dh_configPtr->p_dnsdomain) {
	client_domain = req->dh_configPtr->p_dnsdomain;
    }
    else if (req->dh_configPtr->p_nisdomain){
	client_domain = req->dh_configPtr->p_nisdomain;
    }
    else {
	client_domain = mydomain;
    }
    if (strchr(rhsname, '.') == NULL) 
	sprintf(hostname1,"%s.%s", rhsname, client_domain);
    else
	sprintf(hostname1,"%s", rhsname);
    mltaddr =
	MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
    netaddr =
	rq->bp_giaddr.s_addr ? rq->bp_giaddr.s_addr : mltaddr;
    
    if(sys0_hostnameIsValid(rhsname) == 0) {
	*dhcp_msg =
	    sys0_name_errmsg(rhsname, NAME_INVALID);
	return 1;
    }
    else if(loc0_is_hostname_assigned(hostname1, db)){
      char *enm;
      /* change - if name is assigned to
       * same ether then it is okay
       */
      enm = loc0_get_hostname_from_cid(cid_ptr, cid_length, (EtherAddr*)rq->bp_chaddr, db);
      enm = strtok(enm, ".");	/* hostname part */
      if (enm && *enm && strcmp(rhsname, enm)) {
	*dhcp_msg =sys0_name_errmsg(rhsname, NAME_INUSE);
	return 1;
      }
    }
    if(dhcp_ipaddr) {
	dhcpConfig	*newConfigPtr;
	/* client is requesting a specific address */
	newConfigPtr = cf0_get_config(dhcp_ipaddr, 1);
	if (!newConfigPtr) {
	    *dhcp_msg =
		sys0_addr_errmsg(dhcp_ipaddr, ADDR_INVALID);
	    return 3;
	}
	if(loc0_is_ipaddress_for_cid((EtherAddr *)rq->bp_chaddr,
				     dhcp_ipaddr, cid_ptr, cid_length, db) == 0){
	    /* 0 = same binding, 1 = ether not found, 2 = Found but diff ip*/
	  if (cf0_is_req_addr_valid(dhcp_ipaddr, netaddr) != 0) {
	    *dhcp_msg = sys0_addr_errmsg(dhcp_ipaddr, ADDR_INVALID);
	    return 3;
	  }
	  /* ok to give address */
	}
	else {
	    /* means ethernet not found or has diff binding */
	    if (loc0_is_ipaddress_assigned(dhcp_ipaddr, db)) {
		*dhcp_msg =
		    sys0_addr_errmsg(dhcp_ipaddr, ADDR_INUSE);
		return 3;
	    }
	    else {		/* not assigned */
		if (cf0_is_req_addr_valid(dhcp_ipaddr, netaddr) != 0) {
		    *dhcp_msg =
			sys0_addr_errmsg(dhcp_ipaddr, ADDR_INVALID);
		    return 3;
		}
	    }
	}
    }
    if (alt_naming_flag) {
	struct hostent *he;
	int i;
	
	free(req->dh_hsname);
	req->dh_hsname = strdup(rhsname);

	he = gethostbyname(hostname1);
	if (he) {
	    for (i = 0; he->h_addr_list[i]; i++) {
		if (rq->bp_giaddr.s_addr) {
		    if (cf0_is_req_addr_valid
			(*(u_long *)he->h_addr_list[i],
			 rq->bp_giaddr.s_addr)) {
			req->dh_ipaddr =
			    *(u_long *)(he->h_addr_list[i]);
			return 2;
		    }
		} else {
		    if (cf0_is_req_addr_valid
			(*(u_long *)he->h_addr_list[i],mltaddr)) {
			req->dh_ipaddr = *(u_long *)(he->h_addr_list[i]);
			return 2;
			
		    }
		}
	    }
	}
    }
    return 0;
}

#ifdef EDHCP
/*
 * when a reply is received on the ping socket we process it to check if this
 * is in reply to an ICMP echo send by us
 */
int 
handle_ping_nonblocking_reply(int sd)
{
    int res_seq;
    int res_ident;
    u_long res_node;
    u_char respack[2048];
    struct icmp *icmp_pack;
    struct dhcp_request_list * dhcp_rq;
    iaddr_t ipn;
    struct timeval tmout;
    long next_tm;
    int ret = 0;
    char tmp_cid[MAXCIDLEN];
    struct ether_addr haa;

    while (icmp_pack = recv_icmp_msg( sd, &res_node, respack, 
				      sizeof(respack))) {
	if (icmp_pack == (struct icmp *)NULL) {
	    return(-1);
	}
	
	if (ping_number_pending <= 0) 
	    continue;

	dhcp_rq = dh0_find_dhcp_rqlist_entry_by_ipaddr(res_node);
	if (dhcp_rq == NULL) {
	    /* we may have already mistakenly given out this address 
	       because the echo reply arrived late */
	    if (getRecByIpAddr(0, &res_node, db) != NULL) {
		ipn.s_addr = res_node;
		syslog(LOG_WARNING, "IPaddress %s served may be STOLEN",
		       inet_ntoa(ipn));
	    }
	}
	else {
	    ret = recv_echo_reply(&res_seq, &res_ident, icmp_pack );
	    if (ret == 0) {
		if ( (res_ident != dhcp_rq->ping_ident) || 
		     (res_seq != dhcp_rq->ping_seqnum) ||
		     (res_node != dhcp_rq->dh_ipaddr) ) {
		    /* this reply does not match - do nothing */
		    syslog(LOG_DEBUG, "ping reply did not match with rqlist but address did match");
		}
		else {
		    /* every thing matched */
		    /* check if the mac address of the replying client is 
		     * the same as the one from which we got the DISCOVER
		     * This is possible only if we didn't get the DISCOVER 
		     * via a relay agent (because we use an arp call) */
		    ipn.s_addr = res_node;
		    if (!dhcp_rq->ping_rq.bp_giaddr.s_addr) {
			if (getarp(&ipn, &haa.ea_addr[0], 6) == 0) {
			    if (bcmp(dhcp_rq->dh_eaddr, &haa.ea_addr[0], 
				     sizeof(haa)) == 0) {
				/* its the same mac so its not stolen */
				return ret;
			    }
			    syslog(LOG_WARNING, "Address %s appears to be STOLEN by %s", 
				   inet_ntoa(ipn), ether_ntoa(&haa));
			}
		    }				
		    else
			syslog(LOG_WARNING, "Address %s appears to be STOLEN", 
			       inet_ntoa(ipn));
		    dh0_remove_from_timelist(dhcp_rq->cid_ptr, dhcp_rq->cid_length);
		    /* don't remove from the reqlist - it will get updated  */
		    /* try to get another address */
		    loc0_remove_entry(dhcp_rq->dh_eaddr, 3, 
				      dhcp_rq->cid_ptr, dhcp_rq->cid_length, db);
		    /* mark the stolen address as bad  */
		    sprintf(tmp_cid, "STOLEN-%d", ++msg_recv_time);
		    loc0_create_update_entry(tmp_cid, strlen(tmp_cid), 
					     &haa, res_node,
					     tmp_cid, 0, STOLEN_LEASE, db);
		    request(&dhcp_rq->ping_rq);
		    time(&TimeNow);
		    next_tm = dh0_update_and_get_next_timeout(TimeNow);
		    dh0_set_timeval(&tmout, next_tm);
		}
	    }
	}
    }
    return ret;
}

/*
 * after timeout occurs for an address which was pinged
 * this function sends an OFFER to that host
 */
int
ping_send_DHCPOFFER(struct dhcp_request_list* dhcp_rq)
{
    char str[MAXCIDLEN];
    long offered_lease;

    /* compute the lease  to offer */
    offered_lease = find_lease_to_offer(dhcp_rq->dh_ipaddr,
					dhcp_rq->dh_reqLease,
					dhcp_rq->dh_configPtr,
					db);
		
    if(dhcp_rq->dh_configPtr->p_choose_name)
	dh0_encode_dhcp_server_packet(dhcp_rq->ping_rp.dh_opts,
				      DHCPOFFER,
				      &dhcp_rq->dh_reqParams, 
				      offered_lease, dhcp_rq->ping_dh_msg,
				      dhcp_rq->dh_hsname, dhcp_rq->dh_hsname, 
				      dhcp_rq->dh_configPtr,
				      &dhcp_rq->dci_data);
    else
	dh0_encode_dhcp_server_packet(dhcp_rq->ping_rp.dh_opts,
				      DHCPOFFER,
				      &dhcp_rq->dh_reqParams, 
				      offered_lease, dhcp_rq->ping_dh_msg,
				      dhcp_rq->dh_hsname, 0, 
				      dhcp_rq->dh_configPtr,
				      &dhcp_rq->dci_data);
    
    dhcp_rq->ping_rp.bp_yiaddr.s_addr = dhcp_rq->dh_ipaddr;
    dhcp_rq->ping_status = PING_RECV;
    
    if (debug) {
	mk_str(1, dhcp_rq->cid_ptr, dhcp_rq->cid_length, str);
	syslog(LOG_DEBUG,"DHCPOFFER:cid (%s),ip (%s), name (%s)",
	       str,inet_ntoa(dhcp_rq->ping_rp.bp_yiaddr), dhcp_rq->dh_hsname);
    }
    if(dhcp_rq->ping_rp.bp_flags & BROADCAST_BIT)
	sendreply(&dhcp_rq->ping_rp, 0, dhcp_rq->ping_has_sname, 1, DHCP_REPLY);
    else
	sendreply(&dhcp_rq->ping_rp, 0, dhcp_rq->ping_has_sname, 0, DHCP_REPLY);
    
    time(&TimeNow);
    dh0_addto_timelist(dhcp_rq->cid_ptr, dhcp_rq->cid_length, 
		       dhcp_rq->dh_eaddr,
		       TimeNow+DHCPOFFER_TIMEOUT, 0);
    return 0;
}

/*
 * processing when a ping has been sent and the timeout has expired
 * for each entry on the time list for which the time has expired
 * an OFFER is sent
 */
    
int process_expired_ping_send(void)
{
    struct dhcp_request_list* dhcp_rq;
    time_t timenow;
    struct dhcp_timelist *tq;
    int done;

    done = 0;
    while (!done) {
	if (dh0_timeouts_cnt <= 1) 
	    return 0;
	tq = dhcp_tmlist_first;
	if(tq == 0)
	    return 0;
	while(tq) {
	    time(&timenow);
	    if ((tq->tm_value - timenow) < 0) {
		if (tq->tm_dhcp_rq) 
		    dhcp_rq = tq->tm_dhcp_rq;
		else
		    dhcp_rq = dh0_find_dhcp_rqlist_entry(tq->cid_ptr,
							 tq->cid_length);
		if (dhcp_rq && dhcp_rq->ping_status == PING_SENT) {
				/* this has expired */
		    dh0_remove_from_timelist(dhcp_rq->cid_ptr, 
					     dhcp_rq->cid_length);
		    ping_send_DHCPOFFER(dhcp_rq);
		    done = 1;	/* don't want to finish actually */
		    break;
		}
	    }
	    tq = tq->tm_next;
	}
	done = (done == 1)? 0 : 1;		/* finish if done is 0 */
    }
    return 0;
}
#endif
