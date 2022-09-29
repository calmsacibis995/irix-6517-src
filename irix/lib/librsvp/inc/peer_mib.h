
/*
 *	Standard Header Files
 */
#include "ame/syspub.h"
#include "ame/odepub.h"
#include "ame/snmperrs.h"
#include "ode/classdef.h"
#include "ode/mgmtpub.h"
#include "ame/smitypes.h"
#include "ame/snmpvbl.h"

/*
 *	Run-Time Library constant data structures
 */
extern READONLY struct index_entry	IX_dotZero[];



/************************************************************************
 *
 *	ifIndex - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	A unique value, greater than zero, for each
 *	interface.  It is recommended that values are assigned
 *	contiguously starting from 1.  The value for each
 *	interface sub-layer must remain constant at least from
 *	one re-initialization of the entity's network
 *	management system to the next re-initialization.
 *
 ************************************************************************/

/*
 *	ifIndex is known as 1.3.6.1.2.1.2.2.1.1
 */
static READONLY ubyte	AIA_ifIndex[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 
};

static READONLY struct object_id	AI_ifIndex =
{	sizeof(AIA_ifIndex),
	AIA_ifIndex,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_ifIndex(void *ctxt, void **indices, void *attr_ref)
#else
GET_ifIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = 1;
	return(SNMP_ERR_NO_ERROR);
}

extern READONLY struct index_entry	IX_ifTable_ifEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_ifIndex =
{
	&AI_ifIndex,
	GET_ifIndex,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_ifTable_ifEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};

#define A_ifTable_ifIndex A_ifIndex


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_ifTable_ifEntry[] =
{
	{ 0 + 0,  &A_ifTable_ifIndex	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_ifEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 
};

static READONLY struct object_id	GI_ifEntry =
{
	8,
	GIA_ifEntry
};



/************************************************************************
 *
 *	intSrvIfAttribAllocatedBits - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of bits/second currently  allocated
 *	to reserved sessions on the interface.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribAllocatedBits is known as 1.3.6.1.2.1.52.1.1.1.1
 */
static READONLY ubyte	AIA_intSrvIfAttribAllocatedBits[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x01, 
};

static READONLY struct object_id	AI_intSrvIfAttribAllocatedBits =
{	sizeof(AIA_intSrvIfAttribAllocatedBits),
	AIA_intSrvIfAttribAllocatedBits,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribAllocatedBits(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribAllocatedBits(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribAllocatedBits;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif
extern READONLY struct index_entry	IX_intSrvIfAttribTable_intSrvIfAttribEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribAllocatedBits =
{
	&AI_intSrvIfAttribAllocatedBits,
	GET_intSrvIfAttribAllocatedBits,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvIfAttribAllocatedBits	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvIfAttribAllocatedBits,
#define MAX_intSrvIfAttribAllocatedBits	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvIfAttribAllocatedBits,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvIfAttribMaxAllocatedBits - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The maximum number of bits/second that may  be
 *	allocated  to  reserved  sessions on the inter-
 *	face.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribMaxAllocatedBits is known as 1.3.6.1.2.1.52.1.1.1.2
 */
static READONLY ubyte	AIA_intSrvIfAttribMaxAllocatedBits[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x02, 
};

static READONLY struct object_id	AI_intSrvIfAttribMaxAllocatedBits =
{	sizeof(AIA_intSrvIfAttribMaxAllocatedBits),
	AIA_intSrvIfAttribMaxAllocatedBits,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribMaxAllocatedBits(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribMaxAllocatedBits(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribMaxAllocatedBits;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvIfAttribMaxAllocatedBits(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvIfAttribMaxAllocatedBits(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribMaxAllocatedBits = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_intSrvIfAttribMaxAllocatedBits(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_intSrvIfAttribMaxAllocatedBits();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribMaxAllocatedBits =
{
	&AI_intSrvIfAttribMaxAllocatedBits,
	GET_intSrvIfAttribMaxAllocatedBits,
	SET_intSrvIfAttribMaxAllocatedBits,
	peer_test_intSrvIfAttribMaxAllocatedBits,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvIfAttribMaxAllocatedBits	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvIfAttribMaxAllocatedBits,
#define MAX_intSrvIfAttribMaxAllocatedBits	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvIfAttribMaxAllocatedBits,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvIfAttribAllocatedBuffer - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The amount of buffer space  required  to  hold
 *	the simultaneous burst of all reserved flows on
 *	the interface.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribAllocatedBuffer is known as 1.3.6.1.2.1.52.1.1.1.3
 */
static READONLY ubyte	AIA_intSrvIfAttribAllocatedBuffer[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x03, 
};

static READONLY struct object_id	AI_intSrvIfAttribAllocatedBuffer =
{	sizeof(AIA_intSrvIfAttribAllocatedBuffer),
	AIA_intSrvIfAttribAllocatedBuffer,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribAllocatedBuffer(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribAllocatedBuffer(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribAllocatedBuffer;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribAllocatedBuffer =
{
	&AI_intSrvIfAttribAllocatedBuffer,
	GET_intSrvIfAttribAllocatedBuffer,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvIfAttribAllocatedBuffer	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvIfAttribAllocatedBuffer,
#define MAX_intSrvIfAttribAllocatedBuffer	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvIfAttribAllocatedBuffer,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvIfAttribFlows - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of reserved flows currently  active
 *	on  this  interface.  A flow can be created ei-
 *	ther from a reservation protocol (such as  RSVP
 *	or ST-II) or via configuration information.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribFlows is known as 1.3.6.1.2.1.52.1.1.1.4
 */
static READONLY ubyte	AIA_intSrvIfAttribFlows[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x04, 
};

static READONLY struct object_id	AI_intSrvIfAttribFlows =
{	sizeof(AIA_intSrvIfAttribFlows),
	AIA_intSrvIfAttribFlows,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribFlows(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribFlows(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribFlows;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribFlows =
{
	&AI_intSrvIfAttribFlows,
	GET_intSrvIfAttribFlows,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvIfAttribPropagationDelay - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The amount of propagation delay that this  in-
 *	terface  introduces  in addition to that intro-
 *	diced by bit propagation delays.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribPropagationDelay is known as 1.3.6.1.2.1.52.1.1.1.5
 */
static READONLY ubyte	AIA_intSrvIfAttribPropagationDelay[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x05, 
};

static READONLY struct object_id	AI_intSrvIfAttribPropagationDelay =
{	sizeof(AIA_intSrvIfAttribPropagationDelay),
	AIA_intSrvIfAttribPropagationDelay,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribPropagationDelay(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribPropagationDelay(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribPropagationDelay;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvIfAttribPropagationDelay(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvIfAttribPropagationDelay(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribPropagationDelay = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribPropagationDelay =
{
	&AI_intSrvIfAttribPropagationDelay,
	GET_intSrvIfAttribPropagationDelay,
	SET_intSrvIfAttribPropagationDelay,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvIfAttribStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' on interfaces that are configured for
 *	RSVP.
 *
 ************************************************************************/

/*
 *	intSrvIfAttribStatus is known as 1.3.6.1.2.1.52.1.1.1.6
 */
static READONLY ubyte	AIA_intSrvIfAttribStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 0x06, 
};

static READONLY struct object_id	AI_intSrvIfAttribStatus =
{	sizeof(AIA_intSrvIfAttribStatus),
	AIA_intSrvIfAttribStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvIfAttribStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvIfAttribStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvIfAttribStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvIfAttribStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvIfAttribEntry_t *)ctxt)->intSrvIfAttribStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvIfAttribEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvIfAttribEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvIfAttribEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvIfAttribEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvIfAttribStatus =
{
	&AI_intSrvIfAttribStatus,
	GET_intSrvIfAttribStatus,
	SET_intSrvIfAttribStatus,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvIfAttribEntry,
	peer_locate_intSrvIfAttribEntry,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvIfAttribStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvIfAttribStatus,
#define MAX_intSrvIfAttribStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_intSrvIfAttribStatus,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_intSrvIfAttribEntry[] =
{
	&A_intSrvIfAttribAllocatedBits,
	&A_intSrvIfAttribMaxAllocatedBits,
	&A_intSrvIfAttribAllocatedBuffer,
	&A_intSrvIfAttribFlows,
	&A_intSrvIfAttribPropagationDelay,
	&A_intSrvIfAttribStatus,
	(struct attribute READONLY *) NULL
};

#define A_intSrvIfAttribTable_ifIndex A_ifIndex


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_intSrvIfAttribTable_intSrvIfAttribEntry[] =
{
	{ 0 + 0,  &A_intSrvIfAttribTable_ifIndex	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_intSrvIfAttribEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 0x01, 
};

static READONLY struct object_id	GI_intSrvIfAttribEntry =
{
	9,
	GIA_intSrvIfAttribEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_intSrvIfAttribEntry;
#endif
READONLY struct class_definition	SMI_GROUP_intSrvIfAttribEntry =
{
	SNMP_CLASS,
	&GI_intSrvIfAttribEntry,
	1,
	AS_intSrvIfAttribEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_intSrvIfAttribTable_intSrvIfAttribEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_intSrvIfAttribEntry = 
{
	&SMI_GROUP_intSrvIfAttribEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_intSrvIfAttribTable[] =
{
	&CG_intSrvIfAttribEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_intSrvIfAttribTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_intSrvIfAttribTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x01, 
};

static READONLY struct object_id	GI_intSrvIfAttribTable =
{
	8,
	GIA_intSrvIfAttribTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_intSrvIfAttribTable;
#endif
READONLY struct class_definition	SMI_GROUP_intSrvIfAttribTable =
{
	SNMP_CLASS,
	&GI_intSrvIfAttribTable,
	1,
	AS_intSrvIfAttribTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_intSrvIfAttribTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_intSrvIfAttribTable = 
{
	&SMI_GROUP_intSrvIfAttribTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	intSrvFlowNumber - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of this flow.  This is for SNMP In-
 *	dexing purposes only and has no relation to any
 *	protocol value.
 *
 ************************************************************************/

/*
 *	intSrvFlowNumber is known as 1.3.6.1.2.1.52.1.2.1.1
 */
static READONLY ubyte	AIA_intSrvFlowNumber[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x01, 
};

static READONLY struct object_id	AI_intSrvFlowNumber =
{	sizeof(AIA_intSrvFlowNumber),
	AIA_intSrvFlowNumber,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowNumber(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowNumber(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowNumber;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
extern READONLY struct index_entry	IX_intSrvFlowTable_intSrvFlowEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowNumber =
{
	&AI_intSrvFlowNumber,
	GET_intSrvFlowNumber,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowNumber	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowNumber,
#define MAX_intSrvFlowNumber	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvFlowNumber,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowType - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The type of session (IP4, IP6, IP6  with  flow
 *	information, etc).
 *
 ************************************************************************/

/*
 *	intSrvFlowType is known as 1.3.6.1.2.1.52.1.2.1.2
 */
static READONLY ubyte	AIA_intSrvFlowType[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x02, 
};

static READONLY struct object_id	AI_intSrvFlowType =
{	sizeof(AIA_intSrvFlowType),
	AIA_intSrvFlowType,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowType(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowType;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowType(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowType = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowType =
{
	&AI_intSrvFlowType,
	GET_intSrvFlowType,
	SET_intSrvFlowType,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowType	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowType,
#define MAX_intSrvFlowType	PEER_PORTABLE_ULONG(0xff)
	MAX_intSrvFlowType,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowOwner - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The process that installed this  flow  in  the
 *	queue policy database.
 *
 ************************************************************************/

/*
 *	intSrvFlowOwner is known as 1.3.6.1.2.1.52.1.2.1.3
 */
static READONLY ubyte	AIA_intSrvFlowOwner[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x03, 
};

static READONLY struct object_id	AI_intSrvFlowOwner =
{	sizeof(AIA_intSrvFlowOwner),
	AIA_intSrvFlowOwner,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowOwner(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowOwner(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowOwner;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowOwner(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowOwner(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowOwner = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowOwner =
{
	&AI_intSrvFlowOwner,
	GET_intSrvFlowOwner,
	SET_intSrvFlowOwner,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowOwner	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowOwner,
#define MAX_intSrvFlowOwner	PEER_PORTABLE_ULONG(0x3)
	MAX_intSrvFlowOwner,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowDestAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The destination address used by all senders in
 *	this  session.   This object may not be changed
 *	when the value of the RowStatus object is  'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	intSrvFlowDestAddr is known as 1.3.6.1.2.1.52.1.2.1.4
 */
static READONLY ubyte	AIA_intSrvFlowDestAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x04, 
};

static READONLY struct object_id	AI_intSrvFlowDestAddr =
{	sizeof(AIA_intSrvFlowDestAddr),
	AIA_intSrvFlowDestAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestAddr.len = attr->len;
	ubcopy(attr->val,
		((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowDestAddr =
{
	&AI_intSrvFlowDestAddr,
	GET_intSrvFlowDestAddr,
	SET_intSrvFlowDestAddr,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_intSrvFlowDestAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_intSrvFlowDestAddr,
#define MAX_intSrvFlowDestAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_intSrvFlowDestAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowSenderAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The source address of the sender  selected  by
 *	this  reservation.  The value of all zeroes in-
 *	dicates 'all senders'.  This object may not  be
 *	changed  when the value of the RowStatus object
 *	is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowSenderAddr is known as 1.3.6.1.2.1.52.1.2.1.5
 */
static READONLY ubyte	AIA_intSrvFlowSenderAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x05, 
};

static READONLY struct object_id	AI_intSrvFlowSenderAddr =
{	sizeof(AIA_intSrvFlowSenderAddr),
	AIA_intSrvFlowSenderAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowSenderAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowSenderAddr.len = attr->len;
	ubcopy(attr->val,
		((intSrvFlowEntry_t *)ctxt)->intSrvFlowSenderAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowSenderAddr =
{
	&AI_intSrvFlowSenderAddr,
	GET_intSrvFlowSenderAddr,
	SET_intSrvFlowSenderAddr,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_intSrvFlowSenderAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_intSrvFlowSenderAddr,
#define MAX_intSrvFlowSenderAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_intSrvFlowSenderAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowDestAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the destination address in bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowDestAddrLength is known as 1.3.6.1.2.1.52.1.2.1.6
 */
static READONLY ubyte	AIA_intSrvFlowDestAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x06, 
};

static READONLY struct object_id	AI_intSrvFlowDestAddrLength =
{	sizeof(AIA_intSrvFlowDestAddrLength),
	AIA_intSrvFlowDestAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowDestAddrLength =
{
	&AI_intSrvFlowDestAddrLength,
	GET_intSrvFlowDestAddrLength,
	SET_intSrvFlowDestAddrLength,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowDestAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowDestAddrLength,
#define MAX_intSrvFlowDestAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_intSrvFlowDestAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowSenderAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the sender's  address  in  bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowSenderAddrLength is known as 1.3.6.1.2.1.52.1.2.1.7
 */
static READONLY ubyte	AIA_intSrvFlowSenderAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x07, 
};

static READONLY struct object_id	AI_intSrvFlowSenderAddrLength =
{	sizeof(AIA_intSrvFlowSenderAddrLength),
	AIA_intSrvFlowSenderAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowSenderAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowSenderAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowSenderAddrLength =
{
	&AI_intSrvFlowSenderAddrLength,
	GET_intSrvFlowSenderAddrLength,
	SET_intSrvFlowSenderAddrLength,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowSenderAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowSenderAddrLength,
#define MAX_intSrvFlowSenderAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_intSrvFlowSenderAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Protocol used by a session.   This  ob-
 *	ject  may  not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowProtocol is known as 1.3.6.1.2.1.52.1.2.1.8
 */
static READONLY ubyte	AIA_intSrvFlowProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x08, 
};

static READONLY struct object_id	AI_intSrvFlowProtocol =
{	sizeof(AIA_intSrvFlowProtocol),
	AIA_intSrvFlowProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowProtocol;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowProtocol(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowProtocol = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowProtocol =
{
	&AI_intSrvFlowProtocol,
	GET_intSrvFlowProtocol,
	SET_intSrvFlowProtocol,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowProtocol,
#define MAX_intSrvFlowProtocol	PEER_PORTABLE_ULONG(0xff)
	MAX_intSrvFlowProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowDestPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used as a  destina-
 *	tion  port for all senders in this session.  If
 *	the  IP   protocol   in   use,   specified   by
 *	intSrvResvFwdProtocol,  is 50 (ESP) or 51 (AH),
 *	this  represents  a  virtual  destination  port
 *	number.   A value of zero indicates that the IP
 *	protocol in use does not have ports.  This  ob-
 *	ject  may  not be changed when the value of the
 *	
 *	
 *	
 *	
 *	
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowDestPort is known as 1.3.6.1.2.1.52.1.2.1.9
 */
static READONLY ubyte	AIA_intSrvFlowDestPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x09, 
};

static READONLY struct object_id	AI_intSrvFlowDestPort =
{	sizeof(AIA_intSrvFlowDestPort),
	AIA_intSrvFlowDestPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowDestPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowDestPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestPort.len = attr->len;
	ubcopy(attr->val,
		((intSrvFlowEntry_t *)ctxt)->intSrvFlowDestPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowDestPort =
{
	&AI_intSrvFlowDestPort,
	GET_intSrvFlowDestPort,
	SET_intSrvFlowDestPort,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_intSrvFlowDestPort	PEER_PORTABLE_ULONG(0x2)
	MIN_intSrvFlowDestPort,
#define MAX_intSrvFlowDestPort	PEER_PORTABLE_ULONG(0x4)
	MAX_intSrvFlowDestPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used  as  a  source
 *	port  for  this sender in this session.  If the
 *	IP    protocol    in    use,    specified    by
 *	intSrvResvFwdProtocol  is  50 (ESP) or 51 (AH),
 *	this represents a generalized  port  identifier
 *	(GPI).   A  value of zero indicates that the IP
 *	protocol in use does not have ports.  This  ob-
 *	ject  may  not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	intSrvFlowPort is known as 1.3.6.1.2.1.52.1.2.1.10
 */
static READONLY ubyte	AIA_intSrvFlowPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0a, 
};

static READONLY struct object_id	AI_intSrvFlowPort =
{	sizeof(AIA_intSrvFlowPort),
	AIA_intSrvFlowPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowPort.len = attr->len;
	ubcopy(attr->val,
		((intSrvFlowEntry_t *)ctxt)->intSrvFlowPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowPort =
{
	&AI_intSrvFlowPort,
	GET_intSrvFlowPort,
	SET_intSrvFlowPort,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_intSrvFlowPort	PEER_PORTABLE_ULONG(0x2)
	MIN_intSrvFlowPort,
#define MAX_intSrvFlowPort	PEER_PORTABLE_ULONG(0x4)
	MAX_intSrvFlowPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowFlowId - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The flow ID that  this  sender  is  using,  if
 *	this  is  an IPv6 session.
 *
 ************************************************************************/

/*
 *	intSrvFlowFlowId is known as 1.3.6.1.2.1.52.1.2.1.11
 */
static READONLY ubyte	AIA_intSrvFlowFlowId[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0b, 
};

static READONLY struct object_id	AI_intSrvFlowFlowId =
{	sizeof(AIA_intSrvFlowFlowId),
	AIA_intSrvFlowFlowId,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowFlowId(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowFlowId(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowId;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowFlowId =
{
	&AI_intSrvFlowFlowId,
	GET_intSrvFlowFlowId,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowFlowId	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowFlowId,
#define MAX_intSrvFlowFlowId	PEER_PORTABLE_ULONG(0xffffff)
	MAX_intSrvFlowFlowId,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowInterface - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The ifIndex value of the  interface  on  which
 *	this reservation exists.
 *
 ************************************************************************/

/*
 *	intSrvFlowInterface is known as 1.3.6.1.2.1.52.1.2.1.12
 */
static READONLY ubyte	AIA_intSrvFlowInterface[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0c, 
};

static READONLY struct object_id	AI_intSrvFlowInterface =
{	sizeof(AIA_intSrvFlowInterface),
	AIA_intSrvFlowInterface,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowInterface(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowInterface;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowInterface(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowInterface = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowInterface =
{
	&AI_intSrvFlowInterface,
	GET_intSrvFlowInterface,
	SET_intSrvFlowInterface,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowIfAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Address on the ifEntry  on  which  this
 *	reservation  exists.  This is present primarily
 *	
 *	
 *	
 *	
 *	
 *	to support those interfaces which layer  multi-
 *	ple IP Addresses on the interface.
 *
 ************************************************************************/

/*
 *	intSrvFlowIfAddr is known as 1.3.6.1.2.1.52.1.2.1.13
 */
static READONLY ubyte	AIA_intSrvFlowIfAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0d, 
};

static READONLY struct object_id	AI_intSrvFlowIfAddr =
{	sizeof(AIA_intSrvFlowIfAddr),
	AIA_intSrvFlowIfAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowIfAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowIfAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowIfAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowIfAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowIfAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowIfAddr.len = attr->len;
	ubcopy(attr->val,
		((intSrvFlowEntry_t *)ctxt)->intSrvFlowIfAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowIfAddr =
{
	&AI_intSrvFlowIfAddr,
	GET_intSrvFlowIfAddr,
	SET_intSrvFlowIfAddr,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_intSrvFlowIfAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_intSrvFlowIfAddr,
#define MAX_intSrvFlowIfAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_intSrvFlowIfAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Reserved Rate of the sender's data stream.
 *	If this is a Controlled Load service flow, this
 *	rate is derived from the Tspec  rate  parameter
 *	(r).   If  this  is  a Guaranteed service flow,
 *	this rate is derived from  the  Rspec  clearing
 *	rate parameter (R).
 *
 ************************************************************************/

/*
 *	intSrvFlowRate is known as 1.3.6.1.2.1.52.1.2.1.14
 */
static READONLY ubyte	AIA_intSrvFlowRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0e, 
};

static READONLY struct object_id	AI_intSrvFlowRate =
{	sizeof(AIA_intSrvFlowRate),
	AIA_intSrvFlowRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowRate =
{
	&AI_intSrvFlowRate,
	GET_intSrvFlowRate,
	SET_intSrvFlowRate,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowRate	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowRate,
#define MAX_intSrvFlowRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvFlowRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowBurst - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The size of the largest  burst  expected  from
 *	the sender at a time.
 *	
 *	If this is less than  the  sender's  advertised
 *	burst  size, the receiver is asking the network
 *	to provide flow pacing  beyond  what  would  be
 *	provided  under normal circumstances. Such pac-
 *	ing is at the network's option.
 *
 ************************************************************************/

/*
 *	intSrvFlowBurst is known as 1.3.6.1.2.1.52.1.2.1.15
 */
static READONLY ubyte	AIA_intSrvFlowBurst[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x0f, 
};

static READONLY struct object_id	AI_intSrvFlowBurst =
{	sizeof(AIA_intSrvFlowBurst),
	AIA_intSrvFlowBurst,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowBurst(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowBurst;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowBurst(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowBurst = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowBurst =
{
	&AI_intSrvFlowBurst,
	GET_intSrvFlowBurst,
	SET_intSrvFlowBurst,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowBurst	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowBurst,
#define MAX_intSrvFlowBurst	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvFlowBurst,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowWeight - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The weight used  to  prioritize  the  traffic.
 *	Note  that the interpretation of this object is
 *	implementation-specific,   as   implementations
 *	vary in their use of weighting procedures.
 *
 ************************************************************************/

/*
 *	intSrvFlowWeight is known as 1.3.6.1.2.1.52.1.2.1.16
 */
static READONLY ubyte	AIA_intSrvFlowWeight[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x10, 
};

static READONLY struct object_id	AI_intSrvFlowWeight =
{	sizeof(AIA_intSrvFlowWeight),
	AIA_intSrvFlowWeight,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowWeight(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowWeight(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowWeight;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowWeight(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowWeight(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowWeight = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowWeight =
{
	&AI_intSrvFlowWeight,
	GET_intSrvFlowWeight,
	SET_intSrvFlowWeight,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowQueue - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of the queue used by this  traffic.
 *	Note  that the interpretation of this object is
 *	implementation-specific,   as   implementations
 *	vary in their use of queue identifiers.
 *
 ************************************************************************/

/*
 *	intSrvFlowQueue is known as 1.3.6.1.2.1.52.1.2.1.17
 */
static READONLY ubyte	AIA_intSrvFlowQueue[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x11, 
};

static READONLY struct object_id	AI_intSrvFlowQueue =
{	sizeof(AIA_intSrvFlowQueue),
	AIA_intSrvFlowQueue,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowQueue(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowQueue(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowQueue;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowQueue(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowQueue(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowQueue = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowQueue =
{
	&AI_intSrvFlowQueue,
	GET_intSrvFlowQueue,
	SET_intSrvFlowQueue,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowMinTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The minimum message size for  this  flow.  The
 *	policing  algorithm will treat smaller messages
 *	as though they are this size.
 *
 ************************************************************************/

/*
 *	intSrvFlowMinTU is known as 1.3.6.1.2.1.52.1.2.1.18
 */
static READONLY ubyte	AIA_intSrvFlowMinTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x12, 
};

static READONLY struct object_id	AI_intSrvFlowMinTU =
{	sizeof(AIA_intSrvFlowMinTU),
	AIA_intSrvFlowMinTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowMinTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowMinTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowMinTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowMinTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowMinTU =
{
	&AI_intSrvFlowMinTU,
	GET_intSrvFlowMinTU,
	SET_intSrvFlowMinTU,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowMinTU	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowMinTU,
#define MAX_intSrvFlowMinTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvFlowMinTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowMaxTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The maximum datagram size for this  flow  that
 *	will conform to the traffic specification. This
 *	value cannot exceed the MTU of the interface.
 *
 ************************************************************************/

/*
 *	intSrvFlowMaxTU is known as 1.3.6.1.2.1.52.1.2.1.19
 */
static READONLY ubyte	AIA_intSrvFlowMaxTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x13, 
};

static READONLY struct object_id	AI_intSrvFlowMaxTU =
{	sizeof(AIA_intSrvFlowMaxTU),
	AIA_intSrvFlowMaxTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowMaxTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowMaxTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowMaxTU =
{
	&AI_intSrvFlowMaxTU,
	GET_intSrvFlowMaxTU,
	SET_intSrvFlowMaxTU,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowMaxTU	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowMaxTU,
#define MAX_intSrvFlowMaxTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_intSrvFlowMaxTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowBestEffort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of packets that  were  remanded  to
 *	best effort service.
 *
 ************************************************************************/

/*
 *	intSrvFlowBestEffort is known as 1.3.6.1.2.1.52.1.2.1.20
 */
static READONLY ubyte	AIA_intSrvFlowBestEffort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x14, 
};

static READONLY struct object_id	AI_intSrvFlowBestEffort =
{	sizeof(AIA_intSrvFlowBestEffort),
	AIA_intSrvFlowBestEffort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowBestEffort(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowBestEffort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Counter32	*attr = (Counter32 *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowBestEffort;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowBestEffort =
{
	&AI_intSrvFlowBestEffort,
	GET_intSrvFlowBestEffort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_CNTER,		/* Counter32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowPoliced - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of packets policed since the incep-
 *	tion of the flow's service.
 *
 ************************************************************************/

/*
 *	intSrvFlowPoliced is known as 1.3.6.1.2.1.52.1.2.1.21
 */
static READONLY ubyte	AIA_intSrvFlowPoliced[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x15, 
};

static READONLY struct object_id	AI_intSrvFlowPoliced =
{	sizeof(AIA_intSrvFlowPoliced),
	AIA_intSrvFlowPoliced,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowPoliced(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowPoliced(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Counter32	*attr = (Counter32 *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowPoliced;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowPoliced =
{
	&AI_intSrvFlowPoliced,
	GET_intSrvFlowPoliced,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_CNTER,		/* Counter32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowDiscard - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If 'true', the flow  is  to  incur  loss  when
 *	traffic is policed.  If 'false', policed traff-
 *	ic is treated as best effort traffic.
 *
 ************************************************************************/

/*
 *	intSrvFlowDiscard is known as 1.3.6.1.2.1.52.1.2.1.22
 */
static READONLY ubyte	AIA_intSrvFlowDiscard[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x16, 
};

static READONLY struct object_id	AI_intSrvFlowDiscard =
{	sizeof(AIA_intSrvFlowDiscard),
	AIA_intSrvFlowDiscard,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowDiscard(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowDiscard(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowDiscard;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowDiscard(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowDiscard(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowDiscard = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowDiscard =
{
	&AI_intSrvFlowDiscard,
	GET_intSrvFlowDiscard,
	SET_intSrvFlowDiscard,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowDiscard	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowDiscard,
#define MAX_intSrvFlowDiscard	PEER_PORTABLE_ULONG(0x2)
	MAX_intSrvFlowDiscard,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowService - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The QoS service being applied to this flow.
 *
 ************************************************************************/

/*
 *	intSrvFlowService is known as 1.3.6.1.2.1.52.1.2.1.23
 */
static READONLY ubyte	AIA_intSrvFlowService[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x17, 
};

static READONLY struct object_id	AI_intSrvFlowService =
{	sizeof(AIA_intSrvFlowService),
	AIA_intSrvFlowService,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowService(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowService(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowService;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowService =
{
	&AI_intSrvFlowService,
	GET_intSrvFlowService,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowService	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowService,
#define MAX_intSrvFlowService	PEER_PORTABLE_ULONG(0x5)
	MAX_intSrvFlowService,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowOrder - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	In the event of ambiguity, the order in  which
 *	the  classifier  should  make  its comparisons.
 *	The row with intSrvFlowOrder=0 is tried  first,
 *	and  comparisons  proceed  in  the order of in-
 *	creasing value.  Non-serial implementations  of
 *	the classifier should emulate this behavior.
 *
 ************************************************************************/

/*
 *	intSrvFlowOrder is known as 1.3.6.1.2.1.52.1.2.1.24
 */
static READONLY ubyte	AIA_intSrvFlowOrder[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x18, 
};

static READONLY struct object_id	AI_intSrvFlowOrder =
{	sizeof(AIA_intSrvFlowOrder),
	AIA_intSrvFlowOrder,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowOrder(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowOrder(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowOrder;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowOrder(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowOrder(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowOrder = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowOrder =
{
	&AI_intSrvFlowOrder,
	GET_intSrvFlowOrder,
	SET_intSrvFlowOrder,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowOrder	PEER_PORTABLE_ULONG(0x0)
	MIN_intSrvFlowOrder,
#define MAX_intSrvFlowOrder	PEER_PORTABLE_ULONG(0xffff)
	MAX_intSrvFlowOrder,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	intSrvFlowStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all active  flows.   This  object
 *	
 *	
 *	
 *	
 *	
 *	may be used to install static classifier infor-
 *	mation, delete classifier information,  or  au-
 *	thorize such.
 *
 ************************************************************************/

/*
 *	intSrvFlowStatus is known as 1.3.6.1.2.1.52.1.2.1.25
 */
static READONLY ubyte	AIA_intSrvFlowStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 0x19, 
};

static READONLY struct object_id	AI_intSrvFlowStatus =
{	sizeof(AIA_intSrvFlowStatus),
	AIA_intSrvFlowStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_intSrvFlowStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_intSrvFlowStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((intSrvFlowEntry_t *)ctxt)->intSrvFlowStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_intSrvFlowStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_intSrvFlowStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((intSrvFlowEntry_t *)ctxt)->intSrvFlowStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_test_generic_fail(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	peer_test_generic_fail();
#endif

#ifdef USE_PROTOTYPES
extern int	peer_next_intSrvFlowEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_intSrvFlowEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_intSrvFlowEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_intSrvFlowEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_intSrvFlowStatus =
{
	&AI_intSrvFlowStatus,
	GET_intSrvFlowStatus,
	SET_intSrvFlowStatus,
	peer_test_generic_fail,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_intSrvFlowEntry,
	peer_locate_intSrvFlowEntry,
	IX_intSrvFlowTable_intSrvFlowEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_intSrvFlowStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_intSrvFlowStatus,
#define MAX_intSrvFlowStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_intSrvFlowStatus,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_intSrvFlowEntry[] =
{
	&A_intSrvFlowNumber,
	&A_intSrvFlowType,
	&A_intSrvFlowOwner,
	&A_intSrvFlowDestAddr,
	&A_intSrvFlowSenderAddr,
	&A_intSrvFlowDestAddrLength,
	&A_intSrvFlowSenderAddrLength,
	&A_intSrvFlowProtocol,
	&A_intSrvFlowDestPort,
	&A_intSrvFlowPort,
	&A_intSrvFlowFlowId,
	&A_intSrvFlowInterface,
	&A_intSrvFlowIfAddr,
	&A_intSrvFlowRate,
	&A_intSrvFlowBurst,
	&A_intSrvFlowWeight,
	&A_intSrvFlowQueue,
	&A_intSrvFlowMinTU,
	&A_intSrvFlowMaxTU,
	&A_intSrvFlowBestEffort,
	&A_intSrvFlowPoliced,
	&A_intSrvFlowDiscard,
	&A_intSrvFlowService,
	&A_intSrvFlowOrder,
	&A_intSrvFlowStatus,
	(struct attribute READONLY *) NULL
};

#define A_intSrvFlowTable_intSrvFlowNumber A_intSrvFlowNumber


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_intSrvFlowTable_intSrvFlowEntry[] =
{
	{ 0 + 0,  &A_intSrvFlowTable_intSrvFlowNumber	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_intSrvFlowEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 0x01, 
};

static READONLY struct object_id	GI_intSrvFlowEntry =
{
	9,
	GIA_intSrvFlowEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_intSrvFlowEntry;
#endif
READONLY struct class_definition	SMI_GROUP_intSrvFlowEntry =
{
	SNMP_CLASS,
	&GI_intSrvFlowEntry,
	1,
	AS_intSrvFlowEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_intSrvFlowTable_intSrvFlowEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_intSrvFlowEntry = 
{
	&SMI_GROUP_intSrvFlowEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_intSrvFlowTable[] =
{
	&CG_intSrvFlowEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_intSrvFlowTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_intSrvFlowTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 0x02, 
};

static READONLY struct object_id	GI_intSrvFlowTable =
{
	8,
	GIA_intSrvFlowTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_intSrvFlowTable;
#endif
READONLY struct class_definition	SMI_GROUP_intSrvFlowTable =
{
	SNMP_CLASS,
	&GI_intSrvFlowTable,
	1,
	AS_intSrvFlowTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_intSrvFlowTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_intSrvFlowTable = 
{
	&SMI_GROUP_intSrvFlowTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_intSrvObjects[] =
{
	&CG_intSrvIfAttribTable,
	&CG_intSrvFlowTable,
	(struct contained_obj READONLY *) NULL
};
#define AS_intSrvObjects (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_intSrvObjects[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x34, 0x01, 
};

static READONLY struct object_id	GI_intSrvObjects =
{
	7,
	GIA_intSrvObjects
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_intSrvObjects;
#endif
READONLY struct class_definition	SMI_GROUP_intSrvObjects =
{
	SNMP_CLASS,
	&GI_intSrvObjects,
	1,
	AS_intSrvObjects,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_intSrvObjects,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};


/************************************************************************
 *
 *	rsvpSessionNumber - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of this session.  This is for  SNMP
 *	Indexing  purposes  only and has no relation to
 *	any protocol value.
 *
 ************************************************************************/

/*
 *	rsvpSessionNumber is known as 1.3.6.1.2.1.51.1.1.1.1
 */
static READONLY ubyte	AIA_rsvpSessionNumber[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpSessionNumber =
{	sizeof(AIA_rsvpSessionNumber),
	AIA_rsvpSessionNumber,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionNumber(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionNumber(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionNumber;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
extern READONLY struct index_entry	IX_rsvpSessionTable_rsvpSessionEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionNumber =
{
	&AI_rsvpSessionNumber,
	GET_rsvpSessionNumber,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSessionNumber	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSessionNumber,
#define MAX_rsvpSessionNumber	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSessionNumber,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionType - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The type of session (IP4, IP6, IP6  with  flow
 *	information, etc).
 *
 ************************************************************************/

/*
 *	rsvpSessionType is known as 1.3.6.1.2.1.51.1.1.1.2
 */
static READONLY ubyte	AIA_rsvpSessionType[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpSessionType =
{	sizeof(AIA_rsvpSessionType),
	AIA_rsvpSessionType,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionType(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionType;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionType =
{
	&AI_rsvpSessionType,
	GET_rsvpSessionType,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSessionType	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSessionType,
#define MAX_rsvpSessionType	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpSessionType,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionDestAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The destination address used by all senders in
 *	this  session.   This object may not be changed
 *	when the value of the RowStatus object is  'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpSessionDestAddr is known as 1.3.6.1.2.1.51.1.1.1.3
 */
static READONLY ubyte	AIA_rsvpSessionDestAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpSessionDestAddr =
{	sizeof(AIA_rsvpSessionDestAddr),
	AIA_rsvpSessionDestAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionDestAddr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionDestAddr =
{
	&AI_rsvpSessionDestAddr,
	GET_rsvpSessionDestAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSessionDestAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpSessionDestAddr,
#define MAX_rsvpSessionDestAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpSessionDestAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionDestAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The CIDR prefix length of the session address,
 *	which  is  32  for  IP4  host and multicast ad-
 *	dresses, and 128 for IP6 addresses.   This  ob-
 *	ject  may  not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSessionDestAddrLength is known as 1.3.6.1.2.1.51.1.1.1.4
 */
static READONLY ubyte	AIA_rsvpSessionDestAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x04, 
};

static READONLY struct object_id	AI_rsvpSessionDestAddrLength =
{	sizeof(AIA_rsvpSessionDestAddrLength),
	AIA_rsvpSessionDestAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionDestAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionDestAddrLength =
{
	&AI_rsvpSessionDestAddrLength,
	GET_rsvpSessionDestAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSessionDestAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSessionDestAddrLength,
#define MAX_rsvpSessionDestAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpSessionDestAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Protocol used by  this  session.   This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSessionProtocol is known as 1.3.6.1.2.1.51.1.1.1.5
 */
static READONLY ubyte	AIA_rsvpSessionProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x05, 
};

static READONLY struct object_id	AI_rsvpSessionProtocol =
{	sizeof(AIA_rsvpSessionProtocol),
	AIA_rsvpSessionProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionProtocol;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionProtocol =
{
	&AI_rsvpSessionProtocol,
	GET_rsvpSessionProtocol,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSessionProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSessionProtocol,
#define MAX_rsvpSessionProtocol	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpSessionProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used as a  destina-
 *	tion  port for all senders in this session.  If
 *	the IP protocol in use, specified  by  rsvpSen-
 *	derProtocol,  is  50  (ESP)  or  51  (AH), this
 *	represents a virtual destination  port  number.
 *	A  value of zero indicates that the IP protocol
 *	in use does not have ports.   This  object  may
 *	not  be changed when the value of the RowStatus
 *	object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSessionPort is known as 1.3.6.1.2.1.51.1.1.1.6
 */
static READONLY ubyte	AIA_rsvpSessionPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x06, 
};

static READONLY struct object_id	AI_rsvpSessionPort =
{	sizeof(AIA_rsvpSessionPort),
	AIA_rsvpSessionPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionPort;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionPort =
{
	&AI_rsvpSessionPort,
	GET_rsvpSessionPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSessionPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpSessionPort,
#define MAX_rsvpSessionPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpSessionPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionSenders - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of distinct senders currently known
 *	to be part of this session.
 *
 ************************************************************************/

/*
 *	rsvpSessionSenders is known as 1.3.6.1.2.1.51.1.1.1.7
 */
static READONLY ubyte	AIA_rsvpSessionSenders[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x07, 
};

static READONLY struct object_id	AI_rsvpSessionSenders =
{	sizeof(AIA_rsvpSessionSenders),
	AIA_rsvpSessionSenders,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionSenders(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionSenders(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionSenders;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionSenders =
{
	&AI_rsvpSessionSenders,
	GET_rsvpSessionSenders,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionReceivers - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of reservations being requested  of
 *	this system for this session.
 *
 ************************************************************************/

/*
 *	rsvpSessionReceivers is known as 1.3.6.1.2.1.51.1.1.1.8
 */
static READONLY ubyte	AIA_rsvpSessionReceivers[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x08, 
};

static READONLY struct object_id	AI_rsvpSessionReceivers =
{	sizeof(AIA_rsvpSessionReceivers),
	AIA_rsvpSessionReceivers,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionReceivers(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionReceivers(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionReceivers;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionReceivers =
{
	&AI_rsvpSessionReceivers,
	GET_rsvpSessionReceivers,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSessionRequests - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of reservation requests this system
 *	is sending upstream for this session.
 *
 ************************************************************************/

/*
 *	rsvpSessionRequests is known as 1.3.6.1.2.1.51.1.1.1.9
 */
static READONLY ubyte	AIA_rsvpSessionRequests[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 0x09, 
};

static READONLY struct object_id	AI_rsvpSessionRequests =
{	sizeof(AIA_rsvpSessionRequests),
	AIA_rsvpSessionRequests,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSessionRequests(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSessionRequests(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpSessionEntry_t *)ctxt)->rsvpSessionRequests;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSessionEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSessionEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSessionEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSessionEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSessionRequests =
{
	&AI_rsvpSessionRequests,
	GET_rsvpSessionRequests,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSessionEntry,
	peer_locate_rsvpSessionEntry,
	IX_rsvpSessionTable_rsvpSessionEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpSessionEntry[] =
{
	&A_rsvpSessionNumber,
	&A_rsvpSessionType,
	&A_rsvpSessionDestAddr,
	&A_rsvpSessionDestAddrLength,
	&A_rsvpSessionProtocol,
	&A_rsvpSessionPort,
	&A_rsvpSessionSenders,
	&A_rsvpSessionReceivers,
	&A_rsvpSessionRequests,
	(struct attribute READONLY *) NULL
};

#define A_rsvpSessionTable_rsvpSessionNumber A_rsvpSessionNumber


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpSessionTable_rsvpSessionEntry[] =
{
	{ 0 + 0,  &A_rsvpSessionTable_rsvpSessionNumber	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpSessionEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 0x01, 
};

static READONLY struct object_id	GI_rsvpSessionEntry =
{
	9,
	GIA_rsvpSessionEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSessionEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSessionEntry =
{
	SNMP_CLASS,
	&GI_rsvpSessionEntry,
	1,
	AS_rsvpSessionEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpSessionTable_rsvpSessionEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSessionEntry = 
{
	&SMI_GROUP_rsvpSessionEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpSessionTable[] =
{
	&CG_rsvpSessionEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpSessionTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpSessionTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x01, 
};

static READONLY struct object_id	GI_rsvpSessionTable =
{
	8,
	GIA_rsvpSessionTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSessionTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSessionTable =
{
	SNMP_CLASS,
	&GI_rsvpSessionTable,
	1,
	AS_rsvpSessionTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpSessionTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSessionTable = 
{
	&SMI_GROUP_rsvpSessionTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpSenderNumber - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of this sender.  This is  for  SNMP
 *	Indexing  purposes  only and has no relation to
 *	any protocol value.
 *
 ************************************************************************/

/*
 *	rsvpSenderNumber is known as 1.3.6.1.2.1.51.1.2.1.1
 */
static READONLY ubyte	AIA_rsvpSenderNumber[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpSenderNumber =
{	sizeof(AIA_rsvpSenderNumber),
	AIA_rsvpSenderNumber,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderNumber(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderNumber(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderNumber;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
extern READONLY struct index_entry	IX_rsvpSenderTable_rsvpSenderEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderNumber =
{
	&AI_rsvpSenderNumber,
	GET_rsvpSenderNumber,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderNumber	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderNumber,
#define MAX_rsvpSenderNumber	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderNumber,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderType - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The type of session (IP4, IP6, IP6  with  flow
 *	information, etc).
 *
 ************************************************************************/

/*
 *	rsvpSenderType is known as 1.3.6.1.2.1.51.1.2.1.2
 */
static READONLY ubyte	AIA_rsvpSenderType[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpSenderType =
{	sizeof(AIA_rsvpSenderType),
	AIA_rsvpSenderType,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderType(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderType;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderType(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderType = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderType =
{
	&AI_rsvpSenderType,
	GET_rsvpSenderType,
	SET_rsvpSenderType,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderType	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderType,
#define MAX_rsvpSenderType	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpSenderType,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderDestAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The destination address used by all senders in
 *	this  session.   This object may not be changed
 *	when the value of the RowStatus object is  'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpSenderDestAddr is known as 1.3.6.1.2.1.51.1.2.1.3
 */
static READONLY ubyte	AIA_rsvpSenderDestAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpSenderDestAddr =
{	sizeof(AIA_rsvpSenderDestAddr),
	AIA_rsvpSenderDestAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderDestAddr =
{
	&AI_rsvpSenderDestAddr,
	GET_rsvpSenderDestAddr,
	SET_rsvpSenderDestAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderDestAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpSenderDestAddr,
#define MAX_rsvpSenderDestAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpSenderDestAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The source address used by this sender in this
 *	session.   This  object may not be changed when
 *	the value of the RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSenderAddr is known as 1.3.6.1.2.1.51.1.2.1.4
 */
static READONLY ubyte	AIA_rsvpSenderAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x04, 
};

static READONLY struct object_id	AI_rsvpSenderAddr =
{	sizeof(AIA_rsvpSenderAddr),
	AIA_rsvpSenderAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAddr =
{
	&AI_rsvpSenderAddr,
	GET_rsvpSenderAddr,
	SET_rsvpSenderAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpSenderAddr,
#define MAX_rsvpSenderAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpSenderAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderDestAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the destination address in bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSenderDestAddrLength is known as 1.3.6.1.2.1.51.1.2.1.5
 */
static READONLY ubyte	AIA_rsvpSenderDestAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x05, 
};

static READONLY struct object_id	AI_rsvpSenderDestAddrLength =
{	sizeof(AIA_rsvpSenderDestAddrLength),
	AIA_rsvpSenderDestAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderDestAddrLength =
{
	&AI_rsvpSenderDestAddrLength,
	GET_rsvpSenderDestAddrLength,
	SET_rsvpSenderDestAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderDestAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderDestAddrLength,
#define MAX_rsvpSenderDestAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpSenderDestAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the sender's  address  in  bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSenderAddrLength is known as 1.3.6.1.2.1.51.1.2.1.6
 */
static READONLY ubyte	AIA_rsvpSenderAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x06, 
};

static READONLY struct object_id	AI_rsvpSenderAddrLength =
{	sizeof(AIA_rsvpSenderAddrLength),
	AIA_rsvpSenderAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAddrLength =
{
	&AI_rsvpSenderAddrLength,
	GET_rsvpSenderAddrLength,
	SET_rsvpSenderAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAddrLength,
#define MAX_rsvpSenderAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpSenderAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Protocol used by  this  session.   This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSenderProtocol is known as 1.3.6.1.2.1.51.1.2.1.7
 */
static READONLY ubyte	AIA_rsvpSenderProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x07, 
};

static READONLY struct object_id	AI_rsvpSenderProtocol =
{	sizeof(AIA_rsvpSenderProtocol),
	AIA_rsvpSenderProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderProtocol;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderProtocol(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderProtocol = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderProtocol =
{
	&AI_rsvpSenderProtocol,
	GET_rsvpSenderProtocol,
	SET_rsvpSenderProtocol,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderProtocol,
#define MAX_rsvpSenderProtocol	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpSenderProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderDestPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used as a  destina-
 *	tion  port for all senders in this session.  If
 *	the IP protocol in use, specified  by  rsvpSen-
 *	derProtocol,  is  50  (ESP)  or  51  (AH), this
 *	represents a virtual destination  port  number.
 *	A  value of zero indicates that the IP protocol
 *	in use does not have ports.   This  object  may
 *	not  be changed when the value of the RowStatus
 *	object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpSenderDestPort is known as 1.3.6.1.2.1.51.1.2.1.8
 */
static READONLY ubyte	AIA_rsvpSenderDestPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x08, 
};

static READONLY struct object_id	AI_rsvpSenderDestPort =
{	sizeof(AIA_rsvpSenderDestPort),
	AIA_rsvpSenderDestPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderDestPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderDestPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestPort.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderDestPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderDestPort =
{
	&AI_rsvpSenderDestPort,
	GET_rsvpSenderDestPort,
	SET_rsvpSenderDestPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderDestPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpSenderDestPort,
#define MAX_rsvpSenderDestPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpSenderDestPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used  as  a  source
 *	port  for  this sender in this session.  If the
 *	IP protocol in use, specified by rsvpSenderPro-
 *	tocol is 50 (ESP) or 51 (AH), this represents a
 *	generalized port identifier (GPI).  A value  of
 *	zero indicates that the IP protocol in use does
 *	not have ports.  This object may not be changed
 *	when  the value of the RowStatus object is 'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpSenderPort is known as 1.3.6.1.2.1.51.1.2.1.9
 */
static READONLY ubyte	AIA_rsvpSenderPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x09, 
};

static READONLY struct object_id	AI_rsvpSenderPort =
{	sizeof(AIA_rsvpSenderPort),
	AIA_rsvpSenderPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderPort.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderPort =
{
	&AI_rsvpSenderPort,
	GET_rsvpSenderPort,
	SET_rsvpSenderPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpSenderPort,
#define MAX_rsvpSenderPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpSenderPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderFlowId - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The flow ID that  this  sender  is  using,  if
 *	this  is  an IPv6 session.
 *
 ************************************************************************/

/*
 *	rsvpSenderFlowId is known as 1.3.6.1.2.1.51.1.2.1.10
 */
static READONLY ubyte	AIA_rsvpSenderFlowId[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0a, 
};

static READONLY struct object_id	AI_rsvpSenderFlowId =
{	sizeof(AIA_rsvpSenderFlowId),
	AIA_rsvpSenderFlowId,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderFlowId(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderFlowId(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderFlowId;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderFlowId =
{
	&AI_rsvpSenderFlowId,
	GET_rsvpSenderFlowId,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderFlowId	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderFlowId,
#define MAX_rsvpSenderFlowId	PEER_PORTABLE_ULONG(0xffffff)
	MAX_rsvpSenderFlowId,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderHopAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The address used  by  the  previous  RSVP  hop
 *	(which may be the original sender).
 *
 ************************************************************************/

/*
 *	rsvpSenderHopAddr is known as 1.3.6.1.2.1.51.1.2.1.11
 */
static READONLY ubyte	AIA_rsvpSenderHopAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0b, 
};

static READONLY struct object_id	AI_rsvpSenderHopAddr =
{	sizeof(AIA_rsvpSenderHopAddr),
	AIA_rsvpSenderHopAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderHopAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderHopAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderHopAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderHopAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderHopAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderHopAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderHopAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderHopAddr =
{
	&AI_rsvpSenderHopAddr,
	GET_rsvpSenderHopAddr,
	SET_rsvpSenderHopAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderHopAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpSenderHopAddr,
#define MAX_rsvpSenderHopAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpSenderHopAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderHopLih - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Logical Interface Handle used by the  pre-
 *	vious  RSVP  hop  (which  may  be  the original
 *	sender).
 *
 ************************************************************************/

/*
 *	rsvpSenderHopLih is known as 1.3.6.1.2.1.51.1.2.1.12
 */
static READONLY ubyte	AIA_rsvpSenderHopLih[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0c, 
};

static READONLY struct object_id	AI_rsvpSenderHopLih =
{	sizeof(AIA_rsvpSenderHopLih),
	AIA_rsvpSenderHopLih,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderHopLih(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderHopLih(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderHopLih;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderHopLih(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderHopLih(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderHopLih = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderHopLih =
{
	&AI_rsvpSenderHopLih,
	GET_rsvpSenderHopLih,
	SET_rsvpSenderHopLih,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderInterface - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The ifIndex value of the  interface  on  which
 *	this PATH message was most recently received.
 *
 ************************************************************************/

/*
 *	rsvpSenderInterface is known as 1.3.6.1.2.1.51.1.2.1.13
 */
static READONLY ubyte	AIA_rsvpSenderInterface[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0d, 
};

static READONLY struct object_id	AI_rsvpSenderInterface =
{	sizeof(AIA_rsvpSenderInterface),
	AIA_rsvpSenderInterface,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderInterface(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderInterface;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderInterface(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderInterface = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderInterface =
{
	&AI_rsvpSenderInterface,
	GET_rsvpSenderInterface,
	SET_rsvpSenderInterface,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTSpecRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Average Bit  Rate  of  the  sender's  data
 *	stream.   Within  a transmission burst, the ar-
 *	rival rate may be as fast  as  rsvpSenderTSpec-
 *	PeakRate  (if  supported by the service model);
 *	however, averaged across two or more burst  in-
 *	tervals,  the  rate  should not exceed rsvpSen-
 *	derTSpecRate.
 *	
 *	Note that this is a prediction, often based  on
 *	the  general  capability  of a type of codec or
 *	particular encoding; the measured average  rate
 *	may be significantly lower.
 *
 ************************************************************************/

/*
 *	rsvpSenderTSpecRate is known as 1.3.6.1.2.1.51.1.2.1.14
 */
static READONLY ubyte	AIA_rsvpSenderTSpecRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0e, 
};

static READONLY struct object_id	AI_rsvpSenderTSpecRate =
{	sizeof(AIA_rsvpSenderTSpecRate),
	AIA_rsvpSenderTSpecRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderTSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderTSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTSpecRate =
{
	&AI_rsvpSenderTSpecRate,
	GET_rsvpSenderTSpecRate,
	SET_rsvpSenderTSpecRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTSpecRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTSpecRate,
#define MAX_rsvpSenderTSpecRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderTSpecRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTSpecPeakRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Peak Bit Rate of the sender's data stream.
 *	Traffic  arrival is not expected to exceed this
 *	rate at any time, apart  from  the  effects  of
 *	jitter in the network.  If not specified in the
 *	TSpec, this returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderTSpecPeakRate is known as 1.3.6.1.2.1.51.1.2.1.15
 */
static READONLY ubyte	AIA_rsvpSenderTSpecPeakRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x0f, 
};

static READONLY struct object_id	AI_rsvpSenderTSpecPeakRate =
{	sizeof(AIA_rsvpSenderTSpecPeakRate),
	AIA_rsvpSenderTSpecPeakRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTSpecPeakRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTSpecPeakRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecPeakRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderTSpecPeakRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderTSpecPeakRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecPeakRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTSpecPeakRate =
{
	&AI_rsvpSenderTSpecPeakRate,
	GET_rsvpSenderTSpecPeakRate,
	SET_rsvpSenderTSpecPeakRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTSpecPeakRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTSpecPeakRate,
#define MAX_rsvpSenderTSpecPeakRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderTSpecPeakRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTSpecBurst - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The size of the largest  burst  expected  from
 *	the sender at a time.
 *
 ************************************************************************/

/*
 *	rsvpSenderTSpecBurst is known as 1.3.6.1.2.1.51.1.2.1.16
 */
static READONLY ubyte	AIA_rsvpSenderTSpecBurst[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x10, 
};

static READONLY struct object_id	AI_rsvpSenderTSpecBurst =
{	sizeof(AIA_rsvpSenderTSpecBurst),
	AIA_rsvpSenderTSpecBurst,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTSpecBurst(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTSpecBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecBurst;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderTSpecBurst(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderTSpecBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecBurst = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTSpecBurst =
{
	&AI_rsvpSenderTSpecBurst,
	GET_rsvpSenderTSpecBurst,
	SET_rsvpSenderTSpecBurst,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTSpecBurst	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTSpecBurst,
#define MAX_rsvpSenderTSpecBurst	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderTSpecBurst,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTSpecMinTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The minimum message size for  this  flow.  The
 *	policing  algorithm will treat smaller messages
 *	as though they are this size.
 *
 ************************************************************************/

/*
 *	rsvpSenderTSpecMinTU is known as 1.3.6.1.2.1.51.1.2.1.17
 */
static READONLY ubyte	AIA_rsvpSenderTSpecMinTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x11, 
};

static READONLY struct object_id	AI_rsvpSenderTSpecMinTU =
{	sizeof(AIA_rsvpSenderTSpecMinTU),
	AIA_rsvpSenderTSpecMinTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTSpecMinTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTSpecMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecMinTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderTSpecMinTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderTSpecMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecMinTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTSpecMinTU =
{
	&AI_rsvpSenderTSpecMinTU,
	GET_rsvpSenderTSpecMinTU,
	SET_rsvpSenderTSpecMinTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTSpecMinTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTSpecMinTU,
#define MAX_rsvpSenderTSpecMinTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderTSpecMinTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTSpecMaxTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The maximum message size for  this  flow.  The
 *	admission  algorithm  will  reject TSpecs whose
 *	Maximum Transmission Unit, plus  the  interface
 *	headers, exceed the interface MTU.
 *
 ************************************************************************/

/*
 *	rsvpSenderTSpecMaxTU is known as 1.3.6.1.2.1.51.1.2.1.18
 */
static READONLY ubyte	AIA_rsvpSenderTSpecMaxTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x12, 
};

static READONLY struct object_id	AI_rsvpSenderTSpecMaxTU =
{	sizeof(AIA_rsvpSenderTSpecMaxTU),
	AIA_rsvpSenderTSpecMaxTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTSpecMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTSpecMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecMaxTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderTSpecMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderTSpecMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderTSpecMaxTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTSpecMaxTU =
{
	&AI_rsvpSenderTSpecMaxTU,
	GET_rsvpSenderTSpecMaxTU,
	SET_rsvpSenderTSpecMaxTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTSpecMaxTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTSpecMaxTU,
#define MAX_rsvpSenderTSpecMaxTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderTSpecMaxTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderInterval - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The interval between refresh messages  as  ad-
 *	
 *	
 *	
 *	vertised by the Previous Hop.
 *
 ************************************************************************/

/*
 *	rsvpSenderInterval is known as 1.3.6.1.2.1.51.1.2.1.19
 */
static READONLY ubyte	AIA_rsvpSenderInterval[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x13, 
};

static READONLY struct object_id	AI_rsvpSenderInterval =
{	sizeof(AIA_rsvpSenderInterval),
	AIA_rsvpSenderInterval,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderInterval(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderInterval;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderInterval(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderInterval = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderInterval =
{
	&AI_rsvpSenderInterval,
	GET_rsvpSenderInterval,
	SET_rsvpSenderInterval,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderInterval	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderInterval,
#define MAX_rsvpSenderInterval	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderInterval,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderRSVPHop - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the node believes that  the  previous
 *	IP  hop is an RSVP hop.  If FALSE, the node be-
 *	lieves that the previous IP hop may not  be  an
 *	RSVP hop.
 *
 ************************************************************************/

/*
 *	rsvpSenderRSVPHop is known as 1.3.6.1.2.1.51.1.2.1.20
 */
static READONLY ubyte	AIA_rsvpSenderRSVPHop[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x14, 
};

static READONLY struct object_id	AI_rsvpSenderRSVPHop =
{	sizeof(AIA_rsvpSenderRSVPHop),
	AIA_rsvpSenderRSVPHop,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderRSVPHop(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderRSVPHop(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderRSVPHop;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderRSVPHop(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderRSVPHop(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderRSVPHop = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderRSVPHop =
{
	&AI_rsvpSenderRSVPHop,
	GET_rsvpSenderRSVPHop,
	SET_rsvpSenderRSVPHop,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderRSVPHop	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderRSVPHop,
#define MAX_rsvpSenderRSVPHop	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderRSVPHop,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderLastChange - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The time of the last change in this PATH  mes-
 *	sage;  This is either the first time it was re-
 *	ceived or the time of the most recent change in
 *	parameters.
 *
 ************************************************************************/

/*
 *	rsvpSenderLastChange is known as 1.3.6.1.2.1.51.1.2.1.21
 */
static READONLY ubyte	AIA_rsvpSenderLastChange[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x15, 
};

static READONLY struct object_id	AI_rsvpSenderLastChange =
{	sizeof(AIA_rsvpSenderLastChange),
	AIA_rsvpSenderLastChange,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderLastChange(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderLastChange(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	TimeTicks	*attr = (TimeTicks *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderLastChange;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderLastChange =
{
	&AI_rsvpSenderLastChange,
	GET_rsvpSenderLastChange,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_TICKS,		/* TimeTicks */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderPolicy - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The contents of the policy  object,  displayed
 *	as an uninterpreted string of octets, including
 *	the object header.  In the absence of  such  an
 *	object, this should be of zero length.
 *
 ************************************************************************/

/*
 *	rsvpSenderPolicy is known as 1.3.6.1.2.1.51.1.2.1.22
 */
static READONLY ubyte	AIA_rsvpSenderPolicy[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x16, 
};

static READONLY struct object_id	AI_rsvpSenderPolicy =
{	sizeof(AIA_rsvpSenderPolicy),
	AIA_rsvpSenderPolicy,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderPolicy(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderPolicy(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderPolicy;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderPolicy(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderPolicy(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderPolicy.len = attr->len;
	ubcopy(attr->val,
		((rsvpSenderEntry_t *)ctxt)->rsvpSenderPolicy.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderPolicy =
{
	&AI_rsvpSenderPolicy,
	GET_rsvpSenderPolicy,
	SET_rsvpSenderPolicy,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpSenderPolicy	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpSenderPolicy,
#define MAX_rsvpSenderPolicy	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpSenderPolicy,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecBreak - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The global break bit general  characterization
 *	parameter  from  the ADSPEC.  If TRUE, at least
 *	one non-IS hop was detected in  the  path.   If
 *	
 *	
 *	
 *	FALSE, no non-IS hops were detected.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecBreak is known as 1.3.6.1.2.1.51.1.2.1.23
 */
static READONLY ubyte	AIA_rsvpSenderAdspecBreak[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x17, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecBreak =
{	sizeof(AIA_rsvpSenderAdspecBreak),
	AIA_rsvpSenderAdspecBreak,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecBreak(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecBreak;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecBreak(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecBreak = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecBreak =
{
	&AI_rsvpSenderAdspecBreak,
	GET_rsvpSenderAdspecBreak,
	SET_rsvpSenderAdspecBreak,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecBreak	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderAdspecBreak,
#define MAX_rsvpSenderAdspecBreak	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderAdspecBreak,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecHopCount - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The hop count general characterization parame-
 *	ter  from  the  ADSPEC.   A  return  of zero or
 *	noSuchValue indicates one of the following con-
 *	ditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecHopCount is known as 1.3.6.1.2.1.51.1.2.1.24
 */
static READONLY ubyte	AIA_rsvpSenderAdspecHopCount[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x18, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecHopCount =
{	sizeof(AIA_rsvpSenderAdspecHopCount),
	AIA_rsvpSenderAdspecHopCount,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecHopCount(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecHopCount;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecHopCount(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecHopCount = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecHopCount =
{
	&AI_rsvpSenderAdspecHopCount,
	GET_rsvpSenderAdspecHopCount,
	SET_rsvpSenderAdspecHopCount,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecHopCount	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecHopCount,
#define MAX_rsvpSenderAdspecHopCount	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecHopCount,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecPathBw - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The path bandwidth estimate general character-
 *	ization parameter from the ADSPEC.  A return of
 *	zero or noSuchValue indicates one of  the  fol-
 *	lowing conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecPathBw is known as 1.3.6.1.2.1.51.1.2.1.25
 */
static READONLY ubyte	AIA_rsvpSenderAdspecPathBw[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x19, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecPathBw =
{	sizeof(AIA_rsvpSenderAdspecPathBw),
	AIA_rsvpSenderAdspecPathBw,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecPathBw(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecPathBw;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecPathBw(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecPathBw = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecPathBw =
{
	&AI_rsvpSenderAdspecPathBw,
	GET_rsvpSenderAdspecPathBw,
	SET_rsvpSenderAdspecPathBw,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecPathBw	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecPathBw,
#define MAX_rsvpSenderAdspecPathBw	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderAdspecPathBw,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecMinLatency - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The minimum path latency general characteriza-
 *	tion  parameter  from  the ADSPEC.  A return of
 *	zero or noSuchValue indicates one of  the  fol-
 *	lowing conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecMinLatency is known as 1.3.6.1.2.1.51.1.2.1.26
 */
static READONLY ubyte	AIA_rsvpSenderAdspecMinLatency[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1a, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecMinLatency =
{	sizeof(AIA_rsvpSenderAdspecMinLatency),
	AIA_rsvpSenderAdspecMinLatency,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecMinLatency;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecMinLatency = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecMinLatency =
{
	&AI_rsvpSenderAdspecMinLatency,
	GET_rsvpSenderAdspecMinLatency,
	SET_rsvpSenderAdspecMinLatency,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecMtu - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The composed Maximum Transmission Unit general
 *	characterization  parameter from the ADSPEC.  A
 *	return of zero or noSuchValue indicates one  of
 *	the following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecMtu is known as 1.3.6.1.2.1.51.1.2.1.27
 */
static READONLY ubyte	AIA_rsvpSenderAdspecMtu[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1b, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecMtu =
{	sizeof(AIA_rsvpSenderAdspecMtu),
	AIA_rsvpSenderAdspecMtu,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecMtu(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecMtu;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecMtu(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecMtu = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecMtu =
{
	&AI_rsvpSenderAdspecMtu,
	GET_rsvpSenderAdspecMtu,
	SET_rsvpSenderAdspecMtu,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecMtu	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecMtu,
#define MAX_rsvpSenderAdspecMtu	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecMtu,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedSvc - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the ADSPEC contains a Guaranteed Ser-
 *	vice  fragment.   If FALSE, the ADSPEC does not
 *	contain a Guaranteed Service fragment.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedSvc is known as 1.3.6.1.2.1.51.1.2.1.28
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedSvc[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1c, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedSvc =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedSvc),
	AIA_rsvpSenderAdspecGuaranteedSvc,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedSvc(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedSvc(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedSvc;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedSvc(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedSvc(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedSvc = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedSvc =
{
	&AI_rsvpSenderAdspecGuaranteedSvc,
	GET_rsvpSenderAdspecGuaranteedSvc,
	SET_rsvpSenderAdspecGuaranteedSvc,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecGuaranteedSvc	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderAdspecGuaranteedSvc,
#define MAX_rsvpSenderAdspecGuaranteedSvc	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderAdspecGuaranteedSvc,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedBreak - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the Guaranteed Service  fragment  has
 *	its  'break'  bit  set,  indicating that one or
 *	more nodes along the path do  not  support  the
 *	guaranteed  service.   If  FALSE,  and rsvpSen-
 *	derAdspecGuaranteedSvc is TRUE, the 'break' bit
 *	is not set.
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns FALSE or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedBreak is known as 1.3.6.1.2.1.51.1.2.1.29
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedBreak[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1d, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedBreak =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedBreak),
	AIA_rsvpSenderAdspecGuaranteedBreak,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedBreak(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedBreak;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedBreak(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedBreak = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedBreak =
{
	&AI_rsvpSenderAdspecGuaranteedBreak,
	GET_rsvpSenderAdspecGuaranteedBreak,
	SET_rsvpSenderAdspecGuaranteedBreak,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecGuaranteedBreak	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderAdspecGuaranteedBreak,
#define MAX_rsvpSenderAdspecGuaranteedBreak	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderAdspecGuaranteedBreak,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedCtot - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is   the  end-to-end  composed  value  for  the
 *	guaranteed service 'C' parameter.  A return  of
 *	zero  or  noSuchValue indicates one of the fol-
 *	lowing conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedCtot is known as 1.3.6.1.2.1.51.1.2.1.30
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedCtot[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1e, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedCtot =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedCtot),
	AIA_rsvpSenderAdspecGuaranteedCtot,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedCtot(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedCtot(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedCtot;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedCtot(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedCtot(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedCtot = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedCtot =
{
	&AI_rsvpSenderAdspecGuaranteedCtot,
	GET_rsvpSenderAdspecGuaranteedCtot,
	SET_rsvpSenderAdspecGuaranteedCtot,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedDtot - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is   the  end-to-end  composed  value  for  the
 *	guaranteed service 'D' parameter.  A return  of
 *	zero  or  noSuchValue indicates one of the fol-
 *	lowing conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedDtot is known as 1.3.6.1.2.1.51.1.2.1.31
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedDtot[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x1f, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedDtot =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedDtot),
	AIA_rsvpSenderAdspecGuaranteedDtot,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedDtot(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedDtot(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedDtot;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedDtot(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedDtot(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedDtot = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedDtot =
{
	&AI_rsvpSenderAdspecGuaranteedDtot,
	GET_rsvpSenderAdspecGuaranteedDtot,
	SET_rsvpSenderAdspecGuaranteedDtot,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedCsum - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is  the  composed value for the guaranteed ser-
 *	
 *	
 *	
 *	vice 'C' parameter  since  the  last  reshaping
 *	point.   A  return of zero or noSuchValue indi-
 *	cates one of the following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedCsum is known as 1.3.6.1.2.1.51.1.2.1.32
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedCsum[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x20, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedCsum =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedCsum),
	AIA_rsvpSenderAdspecGuaranteedCsum,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedCsum(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedCsum(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedCsum;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedCsum(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedCsum(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedCsum = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedCsum =
{
	&AI_rsvpSenderAdspecGuaranteedCsum,
	GET_rsvpSenderAdspecGuaranteedCsum,
	SET_rsvpSenderAdspecGuaranteedCsum,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedDsum - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is  the  composed value for the guaranteed ser-
 *	vice 'D' parameter  since  the  last  reshaping
 *	point.   A  return of zero or noSuchValue indi-
 *	cates one of the following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedDsum is known as 1.3.6.1.2.1.51.1.2.1.33
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedDsum[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x21, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedDsum =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedDsum),
	AIA_rsvpSenderAdspecGuaranteedDsum,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedDsum(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedDsum(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedDsum;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedDsum(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedDsum(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedDsum = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedDsum =
{
	&AI_rsvpSenderAdspecGuaranteedDsum,
	GET_rsvpSenderAdspecGuaranteedDsum,
	SET_rsvpSenderAdspecGuaranteedDsum,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedHopCount - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is  the  service-specific  override  of the hop
 *	count general characterization  parameter  from
 *	the  ADSPEC.   A  return of zero or noSuchValue
 *	indicates one of the following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	
 *	
 *	
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedHopCount is known as 1.3.6.1.2.1.51.1.2.1.34
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedHopCount[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x22, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedHopCount =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedHopCount),
	AIA_rsvpSenderAdspecGuaranteedHopCount,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedHopCount(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedHopCount;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedHopCount(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedHopCount = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedHopCount =
{
	&AI_rsvpSenderAdspecGuaranteedHopCount,
	GET_rsvpSenderAdspecGuaranteedHopCount,
	SET_rsvpSenderAdspecGuaranteedHopCount,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecGuaranteedHopCount	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecGuaranteedHopCount,
#define MAX_rsvpSenderAdspecGuaranteedHopCount	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecGuaranteedHopCount,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedPathBw - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is  the  service-specific  override of the path
 *	bandwidth  estimate  general   characterization
 *	parameter from the ADSPEC.  A return of zero or
 *	noSuchValue indicates one of the following con-
 *	ditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedPathBw is known as 1.3.6.1.2.1.51.1.2.1.35
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedPathBw[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x23, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedPathBw =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedPathBw),
	AIA_rsvpSenderAdspecGuaranteedPathBw,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedPathBw(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedPathBw;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedPathBw(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedPathBw = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedPathBw =
{
	&AI_rsvpSenderAdspecGuaranteedPathBw,
	GET_rsvpSenderAdspecGuaranteedPathBw,
	SET_rsvpSenderAdspecGuaranteedPathBw,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecGuaranteedPathBw	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecGuaranteedPathBw,
#define MAX_rsvpSenderAdspecGuaranteedPathBw	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderAdspecGuaranteedPathBw,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedMinLatency - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is the service-specific override of the minimum
 *	path latency general characterization parameter
 *	from  the  ADSPEC.  A return of zero or noSuch-
 *	Value indicates one  of  the  following  condi-
 *	tions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedMinLatency is known as 1.3.6.1.2.1.51.1.2.1.36
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedMinLatency[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x24, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedMinLatency =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedMinLatency),
	AIA_rsvpSenderAdspecGuaranteedMinLatency,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedMinLatency;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedMinLatency = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedMinLatency =
{
	&AI_rsvpSenderAdspecGuaranteedMinLatency,
	GET_rsvpSenderAdspecGuaranteedMinLatency,
	SET_rsvpSenderAdspecGuaranteedMinLatency,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecGuaranteedMtu - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecGuaranteedSvc is TRUE, this
 *	is  the  service-specific  override of the com-
 *	posed Maximum Transmission Unit general charac-
 *	terization parameter from the ADSPEC.  A return
 *	of zero or noSuchValue  indicates  one  of  the
 *	following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecGuaranteedSvc is FALSE, this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecGuaranteedMtu is known as 1.3.6.1.2.1.51.1.2.1.37
 */
static READONLY ubyte	AIA_rsvpSenderAdspecGuaranteedMtu[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x25, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecGuaranteedMtu =
{	sizeof(AIA_rsvpSenderAdspecGuaranteedMtu),
	AIA_rsvpSenderAdspecGuaranteedMtu,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecGuaranteedMtu(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecGuaranteedMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedMtu;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecGuaranteedMtu(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecGuaranteedMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecGuaranteedMtu = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecGuaranteedMtu =
{
	&AI_rsvpSenderAdspecGuaranteedMtu,
	GET_rsvpSenderAdspecGuaranteedMtu,
	SET_rsvpSenderAdspecGuaranteedMtu,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecGuaranteedMtu	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecGuaranteedMtu,
#define MAX_rsvpSenderAdspecGuaranteedMtu	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecGuaranteedMtu,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadSvc - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the ADSPEC contains a Controlled Load
 *	Service  fragment.   If  FALSE, the ADSPEC does
 *	not contain a  Controlled  Load  Service  frag-
 *	ment.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadSvc is known as 1.3.6.1.2.1.51.1.2.1.38
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadSvc[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x26, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadSvc =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadSvc),
	AIA_rsvpSenderAdspecCtrlLoadSvc,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadSvc(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadSvc(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadSvc;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadSvc(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadSvc(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadSvc = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadSvc =
{
	&AI_rsvpSenderAdspecCtrlLoadSvc,
	GET_rsvpSenderAdspecCtrlLoadSvc,
	SET_rsvpSenderAdspecCtrlLoadSvc,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecCtrlLoadSvc	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderAdspecCtrlLoadSvc,
#define MAX_rsvpSenderAdspecCtrlLoadSvc	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderAdspecCtrlLoadSvc,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadBreak - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the Controlled Load Service  fragment
 *	has its 'break' bit set, indicating that one or
 *	more nodes along the path do  not  support  the
 *	controlled   load   service.    If  FALSE,  and
 *	rsvpSenderAdspecCtrlLoadSvc   is   TRUE,    the
 *	'break' bit is not set.
 *	
 *	If rsvpSenderAdspecCtrlLoadSvc is  FALSE,  this
 *	returns FALSE or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadBreak is known as 1.3.6.1.2.1.51.1.2.1.39
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadBreak[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x27, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadBreak =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadBreak),
	AIA_rsvpSenderAdspecCtrlLoadBreak,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadBreak(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadBreak;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadBreak(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadBreak(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadBreak = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadBreak =
{
	&AI_rsvpSenderAdspecCtrlLoadBreak,
	GET_rsvpSenderAdspecCtrlLoadBreak,
	SET_rsvpSenderAdspecCtrlLoadBreak,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecCtrlLoadBreak	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderAdspecCtrlLoadBreak,
#define MAX_rsvpSenderAdspecCtrlLoadBreak	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpSenderAdspecCtrlLoadBreak,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadHopCount - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecCtrlLoadSvc is  TRUE,  this
 *	is  the  service-specific  override  of the hop
 *	count general characterization  parameter  from
 *	the  ADSPEC.   A  return of zero or noSuchValue
 *	indicates one of the following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecCtrlLoadSvc is  FALSE,  this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadHopCount is known as 1.3.6.1.2.1.51.1.2.1.40
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadHopCount[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x28, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadHopCount =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadHopCount),
	AIA_rsvpSenderAdspecCtrlLoadHopCount,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadHopCount(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadHopCount;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadHopCount(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadHopCount(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadHopCount = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadHopCount =
{
	&AI_rsvpSenderAdspecCtrlLoadHopCount,
	GET_rsvpSenderAdspecCtrlLoadHopCount,
	SET_rsvpSenderAdspecCtrlLoadHopCount,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecCtrlLoadHopCount	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecCtrlLoadHopCount,
#define MAX_rsvpSenderAdspecCtrlLoadHopCount	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecCtrlLoadHopCount,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadPathBw - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecCtrlLoadSvc is  TRUE,  this
 *	is  the  service-specific  override of the path
 *	bandwidth  estimate  general   characterization
 *	parameter from the ADSPEC.  A return of zero or
 *	noSuchValue indicates one of the following con-
 *	ditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecCtrlLoadSvc is  FALSE,  this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadPathBw is known as 1.3.6.1.2.1.51.1.2.1.41
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadPathBw[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x29, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadPathBw =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadPathBw),
	AIA_rsvpSenderAdspecCtrlLoadPathBw,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadPathBw(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadPathBw;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadPathBw(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadPathBw(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadPathBw = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadPathBw =
{
	&AI_rsvpSenderAdspecCtrlLoadPathBw,
	GET_rsvpSenderAdspecCtrlLoadPathBw,
	SET_rsvpSenderAdspecCtrlLoadPathBw,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecCtrlLoadPathBw	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecCtrlLoadPathBw,
#define MAX_rsvpSenderAdspecCtrlLoadPathBw	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderAdspecCtrlLoadPathBw,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadMinLatency - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecCtrlLoadSvc is  TRUE,  this
 *	
 *	
 *	
 *	is the service-specific override of the minimum
 *	path latency general characterization parameter
 *	from  the  ADSPEC.  A return of zero or noSuch-
 *	Value indicates one  of  the  following  condi-
 *	tions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecCtrlLoadSvc is  FALSE,  this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadMinLatency is known as 1.3.6.1.2.1.51.1.2.1.42
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadMinLatency[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x2a, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadMinLatency =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadMinLatency),
	AIA_rsvpSenderAdspecCtrlLoadMinLatency,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadMinLatency;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadMinLatency(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadMinLatency(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadMinLatency = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadMinLatency =
{
	&AI_rsvpSenderAdspecCtrlLoadMinLatency,
	GET_rsvpSenderAdspecCtrlLoadMinLatency,
	SET_rsvpSenderAdspecCtrlLoadMinLatency,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderAdspecCtrlLoadMtu - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If rsvpSenderAdspecCtrlLoadSvc is  TRUE,  this
 *	is  the  service-specific  override of the com-
 *	posed Maximum Transmission Unit general charac-
 *	terization parameter from the ADSPEC.  A return
 *	of zero or noSuchValue  indicates  one  of  the
 *	following conditions:
 *	
 *	   the invalid bit was set
 *	   the parameter was not present
 *	
 *	If rsvpSenderAdspecCtrlLoadSvc is  FALSE,  this
 *	returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpSenderAdspecCtrlLoadMtu is known as 1.3.6.1.2.1.51.1.2.1.43
 */
static READONLY ubyte	AIA_rsvpSenderAdspecCtrlLoadMtu[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x2b, 
};

static READONLY struct object_id	AI_rsvpSenderAdspecCtrlLoadMtu =
{	sizeof(AIA_rsvpSenderAdspecCtrlLoadMtu),
	AIA_rsvpSenderAdspecCtrlLoadMtu,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderAdspecCtrlLoadMtu(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderAdspecCtrlLoadMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadMtu;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderAdspecCtrlLoadMtu(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderAdspecCtrlLoadMtu(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderAdspecCtrlLoadMtu = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderAdspecCtrlLoadMtu =
{
	&AI_rsvpSenderAdspecCtrlLoadMtu,
	GET_rsvpSenderAdspecCtrlLoadMtu,
	SET_rsvpSenderAdspecCtrlLoadMtu,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderAdspecCtrlLoadMtu	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderAdspecCtrlLoadMtu,
#define MAX_rsvpSenderAdspecCtrlLoadMtu	PEER_PORTABLE_ULONG(0xffff)
	MAX_rsvpSenderAdspecCtrlLoadMtu,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all active PATH  messages.   This
 *	object  may  be used to install static PATH in-
 *	formation or delete PATH information.
 *
 ************************************************************************/

/*
 *	rsvpSenderStatus is known as 1.3.6.1.2.1.51.1.2.1.44
 */
static READONLY ubyte	AIA_rsvpSenderStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x2c, 
};

static READONLY struct object_id	AI_rsvpSenderStatus =
{	sizeof(AIA_rsvpSenderStatus),
	AIA_rsvpSenderStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpSenderEntry_t *)ctxt)->rsvpSenderStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderStatus =
{
	&AI_rsvpSenderStatus,
	GET_rsvpSenderStatus,
	SET_rsvpSenderStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderStatus,
#define MAX_rsvpSenderStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpSenderStatus,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderTTL - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The TTL value in the RSVP header that was last
 *	received.
 *
 ************************************************************************/

/*
 *	rsvpSenderTTL is known as 1.3.6.1.2.1.51.1.2.1.45
 */
static READONLY ubyte	AIA_rsvpSenderTTL[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 0x2d, 
};

static READONLY struct object_id	AI_rsvpSenderTTL =
{	sizeof(AIA_rsvpSenderTTL),
	AIA_rsvpSenderTTL,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderTTL(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderTTL(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderEntry_t *)ctxt)->rsvpSenderTTL;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpSenderEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpSenderEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpSenderEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpSenderEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderTTL =
{
	&AI_rsvpSenderTTL,
	GET_rsvpSenderTTL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpSenderEntry,
	peer_locate_rsvpSenderEntry,
	IX_rsvpSenderTable_rsvpSenderEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderTTL	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderTTL,
#define MAX_rsvpSenderTTL	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpSenderTTL,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpSenderEntry[] =
{
	&A_rsvpSenderNumber,
	&A_rsvpSenderType,
	&A_rsvpSenderDestAddr,
	&A_rsvpSenderAddr,
	&A_rsvpSenderDestAddrLength,
	&A_rsvpSenderAddrLength,
	&A_rsvpSenderProtocol,
	&A_rsvpSenderDestPort,
	&A_rsvpSenderPort,
	&A_rsvpSenderFlowId,
	&A_rsvpSenderHopAddr,
	&A_rsvpSenderHopLih,
	&A_rsvpSenderInterface,
	&A_rsvpSenderTSpecRate,
	&A_rsvpSenderTSpecPeakRate,
	&A_rsvpSenderTSpecBurst,
	&A_rsvpSenderTSpecMinTU,
	&A_rsvpSenderTSpecMaxTU,
	&A_rsvpSenderInterval,
	&A_rsvpSenderRSVPHop,
	&A_rsvpSenderLastChange,
	&A_rsvpSenderPolicy,
	&A_rsvpSenderAdspecBreak,
	&A_rsvpSenderAdspecHopCount,
	&A_rsvpSenderAdspecPathBw,
	&A_rsvpSenderAdspecMinLatency,
	&A_rsvpSenderAdspecMtu,
	&A_rsvpSenderAdspecGuaranteedSvc,
	&A_rsvpSenderAdspecGuaranteedBreak,
	&A_rsvpSenderAdspecGuaranteedCtot,
	&A_rsvpSenderAdspecGuaranteedDtot,
	&A_rsvpSenderAdspecGuaranteedCsum,
	&A_rsvpSenderAdspecGuaranteedDsum,
	&A_rsvpSenderAdspecGuaranteedHopCount,
	&A_rsvpSenderAdspecGuaranteedPathBw,
	&A_rsvpSenderAdspecGuaranteedMinLatency,
	&A_rsvpSenderAdspecGuaranteedMtu,
	&A_rsvpSenderAdspecCtrlLoadSvc,
	&A_rsvpSenderAdspecCtrlLoadBreak,
	&A_rsvpSenderAdspecCtrlLoadHopCount,
	&A_rsvpSenderAdspecCtrlLoadPathBw,
	&A_rsvpSenderAdspecCtrlLoadMinLatency,
	&A_rsvpSenderAdspecCtrlLoadMtu,
	&A_rsvpSenderStatus,
	&A_rsvpSenderTTL,
	(struct attribute READONLY *) NULL
};

#define A_rsvpSenderTable_rsvpSessionNumber A_rsvpSessionNumber
#define A_rsvpSenderTable_rsvpSenderNumber A_rsvpSenderNumber


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpSenderTable_rsvpSenderEntry[] =
{
	{ 0 + 0,  &A_rsvpSenderTable_rsvpSessionNumber	},
	{ 0 + 0,  &A_rsvpSenderTable_rsvpSenderNumber	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpSenderEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 0x01, 
};

static READONLY struct object_id	GI_rsvpSenderEntry =
{
	9,
	GIA_rsvpSenderEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSenderEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSenderEntry =
{
	SNMP_CLASS,
	&GI_rsvpSenderEntry,
	1,
	AS_rsvpSenderEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpSenderTable_rsvpSenderEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSenderEntry = 
{
	&SMI_GROUP_rsvpSenderEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpSenderTable[] =
{
	&CG_rsvpSenderEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpSenderTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpSenderTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x02, 
};

static READONLY struct object_id	GI_rsvpSenderTable =
{
	8,
	GIA_rsvpSenderTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSenderTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSenderTable =
{
	SNMP_CLASS,
	&GI_rsvpSenderTable,
	1,
	AS_rsvpSenderTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpSenderTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSenderTable = 
{
	&SMI_GROUP_rsvpSenderTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpSenderOutInterfaceStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all active PATH messages.
 *
 ************************************************************************/

/*
 *	rsvpSenderOutInterfaceStatus is known as 1.3.6.1.2.1.51.1.3.1.1
 */
static READONLY ubyte	AIA_rsvpSenderOutInterfaceStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x03, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpSenderOutInterfaceStatus =
{	sizeof(AIA_rsvpSenderOutInterfaceStatus),
	AIA_rsvpSenderOutInterfaceStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderOutInterfaceStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderOutInterfaceStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpSenderOutInterfaceEntry_t *)ctxt)->rsvpSenderOutInterfaceStatus;
	return(SNMP_ERR_NO_ERROR);
}

extern READONLY struct index_entry	IX_rsvpSenderOutInterfaceTable_rsvpSenderOutInterfaceEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderOutInterfaceStatus =
{
	&AI_rsvpSenderOutInterfaceStatus,
	GET_rsvpSenderOutInterfaceStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
        peer_next_rsvpSenderOutInterfaceEntry,
        peer_locate_rsvpSenderOutInterfaceEntry,
	IX_rsvpSenderOutInterfaceTable_rsvpSenderOutInterfaceEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderOutInterfaceStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpSenderOutInterfaceStatus,
#define MAX_rsvpSenderOutInterfaceStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpSenderOutInterfaceStatus,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpSenderOutInterfaceEntry[] =
{
	&A_rsvpSenderOutInterfaceStatus,
	(struct attribute READONLY *) NULL
};

#define A_rsvpSenderOutInterfaceTable_rsvpSessionNumber A_rsvpSessionNumber
#define A_rsvpSenderOutInterfaceTable_rsvpSenderNumber A_rsvpSenderNumber
#define A_rsvpSenderOutInterfaceTable_ifIndex A_ifIndex


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpSenderOutInterfaceTable_rsvpSenderOutInterfaceEntry[] =
{
	{ 0 + 0,  &A_rsvpSenderOutInterfaceTable_rsvpSessionNumber	},
	{ 0 + 0,  &A_rsvpSenderOutInterfaceTable_rsvpSenderNumber	},
	{ 0 + 0,  &A_rsvpSenderOutInterfaceTable_ifIndex	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpSenderOutInterfaceEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x03, 0x01, 
};

static READONLY struct object_id	GI_rsvpSenderOutInterfaceEntry =
{
	9,
	GIA_rsvpSenderOutInterfaceEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSenderOutInterfaceEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSenderOutInterfaceEntry =
{
	SNMP_CLASS,
	&GI_rsvpSenderOutInterfaceEntry,
	1,
	AS_rsvpSenderOutInterfaceEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpSenderOutInterfaceTable_rsvpSenderOutInterfaceEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSenderOutInterfaceEntry = 
{
	&SMI_GROUP_rsvpSenderOutInterfaceEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpSenderOutInterfaceTable[] =
{
	&CG_rsvpSenderOutInterfaceEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpSenderOutInterfaceTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpSenderOutInterfaceTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x03, 
};

static READONLY struct object_id	GI_rsvpSenderOutInterfaceTable =
{
	8,
	GIA_rsvpSenderOutInterfaceTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpSenderOutInterfaceTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpSenderOutInterfaceTable =
{
	SNMP_CLASS,
	&GI_rsvpSenderOutInterfaceTable,
	1,
	AS_rsvpSenderOutInterfaceTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpSenderOutInterfaceTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpSenderOutInterfaceTable = 
{
	&SMI_GROUP_rsvpSenderOutInterfaceTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpResvNumber - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of this reservation request.   This
 *	is  for  SNMP Indexing purposes only and has no
 *	relation to any protocol value.
 *
 ************************************************************************/

/*
 *	rsvpResvNumber is known as 1.3.6.1.2.1.51.1.4.1.1
 */
static READONLY ubyte	AIA_rsvpResvNumber[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpResvNumber =
{	sizeof(AIA_rsvpResvNumber),
	AIA_rsvpResvNumber,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvNumber(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvNumber(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvNumber;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
extern READONLY struct index_entry	IX_rsvpResvTable_rsvpResvEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvNumber =
{
	&AI_rsvpResvNumber,
	GET_rsvpResvNumber,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvNumber	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvNumber,
#define MAX_rsvpResvNumber	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvNumber,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvType - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The type of session (IP4, IP6, IP6  with  flow
 *	information, etc).
 *
 ************************************************************************/

/*
 *	rsvpResvType is known as 1.3.6.1.2.1.51.1.4.1.2
 */
static READONLY ubyte	AIA_rsvpResvType[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpResvType =
{	sizeof(AIA_rsvpResvType),
	AIA_rsvpResvType,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvType(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvType;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvType(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvType = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvType =
{
	&AI_rsvpResvType,
	GET_rsvpResvType,
	SET_rsvpResvType,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvType	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvType,
#define MAX_rsvpResvType	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvType,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvDestAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The destination address used by all senders in
 *	this  session.   This object may not be changed
 *	when the value of the RowStatus object is  'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpResvDestAddr is known as 1.3.6.1.2.1.51.1.4.1.3
 */
static READONLY ubyte	AIA_rsvpResvDestAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpResvDestAddr =
{	sizeof(AIA_rsvpResvDestAddr),
	AIA_rsvpResvDestAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvDestAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvDestAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvDestAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvDestAddr =
{
	&AI_rsvpResvDestAddr,
	GET_rsvpResvDestAddr,
	SET_rsvpResvDestAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvDestAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvDestAddr,
#define MAX_rsvpResvDestAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvDestAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvSenderAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The source address of the sender  selected  by
 *	this  reservation.  The value of all zeroes in-
 *	dicates 'all senders'.  This object may not  be
 *	changed  when the value of the RowStatus object
 *	is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvSenderAddr is known as 1.3.6.1.2.1.51.1.4.1.4
 */
static READONLY ubyte	AIA_rsvpResvSenderAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x04, 
};

static READONLY struct object_id	AI_rsvpResvSenderAddr =
{	sizeof(AIA_rsvpResvSenderAddr),
	AIA_rsvpResvSenderAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvSenderAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvSenderAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvSenderAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvSenderAddr =
{
	&AI_rsvpResvSenderAddr,
	GET_rsvpResvSenderAddr,
	SET_rsvpResvSenderAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvSenderAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvSenderAddr,
#define MAX_rsvpResvSenderAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvSenderAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvDestAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the destination address in bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvDestAddrLength is known as 1.3.6.1.2.1.51.1.4.1.5
 */
static READONLY ubyte	AIA_rsvpResvDestAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x05, 
};

static READONLY struct object_id	AI_rsvpResvDestAddrLength =
{	sizeof(AIA_rsvpResvDestAddrLength),
	AIA_rsvpResvDestAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvDestAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvDestAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvDestAddrLength =
{
	&AI_rsvpResvDestAddrLength,
	GET_rsvpResvDestAddrLength,
	SET_rsvpResvDestAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvDestAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvDestAddrLength,
#define MAX_rsvpResvDestAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpResvDestAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvSenderAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the sender's  address  in  bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvSenderAddrLength is known as 1.3.6.1.2.1.51.1.4.1.6
 */
static READONLY ubyte	AIA_rsvpResvSenderAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x06, 
};

static READONLY struct object_id	AI_rsvpResvSenderAddrLength =
{	sizeof(AIA_rsvpResvSenderAddrLength),
	AIA_rsvpResvSenderAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvSenderAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvSenderAddrLength = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvSenderAddrLength =
{
	&AI_rsvpResvSenderAddrLength,
	GET_rsvpResvSenderAddrLength,
	SET_rsvpResvSenderAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvSenderAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvSenderAddrLength,
#define MAX_rsvpResvSenderAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpResvSenderAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Protocol used by  this  session.   This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvProtocol is known as 1.3.6.1.2.1.51.1.4.1.7
 */
static READONLY ubyte	AIA_rsvpResvProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x07, 
};

static READONLY struct object_id	AI_rsvpResvProtocol =
{	sizeof(AIA_rsvpResvProtocol),
	AIA_rsvpResvProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvProtocol;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvProtocol(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvProtocol = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvProtocol =
{
	&AI_rsvpResvProtocol,
	GET_rsvpResvProtocol,
	SET_rsvpResvProtocol,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvProtocol,
#define MAX_rsvpResvProtocol	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvDestPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used as a  destina-
 *	tion  port for all senders in this session.  If
 *	the  IP   protocol   in   use,   specified   by
 *	rsvpResvProtocol,  is 50 (ESP) or 51 (AH), this
 *	represents a virtual destination  port  number.
 *	A  value of zero indicates that the IP protocol
 *	in use does not have ports.   This  object  may
 *	not  be changed when the value of the RowStatus
 *	object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvDestPort is known as 1.3.6.1.2.1.51.1.4.1.8
 */
static READONLY ubyte	AIA_rsvpResvDestPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x08, 
};

static READONLY struct object_id	AI_rsvpResvDestPort =
{	sizeof(AIA_rsvpResvDestPort),
	AIA_rsvpResvDestPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvDestPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvDestPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvDestPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvDestPort.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvDestPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvDestPort =
{
	&AI_rsvpResvDestPort,
	GET_rsvpResvDestPort,
	SET_rsvpResvDestPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvDestPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpResvDestPort,
#define MAX_rsvpResvDestPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpResvDestPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used  as  a  source
 *	port  for  this sender in this session.  If the
 *	IP protocol in use, specified by rsvpResvProto-
 *	col  is  50 (ESP) or 51 (AH), this represents a
 *	generalized port identifier (GPI).  A value  of
 *	zero indicates that the IP protocol in use does
 *	not have ports.  This object may not be changed
 *	when  the value of the RowStatus object is 'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpResvPort is known as 1.3.6.1.2.1.51.1.4.1.9
 */
static READONLY ubyte	AIA_rsvpResvPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x09, 
};

static READONLY struct object_id	AI_rsvpResvPort =
{	sizeof(AIA_rsvpResvPort),
	AIA_rsvpResvPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvPort;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvPort(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvPort.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvPort.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvPort =
{
	&AI_rsvpResvPort,
	GET_rsvpResvPort,
	SET_rsvpResvPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpResvPort,
#define MAX_rsvpResvPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpResvPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvHopAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The address used by the next RSVP  hop  (which
 *	may be the ultimate receiver).
 *
 ************************************************************************/

/*
 *	rsvpResvHopAddr is known as 1.3.6.1.2.1.51.1.4.1.10
 */
static READONLY ubyte	AIA_rsvpResvHopAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0a, 
};

static READONLY struct object_id	AI_rsvpResvHopAddr =
{	sizeof(AIA_rsvpResvHopAddr),
	AIA_rsvpResvHopAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvHopAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvHopAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvHopAddr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvHopAddr(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvHopAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvHopAddr.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvHopAddr.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvHopAddr =
{
	&AI_rsvpResvHopAddr,
	GET_rsvpResvHopAddr,
	SET_rsvpResvHopAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvHopAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvHopAddr,
#define MAX_rsvpResvHopAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvHopAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvHopLih - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Logical Interface Handle received from the
 *	previous  RSVP  hop  (which may be the ultimate
 *	receiver).
 *
 ************************************************************************/

/*
 *	rsvpResvHopLih is known as 1.3.6.1.2.1.51.1.4.1.11
 */
static READONLY ubyte	AIA_rsvpResvHopLih[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0b, 
};

static READONLY struct object_id	AI_rsvpResvHopLih =
{	sizeof(AIA_rsvpResvHopLih),
	AIA_rsvpResvHopLih,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvHopLih(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvHopLih(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvHopLih;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvHopLih(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvHopLih(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvHopLih = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvHopLih =
{
	&AI_rsvpResvHopLih,
	GET_rsvpResvHopLih,
	SET_rsvpResvHopLih,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvInterface - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The ifIndex value of the  interface  on  which
 *	this RESV message was most recently received.
 *
 ************************************************************************/

/*
 *	rsvpResvInterface is known as 1.3.6.1.2.1.51.1.4.1.12
 */
static READONLY ubyte	AIA_rsvpResvInterface[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0c, 
};

static READONLY struct object_id	AI_rsvpResvInterface =
{	sizeof(AIA_rsvpResvInterface),
	AIA_rsvpResvInterface,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvInterface(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvInterface;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvInterface(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvInterface = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvInterface =
{
	&AI_rsvpResvInterface,
	GET_rsvpResvInterface,
	SET_rsvpResvInterface,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvService - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The QoS Service  classification  requested  by
 *	the receiver.
 *
 ************************************************************************/

/*
 *	rsvpResvService is known as 1.3.6.1.2.1.51.1.4.1.13
 */
static READONLY ubyte	AIA_rsvpResvService[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0d, 
};

static READONLY struct object_id	AI_rsvpResvService =
{	sizeof(AIA_rsvpResvService),
	AIA_rsvpResvService,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvService(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvService(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvService;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvService(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvService(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvService = *attr;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
TEST_rsvpResvService(void *ctxt, void **indices, void *attr_ref)
#else
TEST_rsvpResvService(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	if ((*attr >= 1) && (*attr <= 2))	{ return(SNMP_ERR_NO_ERROR); }
	if (*attr == 5)	{ return(SNMP_ERR_NO_ERROR); }
	return(SNMP_ERR_BAD_VALUE);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvService =
{
	&AI_rsvpResvService,
	GET_rsvpResvService,
	SET_rsvpResvService,
	TEST_rsvpResvService,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvService	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvService,
#define MAX_rsvpResvService	PEER_PORTABLE_ULONG(0x5)
	MAX_rsvpResvService,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTSpecRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Average Bit  Rate  of  the  sender's  data
 *	
 *	
 *	
 *	stream.   Within  a transmission burst, the ar-
 *	rival rate may be  as  fast  as  rsvpResvTSpec-
 *	PeakRate  (if  supported by the service model);
 *	however, averaged across two or more burst  in-
 *	tervals,    the    rate   should   not   exceed
 *	rsvpResvTSpecRate.
 *	
 *	Note that this is a prediction, often based  on
 *	the  general  capability  of a type of codec or
 *	particular encoding; the measured average  rate
 *	may be significantly lower.
 *
 ************************************************************************/

/*
 *	rsvpResvTSpecRate is known as 1.3.6.1.2.1.51.1.4.1.14
 */
static READONLY ubyte	AIA_rsvpResvTSpecRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0e, 
};

static READONLY struct object_id	AI_rsvpResvTSpecRate =
{	sizeof(AIA_rsvpResvTSpecRate),
	AIA_rsvpResvTSpecRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvTSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvTSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTSpecRate =
{
	&AI_rsvpResvTSpecRate,
	GET_rsvpResvTSpecRate,
	SET_rsvpResvTSpecRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTSpecRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTSpecRate,
#define MAX_rsvpResvTSpecRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvTSpecRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTSpecPeakRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Peak Bit Rate of the sender's data stream.
 *	Traffic  arrival is not expected to exceed this
 *	rate at any time, apart  from  the  effects  of
 *	jitter in the network.  If not specified in the
 *	TSpec, this returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvTSpecPeakRate is known as 1.3.6.1.2.1.51.1.4.1.15
 */
static READONLY ubyte	AIA_rsvpResvTSpecPeakRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x0f, 
};

static READONLY struct object_id	AI_rsvpResvTSpecPeakRate =
{	sizeof(AIA_rsvpResvTSpecPeakRate),
	AIA_rsvpResvTSpecPeakRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTSpecPeakRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTSpecPeakRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecPeakRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvTSpecPeakRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvTSpecPeakRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecPeakRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTSpecPeakRate =
{
	&AI_rsvpResvTSpecPeakRate,
	GET_rsvpResvTSpecPeakRate,
	SET_rsvpResvTSpecPeakRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTSpecPeakRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTSpecPeakRate,
#define MAX_rsvpResvTSpecPeakRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvTSpecPeakRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTSpecBurst - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The size of the largest  burst  expected  from
 *	the sender at a time.
 *	
 *	If this is less than  the  sender's  advertised
 *	burst  size, the receiver is asking the network
 *	to provide flow pacing  beyond  what  would  be
 *	provided  under normal circumstances. Such pac-
 *	ing is at the network's option.
 *
 ************************************************************************/

/*
 *	rsvpResvTSpecBurst is known as 1.3.6.1.2.1.51.1.4.1.16
 */
static READONLY ubyte	AIA_rsvpResvTSpecBurst[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x10, 
};

static READONLY struct object_id	AI_rsvpResvTSpecBurst =
{	sizeof(AIA_rsvpResvTSpecBurst),
	AIA_rsvpResvTSpecBurst,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTSpecBurst(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTSpecBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecBurst;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvTSpecBurst(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvTSpecBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecBurst = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTSpecBurst =
{
	&AI_rsvpResvTSpecBurst,
	GET_rsvpResvTSpecBurst,
	SET_rsvpResvTSpecBurst,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTSpecBurst	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTSpecBurst,
#define MAX_rsvpResvTSpecBurst	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvTSpecBurst,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTSpecMinTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The minimum message size for  this  flow.  The
 *	policing  algorithm will treat smaller messages
 *	as though they are this size.
 *
 ************************************************************************/

/*
 *	rsvpResvTSpecMinTU is known as 1.3.6.1.2.1.51.1.4.1.17
 */
static READONLY ubyte	AIA_rsvpResvTSpecMinTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x11, 
};

static READONLY struct object_id	AI_rsvpResvTSpecMinTU =
{	sizeof(AIA_rsvpResvTSpecMinTU),
	AIA_rsvpResvTSpecMinTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTSpecMinTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTSpecMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecMinTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvTSpecMinTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvTSpecMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecMinTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTSpecMinTU =
{
	&AI_rsvpResvTSpecMinTU,
	GET_rsvpResvTSpecMinTU,
	SET_rsvpResvTSpecMinTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTSpecMinTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTSpecMinTU,
#define MAX_rsvpResvTSpecMinTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvTSpecMinTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTSpecMaxTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The maximum message size for  this  flow.  The
 *	admission  algorithm  will  reject TSpecs whose
 *	Maximum Transmission Unit, plus  the  interface
 *	headers, exceed the interface MTU.
 *
 ************************************************************************/

/*
 *	rsvpResvTSpecMaxTU is known as 1.3.6.1.2.1.51.1.4.1.18
 */
static READONLY ubyte	AIA_rsvpResvTSpecMaxTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x12, 
};

static READONLY struct object_id	AI_rsvpResvTSpecMaxTU =
{	sizeof(AIA_rsvpResvTSpecMaxTU),
	AIA_rsvpResvTSpecMaxTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTSpecMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTSpecMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecMaxTU;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvTSpecMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvTSpecMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvTSpecMaxTU = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTSpecMaxTU =
{
	&AI_rsvpResvTSpecMaxTU,
	GET_rsvpResvTSpecMaxTU,
	SET_rsvpResvTSpecMaxTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTSpecMaxTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTSpecMaxTU,
#define MAX_rsvpResvTSpecMaxTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvTSpecMaxTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvRSpecRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If the requested  service  is  Guaranteed,  as
 *	specified   by  rsvpResvService,  this  is  the
 *	clearing rate that is being requested.   Other-
 *	wise,  it  is  zero,  or  the  agent may return
 *	noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvRSpecRate is known as 1.3.6.1.2.1.51.1.4.1.19
 */
static READONLY ubyte	AIA_rsvpResvRSpecRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x13, 
};

static READONLY struct object_id	AI_rsvpResvRSpecRate =
{	sizeof(AIA_rsvpResvRSpecRate),
	AIA_rsvpResvRSpecRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvRSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvRSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvRSpecRate;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvRSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvRSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvRSpecRate = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvRSpecRate =
{
	&AI_rsvpResvRSpecRate,
	GET_rsvpResvRSpecRate,
	SET_rsvpResvRSpecRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvRSpecRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvRSpecRate,
#define MAX_rsvpResvRSpecRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvRSpecRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvRSpecSlack - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If the requested  service  is  Guaranteed,  as
 *	specified by rsvpResvService, this is the delay
 *	slack.  Otherwise, it is zero, or the agent may
 *	return noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvRSpecSlack is known as 1.3.6.1.2.1.51.1.4.1.20
 */
static READONLY ubyte	AIA_rsvpResvRSpecSlack[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x14, 
};

static READONLY struct object_id	AI_rsvpResvRSpecSlack =
{	sizeof(AIA_rsvpResvRSpecSlack),
	AIA_rsvpResvRSpecSlack,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvRSpecSlack(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvRSpecSlack(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvRSpecSlack;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvRSpecSlack(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvRSpecSlack(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvRSpecSlack = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvRSpecSlack =
{
	&AI_rsvpResvRSpecSlack,
	GET_rsvpResvRSpecSlack,
	SET_rsvpResvRSpecSlack,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvInterval - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The interval between refresh messages  as  ad-
 *	vertised by the Next Hop.
 *
 ************************************************************************/

/*
 *	rsvpResvInterval is known as 1.3.6.1.2.1.51.1.4.1.21
 */
static READONLY ubyte	AIA_rsvpResvInterval[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x15, 
};

static READONLY struct object_id	AI_rsvpResvInterval =
{	sizeof(AIA_rsvpResvInterval),
	AIA_rsvpResvInterval,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvInterval(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvInterval;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvInterval(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvInterval = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvInterval =
{
	&AI_rsvpResvInterval,
	GET_rsvpResvInterval,
	SET_rsvpResvInterval,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvInterval	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvInterval,
#define MAX_rsvpResvInterval	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvInterval,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvScope - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The contents of the scope object, displayed as
 *	an  uninterpreted  string  of octets, including
 *	the object header.  In the absence of  such  an
 *	object, this should be of zero length.
 *	
 *	If the length  is  non-zero,  this  contains  a
 *	series of IP4 or IP6 addresses.
 *
 ************************************************************************/

/*
 *	rsvpResvScope is known as 1.3.6.1.2.1.51.1.4.1.22
 */
static READONLY ubyte	AIA_rsvpResvScope[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x16, 
};

static READONLY struct object_id	AI_rsvpResvScope =
{	sizeof(AIA_rsvpResvScope),
	AIA_rsvpResvScope,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvScope(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvScope(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvScope;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvScope(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvScope(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvScope.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvScope.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvScope =
{
	&AI_rsvpResvScope,
	GET_rsvpResvScope,
	SET_rsvpResvScope,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvScope	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvScope,
#define MAX_rsvpResvScope	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpResvScope,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvShared - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, a reservation shared among senders is
 *	requested.  If FALSE, a reservation specific to
 *	this sender is requested.
 *
 ************************************************************************/

/*
 *	rsvpResvShared is known as 1.3.6.1.2.1.51.1.4.1.23
 */
static READONLY ubyte	AIA_rsvpResvShared[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x17, 
};

static READONLY struct object_id	AI_rsvpResvShared =
{	sizeof(AIA_rsvpResvShared),
	AIA_rsvpResvShared,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvShared(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvShared(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvShared;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvShared(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvShared(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvShared = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvShared =
{
	&AI_rsvpResvShared,
	GET_rsvpResvShared,
	SET_rsvpResvShared,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvShared	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvShared,
#define MAX_rsvpResvShared	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvShared,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvExplicit - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, individual senders are  listed  using
 *	Filter  Specifications.   If FALSE, all senders
 *	are implicitly selected.  The Scope Object will
 *	contain  a list of senders that need to receive
 *	this reservation request  for  the  purpose  of
 *	routing the RESV message.
 *
 ************************************************************************/

/*
 *	rsvpResvExplicit is known as 1.3.6.1.2.1.51.1.4.1.24
 */
static READONLY ubyte	AIA_rsvpResvExplicit[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x18, 
};

static READONLY struct object_id	AI_rsvpResvExplicit =
{	sizeof(AIA_rsvpResvExplicit),
	AIA_rsvpResvExplicit,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvExplicit(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvExplicit(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvExplicit;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvExplicit(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvExplicit(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvExplicit = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvExplicit =
{
	&AI_rsvpResvExplicit,
	GET_rsvpResvExplicit,
	SET_rsvpResvExplicit,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvExplicit	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvExplicit,
#define MAX_rsvpResvExplicit	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvExplicit,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvRSVPHop - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the node believes that  the  previous
 *	IP  hop is an RSVP hop.  If FALSE, the node be-
 *	lieves that the previous IP hop may not  be  an
 *	RSVP hop.
 *
 ************************************************************************/

/*
 *	rsvpResvRSVPHop is known as 1.3.6.1.2.1.51.1.4.1.25
 */
static READONLY ubyte	AIA_rsvpResvRSVPHop[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x19, 
};

static READONLY struct object_id	AI_rsvpResvRSVPHop =
{	sizeof(AIA_rsvpResvRSVPHop),
	AIA_rsvpResvRSVPHop,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvRSVPHop(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvRSVPHop(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvRSVPHop;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvRSVPHop(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvRSVPHop(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvRSVPHop = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvRSVPHop =
{
	&AI_rsvpResvRSVPHop,
	GET_rsvpResvRSVPHop,
	SET_rsvpResvRSVPHop,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvRSVPHop	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvRSVPHop,
#define MAX_rsvpResvRSVPHop	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvRSVPHop,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvLastChange - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The time of the last change in  this  reserva-
 *	tion  request; This is either the first time it
 *	was received or the time  of  the  most  recent
 *	change in parameters.
 *
 ************************************************************************/

/*
 *	rsvpResvLastChange is known as 1.3.6.1.2.1.51.1.4.1.26
 */
static READONLY ubyte	AIA_rsvpResvLastChange[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x1a, 
};

static READONLY struct object_id	AI_rsvpResvLastChange =
{	sizeof(AIA_rsvpResvLastChange),
	AIA_rsvpResvLastChange,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvLastChange(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvLastChange(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	TimeTicks	*attr = (TimeTicks *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvLastChange;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvLastChange =
{
	&AI_rsvpResvLastChange,
	GET_rsvpResvLastChange,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_TICKS,		/* TimeTicks */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvPolicy - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The contents of the policy  object,  displayed
 *	as an uninterpreted string of octets, including
 *	the object header.  In the absence of  such  an
 *	object, this should be of zero length.
 *
 ************************************************************************/

/*
 *	rsvpResvPolicy is known as 1.3.6.1.2.1.51.1.4.1.27
 */
static READONLY ubyte	AIA_rsvpResvPolicy[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x1b, 
};

static READONLY struct object_id	AI_rsvpResvPolicy =
{	sizeof(AIA_rsvpResvPolicy),
	AIA_rsvpResvPolicy,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvPolicy(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvPolicy(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvPolicy;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvPolicy(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvPolicy(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr;

	attr = (OCTETSTRING *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvPolicy.len = attr->len;
	ubcopy(attr->val,
		((rsvpResvEntry_t *)ctxt)->rsvpResvPolicy.val,
		attr->len);
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvPolicy =
{
	&AI_rsvpResvPolicy,
	GET_rsvpResvPolicy,
	SET_rsvpResvPolicy,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvPolicy	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvPolicy,
#define MAX_rsvpResvPolicy	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpResvPolicy,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all active RESV  messages.   This
 *	object  may  be used to install static RESV in-
 *	formation or delete RESV information.
 *
 ************************************************************************/

/*
 *	rsvpResvStatus is known as 1.3.6.1.2.1.51.1.4.1.28
 */
static READONLY ubyte	AIA_rsvpResvStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x1c, 
};

static READONLY struct object_id	AI_rsvpResvStatus =
{	sizeof(AIA_rsvpResvStatus),
	AIA_rsvpResvStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvEntry_t *)ctxt)->rsvpResvStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvStatus =
{
	&AI_rsvpResvStatus,
	GET_rsvpResvStatus,
	SET_rsvpResvStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvStatus,
#define MAX_rsvpResvStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpResvStatus,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvTTL - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The TTL value in the RSVP header that was last
 *	received.
 *
 ************************************************************************/

/*
 *	rsvpResvTTL is known as 1.3.6.1.2.1.51.1.4.1.29
 */
static READONLY ubyte	AIA_rsvpResvTTL[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x1d, 
};

static READONLY struct object_id	AI_rsvpResvTTL =
{	sizeof(AIA_rsvpResvTTL),
	AIA_rsvpResvTTL,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvTTL(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvTTL(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvTTL;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvTTL =
{
	&AI_rsvpResvTTL,
	GET_rsvpResvTTL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvTTL	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvTTL,
#define MAX_rsvpResvTTL	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvTTL,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFlowId - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The flow ID that this receiver  is  using,  if
 *	this  is  an IPv6 session.
 *
 ************************************************************************/

/*
 *	rsvpResvFlowId is known as 1.3.6.1.2.1.51.1.4.1.30
 */
static READONLY ubyte	AIA_rsvpResvFlowId[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 0x1e, 
};

static READONLY struct object_id	AI_rsvpResvFlowId =
{	sizeof(AIA_rsvpResvFlowId),
	AIA_rsvpResvFlowId,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFlowId(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFlowId(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvEntry_t *)ctxt)->rsvpResvFlowId;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFlowId =
{
	&AI_rsvpResvFlowId,
	GET_rsvpResvFlowId,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvEntry,
	peer_locate_rsvpResvEntry,
	IX_rsvpResvTable_rsvpResvEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFlowId	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFlowId,
#define MAX_rsvpResvFlowId	PEER_PORTABLE_ULONG(0xffffff)
	MAX_rsvpResvFlowId,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpResvEntry[] =
{
	&A_rsvpResvNumber,
	&A_rsvpResvType,
	&A_rsvpResvDestAddr,
	&A_rsvpResvSenderAddr,
	&A_rsvpResvDestAddrLength,
	&A_rsvpResvSenderAddrLength,
	&A_rsvpResvProtocol,
	&A_rsvpResvDestPort,
	&A_rsvpResvPort,
	&A_rsvpResvHopAddr,
	&A_rsvpResvHopLih,
	&A_rsvpResvInterface,
	&A_rsvpResvService,
	&A_rsvpResvTSpecRate,
	&A_rsvpResvTSpecPeakRate,
	&A_rsvpResvTSpecBurst,
	&A_rsvpResvTSpecMinTU,
	&A_rsvpResvTSpecMaxTU,
	&A_rsvpResvRSpecRate,
	&A_rsvpResvRSpecSlack,
	&A_rsvpResvInterval,
	&A_rsvpResvScope,
	&A_rsvpResvShared,
	&A_rsvpResvExplicit,
	&A_rsvpResvRSVPHop,
	&A_rsvpResvLastChange,
	&A_rsvpResvPolicy,
	&A_rsvpResvStatus,
	&A_rsvpResvTTL,
	&A_rsvpResvFlowId,
	(struct attribute READONLY *) NULL
};

#define A_rsvpResvTable_rsvpSessionNumber A_rsvpSessionNumber
#define A_rsvpResvTable_rsvpResvNumber A_rsvpResvNumber


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpResvTable_rsvpResvEntry[] =
{
	{ 0 + 0,  &A_rsvpResvTable_rsvpSessionNumber	},
	{ 0 + 0,  &A_rsvpResvTable_rsvpResvNumber	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpResvEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 0x01, 
};

static READONLY struct object_id	GI_rsvpResvEntry =
{
	9,
	GIA_rsvpResvEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpResvEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpResvEntry =
{
	SNMP_CLASS,
	&GI_rsvpResvEntry,
	1,
	AS_rsvpResvEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpResvTable_rsvpResvEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpResvEntry = 
{
	&SMI_GROUP_rsvpResvEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpResvTable[] =
{
	&CG_rsvpResvEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpResvTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpResvTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x04, 
};

static READONLY struct object_id	GI_rsvpResvTable =
{
	8,
	GIA_rsvpResvTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpResvTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpResvTable =
{
	SNMP_CLASS,
	&GI_rsvpResvTable,
	1,
	AS_rsvpResvTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpResvTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpResvTable = 
{
	&SMI_GROUP_rsvpResvTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpResvFwdNumber - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of this reservation request.   This
 *	is  for  SNMP Indexing purposes only and has no
 *	relation to any protocol value.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdNumber is known as 1.3.6.1.2.1.51.1.5.1.1
 */
static READONLY ubyte	AIA_rsvpResvFwdNumber[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpResvFwdNumber =
{	sizeof(AIA_rsvpResvFwdNumber),
	AIA_rsvpResvFwdNumber,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdNumber(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdNumber(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdNumber;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
extern READONLY struct index_entry	IX_rsvpResvFwdTable_rsvpResvFwdEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdNumber =
{
	&AI_rsvpResvFwdNumber,
	GET_rsvpResvFwdNumber,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdNumber	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdNumber,
#define MAX_rsvpResvFwdNumber	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdNumber,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdType - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The type of session (IP4, IP6, IP6  with  flow
 *	information, etc).
 *
 ************************************************************************/

/*
 *	rsvpResvFwdType is known as 1.3.6.1.2.1.51.1.5.1.2
 */
static READONLY ubyte	AIA_rsvpResvFwdType[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpResvFwdType =
{	sizeof(AIA_rsvpResvFwdType),
	AIA_rsvpResvFwdType,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdType(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdType(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdType;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdType =
{
	&AI_rsvpResvFwdType,
	GET_rsvpResvFwdType,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdType	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdType,
#define MAX_rsvpResvFwdType	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvFwdType,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdDestAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The destination address used by all senders in
 *	this  session.   This object may not be changed
 *	when the value of the RowStatus object is  'ac-
 *	tive'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdDestAddr is known as 1.3.6.1.2.1.51.1.5.1.3
 */
static READONLY ubyte	AIA_rsvpResvFwdDestAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpResvFwdDestAddr =
{	sizeof(AIA_rsvpResvFwdDestAddr),
	AIA_rsvpResvFwdDestAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdDestAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdDestAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdDestAddr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdDestAddr =
{
	&AI_rsvpResvFwdDestAddr,
	GET_rsvpResvFwdDestAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdDestAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvFwdDestAddr,
#define MAX_rsvpResvFwdDestAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvFwdDestAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdSenderAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The source address of the sender  selected  by
 *	this  reservation.  The value of all zeroes in-
 *	dicates 'all senders'.  This object may not  be
 *	changed  when the value of the RowStatus object
 *	is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdSenderAddr is known as 1.3.6.1.2.1.51.1.5.1.4
 */
static READONLY ubyte	AIA_rsvpResvFwdSenderAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x04, 
};

static READONLY struct object_id	AI_rsvpResvFwdSenderAddr =
{	sizeof(AIA_rsvpResvFwdSenderAddr),
	AIA_rsvpResvFwdSenderAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdSenderAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdSenderAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdSenderAddr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdSenderAddr =
{
	&AI_rsvpResvFwdSenderAddr,
	GET_rsvpResvFwdSenderAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdSenderAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvFwdSenderAddr,
#define MAX_rsvpResvFwdSenderAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvFwdSenderAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdDestAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the destination address in bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdDestAddrLength is known as 1.3.6.1.2.1.51.1.5.1.5
 */
static READONLY ubyte	AIA_rsvpResvFwdDestAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x05, 
};

static READONLY struct object_id	AI_rsvpResvFwdDestAddrLength =
{	sizeof(AIA_rsvpResvFwdDestAddrLength),
	AIA_rsvpResvFwdDestAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdDestAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdDestAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdDestAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdDestAddrLength =
{
	&AI_rsvpResvFwdDestAddrLength,
	GET_rsvpResvFwdDestAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdDestAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdDestAddrLength,
#define MAX_rsvpResvFwdDestAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpResvFwdDestAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdSenderAddrLength - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The length of the sender's  address  in  bits.
 *	This  is  the CIDR Prefix Length, which for IP4
 *	hosts and multicast addresses is 32 bits.  This
 *	object may not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdSenderAddrLength is known as 1.3.6.1.2.1.51.1.5.1.6
 */
static READONLY ubyte	AIA_rsvpResvFwdSenderAddrLength[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x06, 
};

static READONLY struct object_id	AI_rsvpResvFwdSenderAddrLength =
{	sizeof(AIA_rsvpResvFwdSenderAddrLength),
	AIA_rsvpResvFwdSenderAddrLength,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdSenderAddrLength(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdSenderAddrLength(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdSenderAddrLength;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdSenderAddrLength =
{
	&AI_rsvpResvFwdSenderAddrLength,
	GET_rsvpResvFwdSenderAddrLength,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdSenderAddrLength	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdSenderAddrLength,
#define MAX_rsvpResvFwdSenderAddrLength	PEER_PORTABLE_ULONG(0x80)
	MAX_rsvpResvFwdSenderAddrLength,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP Protocol used by a session. for  secure
 *	sessions, this indicates IP Security.  This ob-
 *	ject may not be changed when the value  of  the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdProtocol is known as 1.3.6.1.2.1.51.1.5.1.7
 */
static READONLY ubyte	AIA_rsvpResvFwdProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x07, 
};

static READONLY struct object_id	AI_rsvpResvFwdProtocol =
{	sizeof(AIA_rsvpResvFwdProtocol),
	AIA_rsvpResvFwdProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdProtocol;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdProtocol =
{
	&AI_rsvpResvFwdProtocol,
	GET_rsvpResvFwdProtocol,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdProtocol,
#define MAX_rsvpResvFwdProtocol	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvFwdProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdDestPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used as a  destina-
 *	tion  port for all senders in this session.  If
 *	
 *	
 *	
 *	the  IP   protocol   in   use,   specified   by
 *	rsvpResvFwdProtocol,  is  50  (ESP) or 51 (AH),
 *	this  represents  a  virtual  destination  port
 *	number.   A value of zero indicates that the IP
 *	protocol in use does not have ports.  This  ob-
 *	ject  may  not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdDestPort is known as 1.3.6.1.2.1.51.1.5.1.8
 */
static READONLY ubyte	AIA_rsvpResvFwdDestPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x08, 
};

static READONLY struct object_id	AI_rsvpResvFwdDestPort =
{	sizeof(AIA_rsvpResvFwdDestPort),
	AIA_rsvpResvFwdDestPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdDestPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdDestPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdDestPort;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdDestPort =
{
	&AI_rsvpResvFwdDestPort,
	GET_rsvpResvFwdDestPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdDestPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpResvFwdDestPort,
#define MAX_rsvpResvFwdDestPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpResvFwdDestPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdPort - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The UDP or TCP port number used  as  a  source
 *	port  for  this sender in this session.  If the
 *	IP    protocol    in    use,    specified    by
 *	rsvpResvFwdProtocol  is  50  (ESP)  or 51 (AH),
 *	this represents a generalized  port  identifier
 *	(GPI).   A  value of zero indicates that the IP
 *	protocol in use does not have ports.  This  ob-
 *	ject  may  not be changed when the value of the
 *	RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdPort is known as 1.3.6.1.2.1.51.1.5.1.9
 */
static READONLY ubyte	AIA_rsvpResvFwdPort[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x09, 
};

static READONLY struct object_id	AI_rsvpResvFwdPort =
{	sizeof(AIA_rsvpResvFwdPort),
	AIA_rsvpResvFwdPort,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdPort(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdPort(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdPort;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdPort =
{
	&AI_rsvpResvFwdPort,
	GET_rsvpResvFwdPort,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdPort	PEER_PORTABLE_ULONG(0x2)
	MIN_rsvpResvFwdPort,
#define MAX_rsvpResvFwdPort	PEER_PORTABLE_ULONG(0x4)
	MAX_rsvpResvFwdPort,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdHopAddr - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The address of the (previous) RSVP  that  will
 *	receive this message.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdHopAddr is known as 1.3.6.1.2.1.51.1.5.1.10
 */
static READONLY ubyte	AIA_rsvpResvFwdHopAddr[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0a, 
};

static READONLY struct object_id	AI_rsvpResvFwdHopAddr =
{	sizeof(AIA_rsvpResvFwdHopAddr),
	AIA_rsvpResvFwdHopAddr,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdHopAddr(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdHopAddr(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdHopAddr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdHopAddr =
{
	&AI_rsvpResvFwdHopAddr,
	GET_rsvpResvFwdHopAddr,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdHopAddr	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpResvFwdHopAddr,
#define MAX_rsvpResvFwdHopAddr	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpResvFwdHopAddr,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdHopLih - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Logical Interface Handle sent to the (pre-
 *	vious) RSVP that will receive this message.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdHopLih is known as 1.3.6.1.2.1.51.1.5.1.11
 */
static READONLY ubyte	AIA_rsvpResvFwdHopLih[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0b, 
};

static READONLY struct object_id	AI_rsvpResvFwdHopLih =
{	sizeof(AIA_rsvpResvFwdHopLih),
	AIA_rsvpResvFwdHopLih,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdHopLih(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdHopLih(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdHopLih;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdHopLih =
{
	&AI_rsvpResvFwdHopLih,
	GET_rsvpResvFwdHopLih,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdInterface - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The ifIndex value of the  interface  on  which
 *	this RESV message was most recently sent.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdInterface is known as 1.3.6.1.2.1.51.1.5.1.12
 */
static READONLY ubyte	AIA_rsvpResvFwdInterface[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0c, 
};

static READONLY struct object_id	AI_rsvpResvFwdInterface =
{	sizeof(AIA_rsvpResvFwdInterface),
	AIA_rsvpResvFwdInterface,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdInterface(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdInterface(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdInterface;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdInterface =
{
	&AI_rsvpResvFwdInterface,
	GET_rsvpResvFwdInterface,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdService - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The QoS Service classification requested.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdService is known as 1.3.6.1.2.1.51.1.5.1.13
 */
static READONLY ubyte	AIA_rsvpResvFwdService[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0d, 
};

static READONLY struct object_id	AI_rsvpResvFwdService =
{	sizeof(AIA_rsvpResvFwdService),
	AIA_rsvpResvFwdService,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdService(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdService(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdService;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdService =
{
	&AI_rsvpResvFwdService,
	GET_rsvpResvFwdService,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdService	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdService,
#define MAX_rsvpResvFwdService	PEER_PORTABLE_ULONG(0x5)
	MAX_rsvpResvFwdService,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTSpecRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Average Bit  Rate  of  the  sender's  data
 *	stream.   Within  a transmission burst, the ar-
 *	rival rate may be as fast as  rsvpResvFwdTSpec-
 *	PeakRate  (if  supported by the service model);
 *	however, averaged across two or more burst  in-
 *	tervals,    the    rate   should   not   exceed
 *	rsvpResvFwdTSpecRate.
 *	
 *	Note that this is a prediction, often based  on
 *	the  general  capability  of a type of codec or
 *	particular encoding; the measured average  rate
 *	may be significantly lower.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTSpecRate is known as 1.3.6.1.2.1.51.1.5.1.14
 */
static READONLY ubyte	AIA_rsvpResvFwdTSpecRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0e, 
};

static READONLY struct object_id	AI_rsvpResvFwdTSpecRate =
{	sizeof(AIA_rsvpResvFwdTSpecRate),
	AIA_rsvpResvFwdTSpecRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTSpecRate;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTSpecRate =
{
	&AI_rsvpResvFwdTSpecRate,
	GET_rsvpResvFwdTSpecRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTSpecRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTSpecRate,
#define MAX_rsvpResvFwdTSpecRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdTSpecRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTSpecPeakRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The Peak Bit Rate of the sender's data  stream
 *	Traffic  arrival is not expected to exceed this
 *	rate at any time, apart  from  the  effects  of
 *	
 *	
 *	
 *	jitter in the network.  If not specified in the
 *	TSpec, this returns zero or noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTSpecPeakRate is known as 1.3.6.1.2.1.51.1.5.1.15
 */
static READONLY ubyte	AIA_rsvpResvFwdTSpecPeakRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x0f, 
};

static READONLY struct object_id	AI_rsvpResvFwdTSpecPeakRate =
{	sizeof(AIA_rsvpResvFwdTSpecPeakRate),
	AIA_rsvpResvFwdTSpecPeakRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTSpecPeakRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTSpecPeakRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTSpecPeakRate;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTSpecPeakRate =
{
	&AI_rsvpResvFwdTSpecPeakRate,
	GET_rsvpResvFwdTSpecPeakRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTSpecPeakRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTSpecPeakRate,
#define MAX_rsvpResvFwdTSpecPeakRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdTSpecPeakRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTSpecBurst - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The size of the largest  burst  expected  from
 *	the sender at a time.
 *	
 *	If this is less than  the  sender's  advertised
 *	burst  size, the receiver is asking the network
 *	to provide flow pacing  beyond  what  would  be
 *	provided  under normal circumstances. Such pac-
 *	ing is at the network's option.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTSpecBurst is known as 1.3.6.1.2.1.51.1.5.1.16
 */
static READONLY ubyte	AIA_rsvpResvFwdTSpecBurst[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x10, 
};

static READONLY struct object_id	AI_rsvpResvFwdTSpecBurst =
{	sizeof(AIA_rsvpResvFwdTSpecBurst),
	AIA_rsvpResvFwdTSpecBurst,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTSpecBurst(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTSpecBurst(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTSpecBurst;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTSpecBurst =
{
	&AI_rsvpResvFwdTSpecBurst,
	GET_rsvpResvFwdTSpecBurst,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTSpecBurst	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTSpecBurst,
#define MAX_rsvpResvFwdTSpecBurst	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdTSpecBurst,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTSpecMinTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The minimum message size for  this  flow.  The
 *	policing  algorithm will treat smaller messages
 *	as though they are this size.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTSpecMinTU is known as 1.3.6.1.2.1.51.1.5.1.17
 */
static READONLY ubyte	AIA_rsvpResvFwdTSpecMinTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x11, 
};

static READONLY struct object_id	AI_rsvpResvFwdTSpecMinTU =
{	sizeof(AIA_rsvpResvFwdTSpecMinTU),
	AIA_rsvpResvFwdTSpecMinTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTSpecMinTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTSpecMinTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTSpecMinTU;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTSpecMinTU =
{
	&AI_rsvpResvFwdTSpecMinTU,
	GET_rsvpResvFwdTSpecMinTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTSpecMinTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTSpecMinTU,
#define MAX_rsvpResvFwdTSpecMinTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdTSpecMinTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTSpecMaxTU - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The maximum message size for  this  flow.  The
 *	admission  algorithm  will  reject TSpecs whose
 *	Maximum Transmission Unit, plus  the  interface
 *	headers, exceed the interface MTU.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTSpecMaxTU is known as 1.3.6.1.2.1.51.1.5.1.18
 */
static READONLY ubyte	AIA_rsvpResvFwdTSpecMaxTU[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x12, 
};

static READONLY struct object_id	AI_rsvpResvFwdTSpecMaxTU =
{	sizeof(AIA_rsvpResvFwdTSpecMaxTU),
	AIA_rsvpResvFwdTSpecMaxTU,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTSpecMaxTU(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTSpecMaxTU(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTSpecMaxTU;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTSpecMaxTU =
{
	&AI_rsvpResvFwdTSpecMaxTU,
	GET_rsvpResvFwdTSpecMaxTU,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTSpecMaxTU	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTSpecMaxTU,
#define MAX_rsvpResvFwdTSpecMaxTU	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdTSpecMaxTU,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdRSpecRate - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If the requested  service  is  Guaranteed,  as
 *	specified   by  rsvpResvService,  this  is  the
 *	clearing rate that is being requested.   Other-
 *	wise,  it  is  zero,  or  the  agent may return
 *	noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdRSpecRate is known as 1.3.6.1.2.1.51.1.5.1.19
 */
static READONLY ubyte	AIA_rsvpResvFwdRSpecRate[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x13, 
};

static READONLY struct object_id	AI_rsvpResvFwdRSpecRate =
{	sizeof(AIA_rsvpResvFwdRSpecRate),
	AIA_rsvpResvFwdRSpecRate,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdRSpecRate(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdRSpecRate(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdRSpecRate;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdRSpecRate =
{
	&AI_rsvpResvFwdRSpecRate,
	GET_rsvpResvFwdRSpecRate,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdRSpecRate	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdRSpecRate,
#define MAX_rsvpResvFwdRSpecRate	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdRSpecRate,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdRSpecSlack - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If the requested  service  is  Guaranteed,  as
 *	specified by rsvpResvService, this is the delay
 *	slack.  Otherwise, it is zero, or the agent may
 *	return noSuchValue.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdRSpecSlack is known as 1.3.6.1.2.1.51.1.5.1.20
 */
static READONLY ubyte	AIA_rsvpResvFwdRSpecSlack[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x14, 
};

static READONLY struct object_id	AI_rsvpResvFwdRSpecSlack =
{	sizeof(AIA_rsvpResvFwdRSpecSlack),
	AIA_rsvpResvFwdRSpecSlack,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdRSpecSlack(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdRSpecSlack(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdRSpecSlack;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdRSpecSlack =
{
	&AI_rsvpResvFwdRSpecSlack,
	GET_rsvpResvFwdRSpecSlack,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
	PEER_PORTABLE_ULONG(0x80000000),
	PEER_PORTABLE_ULONG(0x7fffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdInterval - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The interval between refresh  messages  adver-
 *	tised to the Previous Hop.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdInterval is known as 1.3.6.1.2.1.51.1.5.1.21
 */
static READONLY ubyte	AIA_rsvpResvFwdInterval[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x15, 
};

static READONLY struct object_id	AI_rsvpResvFwdInterval =
{	sizeof(AIA_rsvpResvFwdInterval),
	AIA_rsvpResvFwdInterval,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdInterval(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdInterval;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdInterval =
{
	&AI_rsvpResvFwdInterval,
	GET_rsvpResvFwdInterval,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdInterval	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdInterval,
#define MAX_rsvpResvFwdInterval	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdInterval,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdScope - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The contents of the scope object, displayed as
 *	an  uninterpreted  string  of octets, including
 *	the object header.  In the absence of  such  an
 *	object, this should be of zero length.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdScope is known as 1.3.6.1.2.1.51.1.5.1.22
 */
static READONLY ubyte	AIA_rsvpResvFwdScope[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x16, 
};

static READONLY struct object_id	AI_rsvpResvFwdScope =
{	sizeof(AIA_rsvpResvFwdScope),
	AIA_rsvpResvFwdScope,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdScope(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdScope(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdScope;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdScope =
{
	&AI_rsvpResvFwdScope,
	GET_rsvpResvFwdScope,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdScope	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdScope,
#define MAX_rsvpResvFwdScope	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpResvFwdScope,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdShared - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, a reservation shared among senders is
 *	requested.  If FALSE, a reservation specific to
 *	this sender is requested.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdShared is known as 1.3.6.1.2.1.51.1.5.1.23
 */
static READONLY ubyte	AIA_rsvpResvFwdShared[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x17, 
};

static READONLY struct object_id	AI_rsvpResvFwdShared =
{	sizeof(AIA_rsvpResvFwdShared),
	AIA_rsvpResvFwdShared,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdShared(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdShared(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdShared;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdShared =
{
	&AI_rsvpResvFwdShared,
	GET_rsvpResvFwdShared,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdShared	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdShared,
#define MAX_rsvpResvFwdShared	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvFwdShared,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdExplicit - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, individual senders are  listed  using
 *	Filter  Specifications.   If FALSE, all senders
 *	are implicitly selected.  The Scope Object will
 *	contain  a list of senders that need to receive
 *	this reservation request  for  the  purpose  of
 *	routing the RESV message.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdExplicit is known as 1.3.6.1.2.1.51.1.5.1.24
 */
static READONLY ubyte	AIA_rsvpResvFwdExplicit[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x18, 
};

static READONLY struct object_id	AI_rsvpResvFwdExplicit =
{	sizeof(AIA_rsvpResvFwdExplicit),
	AIA_rsvpResvFwdExplicit,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdExplicit(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdExplicit(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdExplicit;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdExplicit =
{
	&AI_rsvpResvFwdExplicit,
	GET_rsvpResvFwdExplicit,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdExplicit	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdExplicit,
#define MAX_rsvpResvFwdExplicit	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvFwdExplicit,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdRSVPHop - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, the node believes that  the  next  IP
 *	hop  is  an  RSVP  hop.  If FALSE, the node be-
 *	lieves that the next IP hop may not be an  RSVP
 *	hop.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdRSVPHop is known as 1.3.6.1.2.1.51.1.5.1.25
 */
static READONLY ubyte	AIA_rsvpResvFwdRSVPHop[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x19, 
};

static READONLY struct object_id	AI_rsvpResvFwdRSVPHop =
{	sizeof(AIA_rsvpResvFwdRSVPHop),
	AIA_rsvpResvFwdRSVPHop,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdRSVPHop(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdRSVPHop(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdRSVPHop;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdRSVPHop =
{
	&AI_rsvpResvFwdRSVPHop,
	GET_rsvpResvFwdRSVPHop,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdRSVPHop	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdRSVPHop,
#define MAX_rsvpResvFwdRSVPHop	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpResvFwdRSVPHop,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdLastChange - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The time of the last change in  this  request;
 *	This  is  either  the first time it was sent or
 *	the time of the most recent change  in  parame-
 *	ters.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdLastChange is known as 1.3.6.1.2.1.51.1.5.1.26
 */
static READONLY ubyte	AIA_rsvpResvFwdLastChange[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x1a, 
};

static READONLY struct object_id	AI_rsvpResvFwdLastChange =
{	sizeof(AIA_rsvpResvFwdLastChange),
	AIA_rsvpResvFwdLastChange,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdLastChange(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdLastChange(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	TimeTicks	*attr = (TimeTicks *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdLastChange;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdLastChange =
{
	&AI_rsvpResvFwdLastChange,
	GET_rsvpResvFwdLastChange,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_TICKS,		/* TimeTicks */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdPolicy - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The contents of the policy  object,  displayed
 *	as an uninterpreted string of octets, including
 *	the object header.  In the absence of  such  an
 *	object, this should be of zero length.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdPolicy is known as 1.3.6.1.2.1.51.1.5.1.27
 */
static READONLY ubyte	AIA_rsvpResvFwdPolicy[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x1b, 
};

static READONLY struct object_id	AI_rsvpResvFwdPolicy =
{	sizeof(AIA_rsvpResvFwdPolicy),
	AIA_rsvpResvFwdPolicy,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdPolicy(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdPolicy(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdPolicy;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdPolicy =
{
	&AI_rsvpResvFwdPolicy,
	GET_rsvpResvFwdPolicy,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpResvFwdPolicy	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdPolicy,
#define MAX_rsvpResvFwdPolicy	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpResvFwdPolicy,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all active RESV  messages.   This
 *	object may be used to delete RESV information.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdStatus is known as 1.3.6.1.2.1.51.1.5.1.28
 */
static READONLY ubyte	AIA_rsvpResvFwdStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x1c, 
};

static READONLY struct object_id	AI_rsvpResvFwdStatus =
{	sizeof(AIA_rsvpResvFwdStatus),
	AIA_rsvpResvFwdStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvFwdStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvFwdStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdStatus =
{
	&AI_rsvpResvFwdStatus,
	GET_rsvpResvFwdStatus,
	SET_rsvpResvFwdStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpResvFwdStatus,
#define MAX_rsvpResvFwdStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpResvFwdStatus,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdTTL - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The TTL value in the RSVP header that was last
 *	received.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdTTL is known as 1.3.6.1.2.1.51.1.5.1.29
 */
static READONLY ubyte	AIA_rsvpResvFwdTTL[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x1d, 
};

static READONLY struct object_id	AI_rsvpResvFwdTTL =
{	sizeof(AIA_rsvpResvFwdTTL),
	AIA_rsvpResvFwdTTL,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdTTL(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdTTL(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdTTL;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdTTL =
{
	&AI_rsvpResvFwdTTL,
	GET_rsvpResvFwdTTL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdTTL	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdTTL,
#define MAX_rsvpResvFwdTTL	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpResvFwdTTL,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdFlowId - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The flow ID that this receiver  is  using,  if
 *	this  is  an IPv6 session.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdFlowId is known as 1.3.6.1.2.1.51.1.5.1.30
 */
static READONLY ubyte	AIA_rsvpResvFwdFlowId[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 0x1e, 
};

static READONLY struct object_id	AI_rsvpResvFwdFlowId =
{	sizeof(AIA_rsvpResvFwdFlowId),
	AIA_rsvpResvFwdFlowId,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdFlowId(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdFlowId(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpResvFwdEntry_t *)ctxt)->rsvpResvFwdFlowId;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpResvFwdEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpResvFwdEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpResvFwdEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpResvFwdEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdFlowId =
{
	&AI_rsvpResvFwdFlowId,
	GET_rsvpResvFwdFlowId,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpResvFwdEntry,
	peer_locate_rsvpResvFwdEntry,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdFlowId	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdFlowId,
#define MAX_rsvpResvFwdFlowId	PEER_PORTABLE_ULONG(0xffffff)
	MAX_rsvpResvFwdFlowId,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpResvFwdEntry[] =
{
	&A_rsvpResvFwdNumber,
	&A_rsvpResvFwdType,
	&A_rsvpResvFwdDestAddr,
	&A_rsvpResvFwdSenderAddr,
	&A_rsvpResvFwdDestAddrLength,
	&A_rsvpResvFwdSenderAddrLength,
	&A_rsvpResvFwdProtocol,
	&A_rsvpResvFwdDestPort,
	&A_rsvpResvFwdPort,
	&A_rsvpResvFwdHopAddr,
	&A_rsvpResvFwdHopLih,
	&A_rsvpResvFwdInterface,
	&A_rsvpResvFwdService,
	&A_rsvpResvFwdTSpecRate,
	&A_rsvpResvFwdTSpecPeakRate,
	&A_rsvpResvFwdTSpecBurst,
	&A_rsvpResvFwdTSpecMinTU,
	&A_rsvpResvFwdTSpecMaxTU,
	&A_rsvpResvFwdRSpecRate,
	&A_rsvpResvFwdRSpecSlack,
	&A_rsvpResvFwdInterval,
	&A_rsvpResvFwdScope,
	&A_rsvpResvFwdShared,
	&A_rsvpResvFwdExplicit,
	&A_rsvpResvFwdRSVPHop,
	&A_rsvpResvFwdLastChange,
	&A_rsvpResvFwdPolicy,
	&A_rsvpResvFwdStatus,
	&A_rsvpResvFwdTTL,
	&A_rsvpResvFwdFlowId,
	(struct attribute READONLY *) NULL
};

#define A_rsvpResvFwdTable_rsvpSessionNumber A_rsvpSessionNumber
#define A_rsvpResvFwdTable_rsvpResvFwdNumber A_rsvpResvFwdNumber


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpResvFwdTable_rsvpResvFwdEntry[] =
{
	{ 0 + 0,  &A_rsvpResvFwdTable_rsvpSessionNumber	},
	{ 0 + 0,  &A_rsvpResvFwdTable_rsvpResvFwdNumber	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpResvFwdEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 0x01, 
};

static READONLY struct object_id	GI_rsvpResvFwdEntry =
{
	9,
	GIA_rsvpResvFwdEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpResvFwdEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpResvFwdEntry =
{
	SNMP_CLASS,
	&GI_rsvpResvFwdEntry,
	1,
	AS_rsvpResvFwdEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpResvFwdTable_rsvpResvFwdEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpResvFwdEntry = 
{
	&SMI_GROUP_rsvpResvFwdEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpResvFwdTable[] =
{
	&CG_rsvpResvFwdEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpResvFwdTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpResvFwdTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x05, 
};

static READONLY struct object_id	GI_rsvpResvFwdTable =
{
	8,
	GIA_rsvpResvFwdTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpResvFwdTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpResvFwdTable =
{
	SNMP_CLASS,
	&GI_rsvpResvFwdTable,
	1,
	AS_rsvpResvFwdTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpResvFwdTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpResvFwdTable = 
{
	&SMI_GROUP_rsvpResvFwdTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpIfUdpNbrs - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of neighbors perceived to be  using
 *	only the RSVP UDP Encapsulation.
 *
 ************************************************************************/

/*
 *	rsvpIfUdpNbrs is known as 1.3.6.1.2.1.51.1.6.1.1
 */
static READONLY ubyte	AIA_rsvpIfUdpNbrs[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpIfUdpNbrs =
{	sizeof(AIA_rsvpIfUdpNbrs),
	AIA_rsvpIfUdpNbrs,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfUdpNbrs(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfUdpNbrs(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfUdpNbrs;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif
extern READONLY struct index_entry	IX_rsvpIfTable_rsvpIfEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfUdpNbrs =
{
	&AI_rsvpIfUdpNbrs,
	GET_rsvpIfUdpNbrs,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfIpNbrs - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of neighbors perceived to be  using
 *	only the RSVP IP Encapsulation.
 *
 ************************************************************************/

/*
 *	rsvpIfIpNbrs is known as 1.3.6.1.2.1.51.1.6.1.2
 */
static READONLY ubyte	AIA_rsvpIfIpNbrs[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpIfIpNbrs =
{	sizeof(AIA_rsvpIfIpNbrs),
	AIA_rsvpIfIpNbrs,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfIpNbrs(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfIpNbrs(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfIpNbrs;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfIpNbrs =
{
	&AI_rsvpIfIpNbrs,
	GET_rsvpIfIpNbrs,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfNbrs - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The number of neighbors  currently  perceived;
 *	this  will  exceed rsvpIfIpNbrs + rsvpIfUdpNbrs
 *	by the number of neighbors using both  encapsu-
 *	lations.
 *
 ************************************************************************/

/*
 *	rsvpIfNbrs is known as 1.3.6.1.2.1.51.1.6.1.3
 */
static READONLY ubyte	AIA_rsvpIfNbrs[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpIfNbrs =
{	sizeof(AIA_rsvpIfNbrs),
	AIA_rsvpIfNbrs,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfNbrs(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfNbrs(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfNbrs;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfNbrs =
{
	&AI_rsvpIfNbrs,
	GET_rsvpIfNbrs,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfRefreshBlockadeMultiple - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The value of the RSVP value 'Kb', Which is the
 *	minimum   number   of  refresh  intervals  that
 *	blockade state will last once entered.
 *
 ************************************************************************/

/*
 *	rsvpIfRefreshBlockadeMultiple is known as 1.3.6.1.2.1.51.1.6.1.4
 */
static READONLY ubyte	AIA_rsvpIfRefreshBlockadeMultiple[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x04, 
};

static READONLY struct object_id	AI_rsvpIfRefreshBlockadeMultiple =
{	sizeof(AIA_rsvpIfRefreshBlockadeMultiple),
	AIA_rsvpIfRefreshBlockadeMultiple,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfRefreshBlockadeMultiple(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfRefreshBlockadeMultiple(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshBlockadeMultiple;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfRefreshBlockadeMultiple(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfRefreshBlockadeMultiple(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshBlockadeMultiple = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfRefreshBlockadeMultiple =
{
	&AI_rsvpIfRefreshBlockadeMultiple,
	GET_rsvpIfRefreshBlockadeMultiple,
	SET_rsvpIfRefreshBlockadeMultiple,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfRefreshBlockadeMultiple	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpIfRefreshBlockadeMultiple,
#define MAX_rsvpIfRefreshBlockadeMultiple	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpIfRefreshBlockadeMultiple,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfRefreshMultiple - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The value of the RSVP value 'K', which is  the
 *	number  of  refresh intervals which must elapse
 *	(minimum) before a PATH or RESV  message  which
 *	is not being refreshed will be aged out.
 *
 ************************************************************************/

/*
 *	rsvpIfRefreshMultiple is known as 1.3.6.1.2.1.51.1.6.1.5
 */
static READONLY ubyte	AIA_rsvpIfRefreshMultiple[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x05, 
};

static READONLY struct object_id	AI_rsvpIfRefreshMultiple =
{	sizeof(AIA_rsvpIfRefreshMultiple),
	AIA_rsvpIfRefreshMultiple,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfRefreshMultiple(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfRefreshMultiple(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshMultiple;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfRefreshMultiple(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfRefreshMultiple(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshMultiple = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfRefreshMultiple =
{
	&AI_rsvpIfRefreshMultiple,
	GET_rsvpIfRefreshMultiple,
	SET_rsvpIfRefreshMultiple,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfRefreshMultiple	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpIfRefreshMultiple,
#define MAX_rsvpIfRefreshMultiple	PEER_PORTABLE_ULONG(0x10000)
	MAX_rsvpIfRefreshMultiple,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfTTL - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The value of SEND_TTL used on  this  interface
 *	for  messages  this node originates.  If set to
 *	zero, the node determines  the  TTL  via  other
 *	means.
 *
 ************************************************************************/

/*
 *	rsvpIfTTL is known as 1.3.6.1.2.1.51.1.6.1.6
 */
static READONLY ubyte	AIA_rsvpIfTTL[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x06, 
};

static READONLY struct object_id	AI_rsvpIfTTL =
{	sizeof(AIA_rsvpIfTTL),
	AIA_rsvpIfTTL,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfTTL(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfTTL(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfTTL;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfTTL(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfTTL(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfTTL = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfTTL =
{
	&AI_rsvpIfTTL,
	GET_rsvpIfTTL,
	SET_rsvpIfTTL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfTTL	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpIfTTL,
#define MAX_rsvpIfTTL	PEER_PORTABLE_ULONG(0xff)
	MAX_rsvpIfTTL,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfRefreshInterval - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The value of the RSVP value 'R', which is  the
 *	minimum period between refresh transmissions of
 *	a given PATH or RESV message on an interface.
 *
 ************************************************************************/

/*
 *	rsvpIfRefreshInterval is known as 1.3.6.1.2.1.51.1.6.1.7
 */
static READONLY ubyte	AIA_rsvpIfRefreshInterval[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x07, 
};

static READONLY struct object_id	AI_rsvpIfRefreshInterval =
{	sizeof(AIA_rsvpIfRefreshInterval),
	AIA_rsvpIfRefreshInterval,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfRefreshInterval(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfRefreshInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshInterval;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfRefreshInterval(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfRefreshInterval(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfRefreshInterval = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfRefreshInterval =
{
	&AI_rsvpIfRefreshInterval,
	GET_rsvpIfRefreshInterval,
	SET_rsvpIfRefreshInterval,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfRefreshInterval	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpIfRefreshInterval,
#define MAX_rsvpIfRefreshInterval	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpIfRefreshInterval,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfRouteDelay - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The approximate period from the time  a  route
 *	is  changed to the time a resulting message ap-
 *	pears on the interface.
 *
 ************************************************************************/

/*
 *	rsvpIfRouteDelay is known as 1.3.6.1.2.1.51.1.6.1.8
 */
static READONLY ubyte	AIA_rsvpIfRouteDelay[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x08, 
};

static READONLY struct object_id	AI_rsvpIfRouteDelay =
{	sizeof(AIA_rsvpIfRouteDelay),
	AIA_rsvpIfRouteDelay,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfRouteDelay(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfRouteDelay(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfRouteDelay;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfRouteDelay(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfRouteDelay(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfRouteDelay = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfRouteDelay =
{
	&AI_rsvpIfRouteDelay,
	GET_rsvpIfRouteDelay,
	SET_rsvpIfRouteDelay,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfRouteDelay	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpIfRouteDelay,
#define MAX_rsvpIfRouteDelay	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpIfRouteDelay,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfEnabled - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, RSVP is enabled  on  this  Interface.
 *	If  FALSE,  RSVP  is not enabled on this inter-
 *	face.
 *
 ************************************************************************/

/*
 *	rsvpIfEnabled is known as 1.3.6.1.2.1.51.1.6.1.9
 */
static READONLY ubyte	AIA_rsvpIfEnabled[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x09, 
};

static READONLY struct object_id	AI_rsvpIfEnabled =
{	sizeof(AIA_rsvpIfEnabled),
	AIA_rsvpIfEnabled,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfEnabled(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfEnabled(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfEnabled;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfEnabled(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfEnabled(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfEnabled = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfEnabled =
{
	&AI_rsvpIfEnabled,
	GET_rsvpIfEnabled,
	SET_rsvpIfEnabled,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfEnabled	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpIfEnabled,
#define MAX_rsvpIfEnabled	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpIfEnabled,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfUdpRequired - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	If TRUE, manual configuration forces  the  use
 *	of  UDP  encapsulation  on  the  interface.  If
 *	FALSE, UDP encapsulation is only used if rsvpI-
 *	fUdpNbrs is not zero.
 *
 ************************************************************************/

/*
 *	rsvpIfUdpRequired is known as 1.3.6.1.2.1.51.1.6.1.10
 */
static READONLY ubyte	AIA_rsvpIfUdpRequired[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x0a, 
};

static READONLY struct object_id	AI_rsvpIfUdpRequired =
{	sizeof(AIA_rsvpIfUdpRequired),
	AIA_rsvpIfUdpRequired,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfUdpRequired(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfUdpRequired(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfUdpRequired;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfUdpRequired(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfUdpRequired(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfUdpRequired = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfUdpRequired =
{
	&AI_rsvpIfUdpRequired,
	GET_rsvpIfUdpRequired,
	SET_rsvpIfUdpRequired,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfUdpRequired	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpIfUdpRequired,
#define MAX_rsvpIfUdpRequired	PEER_PORTABLE_ULONG(0x2)
	MAX_rsvpIfUdpRequired,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpIfStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' on interfaces that are configured for
 *	RSVP.
 *
 ************************************************************************/

/*
 *	rsvpIfStatus is known as 1.3.6.1.2.1.51.1.6.1.11
 */
static READONLY ubyte	AIA_rsvpIfStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 0x0b, 
};

static READONLY struct object_id	AI_rsvpIfStatus =
{	sizeof(AIA_rsvpIfStatus),
	AIA_rsvpIfStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpIfStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpIfStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpIfEntry_t *)ctxt)->rsvpIfStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpIfStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpIfStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpIfEntry_t *)ctxt)->rsvpIfStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpIfEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpIfEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpIfEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpIfEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpIfStatus =
{
	&AI_rsvpIfStatus,
	GET_rsvpIfStatus,
	SET_rsvpIfStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpIfEntry,
	peer_locate_rsvpIfEntry,
	IX_rsvpIfTable_rsvpIfEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpIfStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpIfStatus,
#define MAX_rsvpIfStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpIfStatus,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpIfEntry[] =
{
	&A_rsvpIfUdpNbrs,
	&A_rsvpIfIpNbrs,
	&A_rsvpIfNbrs,
	&A_rsvpIfRefreshBlockadeMultiple,
	&A_rsvpIfRefreshMultiple,
	&A_rsvpIfTTL,
	&A_rsvpIfRefreshInterval,
	&A_rsvpIfRouteDelay,
	&A_rsvpIfEnabled,
	&A_rsvpIfUdpRequired,
	&A_rsvpIfStatus,
	(struct attribute READONLY *) NULL
};

#define A_rsvpIfTable_ifIndex A_ifIndex


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpIfTable_rsvpIfEntry[] =
{
	{ 0 + 0,  &A_rsvpIfTable_ifIndex	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpIfEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 0x01, 
};

static READONLY struct object_id	GI_rsvpIfEntry =
{
	9,
	GIA_rsvpIfEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpIfEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpIfEntry =
{
	SNMP_CLASS,
	&GI_rsvpIfEntry,
	1,
	AS_rsvpIfEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpIfTable_rsvpIfEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpIfEntry = 
{
	&SMI_GROUP_rsvpIfEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpIfTable[] =
{
	&CG_rsvpIfEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpIfTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpIfTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x06, 
};

static READONLY struct object_id	GI_rsvpIfTable =
{
	8,
	GIA_rsvpIfTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpIfTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpIfTable =
{
	SNMP_CLASS,
	&GI_rsvpIfTable,
	1,
	AS_rsvpIfTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpIfTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpIfTable = 
{
	&SMI_GROUP_rsvpIfTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};

/************************************************************************
 *
 *	rsvpNbrAddress - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The IP4 or IP6 Address used by this  neighbor.
 *	This  object  may not be changed when the value
 *	of the RowStatus object is 'active'.
 *
 ************************************************************************/

/*
 *	rsvpNbrAddress is known as 1.3.6.1.2.1.51.1.7.1.1
 */
static READONLY ubyte	AIA_rsvpNbrAddress[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x07, 0x01, 0x01, 
};

static READONLY struct object_id	AI_rsvpNbrAddress =
{	sizeof(AIA_rsvpNbrAddress),
	AIA_rsvpNbrAddress,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpNbrAddress(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpNbrAddress(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	OCTETSTRING	*attr = (OCTETSTRING *) attr_ref;

	*attr = ((rsvpNbrEntry_t *)ctxt)->rsvpNbrAddress;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpNbrEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpNbrEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpNbrEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpNbrEntry();
#endif
extern READONLY struct index_entry	IX_rsvpNbrTable_rsvpNbrEntry[];

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpNbrAddress =
{
	&AI_rsvpNbrAddress,
	GET_rsvpNbrAddress,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpNbrEntry,
	peer_locate_rsvpNbrEntry,
	IX_rsvpNbrTable_rsvpNbrEntry,
	SNMP_STRING,		/* OCTETSTRING */
#define MIN_rsvpNbrAddress	PEER_PORTABLE_ULONG(0x4)
	MIN_rsvpNbrAddress,
#define MAX_rsvpNbrAddress	PEER_PORTABLE_ULONG(0x10)
	MAX_rsvpNbrAddress,
	GET_ACCESS_NOT_ALLOWED
};


/************************************************************************
 *
 *	rsvpNbrProtocol - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	The encapsulation being used  by  this  neigh-
 *	bor.
 *
 ************************************************************************/

/*
 *	rsvpNbrProtocol is known as 1.3.6.1.2.1.51.1.7.1.2
 */
static READONLY ubyte	AIA_rsvpNbrProtocol[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x07, 0x01, 0x02, 
};

static READONLY struct object_id	AI_rsvpNbrProtocol =
{	sizeof(AIA_rsvpNbrProtocol),
	AIA_rsvpNbrProtocol,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpNbrProtocol(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpNbrProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpNbrEntry_t *)ctxt)->rsvpNbrProtocol;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpNbrProtocol(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpNbrProtocol(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpNbrEntry_t *)ctxt)->rsvpNbrProtocol = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpNbrEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpNbrEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpNbrEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpNbrEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpNbrProtocol =
{
	&AI_rsvpNbrProtocol,
	GET_rsvpNbrProtocol,
	SET_rsvpNbrProtocol,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpNbrEntry,
	peer_locate_rsvpNbrEntry,
	IX_rsvpNbrTable_rsvpNbrEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpNbrProtocol	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpNbrProtocol,
#define MAX_rsvpNbrProtocol	PEER_PORTABLE_ULONG(0x3)
	MAX_rsvpNbrProtocol,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpNbrStatus - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	'active' for all neighbors.  This  object  may
 *	be  used  to configure neighbors.  In the pres-
 *	ence of configured neighbors,  the  implementa-
 *	tion may (but is not required to) limit the set
 *	of valid neighbors to those configured.
 *
 ************************************************************************/

/*
 *	rsvpNbrStatus is known as 1.3.6.1.2.1.51.1.7.1.3
 */
static READONLY ubyte	AIA_rsvpNbrStatus[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x07, 0x01, 0x03, 
};

static READONLY struct object_id	AI_rsvpNbrStatus =
{	sizeof(AIA_rsvpNbrStatus),
	AIA_rsvpNbrStatus,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpNbrStatus(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpNbrStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr = (INTEGER *) attr_ref;

	*attr = ((rsvpNbrEntry_t *)ctxt)->rsvpNbrStatus;
	return(SNMP_ERR_NO_ERROR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpNbrStatus(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpNbrStatus(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	INTEGER	*attr;

	attr = (INTEGER *) attr_ref;
	((rsvpNbrEntry_t *)ctxt)->rsvpNbrStatus = *attr;
	return(SNMP_ERR_NO_ERROR);
}

#ifdef USE_PROTOTYPES
extern int	peer_next_rsvpNbrEntry(Void *ctxt, Void **indices);
#else
extern int	peer_next_rsvpNbrEntry();
#endif
#ifdef USE_PROTOTYPES
extern Void *peer_locate_rsvpNbrEntry(Void *ctxt, Void **indices, int op);
#else
extern Void *peer_locate_rsvpNbrEntry();
#endif

/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpNbrStatus =
{
	&AI_rsvpNbrStatus,
	GET_rsvpNbrStatus,
	SET_rsvpNbrStatus,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	peer_next_rsvpNbrEntry,
	peer_locate_rsvpNbrEntry,
	IX_rsvpNbrTable_rsvpNbrEntry,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpNbrStatus	PEER_PORTABLE_ULONG(0x1)
	MIN_rsvpNbrStatus,
#define MAX_rsvpNbrStatus	PEER_PORTABLE_ULONG(0x6)
	MAX_rsvpNbrStatus,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpNbrEntry[] =
{
	&A_rsvpNbrAddress,
	&A_rsvpNbrProtocol,
	&A_rsvpNbrStatus,
	(struct attribute READONLY *) NULL
};

#define A_rsvpNbrTable_ifIndex A_ifIndex
#define A_rsvpNbrTable_rsvpNbrAddress A_rsvpNbrAddress


#ifdef PEER_NO_STATIC_FORWARD_DECL 
READONLY struct index_entry	
#else 
static READONLY struct index_entry	
#endif 
IX_rsvpNbrTable_rsvpNbrEntry[] =
{
	{ 0 + 0,  &A_rsvpNbrTable_ifIndex	},
	{ 0 + 0,  &A_rsvpNbrTable_rsvpNbrAddress	},
	{ 0,  (struct attribute READONLY *) NULL}
};
static READONLY ubyte	GIA_rsvpNbrEntry[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x07, 0x01, 
};

static READONLY struct object_id	GI_rsvpNbrEntry =
{
	9,
	GIA_rsvpNbrEntry
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpNbrEntry;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpNbrEntry =
{
	SNMP_CLASS,
	&GI_rsvpNbrEntry,
	1,
	AS_rsvpNbrEntry,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	IX_rsvpNbrTable_rsvpNbrEntry,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpNbrEntry = 
{
	&SMI_GROUP_rsvpNbrEntry,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpNbrTable[] =
{
	&CG_rsvpNbrEntry,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpNbrTable (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpNbrTable[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 0x07, 
};

static READONLY struct object_id	GI_rsvpNbrTable =
{
	8,
	GIA_rsvpNbrTable
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpNbrTable;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpNbrTable =
{
	SNMP_CLASS,
	&GI_rsvpNbrTable,
	1,
	AS_rsvpNbrTable,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpNbrTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};

static READONLY struct contained_obj	CG_rsvpNbrTable = 
{
	&SMI_GROUP_rsvpNbrTable,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL
};
static READONLY struct contained_obj *CGS_rsvpObjects[] =
{
	&CG_rsvpSessionTable,
	&CG_rsvpSenderTable,
	&CG_rsvpSenderOutInterfaceTable,
	&CG_rsvpResvTable,
	&CG_rsvpResvFwdTable,
	&CG_rsvpIfTable,
	&CG_rsvpNbrTable,
	(struct contained_obj READONLY *) NULL
};
#define AS_rsvpObjects (struct attribute READONLY **) NULL
static READONLY ubyte	GIA_rsvpObjects[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x01, 
};

static READONLY struct object_id	GI_rsvpObjects =
{
	7,
	GIA_rsvpObjects
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpObjects;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpObjects =
{
	SNMP_CLASS,
	&GI_rsvpObjects,
	1,
	AS_rsvpObjects,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	CGS_rsvpObjects,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};


/************************************************************************
 *
 *	rsvpBadPackets - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	This object keeps a count of the number of bad
 *	RSVP packets received.
 *
 ************************************************************************/

/*
 *	rsvpBadPackets is known as 1.3.6.1.2.1.51.2.1.0
 */
static READONLY ubyte	AIA_rsvpBadPackets[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x02, 0x01, 
};

static READONLY struct object_id	AI_rsvpBadPackets =
{	sizeof(AIA_rsvpBadPackets),
	AIA_rsvpBadPackets,
};

#ifdef USE_PROTOTYPES
extern int	GET_rsvpBadPackets(Void *ctxt, Void **indices, Void *attr_ref);
#else
extern int	GET_rsvpBadPackets();
#endif


/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpBadPackets =
{
	&AI_rsvpBadPackets,
	GET_rsvpBadPackets,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_dotZero,
	SNMP_GUAGE,		/* Gauge32 */
	PEER_PORTABLE_ULONG(0x0),
	PEER_PORTABLE_ULONG(0xffffffff),
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpSenderNewIndex - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	This  object  is  used  to  assign  values  to
 *	rsvpSenderNumber  as described in 'Textual Con-
 *	ventions  for  SNMPv2'.   The  network  manager
 *	reads  the  object,  and  then writes the value
 *	back in the SET that creates a new instance  of
 *	rsvpSenderEntry.   If  the  SET  fails with the
 *	code 'inconsistentValue', then the process must
 *	be  repeated; If the SET succeeds, then the ob-
 *	ject is incremented, and the  new  instance  is
 *	created according to the manager's directions.
 *
 ************************************************************************/

/*
 *	rsvpSenderNewIndex is known as 1.3.6.1.2.1.51.2.2.0
 */
static READONLY ubyte	AIA_rsvpSenderNewIndex[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x02, 0x02, 
};

static READONLY struct object_id	AI_rsvpSenderNewIndex =
{	sizeof(AIA_rsvpSenderNewIndex),
	AIA_rsvpSenderNewIndex,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpSenderNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpSenderNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpSenderNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpSenderNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
TEST_rsvpSenderNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
TEST_rsvpSenderNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}


/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpSenderNewIndex =
{
	&AI_rsvpSenderNewIndex,
	GET_rsvpSenderNewIndex,
	SET_rsvpSenderNewIndex,
	TEST_rsvpSenderNewIndex,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_dotZero,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpSenderNewIndex	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpSenderNewIndex,
#define MAX_rsvpSenderNewIndex	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpSenderNewIndex,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvNewIndex - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	This  object  is  used  to  assign  values  to
 *	rsvpResvNumber as described in 'Textual Conven-
 *	tions for SNMPv2'.  The network  manager  reads
 *	the  object,  and then writes the value back in
 *	the  SET  that  creates  a  new   instance   of
 *	rsvpResvEntry.   If the SET fails with the code
 *	'inconsistentValue', then the process  must  be
 *	repeated;  If the SET succeeds, then the object
 *	is incremented, and the new instance is created
 *	according to the manager's directions.
 *
 ************************************************************************/

/*
 *	rsvpResvNewIndex is known as 1.3.6.1.2.1.51.2.3.0
 */
static READONLY ubyte	AIA_rsvpResvNewIndex[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x02, 0x03, 
};

static READONLY struct object_id	AI_rsvpResvNewIndex =
{	sizeof(AIA_rsvpResvNewIndex),
	AIA_rsvpResvNewIndex,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
TEST_rsvpResvNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
TEST_rsvpResvNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}


/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvNewIndex =
{
	&AI_rsvpResvNewIndex,
	GET_rsvpResvNewIndex,
	SET_rsvpResvNewIndex,
	TEST_rsvpResvNewIndex,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_dotZero,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvNewIndex	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvNewIndex,
#define MAX_rsvpResvNewIndex	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvNewIndex,
	GET_ACCESS_ALLOWED
};


/************************************************************************
 *
 *	rsvpResvFwdNewIndex - access methods for SNMP MIB variable
 *
 *	Description:
 *
 *	This  object  is  used  to  assign  values  to
 *	rsvpResvFwdNumber as described in 'Textual Con-
 *	ventions  for  SNMPv2'.   The  network  manager
 *	reads  the  object,  and  then writes the value
 *	back in the SET that creates a new instance  of
 *	rsvpResvFwdEntry.   If  the  SET fails with the
 *	code 'inconsistentValue', then the process must
 *	be  repeated; If the SET succeeds, then the ob-
 *	ject is incremented, and the  new  instance  is
 *	created according to the manager's directions.
 *
 ************************************************************************/

/*
 *	rsvpResvFwdNewIndex is known as 1.3.6.1.2.1.51.2.4.0
 */
static READONLY ubyte	AIA_rsvpResvFwdNewIndex[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x02, 0x04, 
};

static READONLY struct object_id	AI_rsvpResvFwdNewIndex =
{	sizeof(AIA_rsvpResvFwdNewIndex),
	AIA_rsvpResvFwdNewIndex,
};

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
GET_rsvpResvFwdNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
GET_rsvpResvFwdNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
SET_rsvpResvFwdNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
SET_rsvpResvFwdNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}

/*ARGSUSED*/
static int
#ifdef USE_PROTOTYPES
TEST_rsvpResvFwdNewIndex(void *ctxt, void **indices, void *attr_ref)
#else
TEST_rsvpResvFwdNewIndex(ctxt, indices, attr_ref)
void			*ctxt;
void			**indices;
void			*attr_ref;
#endif
{
	return(SNMP_ERR_GEN_ERR);
}


/*
 *	The following structure binds the access methods for
 *	this attribute to its object identifier, type, range,
 *	and indexing information.
 */
static READONLY struct attribute	A_rsvpResvFwdNewIndex =
{
	&AI_rsvpResvFwdNewIndex,
	GET_rsvpResvFwdNewIndex,
	SET_rsvpResvFwdNewIndex,
	TEST_rsvpResvFwdNewIndex,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	(Void *(*)PEER_PROTO_ARG((Void *, Void**, int))) NULL,
	IX_dotZero,
	SNMP_NUMBER,		/* INTEGER */
#define MIN_rsvpResvFwdNewIndex	PEER_PORTABLE_ULONG(0x0)
	MIN_rsvpResvFwdNewIndex,
#define MAX_rsvpResvFwdNewIndex	PEER_PORTABLE_ULONG(0x7fffffff)
	MAX_rsvpResvFwdNewIndex,
	GET_ACCESS_ALLOWED
};

READONLY struct attribute *AS_rsvpGenObjects[] =
{
	&A_rsvpBadPackets,
	&A_rsvpSenderNewIndex,
	&A_rsvpResvNewIndex,
	&A_rsvpResvFwdNewIndex,
	(struct attribute READONLY *) NULL
};

static READONLY ubyte	GIA_rsvpGenObjects[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x02, 
};

static READONLY struct object_id	GI_rsvpGenObjects =
{
	7,
	GIA_rsvpGenObjects
};

#ifdef __cplusplus
extern READONLY struct class_definition	SMI_GROUP_rsvpGenObjects;
#endif
READONLY struct class_definition	SMI_GROUP_rsvpGenObjects =
{
	SNMP_CLASS,
	&GI_rsvpGenObjects,
	1,
	AS_rsvpGenObjects,
	(struct group_attribute READONLY **) NULL,
	(struct action READONLY **) NULL,
	(struct notification READONLY **) NULL,
	(int (*) PEER_PROTO_ARG((Void *, Void **, int))) NULL,
	(struct class_definition READONLY **) NULL,
	(struct index_entry READONLY *) NULL,
	(struct contained_obj READONLY **) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
	(int (*)PEER_PROTO_ARG((Void *, Void **, Void *))) NULL,
};


/************************************************************************
 *
 *	newFlow - data structures for SNMP TRAP
 *
 *	Description:
 *
 *	The newFlow trap indicates that the  originat-
 *	ing  system  has  installed  a  new flow in its
 *	classifier, or (when reservation  authorization
 *	is  in view) is prepared to install such a flow
 *	in the classifier and is requesting  authoriza-
 *	tion.   The objects included with the Notifica-
 *	tion may be used to  read  further  information
 *	using  the  Integrated  Services and RSVP MIBs.
 *	Authorization  or  non-authorization   may   be
 *	enacted by a write to the variable intSrvFlowS-
 *	tatus.
 *
 ************************************************************************/

/*
 *	newFlow is known as 1.3.6.1.2.1.51.3.0.1
 */
static READONLY ubyte	TIA_newFlow[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x03, 0x00, 0x01, 
};

static READONLY struct object_id	TI_newFlow =
{
	sizeof(TIA_newFlow),
	TIA_newFlow
};

/*
 *	Variables carried by this trap type
 */
static READONLY struct attribute *	TAS_newFlow[] =
{
	&A_intSrvFlowStatus,
	&A_rsvpSessionDestAddr,
	&A_rsvpResvFwdStatus,
	&A_rsvpResvStatus,
	&A_rsvpSenderStatus,
	(struct attribute READONLY *) NULL
};
/*
 *	This structure relates the trap enterprise and
 *	related fields to its variable list
 */
#ifdef __cplusplus
extern READONLY struct notification	TRAP_newFlow;
#endif
READONLY struct notification	TRAP_newFlow =
{
	&TI_newFlow,
	1,
	(unsigned char (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	TAS_newFlow,
	6,					/* generic trap		*/
	1,					/* specific trap	*/
};

/************************************************************************
 *
 *	send_newFlow_trap - generate SNMP newFlow TRAP
 *
 *	Calling Sequence:
 *		status = send_newFlow_trap(mgmt_env, );
 *
 *	Inputs:
 *
 *	Outputs:
 *	Description:
 *
 *	The newFlow trap indicates that the  originat-
 *	ing  system  has  installed  a  new flow in its
 *	classifier, or (when reservation  authorization
 *	is  in view) is prepared to install such a flow
 *	in the classifier and is requesting  authoriza-
 *	tion.   The objects included with the Notifica-
 *	tion may be used to  read  further  information
 *	using  the  Integrated  Services and RSVP MIBs.
 *	Authorization  or  non-authorization   may   be
 *	enacted by a write to the variable intSrvFlowS-
 *	tatus.
 *
 ************************************************************************/

int
#ifdef USE_PROTOTYPES
send_newFlow_trap(Void	*handle,
		INTEGER		INDEX_1_intSrvFlowStatus,
		INTEGER		intSrvFlowStatus_PARM,
		INTEGER		INDEX_1_rsvpSessionDestAddr,
		OCTETSTRING		rsvpSessionDestAddr_PARM,
		INTEGER		INDEX_1_rsvpResvFwdStatus,
		INTEGER		INDEX_2_rsvpResvFwdStatus,
		INTEGER		rsvpResvFwdStatus_PARM,
		INTEGER		INDEX_1_rsvpResvStatus,
		INTEGER		INDEX_2_rsvpResvStatus,
		INTEGER		rsvpResvStatus_PARM,
		INTEGER		INDEX_1_rsvpSenderStatus,
		INTEGER		INDEX_2_rsvpSenderStatus,
		INTEGER		rsvpSenderStatus_PARM)
#else
send_newFlow_trap(handle,
	INDEX_1_intSrvFlowStatus,
	intSrvFlowStatus_PARM,
	INDEX_1_rsvpSessionDestAddr,
	rsvpSessionDestAddr_PARM,
	INDEX_1_rsvpResvFwdStatus,
	INDEX_2_rsvpResvFwdStatus,
	rsvpResvFwdStatus_PARM,
	INDEX_1_rsvpResvStatus,
	INDEX_2_rsvpResvStatus,
	rsvpResvStatus_PARM,
	INDEX_1_rsvpSenderStatus,
	INDEX_2_rsvpSenderStatus,
	rsvpSenderStatus_PARM)

Void	*handle;
INTEGER		INDEX_1_intSrvFlowStatus;
INTEGER		intSrvFlowStatus_PARM;
INTEGER		INDEX_1_rsvpSessionDestAddr;
OCTETSTRING		rsvpSessionDestAddr_PARM;
INTEGER		INDEX_1_rsvpResvFwdStatus;
INTEGER		INDEX_2_rsvpResvFwdStatus;
INTEGER		rsvpResvFwdStatus_PARM;
INTEGER		INDEX_1_rsvpResvStatus;
INTEGER		INDEX_2_rsvpResvStatus;
INTEGER		rsvpResvStatus_PARM;
INTEGER		INDEX_1_rsvpSenderStatus;
INTEGER		INDEX_2_rsvpSenderStatus;
INTEGER		rsvpSenderStatus_PARM;
#endif
{
	Void	*parms[14];

	/*
	 *	build parameter vector for library
	 */
	parms[13] = NULL;
	parms[0] = (Void *) &INDEX_1_intSrvFlowStatus;
	parms[1] = (Void *) &intSrvFlowStatus_PARM;
	parms[2] = (Void *) &INDEX_1_rsvpSessionDestAddr;
	parms[3] = (Void *) &rsvpSessionDestAddr_PARM;
	parms[4] = (Void *) &INDEX_1_rsvpResvFwdStatus;
	parms[5] = (Void *) &INDEX_2_rsvpResvFwdStatus;
	parms[6] = (Void *) &rsvpResvFwdStatus_PARM;
	parms[7] = (Void *) &INDEX_1_rsvpResvStatus;
	parms[8] = (Void *) &INDEX_2_rsvpResvStatus;
	parms[9] = (Void *) &rsvpResvStatus_PARM;
	parms[10] = (Void *) &INDEX_1_rsvpSenderStatus;
	parms[11] = (Void *) &INDEX_2_rsvpSenderStatus;
	parms[12] = (Void *) &rsvpSenderStatus_PARM;

	return(mgmt_lowlevel_trap(handle, &TRAP_newFlow, parms));
}

/************************************************************************
 *
 *	lostFlow - data structures for SNMP TRAP
 *
 *	Description:
 *
 *	The lostFlow trap indicates that the originat-
 *	ing system has removed a flow from its classif-
 *	ier.
 *
 ************************************************************************/

/*
 *	lostFlow is known as 1.3.6.1.2.1.51.3.0.2
 */
static READONLY ubyte	TIA_lostFlow[] =
{
	0x2b, 0x06, 0x01, 0x02, 0x01, 0x33, 0x03, 0x00, 0x02, 
};

static READONLY struct object_id	TI_lostFlow =
{
	sizeof(TIA_lostFlow),
	TIA_lostFlow
};

/*
 *	Variables carried by this trap type
 */
static READONLY struct attribute *	TAS_lostFlow[] =
{
	&A_intSrvFlowStatus,
	&A_rsvpSessionDestAddr,
	&A_rsvpResvFwdStatus,
	&A_rsvpResvStatus,
	&A_rsvpSenderStatus,
	(struct attribute READONLY *) NULL
};
/*
 *	This structure relates the trap enterprise and
 *	related fields to its variable list
 */
#ifdef __cplusplus
extern READONLY struct notification	TRAP_lostFlow;
#endif
READONLY struct notification	TRAP_lostFlow =
{
	&TI_lostFlow,
	1,
	(unsigned char (*)PEER_PROTO_ARG((Void *, Void **))) NULL,
	TAS_lostFlow,
	6,					/* generic trap		*/
	2,					/* specific trap	*/
};

/************************************************************************
 *
 *	send_lostFlow_trap - generate SNMP lostFlow TRAP
 *
 *	Calling Sequence:
 *		status = send_lostFlow_trap(mgmt_env, );
 *
 *	Inputs:
 *
 *	Outputs:
 *	Description:
 *
 *	The lostFlow trap indicates that the originat-
 *	ing system has removed a flow from its classif-
 *	ier.
 *
 ************************************************************************/

int
#ifdef USE_PROTOTYPES
send_lostFlow_trap(Void	*handle,
		INTEGER		INDEX_1_intSrvFlowStatus,
		INTEGER		intSrvFlowStatus_PARM,
		INTEGER		INDEX_1_rsvpSessionDestAddr,
		OCTETSTRING		rsvpSessionDestAddr_PARM,
		INTEGER		INDEX_1_rsvpResvFwdStatus,
		INTEGER		INDEX_2_rsvpResvFwdStatus,
		INTEGER		rsvpResvFwdStatus_PARM,
		INTEGER		INDEX_1_rsvpResvStatus,
		INTEGER		INDEX_2_rsvpResvStatus,
		INTEGER		rsvpResvStatus_PARM,
		INTEGER		INDEX_1_rsvpSenderStatus,
		INTEGER		INDEX_2_rsvpSenderStatus,
		INTEGER		rsvpSenderStatus_PARM)
#else
send_lostFlow_trap(handle,
	INDEX_1_intSrvFlowStatus,
	intSrvFlowStatus_PARM,
	INDEX_1_rsvpSessionDestAddr,
	rsvpSessionDestAddr_PARM,
	INDEX_1_rsvpResvFwdStatus,
	INDEX_2_rsvpResvFwdStatus,
	rsvpResvFwdStatus_PARM,
	INDEX_1_rsvpResvStatus,
	INDEX_2_rsvpResvStatus,
	rsvpResvStatus_PARM,
	INDEX_1_rsvpSenderStatus,
	INDEX_2_rsvpSenderStatus,
	rsvpSenderStatus_PARM)

Void	*handle;
INTEGER		INDEX_1_intSrvFlowStatus;
INTEGER		intSrvFlowStatus_PARM;
INTEGER		INDEX_1_rsvpSessionDestAddr;
OCTETSTRING		rsvpSessionDestAddr_PARM;
INTEGER		INDEX_1_rsvpResvFwdStatus;
INTEGER		INDEX_2_rsvpResvFwdStatus;
INTEGER		rsvpResvFwdStatus_PARM;
INTEGER		INDEX_1_rsvpResvStatus;
INTEGER		INDEX_2_rsvpResvStatus;
INTEGER		rsvpResvStatus_PARM;
INTEGER		INDEX_1_rsvpSenderStatus;
INTEGER		INDEX_2_rsvpSenderStatus;
INTEGER		rsvpSenderStatus_PARM;
#endif
{
	Void	*parms[14];

	/*
	 *	build parameter vector for library
	 */
	parms[13] = NULL;
	parms[0] = (Void *) &INDEX_1_intSrvFlowStatus;
	parms[1] = (Void *) &intSrvFlowStatus_PARM;
	parms[2] = (Void *) &INDEX_1_rsvpSessionDestAddr;
	parms[3] = (Void *) &rsvpSessionDestAddr_PARM;
	parms[4] = (Void *) &INDEX_1_rsvpResvFwdStatus;
	parms[5] = (Void *) &INDEX_2_rsvpResvFwdStatus;
	parms[6] = (Void *) &rsvpResvFwdStatus_PARM;
	parms[7] = (Void *) &INDEX_1_rsvpResvStatus;
	parms[8] = (Void *) &INDEX_2_rsvpResvStatus;
	parms[9] = (Void *) &rsvpResvStatus_PARM;
	parms[10] = (Void *) &INDEX_1_rsvpSenderStatus;
	parms[11] = (Void *) &INDEX_2_rsvpSenderStatus;
	parms[12] = (Void *) &rsvpSenderStatus_PARM;

	return(mgmt_lowlevel_trap(handle, &TRAP_lostFlow, parms));
}
