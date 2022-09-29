#include "odsy_internals.h"
#include "sys/sysinfo.h"


int
odsy_poll_dfifo_level(odsy_hw *hw, int wait_level)
{
    __odsyreg_fifo_levels       levels;
    int passes = 0;

    do {
        ODSY_READ_HW(hw, sys.fifo_levels, *(__uint32_t*)&levels);
        if (levels.dbe_flow_fifo_dw_cnt > wait_level)
            DELAY(ODSY_POLL_DELAY);
    } while (levels.dbe_flow_fifo_dw_cnt > wait_level && ++passes < 10000);
    if (levels.dbe_flow_fifo_dw_cnt > wait_level) {
        cmn_err(CE_WARN, "odsy dfifo timeout\n");
        return 1;
    } else
        return 0; /* return false if dfifo is ready to be written */
}


#if !defined( _STANDALONE )

/*
 * hiwater/lowater/timeout routines
 * cloned from MGRAS versions
 */

void
odsyHiwaterInterrupt(odsy_data *bdata)
{
    odsy_hw 	*hw = bdata->hw;
    struct gfx_gfx	*gfxp;
    int status;
    int fc_num;
    unsigned int crcnt;
    __odsyreg_status0 status0;
    int ii, s;

    /* 
     * Dismiss spurious interrupts caused by fc timers.
     * The isr is cleared by heart_intr so the timers
     * will rearm and generate more interrupts if the 
     * credit level has not risen above the bias.  We can 
     * dismiss those until we are not backedup.
     */
    s = mutex_spinlock(&bdata->hiwaterlock);
    if (bdata->gfx_data.gfxbackedup) {
	mutex_spinunlock(&bdata->hiwaterlock, s);
	return;
    }

    fc_num = gfx_flow_pipe_head(bdata->flow_hdl);
    GFXLOGT(ODSY_HIWATER);
    /*
     * spin for a short time in the interrupt routine
     */
    for (ii = ODY_FC_TIMER_US; ii > 0; ii--) {
	status = gfx_flow_credit_count_get(bdata->flow_hdl) <
		 gfx_flow_credit_bias_get(bdata->flow_hdl);
	if (status == 0) {
	    GFXINFO.fifonowait++;   /* sar */
            GFXLOGT(ODSY_HIWATER_SPIN_ESC);
	    mutex_spinunlock(&bdata->hiwaterlock,s);
	    return;
	}
	DELAY(1);
    }
    GFXINFO.fifowait++;
    GFXLOGT(ODSY_HIWATER_WAIT);

    ASSERT (bdata->gfx_data.gfxbackedup == 0);
    bdata->gfx_data.gfxbackedup = 1;
    gfxp = bdata->gfx_data.gfxsema_owner;
    INCR_GFXLOCK(gfxp);
    force_resched();

    /* don't let process hog pipe */
    bdata->gfx_data.gfxresched = 1;

    /* Set lowater mark dynamically */
    /* Set up timer in case gfx is dead */
    odsyFifoTimer(bdata, 1);

    /* Clear the lowater flag */
    ODSY_WRITE_HW(hw,sys.flag_clr_priv,ODSY_FLAG_REG_CFIFO_LW_FLAG);

    /* 
    ** This codes prevents a race condition that could cause us to
    ** miss the lowater interrupt.  See the system programmers guide
    ** to interrupts for details.
    */

    /* See if we may have hit lowater before we cleared the flag */
    ODSY_READ_HW(bdata->hw,sys.status0,*(__uint32_t*)&status0);
    if (status0.cfifo_lw) {
	/* Set the lowater flag if it should be set */
	ODSY_WRITE_HW(hw,sys.flag_set_priv,ODSY_FLAG_REG_CFIFO_LW_FLAG);
    }

    /* Enable lowater interrupt */
    UINT32(bdata->sysreg_shadow.flag_intr_enab_set) |= ODSY_FLAG_REG_CFIFO_LW_FLAG;
    ODSY_WRITE_HW(hw, sys.flag_intr_enab_set, ODSY_FLAG_REG_CFIFO_LW_FLAG);

    mutex_spinunlock(&bdata->hiwaterlock, s);
}

void
odsyLowaterInterrupt(odsy_data *bdata)
{
    odsy_hw 	*hw = bdata->hw;
    struct gfx_gfx	*gfxp;
    int s;

    GFXLOGT(ODSY_LOWATER);

    /* kill the timer interrupt */
    odsyFifoTimer(bdata, 0);

    /* Disable lowater interrupt */
    UINT32(bdata->sysreg_shadow.flag_intr_enab_set) &= ~ODSY_FLAG_REG_CFIFO_LW_FLAG;
    ODSY_WRITE_HW(hw, sys.flag_intr_enab_clr, ODSY_FLAG_REG_CFIFO_LW_FLAG);

    /*
     * do not enable flow control interrupt (set imr) here
     * it will be enabled when the process resumes
     */

    gfxp = bdata->gfx_data.gfxsema_owner;
    DECR_GFXLOCK(gfxp);

    /* 
     * It is important to not clear the backup flag before lowater handler
     * finishes its business.  On a MP system clearing the flag will allow 
     * gfx processess to use the pipe and cause another hiwater interrupt.
     * This can create a race in changing gfxlock since the macros are not
     * atomic (Especially when we are doing all these GFXLOG calls within it.
     */
    s = mutex_spinlock(&bdata->hiwaterlock);
    bdata->gfx_data.gfxbackedup = 0;
    mutex_spinunlock(&bdata->hiwaterlock, s);
    /* force resched */
    force_resched();
}


void
odsyFifoTimer(odsy_data *bdata, int flag)
{
    if (flag) {
        if (bdata->ftcallid)     /* Already have timeout pending */
            return;
        bdata->ftcallid = timeout(odsyFifoTimeout, bdata, ODY_FIFO_TIMEOUT);
    } else {
        /* Cancel timeout callback */
	ASSERT(bdata->ftcallid);
	untimeout(bdata->ftcallid);
	bdata->ftcallid = 0;
    }
}


void
odsyFifoTimeout(odsy_data *bdata)
{
    GFXLOGT(ODSY_FIFO_TIMEOUT);
    /* Call odsy_teardown so that abnormal_teardown will get set first */
    odsy_teardown(bdata);

    bdata->gfx_data.gfxbackedup = 0;
    GFXLOGT(MG_CFIFO_TIMEOUT);

    /*
     * We have to make sure that we clear pending flow control interrupt.
     * This is normally cleared from mgras_hq4_lowater_intr.  But since the
     * pipe is not draining we have to do it here.  Otherwise we will be
     * getting this continuosly whenever the any gfx process resumes and
     * cause flow to be enabled again.
     */
    gfx_flow_dealloc(bdata->flow_hdl);
    bdata->flow_hdl = 0;

    DECR_GFXLOCK(bdata->gfx_data.gfxsema_owner);
    cmn_err(CE_WARN, "odsy: CFIFO timeout\n");
    bdata->ftcallid = 0;
}


#endif
