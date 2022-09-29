/*
 * addrd.h
 *	Declarations for FFSC Address Daemon Functions
 */

#ifndef _ADDRD_H_
#define _ADDRD_H_

#include <types/vxtypes.h>
#include "ffsc.h"
#include "identity.h"
#include "net.h"


/* Constants */
#define ETHERTYPE_ADDRD		0x0801		/* AddrD ethernet protocol */

#define ADDRD_PIPE_NAME		"/pipe/addrd"	/* AddrD pipe traits */
#define ADDRD_PIPE_SIZE		MAX_RACKS
#define ADDRD_PIPE_MSGLEN	ADM_MSGLEN


/* An identity struct from the version 1 protocol */
typedef struct identity_v1 {
        uniqueid_t      uniqueid;       /* FFSC identifier */
        struct in_addr  ipaddr;         /* IP address */
        rackid_t        rackid;         /* Rack ID */
} identity_v1_t;


/* An address daemon protocol message. The extra padding is not only for */
/* future expansion, but also to push the size of the overall packet to  */
/* be at least 60 bytes, the minimum size for an ethernet frame.	 */
typedef struct addrdmsg {
	uint32_t	version;	/* Version code */
	uint32_t	type;		/* Message type */
	modulenum_t	modulenums[2];	/* Module #'s for this FFSC */
	uint32_t	reserved[3];	/* for future expansion */
	uint32_t	capabilities;	/* Capability flags */

	identity_v1_t	oident;		/* Other identifiers for this FFSC */
} addrdmsg_t;

#define ADM_VERSION_CURR	0x00000003	/* Current version number */

#define ADM_TYPE_IDENTITY	1	/* IDENTITY info */
#define ADM_TYPE_QUERY		2	/* QUERY identifiers */
#define ADM_TYPE_ASSIGN		3	/* ASSIGN new identifier(s) */
#define ADM_TYPE_ELECTION_START 4	/* START a master FFSC election */
#define ADM_TYPE_ELECTION_END	5	/* END a master FFSC election */
#define ADM_TYPE_REQUEST_RACKID	6	/* REQUEST a specific RACK ID */
#define ADM_TYPE_NEW_PARTINFO   7       /* Update new partition info */

#define ADM_MSGLEN	(sizeof(struct ether_header) + sizeof(addrdmsg_t))


/* Private functions (only used by addrd.c and debug.c) */
STATUS addrdBroadcastMsg(addrdmsg_t *Msg);
void   addrdExtractIdentity(addrdmsg_t *, identity_t *);
void   addrdInsertIdentity(addrdmsg_t *, identity_t *);
STATUS addrdLocalMsg(addrdmsg_t *Msg);
STATUS addrdUpdatePartInfo(addrdmsg_t* Msg);

/* Public functions */
STATUS addrdElectMaster(void);
STATUS addrdInit(void);
STATUS addrdRequestRackID(identity_t *);
STATUS addrdUpdateLocalIdentity(identity_t *);
STATUS broadcastPartitionInfo(part_info_t** pInfo);


#endif  /* _ADDRD_H_ */
