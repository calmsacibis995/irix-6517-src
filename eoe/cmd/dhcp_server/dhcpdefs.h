#include <sys/time.h>
#include <arpa/nameser.h>
#include "dhcp.h"
#include "configdefs.h"
#include "dhcp_common.h"

typedef enum ping_st {
    PING_NONE, 
    PING_SENT, 
    PING_RECV 
} ping_status_t;

/* store all kinds of dhcp client information */
struct dhcp_client_info {
    char	client_fqdn[MAXDNAME+3];
    char	vendor_tag[64];
};

struct dhcp_request_list {
    char        *cid_ptr;
    int         cid_length;
    EtherAddr	*dh_eaddr;
    u_long	dh_ipaddr;
    char	*dh_hsname;
    struct getParams dh_reqParams;
    u_long	dh_reqServer;
    int		dh_reqLease;
    dhcpConfig	*dh_configPtr;
    ping_status_t ping_status;
    struct bootp ping_rp;
    struct bootp ping_rq;
    int ping_ident;
    int ping_seqnum;
    char *ping_dh_msg;
    int ping_has_sname;
    struct dhcp_client_info dci_data;
    struct dhcp_request_list *dh_prev;
    struct dhcp_request_list *dh_next;
};

struct dhcp_timelist {
    char        *cid_ptr;
    int         cid_length;
    EtherAddr   *tm_eaddr;
    time_t	tm_value;
    struct dhcp_request_list* tm_dhcp_rq;
    struct dhcp_timelist *tm_prev;
    struct dhcp_timelist *tm_next;
};

#ifndef iaddr_t
#define	iaddr_t struct in_addr
#endif

#define IFMAX   200

typedef struct sockaddr_in SCIN;

#ifndef SINPTR
#define SINPTR(p)	((struct sockaddr_in *)(p))
#endif

/*
 * List of netmasks and corresponding network numbers for all
 * attached interfaces that are configured for Internet.
 */

struct	netaddr {
	u_long	netmask;	 /* network mask (not shifted) */
	u_long	net;		 /* address masked by netmask */
	iaddr_t	myaddr;		 /* this hosts full ip address on that net */
        int     first_alias;	 /* alias addresses - index into myaddrs */
        int     last_alias;	 /* last alias - index into myaddrs array */
	int	rsockout;	 /* raw socket fd for broadcast */
	char	etheraddr[8];	 /* hardware address */
	char	ifname[IFNAMSIZ];/* Interface name: ec0, et0... */
};

#define MULTIHMDHCP	(ProclaimServer && (ifcount > 1))

#define BROADCAST_BIT		0x8000
#define BROADCAST_ADDRESS	0xffffffff

#define DHCPOFFER_TIMEOUT	240
#define DHCPPING_TIMEOUT	1
#define DHCPPING_MAXOUT		16

#define BOOTSTRAP_REPLY		1
#define DHCP_REPLY		2

#define	BOOTPKTSZ		300	/* minimum size of the bootp pkt */

typedef enum state_change {
    STATE_CREATE, 
    STATE_DELETE, 
    STATE_LEASE } state_change_op;

#define MAXCIDLEN		128 /* this is not max actually */
#define MAXFQDNLEN		128 /* this is not max ? */
#define MAXIPLEN		32
#define MAXSTATECHANGES		8
struct state_change_info {
    char cid[MAXCIDLEN];
    int cid_length;
    EtherAddr mac;
    char ipc[MAXIPLEN];
    char hostname[MAXFQDNLEN];
    u_long lease;
    state_change_op op;
};

    
typedef struct state_change_info state_change_t;
