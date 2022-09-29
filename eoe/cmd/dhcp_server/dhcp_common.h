/* 
 * This file contains the common definitions used by the DHCP server and client
 * implementations. The file resides in the client directory and is linked into
 * the server tree.
 */

#ifndef _dhcp_common_h
#define _dhcp_common_h

#include <sys/file.h>
#include <ndbm.h>

#define INFINITE_LEASE	0xffffffff
#define STATIC_LEASE	-3
#define STOLEN_LEASE	-2

struct ether_addr {
    u_char ea_addr[6];
};
extern char *ether_ntoa(struct ether_addr *);

typedef struct ether_addr EtherAddr;

#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8
#define DHCPREVALIDATE	9
#define DHCPPOLL	10
#define DHCPPRPL	11

/* Vendor Extensions */
#define PAD_TAG			0
#define SUBNETMASK_TAG		1
#define TIME_OFFSET_TAG		2
#define	ROUTER_TAG		3
#define	TIMESERVERS_TAG		4
#define NAME_SERVER116_TAG	5
#define	DNSSERVERS_TAG		6
#define LOGSERVERS_TAG		7
#define COOKIESERVERS_TAG	8
#define LPRSERVERS_TAG		9
#define IMPRESSSERVERS_TAG	10
#define RESOURCESERVERS_TAG	11
#define HOSTNAME_TAG		12
#define BOOTFILE_SIZE_TAG	13
#define MERITDUMP_FILE_TAG	14
#define DNSDOMAIN_NAME_TAG	15
#define SWAPSERVER_ADDRESS_TAG	16
#define ROOT_PATH_TAG		17
#define EXTENSIONS_PATH_TAG	18

/* IP Layer params per host */

#define	IP_FORWARDING_TAG	19
#define	NON_LOCAL_SRC_ROUTE_TAG	20
#define	POLICY_FILTER_TAG	21
#define	MAX_DGRAM_REASSEMBL_TAG	22
#define	DEF_IP_TIME_LIVE_TAG	23
#define	PATH_MTU_AGE_TMOUT_TAG	24
#define	PATH_MTU_PLT_TABLE_TAG	25

/* IP Layer parameters per interface */
#define	IF_MTU_TAG		26
#define	ALL_SUBNETS_LOCAL_TAG	27
#define	BROADCAST_ADDRESS_TAG	28
#define	CLIENT_MASK_DISC_TAG	29
#define MASK_SUPPLIER_TAG	30
#define ROUTER_DISC_TAG		31
#define ROUTER_SOLICIT_ADDR_TAG 32
#define STATIC_ROUTE_TAG	33

/* Link layer params per interface */
#define TRAILER_ENCAPS_TAG	34
#define ARP_CACHE_TIMEOUT_TAG	35
#define ETHERNET_ENCAPS_TAG	36


/* TCP Parameters */
#define TCP_DEFAULT_TTL_TAG	37
#define TCP_KEEPALIVE_INTVL_TAG	38
#define TCP_KEEPALIVE_GARBG_TAG	39


/* Applicatopn and service parameters */
#define NISDOMAIN_NAME_TAG	40
#define NIS_SERVERS_TAG		41
#define NTP_SERVERS_TAG		42

/* Vendor specific extensions */
#define SGI_VENDOR_TAG		43

/* NetBIOS */
#define NETBIOS_NMSERV_ADDR_TAG 44
#define NETBIOS_DISTR_ADDR_TAG  45
#define NETBIOS_NODETYPE_TAG    46
#define NETBIOS_SCOPE_TAG       47

/* X window system */
#define X_FONTSERVER_ADDR_TAG   48
#define X_DISPLAYMGR_ADDR_TAG   49

/* DHCP extensions */

#define IPADDR_TAG		50
#define IP_LEASE_TIME_TAG	51
#define	OPTION_OVERLOAD_TAG	52
#define DHCP_MSG_TAG		53
#define DHCP_SERVER_TAG		54
#define DHCP_PARAM_REQ_TAG	55
#define	DHCP_SERVER_MSG		56
#define	MAX_DHCP_MSG_SIZE_TAG	57
#define RENEW_LEASE_TIME_TAG	58
#define REBIND_LEASE_TIME_TAG	59
#define DHCP_CLASS_TAG		60
#define DHCP_CLIENT_ID_TAG	61

/* NIS+ */
#define NISPLUS_DOMAIN_TAG	64
#define NISPLUS_SERVER_ADDR_TAG 65

#define TFTP_SERVER_NAME_TAG    66
#define BOOTFILE_NAME_TAG       67

#define MBLEIP_HMAGENT_ADDR_TAG 68
#define SMTP_SERVER_ADDR_TAG	69
#define POP3_SERVER_ADDR_TAG	70
#define NNTP_SERVER_ADDR_TAG	71
#define WWW_SERVER_ADDR_TAG	72
#define FINGER_SERVER_ADDR_TAG	73
#define IRC_SERVER_ADDR_TAG	74
#define STTALK_SERVER_ADDR_TAG	75
#define STDA_SERVER_ADDR_TAG	76

#define CLIENT_FQDN_TAG		81

/* SGI */
#define SDIST_SERVER_TAG	80
#define RESOLVE_HOSTNAME_TAG	81

#define END_TAG			255

struct getParams {
/* Flags */
    unsigned  GET_IPLEASE_TIME:1;
    unsigned  GET_SUBNETMASK:1;
    unsigned  GET_HOSTNAME:1;
    unsigned  GET_NISDOMAIN:1;
    unsigned  GET_DNSDOMAIN:1;
    unsigned  GET_DHCP_SERVER:1;
    unsigned  GET_IPADDRESS:1;
    unsigned  GET_SDIST_SERVER:1;
    unsigned  RESOLVE_HOSTNAME:1;
    unsigned  GET_ROUTER:1;
    unsigned  GET_TIMESERVERS:1;
    unsigned  GET_DNSSERVERS:1;
    unsigned  GET_NIS_SERVERS:1;
    unsigned  GET_IF_MTU:1;
    unsigned  GET_ALL_SUBNETS_LOCAL:1;
    unsigned  GET_BROADCAST_ADDRESS:1;
    unsigned  GET_CLIENT_MASK_DISC:1;
    unsigned  GET_MASK_SUPPLIER:1;
    unsigned  GET_STATIC_ROUTE:1;
    unsigned  HAS_OPTION_OVERLOAD:1;
    unsigned  HAS_MAX_DHCP_MSG_SIZE:1;
    unsigned  GET_RENEW_LEASE_TIME:1;
    unsigned  GET_REBIND_LEASE_TIME:1;

    unsigned  GET_LOGSERVERS:1;
    unsigned  GET_COOKIESERVERS:1;
    unsigned  GET_LPRSERVERS:1;
    unsigned  GET_RESOURCESERVERS:1;
    unsigned  GET_BOOTFILE_SIZE:1;
    unsigned  GET_SWAPSERVER_ADDRESS:1;
    unsigned  GET_IP_FORWARDING:1;
    unsigned  GET_NON_LOCAL_SRC_ROUTE:1;
    unsigned  GET_POLICY_FILTER:1;

    unsigned  GET_MAX_DGRAM_REASSEMBL:1;
    unsigned  GET_DEF_IP_TIME_LIVE:1;
    unsigned  GET_PATH_MTU_AGE_TMOUT:1;
    unsigned  GET_PATH_MTU_PLT_TABLE:1;
    unsigned  GET_ROUTER_DISC:1;
    unsigned  GET_ROUTER_SOLICIT_ADDR:1;
    unsigned  GET_TRAILER_ENCAPS:1;
    unsigned  GET_ARP_CACHE_TIMEOUT:1;
    unsigned  GET_ETHERNET_ENCAPS:1;
    unsigned  GET_TCP_DEFAULT_TTL:1;
    unsigned  GET_TCP_KEEPALIVE_INTVL:1;
    unsigned  GET_TCP_KEEPALIVE_GARBG:1;
    unsigned  GET_NETBIOS_NMSERV_ADDR:1;
    unsigned  GET_NETBIOS_DISTR_ADDR:1;
    unsigned  GET_NETBIOS_NODETYPE:1;
    unsigned  GET_NETBIOS_SCOPE:1;
    unsigned  GET_X_FONTSERVER_ADDR:1;
    unsigned  GET_X_DISPLAYMGR_ADDR:1;
    unsigned  GET_NISPLUS_DOMAIN:1;
    unsigned  GET_NISPLUS_SERVER_ADDR:1;
    unsigned  GET_MBLEIP_HMAGENT_ADDR:1;
    unsigned  GET_SMTP_SERVER_ADDR:1;
    unsigned  GET_POP3_SERVER_ADDR:1;
    unsigned  GET_NNTP_SERVER_ADDR:1;
    unsigned  GET_WWW_SERVER_ADDR:1;
    unsigned  GET_FINGER_SERVER_ADDR:1;
    unsigned  GET_IRC_SERVER_ADDR:1;
    unsigned  GET_STTALK_SERVER_ADDR:1;
    unsigned  GET_STDA_SERVER_ADDR:1;
    unsigned  GET_TIME_OFFSET:1;
    unsigned  GET_NAMESERVER116_ADDR:1;
    unsigned  GET_IMPRESS_SERVER_ADDR:1;
    unsigned  GET_MERITDUMP_FILE:1;
    unsigned  GET_ROOT_PATH:1;
    unsigned  GET_EXTENSIONS:1;
    unsigned  GET_NTP_SERVER_ADDR:1;
    unsigned  GET_TFTP_SERVER_NAME:1;
    unsigned  GET_BOOTFILE_NAME:1;
    unsigned  GET_CLIENT_FQDN:1;
};


#define ONE_YEAR_LEASE		0x1e13380

#define NAME_INUSE		1
#define NAME_INVALID		2
#define OFFERED_NAME		3
#define SELECTED_NAME		4
#define KEEP_OLD_NAMEONLY	5
#define KEEP_OLD_NAMEADDR	6
#define NEW_NAMEADDR		7
#define NO_DHCP			8
#define ADDR_INUSE		9
#define ADDR_INVALID		10

#define RELEASE_LEASE		1
#define RENEW_LEASE		2
#define REBIND_LEASE		3

#define CHK_ON			0
#define CHK_OFF			1
#define CHK_OTHER		2
#define CHK_ERROR		-1

#define PROCLAIM_NO_ERROR	0
#define PROCLAIM_SOCK_ERR	1
#define PROCLAIM_SOCKOPT_ERR	2
#define PROCLAIM_BIND_ERR	3
#define PROCLAIM_SNOOP_ERR	4
#define PROCLAIM_SENDTO_ERR	5

#define INTERFACE_DOWN		0
#define INTERFACE_UP		1

#define TYPE_UNSUPPORT  0
#define TYPE_ETHER      1
#define TYPE_FDDI       2

/* Locking DHCP database */
#define LOCKDHCPDB(db) \
 	if (flock(db, LOCK_EX) < 0) \
		syslog(LOG_ERR, "Error while locking rec.")
#define UNLOCKDHCPDB(db) \
 	if (flock(db, LOCK_UN) < 0) \
		syslog(LOG_ERR, "Error while locking rec.")

/* function prototypes  */
extern int parse_ether_entry_l(char *, char **, char **, char **, char **);

extern char * getRecByCid(char *sbuf, char *cid_ptr, int cid_length, DBM *db);
extern char *getRecByHostName(char *sbuf, char *hname, DBM *db);
extern char *getRecByIpAddr(char *sbuf, u_long *ipa, DBM *db);
extern void putRecByCid(char *cid_ptr, int cid_length, DBM *db, char *frmted_data);
extern void putRecByEtherAddr(EtherAddr *eth, DBM *db, char *cid_ptr, int cid_length);
extern void putRecByHostName(char *hname, DBM *db, char *cid_ptr, int cid_length);
extern void putRecByIpAddr(u_long ipaddr, DBM *db, char *cid_ptr, int cid_length);
extern void deleteRec(char *cid_ptr, int cid_length, EtherAddr *eth, u_long ipcl, char *hname, DBM *db);
extern char *getCidByIpAddr(char *cid_ptr, int *cid_len, u_long *ipa, DBM *db);


#ifdef EDHCP_LDAP
extern char *getRecByCidLdap(char *, char *, int);
extern char *getRecByHostNameLdap(char *, char *);
extern char *getRecByIpAddrLdap(char *, u_long *);
extern char *getReservationByCidLdap(char *, char *, int);
extern char *getReservationByIpAddrLdap(char *, u_long *);
extern char *getReservationByHostNameLdap(char *, char *);
extern void addLeaseLdap(char *, int, char *);
extern void deleteLeaseLdap(char *, int, u_long);
extern void updateLeaseLdap(char *, int, char*, long);
extern void updRecByCid(char *cid_ptr, int cid_length, char* ipc, long lease);
void invalidateLeaseCache(void);
#endif


#endif /* _dhcp_common_h */
