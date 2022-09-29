/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module portpub.h, release 1.16 dated 96/04/30
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.portpub.h
 *sccs
 *sccs    1.16	96/04/30 12:20:14 rpresuhn
 *sccs	made out_decimal public (PR#714)
 *sccs
 *sccs    1.15	96/02/12 13:17:24 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.14	95/07/20 17:26:41 sam
 *sccs	Minor additions for Winsock support.
 *sccs
 *sccs    1.13	95/05/24 16:59:22 randy
 *sccs	tighter prototype for generic_tcp_write (PR#338)
 *sccs
 *sccs    1.12	95/05/05 16:58:16 randy
 *sccs	added peer_perror and changed signedness to aid portability (PR#338)
 *sccs
 *sccs    1.11	95/01/24 13:00:50 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.10	94/05/25 11:53:09 randy
 *sccs	loosened prototype to keep vrtx happy
 *sccs
 *sccs    1.9	93/10/21 11:20:01 randy
 *sccs	added out_string function
 *sccs
 *sccs    1.8	93/06/04 15:25:20 randy
 *sccs	added missing MS-DOS specific entry
 *sccs
 *sccs    1.7	93/04/08 17:28:52 randy
 *sccs	better use of READONLY construct
 *sccs
 *sccs    1.6	93/03/17 17:40:44 randy
 *sccs	added support for PDU tracing
 *sccs
 *sccs    1.5	93/03/05 09:34:53 randy
 *sccs	added manifests for target-specific versions
 *sccs
 *sccs    1.4	92/12/22 14:08:42 randy
 *sccs	added getsysname
 *sccs
 *sccs    1.3	92/11/12 16:33:56 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:11:49 randy
 *sccs	moved functions from ode library to ports library
 *sccs
 *sccs    1.1	92/08/31 14:17:30 randy
 *sccs	date and time created 92/08/31 14:17:30 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_PORTPUBH			/* avoid re-inclusion		*/
#define AME_PORTPUBH			/* prevent re-inclusion		*/

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
 *	portpub.h - public routines from the ports library
 *
 ************************************************************************/

#include "ame/machtype.h"

#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/fd.h"
#include "ame/time.h"
#include "ame/socket.h"
#include "ame/appladdr.h"


/*
 *	In a TSR environment, a callback for context switching is needed
 *	Under Unix and virtually everything else, this is ignored.
 */
#ifdef PEER_MSDOS_PORT
#ifndef USE_PROTOTYPES
extern void	swap_bkg();
#else
extern void	swap_bkg(void);
#endif                  /* USE_PROTOTYPES */
#else
#ifdef PEER_WINSOCK_PORT
extern void	WindowsIdleProc(void);
#define swap_bkg	WindowsIdleProc
#else
#define swap_bkg	(void_function) NULL
#endif                  /* PEER_WINSOCK_PORT */
#endif                  /* PEER_MSDOS_PORT */


#ifndef USE_PROTOTYPES

/*
 *	addrcmp - address comparison.  This is very specific to the local
 *	system's network address representation.
 */
int	addrcmp();

/*
 *	bind_appladdr - bind an application address to a socket
 */
int	bind_appladdr();

/*
 *	connect_appladdr - request a connection to an application address
 */
int	connect_appladdr();

/*
 *	del_appladdr - get rid of an application address
 */
void	del_appladdr();

/*
 *	generic_select - wait for something to happen
 */
int	generic_select();

/*
 *	generic_tcp_close - issue a DISCONNECT Request / close
 */
void	generic_tcp_close();

/*
 *	generic_tcp_connected - determine whether a connection is complete
 */
int	generic_tcp_connected();

/*
 *	generic_tcp_read - pull in data from a socket
 */
int	generic_tcp_read();

/*
 *	generic_tcp_ready - determine whether data is waiting on a socket
 */
int	generic_tcp_ready();

/*
 *	generic_tcp_write - write data onto a TCP connection
 */
int	generic_tcp_write();

/*
 *	generic_udp_close - close a UDP socket
 */
void	generic_udp_close();

/*
 *	generic_udp_read - read a datagram
 */
int	generic_udp_read();

/*
 *	generic_udp_write - send a datagram
 */
int	generic_udp_write();

/*
 *	get_appladdr_ip - get the IP portion of an application address
 */
IpAddr_t get_appladdr_ip();

/*
 *	get_appladdr_port - get the port portion of an application address
 */
int	get_appladdr_port();

/*
 *	get_ip_by_name - figure out the IP address to go with a host name
 */
IpAddr_t	get_ip_by_name();

/*
 *	get_own_ip - determine what our own IP address is
 */
IpAddr_t	get_own_ip();

/*
 *	getnow - retrieve the current system time
 */
void	getnow();

/*
 *	getownname - determine what this host likes to be called
 */
int	getownname();

/*
 *	getsysname - determine what this host likes to be called, including
 *	domain information
 */
int	getsysname();

/*
 *	init_appladdr - initialize an address structure
 */
void	init_appladdr();

/*
 *	maxfiles - determine the maximum number of open files allowed
 */
unsigned int	maxfiles();

/*
 *	out_decimal - send a decimal string to stderr
 */
void	out_decimal();

/*
 *	out_string - send a string to stderr
 */
void	out_string();

/*
 *	peer_perror - stand-in function for perror
 */
void	peer_perror();

/*
 *	new_appladdr - create a new application address
 */
struct application_addr *new_appladdr();

/*
 *	set_appladdr_host  - set the host address portion (ip + port)
 */
void	set_appladdr_host();

/*
 *	set_appladdr_ip - set the IP portion of an application address
 */
void	set_appladdr_ip();

/*
 *	set_appladdr_port - set the port portion of an application address
 */
void	set_appladdr_port();

/*
 *	snmp_delta_t - retern ticks between now and some starting point
 */
Counter_t	snmp_delta_t();


#ifdef NEED_YYWRAP
/*
 *	yywrap - this may be supplied by your system's libraries
 */
int	yywrap();
#endif


#else


/*
 *	addrcmp - address comparison.  This is very specific to the local
 *	system's network address representation.
 */
int	addrcmp(READONLY struct application_addr *l,
		READONLY struct application_addr *r);

/*
 *	bind_appladdr - bind an application address to a socket
 */
int	bind_appladdr(int			*s,
		      struct application_addr	*a);

/*
 *	connect_appladdr - request a connection to an application address
 */
int	connect_appladdr(int				*s,
			 struct application_addr	*a);

/*
 *	del_appladdr - get rid of an application address
 */
void	del_appladdr(struct application_addr	**a);

/*
 *	generic_tcp_close - issue a DISCONNECT Request / close
 */
void	generic_tcp_close(TcpSocket_type	*sock);

/*
 *	generic_tcp_connected - determine whether a connection is complete
 */
int	generic_tcp_connected(TcpSocket_type	sock,
			      fd_set		*mask);

/*
 *	generic_tcp_read - pull in data from a socket
 */
int	generic_tcp_read(TcpSocket_type		sock,
			 char			*buf,
			 int			len);

/*
 *	generic_tcp_ready - determine whether data is waiting on a socket
 */
int	generic_tcp_ready(TcpSocket_type	sock,
			  fd_set		*mask);

/*
 *	generic_tcp_write - write data onto a TCP connection
 */
int	generic_tcp_write(TcpSocket_type	sock,
			  READONLY char		*buf,
			  int			len);

/*
 *	generic_select - wait for something to happen
 */
int	generic_select(unsigned int	mask_width,
		       fd_set		*read_mask,
		       struct timeval	*timeout);

/*
 *	generic_udp_close - close a UDP socket
 */
void	generic_udp_close(UdpSocket_type	*sock_ptr);

/*
 *	generic_udp_read - read a datagram
 */
int	generic_udp_read(UdpSocket_type			sock,
			 char				*buf,
			 int				len,
			 struct application_addr	*appl_addr);

/*
 *	generic_udp_write - send a datagram
 */
int	generic_udp_write(UdpSocket_type		sock,
			  char				*buf,
			  int				len,
			  struct application_addr	*appl_addr,
			  void				(*idle)(void));

/*
 *	get_appladdr_ip - get the IP portion of an application address
 */
IpAddr_t get_appladdr_ip(READONLY struct application_addr	*a);

/*
 *	get_appladdr_port - get the port portion of an application address
 */
int	get_appladdr_port(READONLY struct application_addr	*a);

/*
 *	get_ip_by_name - figure out the IP address to go with a host name
 */
IpAddr_t	get_ip_by_name(char *hostname);

/*
 *	get_own_ip - determine what our own IP address is
 */
IpAddr_t	get_own_ip(void);

/*
 *	getnow - retrieve the current system time
 */
void	getnow(struct timeval	*tv);

/*
 *	getownname - determine what this host likes to be called
 */
int	getownname(ubyte *dest_name, Len_t	maxlen);

/*
 *	getsysname - determine what this host likes to be called, including
 *	domain information
 */
int	getsysname(ubyte *dest_name, Len_t	maxlen);

/*
 *	init_appladdr - initialize an address structure
 */
void	init_appladdr(struct application_addr	*a,
		      READONLY IpAddr_t		ip,
		      READONLY int		p);

/*
 *	maxfiles - determine the maximum number of open files allowed
 */
unsigned int	maxfiles(void);

/*
 *	out_decimal - send a decimal string to stderr
 */
void	out_decimal(READONLY Number_t	n);

/*
 *	out_string - send a string to stderr
 */
void	out_string(READONLY char *s);

/*
 *	peer_perror - stand-in function for perror
 */
void	peer_perror(READONLY char *s);

/*
 *	new_appladdr - create a new application address
 */
struct application_addr *new_appladdr(READONLY IpAddr_t ip,
				      READONLY int	port);

/*
 *	set_appladdr_host  - set the host address portion (ip + port)
 */
void	set_appladdr_host(struct application_addr	*a,
			  READONLY struct sockaddr	*s);

/*
 *	set_appladdr_ip - set the IP portion of an application address
 */
void	set_appladdr_ip(struct application_addr *a,
			READONLY IpAddr_t	i);

/*
 *	set_appladdr_port - set the port portion of an application address
 */
void	set_appladdr_port(struct application_addr	*a,
			  READONLY int			p);

/*
 *	snmp_delta_t - retern ticks between now and some starting point
 */
Counter_t	snmp_delta_t(struct timeval *t);


#ifdef NEED_YYWRAP
/*
 *	yywrap - this may be supplied by your system's libraries
 */
int	yywrap(void);
#endif


#endif


/*
 *	The following are peculiar to the MS-DOS environment.
 *	They provide the address of the call table used to provide the
 *	addresses of the standard API calls' implementations.
 */
#ifdef PEER_MSDOS_PORT
#ifdef USE_PROTOTYPES

extern Void	*apihook_addr(void);

#else

extern Void	*apihook_addr();

#endif
#endif

/*
 *	The following exist only if the tracing package is enabled.  This
 *	is a compile-time option that should not be used for production
 *	code, since it adversely affects both size and cpu usage.
 */
#ifdef TRACE_ENABLED
#ifdef USE_PROTOTYPES

#include "ame/snmpvbl.h"
#include "ame/snmppdus.h"
#include "ame/snmpmsg.h"
#include "ame/smuxpdu.h"

/*
 *	trace_header - output a header for a trace message
 */
void	trace_header(READONLY char	*msg);

/*
 *	display_buf - quick routine to display contents of buffer
 */
void	display_buf(READONLY ubyte	*buf,
		    READONLY Len_t	len);

/*
 *	display_objid - display object identifier in dotted form
 */
void	display_objid(READONLY ObjId_t	*objid);

/*
 *	display_obj - quick routine to display contents of object
 */
void	display_obj(READONLY struct snmp_object *obj);

/*
 *	display_vbinds - display the contents of a variable binding list
 */
void	display_vbinds(READONLY struct var_bind_list	*vars);

/*
 *	display_pdu - display an snmp pdu (formatted)
 */
void	display_pdu(READONLY struct snmp_pdus	*pdu);

/*
 *	display_msg - display an snmp message (formatted)
 */
void	display_msg(READONLY struct snmp_message	*msg);

/*
 *	display_smux - display a formatted smux pdu
 */
void	display_smux(READONLY struct smux_pdu	*s);

#else	/* USE_PROTOTYPES */

/*
 *	trace_header - output a header for a trace message
 */
void	trace_header();

/*
 *	display_buf - quick routine to display contents of buffer
 */
void	display_buf();

/*
 *	display_objid - display object identifier in dotted form
 */
void	display_objid();

/*
 *	display_obj - quick routine to display contents of object
 */
void	display_obj();

/*
 *	display_vbinds - display the contents of a variable binding list
 */
void	display_vbinds();

/*
 *	display_pdu - display an snmp pdu (formatted)
 */
void	display_pdu();

/*
 *	display_msg - display an snmp message (formatted)
 */
void	display_msg();

/*
 *	display_smux - display a formatted smux pdu
 */
void	display_smux();

#endif	/* USE_PROTOTYPES */
#endif	/* TRACE_ENABLED */

#endif
