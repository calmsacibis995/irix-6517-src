/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module classdef.h, release 1.15 dated 96/03/20
 *sccs retrieved from /home/peer+bmc/ame/common/ode/include/SCCS/s.classdef.h
 *sccs
 *sccs    1.15	96/03/20 14:24:27 santosh
 *sccs	New fields get_access and layout_version added to structs (PR#596)
 *sccs
 *sccs    1.14	96/02/12 12:56:45 randy
 *sccs	update company address (PR#613)
 *sccs
 *sccs    1.13	95/05/05 17:01:01 randy
 *sccs	provided additional prototypes to placate strict compilers (PR#336)
 *sccs
 *sccs    1.12	95/01/24 11:57:22 randy
 *sccs	update company address (PR#183)
 *sccs
 *sccs    1.11	94/06/09 15:37:38 randy
 *sccs	added #defines for implied and anchored index entries
 *sccs
 *sccs    1.10	94/04/15 17:12:56 randy
 *sccs	added hooks for consistency functions
 *sccs
 *sccs    1.9	94/03/18 17:05:12 randy
 *sccs	cleaned up use of READONLY for latest G++
 *sccs
 *sccs    1.8	93/10/19 10:55:41 randy
 *sccs	change prototype handling to allow more readable compiler output
 *sccs
 *sccs    1.7	93/08/19 14:10:08 randy
 *sccs	preliminary support for snmpv2
 *sccs
 *sccs    1.6	92/11/12 16:32:58 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.5	92/10/26 22:46:47 randy
 *sccs	romability improvements
 *sccs
 *sccs    1.4	92/10/23 15:20:11 randy
 *sccs	final cosmetics for release 1.5
 *sccs
 *sccs    1.3	92/09/11 19:04:25 randy
 *sccs	alignment of prototypes
 *sccs
 *sccs    1.2	91/09/19 17:39:27 randy
 *sccs	support for snmp traps
 *sccs
 *sccs    1.1	91/09/07 16:27:38 timon
 *sccs	date and time created 91/09/07 16:27:38 by timon
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef CLASS_DEF_H	
#define CLASS_DEF_H 1

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
 *	classdef.h - definitions for the data structures output by the
 *	PEER Networks MIB compiler.  User code should never have to look
 *	inside these structures.  
 *
 ************************************************************************/

#include "ame/machtype.h"
#include "ame/stdtypes.h"

/*
 *	To make prototypes a bit more readable, the following definitions
 *	are helpful.  They help us avoid massive ifdefs in the compiler
 *	generated code.
 */
#ifdef USE_PROTOTYPES
#ifndef PEER_PROTO_ARG
#define PEER_PROTO_ARG(X)	X
#endif
#else
#ifndef PEER_PROTO_ARG
#define PEER_PROTO_ARG(X)	()
#endif
#endif


/*
 *	define a structure to store a the asn1 string with its length
 */
struct object_id
{
	int		len;		/* string length		*/
	ubyte READONLY	*ptr;		/* -> to asn1 string		*/
};

/*
 * The values which the get_access field in the attribute structure  
 * can have. This determines whether GET, GET-NEXT operations can use 
 * the get_func to access the attribute value. 
 */
#define GET_ACCESS_NOT_ALLOWED		0
#define GET_ACCESS_ALLOWED		1

/*
 *	define a structure that will be used to store attributes
 *	and related set, get & match functions
 *
 *		id:		object id of attribute
 *		get_func	retrieve value of attribute
 *		set_func	set value of attribute
 *		setability_func check if the set_func would be
 *					successful with a certain value
 *		next_func	determine "successor"
 *		locator		locate a given instance
 *		index_list	list of distinguishing attributes
 *		kind		basic type
 *		min_pat		lower bounds for built-in range checks
 *		max_pat		upper bounds for built-in range checks
 *		get_access      whether get_func allowed for GET/GET-NEXT
 */
struct attribute
{
	struct object_id READONLY	*id;		/* -> to asn1 string  */
	int				(*get_func)	/* -> to get function */
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
	int				(*set_func)	/* -> to set function */
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
	int			(*setability_func)	/* -> to set test func*/
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
	int			(*consistency_func)	/* -> to set test func*/
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
	int				(*next_func)	/* -> user next func  */
		PEER_PROTO_ARG((Void *ctxt, Void **indices));
	Void				*(*locator)	/* -> instance locator*/
		PEER_PROTO_ARG((Void *ctxt, Void **indices, int op));

	/*
	 *	The index_list element points to a structure describing
	 *	the attributes used to name an instance of this attribute.
	 *	For a non-indexed attribute, this field is null.
	 */
	struct index_entry READONLY *index_list;

	int			kind;			/* type constraints   */
	unsigned long		min_pat;		/* min len or value   */
	unsigned long		max_pat;		/* max len or value   */
	int			get_access;		/* 
							 * mgmt get/get-next 
							 * allowed ?  
							 */
};	

/*
 *	The index_entry structure identifies an attributes used to name
 *	an instance of something.  The index component points to 
 *	an attribute description.  The implied flag indicates whether the
 *	the special SNMPv2 rules for encoding and decoding indices are needed.
 *	The anchored flag indicates an index whose values have been supplied
 *	at registration time, rather than on the fly.
 */
#define IS_IMPLIED	0x01
#define IS_ANCHORED	0x02

struct index_entry
{
	int			special_flags;	/* special SNMPv2 rules? */
	struct attribute READONLY *index;
};

/*
 *	the group attribute structure
 */
struct group_attribute
{
	struct object_id READONLY	*id;	/*-> to group identifier  */

	/*
	 *	the list element points to a null-terminated list of the
	 *	addresses of the attributes beloning to this group
	 */
	struct attribute READONLY **list;
};

/*
 *	define the action structure
 */
struct action
{
	struct object_id READONLY	*id;
	unsigned char		(*func)		/* action function */
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
};

#define CURRENT_LAYOUT_VERSION		1

/*
 *	define the notification structure
 */
struct notification
{
	struct object_id READONLY	*id;
	int				layout_version;
	unsigned char		(*notif_func)	/* -> to notif encoding func */
		PEER_PROTO_ARG((Void *ctxt, Void **indices));
	struct attribute READONLY **attributes;
	int			generic_trap;
	int			specific_trap;
};

/*
 *	define the contained object structure
 */
struct contained_obj
{
	struct class_definition READONLY	*child_object;
	int					(*create_func)
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
};

/*
 *	define a structure for a class
 */

#define CMIP_CLASS	1
#define SNMP_CLASS	2

struct class_definition
{
	int			type;		/* CMIP or SNMP class? */

	/*
	 * Object Identifier for this class of object
	 */
	struct object_id READONLY *class_id;

	/*
	 * The current structural layout version. 
	 */
	int			  layout_version;

	/*
	 * list of attributes for this object
	 */
	struct attribute READONLY **attributes;

	/*
	 * list of group attributes supported by this object
	 */
	struct group_attribute READONLY **groups;

	/*
	 * list of legal actions that this object supports
	 */
	struct action		READONLY **actions;

	/*
	 * list of notifications that may be emitted by this object
	 */
	struct notification	READONLY **notifications;

	/*
	 * access authorization proc.
	 */
	int			(*access_proc)
		PEER_PROTO_ARG((Void *ctxt, Void **indices, int op));

	/*
	 * list of allomorphic classes this object supports
	 */
	struct class_definition READONLY **allomorphs;

	/*
	 * list of distinguishing attributes 
	 */
	struct index_entry	READONLY *rdn;

	/*
	 * list of legal contained objects that this task supports
	 */
	struct contained_obj	READONLY **contained_objects;

	/*
	 * create_callback() function to be called if the object
	 * specified in the M_CREATE is not specfied in the 
	 * contained_objects list for this object.
	 */
	int			(*unknown_contained_obj_func)
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));

	/*
	 * delete_callback() function to be called to delete this object
	 */
	int			(*delete_func)
		PEER_PROTO_ARG((Void *ctxt, Void **indices, Void *value));
};
#endif	/* CLASS_DEF_H */
