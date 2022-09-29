/* m2Lib.h - VxWorks MIB-II interface to SNMP Agent */

/* Copyright 1984-1993 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01a,08dec93,jag  written
*/

#ifndef __INCm2LibPh
#define __INCm2LibPh

#ifdef __cplusplus
extern "C" {
#endif

#include <socket.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* This structure is used to keep track of the  Network Interfaces */

typedef struct
    {
    int              netIfIndex;      /* I/F index returned to SNMP */
    long             netIfType;       /* I/F type from MIB-II specifications */
    unsigned long    netIfSpeed;      /* I/F Speed from MIB-II specifications */
    unsigned long    netIfAdminStatus;  /* I/F old status used for traps */
    unsigned long    netIfLastChange;   /* I/F Time of Interface last change */
    M2_OBJECTID      netIfOid;          /* I/F Object ID */
    struct arpcom  * pNetIfDrv;         /* Pointer to BSD Network Driver I/F */
 
    } NET_IF_TBL;

/*
 *  These IOCTL commands are used exclusively by the mib2Lib.c module to retrive
 *  MIB-II parameters from network drivers.
 */
 
#define SIOCGMIB2CNFG   _IOR('m',  0, int)            /* Get Configuration */
#define SIOCGMIB2CNTRS  _IOR('m',  1, int)            /* Get driver counters */

/*
 * This structure is used to obtain the configuration information from the
 * network drivers.  This information is static and does not change through the
 * life time of the driver. 
 */
 
typedef struct
    {
    long            ifType;
    M2_OBJECTID     ifSpecific;
 
    } M2_NETDRVCNFG;
 
/*
 * This structure is used to retrive counters from the network driver.  The
 * variable ifSpeed is included here to support devices that can compute the
 * nominal bandwidth.
 */
 
typedef struct
    {
    unsigned long   ifSpeed;
    unsigned long   ifInOctets;
    unsigned long   ifInNUcastPkts;
    unsigned long   ifInDiscards;
    unsigned long   ifInUnknownProtos;
    unsigned long   ifOutOctets;
    unsigned long   ifOutNUcastPkts;
    unsigned long   ifOutDiscards;
 
    } M2_NETDRVCNTRS;
 
 
/* 
 * Imported from if.c, this flag is used to notify the MIB-II I/F that a 
 * change in the network interface list has occured.  The table will be read
 * again.
 */
extern int                ifAttachChange;

extern NET_IF_TBL * pm2IfTable;  /* Network I/F table (Allocated Dynamically) */
extern int 	     m2IfCount;
extern M2_OBJECTID zeroObjectId;

#if defined(__STDC__) || defined(__cplusplus)

extern int m2NetIfTableRead (void);

#else   /* __STDC__ */

extern int m2NetIfTableRead ();

#endif  /* __STDC__ */

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCm2LibPh */
