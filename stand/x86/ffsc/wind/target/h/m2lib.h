/* m2Lib.h - VxWorks MIB-II interface to SNMP Agent */

/* Copyright 1984-1993 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01a,08dec93,jag  written
*/

#ifndef __INCm2Libh
#define __INCm2Libh

#ifdef __cplusplus
extern "C" {
#endif

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* m2Lib.c Error Codes */

#define S_m2Lib_INVALID_PARAMETER               (M_m2Lib | 1)
#define S_m2Lib_ENTRY_NOT_FOUND			(M_m2Lib | 2)
#define S_m2Lib_TCPCONN_FD_NOT_FOUND		(M_m2Lib | 3)
#define S_m2Lib_INVALID_VAR_TO_SET		(M_m2Lib | 4)
#define S_m2Lib_CANT_CREATE_SYS_SEM		(M_m2Lib | 5)
#define S_m2Lib_CANT_CREATE_IF_SEM		(M_m2Lib | 6)
#define S_m2Lib_CANT_CREATE_ROUTE_SEM		(M_m2Lib | 7)
#define S_m2Lib_ARP_PHYSADDR_NOT_SPECIFIED	(M_m2Lib | 8)
#define S_m2Lib_IF_TBL_IS_EMPTY			(M_m2Lib | 9)
#define S_m2Lib_IF_CNFG_CHANGED			(M_m2Lib | 10)

#define   ETHERADDRLEN            6
#define   M2DISPLAYSTRSIZE	256
#define   MAXOIDLENGH            40
#define   MAXIFPHYADDR		 16

/* defines for enumerated types as specified by RFC 1213 */

/* possible values for ifType */

#define M2_ifType_other                 		1
#define M2_ifType_regular1822           		2
#define M2_ifType_hdh1822               		3
#define M2_ifType_ddn_x25               		4
#define M2_ifType_rfc877_x25            		5
#define M2_ifType_ethernet_csmacd       		6
#define M2_ifType_iso88023_csmacd       		7
#define M2_ifType_iso88024_tokenBus     		8
#define M2_ifType_iso88025_tokenRing    		9
#define M2_ifType_iso88026_man          		10
#define M2_ifType_starLan               		11
#define M2_ifType_proteon_10Mbit        		12
#define M2_ifType_proteon_80Mbit        		13
#define M2_ifType_hyperchannel          		14
#define M2_ifType_fddi                  		15
#define M2_ifType_lapb                  		16
#define M2_ifType_sdlc                  		17
#define M2_ifType_ds1					18
#define M2_ifType_e1					19
#define M2_ifType_basicISDN             		20
#define M2_ifType_primaryISDN           		21
#define M2_ifType_propPointToPointSerial 		22
#define M2_ifType_ppp                   		23
#define M2_ifType_softwareLoopback      		24
#define M2_ifType_eon                   		25
#define M2_ifType_ethernet_3Mbit        		26
#define M2_ifType_nsip                  		27
#define M2_ifType_slip                  		28
#define M2_ifType_ultra                 		29
#define M2_ifType_ds3                   		30
#define M2_ifType_sip					31
#define M2_ifType_frame_relay				32

/* possible values for ifAdminStatus */

#define M2_ifAdminStatus_up           			1
#define M2_ifAdminStatus_down				2
#define M2_ifAdminStatus_testing        		3


/* possible values for ifOperStatus */

#define M2_ifOperStatus_up              		1
#define M2_ifOperStatus_down            		2
#define M2_ifOperStatus_testing         		3

/* possible values for  ipForwarding */

#define M2_ipForwarding_forwarding      		1
#define M2_ipForwarding_not_forwarding  		2

/* possible values for ipRouteType */

#define M2_ipRouteType_other            		1
#define M2_ipRouteType_invalid          		2
#define M2_ipRouteType_direct           		3
#define M2_ipRouteType_indirect         		4

/* possible values for ipRouteProto */

#define M2_ipRouteProto_other           		1
#define M2_ipRouteProto_local           		2
#define M2_ipRouteProto_netmgmt         		3
#define M2_ipRouteProto_icmp            		4
#define M2_ipRouteProto_egp             		5
#define M2_ipRouteProto_ggp             		6
#define M2_ipRouteProto_hello           		7
#define M2_ipRouteProto_rip             		8
#define M2_ipRouteProto_is_is           		9
#define M2_ipRouteProto_es_is				10
#define M2_ipRouteProto_ciscoIgrp			11
#define M2_ipRouteProto_bbnSpfIgp			12
#define M2_ipRouteProto_ospf				13
#define M2_ipRouteProto_bgp     			14

/* possible values for ipNetToMediaType */

#define M2_ipNetToMediaType_other			1
#define M2_ipNetToMediaType_invalid     		2
#define M2_ipNetToMediaType_dynamic     		3
#define M2_ipNetToMediaType_static      		4

/* possible values for tcpRtoAlgorithm */

#define M2_tcpRtoAlgorithm_other        		1
#define M2_tcpRtoAlgorithm_constant     		2
#define M2_tcpRtoAlgorithm_rsre         		3
#define M2_tcpRtoAlgorithm_vanj         		4

/* possible values for tcpConnState */

#define M2_tcpConnState_closed          		1
#define M2_tcpConnState_listen          		2
#define M2_tcpConnState_synSent         		3
#define M2_tcpConnState_synReceived     		4
#define M2_tcpConnState_established     		5
#define M2_tcpConnState_finWait1        		6
#define M2_tcpConnState_finWait2        		7
#define M2_tcpConnState_closeWait       		8
#define M2_tcpConnState_lastAck         		9
#define M2_tcpConnState_closing         		10
#define M2_tcpConnState_timeWait        		11
#define M2_tcpConnState_deleteTCB       		12


/*
 * When using vxWorks SNMP this constants must have the same value as NEXT and
 * EXACT
 */

#define M2_EXACT_VALUE      	0xA0
#define M2_NEXT_VALUE           0xA1 

typedef struct
    {
    long   idLength;			/* Length of the object identifier */
    long   idArray [MAXOIDLENGH];	/* Object Id numbers */
 
    } M2_OBJECTID;

typedef struct
    {
    long           addrLength;			/* Length of address */
    unsigned char  phyAddress [MAXIFPHYADDR];   /* physical address value */

    } M2_PHYADDR;


/*
 * The structures that follow are based on the MIB-II RFC-1213.  Each field in
 * each of the structures has the same name as the name specified in by the RFC.
 * Please refer to the RFC for a complete description of the variable and its
 * semantics.
 */


/* System Group bit fields that map to variables that can be set */

#define    M2SYSNAME    	0x01
#define    M2SYSCONTACT 	0x02
#define	   M2SYSLOCATION	0x04

typedef struct
    {
    unsigned char   sysDescr     [M2DISPLAYSTRSIZE];
    M2_OBJECTID     sysObjectID;
    unsigned long   sysUpTime;
    unsigned char   sysContact   [M2DISPLAYSTRSIZE];
    unsigned char   sysName      [M2DISPLAYSTRSIZE];
    unsigned char   sysLocation  [M2DISPLAYSTRSIZE];
    long            sysServices;

    }  M2_SYSTEM;


/* Interface group variables */

typedef struct
    {
    long ifNumber;	/* Number of Interfaces in the System */

    }  M2_INTERFACE;


/* values as per RFC 1215 */

#define M2_LINK_DOWN_TRAP	2
#define M2_LINK_UP_TRAP		3

typedef struct
    {
    int  	    ifIndex;
    char 	    ifDescr       [M2DISPLAYSTRSIZE];
    long            ifType;
    long            ifMtu;
    unsigned long   ifSpeed;
    M2_PHYADDR      ifPhysAddress;
    long            ifAdminStatus;
    long            ifOperStatus;
    unsigned long   ifLastChange;
    unsigned long   ifInOctets;
    unsigned long   ifInUcastPkts;
    unsigned long   ifInNUcastPkts;
    unsigned long   ifInDiscards;
    unsigned long   ifInErrors;
    unsigned long   ifInUnknownProtos;
    unsigned long   ifOutOctets;
    unsigned long   ifOutUcastPkts;
    unsigned long   ifOutNUcastPkts;
    unsigned long   ifOutDiscards;
    unsigned long   ifOutErrors;
    unsigned long   ifOutQLen;
    M2_OBJECTID     ifSpecific;

    }  M2_INTERFACETBL;

/* IP group bit fields that map to variables that can be set */

#define M2_IPFORWARDING		0x01
#define M2_IPDEFAULTTTL		0x02

typedef struct
    {
    long            ipForwarding;
    long            ipDefaultTTL;
    unsigned long   ipInReceives;
    unsigned long   ipInHdrErrors;
    unsigned long   ipInAddrErrors;
    unsigned long   ipForwDatagrams;
    unsigned long   ipInUnknownProtos;
    unsigned long   ipInDiscards;
    unsigned long   ipInDelivers;
    unsigned long   ipOutRequests;
    unsigned long   ipOutDiscards;
    unsigned long   ipOutNoRoutes;
    long            ipReasmTimeout;
    unsigned long   ipReasmReqds;
    unsigned long   ipReasmOKs;
    unsigned long   ipReasmFails;
    unsigned long   ipFragOKs;
    unsigned long   ipFragFails;
    unsigned long   ipFragCreates;
    unsigned long   ipRoutingDiscards;

    } M2_IP;


/* IP Address Table group */

typedef struct
    {
    unsigned long   ipAdEntAddr;
    long            ipAdEntIfIndex;
    unsigned long   ipAdEntNetMask;
    long            ipAdEntBcastAddr;
    long            ipAdEntReasmMaxSize;

    } M2_IPADDRTBL;

/* IP Routing Table group */

typedef struct
    {
    unsigned long   ipRouteDest;
    long            ipRouteIfIndex;
    long            ipRouteMetric1;
    long            ipRouteMetric2;
    long            ipRouteMetric3;
    long            ipRouteMetric4;
    unsigned long   ipRouteNextHop;
    long            ipRouteType;   
    long            ipRouteProto;
    long            ipRouteAge;
    unsigned long   ipRouteMask;
    long            ipRouteMetric5;
    M2_OBJECTID     ipRouteInfo;

    } M2_IPROUTETBL;

/* IP route table entry bit fields that map to variables that can be set */

#define M2_IP_ROUTE_DEST 		1
#define M2_IP_ROUTE_NEXT_HOP		2
#define M2_IP_ROUTE_TYPE		4

/* IP Address Translation Table group */

typedef struct
    {
    long            ipNetToMediaIfIndex;
    M2_PHYADDR 	    ipNetToMediaPhysAddress;
    unsigned long   ipNetToMediaNetAddress;
    long            ipNetToMediaType; 

    } M2_IPATRANSTBL;

/* ICMP group */

typedef struct
    {
    unsigned long   icmpInMsgs;
    unsigned long   icmpInErrors;
    unsigned long   icmpInDestUnreachs;
    unsigned long   icmpInTimeExcds;
    unsigned long   icmpInParmProbs;
    unsigned long   icmpInSrcQuenchs;
    unsigned long   icmpInRedirects;
    unsigned long   icmpInEchos;
    unsigned long   icmpInEchoReps;
    unsigned long   icmpInTimestamps;
    unsigned long   icmpInTimestampReps;
    unsigned long   icmpInAddrMasks;
    unsigned long   icmpInAddrMaskReps;
    unsigned long   icmpOutMsgs;
    unsigned long   icmpOutErrors;
    unsigned long   icmpOutDestUnreachs;
    unsigned long   icmpOutTimeExcds;
    unsigned long   icmpOutParmProbs;
    unsigned long   icmpOutSrcQuenchs;
    unsigned long   icmpOutRedirects;
    unsigned long   icmpOutEchos;
    unsigned long   icmpOutEchoReps;
    unsigned long   icmpOutTimestamps;
    unsigned long   icmpOutTimestampReps;
    unsigned long   icmpOutAddrMasks;
    unsigned long   icmpOutAddrMaskReps;

    } M2_ICMP;


/* TCP Group */

typedef struct 
    {
    long            tcpRtoAlgorithm;
    long            tcpRtoMin;
    long            tcpRtoMax;
    long            tcpMaxConn;
    unsigned long   tcpActiveOpens;
    unsigned long   tcpPassiveOpens;
    unsigned long   tcpAttemptFails;
    unsigned long   tcpEstabResets;
    unsigned long   tcpCurrEstab;
    unsigned long   tcpInSegs;
    unsigned long   tcpOutSegs;
    unsigned long   tcpRetransSegs;
    unsigned long   tcpInErrs;
    unsigned long   tcpOutRsts;

    } M2_TCPINFO;


/* TCP Connection Table Entry */

typedef struct
    {
    long            tcpConnState;
    unsigned long   tcpConnLocalAddress;
    long            tcpConnLocalPort;
    unsigned long   tcpConnRemAddress;
    long            tcpConnRemPort;

    } M2_TCPCONNTBL;


/* User Datagram Protocol Group */

typedef struct
    {
    unsigned long   udpInDatagrams;
    unsigned long   udpNoPorts;
    unsigned long   udpInErrors;
    unsigned long   udpOutDatagrams;

    } M2_UDP;

/* UDP Connection Table Entry */

typedef struct
    {
    unsigned long   udpLocalAddress;
    long            udpLocalPort;

    } M2_UDPTBL;


/* function declarations */
 
 #if defined(__STDC__) || defined(__cplusplus)

extern STATUS m2SysInit (char *	mib2SysDescr, char * mib2SysContact,
		 char *	mib2SysLocation, M2_OBJECTID * pObjectId);

extern STATUS m2SysGroupInfoGet (M2_SYSTEM * pSysInfo);
extern STATUS m2SysGroupInfoSet (unsigned int varToSet, M2_SYSTEM * pSysInfo);
extern STATUS m2SysDelete (void);

extern STATUS m2IfInit (FUNCPTR pTrapRtn, void * pTrapArg);

extern STATUS m2IfGroupInfoGet (M2_INTERFACE * pIfInfo);
extern STATUS
	   m2IfTblEntryGet (int search, M2_INTERFACETBL * pIfTblEntry);
extern STATUS
           m2IfTblEntrySet (M2_INTERFACETBL * pIfTblEntry);

extern STATUS m2IfDelete (void);

extern STATUS m2IpInit (int maxRouteTableSize);
extern STATUS m2IpGroupInfoGet (M2_IP * pIpInfo);
extern STATUS m2IpGroupInfoSet (unsigned int varToSet, M2_IP * pIpInfo);

extern STATUS
	   m2IpAddrTblEntryGet (int search, M2_IPADDRTBL * pIpAddrTblEnry);

extern STATUS
m2IpRouteTblEntryGet (int   search, M2_IPROUTETBL * pIpRouteTblEntry);

extern STATUS
m2IpRouteTblEntrySet (int varToSet, M2_IPROUTETBL * pIpRouteTblEntry);

extern STATUS
    m2IpAtransTblEntryGet (int search, M2_IPATRANSTBL * pIpAtEntry);
extern STATUS
    m2IpAtransTblEntrySet (M2_IPATRANSTBL * pIpAtEntry);
extern STATUS m2IpDelete (void);

extern STATUS m2IcmpInit (void);
extern STATUS m2IcmpGroupInfoGet (M2_ICMP * pIcmpInfo);

extern STATUS m2TcpInit (void);
extern STATUS m2TcpGroupInfoGet (M2_TCPINFO * pTcpInfo);

extern STATUS m2TcpConnEntryGet (int search, M2_TCPCONNTBL * pTcpConnEntry);
extern STATUS m2TcpConnEntrySet (M2_TCPCONNTBL * pTcpConnEntry);

extern STATUS m2UdpInit (void);
extern STATUS m2UdpGroupInfoGet (M2_UDP * pUdpInfo);

extern STATUS m2UdpTblEntryGet (int search, M2_UDPTBL * pUdpEntry);
STATUS m2Init (char * mib2SysDescr, char * mib2SysContact, 
               char * mib2SysLocation, M2_OBJECTID * pMib2SysObjectId,
	       FUNCPTR pTrapRtn, void * pTrapArg, int maxRouteTableSize);
extern STATUS m2Delete (void);
extern STATUS m2TcpDelete (void);
extern STATUS m2UdpDelete (void);
extern STATUS m2IcmpDelete (void);

#else   /* __STDC__ */

extern STATUS m2SysInit ();
extern STATUS m2SysGroupInfoGet ();
extern STATUS m2SysGroupInfoSet ();
extern STATUS m2SysDelete ();
extern STATUS m2IfInit ();
extern STATUS m2IfGroupInfoGet ();
extern STATUS m2IfTblEntryGet ();
extern STATUS m2IfTblEntrySet ();
extern STATUS m2IfDelete ();
extern STATUS m2IpInit ();
extern STATUS m2IpGroupInfoGet ();
extern STATUS m2IpGroupInfoSet ();
extern STATUS m2IpAddrTblEntryGet ();
extern STATUS m2IpRouteTblEntryGet ();
extern STATUS m2IpRouteTblEntrySet ();
extern STATUS m2IpAtransTblEntryGet ();
extern STATUS m2IpAtransTblEntrySet ();
extern STATUS m2IpDelete ();
extern STATUS m2IcmpInit ();
extern STATUS m2IcmpGroupInfoGet ();
extern STATUS m2TcpInit ();
extern STATUS m2TcpGroupInfoGet ();
extern STATUS m2TcpConnEntryGet ();
extern STATUS m2TcpConnEntrySet ();
extern STATUS m2UdpInit ();
extern STATUS m2UdpGroupInfoGet ();
extern STATUS m2UdpTblEntryGet ();
extern STATUS m2Init ();
extern STATUS m2Delete ();
extern STATUS m2TcpDelete ();
extern STATUS m2UdpDelete ();
extern STATUS m2IcmpDelete ();
#endif  /* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCm2Libh */
