/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module pwlpub.h, release 1.8 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.pwlpub.h
 *sccs
 *sccs    1.8	96/02/12 13:17:40 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.7	95/01/24 13:00:57 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.6	93/06/04 15:25:58 randy
 *sccs	added missing ms-dos entry point
 *sccs
 *sccs    1.5	93/04/15 22:04:41 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.4	93/03/05 09:34:57 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.3	92/11/12 16:34:00 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/10/23 15:28:59 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.1	92/09/11 19:05:47 randy
 *sccs	date and time created 92/09/11 19:05:47 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_PWLPUBH				/* avoid re-inclusion	*/
#define AME_PWLPUBH				/* prevent re-inclusion */

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


/*
 *	pwlpub.h - public function prototypes for pwllib.lib
 *
 *	These are only relevant under MSDOS using Peer's adaptation of the
 *	Waterloo TCP/IP stack.
 *
 *		pwl_installed		determine installation status
 *		pwl_getownip		determine system's IP address
 *		pwl_gethostip		get another systems's address
 *		pwl_gettimeofday	get UNIX time of day
 *		pwl_udp_open		open a UDP socket
 *		pwl_udp_sendto		transmit UDP packet
 *		pwl_udp_recvfrom	receive UDP data
 *		pwl_udp_close		close a UDP socket
 *		pwl_tcp_listen		listen on TCP socket
 *		pwl_tcp_open		open a TCP connection
 *		pwl_tcp_connected	check connection status
 *		pwl_tcp_ready		is data waiting on TCP connection?
 *		pwl_tcp_read		read data from TCP connection
 *		pwl_tcp_write		write data onto TCP connection
 *		pwl_tcp_close		close a TCP connection
 *		pwl_mgmt_init		hook into management of TCP/IP
 */

#include "ame/machtype.h"

#ifdef PEER_MSDOS_PORT

#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/appladdr.h"
#include "ame/time.h"
#include "ode/classdef.h"

#ifndef USE_PROTOTYPES

int		pwl_installed();
IpAddr_t	pwl_getownip();
IpAddr_t	pwl_gethostip();
int		pwl_gettimeofday();
UdpSocket_type	pwl_udp_open();
int		pwl_udp_sendto();
int		pwl_udp_recvfrom();
int		pwl_udp_close();
TcpSocket_type	pwl_tcp_listen();
TcpSocket_type	pwl_tcp_open();
int		pwl_tcp_connected();
int		pwl_tcp_ready();
int		pwl_tcp_read();
int		pwl_tcp_write();
int		pwl_tcp_close();
int		pwl_mgmt_init();

#else

/*
 *	pwl_installed - Determine installation status
 */
int		pwl_installed(int suggested_mux_id);

/*
 *	pwl_getownip - get this system's IP address
 */
IpAddr_t	pwl_getownip(void);

/*
 *	pwl_gethostip - determine another system's IP address
 */
IpAddr_t	pwl_gethostip(void_function yield, READONLY char *name);

/*
 *	pwl_gettimeofday - determine system time in unix format
 */
int		pwl_gettimeofday(struct timeval *tv, struct timezone *tz);

/*
 *	pwl_udp_open - create socket for udp communication
 */
UdpSocket_type	pwl_udp_open(int port);

/*
 *	pwl_udp_sendto	- send a datagram
 */
int		pwl_udp_sendto(UdpSocket_type handle, char *buf, int buflen,
			       struct sockaddr *addr,
			       void_function yield);

/*
 *	pwl_udp_recvfrom - receive a datagram (non-blocking)
 */
int		pwl_udp_recvfrom(UdpSocket_type handle, char *buf, int buflen,
				 struct sockaddr *from);

/*
 *	pwl_udp_close - close a UDP socket
 */
int		pwl_udp_close(UdpSocket_type handle);

/*
 *	pwl_tcp_listen - open a tcp socket for listen
 */
TcpSocket_type	pwl_tcp_listen(int lport, void_function yield);

/*
 *	pwl_tcp_open - request a TCP connection
 */
TcpSocket_type	pwl_tcp_open(IpAddr_t		ip,
			     int		port,
			     void_function	swap);

/*
 *	pwl_tcp_connected - has a connection been established?
 */
int		pwl_tcp_connected(TcpSocket_type s);

/*
 *	pwl_tcp_ready - is data waiting on TCP connection?
 */
int		pwl_tcp_ready(TcpSocket_type s);

/*
 *	pwl_tcp_read - read data from TCP connection
 */
int		pwl_tcp_read(TcpSocket_type s, char *data, int len);

/*
 *	pwl_tcp_write - write data onto TCP connection
 */
int		pwl_tcp_write(TcpSocket_type s, char *data, int len);

/*
 *	pwl_tcp_close - close a TCP connection
 */
int		pwl_tcp_close(TcpSocket_type s);


/*
 *	pwl_mgmt_init - hook up TCP/IP stack management	 (should only be
 *	called by the sub-agent resposible for the TCP/IP stack!)
 */
int	pwl_mgmt_init(Void *(*func) (Void	*handle,
				     READONLY struct class_definition	*cls,
				     Void	*context),
		      Void *ctxt);

#endif

#endif

#endif
