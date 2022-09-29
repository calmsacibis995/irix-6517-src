/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module snmpsmpu.h, release 1.16 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.snmpsmpu.h
 *sccs
 *sccs    1.16	96/02/12 13:20:07 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.15	95/09/01 11:55:02 randy
 *sccs	cleaner compile under win32 (PR#534)
 *sccs
 *sccs    1.14	95/07/07 14:09:19 randy
 *sccs	make sure snmpv2 conditionality of prototypes matches than of definitions (PR#265)
 *sccs
 *sccs    1.13	95/05/02 15:18:40 randy
 *sccs	ensure that community member count is unsigned (PR#338)
 *sccs
 *sccs    1.12	95/01/24 13:01:35 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.11	94/11/30 19:10:32 randy
 *sccs	include ame/snmpmsg.h for structure definition (PR#142)
 *sccs
 *sccs    1.10	94/11/02 10:22:07 randy
 *sccs	modified prototype for environments with 16-bit ints
 *sccs
 *sccs    1.9	94/08/31 11:55:08 randy
 *sccs	included prototype to allow G++ compilation of manager sample program
 *sccs
 *sccs    1.8	94/08/30 16:41:55 randy
 *sccs	changed ifdefs to tolerate some snmpv2 parameters in v1 mode
 *sccs
 *sccs    1.7	94/04/15 17:11:44 randy
 *sccs	novram can be seeded with arbitrary octet strings
 *sccs
 *sccs    1.6	94/04/09 11:29:31 randy
 *sccs	seed party mib from config file
 *sccs
 *sccs    1.5	94/01/27 18:12:32 randy
 *sccs	added prototypes to support new configuration file options
 *sccs
 *sccs    1.4	94/01/06 16:23:12 randy
 *sccs	added prototypes for setting parameters from configuration file
 *sccs
 *sccs    1.3	92/11/12 16:34:35 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:16:26 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/08/31 14:15:57 randy
 *sccs	date and time created 92/08/31 14:15:57 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_SNMPSMPUH				/* avoid re-inclusion	*/
#define AME_SNMPSMPUH				/* prevent re-inclusion */

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
 *	snmpsmpu.h  -  public functions exported from snmpsm directory
 *
 *	If you replace the configuration directory with some other meachanism,
 *	these entry points will be your means of telling the agent about
 *	community and manager configuration.
 */
#include "ame/machtype.h"
#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/appladdr.h"
#include "ame/snmpmsg.h"


/*
 *	Define constants used to pass information from configuration file
 *	processing to the snmp protocol engine
 *
 *	These are ORed together to describe a specific interface.
 *	An interface description is built from three components:
 *		- the application protocol (snmp or smux)
 *		- the api to use (socket or TLI)
 *		- the transport (tcp, udp, or unix named pipes)
 */
#define LISTENER_SNMP	0x0001			/* SNMP application layer*/
#define LISTENER_TRAP	0x0002			/* SNMP application layer*/
#define LISTENER_SMUX	0x0004			/* SMUX application layer*/

#define LISTENER_SOCKET 0x0010			/* using sockets API	 */
#define LISTENER_TLI	0x0020			/* using streams TLI API */

#define LISTENER_UDP	0x0100			/* use UDP as transport	 */
#define LISTENER_TCP	0x0200			/* use TCP as transport	 */
#define LISTENER_UNIX	0x0400			/* use unix named pipes	 */


/*
 *	The following are used in seeding non-volatile MIB parameters
 *	These are passed to the function seed_nonvola().
 */
#define SYSLOCATION_ID	1
#define SYSCONTACT_ID	2


/*
 *	Define SNMPv2 transport mappings (see rfc 1449)
 *	These must align with data structures in snmpsm/src/wrk/party.c
 */
#define TM_none		  0
#define TM_snmpUDPDomain  1
#define TM_snmpCLNSDomain 2
#define TM_snmpCONSDomain 3
#define TM_snmpDDPDomain  4
#define TM_snmpIPXDomain  5
#define TM_rfc1157Domain  6


#ifdef SNMPV2_SUPPORTED
/*
 *	Define SNMPv2 Authentication Protocols (see rfc1449 and rfc 1447)
 */
#define AP_none			0
#define AP_noAuth		1
#define AP_rfc1157noAuth	2
#ifdef MD5_SUPPORTED
#define AP_v2md5AuthProtocol	3
#endif


/*
 *	Define SNMPv2 Privacy Protocols
 */
#define PP_none			0
#define PP_noPriv		1
#ifdef DES_SUPPORTED
#define PP_desPrivProtocol	2
#endif

#endif



#ifndef USE_PROTOTYPES

/*
 *	choose_snmp_interface - select an appropriate interface for an
 *				address needing an SNMP transport
 */
int	choose_snmp_interface();

/*
 *	new_community - add a community to this agent's list
 */
int	new_community();

/*
 *	new_entity - make note of a new spatial/temporal entity
 */
int	new_entity();

/*
 *	new_mgr_partner - make note of a new manager partner
 */
int	new_mgr_partner();

/*
 *	new_subagent_control - identify a subagent control record
 */
int	new_subagent_control();

/*
 *	open_listener - open a listener interface
 */
int	open_listener();

/*
 *	seed_nonvola - Seed a nonvolatile memory parameter
 */
void	seed_nonvola();

#ifdef SNMPV2_SUPPORTED
/*
 *	seed_acl - inform core agent of potential access control list entry
 */
int	seed_acl();

/*
 *	seed_context - inform core agent of potential context definition
 */
int	seed_context();

/*
 *	seed_party - inform core agent of a potential party definition
 */
int	seed_party();
#endif

/*
 *	seed_view_set - inform core agent of a potential view set
 */
int	seed_view_set();

/*
 *	seed_view_subtree - inform core agent of view subtree
 */
int	seed_view_subtree();

/*
 *	send_snmp - generic routine to send snmp pdus
 */
int	send_smnp();

#else

/*
 *	choose_snmp_interface - select an appropriate interface for an
 *				address needing an SNMP transport
 */
int	choose_snmp_interface(Void			*agent,
			      struct application_addr	*this_end);

/*
 *	new_community - add a community to this agent's list
 */
int	new_community(Void			*hand,
		      Octets_t			*name,
		      unsigned long		algorithm,
		      unsigned long		permissions,
		      unsigned int		member_cnt,
		      struct application_addr	*member_list,
		      int			entity);

/*
 *	new_entity - make note of a new spatial/temporal entity
 */
int	new_entity(Void		*context,
		   Octets_t	*description,
		   ObjId_t	*time_domain);

/*
 *	new_mgr_partner - make note of a new manager partner
 */
int	new_mgr_partner(Void			*agent,
			struct application_addr *reply_addr,
			struct application_addr *trap_addr,
			Octets_t		*trap_commun);

/*
 *	new_subagent_control - identify a subagent control record
 */
int	new_subagent_control(Void			*ctxt,
			     ObjId_t			*oid,
			     struct application_addr	*hostid,
			     Octets_t			*password,
			     int			allow,
			     int			timeout,
			     unsigned int		entity);

/*
 *	open_listener - open a listener interface
 */
int	open_listener(Void			*hand,
		      int			kind,
		      struct application_addr	*address);

/*
 *	seed_nonvola - Seed a nonvolatile memory parameter
 */
void	seed_nonvola(Void			*agent,
		     int			kind,
		     Octets_t			*val);

#ifdef SNMPV2_SUPPORTED
/*
 *	seed_acl - inform core agent of potential access control list entry
 */
int	seed_acl(Void				*agent,
		 int				operations,
		 INTEGER			destination_index,
		 INTEGER			source_index,
		 INTEGER			context_index);

/*
 *	seed_context - inform core agent of potential context definition
 */
int	seed_context(Void			*agent,
		     ObjId_t			*context_oid,
		     int			is_local,
		     int			view_index,
		     unsigned int		entity_index,
		     ObjId_t			*proxy_source,
		     ObjId_t			*proxy_dest,
		     ObjId_t			*proxy_context);

/*
 *	seed_party - inform core agent of a potential party definition
 */
int	seed_party(Void				*agent,
		   ObjId_t			*party_oid,
		   int				transport,
		   struct application_addr	*transport_address,
		   int				is_local,
		   int				auth_proto,
		   Octets_t			*auth_priv_key,
		   Octets_t			*auth_pub_key,
		   INTEGER			lifetime,
		   int				priv_proto,
		   Octets_t			*priv_priv_key,
		   Octets_t			*priv_pub_key);
#endif

/*
 *	seed_view_set - inform core agent of a potential view set
 */
int	seed_view_set(Void			*agent);

/*
 *	seed_view_subtree - inform core agent of view subtree
 */
int	seed_view_subtree(Void			*agent,
			  int			view_set_index,
			  int			included,
			  ObjId_t		*subtree,
			  Octets_t		*mask);

/*
 *	send_snmp - generic routine to send snmp pdus
 */
int	send_smnp(Void				*agent,
		  struct application_addr	*addr,
		  struct snmp_message		*msg);

#endif
#endif
