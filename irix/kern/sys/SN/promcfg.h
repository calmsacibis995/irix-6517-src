/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	__SYS_SN_PCFG_H__
#define	__SYS_SN_PCFG_H__


#include <sys/types.h>
#include <sys/SN/agent.h>
#include <sys/SN/router.h>
#ifndef _KLCONFIG_H_
#include <sys/SN/klconfig.h>
#endif

/*
 *    Hardware topology graph structure for the IP27prom and IO6prom
 *
 * The CPU PROM SN0 System Configuration information structure resides at
 * a fixed location in memory (IP27PROM_PCFG) and contains all information
 * discovered by the CPU PROM at system boot.  All information passed from
 * the CPU PROM to the I/O PROM and diagnostics is stored here.
 *
 * Pcfg_t consists of header entries followed by an array of pcfg_ent_t.
 * Together these array entries describe all hubs and routers in the system,
 * their connectivity, NICs, and additional miscellaneous information.
 *
 * There may be a maximum of IP27PROM_PCFG_SIZE / sizeof (pcfg_ent_t) - 1
 * entries in the pcfg structure (currently 1663).
 */

#define PCFG_MAGIC		0x50434647

#define PCFG_TYPE_NONE		0
#define PCFG_TYPE_HUB		1
#define PCFG_TYPE_ROUTER	2
#define PCFG_TYPE_MAX		3

/* pcfg_hub_t flags
 */
#define PCFG_HUB_CPU(n)		(1 << (n))	/* CPU n present/enabled     */
#define PCFG_HUB_HEADLESS	(1 << 16)	/* No CPUs present/enabled   */
#define PCFG_HUB_I2C		(1 << 17)	/* I2C system ctrlr avail.   */
#define PCFG_HUB_XBOW		(1 << 18)	/* Crossbow found	     */
#define PCFG_HUB_BRIDGE		(1 << 19)	/* Bridge found		     */
#define PCFG_HUB_BASEIO		(1 << 20)	/* BaseIO found		     */
#define PCFG_HUB_GMASTER	(1 << 21)	/* Want to be global master  */
#define PCFG_HUB_POWERON	(1 << 22)	/* Reset due to power-on     */
#define PCFG_HUB_LOCALPOD	(1 << 23)	/* Want local POD mode       */
#define PCFG_HUB_GLOBALPOD	(1 << 24)	/* Want global POD mode      */
#define PCFG_HUB_PORTSTAT1	(1 << 25)	/* Basic diag status of port */
#define PCFG_HUB_PORTSTAT2	(1 << 26)	/* Heavy diag status of port */
#define PCFG_HUB_PREMDIR	(1 << 27)	/* Node has premium dirs     */
#define PCFG_HUB_REMOTE_SPACE	(1 << 28)	/* Not in reset partition    */
#define PCFG_HUB_PARTPROM	(1 << 29)	/* pttnid needs to be voted  */
#define	PCFG_HUB_EXCP		(1 << 30)	/* Did it take a exception ? */
#define PCFG_HUB_MEMLESS 	(1 << 31)	/* No memory enabled         */

/* pcfg_router_t flags
 */
#define PCFG_ROUTER_META         (1 <<  0)	/* Is router a meta-router?  */
#define PCFG_ROUTER_SKIPPED      (1 <<  1)	/* Is router a meta-router?  */
#define PCFG_ROUTER_REMOTE_SPACE (1 <<  2)	/* Not in reset partition    */

#define PCFG_ROUTER_STATE_SHFT 	4
#define PCFG_ROUTER_STATE_MASK 	0x00f0U
#define PCFG_ROUTER_DEPTH_SHFT	8
#define PCFG_ROUTER_DEPTH_MASK	0x0f00U		/* max depth 15 */

#define PCFG_ROUTER_STATE_NEW  	0
#define PCFG_ROUTER_STATE_SEEN 	1
#define PCFG_ROUTER_STATE_DONE 	2

/* pcfg_port_t flags
 */
#define	PCFG_PORT_UP_SHFT	0
#define	PCFG_PORT_UP_MASK	(1 << PCFG_PORT_UP_SHFT)
#define	PCFG_PORT_FENCE_SHFT	1
#define	PCFG_PORT_FENCE_MASK	(1 << PCFG_PORT_FENCE_SHFT)
#define PCFG_PORT_XPRESS_SHFT	4
#define PCFG_PORT_XPRESS_MASK	(1 << PCFG_PORT_XPRESS_SHFT)
#define PCFG_PORT_FAKE_SHFT	5
#define PCFG_PORT_FAKE_MASK	(1 << PCFG_PORT_FAKE_SHFT)
#define PCFG_PORT_ESCP_SHFT	6
#define PCFG_PORT_ESCP_MASK	(1 << PCFG_PORT_ESCP_SHFT)

#define PCFG_INDEX_INVALID	((short) -1)
#define PCFG_PORT_INVALID	((char ) -1)
#define PCFG_METAID_INVALID	((short) -1)
#define PCFG_DIMN_INVALID	((char ) -1)

#define IS_HUB_HEADLESS(h)      (h->flags & PCFG_HUB_HEADLESS)
#define IS_HUB_MEMLESS(h)       (h->flags & PCFG_HUB_MEMLESS)

/* RESET partition is defined as the one that's coming up */

#define IS_RESET_SPACE_HUB(ph)		\
				(!((ph)->flags & PCFG_HUB_REMOTE_SPACE))
#define IS_RESET_SPACE_RTR(pr)		\
				(!((pr)->flags & PCFG_ROUTER_REMOTE_SPACE))

typedef struct pcfg_port_s {
	short		index;		/* Index of attached component 	     */
	char		port;		/* Absolute port (1 to 6) if router  */
	char		flags;		/* general purpose flag bits         */
	__uint64_t	error;		/* Status_error per port for routers */
} pcfg_port_t;

typedef struct pcfg_any_s {		/* Common entries in pcfg_t union    */
	char		type;		/* PCFG_TYPE_*	 		     */
} pcfg_any_t;

typedef struct pcfg_hub_s {
	char		type;		/* PCFG_TYPE_HUB 		     */
	char		version;	/* Hub hardware version		     */
	nasid_t		nasid;		/* Arbitrated NASID (node number)    */
	uint		flags;		/* PCFG_HUB_xxx flags 		     */
	char		partition;	/* Partition ID determined by IO6    */
	char		promvers;	/* PROM version num, if not headless */
	char		promrev;	/* PROM Revision num if not headless */
	char		slot;		/* IP27 slot number [1-4]	     */
	uchar_t		module;		/* Module ID			     */
	char		unused1[3];	/* Space wasted due to alignment     */
	pcfg_port_t	port;		/* Device attached via SN0net        */
	nic_t		nic;		/* Universally unique ID no.         */
	uint		htlocal[NI_LOCAL_ENTRIES];
	uint		htmeta[NI_META_ENTRIES];
} pcfg_hub_t;

typedef struct pcfg_router_s {
	char		type;		/* PCFG_TYPE_ROUTER                  */
	char		version;	/* Router hardware version	     */
	short		metaid;		/* Arbitrated meta-ID bits	     */
	char		force_local;	/* RR_FORCE_LOCAL bit		     */
	uchar_t		module;		/* Module ID of containing module    */
	ushort		flags;		/* PCFG_ROUTER_xxx flags	     */
	nic_t		nic;		/* Universally unique ID no.         */
	pcfg_port_t	port[7];	/* Port devices 1 to 6 (zero unused) */
	char		portstat1;	/* Basic diag status of ports 1 to 6 */
	char		partition;	/* partition id of router.           */
	char		slot;		/* Router slot number [1-2]	     */
	char		unused;		/* Unused                            */
	uint		rtlocal[RR_LOCAL_ENTRIES];
	uint		rtmeta[RR_META_ENTRIES];
} pcfg_router_t;

typedef union pcfg_ent_u {
	pcfg_any_t	any;
	pcfg_hub_t	hub;
	pcfg_router_t	router;
	char		padsize[512];	/* Pad entry size to power of 2      */
} pcfg_ent_t;

typedef struct pcfg_s {
	uint		magic;		/* PCFG_MAGIC identifies valid data  */
	int		alloc;		/* Number of entries in array (room) */
	int		count;		/* Number of entries in array (used) */
	int		count_type[PCFG_TYPE_MAX];
	int		gmaster;	/* Index of arbitrated global master */
	int		pmaster;	/* Index of arbitrated pttn master   */
	pcfg_ent_t	array[1];	/* Placeholder (must be last member) */
} pcfg_t;

/*
 * This structure is needed by the IO6PROM during graph build.
 */

typedef struct pcfg_lboard_s {
	lboard_t       *pcfg_rlbptr;
	char		pcfg_rflags;
} pcfg_lboard_t;

#define PCFG(nasid)		((pcfg_t *) TO_NODE(nasid, IP27PROM_PCFG))
#define PCFG_PTR(nasid, i)	(&PCFG(nasid)->array[i])

#endif /*__SYS_SN_PCFG_H__ */
