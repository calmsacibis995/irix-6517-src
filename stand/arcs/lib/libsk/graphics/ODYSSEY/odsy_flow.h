#ifndef _ODSY_FLOW_H_
#define _ODSY_FLOW_H_


/* CFIFO tiles are 2bpp tiles, so we multiply by 16 to
** get into the 32bpp tile-space when setting the sdram
** base address... *it is zero anyway* but the next guy
** needs to get the next 32bpp tile (framebuffer is next)
**
** XXXtodo: this stuff needs to be configurable by user/sysadm
** in some way shape or form...
*/
#define ODSY_CFIFO_NR_32BPP_TILES 16 /*16*8k=>128k cfifo*/
#define ODSY_CFIFO_NR_2BPP_TILES (ODSY_CFIFO_NR_32BPP_TILES*16)
#define ODSY_CFIFO_NR_WORDS      ((ODSY_CFIFO_NR_32BPP_TILES*32*16*16)/4)
#define ODSY_CFIFO_PHYS_LW_WORDS (ODSY_CFIFO_PHYS_HW_WORDS*3/4)
#define ODSY_CFIFO_PHYS_HW_WORDS (ODSY_CFIFO_NR_WORDS-ODSY_CFIFO_SKID)

/*
 * there is no wrap around protection for the sdram cfifo
 * the physical high-water mark must be set low enough to account for
 *	the buffering in the 'in' block	(16 cl)
 *	and misc pipe stages in the blocks between 'in' and 'iw' (about 2 cl)
 */
#define ODSY_CFIFO_SKID (ODSY_CFIFO_DWORDS * 2 + 0x40)


/*
** for use as gfe flow control config data payload
*/
#define ODSY_FLOWCTL_TIMO(x)            (x)             /* 10 bits */
#define ODSY_FLOWCTL_COHERENT(x)        ((x)<<10)       /* 0|1 */
#define ODSY_FLOWCTL_GBR(x)             ((x)<<11)       /* 0|1 */
#define ODSY_FLOWCTL_TNUM(x)            ((x)<<12)       /* 5 bits */
#define ODSY_FLOWCTL_DIDN(x)            ((x)<<17)       /* 4 bits */
#define ODSY_FLOWCTL_DBE_FL_CTL_EN(x)   ((x)<<21)       /* 0|1 */
#define ODSY_FLOWCTL_ASD_HACK_MODE      (3<<23)
#define ODSY_FLOWCTL_BEAUTY_MODE        (2<<23)
#define ODSY_FLOWCTL_HEART_MODE         (1<<23)
#define ODSY_FLOWCTL_DISABLED_MODE      (0<<23)




/*
 * high-level flow control macros
 *      CMD is a name-tag for logging purposes
 *      N.B. need variables bdata and gfxp from surrounding block
 */
# define SET_FC(CMD)                                                    \
{                                                                       \
    int spl = disableintr();                                            \
    SET_FC_SPLHI(CMD);                                                  \
    enableintr(spl);                                                    \
}

# define SET_FC_AND_FLAG(CMD, flagbit)                                  \
{                                                                       \
    int spl = disableintr();                                            \
    gfxp->gx_flags |= (flagbit);                                        \
    SET_FC_SPLHI(CMD);                                                  \
    enableintr(spl);                                                    \
}
#  define UNSET_FC(CMD)                                                 \
{                                                                       \
    int spl = disableintr();                                            \
    UNSET_FC_SPLHI(CMD);                                                \
    enableintr(spl);                                                    \
}
#  define UNSET_FC_AND_FLAG(CMD, flagbit)                               \
{                                                                       \
    int spl = disableintr();                                            \
    gfxp->gx_flags &= ~(flagbit);                                       \
    UNSET_FC_SPLHI(CMD);                                                \
    enableintr(spl);                                                    \
}



/*
 * low-level flow control macros
 *    ATTACH_FLOW and DETATCH_FLOW do the odyssey specific work
 *      of binding/unbinding a gfx board to/from a pipe head.
 *    gfx_flow_attach_me and gfx_flow_detach_me do the non-device specific work
 *      of binding/unbinding a pipe head to/from a cpu.
 *      these routines are shared with MGRAS.
 */
#if defined(ODSY_NO_FC)
#define ATTACH_FLOW(bdata)
#define DETACH_FLOW(bdata)
#define SET_FC_SPLHI(CMD)
#define UNSET_FC_SPLHI(CMD)
#define gfx_flow_am_i_attached(x) 1
#elif IP30

#define ATTACH_FLOW(bdata) \
      { unsigned int    fcinfo; \
        fcinfo = UINT32(bdata->sysreg_shadow.flow_ctl_config); \
        fcinfo &= ~ODSY_FLOWCTL_TNUM(0x1f); \
        fcinfo |= ODSY_FLOWCTL_TNUM( gfx_flow_credit_tnum_get(bdata->flow_hdl) ); \
	UINT32(bdata->sysreg_shadow.flow_ctl_config) = fcinfo; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_config); \
	bdata->sysreg_shadow.flow_ctl_detach = 0; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_detach); \
      }

#define DETACH_FLOW(bdata) \
      { volatile unsigned int tmp; \
	bdata->sysreg_shadow.flow_ctl_detach = 1; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_detach); \
	ODSY_READ_HW(bdata->hw, sys.fifo_levels, tmp); \
      }

#  define SET_FC_SPLHI(CMD)                                             \
{                                                                       \
    ASSERT(!bdata->gfx_data.gfxbackedup);                               \
    if (bdata->flow_hdl && !gfx_flow_am_i_attached(bdata->flow_hdl)) {  \
        gfx_flow_attach_me(bdata->flow_hdl, gfxp);                      \
        ATTACH_FLOW(bdata);                                             \
        GFXLOGT(CMD);                                                   \
    }                                                                   \
}
#  define UNSET_FC_SPLHI(CMD)                                           \
{                                                                       \
    if (bdata->flow_hdl && gfx_flow_am_i_attached(bdata->flow_hdl)) {   \
        DETACH_FLOW(bdata);                                             \
        gfx_flow_detach_me(bdata->flow_hdl, gfxp);                      \
        GFXLOGT(CMD);                                                   \
    }                                                                   \
}

#elif IP27

/* these macros start out same as the IP30 versions */
#define ATTACH_FLOW(bdata) \
      { unsigned int    fcinfo; \
        fcinfo = UINT32(bdata->sysreg_shadow.flow_ctl_config); \
        fcinfo &= ~ODSY_FLOWCTL_TNUM(0x1f); \
        fcinfo |= ODSY_FLOWCTL_TNUM( gfx_flow_credit_tnum_get(bdata->flow_hdl) ); \
	UINT32(bdata->sysreg_shadow.flow_ctl_config) = fcinfo; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_config); \
	bdata->sysreg_shadow.flow_ctl_detach = 0; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_detach); \
      }

#define DETACH_FLOW(bdata) \
      { volatile unsigned int tmp; \
	bdata->sysreg_shadow.flow_ctl_detach = 1; \
	PUSH_SHADOWED_SYSREG(bdata, flow_ctl_detach); \
	ODSY_READ_HW(bdata->hw, sys.fifo_levels, tmp); \
      }

#  define SET_FC_SPLHI(CMD)                                             \
{                                                                       \
    ASSERT(!bdata->gfx_data.gfxbackedup);                               \
    if (bdata->flow_hdl && !gfx_flow_am_i_attached(bdata->flow_hdl)) {  \
        gfx_flow_attach_me(bdata->flow_hdl, gfxp);                      \
        ATTACH_FLOW(bdata);                                             \
        GFXLOGT(CMD);                                                   \
    }                                                                   \
}
#  define UNSET_FC_SPLHI(CMD)                                           \
{                                                                       \
    if (bdata->flow_hdl && gfx_flow_am_i_attached(bdata->flow_hdl)) {   \
        DETACH_FLOW(bdata);                                             \
        gfx_flow_detach_me(bdata->flow_hdl, gfxp);                      \
        GFXLOGT(CMD);                                                   \
    }                                                                   \
}

#endif

/*
 * private interface for some flow control stuff
 */
extern int gfx_flow_pipe_head( gfx_flow_hdl_t gfx_flow_hdl );

/*
 * f(addr) := (addr & ~ODY_FC_MASK) == ODY_FC_ADDR_LO)
 * want f(0x10xxxx) == true
 *	f(0x11xxxx) == true
 * but	f(0x12xxxx) == false
 * and  f(>= 0x2xxxxx) == false
 * and  f(<= 0x0xxxxx) == false
 */
#define ODY_FC_ADDR_LO		0x100000
#define ODY_FC_MASK		0x01ffff
#ifndef ODSY_DO_NOT_USE_MAX_IN_CFIFO
# define ODY_FC_START_CREDIT	ODSY_CFIFO_DWORDS
#else
# define ODY_FC_START_CREDIT	0xe5 /* back off a little from ODSY_CFIFO_DWORDS */
#endif
#define ODY_FC_TIMER_NS		10000
#define ODY_FLOW_CTL_TIMEOUT	200	/* ticks of the 100 MHz clock */
#define ODY_FLOW_CTL_DIDN	8
#define ODY_FC_TIMER_US		10
#define ODY_FIFO_TIMEOUT	(15*HZ)	/* 15 seconds */

#endif
