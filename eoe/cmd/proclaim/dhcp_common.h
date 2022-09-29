/* This file contains the common definitions used by the DHCP server and client
 * implementations. The file resides in the client directory and is linked into
 * the server tree.
 */

struct ether_addr {
    u_char ea_addr[6];
};
extern char *ether_ntoa(struct ether_addr *);

typedef struct ether_addr EtherAddr;

#define BOOTP1533	100
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

/* Flags */
#define GET_IPLEASE_TIME	0x1
#define GET_SUBNETMASK		0x2
#define GET_HOSTNAME		0x4
#define GET_NISDOMAIN		0x8
#define GET_DNSDOMAIN		0x10
#define GET_DHCP_SERVER		0x20
#define GET_IPADDRESS		0x40
#define GET_SDIST_SERVER	0x80
#define RESOLVE_HOSTNAME	0x100
#define	GET_ROUTER		0x200
#define	GET_TIMESERVERS		0x400
#define	GET_DNSSERVERS		0x800
#define GET_NIS_SERVERS		0x1000
#define	GET_IF_MTU		0x2000
#define	GET_ALL_SUBNETS_LOCAL	0x4000
#define	GET_BROADCAST_ADDRESS	0x8000
#define	GET_CLIENT_MASK_DISC	0x10000
#define GET_MASK_SUPPLIER	0x20000
#define GET_STATIC_ROUTE	0x40000
#define	HAS_OPTION_OVERLOAD	0x80000
#define	HAS_MAX_DHCP_MSG_SIZE	0x100000
#define GET_RENEW_LEASE_TIME	0x200000
#define GET_REBIND_LEASE_TIME	0x400000
#define GET_CLIENT_FQDN		0x800000

#define ONE_YEAR_LEASE		0x1e13380
#define THREE_YEAR_LEASE	0x5a39a80

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
#define POLL_SERVER		4

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
#define PROCLAIM_UNSUPPORTED_IF	6

#define INTERFACE_DOWN		0
#define INTERFACE_UP		1

