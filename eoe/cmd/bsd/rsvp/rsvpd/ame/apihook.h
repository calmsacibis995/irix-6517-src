/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module apihook.h, release 1.8 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.apihook.h
 *sccs
 *sccs    1.8	96/02/12 13:14:26 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.7	95/01/24 12:59:52 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.6	94/07/11 16:44:22 randy
 *sccs	winsock port
 *sccs
 *sccs    1.5	93/07/01 18:44:57 randy
 *sccs	added hooks to sidestep DOS TSR interrupt nesting problems
 *sccs
 *sccs    1.4	93/04/15 22:04:26 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.3	92/11/12 16:33:16 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/10/23 15:28:48 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.1	92/09/11 19:07:09 randy
 *sccs	date and time created 92/09/11 19:07:09 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_APIHOOKH				/* avoid re-inclusion	*/
#define AME_APIHOOKH				/* prevent re-inclusion */

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
 *	apihook.h - data structures  needed by MS-DOS TSR access library
 */
#include "ame/machtype.h"


#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/appladdr.h"
#include "ame/fd.h"
#include "ame/time.h"
#include "ode/classdef.h"
#include "ode/mgmtpub.h"

/*
 *	This data structure contains the addresses of functions implemented
 *	in the "common" agent access library.  To avoid loading duplicate
 *	code with every sub-agent, we use the multiplex interrupt to find
 *	the address of this structure, and then make an indirect call into
 *	the real library.  See "apilib.c" in the peerlibs/apilib directory.
 *
 *	This is totally irrelevant outside the MS-DOS environment.
 */
struct mgmt_api_hooks
{
	short	major_version;
	short	minor_version;

#ifdef USE_PROTOTYPES
	Void	*(*mgmt_init_env)(struct sockaddr	*host,
				  READONLY ObjId_t	*me,
				  READONLY char		*desc,
				  READONLY Octets_t	*passwd,
				  void_function		idle);

	void	(*mgmt_term_env)(Void	*handle);

	Void	*(*mgmt_new_instance)(Void				*handle,
				      READONLY struct class_definition	*cls,
				      Void				*ctxt);

	int	(*mgmt_del_instance)(Void	*handle,
				     Void	*instance);

	void	(*mgmt_select_mask)(Void	*handle,
				    fd_set	*mask);

	int	(*mgmt_poll)(Void		*handle,
			     struct timeval	*timeout);

	int	(*mgmt_trap)(Void				*handle,
			     READONLY struct notification	*trap_type);

	void	(*bkg_lock)(void_function idle);
	void	(*bkg_unlock)(void);
#else
	Void	*(*mgmt_init_env)();
	void	(*mgmt_term_env)();
	Void	*(*mgmt_new_instance)();
	int	(*mgmt_del_instance)();
	void	(*mgmt_select_mask)();
	int	(*mgmt_poll)();
	int	(*mgmt_trap)();
	void	(*bkg_lock)();
	void	(*bkg_unlock)();
#endif

};

#endif
