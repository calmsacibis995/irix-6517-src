/*
 * ffscnet.h
 *	Declarations for FFSC Networking Functions
 */

#ifndef _FFSCNET_H_
#define _FFSCNET_H_

#include <types/vxtypes.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>


/* Constants */
#define FFSC_NET_DEVICE		"ene"		/* Ethernet device name */
#define FFSC_NET_INTERFACE	"ene0"		/* Interface name */


/* Basic types */
typedef unsigned char macaddr_t[6];	/* An ethernet MAC address */

typedef struct netinfo {
	int32_t	 HostAddr;		/* Host portion of IP address */
	int32_t	 NetAddr;		/* Network portion of IP address */
	int32_t	 NetMask;		/* Netmask for IP address */
	uint32_t reserved[5];		/* reserved for future expansion */
} netinfo_t;		/* Network information */


/*
 * Initial IP address info
 *
 * All FFSC IP address will be on the 100.x.x.x class-A network (since the
 * FFSC ethernet is private, this is just an arbitrary assignment, any
 * class-A network would do). IP addresses are normally formed by appending
 * the last 3 octets of the ethernet MAC to the network address, with the
 * exception that the addresses 100.0.0.1-100.0.0.15 are reserved for debug
 * purposes. Conflicting IP addresses are resolved by the address daemon
 * by searching for the first unused IP address in the allowed range.
 */
extern netinfo_t ffscNetInfo;		/* FFSC network information */

#define FFSC_NETWORK_ADDR	100		/* Init FFSC network address */

#define FFSC_NETMASK		0xff000000	/* Init FFSC netmask */

#define FFSC_DEVPC_HOSTADDR	0x00000001	/* Reserved for dev. PC */
#define FFSC_NO_HOSTADDR	0x0000000f	/* No IP address available! */
#define FFSC_MIN_HOSTADDR	0x00000010	/* Min normal host addr */
#define FFSC_MAX_HOSTADDR	0x00fffffe	/* Max normal host addr */


/* Other global variables */
extern struct ifnet	*ffscNetIF;	/* ifnet for this FFSC's NIC */
extern macaddr_t	ffscMACAddr;	/* MAC address of this FFSC's NIC */

#endif  /* _FFSCNET_H_ */
