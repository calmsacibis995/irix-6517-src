/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module odepub.h, release 1.17 dated 96/03/21
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.odepub.h
 *sccs
 *sccs    1.17	96/03/21 11:12:32 santosh
 *sccs	Update copyright notice (PR#693)
 *sccs
 *sccs    1.16	96/02/13 19:59:17 sam
 *sccs	More fixes to getnext code. (PR#30)
 *sccs
 *sccs    1.15	96/02/12 13:16:54 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.14	96/02/10 12:25:37 sam
 *sccs	Minor Updates to previous fixes. (PR#30) (PR#207) (PR#323) (PR#334) (PR#432)
 *sccs
 *sccs    1.13	96/02/08 15:48:30 sam
 *sccs	Added a convenient macro for comparing OIDs and added a few extra prototypes. (PR#30) (PR#207) (PR#323) (PR#334)
 *sccs
 *sccs    1.12	95/06/16 10:36:08 randy
 *sccs	added prototypes for support functions for explode/implode of oids (PR#323)
 *sccs
 *sccs    1.11	95/01/24 13:00:42 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.10	94/01/27 18:13:06 randy
 *sccs	made more utiltity functions public
 *sccs
 *sccs    1.9	93/10/26 15:39:29 randy
 *sccs	avoid C++ keyword conflict
 *sccs
 *sccs    1.8	93/08/19 13:40:42 randy
 *sccs	added mask object identifier comparison
 *sccs
 *sccs    1.7	93/04/26 18:18:20 randy
 *sccs	microsoft nit fix in ubzero prototype
 *sccs
 *sccs    1.6	93/04/15 22:04:38 randy
 *sccs	support compilation with g++
 *sccs
 *sccs    1.5	93/04/08 17:28:48 randy
 *sccs	better use of READONLY construct
 *sccs
 *sccs    1.4	92/11/12 16:33:48 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.3	92/10/23 15:28:55 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.2	92/09/11 19:16:12 randy
 *sccs	flatten include dependencies
 *sccs
 *sccs    1.1	92/08/31 14:16:23 randy
 *sccs	date and time created 92/08/31 14:16:23 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_ODEPUBH				/* avoid re-inclusion	*/
#define AME_ODEPUBH				/* prevent re-inclusion */

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
 *	odepub.h  -  function prototypes for routines defined in the ODE
 *	library that are used by agent code.  These are NOT intended for
 *	use by sub-agent developers.  These are private interfaces used
 *	to reduce runtime code size.
 */
#include "ame/machtype.h"

#include "ame/stdtypes.h"
#include "ame/smitypes.h"
#include "ame/socket.h"
#include "ame/appladdr.h"


/*
 * A convenient macro, returns TRUE (1) if left is lexicographically less 
 * than right, otherwise FALSE (0)
 */
#define objid_betterThan(left, right) \
	((right == NULL)? 1 : ((objidcmp(left, right) < 0) ? 1 : 0))

#ifndef USE_PROTOTYPES

/*
 *	create_oid	- add an oid to the table
 */
int		create_oid();

/*
 *	create_oidtbl	- create a new table organized by object identifier
 */
Void	*create_oidtbl();

/*
 *	delete_oid	- remove an oid from the table
 */
int		delete_oid();

/*
 *	delete_oidtbl	- get rid of a table and its contents
 */
int	delete_oidtbl();

/*
 *	explode_oid - turn an object identifier into a list of integers
 */
int	explode_oid();

/*
 *	implode_oid  - transform a list of integers into an object identifier
 */
int	implode_oid();

/*
 *	isDisplayString - check string within rfc 854's constraints
 */
int	isDisplayString();

#ifdef SNMPV2_SUPPORTED
/*
 *	masked_objidcmp - compare two object identifiers under mask
 */
int	masked_objidcmp();
#endif

/*
 *	objidcmp - compare two object identifiers
 */
int	objidcmp();

/*
 *	oct_free - release the data pointed to by an octet string type
 */
void	oct_free();

/*
 *	oct_clone - clone an octet string structure
 */
int	oct_clone();

/*
 *	octcmp - compare two octet string structures
 */
int	octcmp();

/*
 *	open_smux - open an smux connection
 */
TcpSocket_type	open_smux();

/*
 *	parent_oid	- find something's non-vacuous parent arc
 */
Void		*parent_oid();

/*
 *	succ_oid - find the successor to an oid in a table
 */
Void		*succ_oid();

/*
 *	succ_oid - find the successor to an oid in a table
 */
Void		*new_succ_oid();

/*
 *	phase1_succ_oid - find the successor to an oid in a table
 */
Void		*phase1_succ_oid();

/*
 *	phase2_succ_oid - find the successor to an oid in a table
 */
Void		*phase2_succ_oid();

/*
 *	tack_on_int  - tack on a single integer element to an object id
 */
void	tack_on_int();

/*
 *	ubcmp - compare two unsigned byte strings
 */
int	ubcmp();

/*
 *	ubcopy - copy unsigned byte strings
 */
void	ubcopy();

/*
 *	ubzero - zero out an area of memory
 */
void	ubzero();

#else


/*
 *	create_oid	- add an oid to the table
 */
int		create_oid(Void *table, READONLY ObjId_t *objid, Void *info);

/*
 *	create_oidtbl	- create a new table organized by object identifier
 */
Void	*create_oidtbl(int (*user_deletion_function) (Void *));

/*
 *	delete_oid	- remove an oid from the table
 */
int		delete_oid(Void *table, READONLY ObjId_t *objid);

/*
 *	delete_oidtbl	- get rid of a table and its contents
 */
int	delete_oidtbl(Void *table);

/*
 *	explode_oid - turn an object identifier into a list of integers
 */
int	explode_oid(READONLY ObjId_t	*src,
		    Counter_t		*dest,
		    unsigned int	dest_max_elem,
		    unsigned int	*dest_cnt_ptr);

/*
 *	implode_oid  - transform a list of integers into an object identifier
 */
int	implode_oid(ObjId_t		*dest,
		    Len_t		max_len,
		    READONLY Counter_t	*src,
		    unsigned int	src_cnt);

/*
 *	isDisplayString - check string within rfc 854's constraints
 */
int	isDisplayString(Octets_t *s);

#ifdef SNMPV2_SUPPORTED
/*
 *	masked_objidcmp - compare two object identifiers under a mask
 */
int	masked_objidcmp(ObjId_t		*READONLY template_string,
			Octets_t	*READONLY mask,
			ObjId_t		*READONLY target);
#endif

/*
 *	objidcmp - compare two object identifiers
 */
int	objidcmp(READONLY ObjId_t *left, READONLY ObjId_t *right);

/*
 *	oct_free - release the data pointed to by an octet string type
 */
void	oct_free(Octets_t *ptr);

/*
 *	oct_clone - clone an octet string structure
 */
int	oct_clone(Octets_t *dest, READONLY Octets_t *src);

/*
 *	octcmp - compare two octet string structures
 */
int	octcmp(READONLY Octets_t *left, READONLY Octets_t *right);

/*
 *	open_smux - open an smux connection
 */
TcpSocket_type	open_smux(int				server,
			  struct application_addr	*host,
			  void_function			idle);

/*
 *	parent_oid	- find something's non-vacuous parent arc
 */
Void		*parent_oid(READONLY Void *table,  READONLY ObjId_t *objid);

/*
 *	succ_oid - find the successor to an oid in a table
 */
Void		*succ_oid(READONLY Void *table, READONLY ObjId_t *objid);

/*
 *	new_succ_oid - find the successor to an oid in a table
 */
Void		*new_succ_oid(READONLY Void *table, READONLY ObjId_t *objid);

/*
 *	phase1_succ_oid - find the successor to an oid in a table
 */
Void		*phase1_succ_oid(READONLY Void *table, READONLY ObjId_t *objid);

/*
 *	phase2_succ_oid - find the successor to an oid in a table
 */
Void		*phase2_succ_oid(READONLY Void *table, READONLY ObjId_t *objid, int bNoParent);

/*
 *	tack_on_int  - tack on a single integer element to an object id
 */
void	tack_on_int(ObjId_t *obj, READONLY Counter_t val);

/*
 *	ubcmp - compare two unsigned byte strings
 */
int	ubcmp(ubyte	*x,
	      ubyte	*y,
	      Len_t	l);

/*
 *	ubcopy - copy unsigned byte strings
 */
void	ubcopy(READONLY ubyte *source, ubyte *dest, Len_t len);

/*
 *	ubzero - zero out an area of memory
 */
void	ubzero(Void		*dest,
	       int		len);

#endif
#endif
