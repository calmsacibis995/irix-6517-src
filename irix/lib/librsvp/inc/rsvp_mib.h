/*
 * Definitions for RSVP and IntServ MIB tables.
 * 
 * $Id: rsvp_mib.h,v 1.1 1998/11/25 08:43:36 eddiem Exp $
 */

#ifndef __rsvp_mib_h__
#define __rsvp_mib_h__

#ifdef _PEER
#include "ame/machtype.h"
#include "ame/syspub.h"
#include "ame/odepub.h"
#include "ame/snmperrs.h"
#include "ode/classdef.h"
#include "ode/locator.h"
#include "ode/mgmtpub.h"
#include "ame/smitypes.h"
#include "ame/portpub.h"
#include "ame/snmpvbl.h"
#include "ame/errno.h"
#else
/*
 * Define some stuff that allows my code to compile without the PEER Stuff.
 */
typedef struct octet_string {
     int	len;
     u_char	*val;
} OCTETSTRING;


#define SNMP_ERR_NO_SUCH_NAME  1

typedef u_char SessionType;
typedef u_int Gauge32;

extern void *mgmt_init_env(void *, void *, char *, void *, void *);
extern void mgmt_get_mask(void *, fd_set);
extern void mgmt_new_instance(void *, void *, void *);
extern int mgmt_poll(void *, struct timeval *);

#endif  /* _PEER */


extern int mib_enabled;

/*
 *********************************************************************************
 *
 * Special types used by the MIB.  Some are general SNMP types, some are 
 * specific to intSrv and RSVP MIBs.
 *
 *********************************************************************************
 */

typedef int BitRate;
typedef int BurstSize;
typedef int MessageSize;
typedef int TimeInterval;

typedef u_char TruthValue;			/* defined by RFC 1903 */
#define TRUTHVALUE_TRUE		1
#define TRUTHVALUE_FALSE	2

typedef u_int	TimeStamp;			/* from 1902 def. of timeticks */
typedef int	InterfaceIndex;			/* from RFC 1573 */
typedef int	ifIndex;

typedef u_char	RowStatus;
#define ROWSTATUS_ACTIVE	1
#define ROWSTATUS_NOTINSERVICE	2
#define ROWSTATUS_NOTREADY	3
#define ROWSTATUS_CREATEANDGO	4
#define ROWSTATUS_CREATEANDWAIT	5
#define ROWSTATUS_DESTROY	6


/*
 * IntSrv specific types.  These are also used in the RSVP MIB.
 */
typedef int		SessionNumber;
typedef unsigned char	Protocol;
typedef unsigned char	SessionType;
typedef unsigned char	QosService;
#define QOS_BESTEFFORT		1
#define QOS_GUARENTEEDDELAY	2
#define QOS_CONTROLLEDLOAD	5

/*
 * RSVP MIB defined types.
 */
typedef u_char RsvpEncapsulation;
#define RSVPENCAP_IP	1
#define RSVPENCAP_UDP	2
#define RSVPENCAP_BOTH	3

typedef int RefreshInterval;

     
/*
 *********************************************************************************
 *
 * intSrvIfAttribTable {intSrvObjects 1}
 * This table is indexed by sgi_ifIndex.
 *
 *********************************************************************************
 */

#define IFATTRIB_NO_TC		1
#define IFATTRIB_ADMIN_OFF	2

typedef struct intSrvIfAttribEntry {
     uint sgi_flags;
     InterfaceIndex sgi_ifIndex;
     uint sgi_intbandwidth;			/* interface bandwidth */
     int intSrvIfAttribAllocatedBits;		/* read-only; currently alloc'd bits */
     int intSrvIfAttribMaxAllocatedBits;	/* read-create; max allocatable bits */
     int intSrvIfAttribAllocatedBuffer;		/* read-only; sum burst of all flows. */
     uint intSrvIfAttribFlows;			/* read-only; num active flows */
     int intSrvIfAttribPropagationDelay;	/* read-create; additional delay */
     RowStatus intSrvIfAttribStatus;		/* read-create */
} intSrvIfAttribEntry_t;


/*
 *********************************************************************************
 *
 * intSrvFlowTable {intSrvObjects 2}
 * This table is indexed by intSrvFlowNumber.
 * sgi_kernel_handle is the number that the kernel identifies the flow with.
 *
 *********************************************************************************
 */

/* for intSrvFlowOwner */
#define FLOWOWNER_OTHER			1
#define FLOWOWNER_RSVP			2
#define FLOWOWNER_MANAGEMENT		3

typedef struct intSrvFlowEntry {
     uint sgi_flags;
     uint sgi_num_filters;		/* ref count for SE type filters */
     uint sgi_kernel_handle;		/* kernel handle for this flow */
     TimeStamp sgi_timestamp;		/* when was this entry deactivated */
     struct intSrvFlowEntry *sgi_next;
     struct intSrvFlowEntry *sgi_prev;
     int intSrvFlowNumber;		/* not-accessable; but incrementable
					   using intSrvFlowNewIndex. */
     uchar_t intSrvFlowType;		/* read-create   */
     int intSrvFlowOwner;		/* read-create   */
     OCTETSTRING intSrvFlowDestAddr;	/* read-create   */
     OCTETSTRING intSrvFlowSenderAddr;	/* read-create   */
     int intSrvFlowDestAddrLength;	/* read-create   */
     int intSrvFlowSenderAddrLength;	/* read-create   */
     uchar_t intSrvFlowProtocol;	/* read-create   */
     OCTETSTRING intSrvFlowDestPort;	/* read-create   */
     OCTETSTRING intSrvFlowPort;	/* read-create   */
     int intSrvFlowId;			/* read-only: IPv6 FlowID*/
     InterfaceIndex intSrvFlowInterface;	/* read-create   */
     OCTETSTRING intSrvFlowIfAddr;	/* read-create   */
     int intSrvFlowRate;		/* read-create; bps   */
     int intSrvFlowBurst;		/* read-create; Bytes   */
     int intSrvFlowWeight;		/* read-create   */
     int intSrvFlowQueue;		/* read-create   */
     int intSrvFlowMinTU;		/* read-create   */
     int intSrvFlowMaxTU;		/* read-create   */
     uint intSrvFlowBestEffort;		/* read-only; pkts remanded to BE   */
     uint intSrvFlowPoliced;		/* read-only; total pkts policed   */
     TruthValue intSrvFlowDiscard;	/* read-create; true = discard when policed
				                false = treat as BE when policed */
     QosService intSrvFlowService;	/* read-only;   */
     int intSrvFlowOrder;		/* read-create   */
     RowStatus intSrvFlowStatus;	/* read-create   */

} intSrvFlowEntry_t;


/*
 *********************************************************************************
 *
 * rsvpSessionTable {rsvpObjects 1}
 * This table acts as the "root" of many of the other tables in the RSVP MIB.
 * For example, senders (table 2) are associated with the session to which
 * they are sending.  Similarly, receivers (table 3) are also associated
 * with the session to which they belong.  That is why there are so many
 * extra sgi_xxx pointers in the session entry structure.
 *
 *********************************************************************************
 */

typedef struct rsvpSessionEntry {
     struct rsvpSessionEntry *sgi_next;
     struct rsvpSessionEntry *sgi_prev;
     struct rsvpSenderEntry *sgi_senderp;
     struct rsvpResvEntry *sgi_resvp;
     struct rsvpResvFwdEntry *sgi_resvfwdp;
     TimeStamp sgi_timestamp;
     RowStatus sgi_status;
     SessionNumber rsvpSessionNumber;	/* not-accessable (index) */
     SessionType rsvpSessionType;	/* read-only */
     OCTETSTRING rsvpSessionDestAddr;	/* read-only */
     int rsvpSessionDestAddrLength;	/* read-only */
     Protocol rsvpSessionProtocol;	/* read-only */
     OCTETSTRING rsvpSessionPort;	/* read-only */
     Gauge32 rsvpSessionSenders;	/* read-only */
     Gauge32 rsvpSessionReceivers;	/* read-only */
     Gauge32 rsvpSessionRequests;	/* read-only */
} rsvpSessionEntry_t;


/*
 *********************************************************************************
 *
 * rsvpSenderTable {rsvpObjects 2}
 * This table is indexed by {rsvpSessionNumber, rsvpSenderNumber}
 * The spec says it is possible to create a new row by writing to the 
 * rsvpSenderNewIndex object, but we will not support that in the first release.
 *
 *********************************************************************************
 */

typedef struct rsvpSenderEntry {
     struct rsvpSenderEntry *sgi_next;
     struct rsvpSenderEntry *sgi_prev;
     rsvpSessionEntry_t *sgi_sessp;	/* back ptr to this sender's session */
     struct rsvpSenderOutInterfaceEntry *sgi_outifp;
     SessionNumber rsvpSenderNumber;	/* not-accessable (index2) */
     SessionType rsvpSenderType;	/* read-create */
     OCTETSTRING rsvpSenderDestAddr;	/* read-create */
     OCTETSTRING rsvpSenderAddr;	/* read-create */
     int rsvpSenderDestAddrLength;	/* read-create */
     int rsvpSenderAddrLength;		/* read-create */
     Protocol rsvpSenderProtocol;	/* read-create */
     OCTETSTRING rsvpSenderDestPort;	/* read-create */
     OCTETSTRING rsvpSenderPort;	/* read-create */
     int rsvpSenderFlowId;		/* read-only */
     OCTETSTRING rsvpSenderHopAddr;	/* read-create */
     int rsvpSenderHopLih;		/* read-create */
     InterfaceIndex rsvpSenderInterface;/* read-create */
     BitRate rsvpSenderTSpecRate;	/* read-create */
     BitRate rsvpSenderTSpecPeakRate;	/* read-create */
     BurstSize rsvpSenderTSpecBurst;	/* read-create */
     MessageSize rsvpSenderTSpecMinTU;	/* read-create */
     MessageSize rsvpSenderTSpecMaxTU;	/* read-create */
     RefreshInterval rsvpSenderInterval;/* read-create */
     TruthValue rsvpSenderRSVPHop;	/* read-create */
     TimeStamp rsvpSenderLastChange;	/* read-only */
     OCTETSTRING rsvpSenderPolicy;	/* read-create */
     TruthValue rsvpSenderAdspecBreak;	/* read-create */
     int rsvpSenderAdspecHopCount;	/* read-create */
     BitRate rsvpSenderAdspecPathBw;	/* read-create */
     BitRate rsvpSenderAdspecMinLatency;/* read-create */
     int rsvpSenderAdspecMtu;		/* read-create */
     TruthValue rsvpSenderAdspecGuaranteedSvc;		/* read-create */
     TruthValue rsvpSenderAdspecGuaranteedBreak;	/* read-create */
     int rsvpSenderAdspecGuaranteedCtot;	/* read-create */
     int rsvpSenderAdspecGuaranteedDtot;	/* read-create */
     int rsvpSenderAdspecGuaranteedCsum;	/* read-create */
     int rsvpSenderAdspecGuaranteedDsum;	/* read-create */
     int rsvpSenderAdspecGuaranteedHopCount;	/* read-create */
     BitRate rsvpSenderAdspecGuaranteedPathBw;	/* read-create */
     int rsvpSenderAdspecGuaranteedMinLatency;	/* read-create */
     int rsvpSenderAdspecGuaranteedMtu;		/* read-create */
     TruthValue rsvpSenderAdspecCtrlLoadSvc;	/* read-create */
     TruthValue rsvpSenderAdspecCtrlLoadBreak;	/* read-create */
     int rsvpSenderAdspecCtrlLoadHopCount;	/* read-create */
     BitRate rsvpSenderAdspecCtrlLoadPathBw;	/* read-create */
     int rsvpSenderAdspecCtrlLoadMinLatency;	/* read-create */
     int rsvpSenderAdspecCtrlLoadMtu;		/* read-create */
     RowStatus rsvpSenderStatus;	/* read-create */
     int rsvpSenderTTL;			/* read-only */
} rsvpSenderEntry_t;



/*
 *********************************************************************************
 *
 * rsvpSenderOutInterfaceTable {rsvpObjects 3}
 * Description: List of outgoing interfaces that PATH messages use.
 *              The ifIndex is the ifIndex value of the egress interface.
 * This table is indexed by {rsvpSessionNumber, rsvpSenderNumber, ifIndex}
 *
 *********************************************************************************
 */

typedef struct rsvpSenderOutInterfaceEntry {
     RowStatus rsvpSenderOutInterfaceStatus;
} rsvpSenderOutInterfaceEntry_t;


/*
 *********************************************************************************
 *
 * rsvpResvTable {rsvpObjects 4}
 * Description: It is in essence a list of the valid RESV messages that the RSVP
 *              router or host is receiving.
 * This table is indexed by {rsvpSessionNumber, rsvpResvNumber}
 * The spec says it is possible to create a new row by writing to the 
 * rsvpResvNewIndex object, but we will not support that in the first release.
 *
 *********************************************************************************
 */

typedef struct rsvpResvEntry {
     struct rsvpResvEntry *sgi_prev;
     struct rsvpResvEntry *sgi_next;
     rsvpSessionEntry_t *sgi_sessp;
     SessionNumber rsvpResvNumber;	/* not-accessable (index2) */
     SessionType rsvpResvType;		/* read-create */
     OCTETSTRING rsvpResvDestAddr;	/* read-create */
     OCTETSTRING rsvpResvSenderAddr;	/* read-create */
     int rsvpResvDestAddrLength;	/* read-create */
     int rsvpResvSenderAddrLength;	/* read-create */
     Protocol rsvpResvProtocol;		/* read-create */
     OCTETSTRING rsvpResvDestPort;	/* read-create */
     OCTETSTRING rsvpResvPort;		/* read-create */
     OCTETSTRING rsvpResvHopAddr;	/* read-create */
     int rsvpResvHopLih;		/* read-create */
     InterfaceIndex rsvpResvInterface;	/* read-create */
     QosService rsvpResvService;	/* read-create */
     BitRate rsvpResvTSpecRate;		/* read-create */
     BitRate rsvpResvTSpecPeakRate;	/* read-create */
     BurstSize rsvpResvTSpecBurst;	/* read-create */
     MessageSize rsvpResvTSpecMinTU;	/* read-create */
     MessageSize rsvpResvTSpecMaxTU;	/* read-create */
     BitRate rsvpResvRSpecRate;		/* read-create */
     int rsvpResvRSpecSlack;		/* read-create */
     RefreshInterval rsvpResvInterval;	/* read-create */
     OCTETSTRING rsvpResvScope;	/* read-create */
     TruthValue rsvpResvShared;		/* read-create */
     TruthValue rsvpResvExplicit;	/* read-create */
     TruthValue rsvpResvRSVPHop;	/* read-create */
     TimeStamp rsvpResvLastChange;	/* read-only */
     OCTETSTRING rsvpResvPolicy;	/* read-create */
     RowStatus rsvpResvStatus;		/* read-create */
     int rsvpResvTTL;			/* read-only */
     int rsvpResvFlowId;		/* read-only */
} rsvpResvEntry_t;


/*
 *********************************************************************************
 *
 * rsvpResvFwdTable {rsvpObjects 5}
 * Description: This table is in essence a list of the valid RESV messages that
 *              the rsvp router or host is sending to its upstream neighbors.
 * This table is indexed by {rsvpSessionNumber, rsvpResvFwdNumber}
 * Again, there is a rsvpResvFwdNewIndex test-and-increment variable which
 * we will not support in the first release.
 *********************************************************************************
 */

typedef struct rsvpResvFwdEntry {
     struct rsvpResvFwdEntry *sgi_prev;
     struct rsvpResvFwdEntry *sgi_next;
     rsvpSessionEntry_t *sgi_sessp;
     SessionNumber rsvpResvFwdNumber;	/* not-accessable (index2) */
     SessionType rsvpResvFwdType;	/* read-create */
     OCTETSTRING rsvpResvFwdDestAddr;	/* read-create */
     OCTETSTRING rsvpResvFwdSenderAddr;	/* read-create */
     int rsvpResvFwdDestAddrLength;	/* read-create */
     int rsvpResvFwdSenderAddrLength;	/* read-create */
     Protocol rsvpResvFwdProtocol;	/* read-create */
     OCTETSTRING rsvpResvFwdDestPort;	/* read-create */
     OCTETSTRING rsvpResvFwdPort;	/* read-create */
     OCTETSTRING rsvpResvFwdHopAddr;	/* read-create */
     int rsvpResvFwdHopLih;		/* read-create */
     InterfaceIndex rsvpResvFwdInterface;	/* read-create */
     QosService rsvpResvFwdService;	/* read-create */
     BitRate rsvpResvFwdTSpecRate;	/* read-create */
     BitRate rsvpResvFwdTSpecPeakRate;	/* read-create */
     BurstSize rsvpResvFwdTSpecBurst;	/* read-create */
     MessageSize rsvpResvFwdTSpecMinTU;	/* read-create */
     MessageSize rsvpResvFwdTSpecMaxTU;	/* read-create */
     BitRate rsvpResvFwdRSpecRate;	/* read-create */
     int rsvpResvFwdRSpecSlack;		/* read-create */
     RefreshInterval rsvpResvFwdInterval;	/* read-create */
     OCTETSTRING rsvpResvFwdScope;	/* read-create */
     TruthValue rsvpResvFwdShared;		/* read-create */
     TruthValue rsvpResvFwdExplicit;	/* read-create */
     TruthValue rsvpResvFwdRSVPHop;	/* read-create */
     TimeStamp rsvpResvFwdLastChange;	/* read-only */
     OCTETSTRING rsvpResvFwdPolicy;	/* read-create */
     RowStatus rsvpResvFwdStatus;		/* read-create */
     int rsvpResvFwdTTL;			/* read-only */
     int rsvpResvFwdFlowId;		/* read-only */
} rsvpResvFwdEntry_t;


/*
 *********************************************************************************
 *
 * rsvpIfTable {rsvpObjects 6}
 * Description: This table contains RSVP specific information for an interface.
 * This table is indexed by {ifIndex}
 *********************************************************************************
 */

typedef struct rsvpIfEntry {
     ifIndex sgi_ifindex;		/* kernel ifindex for this interface */
     int sgi_ifvec;			/* index into the if_vec array */
     struct rsvpNbrEntry *sgi_nbrp;	/* nbrs seen on this interface */
     Gauge32 rsvpIfUdpNbrs;		/* read-only */
     Gauge32 rsvpIfIpNbrs;		/* read-only */
     Gauge32 rsvpIfNbrs;		/* read-only */
     int rsvpIfRefreshBlockadeMultiple;	/* read-create */
     int rsvpIfRefreshMultiple;		/* read-create */
     int rsvpIfTTL;			/* read-create */
     TimeInterval rsvpIfRefreshInterval;/* read-create */
     TimeInterval rsvpIfRouteDelay;	/* read-create */
     TruthValue rsvpIfEnabled;		/* read-create */
     TruthValue rsvpIfUdpRequired;	/* read-only */
     RowStatus rsvpIfStatus;		/* read-create */
} rsvpIfEntry_t;


/*
 *********************************************************************************
 *
 * rsvpNbrTable {rsvpObjects 7}
 * Description: This table lists neighbors that rsvp process is currently receiving
 *              messages from.
 * This table is indexed by {ifIndex, rsvpNbrAddress}
 *********************************************************************************
 */

typedef struct rsvpNbrEntry {
     struct rsvpNbrEntry *sgi_prev;
     struct rsvpNbrEntry *sgi_next;
     rsvpIfEntry_t *sgi_ifp;		/* rsvpIfEntry to which is nbr belongs */
     OCTETSTRING rsvpNbrAddress;	/* not-accessable */
     RsvpEncapsulation rsvpNbrProtocol; /* read-create */
     RowStatus rsvpNbrStatus;		/* read-create */
}rsvpNbrEntry_t;



/*
 *********************************************************************************
 *
 * Entry points for main rsvpd code to update or get info from the MIB portion
 * of rsvpd, i.e. entry points into rsvp_mib.c and rsvp_mglue.c
 *
 *********************************************************************************
 */
int mib_init(void);
void mib_process_req(void);

/*
 * intSrvIfAttribTable entry points
 */
int rsvpd_update_intSrvIfAttribEntry(int ifIndex, int allocbufs, int numflows);


/*
 * intSrvFlowTable entry points
 */
void rsvpd_insert_intSrvFlowEntry(int kernel_handle, intSrvFlowEntry_t *flowentryp);
int rsvpd_activate_intSrvFlowEntry(int kernel_handle, int protocol,
				   struct in_addr *src_inap, int src_port,
				   struct in_addr *dest_inap, int dest_port);
int rsvpd_dec_intSrvFlowEntry(int kernel_handle);
int rsvpd_get_intSrvFlowEntry_bkt(int kernel_handle);
int rsvpd_set_intSrvFlowEntry_rate_bkt(int flownum, int rate, int bkt);
void rsvpd_deactivate_intSrvFlowEntry(int flownum);

/*
 * rsvpSessionTable entry points
 */
void rsvpd_insert_rsvpSessionEntry(rsvpSessionEntry_t *entryp);
void rsvpd_mod_rsvpSessionSenders(void *sessionhandle, int inc);
void rsvpd_mod_rsvpSessionReceivers(void *sessionhandle, int inc);
void rsvpd_mod_rsvpSessionRequests(void *sessionhandle, int inc);
void rsvpd_deactivate_rsvpSessionEntry(void *sessionhandle);

/*
 * rsvpSenderTable entry points
 */
void rsvpd_insert_rsvpSenderEntry(void *sessionhandle, rsvpSenderEntry_t *entryp);
void rsvpd_update_rsvpSenderPhop(void *senderhandle, RSVP_HOP *phop);
void rsvpd_update_rsvpSenderTspec(void *senderhandle, SENDER_TSPEC *Tspecp);
void rsvpd_deactivate_rsvpSenderEntry(void *senderhandle);

/*
 * rsvpSenderOutInterfaceTable entry points
 */
void rsvpd_update_rsvpSenderOutInterfaceEntry(void *rsvpSenderhandle, bitmap bm);

/*
 * rsvpResvTable entry points
 */
void rsvpd_insert_rsvpResvEntry(void *sessionhandle, rsvpResvEntry_t *entryp);
void rsvpd_update_rsvpResvTspec(void *resvhandle, FLOWSPEC *flowspecp);
void rsvpd_deactivate_rsvpResvEntry(void *resvhandle);


/*
 * rsvpResvFwdTable entry points
 */
void rsvpd_insert_rsvpResvFwdEntry(void *sessionhandle, rsvpResvFwdEntry_t *entryp);
void rsvpd_update_rsvpResvFwdTspec(void *resvfwdhandle, FLOWSPEC *flowspecp);
void rsvpd_deactivate_rsvpResvFwdEntry(void *resvfwdhandle);

/*
 * rsvpNbrTable entry points
 */
void rsvpd_heard_from(int input_if, struct sockaddr_in *sin_addrp, int udp_enacaps);

/*
 * rsvpBadPackets entry points.
 */
void rsvpd_inc_rsvpBadPackets(void);


/*
 * Notifications entry points.
 */
void rsvpd_newFlow(int kernel_handle, void *sessp, void *resvp, void *fwdp);
void rsvpd_lostFlow(int kernel_handle, void *sessp, void *resvp, void *fwdp);


/*
 * rsvp_mglue functions
 */
void *mglue_new_session(Session *sessp);
void *mglue_new_sender(Session *destp, PSB *psbp, struct packet *pkt);
void *mglue_new_resv(Session *destp, RSB *rp, struct packet *pkt);
void *mglue_new_resvfwd(Session *destp, RSB *rp, struct packet *pkt);


/*
 * generally useful functions
 */
void octetstring_fill(OCTETSTRING *osp, void *val, int len);
TimeStamp get_mib_timestamp(void);


/*
 *********************************************************************************
 *
 * Entry points for the PEER SNMP master agent to the MIB portion of rsvpd.
 *
 *********************************************************************************
 */
#ifdef _PEER
int peer_next_intSrvIfAttribEntry(void *ctxt, void **indices);
void *peer_locate_intSrvIfAttribEntry(void *ctxt, void **indices, int op);
int peer_test_intSrvIfAttribMaxAllocatedBits(void *ctxt, void **indices, void *attr_ref);
int peer_test_generic_fail(void *ctxt, void **indices, void *attr_ref);

int peer_next_intSrvFlowEntry(void *ctxt, void **indices);
void *peer_locate_intSrvFlowEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpSessionEntry(void *ctxt, void **indices);
void *peer_locate_rsvpSessionEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpSenderEntry(void *ctxt, void **indices);
void *peer_locate_rsvpSenderEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpSenderOutInterfaceEntry(void *ctxt, void **indices);
void *peer_locate_rsvpSenderOutInterfaceEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpResvEntry(void *ctxt, void **indices);
void *peer_locate_rsvpResvEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpResvFwdEntry(void *ctxt, void **indices);
void *peer_locate_rsvpResvFwdEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpIfEntry(void *ctxt, void **indices);
void *peer_locate_rsvpIfEntry(void *ctxt, void **indices, int op);

int peer_next_rsvpNbrEntry(void *ctxt, void **indices);
void *peer_locate_rsvpNbrEntry(void *ctxt, void **indices, int op);


#endif /* _PEER */

#endif /* __rsvp_mib_h__ */
