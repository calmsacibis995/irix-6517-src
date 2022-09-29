/* bootpLib.h - vxWorks BOOTP Client Library header file */

/* Copyright 1990-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01k,22sep92,rrr  added support for c++
01j,04jul92,jcf  cleaned up.
01i,11jun92,elh  modified parameters to bootpParamsGet.
01h,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01g,17apr92,elh  moved prototypes icmpLib.
01f,28feb92,elh  ansified.
01e,27aug91,elh  added RFC 1048 stuff, and added errors.
01d,12aug90,dnw  changed retransmission delay parameters
01c,12aug90,hjb  major revision
01b,19apr90,hjb  added VX_LOG_FILE definition
01a,11mar90,hjb  written
*/

#ifndef __INCbootpLibh
#define __INCbootpLibh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "vwmodnum.h"
#include "netinet/in.h"
#include "net/if.h"
#include "netinet/in_systm.h"
#include "netinet/ip.h"

/* useful defines */
						/* BOOTP reserved ports */
#define IPPORT_BOOTPS           67
#define IPPORT_BOOTPC           68

#define BOOTREQUEST             1		/* BOOTP operations	*/
#define BOOTREPLY               2

#define SIZE_VEND		64
#define SIZE_FILE		128

/* retransmission delay parameters */

#define INIT_BOOTP_DELAY	2	/* initial retransmit delay (secs) */
#define MAX_BOOTP_DELAY		16 	/* maximum retransmit delay (secs) */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1					/* dont optimize alignments */
#endif  /* CPU_FAMILY==I960 */

						/* BOOTP message structure */
typedef struct bootp_msg
    {
    unsigned char	bp_op;			/* packet opcode type	*/
    unsigned char	bp_htype;       	/* hardware addr type	*/
    unsigned char	bp_hlen;		/* hardware addr length */
    unsigned char	bp_hops;		/* gateway hops 	*/
    unsigned long	bp_xid;			/* transaction ID 	*/
    unsigned short	bp_secs;		/* seconds since boot 	*/
    unsigned short	bp_unused;
    struct in_addr	bp_ciaddr;		/* client IP address 	*/
    struct in_addr	bp_yiaddr;		/* 'your' IP address 	*/
    struct in_addr	bp_siaddr;		/* server IP address 	*/
    struct in_addr	bp_giaddr;		/* gateway IP address 	*/
    unsigned char	bp_chaddr [16];		/* client hardware addr	*/
    unsigned char	bp_sname [64];		/* server host name 	*/
    unsigned char	bp_file [SIZE_FILE];	/* boot file name 	*/
    unsigned char	bp_vend [SIZE_VEND];	/* vendor-specific area */
    } BOOTP_MSG;

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0					/* turn off */
#endif  /* CPU_FAMILY==I960 */

#define VM_RFC1048      	{ 99, 130, 83, 99 }


#define TAG_PAD                 	0	/* 1048 vendor tags */
#define TAG_SUBNET_MASK         	1
#define TAG_TIME_OFFSET         	2
#define TAG_GATEWAY             	3
#define TAG_TIME_SERVER         	4
#define TAG_NAME_SERVER         	5
#define TAG_DOMAIN_SERVER       	6
#define TAG_LOG_SERVER          	7
#define TAG_COOKIE_SERVER       	8
#define TAG_LPR_SERVER          	9
#define TAG_IMPRESS_SERVER      	10
#define TAG_RLP_SERVER          	11
#define TAG_HOSTNAME            	12
#define TAG_BOOTSIZE            	13
#define TAG_END                 	255

/* error values */

#define S_bootpLib_INVALID_ARGUMENT	(M_bootpLib | 1)
#define S_bootpLib_INVALID_COOKIE	(M_bootpLib | 2)
#define S_bootpLib_NO_BROADCASTS	(M_bootpLib | 3)
#define S_bootpLib_PARSE_ERROR		(M_bootpLib | 4)
#define S_bootpLib_INVALID_TAG		(M_bootpLib | 5)
#define S_bootpLib_TIME_OUT		(M_bootpLib | 6)

/* function prototypes */

#if defined(__STDC__) || defined(__cplusplus)

STATUS 	bootpParamsGet (char *ifName, int port, char *pInetAddr,
		        char *pHostAddr, char *pBootFile,
    		        int *pSizeFile, int *pSubnet, u_int timeOut);

STATUS 	bootpMsgSend (char *ifName, struct in_addr *pIpDest, int port,
		      BOOTP_MSG *pBootpMsg, u_int timeOut);

u_char * bootpTagFind (u_char *pVend, int tag, int *pSize);

#else	/* __STDC__ */

STATUS		bootpParamsGet ();
STATUS		bootpMsgSend ();
u_char *	bootpTagFind ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCbootpLibh */
