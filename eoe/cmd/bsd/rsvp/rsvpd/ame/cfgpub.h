/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module cfgpub.h, release 1.6 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.cfgpub.h
 *sccs
 *sccs    1.6	96/02/12 13:15:08 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.5	95/01/24 13:00:08 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.4	94/07/11 16:52:09 randy
 *sccs	make free_string public for use by manager demo
 *sccs
 *sccs    1.3	92/11/12 16:33:27 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 19:16:05 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/08/31 14:05:24 randy
 *sccs	date and time created 92/08/31 14:05:24 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_CFGPUBH				/* avoid re-inclusion	*/
#define AME_CFGPUBH				/* prevent re-inclusion */

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
 *	cfgpub.h - function prototypes for access points into configuration
 *	library.  These can be stubbed if your target does not need the
 *	agent configuration file capability.
 */
#include "ame/machtype.h"
#include "ame/stdtypes.h"

#ifndef USE_PROTOTYPES

int	parse_config_file();

/*
 *	free_string - get rid of a string allocated from the heap
 */
void	free_string();

#else

int	parse_config_file(Void	*agent,
			  char	*file_name);

/*
 *	free_string - get rid of a string allocated from the heap
 */
void	free_string(char **str_ptr);

#endif

#endif
