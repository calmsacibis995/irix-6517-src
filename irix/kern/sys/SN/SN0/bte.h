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

#ifndef __SYS_SN_SN0_BTE_H__
#define __SYS_SN_SN0_BTE_H__

#include <sys/SN/SN0/arch.h>
#include <sys/ioerror.h>
/*
 * bte.h -- Contains structure definitions and macros needed by
 *	clients of the block transfer engine.  The BTE is a 
 *	general purpose cache-coherent DMA engine.  Residing in
 *	the IO section of the Hub ASIC, the BTE can be programmed
 *	either to copy from one memory range to another or to fill
 *	a specified memory range with copies of a specified
 *	128 byte aligned block of memory.	
 */

/*
 * Machine-specific constants.
 */

#if defined(SN0)
#  define BTE_SRC_ALIGN		0x7f
#  define BTE_DEST_ALIGN	0x7f
#  define BTE_LEN_MINSIZE	0x80
#  define BTE_LEN_SHIFT		0x7
#  define BTE_LEN_MAXLINES	((1 << 16) - 1)	/* Size of the register */
#  define BTE_LEN_MAXSIZE	(BTE_LEN_MAXLINES << BTE_LEN_SHIFT)
#  define BTE_LEN_ALIGN		0x7f
#endif /* defined(SN0) */

typedef enum {
    BTE_SUCCESS,			/* 0 is success */
    BTEFAIL_NOTAVAIL,			/* BTE not available */
    BTEFAIL_POISON,			/* poison page */
    BTEFAIL_PROT,			/* Protection violation */
    BTEFAIL_ACCESS,			/* access error */
    BTEFAIL_TOUT,			/* Time out */
    BTEFAIL_ERROR,			/* Generic error */
    BTEFAIL_DIR				/* Diretory error */
} bte_result_t;

/* 
 * The following macros may be used to check the correctness of
 * the parameters passed to the bte routines.  None of the routines
 * perform any validity checking on their parameters, so if you
 * aren't sure that the parameters are correctly aligned you 
 * need to call one or more of the following.
 */
 
#define BTE_VALID_SRC(_src)	!((_src) & BTE_SRC_ALIGN)
#define BTE_VALID_DEST(_dst)	!((_dst) & BTE_DEST_ALIGN)
#define BTE_VALID_LEN(_len)	( ((_len) >= BTE_LEN_MINSIZE) && \
				  !((_len) & BTE_LEN_ALIGN) )

#define BTE_CHECKPARMS(_src, _dst, _len) \
	( BTE_VALID_SRC(_src) && BTE_VALID_DEST(_dst) && \
	  BTE_VALID_LEN(_len) )


/*
 * Mode flags for the various BTE routines.  The values for these flags
 * are not arbitrary; the least significant 4 bits actually encode the
 * hardware modes for the SN0 Hub ASIC.  For this reason, please don't
 * change the flag definitions unless you really know what you're doing.
 */

#if defined(SN0)
#  define BTE_NORMAL	IBCT_NOTIFY	
#  define BTE_POISON	IBCT_POISON
#  define BTE_ZFILL	IBCT_NOTIFY|IBCT_ZFIL_MODE
#  define BTE_ASYNC_BIT	0x1000
#  define BTE_ASYNC	(BTE_ASYNC_BIT|BTE_NORMAL)
#endif

#define BTERNDVZ_SPIN	0x1
#define BTERNDVZ_SLEEP	0x2
#define BTERNDVZ_POLL	0x4

#define BTE_IS_ASYNC(_x)	((_x) & BTE_ASYNC_BIT)
#define BTE_HW_MODE_MASK	0x1ff

/*
 * Maximum number of retries done to complete a transfer before
 * giving up, and killing the system.
 */
#define	BTE_RETRY_COUNT_MAX	16

/* 
 * Declarations for the high-level routines
 */

extern int bte_pbcopy(paddr_t src, paddr_t dest, unsigned len);
extern int bte_pbzero(paddr_t dest, unsigned length, int nofault);


/*
 * btecontext_t is a structure which is placed at the beginning of a
 * cache line by the bte_getcontext routine.  The BTE routines which 
 * take a btecontext_t* as an argument assume that the btecontext_t
 * was allocated by the bte_getcontext routine.  Because the alignment
 * and size of the memory in which the btecontext structure is placed 
 * is extremely important, users should NOT allocate btecontext structures
 * on the stack.  Use bte_getcontext!
 */
typedef struct bte_handle_s {
        union _btecontext 	*bh_ctx; /* context info */
	struct bteinfo_s	*bh_bte; /* bte info */
	short			bh_error; /* error (crb "ecode") */
} bte_handle_t;
    
typedef union _btecontext {
	union _btecontext* 	 next;
	volatile hubreg_t 	 status; /* updated by bte */
} btecontext_t;

typedef struct bteinfo_s {
	caddr_t         bte_base;	/* Base addr of my BTE's status reg */
	int		bte_num;	/* 0 --> BTE0, 1 --> BTE1	    */
	void		*bte_cookie;	/* Cooked from rt_pin_thread	    */
	paddr_t		bte_zpage;	/* BTE Page used for Page zero 	    */
	btecontext_t	*bte_context;	/* Phys addr of default context     */
	bte_handle_t	bte_bh;		/* passed to user (FOR NOW) 	    */
	btecontext_t	*bte_ctxcache;	/* Cache of free contexts 	    */
	paddr_t         bte_zeroline;	/* Phys addr for zeroed cache line  */
	paddr_t		bte_notif_src;	/* Source addr used for 1 line xfer */
	paddr_t		bte_notif_targ;	/* Target addr used for 1 line xfer */
	paddr_t		bte_poison_target; /* Used as target for poison xfer*/
	mrlock_t        bte_lock;	/* Guards access to bte registers   */
	int		bte_intr_count; /* Number of bte interrupts 	    */
	int		bte_xfer_count; /* Number of transfers done via BTE */
	int		bte_retry_count; /* NUmber of retries done for xfer */
	int		bte_retry_stat; /* BTE xfer Status before retrying  */
	ushort		bte_freectxt;	/* Count of free contexts 	    */
	ushort		bte_intrbit;	/* Interrup bit number		    */
	cnodeid_t	bte_cnode;	/* Node number of this BTE 	    */
	cpuid_t		bte_cpuid;	/* CPU to which this BTE belongs to */
	uchar_t		bte_thread_pinned; /* 1 if thread has been pinned   */
	uchar_t		bte_enabled;	/* 1 if this bte is enabled	    */
	uchar_t		bte_bzero_mode;	/* Mode for doing Bzero 	    */
	uchar_t		bte_war_inprogress; /* BTE WAR in progress	    */
	sema_t		bte_war_sema;	/* sleep waiting for completion     */
	int		bte_war_waiting; /* # waiting on war_sema           */
} bteinfo_t;

/* BTE availablility */

extern int bte_avail(void);

/* Allocate a BTE context for asynchronous transfers */
extern btecontext_t* bte_getcontext(void);
extern void bte_freecontext(btecontext_t* ctx);

/* Perform basic block transfer and replication operations */
extern int bte_copy(bte_handle_t *, paddr_t src, paddr_t dest, 
		    unsigned len, unsigned mode);
extern int bte_fill(paddr_t dest, unsigned len, unsigned long value, 
		    unsigned mode, btecontext_t* ctx);

/* Rendezvous with a previously initiated asynchronous transfer */
/* Not implemented */
extern int bte_rendezvous(bte_handle_t* h, unsigned mode);

/* Page migration support */
extern bte_handle_t *bte_acquire(int);
extern void bte_release(bte_handle_t *);
extern int bte_poison_copy(bte_handle_t *, paddr_t src, paddr_t dest, 
			   unsigned len, unsigned mode);
extern int bte_poison_range(paddr_t src, unsigned);

/* Error support */

extern void bte_crb_error_handler(vertex_hdl_t, int, int, ioerror_t *); 
extern void bte_disable(void);
extern void bte_stall_node(cnodeid_t);
extern int  bte_bpush_war(cnodeid_t, void *);

#endif /* __SYS_SN_SN0_BTE_H__ */ 
