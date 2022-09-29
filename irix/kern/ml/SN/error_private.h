/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: error_private.h
 *	Pertains to SN0 errors.
 */

#include <sys/clksupport.h>

#include <ksys/partition.h>
#include "sn_private.h"
#include <sys/SN/error_force.h>

#define MD_MAX_ERR_LOG			256 /* Max saved errors */
#define MD_ERR_LOG_INIT(elog)		((elog)->el_ndx = 0)
#define MD_ERR_LOG_FIRST(_elog)		md_err_log_first(_elog)
#define MD_ERR_LOG_NEXT(_elog, _ent)	md_err_log_next(_elog, _ent)
#define MD_ERR_LOG_LAST(_elog)		md_err_log_last(_elog)
#define MD_ERR_LOG_PREV(_elog, _ent)	md_err_log_prev(_elog, _ent)
#define MD_ERR_LOG_NEW(_elog)		md_err_log_new(_elog)
#define ERROR_INTR			1

#define BERR_BAD_ADDR_MAGIC	0xbad00bad	/* potentially a valid address.
						 * this is returned from ld_st
						 * in trap.c. Changing that
						 * to return -1 would probably
						 * be more reasonable 
						 */
enum md_err_enum {
	NO_ERROR = 0,
	DIR_ERROR,
	MEM_ERROR, 
	PROTO_ERROR, 
	MISC_ERROR
};

enum pi_spool_type {
	SPOOL_LOST = -1,
	SPOOL_NONE =  0,
	SPOOL_MEM,
	SPOOL_REG
};

typedef enum error_result {
	ERROR_NONE,
	ERROR_HANDLED,
	ERROR_USER,
	ERROR_FATAL
} error_result_t;

enum part_error_type;			/* defined in ksys/partition.h */

#define CORRECTABLE_ERROR	0
#define UNCORRECTABLE_ERROR	1

#define PERMANENT_ERROR		1
#define TRANSIENT_ERROR		0

enum pi_spool_type get_matching_spool_addr(hubreg_t *, 
					   hubreg_t *, paddr_t, int, int);

error_result_t	error_check_poisoned_state(paddr_t, caddr_t, int);
error_result_t  error_check_memory_access(paddr_t, int, int);


#define RECOVER_MIGRATE		0
#define RECOVER_NACK		1
#define RECOVER_MISC		2
#define RECOVER_UNKNOWN		3

#define RECOVER_MAX_TYPES	(RECOVER_UNKNOWN + 1)

#define KLE_MIGR_THRESH		3
#define	KLE_NACK_THRESH		10
#define KLE_MISC_THRESH		10
#define KLE_UNK_THRESH		3

#if defined (DEBUG)
#define KLE_MIGR_TDIFF		0x2000
#define KLE_NACK_TDIFF		0x1000000
#define KLE_MISC_TDIFF		0x1000000
#define KLE_UNK_TDIFF		0x1000000

#define GL_ERROR_THRESH		30
#define GL_TIME_THRESH		0x1000000

#else
#define KLE_MIGR_TDIFF		0x10000
#define KLE_NACK_TDIFF		0x100000	
#define KLE_MISC_TDIFF		0x100000
#define KLE_UNK_TDIFF		0x1000000

#define GL_ERROR_THRESH		30
#define GL_TIME_THRESH		0x1000000
#endif



typedef struct klerror_recover {
	unsigned char	kr_threshold;
	unsigned char	kr_err_count;
	unsigned char	kr_global;
	unsigned char	kr_paddr_depend;
	__uint32_t	kr_time_diff;
	__uint64_t 	kr_start_time;
	__psunsigned_t  kr_paddr;
	void	 	*kr_key;
}klerror_recover_t;


typedef struct sn0_error_log {
	unsigned int el_ndx;
	struct md_err_reg {
		hubreg_t er_reg;
		clkreg_t er_rtc;
		enum md_err_enum er_type;
	} el_err_reg[MD_MAX_ERR_LOG];
	int el_err_found;
	klerror_recover_t el_recover_table[CPUS_PER_NODE][RECOVER_MAX_TYPES];
	unsigned long el_spool_cur_addr[CPUS_PER_NODE];
	unsigned long el_spool_last_addr[CPUS_PER_NODE];
	unsigned long el_spool_size_bytes;
} sn0_error_log_t;


struct error_progress_state {
	int eps_state;
	cpuid_t eps_cpuid;
	lock_t	eps_lock;
	int	eps_discard_count;
	paddr_t	eps_discard_paddr[MAX_COMPACT_NODES];
};
extern struct error_progress_state error_consistency_check;

#define ERROR_CONSISTENCY_LOCK(_s)	\
	(_s) = mutex_spinlock(&error_consistency_check.eps_lock)
#define ERROR_CONSISTENCY_UNLOCK(_s)	\
	mutex_spinunlock(&error_consistency_check.eps_lock, (_s));


#define DUMP_ERRSAVE_START	0x01
#define DUMP_ERRSAVE_DONE	0x02
#define DUMP_FRU_START		0x04
#define DUMP_FRU_DONE		0x08
#define DUMP_ERRSHOW_START	0x10
#define DUMP_ERRSHOW_DONE	0x20

#define ERROR_ANYTIME 		-1
#define ERROR_LONG_TIME		300000000	/* 300 secs? */

#define MDERR_DISCARD_AFTER	300000000	/* ok to discard error after
						 * this period of time.
						 */
#define MDERR_DISCARD(_clk, _clk1) \
        (((_clk1) - (_clk)) > MDERR_DISCARD_AFTER)

#define MDERR_TIME_CHECK(_ent, _gap)		\
        (((_gap) == ERROR_ANYTIME) || \
	 ((ERROR_TIMESTAMP - (_ent)->er_rtc) < (_gap)))

extern sn0_error_log_t 	*kl_error_log[MAX_COMPACT_NODES];

#define HUBPI_SPOOL_ADDR(_x)	((__int64_t *)(UALIAS_BASE + (_x)))
#define SN0_ERROR_LOG(_cnodeid)	(kl_error_log[(_cnodeid)])

/*
 * move the spool indices to the pda later?
 */

#define CUR_SPOOL_ADDR	\
      (SN0_ERROR_LOG(cnodeid())->el_spool_cur_addr[cputoslice(cpuid())])
#define LAST_SPOOL_ADDR	\
      (SN0_ERROR_LOG(cnodeid())->el_spool_last_addr[cputoslice(cpuid())])

#define PI_SPOOL_SIZE_BYTES \
      (SN0_ERROR_LOG(cnodeid())->el_spool_size_bytes)

#define RECOVER_ERROR_TABLE(_n, _c) \
      (SN0_ERROR_LOG(_n)->el_recover_table[(_c)])

#define SPOOL_MEM_ERROR_ADDR(_stk) 	\
        ((_stk).pi_stk_fmt.sk_addr << ERR_STK_ADDR_SHFT)

#define SPOOL_REG_ERROR_ADDR(_stk)	\
        ((_stk).pi_stat0_fmt.s0_addr << ERR_STAT0_ADDR_SHFT)

#define MEM_ERROR_ADDR(_merr)		\
        ((_merr).merr_fmt.address << ERROR_ADDR_SHFT)
#define MEM_ERROR_ADDR_MATCH(err_addr, real_addr) \
        (((err_addr) & ~scache_linemask) == ((real_addr) & ~scache_linemask))

#define PROTO_ERROR_ADDR(_perr)		\
        ((_perr).perr_fmt.address << ERROR_ADDR_SHFT)
#define PROTO_ERROR_ADDR_MATCH(err_addr, real_addr) \
        (((err_addr) & ~scache_linemask) == ((real_addr) & ~scache_linemask))

#define DIR_ERROR_ADDR(_derr)		\
        ((_derr).derr_fmt.hspec_addr << ERROR_HSPEC_SHFT)
/* Only test the ecc space offset, and match low or high dir. entry addr. */
#define DIR_ERR_ADDR_MASK  (DIR_ERR_HSPEC_MASK & ~0x8)
#define DIR_ERROR_ADDR_MATCH(err_addr, real_addr) \
        (((err_addr) & DIR_ERR_ADDR_MASK) == ((real_addr) & DIR_ERR_ADDR_MASK))


#define SPOOL_MEM_RANGE	(1 << ERR_STK_ADDR_SHFT)
#define SPOOL_MEM_ADDR_MATCH(err_addr, real_addr, range)	\
        ((SPOOL_MEM_RANGE > (range)) ?			\
	 (((real_addr)>=(err_addr)) && ((real_addr) <= ((err_addr)+SPOOL_MEM_RANGE))):\
	 (((real_addr)>=(err_addr)) && ((real_addr) <= ((err_addr) + (range)))))

#define SPOOL_REG_RANGE	(1 << ERR_STAT0_ADDR_SHFT)

#define SPOOL_REG_ADDR_MATCH(err_addr, real_addr, range)	\
        ((SPOOL_REG_RANGE > (range)) ?				\
	 (((real_addr)>=(err_addr)) && ((real_addr) <= ((err_addr)+SPOOL_REG_RANGE))):\
	 (((real_addr)>=(err_addr)) && ((real_addr) <= ((err_addr) + (range)))))


extern void process_error_spool(void);
extern void process_write_error_intr(hubreg_t);
extern void kl_panic(char *);
extern void kl_panic_setup(void);
extern void log_md_errors(nasid_t);
extern int check_md_errors(paddr_t, hubreg_t *, int, __psunsigned_t *);
extern int error_page_discard(paddr_t, int, int);
#if defined (T5_WB_SURPRISE_WAR)
extern int error_try_wb_surprise_war(paddr_t);
extern int error_check_wb_surprise(paddr_t, hubreg_t *);
extern int error_search_wb_surprise(void);
#endif

void	hubpi_eint_handler(eframe_t *);
void	hubni_eint_handler(void);
void	hubni_rmteint_handler(void);
void	hubii_eint_handler(void *, eframe_t *);
void 	hubii_eint_init(cnodeid_t);
void 	hubni_eint_init(cnodeid_t);

int	hubiio_crb_error_handler(vertex_hdl_t, hubinfo_t);
int	handle_errstk_entry(hubreg_t *, hubreg_t *, enum pi_spool_type);
void 	hubpi_stack_params(nasid_t, unsigned int *, unsigned int *);

void	klsave_spool_registers(hubreg_t, hubreg_t, int);
void	klsave_md_registers(hubreg_t, enum md_err_enum, nasid_t);


#define HUBPI_ERR_INT_MASK	((cputoslice(cpuid()) == 0) ? \
				 PI_ERR_INT_MASK_A     : PI_ERR_INT_MASK_B)
#define HUBPI_ERR_SPOOL_CMP	((cputoslice(cpuid()) == 0) ? \
				 PI_ERR_SPOOL_CMP_A    : PI_ERR_SPOOL_CMP_B)
#define HUBPI_ERR_WRB_WERR	((cputoslice(cpuid()) == 0) ? \
				 PI_ERR_WRB_WERR_A     : PI_ERR_WRB_WERR_B)
#define HUBPI_ERR_WRB_TERR	((cputoslice(cpuid()) == 0) ? \
				 PI_ERR_WRB_TERR_A     : PI_ERR_WRB_TERR_B)

#define HUBPI_ERR_UNCAC_UNCORR	((cputoslice(cpuid()) == 0) ?	\
				 PI_ERR_UNCAC_UNCORR_A : PI_ERR_UNCAC_UNCORR_B)

#define HUBPI_ERR_SPUR_MSG	((cputoslice(cpuid()) == 0) ?	\
				 PI_ERR_SPUR_MSG_A     : PI_ERR_SPUR_MSG_B)

#define HUBPI_ERR_STATUS0	((cputoslice(cpuid()) == 0) ? 	\
				 PI_ERR_STATUS0_A      : PI_ERR_STATUS0_B)

#define HUBPI_ERR_STATUS1	((cputoslice(cpuid()) == 0) ? 	\
				 PI_ERR_STATUS1_A      : PI_ERR_STATUS1_B)

#define HUBPI_ERR_STATUS0_RCLR	((cputoslice(cpuid()) == 0) ? 	\
				 PI_ERR_STATUS0_A_RCLR : PI_ERR_STATUS0_B_RCLR)

#define HUBPI_ERR_STATUS1_RCLR	((cputoslice(cpuid()) == 0) ? 	\
				 PI_ERR_STATUS1_A_RCLR : PI_ERR_STATUS1_B_RCLR)

#define HUBPI_ERR_STACK_ADDR	((cputoslice(cpuid()) == 0) ? 	\
				 PI_ERR_STACK_ADDR_A   : PI_ERR_STACK_ADDR_B)


#define ERR_NDX_BUSERR		2
#define ERR_NDX_PI_EINT		3
#define ERR_NDX_NI_EINT		4
#define ERR_NDX_II_EINT		5

#define ERR_CONT	CE_CONT  | CE_PHYSID
#define ERR_WARN	CE_WARN  | CE_PHYSID
#define ERR_NOTE	CE_NOTE  | CE_PHYSID
#define ERR_PANIC	CE_PANIC | CE_PHYSID


#define READ_ERROR	0
#define WRITE_ERROR	1


/**** FIXME *** this belong in hub includes! -uday ****/

#define INITIATOR_TO_NODE(_x)	((_x) >> 2)
#define INITIATOR_TO_CPU(_x)	((_x) & 0x3)
#define INITIATOR_IS_CPU(_x)	(((_x) & 0x2) ? 0 : 1)

#define PROTO_ERR_MSG_WB	0x40

#define FATAL_ERROR_USER(_fl)	\
               (((_fl) == BERR_NOFAULT) ? FATAL_NOFAULT :	\
		((_fl) == BERR_USER) ? FATAL_USER : FATAL_KERNEL)

#define FATAL_ERROR_KERNEL(_fl)	\
               (((_fl) == BERR_NOFAULT) ? FATAL_NOFAULT : FATAL_KERNEL)


/* 
 * Premium Directory ECC matrix
 */


#define E7_6W_PDR 0x137F6091283
#define E7_5W_PDR 0x1109FFCA484
#define E7_4W_PDR 0x11904437F36
#define E7_3W_PDR 0x09A229780FF
#define E7_2W_PDR 0x05E53258F68
#define E7_1W_PDR 0x0E86E3AF808
#define E7_0W_PDR 0x0ED8DC1C151


/*
 * Standard Dir ECC matrix
 */

#define E5_4W_SDR 0x71E
#define E5_3W_SDR 0x667
#define E5_2W_SDR 0x4DB
#define E5_1W_SDR 0x7F0
#define E5_0W_SDR 0x5AD

struct dir_ecc_mask {
	__uint64_t dir_emask;
};

#define PDIR_ECC_BITS    7
#define PREM_DIR_BITS	41
#define PREM_DIR_MASK	((1L << PREM_DIR_BITS) - 1)

#define SDIR_ECC_BITS    5
#define STD_DIR_BITS	11
#define STD_DIR_MASK	((1 << STD_DIR_BITS) - 1)
/*
 * Memory ECC matrix
 */


#define E8_7W 0xFF280FF088880928
#define E8_6W 0xFA24000F4444FF24
#define E8_5W 0x0B22FF002222FA32
#define E8_4W 0x0931F0FF11110B21
#define E8_3W 0x84D08888FF0F8C50
#define E8_2W 0x4C9F444400FF44D0
#define E8_1W 0x24FF2222F000249F
#define E8_0W 0x145011110FF014FF


struct mem_ecc_mask {
	__uint64_t mem_emask;
};


#define MEM_ECC_BITS 8


/*
 *  Flag from the bus error.
 */
#define BERR_KERNEL	0
#define	BERR_NOFAULT	1
#define BERR_USER	2


#define ERR_INT_SET(_bitmask)      (LOCAL_HUB_L(PI_ERR_INT_PEND) & (_bitmask))
#define ERR_INT_RESET(_bitmask)	   (LOCAL_HUB_L(PI_ERR_INT_PEND) = (_bitmask))

/*
 * We know a node is not in our partition if the translation from nasid
 * to partid doesn't match our part id.  If partitioning is not present
 * on the machine then both nasid_to_partid and part_id both return 0 by
 * the stubs and this returns the correct thing because we know that there
 * are no remote partitions and this should always be false!  If the
 * nasid_to_partid is not valid, then it could be on another partition
 * that went down.....
 */


#define REMOTE_PARTITION_NODE(_nasid)	\
                ((nasid_to_partid(_nasid) != part_id()) ? 1 : 0)

#define PARTITION_HANDLE_ERROR(_ep, _addr, _code)		\
                partition_handle_error(_ep, _addr, _code)

#define PARTITION_PAGE(_addr)		\
                error_check_part_page(_addr)

extern void error_reg_partition_handler(error_result_t 
					(*handler)(eframe_t *,
						   paddr_t,
						   enum part_error_type));
extern int error_check_part_page(paddr_t);
error_result_t partition_handle_error(eframe_t *, paddr_t, 
				      enum part_error_type);

/*
 * Various error related compilation place holders. These will get removed
 * as the respective functionality is added.
 */

/*
 * SET_PAGE_INACCESSIBLE does 2 things:
 * 1) uses vm support to mark a page as not accessible
 * 2) modifies the access bits on the hub for in partition access.
 */
#define SET_PAGE_INACCESSIBLE(_addr)	\
                (printf("fixme, SET_PAGE_INACCESSIBLE\n"), 0)

#define REGION_PRESENT(_reg)	\
	 (LOCAL_HUB_L(PI_REGION_PRESENT) & ((__uint64_t)1 << (_reg)))


#define WIDGET_TYPE(_wp)	(printf("fixme, WIDGET_TYPE\n"), 0)
#define WID_ID(_wp)		(printf("fixme, WID_ID\n"), 0)
#define WIDGET_NASID_NODEID(_wp) (printf("fixme, WIDGET_NASID_NODEID\n"), 0)

struct md_err_reg *md_err_log_first(sn0_error_log_t *);
struct md_err_reg *md_err_log_next(sn0_error_log_t *, struct md_err_reg *);
struct md_err_reg *md_err_log_new(sn0_error_log_t *);
struct md_err_reg *md_err_log_last(sn0_error_log_t *);
struct md_err_reg *md_err_log_prev(sn0_error_log_t *, struct md_err_reg *);

extern void show_ni_port_error(hubreg_t, int, void (*)(int, char *, ...));
extern void show_rr_status_rev_id(router_reg_t, int, int, void (*)(int, char *, ...));
extern void show_rr_error_status(router_reg_t, int, int, void (*)(int, char *, ...), int);

#if defined (SN0)  && defined (FORCE_ERRORS)

#define EF_BTE_PULL 0
#define EF_BTE_PUSH 1

#define EF_FORGE_SPURIOUS	1
#define EF_FORGE_WRITE		2
#define EF_FORGE_READ		3

#define EF_ABSENT_MEM_BANK	1
#define EF_ABSENT_NODE		2

#define EF_MEMBANK_ADDR(_node,_bank) (NODE_CAC_BASE(_node)+(UINT64_CAST (_bank) << 29))


extern int kl_force_pdecc(paddr_t, md_error_params_t *, rval_t *);
extern int kl_force_scecc(paddr_t, md_error_params_t *, rval_t *);
extern int kl_force_direcc(md_error_params_t *, rval_t *);
extern int kl_force_memecc(paddr_t, md_error_params_t *, rval_t *);
extern int kl_force_piecc(paddr_t, int);

/* MD errors */
extern int error_force_memerror(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_direrror(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_prot(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_protoerr(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_poisonerr(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_nackerr(md_error_params_t *uap,  rval_t *rvp);
extern int error_force_flush(__psunsigned_t addr);
extern int error_force_aerr_bte_pull(md_error_params_t *params, rval_t *rvp);
extern int error_force_bddir_bte(md_error_params_t *params, int direction, rval_t *rvp);
extern int error_force_perr_bte(int direction,rval_t *rvp);
extern int error_force_corrupt_bte_pull(md_error_params_t *params, rval_t *rvp);
extern int error_force_bte_absent_node(int direction, rval_t *rvp);
extern int error_force_set_dir_state(md_error_params_t *uap,rval_t *rvp);

/* IO error code */
extern int error_force_set_read_access(io_error_params_t * params, int val, 
				       rval_t *rvp);
extern int error_force_pio_write_absent_reg(io_error_params_t * params, int response,
					    int twice, rval_t *rvp);
extern int error_force_absent_widget(rval_t *rvp);
extern int error_force_forge_widgetId(io_error_params_t *params, int action, rval_t *rvp);
extern int error_force_xtalk_sideband(io_error_params_t *params, rval_t *rvp);
extern int error_force_clr_outbound_access(io_error_params_t *params, rval_t *rvp);
extern int error_force_clr_local_access(io_error_params_t *params, rval_t *rvp);
extern int error_force_xtalk_credit_timeout(io_error_params_t *params, rval_t *rvp);
extern int error_force_set_CRB_timeout(io_error_params_t *params, int count,
				       rval_t *rvp);
extern int error_force_xtalk_force_addr(io_error_params_t *params,int type, 
					rval_t *rvp);

/* NI error code */
extern int error_force_link_reset(ni_error_params_t *params, rval_t *rvp);
extern int error_force_rtr_link_down(cnodeid_t cnode,net_vec_t vec,int port);

/* General code */
extern void* error_alloc(int size);
extern int error_force_nodeValid(cnodeid_t node);
extern int error_force_daccess(volatile __uint64_t *, int ,int ,int , rval_t *);
extern int error_force_waccess(volatile __uint32_t *, int ,int ,int , rval_t *);
extern void error_reg_partition_handler(error_result_t (*)(eframe_t *, paddr_t,
							   enum part_error_type));
extern cnodeid_t error_force_get_absent_node(void);
extern __int64_t error_force_absent_node_mem(void);

#define EF_DEBUG

#if defined(EF_DEBUG)
extern int ef_print;
#define EF_PRINT(x) { if (ef_print) printf##x; }
#define EF_PRINT_PARAMS(x) { print_error_params(x); }

#else
#define EF_PRINT(x)
#define EF_PRINT_PARAMS(x)

#endif /* EF_DEBUG */


#endif /* SN0 && FORCE_ERRORS */



extern int sCacheSize		(void);
extern int get_cpu_cnf 		(void);

extern void iLine	(__uint64_t,r10k_il_t *);
extern void dLine	(__uint64_t,r10k_dl_t *);
extern void sLine	(__uint64_t,r10k_sl_t *);


#define R10K_CACHSAVE_NO                ' no '
#define R10K_CACHSAVE_PART              'part'
#define R10K_CACHSAVE_OK                ' ok '


typedef struct r10k_cache_debug_s {
        rev_id_t         cpu_ri;
        r10k_conf_t      cpu_conf;
        __uint32_t      *iCacheMem;
        __uint32_t       il_stat;
        r10k_il_t       *il;

        __uint32_t      *dCacheMem;
        __uint32_t       dl_stat;
        r10k_dl_t       *dl;

        __uint64_t       sl_size;
        __uint32_t      *sCacheMem;
        __uint32_t       sl_stat;
        r10k_sl_t       *sl;
} r10k_cache_debug_t;

