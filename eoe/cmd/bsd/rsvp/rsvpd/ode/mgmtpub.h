/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module mgmtpub.h, release 1.11 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/ode/include/SCCS/s.mgmtpub.h
 *sccs
 *sccs    1.11	96/02/12 12:57:27 randy
 *sccs	update company address (PR#613)
 *sccs
 *sccs    1.10	95/01/24 11:57:34 randy
 *sccs	update company address (PR#183)
 *sccs
 *sccs    1.9	94/09/26 02:13:13 randy
 *sccs	simplified trap generation by compiler generated functions
 *sccs
 *sccs    1.8	94/06/09 15:38:21 randy
 *sccs	added prototypes for mgmt_set functions
 *sccs
 *sccs    1.7	93/04/26 17:51:39 randy
 *sccs	g++ support
 *sccs
 *sccs    1.6	93/04/14 13:10:32 randy
 *sccs	changed parameter name in anticipation of c++
 *sccs
 *sccs    1.5	92/12/17 17:42:29 randy
 *sccs	added mgmt_new_row()
 *sccs
 *sccs    1.4	92/11/12 16:33:05 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.3	92/10/23 15:20:15 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.2	92/09/11 19:04:40 randy
 *sccs	flattened dependencies
 *sccs
 *sccs    1.1	92/07/25 18:03:11 randy
 *sccs	date and time created 92/07/25 18:03:11 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef ODE_MGMTPUBH		/* avoid re-inclusion			*/
#define ODE_MGMTPUBH		/* prevent re-inclusion			*/

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
 *	mgmtpub.h - this header file contains function prototypes for
 *	all published interfaces to the Peer ODE (Object Development 
 *	Environment.)  The source for these functions is in mgmtapi.c
 *	in the ode library.
 */
#include "ame/machtype.h"
#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/time.h"
#include "ame/fd.h"
#include "ame/appladdr.h"	/* no matter how ugly they may be	*/
#include "ode/classdef.h"	/* get necessary data types		*/


/*
 *	Bit masks for return codes from mgmt_poll()
 */
#define MGMT_PROCESSED_COMMIT	0x01
#define MGMT_PROCESSED_SET	0x02
#define MGMT_PROCESSED_GET	0x04
#define MGMT_PROCESSED_NEXT	0x08
#define MGMT_PROCESSED_BULK	0x10
#define MGMT_PROCESSED_RRSP	0x20



#ifndef USE_PROTOTYPES

int	mgmt_check_install();	/* determine installation status	*/
Void	*mgmt_init_env();	/* initialize management environment	*/
void	mgmt_term_env();	/* terminate management environment	*/
Void	*mgmt_new_instance();	/* create new instance			*/
Void	*mgmt_new_row();	/* create new table row			*/
int	mgmt_del_instance();	/* delete previously created instance	*/
void	mgmt_select_mask();	/* retrieve mask for select()		*/
int	mgmt_poll();		/* let management environment run	*/
int	mgmt_set_priority();	/* set priority of registration requests*/
int	mgmt_set_rrsp_timeout();/* set timeout registration responses	*/
int	mgmt_set_trap_ip();	/* set IP address of SNMPv1 traps	*/
int	mgmt_trap();		/* generate an SNMP trap		*/
int	mgmt_lowlevel_trap();	/* generate an SNMP trap (low level)	*/
int	mgmt_get_status();	/* get connection status		*/

#else

/*
 *	mgmt_check_install - (dos compatibility function) determine whether
 *			     we exist.
 */
int	mgmt_check_install(int	suggested_mux_id);

/*
 *	mgmt_init_env -	    Initialize the management environment for this
 *			    task.  Establishes a session with the agent entity.
 */
Void	*mgmt_init_env(struct sockaddr		*host,
		       READONLY ObjId_t		*me,
		       READONLY char		*desc,
		       READONLY Octets_t	*passwd,
		       void_function		idle);

/*
 *	mgmt_term_env -	    Terminate the management environment for this
 *			    task.  Closes any session with the agent entity,
 *			    and releases any resources allocated by the ODE
 *			    for this task.
 */
void	mgmt_term_env(Void	*handle);

/*
 *	mgmt_new_instance - Inform the ODE of a new object instance, making
 *			    it accessible to management.
 */
Void	*mgmt_new_instance(Void					*handle,
			   READONLY struct class_definition	*classdef,
			   Void					*context);

/*
 *	mgmt_new_row - Inform the ODE of a single table row.  Do not
 *			    optimize registration with agent.
 */
Void	*mgmt_new_row(Void				*handle,
		      READONLY struct class_definition	*classdef,
		      Void				*context);

/*
 *	mgmt_del_instance - Inform the ODE of the demise of an object instance,
 *			    revoking management access to its information.
 */
int	mgmt_del_instance(Void	*handle,
			  Void	*instance);

/*
 *	mgmt_select_mask -  Set the bits in a mask, suitable for use by
 *			    select() for applications that need to block
 *			    while waiting for I/O on their own devices but
 *			    wish to continue management access.
 */
void	mgmt_select_mask(Void	*handle,
			 fd_set *mask);

/*
 *	mgmt_poll -	    Give the management environment an opportunity
 *			    to process any management requests forwarded by
 *			    the agent.
 */
int	mgmt_poll(Void			*handle,
		  struct timeval	*timeout);

/*
 *	mgmt_set_priority - set priority for registration requests
 */
int	mgmt_set_priority(void		*handle,
			  Number_t	new_priority);

/*
 *	mgmt_set_rrsp_timeout - set timeout for registration responses
 */
int	mgmt_set_rrsp_timeout(void	*handle,
			      Number_t	new_timeout);

/*
 *	mgmt_set_trap_ip -  set the subagent IP address for SNMPv1 traps
 */
int	mgmt_set_trap_ip(void		*handle,
			 IpAddr_t	new_ip);

/*
 *	mgmt_trap -	    Generate an SNMP trap, and send it to the agent.
 */
int	mgmt_trap(Void				*handle,
		  READONLY struct notification	*trap_type);

/*
 *	mgmt_lowlevel_trap - function for low-level access to trap sending
 */
int	mgmt_lowlevel_trap(Void			*handle,
		  READONLY struct notification	*trap_type,
		  Void				**parms);

/*
 *	mgmt_get_status - get connection status
 */
int	mgmt_get_status(Void	*handle);

#endif
#endif
