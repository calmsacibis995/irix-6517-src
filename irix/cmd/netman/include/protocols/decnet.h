#ifndef DECNET_H
#define DECNET_H
/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * This file contains the definitions of all the Routing Procotol constants,
 * structures, and functions. The message structures sent by the routing
 * layer are also defined.
 */
#include <sys/types.h>
#include "protostack.h"
#include "protocols/ether.h"

#ifdef sun
#define ETHERTYPE_DECMOP        0x6001  /* DEC dump/load (MOP) */
#define ETHERTYPE_DECCON        0x6002  /* DEC remote console */
#define ETHERTYPE_DECnet        0x6003  /* DECnet 'routing' */
#define ETHERTYPE_DECLAT        0x6004  /* DEC LAT */
#endif

/*
 * Multicast IDs for "All Routers" and "All Endnodes"
 * All-Routers  = AB-00-00-03-00-00
 * All-Endnodes = AB-00-00-04-00-00
 */
#define HIORD			0xAA000400
#define	ALL_ROUTERS_PREFIX	0xAB000003
#define ALL_ENDNODES_PREFIX	0xAB000004

/*
 * protocols on DECnet Routing Protocol
 */
#define	RPPROTO_NSP	0

/*
 * addresses
 */
#define	DECNETADDRLEN	2
typedef struct etheraddr	Ether_addr;
typedef struct Decnet_addr {
	unsigned char	da_vec[DECNETADDRLEN];
} Decnet_addr;
typedef	unsigned short	dn_addr; 

#define	MAXPADSIZE	7


/*
 * masks used by routing to access the packet type on input
 */
#define	ROUT_PAD_TYPE	0x80
#define	ROUT_PADDING	0x7F	/* byte: 1??? ???? */

#define ROUT_PKT_FLAG   0x01	/* 0 = data packet; 1 = control packet */

#define DATA_PKT_TYPE   0x07	/* byte: XXXX X??? */
#define ROUT_PKT_TYPE   0x0E	/* byte: XXXX ???1 */

#define LEVEL_1_ROUTER  0x06
#define LEVEL_2_ROUTER  0x08
#define ROUTER_HELLO    0x0A
#define ENDNODE_HELLO   0x0C

#define LONG_FORMAT     0x06
#define SHORT_FORMAT    0x02

/*
 * Data flags
 */
#define FLAG_RQR	0x08	/* Return to Sender Request */
#define FLAG_RTS	0x10	/* Return to Sender */
#define	FLAG_IE		0x20	/* Intra-Ethernet packet */
#define	FLAG_V		0x40	/* Version: reserved, discard if 1 */

/*
 * Hello flags
 */
#define	FLAG_RES	0x70	/* Reserved */
#define	FLAG_TYPE	0x0E	/* Type */

/*
 * packet type values
 */
#define	PKT_LEVEL_1_ROUTER	(ROUT_PKT_FLAG | LEVEL_1_ROUTER)
#define	PKT_LEVEL_2_ROUTER	(ROUT_PKT_FLAG | LEVEL_2_ROUTER)
#define	PKT_ROUTER_HELLO	(ROUT_PKT_FLAG | ROUTER_HELLO)
#define	PKT_ENDNODE_HELLO	(ROUT_PKT_FLAG | ENDNODE_HELLO)
#define PKT_LONG_FORMAT		LONG_FORMAT
#define PKT_SHORT_FORMAT	SHORT_FORMAT

/*
 * Macros for handling byte ordering issues. 
 */
#define SWAP16(a)	( (((a) >> 8) & 0x00FF) | (((a) << 8) & 0xFF00) )
#define decnet_htons(x) SWAP16(x)
#define decnet_ntohs(x) SWAP16(x)
#define NODE_NUM(id)    ( (((id)<<8) & 0x300) + (((id)>>8) & 0xff) )
#define AREA_NUM(id)    ( ((id)>>2) & 0x3f )

/*
 * Swap a sequence of two bytes.
 */
#define MakeShort(ptr)  (*(ptr) | (*((ptr)+1)) << 8)

/*
 * RP protocol stack frame to be used in snoop filter matching.
 */
struct rpframe {
	ProtoStackFrame		rpf_frame;	/* base class state */
	u_char			rpf_type;	/* rp message type */
	Ether_addr		rpf_src;	/* source ether address*/
	Ether_addr		rpf_dst;	/* destination ether address */
	dn_addr			rpf_srcnode;	/* source address */
	dn_addr			rpf_dstnode;	/* destination address */
	struct protocol		*rpf_proto;	/* nested protocol interface */
};

/*
 * --------------------------------------------------------------------------
 * 	D A T A   S T R U C T U R E S   O N   T H E   W I R E
 * --------------------------------------------------------------------------
 */

/*
 * Routing route header -- This is used for End Communications Layer
 * segments which may required forwarding. There are two possible formats
 * for data packet route headers:
 *	1.	short format (identical to Phase III format)
 *	2.	long format (Ethernet Endnode data packet)
 */

/* 
 * Short Data Packet Format
 */
typedef struct short_msg {
	char		flags;		/* routing message flags */
        u_char		dstnode[2];	/* destination address */
        u_char		srcnode[2];	/* source address */
        char		forward;	/* forwarding information */
        } RP_short_msg;


/* 
 * Long Data Packet Format
 */
typedef struct long_msg {
	char		flags;		/* routing message flags */
        char		d_area;		/* reserved */
        char		d_subarea;	/* reserved */
        Ether_addr	d_id;		/* destination ID */
        char		s_area;		/* reserved */
        char		s_subarea;	/* reserved */
        Ether_addr	s_id;		/* source ID */
        char		nl2;		/* next level 2 router */
        char		visit_ct;	/* visit count for this packet */
        char		s_class;	/* service class, reserved */
        char		pt;		/* protocol type, reserved */
        } RP_long_msg;


/*
 * Routing Layer control -- These control Routing Layer routing and
 * initialization functions. On non-broadcast circuits the types of
 * Routing Layer control messages are:
 *	1.	Initialization Message
 *	2.	Verification Message
 *	3.	Hello And Test Message
 *	4.	Level 1 Routing Message
 *	5.	Level 2 Routing Message
 * 
 * On broadcast circuits the types of Routing Layer control messages are:
 *	1.	Ethernet Router Hello Message
 *	2.	Ethernet Endnode Hello Message
 *	3.	Level 1 Routing Message
 *	4.	Level 2 Routing Message
 */


/*
 * Ether_iinfo defines the type of router sending the 
 * msg and the whether multicast traffic is handled.
 * The other flags are not used on ethernet.
 */

typedef struct ether_iinfo {
        unsigned char resrvd : 1;       /* reserved */
        unsigned char blk_reqstd : 1;   /* blocking requested 0 on ethernet */
        unsigned char no_multicast : 1; /* 0 => multicast traffic accepted */
        unsigned char verif_failed : 1; /* reserved */
        unsigned char reject_flg : 1;   /* reserved */
        unsigned char verif : 1;        /* 0 for ethernet */
        unsigned char type_ind : 2;	/* 3 => endnode */
					/* 2 => level 1 router */
                                      	/* 1 => level 2 router */
        } Ether_iinfo;


/*
 * Ethernet Router Hello Message 
 */
typedef struct {
        char		flags;
        char		ver_number;	/* routing layer version */
        char		eco_number;	/* routing layer eco number */
        char		user_eco_number;/* routing layer user eco number */
        Ether_addr	trans_id;	/* system id of the transmitter */
        char		enet_iinfo;	/* router information */
        u_char		blksize[2];	/* block size */
	/* router specific */
        char		priority;	/* router priority */
        char		area;		/* reserved  */
        u_char		timer[2];	/* hello timer in seconds */
        char		mpd;		/* reserved  */
        char		elist_count;
        char		elist_name[7];
        char		rslist_count;
        /* the list of known ethernet routers will follow */
        char		rslist_start;
        } RP_router_hello;


/*
 * Ethernet Endnode Hello message.
 */
typedef struct seed {unsigned char b[8];} Seed;
typedef struct {
        char		flags;
        char		ver_number;	/* routing layer version */
        char		eco_number;	/* routing layer eco number */
        char		user_eco_number;/* routing layer user eco num */
        Ether_addr	trans_id;	/* transmitter id */
        char		enet_iinfo;	/* router info */
        u_char		blksize[2]; 	/* block size */
	/* endnode specific */
        char		area;		/* reserved */
        Seed		ver_seed;	/* verification seed (0) */
        Ether_addr	neigh_id;	/* neighbor's system id */
        u_char		timer[2];
        char		mpd;		/* reserved */
        } RP_endnode_hello;

/*
 * --------------------------------------------------------------------------
 * 	O P E R A T I O N S   /   P R O C E D U R E S
 * --------------------------------------------------------------------------
 */

/*
 * Call rp_hostname to translate a DECnet (Routing Protocol) address in host
 * or net byte order into a hostname or dot-notation string representation.
 * Call rp_hostaddr to go from a hostname or dot-string to DECnet address.
 * Call rp_addrfromether to extract the DECnet address from the ethernet
 * address.
 */
enum rporder { RP_NET, RP_HOST };

char    *rp_hostname(dn_addr, enum rporder);
int     rp_hostaddr(char *, enum rporder, dn_addr *);
int	rp_addrfromether(Ether_addr *, enum rporder, dn_addr *);
int	rp_etherfromaddr(Ether_addr *, enum rporder, dn_addr *);

/*
 * Translate an ethernet address to a string representation indicating: 
 *	1) "All DECnet routers" OR "All DECnet endnodes"
 *	2) the corresponding local DECnet node name
 *	3) the DECnet address in dot-notation
 */
char	*rp_hostnamefromether(Ether_addr *);

/*
 * This function adds a new host/address association to the cache of RP
 * hostnames and addresses.  It returns a pointer to the named symbol in
 * the RP hostname symbol table.
 */
struct symbol   *rp_addhost(char *, dn_addr);

/*
 * Translate a dot-notation string into an unsigned short RP address in
 * host order.  Return updated string pointer or 0 on error.
 */
char    *rp_addr(char *, u_short *);

/*
 * Index operations for hashing and comparing RP addresses.
 */
u_int   rp_hashaddr(void *);
int     rp_cmpaddrs(void *, void *);

/*
 * DECnet RP family protocol names ("rp", "nsp").
 */
extern char     rpname[];
extern char     nspname[];

#define	RP_MINHDRLEN	sizeof(RP_short_msg)
#define RP_MAXHDRLEN	sizeof(RP_endnode_hello)

#endif /* DECNET_H */
