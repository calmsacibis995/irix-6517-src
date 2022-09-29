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

#ifndef	__SYS_SN_ROUTER_H__
#define	__SYS_SN_ROUTER_H__

/*
 * Router Register definitions
 *
 * Macro argument _L always stands for a link number (1 to 6, inclusive).
 */

#ident "$Revision: 1.53 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#include <sys/SN/vector.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/arch.h>
#include <sys/time.h>
#include <sys/sema.h>

typedef __uint64_t	router_reg_t;

#define MAX_ROUTERS	256

#define MAX_ROUTER_PATH	80

#define ROUTER_REG_CAST		(volatile router_reg_t *)
#define PS_UINT_CAST		(__psunsigned_t)
#define UINT64_CAST		(__uint64_t)
typedef signed char port_no_t;	 /* Type for router port number      */

#elif _LANGUAGE_ASSEMBLY

#define ROUTERREG_CAST
#define PS_UINT_CAST
#define UINT64_CAST

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#define MAX_ROUTER_PORTS (6)	 /* Max. number of ports on a router */
#define PORT_INVALID (-1)	 /* Invalid port number              */

#define	IS_META(_rp)	((_rp)->flags & PCFG_ROUTER_META)

/*
 * RR_TURN makes a given number of clockwise turns (0 to 5) from an inport
 * port to generate an output port.
 *
 * RR_DISTANCE returns the number of turns necessary (0 to 5) to go from
 * an input port (_L1 = 1 to 6) to an output port ( _L2 = 1 to 6).
 *
 * These are written to work on unsigned data.
 */

#define RR_TURN(_L, count)	((_L) + (count) > 6 ?			\
				 (_L) + (count) - 6 :			\
				 (_L) + (count))

#define RR_DISTANCE(_LS, _LD)	((_LD) >= (_LS) ?			\
				 (_LD) - (_LS) :			\
				 (_LD) + 6 - (_LS))

/* Router register addresses */

#define RR_STATUS_REV_ID	0x00000	/* Status register and Revision ID  */
#define RR_PORT_RESET		0x00008	/* Multiple port reset              */
#define RR_PROT_CONF		0x00010	/* Inter-partition protection conf. */
#define RR_GLOBAL_PARMS		0x00018	/* Parameters shared by all 6 ports */
#define RR_SCRATCH_REG0		0x00020	/* Scratch 0 is 64 bits; scratch 1  */
#define RR_SCRATCH_REG1		0x00028	/*   is 16 bits (garbage upper 48)  */
#define RR_DIAG_PARMS		0x00030	/* Parameters for diag. testing     */
#define RR_NIC_ULAN		0x00038	/* Number-in-a-can microlan I/F     */

/* Router built in self test registers */

#define RR_BIST_DATA		0x00050	/* BIST read and write data         */
#define RR_BIST_READY		0x00058	/* BIST Ready indicator             */
#define RR_BIST_COUNT_TARG	0x00060	/* BIST Count and Target            */
#define RR_BIST_SHIFT_LOAD	0x00068	/* BIST control                     */
#define RR_BIST_SHIFT_UNLOAD	0x00070	/* BIST control                     */
#define RR_BIST_ENTER_RUN	0x00078	/* BIST control                     */

/* Port-specific registers (_L is the link number from 1 to 6) */

#define RR_PORT_PARMS(_L)	((_L) << 16 | 0x0000) /* LLP parameters     */
#define RR_STATUS_ERROR(_L)	((_L) << 16 | 0x0008) /* Port-related errs  */
#define RR_HISTOGRAM(_L)	((_L) << 16 | 0x0010) /* Port usage histgrm */
#define RR_RESET_MASK(_L)	((_L) << 16 | 0x0018) /* Remote reset mask  */
#define RR_ERROR_CLEAR(_L)	((_L) << 16 | 0x0088) /* Read/clear errors  */

/*
 * The meta-level routing table contains 32 entries and is indexed
 * by the 5-bit meta-cube destination.
 */

#define RR_META_TABLE0		0x70000 /* First meta routing table entry   */
#define RR_META_TABLE(_x)	(RR_META_TABLE0 + 8 * (_x))
#define RR_META_ENTRIES		32

/*
 * The local routing table contains 16 entries and is indexed by the
 * 4-bit local cube destination.
 */

#define RR_LOCAL_TABLE0		0x70100 /* First local routing table entry  */
#define RR_LOCAL_TABLE(_x)	(RR_LOCAL_TABLE0 + 8 * (_x))
#define RR_LOCAL_ENTRIES	16

/*
 * RR_STATUS_REV_ID mask and shift definitions
 */

#define RSRI_INPORT_SHFT	46
#define RSRI_INPORT_MASK	(UINT64_CAST 0x7 << 46)
#define RSRI_LSTAT_SHFT(_L)	(25 + 3 * (_L))
#define RSRI_LSTAT_MASK(_L)	(UINT64_CAST 0x7 << 25 + 3 * (_L))
#define RSRI_LINK8BIT_BIT(_L)	(27 + 3 * (_L))
#define RSRI_LINK8BIT(_L)	(UINT64_CAST 1 << (27 + 3 * (_L)))
#define RSRI_LINKWORKING_BIT(_L)	(26 + 3 * (_L))
#define RSRI_LINKWORKING(_L)	(UINT64_CAST 1 << (26 + 3 * (_L)))
#define RSRI_LINKRESETFAIL_BIT(_L)	(25 + 3 * (_L))
#define RSRI_LINKRESETFAIL(_L)	(UINT64_CAST 1 << (25 + 3 * (_L)))
#define RSRI_LOCAL_SHFT		24
#define RSRI_LOCAL_MASK		(UINT64_CAST 0xf << 24)
#define RSRI_LOCALSBERROR	(UINT64_CAST 1 << 27)
#define RSRI_LOCALSTUCK		(UINT64_CAST 1 << 26)
#define RSRI_LOCALBADVEC	(UINT64_CAST 1 << 25)
#define RSRI_LOCALTAILERR	(UINT64_CAST 1 << 24)
#define RSRI_SWLED_SHFT		16
#define RSRI_SWLED_MASK		(UINT64_CAST 0x3f << 16)
#define RSRI_CHIPOUT_SHFT	12
#define RSRI_CHIPOUT_MASK	(UINT64_CAST 0xf << 12)
#define RSRI_CHIPIN_SHFT	8
#define RSRI_CHIPIN_MASK	(UINT64_CAST 0xf << 8)
#define RSRI_CHIPREV_SHFT	4
#define RSRI_CHIPREV_MASK	(UINT64_CAST 0xf << 4)
#define RSRI_CHIPID_SHFT	0
#define RSRI_CHIPID_MASK	(UINT64_CAST 0xf)

#define RSRI_LSTAT_WENTDOWN	0
#define RSRI_LSTAT_RESETFAIL	1
#define RSRI_LSTAT_LINKUP	2
#define RSRI_LSTAT_NOTUSED	3

/*
 * RR_PORT_RESET mask definitions
 */

#define RPRESET_WARM		(UINT64_CAST 1 << 7)
#define RPRESET_LINK(_L)	(UINT64_CAST 1 << (_L))
#define RPRESET_LOCAL		(UINT64_CAST 1)

/*
 * RR_PROT_CONF mask and shift definitions
 */

#define RPCONF_PTTNID_SHFT	13	/* Partition ID */
#define RPCONF_PTTNID_MASK	(UINT64_CAST 0xff << 13)
#define RPCONF_FORCELOCAL	(UINT64_CAST 1 << 12)
#define RPCONF_FLOCAL_SHFT	12
#define RPCONF_METAIDVALID	(UINT64_CAST 1 << 11)
#define RPCONF_METAID_SHFT	6
#define RPCONF_METAID_MASK	(UINT64_CAST 0x1f << 6)
#define RPCONF_RESETOK(_L)	(UINT64_CAST 1 << ((_L) - 1))

/*
 * RR_GLOBAL_PARMS mask and shift definitions
 */

#define RGPARM_LOCGNTTO_SHFT	58	/* Local grant timeout */
#define RGPARM_LOCGNTTO_MASK	(UINT64_CAST 0x3f << 58)
#define RGPARM_MAXRETRY_SHFT	48	/* Max retry count */
#define RGPARM_MAXRETRY_MASK	(UINT64_CAST 0x3ff << 48)
#define RGPARM_TTOWRAP_SHFT	36	/* Tail timeout wrap */
#define RGPARM_TTOWRAP_MASK	(UINT64_CAST 0xfff << 36)
#define RGPARM_AGEWRAP_SHFT	28	/* Age wrap */
#define RGPARM_AGEWRAP_MASK	(UINT64_CAST 0xff << 28)
#define RGPARM_URGWRAP_SHFT	20	/* Urgent wrap */
#define RGPARM_URGWRAP_MASK	(UINT64_CAST 0xff << 20)
#define RGPARM_DEADLKTO_SHFT	16	/* Deadlock timeout */
#define RGPARM_DEADLKTO_MASK	(UINT64_CAST 0xf << 16)
#define RGPARM_URGVAL_SHFT	12	/* Urgent value */
#define RGPARM_URGVAL_MASK	(UINT64_CAST 0xf << 12)
#define RGPARM_LOCURGTO_SHFT	8	/* Local urgent timeout */
#define RGPARM_LOCURGTO_MASK	(UINT64_CAST 0x3 << 8)
#define RGPARM_TAILVAL_SHFT	4	/* Tail value */
#define RGPARM_TAILVAL_MASK	(UINT64_CAST 0xf << 4)
#define RGPARM_CLOCK_SHFT	1	/* Global clock select */
#define RGPARM_CLOCK_MASK	(UINT64_CAST 0x7 << 1)
#define RGPARM_BYPEN_SHFT	0
#define RGPARM_BYPEN_MASK	(UINT64_CAST 1)	/* Bypass enable */

#ifdef SN0XXL
#define RGPARM_AGEWRAP_DEFAULT	0xfe
#else
#define RGPARM_AGEWRAP_DEFAULT	0x20
#endif

/*
 * RR_SCRATCH_REG0 mask and shift definitions
 *  note: these fields represent a software convention, and are not
 *        understood/interpreted by the hardware. 
 */

#define	RSCR0_BOOTED_SHFT	60
#define	RSCR0_BOOTED_MASK	(UINT64_CAST 0x1 << RSCR0_BOOTED_SHFT)
#define RSCR0_LOCALID_SHFT	56
#define RSCR0_LOCALID_MASK	(UINT64_CAST 0xf << RSCR0_LOCALID_SHFT)
#define	RSCR0_UNUSED_SHFT	48
#define	RSCR0_UNUSED_MASK	(UINT64_CAST 0xff << RSCR0_UNUSED_SHFT)
#define RSCR0_NIC_SHFT		0
#define RSCR0_NIC_MASK		(UINT64_CAST 0xffffffffffff)

/*
 * RR_DIAG_PARMS mask and shift definitions
 */

#define RDPARM_ABSHISTOGRAM	(UINT64_CAST 1 << 19)	/* Absolute histgrm */
#define RDPARM_DEADLOCKRESET	(UINT64_CAST 1 << 18)	/* Reset on deadlck */
#define RDPARM_LLP8BIT(_L)	(UINT64_CAST 1 << ((_L) + 11))
#define RDPARM_DISABLE(_L)	(UINT64_CAST 1 << ((_L) +  5))
#define RDPARM_SENDERROR(_L)	(UINT64_CAST 1 << ((_L) -  1))

/*
 * RR_PORT_PARMS(_L) mask and shift definitions
 */

#define RPPARM_HISTSEL_SHFT	18
#define RPPARM_HISTSEL_MASK	(UINT64_CAST 0x3 << 18)
#define RPPARM_DAMQHS_SHFT	16
#define RPPARM_DAMQHS_MASK	(UINT64_CAST 0x3 << 16)
#define RPPARM_NULLTO_SHFT	10
#define RPPARM_NULLTO_MASK	(UINT64_CAST 0x3f << 10)
#define RPPARM_MAXBURST_SHFT	0
#define RPPARM_MAXBURST_MASK	(UINT64_CAST 0x3ff)

/*
 * NOTE: Normally the kernel tracks only UTILIZATION statistics.
 * The other 2 should not be used, except during any experimentation
 * with the router.
 */
#define RPPARM_HISTSEL_AGE	0	/* Histogram age characterization.  */
#define RPPARM_HISTSEL_UTIL	1	/* Histogram link utilization 	    */
#define RPPARM_HISTSEL_DAMQ	2	/* Histogram DAMQ characterization. */

/*
 * RR_STATUS_ERROR(_L) and RR_ERROR_CLEAR(_L) mask and shift definitions
 */

#define RSERR_FIFOOVERFLOW	(UINT64_CAST 1 << 33)
#define RSERR_ILLEGALPORT	(UINT64_CAST 1 << 32)
#define RSERR_DEADLOCKTO_SHFT	28
#define RSERR_DEADLOCKTO_MASK	(UINT64_CAST 0xf << 28)
#define RSERR_RECVTAILTO_SHFT	24
#define RSERR_RECVTAILTO_MASK	(UINT64_CAST 0xf << 24)
#define RSERR_RETRYCNT_SHFT	16
#define RSERR_RETRYCNT_MASK	(UINT64_CAST 0xff << 16)
#define RSERR_CBERRCNT_SHFT	8
#define RSERR_CBERRCNT_MASK	(UINT64_CAST 0xff << 8)
#define RSERR_SNERRCNT_SHFT	0
#define RSERR_SNERRCNT_MASK	(UINT64_CAST 0xff << 0)


#define PORT_STATUS_UP		(1 << 0)	/* Router link up */
#define PORT_STATUS_FENCE	(1 << 1)	/* Router link fenced */
#define PORT_STATUS_RESETFAIL	(1 << 2)	/* Router link didnot 
						 * come out of reset */
#define PORT_STATUS_DISCFAIL	(1 << 3)	/* Router link failed after 
						 * out of reset but before
						 * router tables were
						 * programmed
						 */
#define PORT_STATUS_KERNFAIL	(1 << 4)	/* Router link failed
						 * after reset and the 
						 * router tables were
						 * programmed
						 */
#define PORT_STATUS_UNDEF	(1 << 5)	/* Unable to pinpoint
						 * why the router link
						 * went down
						 */	
#define PROBE_RESULT_BAD	(-1)		/* Set if any of the router
						 * links failed after reset
						 */
#define PROBE_RESULT_GOOD	(0)		/* Set if all the router links
						 * which came out of reset 
						 * are up
						 */
/* 
 * Useful macros to put & get info from the following
 * definition of a router map.
 * 	+-----------------------------------------------+
 *	|MODULE(8bits)|SLOT(8bits) |	NIC (48bits)	|
 * 	+-----------------------------------------------+
 */

/* Module field constants  */
#define ROUTER_MAP_MOD_SHFT	56				
#define ROUTER_MAP_MOD_MASK	(UINT64_CAST 0xff << 56)	
/* Slot field constants */
#define ROUTER_MAP_SLOT_SHFT	48			       
#define ROUTER_MAP_SLOT_MASK	(UINT64_CAST 0xff << 48)
/* Nic field constants */
#define ROUTER_MAP_NIC_SHFT	0				
#define ROUTER_MAP_NIC_MASK	(UINT64_CAST 0xffffffffffff)
		
/* Should be enough for 512 CPUs */
#ifdef SN0XXL
#define MAX_RTR_BREADTH		256		/* Max # of routers possible */
#else
#define MAX_RTR_BREADTH		128		/* Max # of routers possible */
#endif

/* Get the require set of bits in a var. corr to a sequence of bits  */
#define GET_FIELD(var, fname) \
        ((var) >> fname##_SHFT & fname##_MASK >> fname##_SHFT)
/* Set the require set of bits in a var. corr to a sequence of bits  */
#define SET_FIELD(var, fname, fval) \
        ((var) = (var) & ~fname##_MASK | (__uint64_t) (fval) << fname##_SHFT)


#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

struct rr_status_error_fmt {
	__uint64_t	rserr_unused		: 30,
			rserr_fifooverflow	: 1,
			rserr_illegalport	: 1,
			rserr_deadlockto	: 4,
			rserr_recvtailto	: 4,
			rserr_retrycnt		: 8,
			rserr_cberrcnt		: 8,
			rserr_snerrcnt		: 8;
};

/*
 * This type is used to store "absolute" counts of router events
 */
typedef int	router_count_t;

/* All utilizations are on a scale from 0 - 1023. */
#define RP_BYPASS_UTIL	0
#define RP_RCV_UTIL	1
#define RP_SEND_UTIL	2
#define RP_TOTAL_PKTS	3	/* Free running clock/packet counter */

#define RP_NUM_UTILS	3

#define RP_NUM_BUCKETS  4
#define RP_HIST_TYPES	3

#define RP_AGE0		0
#define RP_AGE1		1
#define RP_AGE2		2
#define RP_AGE3		3


#define RR_UTIL_SCALE	1024

/*
 * Router port-oriented information
 */
typedef struct router_port_info_s {
	router_reg_t	rp_histograms;		/* Port usage info */
	router_reg_t	rp_port_error;		/* Port error info */
	router_count_t	rp_retry_errors;	/* Total retry errors */
	router_count_t	rp_sn_errors;		/* Total sn errors */
	router_count_t	rp_cb_errors;		/* Total cb errors */
	int		rp_overflows;		/* Total count overflows */
	int		rp_excess_err;		/* Port has excessive errors */
	ushort		rp_util[RP_NUM_BUCKETS];/* Port utilization */
} router_port_info_t;

#define ROUTER_INFO_VERSION	6

struct lboard_s;

/*
 * Router information
 */
typedef struct router_info_s {
	char		ri_version;	/* structure version		    */
	cnodeid_t	ri_cnode;	/* cnode of its legal guardian hub  */
	nasid_t		ri_nasid;	/* Nasid of same 		    */
	char		ri_ledcache;	/* Last LED bitmap		    */
	char		ri_leds;	/* Current LED bitmap		    */
	char		ri_portmask;	/* Active port bitmap		    */
	router_reg_t	ri_stat_rev_id;	/* Status rev ID value		    */
	net_vec_t	ri_vector;	/* vector from guardian to router   */
	int		ri_writeid;	/* router's vector write ID	    */
	__int64_t	ri_timebase;	/* Time of first sample		    */
	__int64_t	ri_timestamp;	/* Time of last sample		    */
	router_port_info_t ri_port[MAX_ROUTER_PORTS]; /* per port info      */
	moduleid_t	ri_module;	/* Which module are we in?	    */
	slotid_t	ri_slotnum;	/* Which slot are we in?	    */
	router_reg_t	ri_glbl_parms;	/* Global parms register contents   */
	vertex_hdl_t	ri_vertex;	/* hardware graph vertex            */
	router_reg_t	ri_prot_conf;	/* protection config. register	    */
	__int64_t	ri_per_minute;	/* Ticks per minute		    */

	/*
	 * Everything below here is for kernel use only and may change at	
	 * at any time with or without a change in teh revision number
	 *
	 * Any pointers or things that come and go with DEBUG must go at
 	 * the bottom of the structure, below the user stuff.
	 */
	char		ri_hist_type;   /* histogram type		    */
	vertex_hdl_t	ri_guardian;	/* guardian node for the router	    */
	__int64_t	ri_last_print;	/* When did we last print	    */
	char		ri_print;	/* Should we print 		    */
	char 		ri_just_blink;	/* Should we blink the LEDs         */
	
#ifdef DEBUG
	__int64_t	ri_deltatime;	/* Time it took to sample	    */
#endif
	lock_t		ri_lock;	/* Lock for access to router info   */
	net_vec_t	*ri_vecarray;	/* Pointer to array of vectors	    */
	struct lboard_s	*ri_brd;	/* Pointer to board structure	    */
	char *		ri_name;	/* This board's hwg path 	    */
        unsigned char	ri_port_maint[MAX_ROUTER_PORTS]; /* should we send a 
					message to availmon */
} router_info_t;


/* Router info location specifiers */

#define RIP_PROMLOG			2	/* Router info in promlog */
#define RIP_CONSOLE			4	/* Router info on console */

#define ROUTER_INFO_PRINT(_rip,_where)	(_rip->ri_print |= _where)	
					/* Set the field used to check if a 
					 * router info can be printed
					 */
#define IS_ROUTER_INFO_PRINTED(_rip,_where)	\
					(_rip->ri_print & _where)	
					/* Was the router info printed to
					 * the given location (_where) ?
					 * Mainly used to prevent duplicate
					 * router error states.
					 */
#define ROUTER_INFO_LOCK(_rip,_s)	_s = mutex_spinlock(&(_rip->ri_lock))
					/* Take the lock on router info
					 * to gain exclusive access
					 */
#define ROUTER_INFO_UNLOCK(_rip,_s)	mutex_spinunlock(&(_rip->ri_lock),_s)
					/* Release the lock on router info */
/* 
 * Router info hanging in the nodepda 
 */
typedef struct nodepda_router_info_s {
	vertex_hdl_t 	router_vhdl;	/* vertex handle of the router 	    */
	short		router_port;	/* port thru which we entered       */
	short		router_portmask;
	moduleid_t	router_module;	/* module in which router is there  */
	slotid_t	router_slot;	/* router slot			    */
	unsigned char	router_type;	/* kind of router 		    */
	net_vec_t	router_vector;	/* vector from the guardian node    */

	router_info_t	*router_infop;	/* info hanging off the hwg vertex  */
	struct nodepda_router_info_s *router_next;
	                                /* pointer to next element 	    */
} nodepda_router_info_t;

#define ROUTER_NAME_SIZE	20	/* Max size of a router name */

#define NORMAL_ROUTER_NAME	"normal_router"
#define NULL_ROUTER_NAME	"null_router"
#define META_ROUTER_NAME	"meta_router"
#define UNKNOWN_ROUTER_NAME	"unknown_router" 

/* The following definitions are needed by the router traversing
 * code either using the hardware graph or using vector operations.
 */
/* Structure of the router queue element */
typedef struct router_elt_s {
	union {
		/* queue element structure during router probing */
		struct {
			/* number-in-a-can (unique) for the router */
			nic_t		nic;	
			/* vector route from the master hub to 
			 * this router.
			 */
			net_vec_t	vec;	
			/* port status */
			__uint64_t	status;	
			char		port_status[MAX_ROUTER_PORTS + 1];
		} r_elt;
		/* queue element structure during router guardian 
		 * assignment
		 */
		struct {
			/* vertex handle for the router */
			vertex_hdl_t	vhdl;
			/* guardian for this router */
			vertex_hdl_t	guard;	
			/* vector router from the guardian to the router */
			net_vec_t	vec;
		} k_elt;
	} u;
	                        /* easy to use port status interpretation */
} router_elt_t;

/* structure of the router queue */

typedef struct router_queue_s {
	char		head;	/* Point where a queue element is inserted */
	char		tail;	/* Point where a queue element is removed */
	int		type;
	router_elt_t	array[MAX_RTR_BREADTH];
	                        /* Entries for queue elements */
} router_queue_t;


#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

/*
 * RR_HISTOGRAM(_L) mask and shift definitions
 */

#define RHIST_BUCKET_SHFT(_x)	(16 * (_x))
#define RHIST_BUCKET_MASK(_x)	(UINT64_CAST 0xffff << RHIST_BUCKET_SHFT(_x))
#define RHIST_GET_BUCKET(_x, _reg)	\
	((RHIST_BUCKET_MASK(_x) & (_reg)) >> RHIST_BUCKET_SHFT(_x))

/*
 * RR_RESET_MASK(_L) mask and shift definitions
 */

#define RRM_RESETOK(_L)		(UINT64_CAST 1 << ((_L) - 1))
#define RRM_RESETOK_ALL		(UINT64_CAST 0x3f)

/*
 * RR_META_TABLE(_x) and RR_LOCAL_TABLE(_x) mask and shift definitions
 */

#define RTABLE_SHFT(_L)		(4 * ((_L) - 1))
#define RTABLE_MASK(_L)		(UINT64_CAST 0x7 << RTABLE_SHFT(_L))


#ifndef _STANDALONE

#define	ROUTERINFO_STKSZ	4096

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#if defined(_LANGUAGE_C_PLUS_PLUS)
extern "C" {
#endif

int router_reg_read(router_info_t *rip, int regno, router_reg_t *val);
int router_reg_write(router_info_t *rip, int regno, router_reg_t val);
int router_get_info(vertex_hdl_t routerv, router_info_t *, int);
int router_init(cnodeid_t cnode,int writeid, nodepda_router_info_t *npda_rip);
int router_set_leds(router_info_t *rip);
void router_print_state(router_info_t *rip, int level,
		   void (*pf)(int, char *, ...),int print_where);
void capture_router_stats(router_info_t *rip);


int 	router_nic_get(net_vec_t path,__uint64_t *nic);
int 	probe_routers(void);
void 	get_routername(unsigned char brd_type,char *rtrname);
void 	router_guardians_set(vertex_hdl_t hwgraph_root);
int 	router_hist_reselect(router_info_t *, __int64_t);
#if defined(_LANGUAGE_C_PLUS_PLUS)
}
#endif
#endif /* _STANDALONE */
#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#endif /* __SYS_SN_ROUTER_H__ */
