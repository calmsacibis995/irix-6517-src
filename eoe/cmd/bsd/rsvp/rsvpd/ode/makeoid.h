/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module makeoid.h, release 1.9 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/ode/include/SCCS/s.makeoid.h
 *sccs
 *sccs    1.9	96/02/12 12:57:14 randy
 *sccs	update company address (PR#613)
 *sccs
 *sccs    1.8	95/01/24 11:57:30 randy
 *sccs	update company address (PR#183)
 *sccs
 *sccs    1.7	94/01/27 17:41:01 randy
 *sccs	moved prototype
 *sccs
 *sccs    1.6	93/08/19 15:55:27 randy
 *sccs	added routines for IMPLIED forms from SNMPv2
 *sccs
 *sccs    1.5	93/04/14 13:09:36 randy
 *sccs	changed parameter name in anticipation of c++
 *sccs
 *sccs    1.4	93/04/08 16:13:26 randy
 *sccs	better use of READONLY
 *sccs
 *sccs    1.3	92/11/12 16:41:10 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.2	92/09/11 18:40:25 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/07/26 08:00:20 randy
 *sccs	date and time created 92/07/26 08:00:20 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef MAKEOIDH			/* avoid recursive re-inclusion */
#define MAKEOIDH

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
 *	This header file has the function prototypes for the public
 *	functions defined in makeoid.c
 */
#include "ame/machtype.h"
#include "ame/smitypes.h"
#include "ame/snmpvbl.h"		/* get required data types	*/


#ifndef USE_PROTOTYPES

/*
 *	makeoid_int - tack an integer onto an object id
 */
int	makeoid_int();

/*
 *	makeoid_fixstr - make an object identifier by tacking on a fixed-
 *			 length string.
 */
int	makeoid_fixstr();

/*
 *	makeoid_str - tack on a variable-length string to object identifier
 */
int	makeoid_str();

/*
 *	makeoid_objid - join two object identifiers
 */
int	makeoid_objid();

/*
 *	makeoid_netaddr - tack on a network address to an object identifier
 */
int	makeoid_netaddr();

/*
 *	makeoid_ipaddr - tack on an IP address to an object identifier
 */
int	makeoid_ipaddr();

/*
 *	makeoid_objid_implied - join two object identifiers using V2 rules
 */
int	makeoid_objid_implied();

/*
 *	extroid_int - extract an integer from an object id
 */
int	extroid_int();

/*
 *	extroid_fixstr - extract a fixed-length string
 */
int	extroid_fixstr();

/*
 *	extroid_str - extract a variable-length string
 */
int	extroid_str();

/*
 *	extroid_objid - extract another object identifier
 */
int	extroid_objid();

/*
 *	extroid_objid_implied - extract IMPLIED object identifier
 */
int	extroid_objid_implied();

/*
 *	extroid_netaddr - extract a network address
 */
int	extroid_netaddr();

/*
 *	extroid_ipaddr	extract an IP address
 */
int	extroid_ipaddr();

#else

/*
 *	makeoid_int - tack an integer onto an object id
 */
int	makeoid_int(READONLY ObjId_t *base,
		    ObjId_t *newoid,
		    READONLY Counter_t val);

/*
 *	makeoid_fixstr - make an object identifier by tacking on a fixed-
 *			 length string.
 */
int	makeoid_fixstr(READONLY ObjId_t *base,
		       ObjId_t *newoid,
		       READONLY Octets_t *val);

/*
 *	makeoid_str - tack on a variable-length string to object identifier
 */
int	makeoid_str(READONLY ObjId_t *base,
		    ObjId_t *newoid,
		    READONLY Octets_t *val);

/*
 *	makeoid_objid - join two object identifiers
 */
int	makeoid_objid(READONLY ObjId_t *base,
		      ObjId_t *newoid,
		      READONLY ObjId_t *val);

/*
 *	makeoid_netaddr - tack on a network address to an object identifier
 */
int	makeoid_netaddr(READONLY ObjId_t *base,
			ObjId_t *newoid,
			READONLY NetAddr_t *val);

/*
 *	makeoid_ipaddr - tack on an IP address to an object identifier
 */
int	makeoid_ipaddr(READONLY ObjId_t *base,
		       ObjId_t *newoid,
		       READONLY IpAddr_t *val);

/*
 *	makeoid_objid_implied - join two object identifiers using V2 rules
 */
int	makeoid_objid_implied(READONLY ObjId_t *base,
			      ObjId_t *newoid,
			      READONLY ObjId_t *val);

/*
 *	extroid_int - extract an integer from an object id
 */
int	extroid_int(ObjId_t *base, READONLY ObjId_t *name, Counter_t *val);

/*
 *	extroid_fixstr - extract a fixed-length string
 */
int	extroid_fixstr(ObjId_t *base, READONLY ObjId_t *name, Octets_t *val);

/*
 *	extroid_str - extract a variable-length string
 */
int	extroid_str(ObjId_t *base, READONLY ObjId_t *name, Octets_t *val);

/*
 *	extroid_objid - extract another object identifier
 */
int	extroid_objid(ObjId_t *base, READONLY ObjId_t *name, ObjId_t *val);

/*
 *	extroid_objid_implied - extract an IMPLIED object identifier
 */
int	extroid_objid_implied(ObjId_t *base, READONLY ObjId_t *name,
			      ObjId_t *val);

/*
 *	extroid_netaddr - extract a network address
 */
int	extroid_netaddr(ObjId_t *base, READONLY ObjId_t *name, NetAddr_t *val);

/*
 *	extroid_ipaddr	extract an IP address
 */
int	extroid_ipaddr(ObjId_t *base, READONLY ObjId_t *name, IpAddr_t *val);

#endif


#endif
