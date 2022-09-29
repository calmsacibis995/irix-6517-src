#ident	"include/net/ei.h:  $Revision: 1.3 $"

/*
 * ethernet udp connection info
 * NOTE: all address info is in network byte order!
 */
struct ether_info {
	char ei_filename[256];
	u_short ei_udpport;		/* local udp port */
	short ei_registry;		/* port registry table */
	struct sockaddr_in ei_srcaddr;	/* source address of last packet */
	struct sockaddr_in ei_gateaddr;	/* gateway address for transmits */
	struct sockaddr_in ei_dstaddr;	/* destination address for transmits */
	struct arpcom *ei_acp;		/* ptr to ARP common header */
	short ei_scan_entered;		/* to prevent reentering scan rtns */
};

#define MAX_IFCS	4	/* max nbr of Ethernet boards supported */
