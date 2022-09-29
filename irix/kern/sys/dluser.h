/*************************************************************************
 *                                                                       *
 *              Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                       *
 * These coded instructions, statements, and computer programs  contain  *
 * unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 * are protected by Federal copyright law.  They  may  not be disclosed  *
 * to  third  parties  or copied or duplicated in any form, in whole or  *
 * in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 *************************************************************************/

#ifndef __SYS_DLUSER_H__
#define __SYS_DLUSER_H__ 

#define DL_SAP_SIZE	sizeof(ulong)

#define ETH_ADDR_LENGTH	6

typedef struct {
	unchar  addr[ETH_ADDR_LENGTH];
} eth_addr_t;


/*
 * The dl_sap_length is + and thus implies that the
 * DLSAP look like the diagram below:
 *
 * |-----------|------------------|
 * | SAP PART  | MAC ADDRESS PART |
 * |-----------|------------------|
 *
 * |<-sap_len->|
 *
 * |<----- dl_addr_length ------->|
 */

typedef struct {
	ulong		sap;
	eth_addr_t	mac;
} eth_dlsap_t;

#define mac_addr		mac.addr

/*
 * Note: DL_SAP_ADDR_LEN is the value of dl_addr_length that
 *	 is returned by info_ack etc.  The value of DL_TOTAL_LEN
 * 	 should be used when incrementing pointers because it
 *	 will word align the sap part if the message contains a
 *	 source address followed by an destination address.
 */
#define DL_SAP_ADDR_LEN		sizeof(eth_addr_t) + DL_SAP_SIZE
#define DL_TOTAL_LEN		sizeof(eth_dlsap_t)

#define SNIF_LOCAL_IOCTL        ('s' << 16 | 'n' << 8)
#define SNIF_DEBUG              (SNIF_LOCAL_IOCTL|0x01)

typedef struct {
	ulong   level;
} snif_debug_t;

#define SNIF_DEBUG_SIZE         sizeof(snif_debug_t)

#endif
