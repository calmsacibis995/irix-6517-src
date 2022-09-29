/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986-1996 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SN_PRIVATE_H__
#define __SN_PRIVATE_H__
/*
 * This file contains definitions that are private to the ml/SN0
 * directory.  They should not be used outside this directory.  We
 * reserve the right to change the implementation of functions and
 * variables here as well as any interfaces defined in this file.
 * Interfaces defined in sys/SN are considered by others to be
 * fair game.
 */

#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xtalk_private.h>

#if defined (SN0)
#include "SN0/sn0_private.h"
#elif defined (SN1)
#include "SN1/sn1_private.h"
#endif

#ifdef SABLE
extern volatile int sable_mastercounter;
extern int fake_prom;
#endif
extern nasid_t master_nasid;

extern hubreg_t get_region(cnodeid_t);
extern hubreg_t	nasid_to_region(nasid_t);
/* promif.c */
extern cpuid_t cpu_node_probe(cpumask_t *cpumask, int *numnodes);
extern void he_arcs_set_vectors(void);
extern void mem_init(void);
extern int cpu_enabled(cpuid_t);
extern void cpu_unenable(cpuid_t);
extern nasid_t get_lowest_nasid(void);
extern __psunsigned_t get_master_bridge_base(void);
extern void set_master_bridge_base(void);
extern int check_nasid_equiv(nasid_t, nasid_t);
extern nasid_t get_console_nasid(void);
extern char get_console_pcislot(void);

extern int is_master_nasid_widget(nasid_t test_nasid, xwidgetnum_t test_wid);

/* memsupport.c */
extern void poison_state_alter_range(__psunsigned_t start, int len, int poison);
extern int memory_present(paddr_t);
extern int memory_read_accessible(paddr_t);
extern int memory_write_accessible(paddr_t);
extern int memory_set_access(paddr_t, int, int);
extern void show_dir_state(paddr_t, void (*)(char *, ...));
extern void check_dir_state(nasid_t, int, void (*)(char *, ...));
extern void set_dir_owner(paddr_t, int);
extern void set_dir_state(paddr_t, int);
extern void get_dir_ent(paddr_t paddr, int *state,
			__uint64_t *vec_ptr, hubreg_t *elo);

/* intr.c */
extern void intr_init(void);
extern int intr_reserve_level(cpuid_t cpu, int level, int err, vertex_hdl_t owner_dev, char *name);
extern void intr_unreserve_level(cpuid_t cpu, int level);
extern int intr_connect_level(cpuid_t cpu, int bit, ilvl_t mask_no, 
			intr_func_t intr_func, void *intr_arg,
			intr_func_t intr_prefunc);
extern int intr_disconnect_level(cpuid_t cpu, int bit);
extern void intr_block_bit(cpuid_t cpu, int bit);
extern void intr_unblock_bit(cpuid_t cpu, int bit);
extern void setrtvector(intr_func_t);
extern void install_cpuintr(cpuid_t cpu);
extern void install_dbgintr(cpuid_t cpu);
extern void install_tlbintr(cpuid_t cpu);
extern void hub_migrintr_init(cnodeid_t /*cnode*/);
extern int cause_intr_connect(int level, intr_func_t handler, uint intr_spl_mask);
extern int cause_intr_disconnect(int level);
extern cpuid_t intr_heuristic(vertex_hdl_t dev, device_desc_t dev_desc,
			      int req_bit,int intr_resflags,dev_t owner_dev,
			      char *intr_name,int *resp_bit);
extern void intr_reserve_hardwired(cnodeid_t);
extern void intr_clear_all(nasid_t);
extern void intr_dumpvec(cnodeid_t cnode, void (*pf)(char *, ...));
extern int protected_broadcast(hubreg_t intrbit);

/* error_dump.c */
extern char *hub_rrb_err_type[];
extern char *hub_wrb_err_type[];

void nmi_dump(void);
void install_cpu_nmi_handler(int slice);

/* klclock.c */
extern void hub_rtc_init(cnodeid_t);

/* klgraph.c */
void klhwg_add_all_nodes(vertex_hdl_t);
void klhwg_add_all_modules(vertex_hdl_t);

/* klidbg.c */
void install_klidbg_functions(void);

/* klnuma.c */
extern void replicate_kernel_text(int numnodes);
extern __psunsigned_t get_freemem_start(cnodeid_t cnode);
extern void setup_replication_mask(int maxnodes);

/* init.c */
extern cnodeid_t get_compact_nodeid(void);	/* get compact node id */
extern void init_platform_nodepda(nodepda_t *npda, cnodeid_t node);
extern void init_platform_pda(pda_t *ppda, cpuid_t cpu);
extern void per_cpu_init(void);
extern void per_hub_init(cnodeid_t);
extern cpumask_t boot_cpumask;
extern int is_fine_dirmode(void);
extern void update_node_information(cnodeid_t);
 
/* clksupport.c */
extern void early_counter_intr(eframe_t *);

/* hubio.c */
extern void hubio_init(void);
extern void hub_merge_clean(nasid_t nasid);
extern void hub_set_piomode(nasid_t nasid, int conveyor);

/* Used for debugger to signal upper software a breakpoint has taken place */

extern void		*debugger_update;
extern __psunsigned_t	debugger_stopped;

/* 
 * SN0 piomap, created by hub_pio_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_piomap_s by generic xtalk routines.
 */
struct hub_piomap_s {
	struct xtalk_piomap_s	hpio_xtalk_info;/* standard crosstalk pio info */
	vertex_hdl_t		hpio_hub;	/* which hub's mapping registers are set up */
	short			hpio_holdcnt;	/* count of current users of bigwin mapping */
	char			hpio_bigwin_num;/* if big window map, which one */
	int 			hpio_flags;	/* defined below */
};
/* hub_piomap flags */
#define HUB_PIOMAP_IS_VALID		0x1
#define HUB_PIOMAP_IS_BIGWINDOW		0x2
#define HUB_PIOMAP_IS_FIXED		0x4

#define	hub_piomap_xt_piomap(hp)	(&hp->hpio_xtalk_info)
#define	hub_piomap_hub_v(hp)	(hp->hpio_hub)
#define	hub_piomap_winnum(hp)	(hp->hpio_bigwin_num)

#if TBD
 /* Ensure that hpio_xtalk_info is first */
 #assert (&(((struct hub_piomap_s *)0)->hpio_xtalk_info) == 0)
#endif


/* 
 * SN0 dmamap, created by hub_pio_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_dmamap_s by generic xtalk routines.
 */
struct hub_dmamap_s {
	struct xtalk_dmamap_s	hdma_xtalk_info;/* standard crosstalk dma info */
	vertex_hdl_t		hdma_hub;	/* which hub we go through */
	int			hdma_flags;	/* defined below */
};
/* hub_dmamap flags */
#define HUB_DMAMAP_IS_VALID		0x1
#define HUB_DMAMAP_USED			0x2
#define	HUB_DMAMAP_IS_FIXED		0x4

#if TBD
 /* Ensure that hdma_xtalk_info is first */
 #assert (&(((struct hub_dmamap_s *)0)->hdma_xtalk_info) == 0)
#endif

/* 
 * SN0 interrupt handle, created by hub_intr_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_intr_s by generic xtalk routines.
 */
struct hub_intr_s {
	struct xtalk_intr_s	i_xtalk_info;	/* standard crosstalk intr info */
	ilvl_t			i_swlevel;	/* software level for blocking intr */
	cpuid_t			i_cpuid;	/* which cpu */
	int			i_bit;		/* which bit */
	int			i_flags;
};
/* flag values */
#define HUB_INTR_IS_ALLOCED	0x1	/* for debug: allocated */
#define HUB_INTR_IS_CONNECTED	0x4	/* for debug: connected to a software driver */

#if TBD
 /* Ensure that i_xtalk_info is first */
 #assert (&(((struct hub_intr_s *)0)->i_xtalk_info) == 0)
#endif


/* SN0 hub-specific information stored under INFO_LBL_HUB_INFO */
/* TBD: SN0-dependent stuff currently in nodepda.h should be here */
typedef struct hubinfo_s {
	nodepda_t			*h_nodepda;	/* pointer to node's private data area */
	cnodeid_t			h_cnodeid;	/* compact nodeid */
	nasid_t				h_nasid;	/* nasid */

	/* structures for PIO management */
	xwidgetnum_t			h_widgetid;	/* my widget # (as viewed from xbow) */
	struct hub_piomap_s		h_small_window_piomap[HUB_WIDGET_ID_MAX+1];
	sv_t				h_bwwait;	/* wait for big window to free */
	lock_t				h_bwlock;	/* guard big window piomap's */
	lock_t				h_crblock;      /* gaurd CRB error handling */
	int				h_num_big_window_fixed;	/* count number of FIXED maps */
	struct hub_piomap_s		h_big_window_piomap[HUB_NUM_BIG_WINDOW];
	hub_intr_t			hub_ii_errintr;
} *hubinfo_t;

#define hubinfo_get(vhdl, infoptr) ((void)hwgraph_info_get_LBL \
	(vhdl, INFO_LBL_NODE_INFO, (arbitrary_info_t *)infoptr))

#define hubinfo_set(vhdl, infoptr) (void)hwgraph_info_add_LBL \
	(vhdl, INFO_LBL_NODE_INFO, (arbitrary_info_t)infoptr)

#define	hubinfo_to_hubv(hinfo, hub_v)	(hinfo->h_nodepda->node_vertex)

/*
 * Hub info PIO map access functions.
 */
#define	hubinfo_bwin_piomap_get(hinfo, win) 	\
			(&hinfo->h_big_window_piomap[win])
#define	hubinfo_swin_piomap_get(hinfo, win)	\
			(&hinfo->h_small_window_piomap[win])
	
/* SN0 cpu-specific information stored under INFO_LBL_CPU_INFO */
/* TBD: SN0-dependent stuff currently in pda.h should be here */
typedef struct cpuinfo_s {
	pda_t		*ci_cpupda;	/* pointer to CPU's private data area */
	cpuid_t		ci_cpuid;	/* CPU ID */
} *cpuinfo_t;

#define cpuinfo_get(vhdl, infoptr) ((void)hwgraph_info_get_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t *)infoptr))

#define cpuinfo_set(vhdl, infoptr) (void)hwgraph_info_add_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t)infoptr)

/* Special initialization function for xswitch vertices created during startup. */
extern void xswitch_vertex_init(vertex_hdl_t xswitch);

extern xtalk_provider_t hub_provider;

/* du.c */
extern void ducons_write(char *buf, int len);

/* memerror.c */

extern void install_eccintr(cpuid_t cpu);
extern void memerror_get_stats(cnodeid_t cnode,
			       int *bank_stats, int *bank_stats_max);
extern void probe_md_errors(nasid_t);
/* sysctlr.c */
extern void sysctlr_init(void);
extern void sysctlr_power_off(int sdonly);
extern void sysctlr_keepalive(void);

#define valid_cpuid(_x)		(((_x) >= 0) && ((_x) < maxcpus))

/* Useful definitions to get the memory dimm given a physical
 * address.
 */
#define MEM_DIMM_MASK		0xe0000000
#define MEM_DIMM_SHFT		29
#define paddr_dimm(_pa)		((_pa & MEM_DIMM_MASK) >> MEM_DIMM_SHFT)
#define paddr_cnode(_pa)	(NASID_TO_COMPACT_NODEID(NASID_GET(_pa)))
extern void membank_pathname_get(paddr_t,char *);

/* To redirect the output into the error buffer */
#define errbuf_print(_s)	printf("#%s",_s)

#ifdef SABLE
extern void du_earlyinit(void);
#ifdef SABLE_SYMMON
extern void sable_symmon_slave_launch( int );
#endif /* SABLE_SYMMON */
#endif /* SABLE */
#endif /* __SN_PRIVATE_H__ */
