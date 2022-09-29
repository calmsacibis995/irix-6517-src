/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1998, Silicon Graphics, Inc                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* $Revision: 1.2 $    $Date: 1999/08/17 20:38:40 $ */

/*
 * st_ifnet.h
 *
 * This file contains structures and defines for the Scheduled Transfer 
 * (ST) protocol interface to device drivers.
 *
 * During driver initialization, a call is made to st_if_attach()
 * either explicitly or implicitly through if_attach() to inform
 * the ST stack of the adapter's ST parameters. 
 *
 * During normal 
 * operation, several ST resources need to be allocated and 
 * de-allocated per adapter, including ports, bufx's,
 * Mx's, and keys. The device driver interface assumes the 
 * upper-layer-protocol manages allocation and deallocation from the name
 * space provided for a given resource. Thus handles for a specific
 * resource are provided on a per interface basis to:
 *
 *	allow the device to specify valid ranges
 *	provide an opaque pointer for the resource allocator to 
 *		store interface specific state.
 *	provide a lock to control access to the interface specific state
 *
 * In addition, once the resources have been allocated, they can
 * be set and cleared in the adapter. This is done through the
 * procedure handles for each resource. Locking must be done internal to
 * the driver if a lock is required. Note that the if_st_get_* functions
 * return the value that was set - it does not assign a value.
 *
 */

#ifndef ___ST_IF_H
#define ___ST_IF_H

#if defined(_KERNEL) || defined(_ST_PRIVATE) || defined(_KMEMUSER)

#include <netinet/st.h>		/* needed for the st_hdr_t struct */
#include <netinet/in.h>
#include <net/route.h>		/* needed for struct route for in_pcb.h*/
#include <netinet/in_pcb.h>	/* needed for struct inaddrpair */
#include <net/if.h>

/*
 * ULP Bufx Flags used in if_st_set/get/clear_bufx()
 */
#define ST_BUFX_SRC     (1<<0)		/* bufx is for source */
#define ST_BUFX_DST     (1<<1)		/* bufx is for destination */

/*
 * ULP Port Flags used in if_st_set/get/clear_port()
 */
#define ST_STANDARD_PORT  (1<<0)
#define ST_APPEND_PORT    (1<<1)

/*
 * st_macaddr_t is passed to the ULP on receive as an opaque cookie
 * that can then be turned around to do ST Striping per the ST Spec.
 */

typedef struct {
	uint64_t	dst_mac;    /* MAC opaque cookie */
	uint64_t	src_mac;    /* MAC opaque cookie */
} st_macaddr_t;


/*
 * Struct used to initialize an Mx (if_st_set/get/clear_mx)
 */

typedef struct {
	uint32_t    base_spray;		/* spray-width - usually 0 */
	uint32_t    bufx_base;		/* starting bufx */
	uint32_t    bufx_num;		/* num of valid bufx's from bufx_base */
	uint32_t    key;		/* set to 0 to invalidate */
	uint16_t    bufsize;		/* as a power of 2 */
	uint16_t    poison       :1,	/* driver side invalidate */
		    reserved     :15;	
	uint16_t    port;
	uint16_t    stu_num;		/* starting stu_num (normally 0) */
} st_mx_t;


/* Structure used to setup and teardown a port */

typedef struct {
	/*-- TX Parameters --*/
	uint32_t bufx_base; 
	uint32_t bufx_num;
	uint16_t src_bufsize;  /* as a pow of 2 */
	uint16_t reserved;

	uchar_t  vc_fifo_credit[ST_MAX_VC];

	/*-- RX Parameters --*/
	uint32_t key;
	uint32_t  bypass	 :1,
		 reserved1       :31;
	uint32_t ddq_size;  /* in bytes */ 
	uint64_t ddq_addr;  /* destination descriptor Q */
} st_port_t; 

#define ST_MAX_SRC_IP_ALIASES	8

/*
 * Receive descriptor into ST stack. Note that the full receive
 * descriptor is a struct ifheader immediately followed by an
 * st_rx_desc_t.
 */

typedef struct {
	struct inaddrpair iap;
	st_macaddr_t	  stmac;
	sthdr_t	   	  sth;
	int32_t		  num_alias;
	struct in_addr	  ip_alias[ST_MAX_SRC_IP_ALIASES];
} st_rx_desc_t;


/* Structure used to describe a transmit operation. Source address
 * can be in either bufx:offset units or kernel virtual/physical address.
 * It is placed in an mbuf at tx_desc_off from the beginning of the 
 * data portion of the mbuf for if_st_output().
 */

/*
 * ULP Desc flags
 */
#define ST_IF_TX_RAW	 	(1<<0)	/* raw format */
#define ST_IF_TX_CTL_MSG     	(1<<1)	/* ST control msg */
#define ST_IF_TX_ADDR_VADDR  	(1<<2)	/* kernel vaddr, not bufx:offset used */
#define ST_IF_TX_APPEND_PAYLOAD (1<<3)	/* append or optional payload is valid*/
#define ST_IF_TX_CONN_MSG    	(1<<4)	/* this is a connection message */
#define ST_IF_TX_ACK	 	(1<<5)	/* need an ack on this message */
#define ST_IF_USER_FIFO      	(1<<6)	/* use the USER tx fifos */
#define ST_IF_TX_MAC_ADDR    	(1<<7)	/* MAC address is valid */

typedef struct {
	uint32_t	vc : 2,		/* which vc to transmit on */
			flags : 30;	/* see above */
	uint16_t	dst_bufsize;	/* pow of 2-for tiling into stus */
	uint16_t	max_STU;	/* pow of 2-max STU to gen for tiling */

	int32_t	 	len;		/* length of message in bytes */
	uint32_t	token;		/* tx done opaque token to be returned*/
	uint32_t	bufx;		/* bufx:offset of source buffer */
	uint32_t	offset;
	caddr_t	 	addr;		/* address of source buffer */
	uint64_t	dst_MAC;	/* opaque (to ulp) cookie*/
} st_tx_desc_t;

/* Structure to specify how many memories a single packet will be 
 * sprayed into. Normally adapters would only support a single
 * memory, thus a spray width of 0. If multiple memories are used,
 * spray 128 bytes at a time, round-robin, into each memory.
 * Ex: Spraying 2-way (i.e. 2 memories)
 *
 * 	Offset	Length	Memory
 *      ----------------------
 *      0	128	0
 *      128	128	1
 *      256	128	0
 *      384	128	1
 *	etc.
 *
 * See comments at the beginning of this file for how locking is done.
 */


typedef struct {
	uint32_t bufx_base;
	int32_t  bufx_num;
	int32_t  bufx_tx_offset_align;	  /* Tx byte alignment required */
	lock_t	 bufx_lock;
	void 	 *bufx_table;
} st_spray_t;


/* Structure describing Scheduled Transfer information about an interface
 * which may be of interest to upper layer protocols. The structure 
 * is registered by st_if_attach().
 */

typedef struct st_ifnet_s {
	struct ifnet		*ifp;
	uint32_t		flags;
#define ST_IFF_CKSUM	     	(1 << 0)  /* driver does ST checksum */
#define ST_IFF_APPEND_PAYLOAD   (1 << 1)  /* driver does append payload */
#define ST_IFF_TILING	    	(1 << 2)  /* driver tiles blocks to stus */
#define ST_IFF_2_XTALK	   	(1 << 3)  /* adapter has 2 xtalk ports */

	int32_t			sp_family; /* protocol family, use PF_INET
					    * for IP encapsulation, 
					    * PF_STP for MAC encapsulation.
					    */

	/* Port allocator uses base/num/lock/table for 
	 * allocation/de-allocation of Ports.
	 * The driver initializes the base and num. The Port Allocator 
	 * initializes the lock and allocates the port_table.
	 */
	int32_t 		port_base_standard;
	int32_t 		port_num_standard;
	int32_t 		port_base_append_payload;
	int32_t 		port_num_append_payload;
	lock_t  		port_lock;   /* Mutex for port_table */
	void   			*port_table; /* opaque ptr for Port allocator */

	/* Mx allocator uses base/num/lock/table for 
	 * allocation/de-allocation of Mx's.
	 * The driver initializes the base and num. The Mx Allocator 
	 * initializes the lock and allocates the mx_table.
	 */
	int32_t 		mx_base;  /* Smallest Mx NIC supports */
	int32_t 		mx_num;	  /* Num Mx's supported by the NIC */
	lock_t  		mx_lock;  /* Mutex for mx_table */
	void   			*mx_table; /* opaque ptr for Mx allocator */

	int32_t 		tx_slot_vc[ST_MAX_VC]; /* max slots avail per tx vc */


	/* The bufx allocator takes the *_bufsize_vec as an input 
	 * bit vector of supported bufx sizes. 
	 * Note: we currently supports 16k, 64k, 256k, 1m, 4m, & 16m 
	 * bits 0-7 are not allowed by ST spec 
	 */
	uint32_t 		tx_bufsize_vec;
	uint32_t 		rx_bufsize_vec;

	/* Bufx allocator takes as input the bufx_spray array and 
	 * initializes it's state, potentially storing the data structure
	 * off the bufx_table void. Access to the structure to 
	 * allocate/deallocate is controlled via the bufx_lock.
	 */

#define ST_MAX_SPRAY 8
	st_spray_t      	bufx_spray[ST_MAX_SPRAY];

	int32_t  tx_max_stu;		  /* pow 2 - max STU size NIC can gen*/
	int32_t  rx_max_stu;		  /* pow 2 - max STU size NIC can rec */
					  /* NOTE: ST spec requires support for 
					   * 1 Byte STUs so don't need min */
	int32_t  tx_max_blk_size;	  /* pow 2. max block size can gen
					   * 1 byte blks for min */
	int32_t  rx_max_blk_size;	  /* pow 2. must be able to recieve
					   * 1 byte blks for min */

	int32_t  max_cts_outstanding;	  /* max CTS's NIC wants outstanding */

	/* offsets into the transmit descriptor to allow 
	 * adapter to avoid wholesale bcopies. Note that offsets 
	 * are from the beginning of the data portion of the mbuf.
	 */

	struct {
		int32_t  tx_desc_off;
		int32_t  sthdr_off;
		int32_t  append_payload_off;
		int32_t  append_payload_len;
		int32_t  raw_off;
	} tx_desc;

	/* procedure handles to setup/teardown ST adapter state */
	/* output routine */
	int32_t (*if_st_output)(struct st_ifnet_s *, struct mbuf *, 
				struct sockaddr *, struct rtentry *); 

	/* configuring ports */
	int32_t (*if_st_get_port)(struct st_ifnet_s *, int32_t, st_port_t *);
	int32_t (*if_st_set_port)(struct st_ifnet_s *, int32_t *,
				  st_port_t *, uint32_t);
	int32_t (*if_st_clear_port)(struct st_ifnet_s *, int);

	/* configuring mx's (aka B_id) */
	int32_t (*if_st_get_mx)(struct st_ifnet_s *, int32_t, st_mx_t *);
	int32_t (*if_st_set_mx)(struct st_ifnet_s *, int32_t, st_mx_t *);
	int32_t (*if_st_clear_mx)(struct st_ifnet_s *, int32_t);

	/* configuring Bufx's */
	int32_t (*if_st_get_bufx)(struct st_ifnet_s *, uint32_t, 
				paddr_t *, int32_t, uint32_t);
	int32_t (*if_st_set_bufx)(struct st_ifnet_s *, uint32_t, paddr_t *,
				  int32_t, int32_t, uint32_t, int32_t);
	int32_t (*if_st_clear_bufx)(struct st_ifnet_s *, uint32_t,
				    int32_t, uint32_t, int32_t);

	/* key allocation */
	int32_t (*if_st_get_key)(struct st_ifnet_s *, int32_t, uint32_t *);
} st_ifnet_t;

void st_if_attach(struct ifnet *, st_ifnet_t *);

#endif /* defined(_KERNEL) || defined(_ST_PRIVATE) || defined (_KMEMUSER) */

#endif /* ___ST_IF_H */

