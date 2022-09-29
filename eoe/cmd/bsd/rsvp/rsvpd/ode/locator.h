/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module locator.h, release 1.5 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/ode/include/SCCS/s.locator.h
 *sccs
 *sccs    1.5	96/02/12 12:57:00 randy
 *sccs	update company address (PR#613)
 *sccs
 *sccs    1.4	95/12/27 18:28:38 randy
 *sccs	added code for locator invocation at registration time (PR#626)
 *sccs
 *sccs    1.3	95/01/24 11:57:26 randy
 *sccs	update company address (PR#183)
 *sccs
 *sccs    1.2	92/11/12 16:33:02 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	92/02/05 14:43:23 randy
 *sccs	date and time created 92/02/05 14:43:23 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef ODELOCATORH
#define ODELOCATORH

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
 *	Manifest constants of use to locator functions.	 These are supplied
 *	as the third parameter to a locator function.  Most applications
 *	won't need this information; those that do will primarily be
 *	interested in whether this is an attempt to write a value.
 *
 *	These values are of greatest importance when the user locator
 *	function is providing access to cacheing mechanisms.  In particular,
 *	the access type may be useful in setting advisory locks, marking
 *	cache entries dirty, and so on.
 */
#define LOCATE_SET		0	/* locate context to write attr value */
#define LOCATE_GET		1	/* locate context to read attr value  */
#define LOCATE_NEXT		2	/* locate context to read attr value  */
#define LOCATE_TEST		3	/* locate context to read attr value  */
#define LOCATE_TRAP		4	/* locate context to read attr value  */
#define LOCATE_REGISTER		5	/* locate context to read attr value  */
#define LOCATE_DEREGISTER	6	/* locate context to read attr value  */

#endif
