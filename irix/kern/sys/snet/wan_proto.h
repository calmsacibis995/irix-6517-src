#ifndef INCwan_protoh
#define INCwan_protoh

/*-----------------------------------------------------------------------------
 * wan_proto.h - SpiderX25 Protocol Streams Component
 *
 *  Copyright 1991  Spider Systems Limited
 *
 * Copyrighted as an unpublished work.
 * (c) 1992-96 SBE, Inc. and by other vendors under license agreements.
 * All Rights Reserved.
 *-----------------------------------------------------------------------------
 * RCS info:
 * RCS revision: $Revision: 1.5 $
 * Last changed on $Date: 1996/07/15 18:14:04 $
 * Changed by $Author: wli $
 *-----------------------------------------------------------------------------
 * $Log: wan_proto.h,v $
 * Revision 1.5  1996/07/15 18:14:04  wli
 * new header file to support SBE PCI board only.
 *
 * Revision 2.0  96/04/08  15:31:21  rickd
 * Updated to support PCI-360 WAN-Only using Shared Memory Buffers.
 * All chars/shorts promoted to longs to make Little Endian byte-swap
 * of cross-bus values (IE.ioctls) easier to maintain and to code for in the
 * PDM (Protocol Download Module).  Clean LOG messages. Redo Copyright notice.
 * 
 *-----------------------------------------------------------------------------
 */


#include <sys/types.h>

/*
    WAN primitive type values
*/
#define WAN_SID           1
#define WAN_REG           2
#define WAN_CTL           3
#define WAN_DAT           4

/*
    WAN_CTL type command values/events pairings
*/
#define WC_CONNECT        1
#define WC_CONCNF         2
#define WC_DISC           3
#define WC_DISCCNF        4

#define wconnect          0x01
#define wconcnf           0x02
#define wdisc             0x04
#define wdisccnf          0x08

/*
    WAN_DAT type command values
*/
#define WC_TX             1
#define WC_RX             2

/*
    Values for wan_status field
*/
#define WAN_FAIL          0
#define WAN_SUCCESS       1

/*
    Values for wan_remtype field
*/
#define WAN_TYPE_ASC    0      /* Ascii length in octets       */
#define WAN_TYPE_BCD    1      /* BCD   length in semi octets  */


/*
    SETSNID message for WAN
*/
struct wan_sid
{
    uint32_t wan_type;           /* WAN_SID                  */
    uint32_t wan_snid;           /* Subnetwork ID for stream */
};

/*
    Registration message for WAN
*/
struct wan_reg
{
    uint32_t wan_type;           /* WAN_REG                        */
    uint32_t wan_snid;           /* Subnetwork ID for the WAN line */
};

/*
    WAN Connection Control
*/
struct wan_ctl
{
    uint32_t wan_type;           /* WAN_CTL                        */
    uint32_t wan_command;        /* WC_CONNECT/CONCNF/DISC/DISCCNF */
    uint32_t wan_remtype;        /* Octets or semi octets          */
    uint32_t wan_remsize;        /* Remote address length          */
    uint8_t wan_remaddr[20];     /* Remote address         */
    uint32_t wan_status;         /* Result status                  */
    uint32_t wan_diag;           /* Extra Diagnostic               */
};

/*
    WAN Data messages - Transmit and Receive
*/
struct wan_msg
{
    uint32_t wan_type;           /* WAN_DAT     */
    uint32_t wan_command;        /* WC_TX/WC_RX */
};


/*
    Generic WAN protocol primitive
*/
union WAN_primitives
{
    uint32_t wan_type;           /* Variant type             */
    struct wan_reg wreg;       /* WAN Registration         */
    struct wan_sid wsid;       /* WAN Subnetwork ID        */
    struct wan_ctl wctl;       /* WAN Connection Control   */
    struct wan_msg wmsg;       /* WAN Data Message         */
};

#endif                         /* INCwan_protoh */

/*
 *  End Of File
 */
