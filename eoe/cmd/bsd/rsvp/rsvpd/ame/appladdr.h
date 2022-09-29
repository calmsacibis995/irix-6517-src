/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module appladdr.h, release 1.10 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.appladdr.h
 *sccs
 *sccs    1.10	96/02/12 13:14:40 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.9	95/09/01 11:54:30 randy
 *sccs	cleaner compile under win32 (PR#534)
 *sccs
 *sccs    1.8	95/01/24 12:59:57 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.7	94/01/06 16:21:29 randy
 *sccs	hooks for multiple active interfaces
 *sccs
 *sccs    1.6	93/05/28 02:15:59 randy
 *sccs	minor preprocessor cleanup
 *sccs
 *sccs    1.5	93/05/27 22:44:59 randy
 *sccs	tolerate lynx
 *sccs
 *sccs    1.4	93/04/26 17:54:08 randy
 *sccs	psos support
 *sccs
 *sccs    1.3	92/11/12 16:33:19 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/08/31 14:23:55 randy
 *sccs	include MS-DOS version support
 *sccs
 *sccs    1.1	92/01/02 11:36:48 randy
 *sccs	date and time created 92/01/02 11:36:48 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AMEAPPLADDRH
#define AMEAPPLADDRH

/************************************************************************
 *                                                                      *
 *          PEER Networks, a division of BMC Software, Inc.             *
 *                   CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                      *
 *	This product is the sole property of PEER Networks, a		*
 *	division of BMC Software, Inc., and is protected by U.S.	*
 *	and other copyright laws and the laws protecting trade		*
 *	secret and confidential information.				*
 *									*
 *	This product contains trade secret and confidential		*
 *	information, and its unauthorized disclosure is			*
 *	prohibited.  Authorized disclosure is subject to Section	*
 *	14 "Confidential Information" of the PEER Networks, a		*
 *	division of BMC Software, Inc., Standard Development		*
 *	License.							*
 *									*
 *	Reproduction, utilization and transfer of rights to this	*
 *	product, whether in source or binary form, is permitted		*
 *	only pursuant to a written agreement signed by an		*
 *	authorized officer of PEER Networks, a division of BMC		*
 *	Software, Inc.							*
 *									*
 *	This product is supplied to the Federal Government as		*
 *	RESTRICTED COMPUTER SOFTWARE as defined in clause		*
 *	55.227-19 of the Federal Acquisitions Regulations and as	*
 *	COMMERCIAL COMPUTER SOFTWARE as defined in Subpart		*
 *	227.401 of the Defense Federal Acquisition Regulations.		*
 *									*
 * Unpublished, Copr. PEER Networks, a division of BMC	Software, Inc.  *
 *                     All Rights Reserved				*
 *									*
 *	PEER Networks, a division of BMC Software, Inc.			*
 *	1190 Saratoga Avenue, Suite 130					*
 *	San Jose, CA 95129-3433 USA					*
 *									*
 ************************************************************************/



/*************************************************************************
 *
 *	ame/appladdr.h - application address structure definition
 *
 *	This header file encapsulates all the tidbits needed to deal
 *	with network and system application addressing.
 *
 ************************************************************************/

#include "ame/machtype.h"


#ifdef PEER_MSDOS_PORT

#include "ame/smitypes.h"

#define AF_INET		2
#define SOCK_STREAM	1
#define SOCK_DGRAM	2

/*
 *	The following is compatible with the definition used by the Waterloo
 *	TCP code.  Modify it as needed to match your system.
 */
struct sockaddr
{
    unsigned short	s_type;
    unsigned short	s_port;
    unsigned long	s_ip;
    ubyte		s_spares[6];	/* unused in TCP realm */
};

struct sockaddr_in
{
	unsigned short	sin_family;
	unsigned short	sin_port;
	struct sin_addr
	{
		IpAddr_t	s_addr;
	} sin_addr;

	ubyte		s_spares[6];	/* unused in TCP realm */
};

#endif


#include "ame/socket.h"



/*
 *	Encapsulate environment-specific representation of addresses of
 *	entities in the network.
 */
struct application_addr
{
	unsigned int	interface_id;	/* what interface to use	*/
	int		addr_len;	/* bytes used in the addr	*/
	struct sockaddr addr;		/* non-specific address layout	*/
};

#endif
