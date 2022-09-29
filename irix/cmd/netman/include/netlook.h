#ifndef NETLOOK_H
#define NETLOOK_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetLook service include file
 *
 *	$Revision: 1.4 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/time.h>
#include "protocols/ether.h"

#define NLP_VERSION	2
#define NLP_NAMELEN	28
#define NLP_MAXPROTOS	16
#define NLP_MAXSPACKETS	64

/*
 * NetLook packet types
 */
#define NLP_ENDOFDATA	0
#define NLP_SNOOPDATA	1

/*
 * An enpoint of communication
 */
struct nlendpoint {
	struct etheraddr	nlep_eaddr;	/* Link address */
	unsigned short		nlep_port;	/* Port number */
	unsigned long		nlep_naddr;	/* Network address */
	char			nlep_name[NLP_NAMELEN];/* Host name */
};

/*
 * A snooped packet
 */
struct nlspacket {
	unsigned short  	nlsp_type;	/* Ether,LLC type of packet */
	unsigned short  	nlsp_length;	/* Length of packet */
	struct timeval		nlsp_timestamp;	/* Timestamp of packet */
	unsigned int		nlsp_protocols;	/* Number of protocols */
	unsigned short		nlsp_protolist[NLP_MAXPROTOS];/* proto IDs */
	struct nlendpoint	nlsp_src;	/* Source of packet */
	struct nlendpoint	nlsp_dst;	/* Destination of packet */
};

/*
 * A set of snooped packets
 */
struct nlpacket {
	unsigned long		nlp_snoophost;	/* Address of sending host */
	unsigned short		nlp_version;	/* Version of packet */
	unsigned short		nlp_type;	/* Type of packet */
	unsigned int		nlp_count;	/* Number of data packets */
	struct nlspacket	nlp_nlsp[NLP_MAXSPACKETS];/* Snooped packets */
};

#endif /* !NETLOOK_H */
