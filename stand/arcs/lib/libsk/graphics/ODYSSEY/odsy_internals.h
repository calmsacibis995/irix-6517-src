#ifndef __ODSY_INTERNALS_H__
#define __ODSY_INTERNALS_H__
/*
** Copyright 1999 Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
*/
#include <sys/errno.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <ksys/ddmap.h>
#include <ksys/as.h>
#include <sys/gfx.h>
#include <sys/rrm.h>
#include <sys/gfx_flow.h>
#include <sys/odsy.h>
#include <sys/ksynch.h>
#include <sys/ddc_i2c.h>

#ifndef _STANDALONE
#include <sys/htport.h>
#include "sys/fpanel.h"
#endif	

#include "odsy_flow.h"
#include "odsy_adt.h"
#include "odsy_sim.h"
#include "odsy_sdram.h"

#include <ODYSSEY/gl/buzz.h>

#ifdef ODSY_OWNPRINTF
#define cmn_err odsy_cmn_err
#define printf  odsy_printf
void odsy_cmn_err(int level, char *fmt, ...);
void odsy_printf(char *fmt, ...);
#endif

#ifdef ODSY_DALLOC
#define kmem_alloc odsybug_kmem_alloc
#define kmem_zalloc odsybug_kmem_zalloc
#define kmem_free odsybug_kmem_free
void    *odsybug_kmem_alloc(size_t, int);
void    *odsybug_kmem_zalloc(size_t, int);
void    odsybug_kmem_free(void *, size_t);
#endif

#define ODSY_DEBUG_KMEM 1

#ifdef ODSY_DEBUG_KMEM
#define KMEM_FREE(ptr,size,poison)  { int i; for (i=0;i<size;i++) { *(((char*)ptr)+i) = (char)poison;} kmem_free(ptr,size); }
#else
#define KMEM_FREE(ptr,size,poison)  { kmem_free(ptr,size); }
#endif

#ifdef _STANDALONE
#define ODSY_MAXBOARDS	1
#else
#define ODSY_MAXBOARDS	2
#endif
#define NR_ODSY_DDRNS	127

#define ODSY_BRDMGR_HW_CONTEXT_ID	0
#define ODSY_BOGUS_HW_CONTEXT_ID	127


#define ODSY_HW_WR_RGN_OFFSET_ADDR_BITS 15

#define ODSY_HW_WR_RGN_BYTES ((1<<ODSY_HW_WR_RGN_OFFSET_ADDR_BITS)*sizeof(odsy_mm_hw_wr_slot))

#define ODSY_GLOBAL_RGN_BYTES (3 * NBPC)

/* Bits for mapping stuff */
#define GOOD_PGBITS    ((unsigned int) (PG_G | PG_M | PG_SV | PG_VR))

#define ODSY_HW_SHADOW_CACHE_PAGES ((ODSY_HW_WR_RGN_BYTES)/_PAGESZ)

struct odsy_dma_rgn; 
struct odsy_drawable;
struct odsy_ddrnode;
typedef __BUZZpackets odsy_sw_shadow;

typedef struct buzz_config_flags {
    unsigned master_buzz_id	:2;	/* board number of master buzz */
    unsigned other_buzz_id	:2;	/* board number of other buzz
					   (only relevant for multi-buzz) */
    unsigned is_master		:1;	/* is this board master? */
    unsigned has_opt_board	:1;	/* option board attached? */
} buzz_config_flags;

typedef struct odsy_retrace_entry {
	
	struct odsy_ddrnode    *ddrnp;
	unsigned short		sync_val;
	__uint32_t		swaptime;

	__uint32_t              swapgroup;

	struct pane	       *pane;
	unsigned int		xwinid;

	unsigned int sync_waited;

} odsy_retrace_entry;


#define ODSY_SLEEPING_RETRACE_LOCK 1
/* can't use spinlocks with simulators */
#ifdef ODSY_SIM_KERN
   #undef ODSY_SLEEPING_RETRACE_LOCK
   #define ODSY_SLEEPING_RETRACE_LOCK 1
#endif

typedef struct odsy_retrace {

#if ODSY_SLEEPING_RETRACE_LOCK
	mutex_t retrace_lock; /* use a sleep lock in simulation */
#else
	lock_t	retrace_lock; /* use a spin lock normally */
#endif
	sv_t	retrace_sv;

	__uint32_t mainbuf;	/* what swapb_sel_HW will be after vert retrace,
				 * based on most-recent writes to swapb_tag_set.
				 */

	__uint32_t swapready;

	__uint32_t waiting_for_sync;
	__uint32_t waiting_for_retrace;
	__uint32_t waiting_for_group;
	__uint32_t waiting_for_swap;
	__uint32_t in_use;

	__uint32_t newbuf;
	__uint32_t decouplebuf;

        int head_wid; /* first wid in list */
	struct odsy_retrace_entry  entries[ODSY_NUM_DISPLAYMODES_MAIN];

	struct rrm_rnode *vrlist;  /* RNs waiting to sync with next retrace */

} odsy_retrace;

/*
 * odsy_i2cinfo structure information
 *
 * There are three possible displays with EDID on Odyssey:
 *   1) The main analog dsiplay
 *   2) The DCD2 Board Channel 0
 *   3) The DCD2 Board Channel 1
 *
 * There are three possible values for each status (*_edid_stat)
 * field in this structure:
 *
 *   1) MONITOR_EDID_UNKNOWN
 *   2) MONITOR_EDID_ERR
 *   3) MONITOR_EDID_OK
 *
 * Status (*_edid_stat) variables should be initialized as 
 * MONITOR_EDID_UNKNOWN.
 *
 * DDC2B protocol probes should occur during earlyinit.  Most
 * monitors should respond appropriately at that time.  
 * Any successful EDID read for any particular channel should cause 
 * that channel's status variable to OK.
 *
 * Certain displays may require additional EDID read attempts later in
 * the boot process.  Each read attempt should increment a particular
 * channel's status variable until it reaches the same value as the ERR 
 * define.  At that point, that channel's display is considered to have
 * no EDID capabilities (or perhaps that channel does not have a display
 * connected).
 *
 */

typedef struct _odsy_i2cinfo {
  /* the following are for the main PBJ display */
  unsigned int main_edid_stat;	/* DDC2B (EDID) status */
  unsigned int main_edid_len;   /* Num EDID blocks for Main EDID */
  ddc_edid_t *main_edid_ptr;	/* Main EDID data pointer */
  /* the following are for the DCD2 (option) channel 0 display */
  unsigned int opt0_edid_stat;	/* DDC2B (EDID) status */
  unsigned int opt0_edid_len;   /* Num EDID blocks for Option 0 EDID */
  ddc_edid_t *opt0_edid_ptr;	/* Option 0 EDID data pointer */
  /* the following are for the DCD2 (option) port channel 1 display */
  unsigned int opt1_edid_stat;	/* DDC2B (EDID) status */
  unsigned int opt1_edid_len;   /* Num EDID blocks for Option 1 EDID */
  ddc_edid_t *opt1_edid_ptr;	/* Option 1 EDID data pointer */
  unsigned short cksum;
  unsigned int ddc2Bstate;
  unsigned short validHdr;
} odsy_i2cinfo_t;

/*
 * main/option edid_stat defines.
 */
#define MONITOR_EDID_UNKNOWN  0
#define MONITOR_EDID_ERR      5  /* May change for more EDID probe retries */
#define MONITOR_EDID_OK       255


/*
 * DMA operation tracking state.
 */

/*
** single line of valid dma data
*  for range checking
*/
typedef struct odsy_dma_one_range
{
	__int64_t phys;
	size_t	len;
} odsy_dma_one_range;

/*
** dma region/op
*/
typedef struct odsy_dma_rgn
{
#ifdef ODSY_SIM_KERN
	odsy_dma_one_range * range_check_tbl;
	int		range_check_tbl_size;
#endif
	void           *xlat_tbl;
	int             xlat_tbl_size;
	struct gfx_gfx *gfxp;
	struct odsy_ddrnode *gctx;
	void           *base;
	size_t          len;
	int             is_user_rgn;
	int             is_pinned;
	odsy_dma_param  dma_param;
} odsy_dma_rgn;


typedef struct odsy_dma_op
{
#ifdef ODSY_SIM_KERN
	odsy_dma_one_range * range_check_tbl;
	int		range_check_tbl_size;
#endif
	int             pending;
	void           *xlat_tbl;
	int             xlat_tbl_size;
	
	mutex_t         lock;
	sv_t            sv;

	// If it is a user-land address region 
	// we need these to track it for unlocking...
	struct gfx_gfx *gfxp;
	struct odsy_ddrnode *gctx;
	int             rgn_hdl;

	void           *base;
	size_t          len;

	odsy_dma_marker op_nr; 
} odsy_dma_op;




#define ODSY_DMA_ASYNC_MODULUS       (16)  /* mod for the async dma counter */
#define ODSY_DMA_SYNC_MODULUS        (256) /* mod for the sw sync write-back payload */

/* we halve these to deal with low >  high */
#define ODSY_MAX_OUTSTANDING_SYNC_DMA  (ODSY_DMA_SYNC_MODULUS/2)  
#define ODSY_MAX_OUTSTANDING_ASYNC_DMA (ODSY_DMA_ASYNC_MODULUS/2)


#define ODSY_DMA_WAIT_ANY  0
#define ODSY_DMA_WAIT_OP   1

typedef struct odsy_dma_ops 
{
	mutex_t lock;
	sv_t    sv;


	volatile odsy_mm_hw_wr_slot *sync_hw_wr;        // sw sync value written by hw
	int sync_hw_wr_slot;                            // sw sync slot id
	int sync_writep;                                // index into sync_ops
	int sync_readp;                                 //   ditto... this is a queue.
	odsy_dma_op *sync_ops	[ODSY_MAX_OUTSTANDING_SYNC_DMA];
	__uint64_t sync_op_nr_low,sync_op_nr_high;      // userland marker ids


	volatile odsy_mm_hw_wr_slot *async_hw_wr;
	int async_hw_wr_slot;   // sw sync slot id
	int async_writep;
	int async_readp;
	odsy_dma_op *async_ops[ODSY_MAX_OUTSTANDING_SYNC_DMA];
	__uint64_t async_op_nr_low,async_op_nr_high;


} odsy_dma_ops;


/* i2c related */
#define ODSY_I2C_MAIN_PORT    0
#define ODSY_I2C_OPT_PORT     1
#define ODSY_I2C_ACK_NULL     0
#define ODSY_I2C_ACK_ASSERT   1
#define ODSY_I2C_STOP_NULL    0
#define ODSY_I2C_STOP_ASSERT  1

#define ODSY_I2C_RESET_TOGGLE 3

struct i2c_port {
	pbj_I2C_control_reg *control_reg;	/* points into sysreg_shadow */
	pbj_I2C_status_reg  *status_reg;	/* points into sysreg_shadow */
	struct odsy_data    *bdata;
	unsigned short addr_control_reg;
	unsigned short addr_status_reg;
	int id;
};
typedef struct i2c_port i2c_port_t;
typedef struct i2c_port port_handle;

/*XXX fake value, need to find real loop limit */
#define LOOP_LIMIT 500	/* each loop 50 micro-second ==> 25 milli-sec total */
#define ODSY_I2C_SUCCESS 0
#define ODSY_I2C_FAILURE 1
#define ODSY_I2C_TIMEOUT 2
#define ODSY_I2C_BUFFER_OVFLW 3

#if !defined(_STANDALONE)
#define ODSY_I2C_GET_LOCK(port) \
	((port_handle*)port)->id ? mutex_lock(&((port_handle*)port)->bdata->i2c_lock_opt,PZERO) : \
                                   mutex_lock(&((port_handle*)port)->bdata->i2c_lock_main,PZERO)
#define ODSY_I2C_RELEASE_LOCK(port) \
	((port_handle*)port)->id ? mutex_lock(&((port_handle*)port)->bdata->i2c_lock_opt,PZERO) : \
                                   mutex_lock(&((port_handle*)port)->bdata->i2c_lock_main,PZERO)
#else
#define ODSY_I2C_GET_LOCK(port)
#define ODSY_I2C_RELEASE_LOCK(port)
#endif

#define DELAY_WHILE_XFER_NOT_DONE 500


#define I2C_CTRL_SEL(port) \
        port ? PBJ_I2C_opt_control : PBJ_I2C_main_control
#define I2C_STAT_SEL(port) \
        port ? PBJ_I2C_opt_status : PBJ_I2C_main_status

#define I2C_SLAVE       0
#define I2C_MASTER      1

#define MAX_I2C_MSGLEN 256
#define MAX_START_ATTEMPTS  2
#define MAX_READ_EDID_ATTEMPTS  2

#define ODSY_PLL_SUCCESS 0
#define ODSY_PLL_FAILURE 1
#define ODSY_PLL_ADDR 0x26
#define ODSY_AMI_PLL_ADDR 0x58
#define ODSY_I2C_WRITE 0
#define ODSY_I2C_READ 1
#define ODSY_PLL_LOCKED 0x04
#define ODSY_DPA_LOCKED 0x01
#define ODSY_PLL_SOFT_RESET 0x0A
#define ODSY_DPA_SOFT_RESET 0x50
#define ODSY_PLL_InputControl_ADDR  0x00
#define ODSY_PLL_LoopControl_ADDR   0x01
#define ODSY_PLL_FdBkDiv0_ADDR      0x02
#define ODSY_PLL_FdBkDiv1_ADDR      0x03
#define ODSY_PLL_DPAOffset_ADDR     0x04
#define ODSY_PLL_DPAControl_ADDR    0x05
#define ODSY_PLL_OutputEnables_ADDR 0x06
#define ODSY_PLL_OscDiv_ADDR	    0x07
#define ODSY_PLL_SoftReset_ADDR     0x08
#define ODSY_PLL_StatusReg_ADDR     0x0C
#define ODY_AMI_PLL_0x0_ADDR        0x00 
#define ODY_AMI_PLL_0x1_ADDR        0x01
#define ODY_AMI_PLL_0x2_ADDR        0x02
#define ODY_AMI_PLL_0x3_ADDR        0x03
#define ODY_AMI_PLL_0x4_ADDR        0x04
#define ODY_AMI_PLL_0x5_ADDR        0x05
#define ODY_AMI_PLL_0x6_ADDR        0x06
#define ODY_AMI_PLL_0x7_ADDR        0x07

#define ODSY_DDC_STATE_GOOD 2
#define HOST2ABADDR 0x50

#define ODSY_GENLOCK_SOURCE_INTERNAL -1
#define ODSY_GENLOCK_SOURCE_EXTERNAL -2
#define ODSY_GENLOCK_ENABLED (0x00000040)
#define ODSY_GENLOCK_LOCK_STATE (0x00000002)
#define ODSY_GENLOCK_LOCK_SOURCE (0x00000001)

#define ID_REPLY_N2_17          "BV1.0   SONY    GDM"
#define ID_REPLY_N2_20          "BV2.06  SONY    GDM"
#define ID_REPLY_24             "BV1.4   SONY    GDM"
                                        /* sizeof includes the trailing null */
#define ID_REPLY_SIZE_N2        (sizeof(ID_REPLY_N2_17) - 1)

#define NUM_N2_MONITORS         3




/*
 * Per registered device.
 */
typedef struct odsy_data {
	struct gfx_data    gfx_data;           	/* gfx di part of the data                     */
	odsy_info	   info;               	/* gf_info+dd_info ,what GFX_GETBOARDINFO gets */

	/* some items used in finding board info, initting board, etc. */
	short		    board_found;	/* -1 (not probed), 0 (not found) , or 1 (found) */
	short		    info_found;
	short		    board_initted;
	short		    boardnr;		/* our index in the data/board array           */
	int		    widget_id;		/* xbow port # */
	short		    board_started;

	gfx_flow_hdl_t      flow_hdl;           /* flow ctl hdl */

	int		    abnormal_teardown;	/* used to be Xabnormalexit in mgras-land      */

	int		    last_opengl_hw_ctx_id;	/* save some ctxsw work by identifying Xserver only swaps */

	odsy_hw		    *hw;		/* if sim, this == head nbr+1 */

	odsy_mm		    mm;			/* sdram mem mgr anchor                        */

	odsy_dma_ops        dma;                /* dma ops anchor                              */

	sema_t		    init_ddrncount_sleeper; /* for dealing with cleanup in gf_OdsyInitialize */
	int		    nropen_ddrn;	    /*                     "   "                     */

	sema_t		    teardown_sema;

	odsy_retrace	    retrace;

	sema_t		    hw_ctx_id_sema;

#if !defined( _STANDALONE )
	void *              hw_shadow_cache_pages[ODSY_HW_SHADOW_CACHE_PAGES];
	int                 hw_shadow_cache_page_in_use[ODSY_HW_SHADOW_CACHE_PAGES];
#endif

	sema_t		    hw_wr_slot_sema;	/* for allocating sw sync offsets */
	odsy_32k_slot_mask  hw_wr_slots;

	odsy_i2cinfo_t      i2cinfo;
	mutex_t             i2c_lock_main;	/* lock for I2C users on main port */
	mutex_t             i2c_lock_opt;	/* lock for I2C users on opt port*/

	sema_t		    dfifo_sema;
	
	lock_t              mouse_pos_lock;
	__uint64_t	    mouse_pos_cmd;

	/*
	** regions which are visible to attached clients
	*/

	int		     num_ucode_lines;
	void                *kv_ucode_rgn;
	
	int                  pf_rgn_size;	/* #bytes */
	__uint32_t	    *kv_pf_rgn;
	__uint32_t	    *uv_pf_rgn;
	ddv_handle_t         pf_rgn_ddv;
	mutex_t              pfstat_lock;

	odsy_sysreg_shadow  sysreg_shadow;

	/* the set of drawables that need sdram allocation */
	struct odsy_hash_table	*drawables;
	mrlock_t		drawable_lock;

	/* loaded display modes, used by rrm code and gfP_Odsy code */
	unsigned int loadedmodes[ODSY_NUM_DISPLAYMODES];

	/* Buzz configuration flags */
	buzz_config_flags	buzz_config;

	/* a software shadow to use with buzz.h macros */
	odsy_sw_shadow		working_sw_shadow;

#ifdef ODSY_SIM_KERN
	struct _sim {
		/*
		** most of this is for dealing with pio/dma 
		** delivery wrt this board...
		*/
		lock_t               pio_buffer_lock;
		sema_t               pio_host_serialize_sema;
		sv_t		     pio_wait_for_host_write_sv;
		sv_t		     pio_wait_for_symb_event_sv;
		volatile int  	     pio_writep;
		volatile int         pio_readp;
		volatile int 	     pio_count;
		volatile odsy_sim_rdwr pio_buffer[ODSY_SIM_MAX_DELAYED_PIOS];
		int		     pio_uthreads;
		struct gfx_gfx      *pio_symbiote_gfxp;
		struct gfx_gfx      *dma_symbiote_gfxp;
		struct odsy_dma_one_range *dma_range_check_rgn;
		int                  dma_range_check_lookup_cache;
		sema_t               ctxsw_poll_sema;
	} sim;
#endif /* ODSY_SIM_KERN */

#if !defined(_STANDALONE)

	struct {
		struct xtalk_intr_s *xt_intr,
				    *flag_intr,
				    *dma_intr,
				    *vtrace_intr,
				    *ssync_intr;
		
		short xt_intr_vector_num,
			flag_intr_vector_num,
			dma_intr_vector_num,
			vtrace_intr_vector_num,
			ssync_intr_vector_num;
		
		iopaddr_t xt_intr_dst_addr,
			intr_ctl;
#ifdef ODSY_DEBUG_INTR
#define ODSY_MAX_INTR_TRACE_RECS 128
		mutex_t trace_lock;
		int     trace_rec_nr;
		struct {
			unsigned long	time;	/* in nanoseconds */
			unsigned long	data;	/* interrupt status */
			char           *str;    /* had better be literal */
			
		} trace_recs[ODSY_MAX_INTR_TRACE_RECS];
#endif
	} intr;
#endif	/* !_STANDALONE */

	unsigned short cursor_x,
		       cursor_y,
		       cursor_xhot,
		       cursor_yhot;

#if !defined(_STANDALONE)
	struct _diag {

		/* for support of a scan tool */
		mutex_t scanbuf_lock;			/* for exclusive access to the following */
		caddr_t scanbuf_kaddr;			/* physically contiguous buffer for scan data */
		caddr_t scanbuf_uvaddr;
		ddv_handle_t scanbuf_ddv;		/* ddv handle for scan buffer */
		int scanbuf_size;			/* size of buffer in bytes */
		struct gfx_gfx *scanbuf_owner;		/* scanbuf owner */

		mutex_t hw_shadow_ptrs_lock;		                   /* for exclusive access to the following */
		struct gfx_gfx *hw_shadow_ptrs_owner;	
		ddv_handle_t hw_shadow_ddv[ODSY_HW_SHADOW_CACHE_PAGES];	   
		caddr_t uv_hw_shadow_ptrs[ODSY_HW_SHADOW_CACHE_PAGES];
		ddv_handle_t hw_wr_rgn_ddv;
		

	} diag;
#endif	/* !_STANDALONE */

	__uint32_t vtg_ctl;	/* Shadow of select PB&J registers */
	__uint32_t vtg_enable;
	__uint32_t vtg_initialState;

	struct i2c_port i2c_port[2];

	vertex_hdl_t dev;	/* obtained at odsy_attach time */
	lock_t hiwaterlock;
	toid_t ftcallid;
	int sbit_ecc_cnt;	/* for debugging ECC errors */

	int debug_hw_wr_slot;   /* sw sync slot for debuggin' use */
	volatile odsy_mm_hw_wr_slot *debug_hw_wr;
} odsy_data;

extern odsy_data OdsyBoards[];
extern int Odsy_is_multi_buzz;
extern odsy_brdmgr_timing_info odsy_default_timing1;
extern odsy_brdmgr_timing_info *default_timing, *current_timing;

/*
** structs related to ValidateClip and drawable support
*/

typedef struct odsy_clip_smask {
	__uint32_t min_xy;
	__uint32_t max_xy;
} odsy_clip_smask;

typedef union _odsy_clip_wid_word {
	__uint32_t w32;
	struct {
#if __FIELD_ORDER_MSB_FIRST
	    unsigned _pad  :15,
	            enable :1,
	              mask :8,
	             value :8;
#else
	    unsigned value :8,
	              mask :8,
	            enable :1,
	              _pad :15;
#endif
	} bf;
} odsy_clip_wid_word;

typedef struct _odsy_clip {
	odsy_clip_wid_word wid_word;
	odsy_clip_smask smask[4];
	__uint32_t wind_off_x;
	__uint32_t wind_off_y;
} odsy_clip;


/*
 * Drawable stuff
 */
#define ODSY_DRAW_HDL_VALID		1	/* has valid id */
#define ODSY_DRAW_HDL_BOUND		2	/* drawable bound in hw context */
#define ODSY_DRAW_HDL_HASHED		4	/* has hash entry in drawable table */

#define ODSY_DRAW_HDL_INIT(hdl)	\
	{ (hdl).flags = 0; (hdl).id = 0; }
#define ODSY_DRAW_HDL_SET_NEW_ID(hdl, xid) 				\
{									\
 	(hdl).id = (xid);						\
  	(hdl).flags |= ODSY_DRAW_HDL_VALID;				\
	(hdl).flags &= ~(ODSY_DRAW_HDL_BOUND | ODSY_DRAW_HDL_HASHED);	\
}
#define ODSY_DRAW_HDL_IS_BOUND(hdl) \
    (((hdl).flags & ODSY_DRAW_HDL_VALID) && ((hdl).flags & ODSY_DRAW_HDL_BOUND))
#define ODSY_DRAW_HDL_NEEDS_BINDING(hdl) \
    (((hdl).flags & ODSY_DRAW_HDL_VALID) && !((hdl).flags & ODSY_DRAW_HDL_BOUND))


#define	ODSY_DRAW_HASH_SIZE	13

#define ODSY_DRAW_DEALLOCATED	1
#define ODSY_DRAW_PBUF		2

#define ODSY_DRAW_HASH(x) ((((x) >> 16) | ((x) & 0xffff)) % ODSY_DRAW_HASH_SIZE)

typedef struct odsy_drawable {
    odsy_hash_elem	head;
    odsy_mm_obj_grp	*mmgrp;
    __uint32_t          level_mask;     /* which levels exist for the pbuffer */
    int			refcnt;		/* reference count */
    int			flags;		/* see above */
} odsy_drawable;



/* which drawable */
typedef enum { odsyDrawPrim = 1, odsyDrawRead = 2 } OdsyDrawType;

typedef struct odsy_draw_hdl {
    	int	flags;		/* defined above */
    	int	id;		/* x window id */
	odsy_ctxmgr_drawable_cfg cfg; /* stuff the gl client needs */
} odsy_draw_hdl;


#define ODSY_HW_SHADOW_VALID         0x1
#define ODSY_HW_SHADOW_IS_CACHE_PAGE 0x2


typedef struct odsy_ddrnode {
	int                   hw_ctx_id;

	odsy_clip             clip;
	__uint32_t            dma_regs[8]; 

	odsy_sw_shadow       *kv_sw_shadow;
	odsy_sw_shadow       *uv_sw_shadow;

	odsy_hw_shadow       *hw_shadow;    /* kv_hw_shadow */
	uvaddr_t              uv_hw_shadow;
	ddv_handle_t          hw_shadow_ddv;
	int                   hw_shadow_flags; /* bits for keeping track of various things */

	struct rrm_rnode     *rnp;

	odsy_mm_sg           *sg;             /* pointer to sg the context is involved in    */
	int                   ctx_hdl;        /* element id within the sharegroup            */

	int sw_syncid;

	__uint64_t	      last_sync_xfr_num; /* for flushing any pending dmas */
	__uint64_t	      last_async_xfr_num;

	odsy_chunked_set      coresid_range_set;  // coresid ranges ctx has created
	odsy_chunked_set      coresid_rgn_set;    // each element is  * odsy_mm_coresid_rgns

	odsy_chunked_set      ibfr_set;           // set of scratch buffer objects this context has created.  

	int                   unlocked_regions;
	odsy_chunked_set      dma_rgn_set;    /* dma regions this gctx has (locked) */

	odsy_draw_hdl	      cur_draw;		/* current write drawable */	
	odsy_draw_hdl	      cur_read;		/* current read drawable */

	int                   has_made_current; /* ever? */
	int 		      is_new_pcx;	/* is the pcx new? */
	__uint32_t            cur_drawbuf;      /* virtual FRONT/BACK/LEFT/RIGHT bits */

	int                   syncid_for_swap;
	int                   syncid_for_finish;
	int		      decouple_swapbuf_from_vr;

} odsy_ddrnode;
typedef odsy_ddrnode odsy_gctx;    /* just for kicks */


/* play the same trick rrm does with its rns
 * so we know we always have enough.
 */
extern odsy_ddrnode ODSYddrn[]; 
extern int rrm_maxrn;  /* that's over _all_ boards, right? */

/* dispatch table for gfx fcns */
extern struct gfx_fncs odsy_gfx_fncs;

/* dispatch table for textport fncs */
extern struct htp_fncs odsy_htp_fncs;



/*
**  random macros 
*/

#define IS_MAPPED(gfxp)       ((gfxp)->gx_flags & GFX_MAPPED)
#define IS_DIAG(gfxp)         ((gfxp)->gx_flags & GFX_DIAG)
#define OWNS_GFXSEMA(gfxp)    ((gfxp)->gx_bdata->gfxsema_owner == (gfxp))
#define CUR_GTHD()            (gthread_find(curthreadhandle()))
#define IS_BRDMGR_GFXP(gfxp)  ((gfxp)->gx_board->gb_manager == (gfxp))
#define IS_BRDMGR_GCTX(gctx)  (gctx->hw_ctx_id == ODSY_BRDMGR_HW_CONTEXT_ID)
#define HAS_MM_NOTIFIED(gctx) ((gctx)->kv_sw_wr_rgn != 0)

#if !defined(ODSY_SIM_KERN)
   #define ODSY_CTXSW_TIMEOUT_INTERVAL (15*HZ)
   #define ODSY_SWAPSYNC_TIMEOUT_INTERVAL (15*HZ)
   #define ODSY_FINISHRN_TIMEOUT_INTERVAL (15*HZ)
   #define ODSY_GLGET_TIMEOUT_INTERVAL (15*HZ)
   #define ODSY_DMAWAIT_TIMEOUT_INTERVAL (15*HZ)
#else
   #define ODSY_CTXSW_TIMEOUT_INTERVAL (180*HZ)
   #define ODSY_SWAPSYNC_TIMEOUT_INTERVAL (180*HZ)
   #define ODSY_FINISHRN_TIMEOUT_INTERVAL (180*HZ)
   #define ODSY_GLGET_TIMEOUT_INTERVAL (180*HZ)
   #define ODSY_DMAWAIT_TIMEOUT_INTERVAL (180*HZ)
#endif

/* 
** !!! USE THESE FOR ALL PIPE ACCESSES !!!
*/ 
#define ODSY_WRITE_HW(base,field,val) ODSY_writePipePIO(base,field,val)
#define ODSY_READ_HW(base,field,var)  ODSY_readPipePIO(base,field,var)

/*
** !!! USE THESE FOR ALL "META" TOKENS TO THE SIMULATOR CHAIN !!!
*/
#if defined(ODSY_SIM_KERN)
#define ODSY_WRITE_HW_META(base,metatoken,val)  __odsy_sim_hw_touch_rc[((__uint64_t)base)-1] |= \
                                                  __odsy_sim_hw_touch_rc[((__uint64_t)base)-1]?0: \
                                                         odsySIMwritePipePIO((__uint32_t)(__uint64_t)hw, \
								   metatoken,                   \
								   ODSY_SIM_META_TOKEN_WIDTH,   \
								   val,0/*NULL*/)

#define ODSY_READ_HW_META(base,metatoken,var)  __odsy_sim_hw_touch_rc[((__uint64_t)base)-1] |= \
                                                  __odsy_sim_hw_touch_rc[((__uint64_t)base)-1]?0: \
                                                         odsySIMreadPipePIO((__uint32_t)(__uint64_t)hw,  \
							      metatoken,                       \
							      ODSY_SIM_META_TOKEN_WIDTH,       \
							      (void*)&(var))


#define ODSY_WRITE_HW_STR(base,string,is_directive) __odsy_sim_hw_touch_rc[((__uint64_t)base)-1] |= \
                                                         odsySIMwritePipeString((__uint32_t)(__uint64_t)base,string,is_directive)

#else
#define ODSY_WRITE_HW_META(b,m,v)
#define ODSY_READ_HW_META(b,m,v)
#define ODSY_WRITE_HW_STR(b,s,d)
#endif

/*
** To allow using shadow grokkin' macros in buzz.h ...
*/
#define ODY_STORE_ui(datum)	ODSY_WRITE_HW(hw,cfifo_priv.by_32.w0,datum)
#define ODY_STORE_ui64(datum)   ODSY_WRITE_HW(hw,cfifo_priv.by_64,datum)
#define ODY_STORE_i(datum)	ODSY_WRITE_HW(hw,cfifo_priv.by_32.w0,datum)
#define ODY_STORE_f(datum)	ODSY_WRITE_HW(hw,cfifo_priv.by_32.w0,*(__uint32_t*)(&(datum)))

#define ODY_STORE_ui_DW(datum1,datum2)	ODSY_WRITE_HW(hw,cfifo_priv.by_64,ODSY_DW_CFIFO_PAYLOAD(datum1,datum2))

/*
 * for the convenience of i2c routines
 * these macros read/write registers by addr rather than by name
 *
 * dbe_diag_rd_pad is an unsigned int array immediately following odsy_dbe_diag_rd
 * in the odsy_hw struct.
 * 32-bit dbe registers are spaced 8 bytes apart.
 */
#define ODSY_WRITE_I2C_CONTROL(port) \
	ODSY_WRITE_HW((port)->bdata->hw, dbe, \
		      DBE_CMD1_DBEADDR(DBE_UPDATE_IMMED, \
		      (port)->addr_control_reg, (port)->control_reg->w32))
#define ODSY_READ_I2C_CONTROL(port) \
	ODSY_READ_HW((port)->bdata->hw, \
		     dbe_diag_rd_pad[-sizeof(odsy_dbe_diag_rd)/sizeof(unsigned int) \
				     +2*(port)->addr_control_reg], \
		     *(unsigned int *)&(port)->control_reg->w32);
#define ODSY_READ_I2C_STATUS(port) \
	ODSY_READ_HW((port)->bdata->hw, \
		     dbe_diag_rd_pad[-sizeof(odsy_dbe_diag_rd)/sizeof(unsigned int) \
				     +2*(port)->addr_status_reg], \
		     *(unsigned int *)&(port)->status_reg->w32);

#define ODSY_ALIGN_DOUBLE()   ODSY_FLUSH_WG()
#define ODSY_FLUSH_WG()	     __synchronize() /*a SYNC instruction flushes the write gatherer */
#define ODSY_PIPE_FLUSH()     ODSY_FLUSH_WG()

/*
** keep us from cluttering up the place with the casting 
** necessary to push down a shadowed system register.  
*/
#define UINT32(sysregfield) (*(__uint32_t*)(&(sysregfield)))
#define PUSH_SHADOWED_SYSREG(bdata, regname) \
	ODSY_WRITE_HW((bdata->hw), sys.regname, UINT32((bdata)->sysreg_shadow.regname))
#define PUSH_SHADOWED_XTREG(bdata, regname) \
	ODSY_WRITE_HW((bdata->hw), xt.regname, UINT32((bdata)->sysreg_shadow.regname))

#if defined(ODSY_SIM_KERN)
   #define ODSY_PIPENR(hwaddr) (((__uint64_t)hwaddr)-1)
#else
   #define ODSY_PIPENR(hwaddr) odsyPipeNrFromBase(hwaddr)
#endif

#define ODSY_PIPENR_FROM_BDATA(bdata)	(bdata-OdsyBoards)
#define ODSY_BDATA_FROM_PIPENR(pipenr)	(&OdsyBoards[(pipenr)])
#define ODSY_BDATA_FROM_BASE(hwaddr)	(&OdsyBoards[ODSY_PIPENR(hwaddr)])
#define ODSY_BDATA_FROM_GFXP(gfxp)	((odsy_data*)((gfxp)->gx_bdata))
#define ODSY_BDATA_FROM_GCTX(gctx)	ODSY_BDATA_FROM_GFXP((gctx)->rnp->gr_gfxp)
#define ODSY_GFXP_FROM_GCTX(gctx)       ((gctx)->rnp->gr_gfxp)
#define ODSY_GCTX_FROM_RNP(rnp)		((odsy_gctx*)((rnp)->gr_private))

	/*XXXadamsk i _think_ it is okay to read the boundrn w/o gfxsema/lock */
	/* as the code for bindproctorn looks safe wrt this behavior.         */
#define ODSY_BOUND_GCTX_FROM_GFXP(gfxp)  ODSY_GCTX_FROM_RNP((struct rrm_rnode*)(gfxp)->gx_boundrn)


/* 
** rvalp might not be accessible in all cases. if that happens
** add a pointer arg to the macro and test it for zero before 
** dereferencing to set...
*/
#define ODSY_COPYIN_SIMPLE_ARG(uarg,karg,argtype,errmsg) \
        if (copyin(uarg,(void*)(karg),sizeof(argtype))){ \
 		cmn_err(CE_WARN,errmsg);                 \
 		*rvalp = EFAULT;                         \
 		return EFAULT;                           \
 	}

#define ODSY_COPYIN_ARG(uarg,karg,size,errmsg) \
        if (copyin(uarg,(void*)(karg),size)){ \
 		cmn_err(CE_WARN,errmsg);                 \
 		*rvalp = EFAULT;                         \
 		return EFAULT;                           \
 	}

#define ODSY_COPYOUT_SIMPLE_ARG(uarg,karg,argtype,errmsg)         \
        if(copyout((void*)(karg),(void*)(uarg),sizeof(argtype))){ \
		cmn_err(CE_WARN,errmsg);                          \
                *rvalp = EFAULT;                                  \
                return EFAULT;                                    \
        }


#if defined(_STANDALONE)
#   define INITSEMA(p, v)
#else	/* !_STANDALONE */

#ifdef DEBUG
/* suppress semameter dup messages */
#   define INITSEMA(p, v)	semameteroff(p); initsema((p), (v))
#else
#   define INITSEMA(p, v) initsema((p),(v))
#endif
#endif	/* _STANDALONE */

/*
 * These macros translates the desired number of empty entries in the 
 * fifo's into the desired level. 
 */
#define ODSY_CFIFO_DWORDS	256	/* size of cfifo in the 'in' block */
#define ODSY_CFIFO_LEVEL_REQ(x) (ODSY_CFIFO_DWORDS-2-(x))

#define ODSY_DFIFO_DWORDS	24	/* dbe fifo depth in double words*/
#define ODSY_DFIFO_LEVEL_REQ(x)	(ODSY_DFIFO_DWORDS-(x))

/*
 * Polling fifo levels.  "wait_level" is the desired fifo 
 * level so at least FIFO_DEPTH - wait_level entries are available.
 * 
 * If necessary call to timeout/untimeout can be added before and
 * after the macro.  The timeout routine will have to reset gfx.
 */
#if !defined(ODSY_SIM_KERN)

#define ODSY_POLL_DELAY 5	/* wait time in micro sec before polling again */
#define ODSY_POLL_CFIFO_LEVEL(hw, wait_level)			\
{								\
    __odsyreg_fifo_levels	levels;				\
								\
    do {							\
	ODSY_READ_HW(hw, sys.fifo_levels, *(__uint32_t*)&levels);		\
	if (levels.gfx_flow_fifo_dw_cnt > wait_level)		\
	    DELAY(ODSY_POLL_DELAY);						\
    } while (levels.gfx_flow_fifo_dw_cnt > wait_level);		\
}

#define ODSY_POLL_DFIFO_LEVEL(hw, wait_level) odsy_poll_dfifo_level(hw, wait_level)

#else

#define RPOLL_WAIT_COUNT 0x10000 /* a large number of clk cycles to wait */
#define CFIFO_MASK    0xff80    /* fifo = bits 7-15 in fifo_levels reg */
#define DFIFO_MASK    0x7f	/* dbe fifo = bits 0-6 in fifo_levels reg */

#define FIFO_LEVELS_OFFSET	0x1068

#define ODSY_POLL_CFIFO_LEVEL(hw, wait_level) \
ODSY_WRITE_SIM_DIRECTIVE_rpoll(hw, FIFO_LEVELS_OFFSET, 0, wait_level, CFIFO_MASK, RPOLL_WAIT_COUNT)

#define ODSY_POLL_DFIFO_LEVEL(hw, wait_level) \
ODSY_WRITE_SIM_DIRECTIVE_rpoll(hw, FIFO_LEVELS_OFFSET, 0, wait_level, DFIFO_MASK, RPOLL_WAIT_COUNT)

#endif /* ODSY_SIM_KERN */


/*	
 * DFIFO SEMAPHORE
 *
 * Because of STREAMS interaction with blocking threads, the mouse interrupt 
 * cannot wait on a sleep lock or semaphore that the Xserver might need.
 * The Xserver needs the interrupt to complete after it acquires a sleep lock
 * or semaphore, but if the interrupt needs this lock or semaphore to complete
 * then we get deadlock.   So the mouse interrupt can only try to acquire the
 * semaphore.  If it succeeds, we go on like normal.  If it fails, then it
 * just stores the mouse position so that the routine with the semaphore can
 * reposition the mouse.  So when the routine holding the semaphore does a 
 * vsema, it may need to send the mouse position itself.
 */

#if !defined(_STANDALONE)
#define ODSY_DFIFO_PSEMA(bdata)	\
		psema(&(bdata)->dfifo_sema, PZERO)

#define ODSY_DFIFO_VSEMA(bdata)							\
{										\
	int __mouse_pos_lock_val = mutex_spinlock(&(bdata)->mouse_pos_lock);	\
	if (! (volatile __uint64_t *) (bdata)->mouse_pos_cmd) {			\
	    vsema(&(bdata)->dfifo_sema);					\
	    mutex_spinunlock(&(bdata)->mouse_pos_lock, __mouse_pos_lock_val);	\
	} else {								\
	    mutex_spinunlock(&(bdata)->mouse_pos_lock, __mouse_pos_lock_val);	\
	    if (!ODSY_POLL_DFIFO_LEVEL((bdata)->hw, ODSY_DFIFO_LEVEL_REQ(1))) {	\
		__mouse_pos_lock_val = mutex_spinlock(&(bdata)->mouse_pos_lock);\
		ODSY_WRITE_HW((bdata)->hw, dbe, (bdata)->mouse_pos_cmd);	\
		(bdata)->mouse_pos_cmd = 0L;					\
		mutex_spinunlock(&(bdata)->mouse_pos_lock, __mouse_pos_lock_val);\
	    }									\
	    vsema(&(bdata)->dfifo_sema);					\
	}									\
}

#else	/* !_STANDALONE */

#define ODSY_DFIFO_PSEMA(bdata)		/* none */

#define ODSY_DFIFO_VSEMA(bdata)		/* none */ 

#endif  /* !_STANDALONE */

/*
** prototypes, ordered by file in which function appears.
**      we'd like to keep these the last things in this file...
**
** prefix  :          denotes
** -------------------------------------
**   gf_       rrm/gfx entry point
**  gfP_       private ioctl entry point
** odsy_       random (UNIX,cn_, timer hooks; still don't know what all yet)
** odsyA       internal to implementation, fn "A"
**
*/

/* odsy.c */
int gf_OdsyInfo(struct gfx_data *gf_data,
                void            *buf,
                unsigned int     len,
                int             *rvalp);

int gf_OdsyPrivate(struct gfx_gfx   *gfxp,
                   struct rrm_rnode *gf_rnode,
                   unsigned int      cmd,
                   void             *arg,
                   int              *rvalp);
odsy_gctx * odsyGctxFromRnid(unsigned short rnid);

/* odsy_attach.c */
void odsy_regmatch(void *ogfxp,void *ddv,void *gfxp);
int  gf_OdsyAttach(struct gfx_gfx *gfxp, caddr_t vaddr);
int  gf_OdsyDetach(struct gfx_gfx *gfxp);


/* odsy_brdmgr.c */
int gfP_OdsyBRDMGRsetColors(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyBRDMGRconfigFb(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyBRDMGRcursor(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyBRDMGRsetDpyMode(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyBRDMGRsetBlanking(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyBRDMGRloadTimingTable(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyBRDMGRenableVTG(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyBRDMGRi2ccmd(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gf_OdsyPositionCursor(struct gfx_data *_bdata, int x, int y);
int odsyLoadTimingTable(odsy_data *, odsy_brdmgr_timing_info *);
int odsyConfigFb(odsy_data *, odsy_brdmgr_config_fb_arg *);
int odsyBlankScreen(odsy_data *, int);


/* odsy_clip.c */
int gf_OdsyValidateClip(struct gfx_gfx          *, 
                        struct rrm_rnode        *,
                        struct rrm_rnode        *,
                        struct RRM_ValidateClip *);
int gf_OdsyValidateBanks(struct rrm_rnode *,
                         struct rrm_rnode *);
int gf_OdsySetNullClip(struct RRM_ValidateClip *);
void odsyClipPush(odsy_data *bdata, odsy_gctx *, int);


/* odsy_ctxsw.c */
int gf_OdsyPcxSwap(struct gfx_data *gbd, 
                   struct rrm_rnode *out_rnp,
                   struct rrm_rnode *in_rnp,
                   struct rrm_rnode *cur_rnp);

int gf_OdsyPcxSwitch(struct gfx_data *, 
                     struct rrm_rnode *,
                     struct rrm_rnode *);

int gfP_OdsyGLGet(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsySaveHwShadow(struct gfx_gfx *,struct rrm_rnode *,void *,int *);

void odsyRestoreRaster(odsy_data *, odsy_gctx *gctx);
void odsyRestoreXform(odsy_data *, odsy_gctx *gctx);
void odsyRestoreGfe(odsy_data *, odsy_gctx *gctx);
void odsyRestartContext(odsy_data *, odsy_gctx *gctx);
void odsyRestorePartialCmd(odsy_data *, odsy_gctx *gctx);
void odsyResetHwShadow(odsy_hw_shadow *hs);
void odsy_ctxswtimeout(odsy_data*);


/* odsy_ddrn.c */
void odsyInitDDRNs(void);
int gf_OdsyCreateDDRN(struct gfx_data *gbd, struct rrm_rnode *rnp);
int gf_OdsyDestroyDDRN(struct gfx_data *gbd, struct gfx_gfx *gfxp, struct rrm_rnode *rnp);

/* odsy_diag.c */
int gfP_OdsyDIAGallocScanbuf(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyDIAGfreeScanbuf(struct gfx_gfx *, struct rrm_rnode *,void *, int *);
int gfP_OdsyDIAGgetShadowPtrs(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyDIAGreleaseShadowPtrs(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyDIAGreset(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyPerfStat(struct gfx_gfx *, struct rrm_rnode *, void *, int *);

/* odsy_dma.c */
int gfP_OdsyMMwaitXfr(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int odsyDMAOp(struct gfx_gfx  *gfxp,
	      odsy_gctx       *gctx,
	      odsy_data       *bdata,
	      void            *start,
	      size_t           len,
	      int             *hw_async,
	      odsy_dma_param  *dma_param,
	      odsy_dma_marker *op_marker);
int odsyDMAWait(odsy_data *bdata, struct gfx_gfx *, odsy_gctx *, int flag, __uint64_t *op_nr);
void odsyDMAWakeup(odsy_data *bdata);
int odsyDMApinPages(struct gfx_gfx *,odsy_gctx *gctx,void *uv_start,size_t bytes);
void odsyDMAunpinPages(struct gfx_gfx *,odsy_gctx *gctx,void *uv_start,size_t bytes);
odsy_dma_rgn *odsy_NewDmaRegion(struct gfx_gfx *,
				odsy_gctx *gctx,
				void *start,size_t len, 
				int rgn_hdl,int is_user_rgn,
				odsy_dma_param  *dma_param,
				int * errcode);
void odsy_DeleteDmaRegion(odsy_dma_rgn *);
int  odsy_DmaRegion_Pin(odsy_dma_rgn *);
void odsy_DmaRegion_UnPin(odsy_dma_rgn *);
int  odsy_DmaRegion_BuildXlatTbl(odsy_dma_rgn *);
int  odsy_DmaRegion_DoTransfer(odsy_data *, odsy_dma_rgn *, __uint32_t );
int  gfP_OdsyDIAGdma(struct gfx_gfx *,struct rrm_rnode *,void *,int *);

/* odsy_i2c.c */
int odsy_i2c_start(port_handle *, unsigned char, unsigned short);
int odsy_i2c_stop(port_handle *);
int odsy_i2c_sendbyte(port_handle *, unsigned char, unsigned short, unsigned short);
int odsy_i2c_long_recvbyte(port_handle *, unsigned short, unsigned short, unsigned char *);
int odsy_i2c_short_recvbyte(port_handle *, unsigned char *);
int odsy_i2c_slave_recvbyte(port_handle *, unsigned short, unsigned short, unsigned char *);
int odsy_i2c_xfer_done_loop(port_handle *);
int odsy_i2c_recv_ack(port_handle *, unsigned short);
int odsy_i2c_becomeSlaveRecv(port_handle *, unsigned short, unsigned short, unsigned char *);
int odsy_i2c_init_port_handle(port_handle *, odsy_data *, int);
void odsy_i2cReset(port_handle *);

/* odsy_ddc.c */
int odsy_i2c_cmd(odsy_data *, struct odsy_brdmgr_i2c_args *, void *);
int odsy_i2cSetPLLFreq(odsy_data *, odsy_brdmgr_timing_info_arg);
int odsy_i2cPLLRegWrite(port_handle *, unsigned char, unsigned char);
int odsy_i2cPLLRegRead(port_handle *, unsigned char, unsigned char *);
int odsy_i2cPLLSeqRegWrite(port_handle *, unsigned char, unsigned char *, int);
int odsy_i2cPLLSeqRegRead(port_handle *, unsigned char, unsigned char *, int);

int odsy_i2cValidEdid(ddc_edid_t *, odsy_i2cinfo_t *);
int sgiEdid(ddc_edid_t *);
void odyFixupSony24(ddc_edid_t *);
unsigned short odsy_EdidCksum(ddc_edid_t *);
int odsy_Probe2AB(port_handle *, unsigned char *);
int odsy_2abCmdWrapper(port_handle *, struct odsy_brdmgr_i2c_args *,
    unsigned char *, unsigned char *);
int odsy_2abSendCmd(port_handle *, int, unsigned char *, int);
int odsy_2abGetReply(port_handle *, unsigned char *, int *);
void odsy_validate_ddc_data(odsy_data *);

#if defined(GFX_IDBG)
/* odsy_idbg.c */
void odsyInitIDBG(void);
#endif

typedef enum { DoDownload = 0, NoDownload } OdsyDoDownloadType;

/* odsy_init.c */
void     odsy_teardown(odsy_data *);
void     odsyPrintRegs(odsy_hw *);
int      odsy_earlyinit(void);
void     odsy_init(void);
int      odsy_attach(vertex_hdl_t conn);
odsy_hw *odsyFindBoardAddr(int brdnr);
int      odsyPipeNrFromBase(odsy_hw *);
void     odsyInitBoard(int board);
void     odsyInitIntr(int board, vertex_hdl_t conn, vertex_hdl_t vhdl);
void     odsyInitBoardGfx(int board);
void     odsyReset(int board);
void     odsyInitCfifo(int board, int useFlowControl);
void     odsyInitDMA(int board);
void     odsyInitInterrupts(int board);
void     odsyInitDBE(odsy_data *, odsy_brdmgr_timing_info *, OdsyDoDownloadType);
int      odsyGatherInfo(int board);
int      gf_OdsyInitialize(struct gfx_gfx *gfxp);
int      gf_OdsyStart(struct gfx_gfx *gfxp);

/* odsy_ucode.c */
int      gf_OdsyDownload(struct gfx_gfx *, struct gfx_download_args *);
int      odsyDownloadUcode(odsy_data *, unsigned int, unsigned int, unsigned int);

/* odsy_intr.c */
void odsyXtErrorInterrupt(odsy_data *bdata);
void odsyFlagInterrupt(odsy_data *bdata);
void odsyDmaInterrupt(odsy_data *bdata);
void odsyVtraceInterrupt(odsy_data *bdata);
void odsySsyncInterrupt(odsy_data *bdata);
void odsyGenlockInterrupt(odsy_data *bdata);

/* odsy_flow.c */
int odsy_poll_dfifo_level(odsy_hw *hw, int wait_level);
void odsyHiwaterInterrupt(odsy_data *bdata);
void odsyLowaterInterrupt(odsy_data *bdata);
void odsyFifoTimer(odsy_data *bdata, int flag);
void odsyFifoTimeout(odsy_data *bdata);

/* odsy_map.c */
int gf_OdsyMapGfx(struct gfx_gfx *gfxp, __psunsigned_t faddr, int pcxid);		
int gf_OdsyUnMapGfx(struct gfx_gfx *gfxp);
int gf_OdsyInvalTLB(struct gfx_gfx *gfxp);
int odsyPinAndMapGctxSwWrRgn(odsy_gctx *gctx,int *rvalp, void *uv_sw_wr_rgn);
int odsyPinAndMapGctxSwShadowRgn(odsy_gctx *gctx,int *rvalp,void *uv_sw_shadow_rgn);
void odsyUnpinAndUnmapGctxRegions(odsy_gctx *gctx);
int odsyMapPages (uvaddr_t      uv_hint, 
		  uvaddr_t     *uv_ret, 
		  caddr_t       kv, 
		  int           nbytes, 
		  v_mapattr_t  *vmap_tab,
		  int           vmap_size,
		  ddv_handle_t *ddvp );
int odsyUnmapPages (ddv_handle_t *ddv);
int odsyCreateRegion (ddv_handle_t *ddvp, int len, caddr_t addrhint, int pflags);
void odsyUnpinAndUnmapSGRegions(odsy_mm_sg *sg);
int odsyUnpinAndUnmapASRegions(odsy_mm_as *as);
int odsyPinAndMapASRegions(odsy_mm_as *as);
int odsyPinAndMapPfStatRegions(odsy_data *);
int odsyUnpinAndUnmapPfStatRegions(odsy_data *);


/* odsy_retrace.c */
void odsyInitRetrace(odsy_data *);
int gf_OdsySchedSwapBuf(struct gfx_data *, struct rrm_rnode *, int, int);  
int gf_OdsyUnSchedSwapBuf(struct gfx_gfx *, struct rrm_rnode *, int);
int gf_OdsySchedRetraceEvent(struct gfx_data *, struct rrm_rnode *);
int gf_OdsyUnSchedRetraceEvent(odsy_data *, struct rrm_rnode *);
int gf_OdsySetDisplayMode(struct gfx_gfx *,int, unsigned int);
int gf_OdsySchedMGRSwapBuf(struct gfx_gfx *, int, int, int, int);  
int gf_OdsyFrsInstall(struct gfx_data *, void* );
int gf_OdsyFrsUninstall(struct gfx_data *);
void odsySwap_softsync_intr(odsy_data *);
void odsyRetraceHandler(odsy_data *);
void odsyInitHwWrRgn(int board);


/* odsy_sched.c */
int gf_OdsySuspend(struct gfx_data *gbd, struct gfx_gfx *gfxp, int kpreempt);
int gf_OdsyResume(struct gfx_data *gbd, struct gfx_gfx *gfxp,  int kpreempt);
int gf_OdsyExit(struct gfx_gfx *gfxp);
int gf_OdsyReleaseGfxSema(struct gfx_gfx*gfxp);
int odsyDiagExit(odsy_data *, struct gfx_gfx *);
/*
 * odsy_sdram.c
 */
int gfP_OdsyMMattach(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMalloc(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMdealloc(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMdebug(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMmakeResident(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMquery(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMnotify(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsyMMxfr(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
void odsyMMdetach(struct gfx_gfx *, odsy_gctx *);
void odsyMMinit(odsy_hw *hw,odsy_mm* mm);
int odsyMMalloc(struct gfx_gfx *gfxp, odsy_mm_sg *sg, odsy_mm_alloc_arg *, odsy_mm_obj_grp *,odsy_mm_obj*);
int odsyMMlameReserve ODSY_MM_FNCS_RESERVE_PROTO;
int odsyMMlameCommit  ODSY_MM_FNCS_COMMIT_PROTO;
int odsyMMlameValidateCommit  ODSY_MM_FNCS_VALIDATE_COMMIT_PROTO;
int odsyMMlameDecommit ODSY_MM_FNCS_DECOMMIT_PROTO;
int odsyMMlameRelease ODSY_MM_FNCS_RELEASE_PROTO;
int odsyMMvalidatePcxSwap(odsy_data *, odsy_gctx *in_gctx, odsy_gctx *out_gctx);

/*
 * odsy_drawable.c
 */
void odsyInitDrawables(odsy_data *);
int odsyBindDrawable(odsy_data *, odsy_gctx *, OdsyDrawType, int);
int odsyUnbindDrawable(odsy_data *, odsy_gctx *, OdsyDrawType);
int gfP_OdsyPbufAssoc(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsyPbufDeassoc(struct gfx_gfx *, struct rrm_rnode *, void *,int *);
int gfP_OdsyMakeCurrent(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
int gfP_OdsySetDrawBuffer(struct gfx_gfx *, struct rrm_rnode *, void *, int *);
void odsyPushBufferSelect(odsy_gctx *, odsy_hw *);
void odsyPushDMABaseRegs(odsy_gctx *, odsy_hw *);

#ifdef ODSY_SIM_KERN
/*
 * odsy_sim.c
 */
int gfP_OdsySIMsymbiotePIO(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsySIMsymbioteDMA(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsySIMUrwPipePIO(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int gfP_OdsySIMzapDelayedPIO(struct gfx_gfx *,struct rrm_rnode *,void *,int *);
int odsySIMdoDelayedPIO(odsy_data *bdata,int zap_instead);
int odsySIMfieldEarlyPIOProbe(__uint32_t ,odsy_sim_rdwr *);
int odsySIMdoPipePIO(odsy_data *, odsy_sim_rdwr *,int kmode);
void odsySIMsetRangeCheckDMA(odsy_data *, odsy_dma_one_range *);
void odsyHandleSimIntr(odsy_data *, __uint64_t);

#endif /* ODSY_SIM_KERN */


/* gfx_flow.c */
void gfx_flow_clean_up_other_gfxp(struct gfx_gfx *gfxp);

#endif /* __ODSY_INTERNALS_H__ */
