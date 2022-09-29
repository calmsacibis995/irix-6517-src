/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996 
 * All rights reserved
 * 
 */
/*
 * HISTORY
 * $Log: tigon_api.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision 1.1.6.55  1999/01/16  02:55:32  shuang
 * 	add new flag RCB_FLAG_RING_DISABLED
 * 	[1999/01/16  02:49:49  shuang]
 *
 * Revision 1.1.6.54  1998/12/08  02:36:18  shuang
 * 	revise comment
 * 	[1998/12/08  02:32:16  shuang]
 * 
 * Revision 1.1.6.53  1998/10/14  01:41:42  shuang
 * 	add VOLATILE to tx_buf_ratio field
 * 	[1998/10/14  01:38:22  shuang]
 * 
 * Revision 1.1.6.52  1998/10/14  01:14:20  shuang
 * 	support tx buffer ratio
 * 	[1998/10/14  01:06:42  shuang]
 * 
 * Revision 1.1.6.51  1998/10/05  21:20:33  dfoy
 * 	Corrected the typedef tg_event_t(missing 'typedef').
 * 	[1998/10/05  21:02:59  dfoy]
 * 
 * Revision 1.1.6.50  1998/09/30  18:49:38  shuang
 * 	change for new 12.3 API
 * 	[1998/09/30  18:39:06  shuang]
 * 
 * Revision 1.1.6.49  1998/09/02  21:00:31  ted
 * 	12.2.1 new features and doc updates
 * 	[1998/08/25  01:57:23  ted]
 * 
 * Revision 1.1.6.48  1998/08/27  23:22:33  shuang
 * 	add new mcast commands, mbox commands, interrupt masks
 * 	[1998/08/27  23:16:39  shuang]
 * 
 * Revision 1.1.6.47  1998/08/03  16:47:14  hayes
 * 	checkin so merge w/ fc can happen
 * 	[1998/07/29  23:15:20  hayes]
 * 
 * 	host based send ring now works off RCB flag
 * 	[1998/07/29  18:30:49  hayes]
 * 
 * 	host based send ring works
 * 	[1998/07/28  03:57:11  hayes]
 * 
 * Revision 1.1.6.46  1998/07/29  00:07:19  shuang
 * 	add feature - send coallescing BD update only
 * 	[1998/07/29  00:02:30  shuang]
 * 
 * 	add feature - send coallescing BD now
 * 	[1998/07/25  02:28:33  shuang]
 * 
 * Revision 1.1.6.45  1998/05/18  20:09:20  ted
 * 	Add link state code for 10/100 PHY
 * 	[1998/05/18  20:00:23  ted]
 * 
 * Revision 1.1.6.44  1998/04/14  21:42:24  ted
 * 	Remove unnecessary DMA type and add little endian support to data structures
 * 	[1998/04/14  21:20:40  ted]
 * 
 * Revision 1.1.6.43  1998/04/01  22:21:19  hayes
 * 	ready for merge
 * 	[1998/04/01  00:57:57  hayes]
 * 
 * Revision 1.1.6.42  1998/04/01  21:25:39  ted
 * 	Solaris driver now handled changed tick default (1ms vs. 100 ms.)
 * 	[1998/04/01  21:25:11  ted]
 * 
 * Revision 1.1.6.41  1998/04/01  07:32:16  shuang
 * 	add define: BD_error_flags, BD_ERR_SHIFT...
 * 	[1998/04/01  06:50:23  shuang]
 * 
 * Revision 1.1.6.40  1998/04/01  00:43:14  skapur
 * 	Adding support for Vlan Tag insertion/deletion support
 * 	[1998/03/25  19:11:42  skapur]
 * 
 * Revision 1.1.6.39  1998/03/26  01:34:16  shuang
 * 	Added nicEnqEventDelayed statcounter
 * 	[1998/03/26  01:19:30  shuang]
 * 
 * Revision 1.1.6.38  1998/03/13  23:06:33  ted
 * 	Fix bad C, sigh
 * 	[1998/03/13  23:06:19  ted]
 * 
 * Revision 1.1.6.37  1998/03/13  23:02:46  ted
 * 	Add support for 10/100 link API and NetScout
 * 	[1998/03/13  23:02:28  ted]
 * 
 * Revision 1.1.6.36  1998/03/08  02:07:28  ted
 * 	Rearrange gencom and tuneparms structures, add VLAN assistance data structures
 * 	[1998/03/08  02:05:01  ted]
 * 
 * Revision 1.1.6.35  1998/03/04  22:28:40  hayes
 * 	enable scratchpad usage, including stats refresh
 * 	[1998/03/04  22:20:36  hayes]
 * 
 * Revision 1.1.6.34  1998/02/19  01:29:40  suresh
 * 	Add new DMA types so hardcoded hex values can be removed
 * 	[1998/02/19  01:13:29  suresh]
 * 
 * Revision 1.1.6.33  1998/02/11  19:11:11  ted
 * 	Add reset jumbo Ring data structures
 * 	[1998/02/11  18:27:59  ted]
 * 
 * Revision 1.1.6.32  1998/02/05  21:27:36  ted
 * 	Add dontFragJumbos flag
 * 	[1998/02/05  21:27:13  ted]
 * 
 * Revision 1.1.6.31  1998/02/03  21:51:26  ted
 * 	Add codes for TG_EVENT_MULTICAST_LIST_UPDATED
 * 	[1998/02/03  21:50:02  ted]
 * 
 * Revision 1.1.6.30  1998/01/30  01:34:06  ted
 * 	Add linkState field for feedback of link negotiation status
 * 	[1998/01/30  01:33:47  ted]
 * 
 * Revision 1.1.6.29  1998/01/29  02:33:01  skapur
 * 	Merged for rev6 checksum
 * 	[1998/01/29  02:29:29  skapur]
 * 
 * 	Adding a dummy DMA Entry type
 * 	[1998/01/28  23:56:50  skapur]
 * 
 * Revision 1.1.6.28  1998/01/24  04:06:40  venkat
 * 	moved SEND_RING_ENTRIES to alttypes.h
 * 	[1998/01/24  04:05:25  venkat]
 * 
 * Revision 1.1.6.27  1997/12/16  23:04:39  skapur
 * 	Added new flags for Checksum off-load
 * 	[1997/12/16  22:51:02  skapur]
 * 
 * Revision 1.1.6.26  1997/11/25  22:06:56  ted
 * 	Clean up statistics and antiquated stuff.
 * 	[1997/11/25  21:56:18  ted]
 * 
 * Revision 1.1.6.25  1997/11/19  19:22:13  ted
 * 	Support Tigon and Tigon2 in the same driver
 * 	[1997/11/19  19:15:17  ted]
 * 
 * Revision 1.1.6.24  1997/11/19  00:13:40  ted
 * 	merged from release 2.0
 * 	[1997/11/19  00:10:33  ted]
 * 
 * Revision 1.1.6.23  1997/11/18  01:33:58  hayes
 * 	correct size of tg_stats structure, remove obsolete code
 * 	[1997/11/18  01:33:01  hayes]
 * 
 * Revision 1.1.6.22  1997/11/14  19:16:09  hayes
 * 	first checkin after mongo merge...
 * 	[1997/11/14  19:12:06  hayes]
 * 
 * Revision 1.1.6.21  1997/11/10  00:27:23  hayes
 * 	fix mcast entries for Tigon2
 * 	[1997/11/10  00:20:39  hayes]
 * 
 * Revision 1.1.6.20  1997/10/21  22:55:56  hayes
 * 	add new TRACE_ flags
 * 	[1997/10/21  22:55:35  hayes]
 * 
 * Revision 1.1.6.19  1997/10/21  01:35:28  skapur
 * 	Merge of release 2.0 and ge_latest branch
 * 	[1997/10/21  01:35:03  skapur]
 * 
 * Revision 1.1.6.18  1997/09/16  21:46:21  ted
 * 	Change from MIB-II V1 (RFC 1213) to MIB-II V2 (RFC 1573)
 * 	[1997/09/16  21:41:44  ted]
 * 
 * Revision 1.1.6.17  1997/09/16  03:38:09  skapur
 * 	Merged with changes from 1.1.6.16
 * 	[1997/09/16  03:38:00  skapur]
 * 
 * 	Added stats for Jumbo Frames
 * 	[1997/09/16  03:36:49  skapur]
 * 
 * Revision 1.1.6.16  1997/09/16  03:10:59  ted
 * 	Add new commands for resetting the stats set up by the host and for
 * 	removing the jumbo ring entries (reset of the jumbo ring indices).
 * 	Renumber the BD_TYPES to make things be back in order.
 * 	[1997/09/16  02:58:36  ted]
 * 
 * Revision 1.1.6.15  1997/08/23  17:39:59  skapur
 * 	Solaris jumbogram support
 * 	[1997/08/23  17:37:41  skapur]
 * 
 * Revision 1.1.6.14  1997/07/29  03:40:55  ted
 * 	Add OP1 LED support
 * 	[1997/07/29  03:39:58  ted]
 * 
 * Revision 1.1.6.13  1997/07/25  22:58:48  ted
 * 	Merge fixes from ge_0_22 and add link negotiation support
 * 	[1997/07/25  22:31:40  ted]
 * 
 * 	Architectural performance change
 * 	[1997/07/24  21:44:52  ted]
 * 
 * Revision 1.1.6.11  1997/06/17  00:36:32  ted
 * 	tighten up driver code for better performance
 * 	[1997/06/06  22:38:11  ted]
 * 
 * Revision 1.1.6.10  1997/05/22  22:25:23  suresh
 * 	Statistics tweak - remove nicCmdsGetStats from tg_stats
 * 	Also bump up nicCmdsClearStats counter in the fw when stats cleared
 * 	via "altstat -C". Adjust altstat.c to print nicCmdsClearStats counter and
 * 	not print nicCmdsGetStats.
 * 	[1997/05/22  22:24:32  suresh]
 * 
 * Revision 1.1.6.9  1997/05/21  22:59:47  ted
 * 	Minor statistics tweaks
 * 	[1997/05/21  22:59:03  ted]
 * 
 * Revision 1.1.6.8  1997/05/03  05:00:17  ted
 * 	Fix up statistics
 * 	[1997/05/03  04:56:36  ted]
 * 
 * Revision 1.1.6.7  1997/04/25  18:48:42  ted
 * 	Replace single dma_config with dma_read_config and dma_write_config
 * 	to allow for future assymetric utilization of the DMA state registers.
 * 	[1997/04/25  18:32:04  ted]
 * 
 * Revision 1.1.6.6  1997/04/11  23:30:51  ted
 * 	Padded Recv BD to make it 32 byte long; added Union into send_bd struct; Added stats for Dma read/write MasterAborts; defined new bits for 32bit mode and dma 1 active channels to support SBUS
 * 	[1997/04/11  23:30:32  ted]
 * 
 * Revision 1.1.6.5  1997/03/28  02:46:51  skapur
 * 	Added 5 new NIC counters to keep track of NIC Resources
 * 	[1997/03/28  01:51:03  skapur]
 * 
 * Revision 1.1.6.4  1997/03/24  08:34:12  ted
 * 	New event ring handling, producer lives in host memory, misc. other more modest changes
 * 	[1997/03/24  08:27:23  ted]
 * 
 * Revision 1.1.6.3  1997/03/21  04:15:29  skapur
 * 	added new BD for send_ring; renamed the old one to recv_bd
 * 	[1997/03/21  04:14:59  skapur]
 * 
 * Revision 1.1.6.2  1997/03/17  08:47:36  skapur
 * 	add mac hardware stats and second recv ring.
 * 	[1997/03/17  08:32:59  skapur]
 * 
 * Revision 1.1.6.1  1997/02/23  00:22:11  ted
 * 	Added bunch of new Mac statistics
 * 	[1997/02/23  00:21:43  ted]
 * 
 * Revision 1.1.2.2  1996/12/18  03:20:10  skapur
 * 	Added new literals to support two new commands, namely,
 * 	TG_CMD_CLEAR_STATS, and TG_CMD_SET_MULTICAST_MODE.
 * 
 * 	Additionally added two new literals to the stats area
 * 	something like nicCmdsClearStats and nicCmdsSetMulticastMode.
 * 	[1996/12/18  03:19:49  skapur]
 * 
 * Revision 1.1.2.1  1996/11/20  20:04:06  ted
 * 	This used to be tg_host.h.  Rename.
 * 	[1996/11/20  19:46:03  ted]
 * 
 * $EndLog$
 */
/*
 * FILE nic_api.h
 *
 * COPYRIGHT (c) Essential Communication Corp. 1995
 *
 * Confidential to Alteon Networks, Inc.
 * Do not copy without Authorization
 *
 * $Source: /proj/irix6.5.7m/isms/irix/kern/bsd/sys/RCS/tigon_api.h,v $
 * $Revision: 1.2 $ $Locker:  $
 * $Date: 1999/05/18 21:44:02 $ $Author: trp $ $State: Exp $
 */

#ifndef _NIC_API_H_
#define _NIC_API_H_


/*
 * these define the layout and data structures used in the TIGON <-> HOST
 * interface.  The data structures defined by the hardware can be found
 * in tg.h
 */

/* first some constants for ring sizes */
#define EVENT_RING_ENTRIES         256
#define COMMAND_RING_ENTRIES        64
#define RECV_STANDARD_RING_ENTRIES 512
#define RECV_JUMBO_RING_ENTRIES    256
#define RECV_MINI_RING_ENTRIES     1024
#define RECV_RETURN_RING_ENTRIES   2048

/*
 * how many multicast addresses do we support?
 * MAX_MCAST_ENTRIES must be at least MAC_HW_MCAST_ENTRIES
 */
#define MAX_HW_MCAST_ENTRIES_R5	128 /* This is how much HW supports */
#define MAX_HW_MCAST_ENTRIES_R4	 32 /* This is how much HW supports */
#define MAX_MCAST_ENTRIES	128 /* This is how much SW supports */

/*
 * fundamental hardtimer time
 */
#define TG_TICK_TIME	 	1 /* usecs */
#define TG_TICKS_PER_SEC	(1000000 / TG_TICK_TIME)

/*
 * Trace size indication to be shared with host and NIC
 */
#define ALT_TRACE_ELEM_SIZE	8 /* size (in u_longs) of trace entry */
#define ALT_NTRACE_SIZE		((8*1024) / (ALT_TRACE_ELEM_SIZE*sizeof(U32)))
 
/*
 * The rings are composed of a ring control block and an array of
 * buffer descriptors.
 */

typedef struct ring_control_block {
    tg_hostaddr_t ring_addr;
    union {
	struct {
#if ALT_BIG_ENDIAN
	    U16 max_len;
	    U16 flags;
#else /* ALT_BIG_ENDIAN */
	    U16 flags;
	    U16 max_len;
#endif /* ALT_BIG_ENDIAN */
	} r_f;
	U32 w2;
    } w2;
    U32 res2;
} ring_control_block_t;

#define RCB_flags	w2.r_f.flags
#define RCB_max_len	w2.r_f.max_len
#define RCB_w2		w2.w2

#define RCB_FLAGS_MASK			0xffff
#define RCB_FLAG_TCP_UDP_CKSUM  	0x0001
#define RCB_FLAG_IP_CKSUM       	0x0002
#define RCB_FLAG_SPLIT_HDRS     	0x0004	/* unused */
#define RCB_FLAG_NO_PSEUDO_HDR_CKSUM	0x0008
#define RCB_FLAG_VLAN_ASSIST    	0x0010
#define RCB_FLAG_COAL_UPDATE_ONLY	0x0020
#define RCB_FLAG_HOST_RING 		0x0040
#define RCB_FLAG_IEEE_SNAP_CKSUM	0x0080
#define RCB_FLAG_USE_EXT_RECV_BD	0x0100
#define RCB_FLAG_RING_DISABLED		0x0200

/*
 * The buffer descriptor structure is defined in two pieces, bd1 and bd2.
 * The bd1 portion is required to be sent across the Host/NIC interface at 
 * all times.  The bd2 portion is only needed when checksum offload is being
 * used.  Note: Even when checksum offload is not being used, space must
 * be allocated for the entire buffer descriptor structure (bd).
 */

struct bd1 {
    tg_hostaddr_t host_addr;
    union {
	struct {
#if ALT_BIG_ENDIAN
	    U16 index;
	    U16 len;
#else /* ALT_BIG_ENDIAN */
	    U16 len;
	    U16 index;
#endif /* ALT_BIG_ENDIAN */
	} c_l;
	U32 w2;
    } w2;
    union {
	struct {
#if ALT_BIG_ENDIAN
	    U16 type;
	    U16 flags;
#else /* ALT_BIG_ENDIAN */
	    U16 flags;
	    U16 type;
#endif /* ALT_BIG_ENDIAN */
	} t_f;
	U32 w3;
    } w3;
};

struct bd2 {
    union {
	struct {
#if ALT_BIG_ENDIAN
	    U16 ip_cksum;
	    U16 tcp_udp_cksum;
#else /* ALT_BIG_ENDIAN */
	    U16 tcp_udp_cksum;
	    U16 ip_cksum;
#endif /* ALT_BIG_ENDIAN */
	} i_t;
	U32 w0;
    } w0;
#if ALT_BIG_ENDIAN
    U16 error_flags;
    U16 vlan_tag;
#else /* ALT_BIG_ENDIAN */
    U16 vlan_tag;
    U16 error_flags;
#endif /* ALT_BIG_ENDIAN */
};

typedef struct ext_bd {
    tg_hostaddr_t host_addr_1;
    tg_hostaddr_t host_addr_2;
    tg_hostaddr_t host_addr_3;
#if ALT_BIG_ENDIAN
    U16 len_1;
    U16 len_2;
    U16 len_3;
    U16 reserved;
#else /* ALT_BIG_ENDIAN */
    U16 len_2;
    U16 len_1;
    U16 reserved;
    U16 len_3;
#endif /* ALT_BIG_ENDIAN */
} ext_bd_t;

typedef struct recv_bd {
    struct bd1 bd1;
    struct bd2 bd2;
    U32 nic_addr;
    U32 opaque_data;
} recv_bd_t;

typedef struct ext_recv_bd {
    struct ext_bd ext_bd;
    struct bd1 bd1;
    struct bd2 bd2;
    U32 nic_addr;
    U32 opaque_data;
} ext_recv_bd_t;
   
#define BD_host_addr_1		ext_bd.host_addr_1
#define BD_host_addr_2		ext_bd.host_addr_2
#define BD_host_addr_3		ext_bd.host_addr_3
#define BD_len_1		ext_bd.len_1
#define BD_len_2		ext_bd.len_2
#define BD_len_3		ext_bd.len_3
#define BD_host_addr		bd1.host_addr
#define BD_len			bd1.w2.c_l.len
#define BD_index		bd1.w2.c_l.index
#define BD_w2			bd1.w2.w2
#define BD_flags		bd1.w3.t_f.flags
#define BD_type			bd1.w3.t_f.type
#define BD_w3			bd1.w3.w3
#define BD_ip_cksum 		bd2.w0.i_t.ip_cksum
#define BD_tcp_udp_cksum	bd2.w0.i_t.tcp_udp_cksum
#define BD_nic_addr		nic_addr
#define BD_error_flags  	bd2.error_flags
#define BD_vlan_tag     	bd2.vlan_tag
#define BD_w4			bd2.w0.w0

#define BD_TYPE_MASK		0xffff0000
#define BD_LEN_MASK		0x0000ffff
#define BD_IP_CKSUM_MASK	0xffff0000
#define BD_TCP_UDP_CKSUM_MASK	0x0000ffff
#define BD_TYPE_SHIFT		16
#define BD_IP_CKSUM_SHIFT	16

#if (TIGON_REV & TIGON_REV4)

#define TIGON_TYPE_NULL				0x0000
#define TIGON_TYPE_SEND_BD			0x0001
#define TIGON_TYPE_RECV_BD			0x0002
#define TIGON_TYPE_RECV_JUMBO_BD		0x0003
#define TIGON_TYPE_RECV_BD_LAST			0x0004
#define TIGON_TYPE_SEND_DATA			0x0005
#define TIGON_TYPE_SEND_DATA_LAST		0x0006
#define TIGON_TYPE_RECV_DATA			0x0007
#define TIGON_TYPE_RECV_DATA_LAST		0x000b
#define TIGON_TYPE_EVENT_RUPT			0x000c
#define TIGON_TYPE_EVENT_NO_RUPT		0x000d
#define TIGON_TYPE_ODD_START			0x000e
#define TIGON_TYPE_UPDATE_STATS			0x000f
#define TIGON_TYPE_SEND_DUMMY_DMA		0x0010
#define TIGON_TYPE_EVENT_PROD			0x0011
#define TIGON_TYPE_TX_CONS			0x0012
#define TIGON_TYPE_RX_PROD			0x0013
#define TIGON_TYPE_REFRESH_STATS		0x0014
#define TIGON_TYPE_SEND_DATA_LAST_VLAN		0x0015
#define TIGON_TYPE_SEND_DATA_COAL		0x0016
#define TIGON_TYPE_SEND_DATA_LAST_COAL		0x0017
#define TIGON_TYPE_SEND_DATA_LAST_VLAN_COAL	0x0018
#define TIGON_TYPE_TX_CONS_NO_INTR		0x0019

#else /* (TIGON_REV & TIGON_REV4) */

/* dma types - to nic */
#define TIGON_TYPE_SEND_BD			(1 << 0)
#define TIGON_TYPE_SEND_DATA			(1 << 1)
#define TIGON_TYPE_SEND_DATA_LAST		(1 << 2)
#define TIGON_TYPE_SEND_DATA_LAST_VLAN		(1 << 3)
#define TIGON_TYPE_SEND_DATA_COAL		(1 << 4)
#define TIGON_TYPE_SEND_DATA_LAST_COAL		(1 << 5)
#define TIGON_TYPE_SEND_DATA_LAST_VLAN_COAL	(1 << 6)
#define TIGON_TYPE_SEND_DUMMY_DMA		(1 << 7)
#define TIGON_TYPE_REFRESH_STATS		(1 << 8)

/* dma types - to host */
#define TIGON_TYPE_RECV_DATA			(1 << 0)
#define TIGON_TYPE_RECV_DATA_LAST		(1 << 1)
#define TIGON_TYPE_EVENT			(1 << 2)
#define TIGON_TYPE_UPDATE_STATS			(1 << 3)
#define TIGON_TYPE_EVENT_PROD			(1 << 4)
#define TIGON_TYPE_TX_CONS			(1 << 5)
#define TIGON_TYPE_RX_PROD			(1 << 6)
#define TIGON_TYPE_TX_CONS_NO_INTR		(1 << 7)

/* dma types - both */
#define TIGON_TYPE_NULL				(0)
#define TIGON_TYPE_RECV_BD			(1 << 12)
#define TIGON_TYPE_RECV_JUMBO_BD		(1 << 13)
#define TIGON_TYPE_RECV_MINI_BD			(1 << 14)
#define TIGON_TYPE_RECV_BD_LAST			(1 << 15)

#endif /* (TIGON_REV & TIGON_REV4) */

/* BD error flags */
#define BD_ERR_BAD_CRC			0x0001
#define BD_ERR_COLL_DETECT		0x0002
#define BD_ERR_LINK_LOST_DURING_PKT	0x0004
#define BD_ERR_PHY_DECODE_ERR		0x0008
#define BD_ERR_ODD_NIBBLES_RCVD_MII	0x0010
#define BD_ERR_MAC_ABORT		0x0020
#define BD_ERR_LEN_LT_64		0x0040
#define BD_ERR_TRUNC_NO_RESOURCES	0x0080
#define BD_ERR_GIANT_FRAME_RCVD  	0x0100
#define	BD_ERR_SHIFT			16

struct send_bd_lf {
#if ALT_BIG_ENDIAN
    U16 length;
    U16 flags;			/* see below */
#else /* ALT_BIG_ENDIAN */
    U16 flags;			/* see below */
    U16 length;
#endif /* ALT_BIG_ENDIAN */
};

/* structure for send BDs, can be and must simpler than receive bds
 * so it fits more into less space since this is accessed through 
 * the memory window */
typedef struct send_bd {
    tg_hostaddr_t host_addr;
    union {
	struct send_bd_lf lf;
	U32 w0;
    } w0;
    union {
	U32 nic_addr;
	struct {
#if ALT_BIG_ENDIAN
	    U16 reserved;
	    U16 vlan_tag;
#else /* ALT_BIG_ENDIAN */
	    U16 vlan_tag;
	    U16 reserved;
#endif /* ALT_BIG_ENDIAN */
	} vlan_struct;
    } w1;
} send_bd_t;

#define sendbd_flags    w0.lf.flags
#define sendbd_length   w0.lf.length
#define sendbd_vlan_tag w1.vlan_struct.vlan_tag
#define sendbd_nic_addr w1.nic_addr

#define BD_FLAG_TCP_UDP_CKSUM	0x0001 /* do TCP/UDP cksum */
#define BD_FLAG_IP_CKSUM	0x0002 /* do IP cksum */
#define BD_FLAG_END		0x0004 /* indicates end of packet */
#define BD_FLAG_MORE            0x0008 /* host indication for split virt pkt */
#define BD_FLAG_JUMBO_RING      0x0010 /* indicates bd is from jumbo ring */
#define BD_FLAG_UCAST_PKT       0x0020 /* unicast packet */
#define BD_FLAG_MCAST_PKT       0x0040 /* multicast packet */
#define BD_FLAG_BCAST_PKT       0x0060 /* broadcast packet */
#define BD_FLAG_PKT_SHIFT       5      /* Shift for PKT type field */
#define BD_FLAG_IP_FRAG		0x0080 /* Part of a fragmented frame */
				       /* (i.e. any (but NOT LAST) fragment) */
#define BD_FLAG_IP_FRAG_END	0x0100 /* END of a fragmented frame  */
				       /* (i.e. last fragment) */
#define BD_FLAG_VLAN_TAG	0x0200 /* Vlan tag in buffer descriptor */
#define BD_FLAG_FRAME_HAS_ERROR 0x0400 /* indicated in error_flags field */
#define BD_FLAG_COAL_NOW	0x0800 /* do send coal when done data DMA */
#define BD_FLAG_MINI_RING	0x1000 /* indicates bd is from mini ring */

/* 
 * Events.
 */

typedef struct tg_event {
    union {
	struct tg_eci {
#if ALT_BIG_ENDIAN
	    U32 event:8;
	    U32 code:12;
	    U32 index:12;
#else /* ALT_BIG_ENDIAN */
	    U32 index:12;
	    U32 code:12;
	    U32 event:8;
#endif /* ALT_BIG_ENDIAN */
	} eci;
	U32 w0;
    } w0;
    U32 w1;			/* reserved, to pad to doubleword */
#if defined(ALLOW_MEM_WRITE_INVALIDATE)
    U32 cache_pad[6];		/* pad to cache size for 32 byte caches */    
#endif /* defined(ALLOW_MEM_WRITE_INVALIDATE) */
} tg_event_t;

#define TG_EVENT_event	w0.eci.event
#define TG_EVENT_code	w0.eci.code
#define TG_EVENT_index	w0.eci.index
#define TG_EVENT_w0	w0.w0	

#define TG_EVENT_EVENT_MASK	0xff000000
#define TG_EVENT_CODE_MASK	0x00fff000
#define TG_EVENT_INDEX_MASK	0x00000fff
#define TG_EVENT_EVENT_SHIFT	24
#define TG_EVENT_CODE_SHIFT	12

/* the events... */

#define TG_EVENT_FIRMWARE_OPERATIONAL	   0x01
#define TG_EVENT_STATS_UPDATED		   0x04

#define TG_EVENT_LINK_STATE_CHANGED	   0x06
#define TG_EVENT_CODE_LINK_UP		   0x01
#define TG_EVENT_CODE_LINK_DOWN		   0x02
#define TG_EVENT_CODE_FAST_LINK_UP         0x03

#define TG_EVENT_ERROR			   0x07
#define TG_EVENT_CODE_ERR_INVALID_CMD      0x01
#define TG_EVENT_CODE_ERR_UNIMP_CMD        0x02
#define TG_EVENT_CODE_ERR_BAD_CONFIG       0x03

#define TG_EVENT_MULTICAST_LIST_UPDATED	   0x08
#define TG_EVENT_CODE_MCAST_ADDR_ADDED     0x01
#define TG_EVENT_CODE_MCAST_ADDR_DELETED   0x02

#define TG_EVENT_RESET_JUMBO_RING          0x09

/*
 * Commands
 */

typedef struct tg_command {
    union {
	struct {
#if ALT_BIG_ENDIAN
	    U32 cmd:8;
	    U32 code:12;
	    U32 index:12;
#else /* ALT_BIG_ENDIAN */
	    U32 index:12;
	    U32 code:12;
	    U32 cmd:8;
#endif /* ALT_BIG_ENDIAN */
	} cif;
	U32 w0;
    } w0;
} tg_command_t;

#define TG_COMMAND_cmd		w0.cif.cmd
#define TG_COMMAND_code	        w0.cif.code
#define TG_COMMAND_index	w0.cif.index
#define TG_COMMAND_w0		w0.w0	

#define TG_COMMAND_CMD_MASK	0xff000000
#define TG_COMMAND_CODE_MASK	0x00fff000
#define TG_COMMAND_INDEX_MASK	0x00000fff
#define TG_COMMAND_CMD_SHIFT	24
#define TG_COMMAND_CODE_SHIFT	12

#define TG_CMD_HOST_STATE               0x01
#define TG_CMD_CODE_STACK_UP            0x01 /* code */
#define TG_CMD_CODE_STACK_DOWN          0x02 /* code */

#define TG_CMD_FDR_FILTERING            0x02
#define TG_CMD_CODE_FDR_FILT_ENABLE     0x01 /* code */
#define TG_CMD_CODE_FDR_FILT_DISABLE    0x02 /* code */

#define TG_CMD_SET_RECV_PRODUCER_INDEX	0x03
#define TG_CMD_UPDATE_GENCOM_STATS      0x04
#define TG_CMD_RESET_JUMBO_RING         0x05
#define TG_CMD_SET_PARTIAL_RECV_COUNT   0x06
#define TG_CMD_ADD_MULTICAST_ADDR	0x08
#define TG_CMD_DEL_MULTICAST_ADDR	0x09

#define TG_CMD_SET_PROMISC_MODE		0x0a
#define TG_CMD_CODE_PROMISC_ENABLE	0x01 /* code */
#define TG_CMD_CODE_PROMISC_DISABLE	0x02 /* code */

#define TG_CMD_LINK_NEGOTIATION		0x0b
#define TG_CMD_CODE_NEGOTIATE_BOTH      0x00 /* code */
#define TG_CMD_CODE_NEGOTIATE_GIGABIT   0x01 /* code */
#define TG_CMD_CODE_NEGOTIATE_10_100    0x02 /* code */

#define TG_CMD_SET_MAC_ADDR		0x0c
#define TG_CMD_CLEAR_PROFILE		0x0d

#define TG_CMD_SET_MULTICAST_MODE	0x0e
#define TG_CMD_CODE_MULTICAST_ENABLE	0x01 /* code */
#define TG_CMD_CODE_MULTICAST_DISABLE	0x02 /* code */

#define TG_CMD_CLEAR_STATS		0x0f
#define TG_CMD_SET_RECV_JUMBO_PRODUCER_INDEX	0x10
#define TG_CMD_REFRESH_STATS            0x11

#define TG_CMD_EXT_ADD_MULTICAST_ADDR   0x12
#define TG_CMD_EXT_DEL_MULTICAST_ADDR   0x13

/*
 * statisitcs maintained by the NIC
 */

typedef struct tg_stats {
    /*
     * MAC stats, taken from RFC 1643, ethernet-like MIB
     */
    VOLATILE U32 dot3StatsAlignmentErrors;
    VOLATILE U32 dot3StatsFCSErrors;
    VOLATILE U32 dot3StatsSingleCollisionFrames;
    VOLATILE U32 dot3StatsMultipleCollisionFrames;
    VOLATILE U32 dot3StatsSQETestErrors;
    VOLATILE U32 dot3StatsDeferredTransmissions;
    VOLATILE U32 dot3StatsLateCollisions;
    VOLATILE U32 dot3StatsExcessiveCollisions;
    VOLATILE U32 dot3StatsInternalMacTransmitErrors;
    VOLATILE U32 dot3StatsCarrierSenseErrors;
    VOLATILE U32 dot3StatsFrameTooLongs;
    VOLATILE U32 dot3StatsInternalMacReceiveErrors;
    /*
     * interface stats, taken from RFC 1213, MIB-II, interfaces group
     */
    VOLATILE U32 ifIndex;
    VOLATILE U32 ifType;
    VOLATILE U32 ifMtu;
    VOLATILE U32 ifSpeed;
    VOLATILE U32 ifAdminStatus;
#define IF_ADMIN_STATUS_UP	1
#define IF_ADMIN_STATUS_DOWN	2
#define IF_ADMIN_STATUS_TESTING	3
    VOLATILE U32 ifOperStatus;
#define IF_OPER_STATUS_UP	1
#define IF_OPER_STATUS_DOWN	2
#define IF_OPER_STATUS_TESTING	3
#define IF_OPER_STATUS_UNKNOWN	4
#define IF_OPER_STATUS_DORMANT	5
    VOLATILE U32 ifLastChange;
    VOLATILE U32 ifInDiscards;
    VOLATILE U32 ifInErrors;
    VOLATILE U32 ifInUnknownProtos;
    VOLATILE U32 ifOutDiscards;
    VOLATILE U32 ifOutErrors;
    VOLATILE U32 ifOutQLen;	/* deprecated */
    VOLATILE tg_macaddr_t  ifPhysAddress; /* 8 bytes */
    VOLATILE U8  ifDescr[32];
    U32 alignIt;		/* align to 64 bit for U64s following */
    /*
     * more interface stats, taken from RFC 1573, MIB-IIupdate, 
     * interfaces group
     */
    VOLATILE U64 ifHCInOctets;
    VOLATILE U64 ifHCInUcastPkts;
    VOLATILE U64 ifHCInMulticastPkts;
    VOLATILE U64 ifHCInBroadcastPkts;
    VOLATILE U64 ifHCOutOctets;
    VOLATILE U64 ifHCOutUcastPkts;
    VOLATILE U64 ifHCOutMulticastPkts;
    VOLATILE U64 ifHCOutBroadcastPkts;
    VOLATILE U32 ifLinkUpDownTrapEnable;
    VOLATILE U32 ifHighSpeed;
    VOLATILE U32 ifPromiscuousMode; 
    VOLATILE U32 ifConnectorPresent; /* follow link state */
    /* 
     * Host Commands
     */
    VOLATILE U32 nicCmdsHostState;
    VOLATILE U32 nicCmdsFDRFiltering;
    VOLATILE U32 nicCmdsSetRecvProdIndex;
    VOLATILE U32 nicCmdsUpdateGencommStats;
    VOLATILE U32 nicCmdsResetJumboRing;
    VOLATILE U32 nicCmdsAddMCastAddr;
    VOLATILE U32 nicCmdsDelMCastAddr;
    VOLATILE U32 nicCmdsSetPromiscMode;
    VOLATILE U32 nicCmdsLinkNegotiate;
    VOLATILE U32 nicCmdsSetMACAddr;
    VOLATILE U32 nicCmdsClearProfile;
    VOLATILE U32 nicCmdsSetMulticastMode;
    VOLATILE U32 nicCmdsClearStats;
    VOLATILE U32 nicCmdsSetRecvJumboProdIndex;
    VOLATILE U32 nicCmdsSetRecvMiniProdIndex;
    VOLATILE U32 nicCmdsRefreshStats;
    VOLATILE U32 nicCmdsUnknown;

    /* 
     * NIC Events
     */
    VOLATILE U32 nicEventsNICFirmwareOperational;
    VOLATILE U32 nicEventsStatsUpdated;
    VOLATILE U32 nicEventsLinkStateChanged;
    VOLATILE U32 nicEventsError;
    VOLATILE U32 nicEventsMCastListUpdated;
    VOLATILE U32 nicEventsResetJumboRing;

    /*
     * Ring manipulation
     */
    VOLATILE U32 nicRingSetSendProdIndex;
    VOLATILE U32 nicRingSetSendConsIndex;
    VOLATILE U32 nicRingSetRecvReturnProdIndex;

    /* 
     * Interrupts
     */
    VOLATILE U32 nicInterrupts;
    VOLATILE U32 nicAvoidedInterrupts;

    /* 
     * BD Coalessing Thresholds
     */
    VOLATILE U32 nicEventThresholdHit;
    VOLATILE U32 nicSendThresholdHit;
    VOLATILE U32 nicRecvThresholdHit;

    /* 
     * DMA Attentions
     */
    VOLATILE U32 nicDmaRdOverrun;
    VOLATILE U32 nicDmaRdUnderrun;
    VOLATILE U32 nicDmaWrOverrun;
    VOLATILE U32 nicDmaWrUnderrun;
    VOLATILE U32 nicDmaWrMasterAborts;
    VOLATILE U32 nicDmaRdMasterAborts;

    /* 
     * NIC Resources
     */
    VOLATILE U32 nicDmaWriteRingFull;
    VOLATILE U32 nicDmaReadRingFull;
    VOLATILE U32 nicEventRingFull;
    VOLATILE U32 nicEventProducerRingFull;
    VOLATILE U32 nicTxMacDescrRingFull;
    VOLATILE U32 nicOutOfTxBufSpaceFrameRetry;
    VOLATILE U32 nicNoMoreWrDMADescriptors;
    VOLATILE U32 nicNoMoreRxBDs;
    VOLATILE U32 nicNoSpaceInReturnRing;
    VOLATILE U32 nicSendBDs;		/* current count */
    VOLATILE U32 nicRecvBDs;		/* current count */
    VOLATILE U32 nicJumboRecvBDs;	/* current count */
    VOLATILE U32 nicMiniRecvBDs;	/* current count */
    VOLATILE U32 nicTotalRecvBDs;	/* current count */
    VOLATILE U32 nicTotalSendBDs;	/* current count */
    VOLATILE U32 nicJumboSpillOver;
    VOLATILE U32 nicSbusHangCleared;
    VOLATILE U32 nicEnqEventDelayed;

    /*
     * Stats from MAC rx completion
     */
    VOLATILE U32 nicMacRxLateColls;
    VOLATILE U32 nicMacRxLinkLostDuringPkt;
    VOLATILE U32 nicMacRxPhyDecodeErr;
    VOLATILE U32 nicMacRxMacAbort;
    VOLATILE U32 nicMacRxTruncNoResources;
    /*
     * Stats from the mac_stats area
     */
    VOLATILE U32 nicMacRxDropUla;
    VOLATILE U32 nicMacRxDropMcast;
    VOLATILE U32 nicMacRxFlowControl;
    VOLATILE U32 nicMacRxDropSpace;
    VOLATILE U32 nicMacRxColls;
    
    /*
     * MAC RX Attentions
     */
    VOLATILE U32 nicMacRxTotalAttns;
    VOLATILE U32 nicMacRxLinkAttns;
    VOLATILE U32 nicMacRxSyncAttns;
    VOLATILE U32 nicMacRxConfigAttns;
    VOLATILE U32 nicMacReset;
    VOLATILE U32 nicMacRxBufDescrAttns;
    VOLATILE U32 nicMacRxBufAttns;
    VOLATILE U32 nicMacRxZeroFrameCleanup;
    VOLATILE U32 nicMacRxOneFrameCleanup;
    VOLATILE U32 nicMacRxMultipleFrameCleanup;
    VOLATILE U32 nicMacRxTimerCleanup;
    VOLATILE U32 nicMacRxDmaCleanup;

    /*
     * Stats from the mac_stats area
     */
    VOLATILE U32 nicMacTxCollisionHistogram[15];
    /*
     * MAC TX Attentions
     */
    VOLATILE U32 nicMacTxTotalAttns;
	     
    /*
     * NIC Profile
     */
    VOLATILE U32 nicProfile[32];

    /*
     * pad out to 1024 bytes...
     */
    U32 res[75];
} tg_stats_t;


/*
 * tuning paramters, change the acttune.c app if this structure changes
 */
typedef struct tg_tune {
    VOLATILE U32 recv_coal_ticks; /* clock ticks between interrupts */
    VOLATILE U32 send_coal_ticks; /* clock ticks between send coal updates */
    VOLATILE U32 stat_ticks;	  /* clock ticks between stat updates */
    VOLATILE U32 send_max_coalesced_bds; /* threshold in bds */
    VOLATILE U32 recv_max_coalesced_bds; /* threshold in bds */
    VOLATILE U32 tracing;	  /* see following */
#define TRACE_TYPE_SEND		0x00000001
#define TRACE_TYPE_RECV		0x00000002
#define TRACE_TYPE_DMA		0x00000004
#define TRACE_TYPE_EVENT	0x00000008
#define TRACE_TYPE_COMMAND	0x00000010
#define TRACE_TYPE_MAC		0x00000020
#define TRACE_TYPE_STATS	0x00000040
#define TRACE_TYPE_TIMER	0x00000080
#define TRACE_TYPE_DISP		0x00000100
#define TRACE_TYPE_MAILBOX	0x00000200
#define TRACE_TYPE_RECV_BD	0x00000400
#define TRACE_TYPE_LNK_PHY	0x00000800
#define TRACE_TYPE_LNK_NEG	0x00001000
#define TRACE_LEVEL_1		0x10000000
#define TRACE_LEVEL_2		0x20000000
    VOLATILE U32 link_negotiation; /* see src/common/link.h */
    VOLATILE U32 fast_link_negotiation; /* see src/common/link.h */
} tg_tune_t;

/*
 * configuration definition file
 */

typedef struct tg_gencom {
    VOLATILE tg_macaddr_t mac_addr;
    VOLATILE tg_hostaddr_t gen_info_ptr;
    VOLATILE tg_macaddr_t mcast_xfer_buf;
    VOLATILE U32 mode_status;
#define TG_CFG_MODE_BYTE_SWAP_BD     0x00000002
#define TG_CFG_MODE_WORD_SWAP_BD     0x00000004
#define TG_CFG_MODE_WARN             0x00000008
#define TG_CFG_MODE_BYTE_SWAP_DATA   0x00000010
#define TG_CFG_MODE_1_DMA_ACTIVE     0x00000040
#define TG_CFG_MODE_USE_OP1_LEDS     0x00000080
#define TG_CFG_MODE_SBUS             0x00000100
#define TG_CFG_MODE_DONT_FRAG_JUMBOS 0x00000200
#define TG_CFG_MODE_INCLUDE_CRC      0x00000400
#define TG_CFG_MODE_ALLOW_BAD_FRAMES 0x00000800
#define TG_CFG_MODE_DONT_RUPT_EVENTS 0x00001000
#define TG_CFG_MODE_DONT_RUPT_SENDS  0x00002000
#define TG_CFG_MODE_DONT_RUPT_RECVS  0x00004000
#define TG_CFG_MODE_FATAL            0x40000000
    VOLATILE U32 dma_read_config;  /* see DMA state bits in tg.h for */
    VOLATILE U32 dma_write_config; /* legal values for these two registers */
    VOLATILE U32 tx_buf_ratio;     /* 6 bits of numerator: range 1/64 ~ 63/64 */
#define TX_BUF_RATIO_MASK    (0x3f)/* 6 bits only */
    VOLATILE U32 event_cons_index;
    VOLATILE U32 command_cons_index;
    VOLATILE struct tg_tune tuneParms;
    VOLATILE U32 nic_trace_ptr;
    VOLATILE U32 nic_trace_start;
    VOLATILE U32 nic_trace_len;
    VOLATILE U32 ifIndex;
    VOLATILE U32 ifMTU;
    VOLATILE U32 maskRupts;
    VOLATILE U32 linkState;	   /* see src/common/link.h */
    VOLATILE U32 fastLinkState;	   /* see src/common/link.h */
    U32 reserved_2[4];
    VOLATILE U32 recv_return_cons_index; /* Host Owns cons of return ring */
    U32 reserved_3[31];
    VOLATILE struct tg_command command_ring[COMMAND_RING_ENTRIES];
} tg_gencom_t;

#define SSP ((volatile struct tg_host_config_shared_mem *)0)

/* offset in SRAM of the command ring from firmware perspective */
#define TG_COMMAND_RING		0x100
#define TG_HOST_COMMAND_RING	0x300 /* from driver perspective */

/*
 * The general information area.  This area is allocated by the host and
 * pointed to by the general information pointer in the config shared memory
 * area.
 */

typedef struct tg_gen_info {
    struct tg_stats stats;
    struct ring_control_block event_rcb;
    struct ring_control_block command_rcb;
    struct ring_control_block send_rcb;
    struct ring_control_block recv_standard_rcb; /* to NIC standard */
    struct ring_control_block recv_jumbo_rcb;	 /* to NIC jumbo */
    struct ring_control_block recv_mini_rcb;	 /* to NIC mini */
    struct ring_control_block recv_return_rcb;   /* to HOST, bufs used */
    tg_hostaddr_t event_producer_ptr;
    tg_hostaddr_t recv_return_producer_ptr;
    tg_hostaddr_t send_consumer_ptr;
    tg_hostaddr_t stats2_ptr;   /* for replacing them */
} tg_gen_info_t;

#endif /* _NIC_API_H_ */
