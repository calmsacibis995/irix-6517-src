/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module mgrstubs.h, release 1.5 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.mgrstubs.h
 *sccs
 *sccs    1.5	96/02/12 13:16:25 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.4	95/01/24 13:00:33 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.3	92/11/12 16:33:41 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:16:08 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/08/31 14:05:57 randy
 *sccs	date and time created 92/08/31 14:05:57 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_MGRSTUBSH
#define AME_MGRSTUBSH

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
 *	mgrstubs.h - hooks for providing combined manager/agent support
 *
 *	These are stubs in a standard agent-only product.  In some kinds
 *	of proxies and protocol gateways, these entry points may be used
 *	to provide a framework for manager capabilities.
 *
 ************************************************************************/

#include "ame/machtype.h"

#ifndef USE_PROTOTYPES

#include "ame/stdtypes.h"

void	mgr_resp();
void	mgr_trap();
void	mgr_mask();
int	mgr_poll();
void	mgr_timer();
Void	*mgr_init();
void	mgr_deinit();

#else

#include "ame/fd.h"
#include "ame/appladdr.h"		/* get needed data types	*/
#include "ame/snmpmsg.h"

/*
 *	mgr_resp - process what looks like a response to a manager request
 */
void	mgr_resp(void				*mgr_env,
		 struct application_addr	*addr,
		 struct snmp_message		*info,
		 ubyte				*buf,
		 Len_t				len);

/*
 *	mgr_trap - a trap pdu has arrived
 */
void	mgr_trap(void				*mgr_env,
		 struct application_addr	*addr,
		 struct snmp_message		*info,
		 ubyte				*buf,
		 Len_t				len);

/*
 *	mgr_mask - update a select mask used to determine when manager
 *	activity is needed
 */
void	mgr_mask(Void	*mgr_env,
		 fd_set *mask);

/*
 *	mgr_poll - give manager code an opportunity to rune
 */
int	mgr_poll(Void	*mgr_env,
		 fd_set *mask);

/*
 *	mgr_timer - provide a timer tick to the manager code
 */
void	mgr_timer(Void	*mgr_env);

/*
 *	mgr_init - initialize manager capability
 */
Void	*mgr_init(int	argc,
		  char	**argv,
		  Void	*handle);

/*
 *	mgr_deinit - graceful shutdown of manager capability
 */
void	mgr_deinit(Void **mgr_env);

#endif
#endif
