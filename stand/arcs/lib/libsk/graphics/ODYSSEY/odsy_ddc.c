/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1999, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Odsy Graphics I2C interface.
 */

#include "odsy_internals.h"

#ifdef DEBUG
/* 
 * ody_dbgi2c	= 0, no debugging msgs
 *		= 1, some debugging msgs.
 */
int ody_dbgi2c = 0;

extern void printf( char* fmt, ... );
#endif


#if !defined( _STANDALONE )

/* XXX Needs cleanup */

#ifdef DEBUG
static void odyPrintEdid(ddc_edid_t *);
static void odyPrintBadEdid(ddc_edid_t *, odsy_i2cinfo_t *);
#endif

static int delayAfterAck2B = 50;
static int delayAfterAck2AB = 150;
static int delayAfterStop2B = 500;
static int delayBefore2abRetry = 1000;
static int delayBefore2abRetry1 = 10000;
static int delayBefore2abRetry2 = 100000;
static int delayAfterGetReply = 50;


static char *n2_reply_n2[NUM_N2_MONITORS] =
        { ID_REPLY_N2_17, ID_REPLY_N2_20, ID_REPLY_24 };
/*
 * 1.  If we're the I2C master transmitter, and if we have just
 * received an ACK bit, we delay before sending another byte.
 *
 * 2.  If we're the I2C master receiver, and if we have just sent an
 * ACK bit, we delay before starting the receive of the next byte.
 */

static int i2c_num_reset_toggles = 3;
static int pll_reset_delay = 100000;	/* 1/100 sec */

static void odsy_i2cFixupAfterErr(port_handle *port)
{
    int del1 = MAX(delayAfterAck2B, delayAfterAck2AB);

    DELAY(del1);
    odsy_i2c_stop(port);
    DELAY(delayAfterStop2B);
}

/*
 * odsy_i2cSetPLLFreq: read Pixel clock freq from vfo and write
 * that to the PLL via the I2C opt port on pbj. This function
 * is implemented using the Flow Chart on pg.16 of the ICS1523
 * product review. 
 *
 * Arguments:
 *		bdata -- odsy board data structure containing pointers
 *			 to shadow state registers for the i2c ports
 *		odsy_tt_args -- contians timing information as read
 *				from the .vfo timing table files.
 * Returns:
 *		ODSY_PLL_SUCCESS or ODSY_PLL_FAILURE
 */

int odsy_i2cSetPLLFreq(odsy_data *bdata, odsy_brdmgr_timing_info_arg odsy_tt_args)
{
    port_handle port;
    unsigned char data;

    /* Initialize the port handle to point to the I2C Option port */
    if (odsy_i2c_init_port_handle(&port, bdata, ODSY_I2C_OPT_PORT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Port handle failed to init properly\n");
        return ODSY_PLL_FAILURE;
    } 

    /* Establish a sleep lock on the port */
    ODSY_I2C_GET_LOCK(&port);

#if ODSY_I2C_HIGH_LEVEL_DEBUG
    printf("Establishing lock on I2C OPTION_PORT\n");
#endif

    /* Attempting here to set DPA Output to 0 */
#if ODSY_I2C_HIGH_LEVEL_DEBUG
    printf("Writing reg ODSY_PLL_DPAOffset with val 0x%x\n",0x00);
#endif
    if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_DPAOffset_ADDR, (unsigned char)0x00) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
        return ODSY_PLL_FAILURE;
    }

    /*
     * Note this is not really an infinite loop. As soon as the PLL is locked
     * 'break' will be called to exit the loop. But we must make sure we loop 
     * until the PLL is locked.
     */
    for (;;)
    {
#if ODSY_I2C_HIGH_LEVEL_DEBUG
        printf("Beginning PLL lock loop\n");
#endif
        /*
         * Setting Input, PFD_Gain, Post and Feedback Divider as per instructions on pg.16
         * of the ICS product review
         */
#if ODSY_I2C_HIGH_LEVEL_DEBUG
        printf("Writting: PLL_LoopControl -- 0x%x; PLL_InputControl -- 0x%x; PLL_FdBkDiv0 -- 0x%x; \
                PLL_FdBkDiv1 -- 0x%x\n",odsy_tt_args.LoopControl, odsy_tt_args.InputControl,
                                        odsy_tt_args.FdBkDiv0, odsy_tt_args.FdBkDiv1);
#endif
        if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_LoopControl_ADDR, (unsigned char)(odsy_tt_args.LoopControl & 0xFF)) 
            || odsy_i2cPLLRegWrite(&port, ODSY_PLL_InputControl_ADDR, (unsigned char)(odsy_tt_args.InputControl & 0xFF))
            || odsy_i2cPLLRegWrite(&port, ODSY_PLL_FdBkDiv0_ADDR, (unsigned char)(odsy_tt_args.FdBkDiv0 & 0xFF))
            || odsy_i2cPLLRegWrite(&port, ODSY_PLL_FdBkDiv1_ADDR, (unsigned char)(odsy_tt_args.FdBkDiv1 & 0xFF)))
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
            return ODSY_PLL_FAILURE;
        }
     
        /* PLL Software Reset */
        if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_SoftReset_ADDR, ODSY_PLL_SOFT_RESET) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
            return ODSY_PLL_FAILURE;
        }

        /* Attempting to wait ~10ms */
        DELAY(pll_reset_delay);

        /* Polling reg 0x0C to determine if the PLL is locked or not */
        if (odsy_i2cPLLRegRead(&port, ODSY_PLL_StatusReg_ADDR, &data) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register read failed\n");
            return ODSY_PLL_FAILURE;
        } 

        /* If the PLL is locked we can break the loop, else we must repeat the above */
        if (data & ODSY_PLL_LOCKED)
        {
#if ODSY_I2C_HIGH_LEVEL_DEBUG
            printf("PLL is locked. Leaving loop\n");
#endif
            break;
        }
    } 

    /*
     * Note this is not really an infinite loop. As soon as the DPA is locked
     * 'break' will be called to exit the loop. But we must loop until the DPA
     * is locked.
     */
    for (;;)
    {
#if ODSY_I2C_HIGH_LEVEL_DEBUG
        printf("Beginning DPA lock loop\n");
#endif
        /* Set the desired DPA resolution */
        if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_DPAControl_ADDR, (unsigned char)(odsy_tt_args.DPAControl & 0xFF)) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
            return ODSY_PLL_FAILURE;
        }

        /* Setting DPA Software reset */
        if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_SoftReset_ADDR, ODSY_DPA_SOFT_RESET) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
            return ODSY_PLL_FAILURE;
        }

        /* Attempting to wait ~10ms */
        DELAY(pll_reset_delay);

        /* Check to see if the DPA is locked */
        if (odsy_i2cPLLRegRead(&port, ODSY_PLL_StatusReg_ADDR, &data) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register read failed\n");
            return ODSY_PLL_FAILURE;
        } 

        /* If the DPA is locked then we can break out of this loop */
        if (data & ODSY_DPA_LOCKED)
        {
#if ODSY_I2C_HIGH_LEVEL_DEBUG
            printf("DPA is locked. Leaving loop\n");
#endif
            break;
        }
    }

    /* Selecting desired DPA Output Delay */
    if (odsy_i2cPLLRegWrite(&port, ODSY_PLL_OutputEnables_ADDR, (unsigned char)(odsy_tt_args.OutputEnables & 0xFF)) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cSetPLLFreq(): Register write failed\n");
        return ODSY_PLL_FAILURE;
    }

    /* Release lock on I2C port */
    ODSY_I2C_RELEASE_LOCK(&port);

#if ODSY_I2C_HIGH_LEVEL_DEBUG
    printf("Releasing lock on I2C OPTION_PORT\n");
#endif

    return ODSY_I2C_SUCCESS;
}

#endif  /* !defined( _STANDALONE ) */

int odsy_i2cPLLRegWrite(port_handle *port, unsigned char reg_addr, unsigned char data)
{
    unsigned int pll_addr = 0;
    odsy_ddinfo *odsyinfo = &(port->bdata->info.dd_info);

    if(odsyinfo->board_rev < 1)
    {
      pll_addr = ODSY_PLL_ADDR;
    }
    else
    {
      pll_addr = ODSY_AMI_PLL_ADDR;
    }

    /* send "start" with PLL address */
    if (odsy_i2c_start(port, (pll_addr << 1) + ODSY_I2C_WRITE, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegWrite(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_sendbyte(port, reg_addr, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegWrite(): Failed to execute first 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_sendbyte(port, data, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegWrite(): Failed to execute second 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    return ODSY_PLL_SUCCESS;
}


#if !defined( _STANDALONE )

int odsy_i2cPLLRegRead(port_handle *port, unsigned char reg_addr, unsigned char *datap)
{
    unsigned int pll_addr = 0;
    odsy_ddinfo *odsyinfo = &(port->bdata->info.dd_info);

    if(odsyinfo->board_rev < 1)
    {
      pll_addr = ODSY_PLL_ADDR;
    }
    else
    {
      pll_addr = ODSY_AMI_PLL_ADDR;
    }

    if (odsy_i2c_start(port, (pll_addr << 1) + ODSY_I2C_WRITE, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegRead(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    } 

    if (odsy_i2c_sendbyte(port, reg_addr, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegRead(): Failed to execute 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_start(port, (ODSY_PLL_ADDR << 1) + ODSY_I2C_READ, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegRead(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    } 

    if (odsy_i2c_long_recvbyte(port, ODSY_I2C_ACK_NULL, ODSY_I2C_STOP_ASSERT, datap) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLRegRead(): Failed to execute 'Long_RecvByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    return ODSY_PLL_SUCCESS;
}

int odsy_i2cPLLSeqRegWrite(port_handle *port, unsigned char reg_addr, unsigned char *datap, int buflen)
{
    int i;

    if (odsy_i2c_start(port, (ODSY_PLL_ADDR << 1) + ODSY_I2C_WRITE, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegWrite(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_sendbyte(port, reg_addr, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegWrite(): Failed to execute 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    for (i = 0; i < buflen - 1; i++)
    {
        if (odsy_i2c_sendbyte(port, datap[i], ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0)
        {
            cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegWrite(): Failed to execute 'SendByte' sequece\n");
            return ODSY_PLL_FAILURE;
        }       
    }

    if (odsy_i2c_sendbyte(port, datap[i], ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegWrite(): Failed to execute 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    return ODSY_PLL_SUCCESS;
}

int odsy_i2cPLLSeqRegRead(port_handle *port, unsigned char reg_addr, unsigned char *datap, int buflen)
{
    int i;

    if (odsy_i2c_start(port, (ODSY_PLL_ADDR << 1) + ODSY_I2C_WRITE, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    } 

    if (odsy_i2c_sendbyte(port, reg_addr, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to execute 'SendByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_start(port, (ODSY_PLL_ADDR << 1) + ODSY_I2C_READ, ODSY_I2C_ACK_ASSERT) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to send 'Start' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    if (odsy_i2c_long_recvbyte(port, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL, datap) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to execute 'Long_RecvByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    for (i = 1; i < buflen - 1; i++)
    {
       if (odsy_i2c_short_recvbyte(port, &datap[i]) != 0)
       {
           cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to execute 'Short_RecvByte' sequece\n");
           return ODSY_PLL_FAILURE;
       }
    }

    if (odsy_i2c_long_recvbyte(port, ODSY_I2C_ACK_NULL, ODSY_I2C_STOP_ASSERT, &datap[i]) != 0)
    {
        cmn_err(CE_ALERT,"odsy_i2cPLLSeqRegRead(): Failed to execute 'Long_RecvByte' sequece\n");
        return ODSY_PLL_FAILURE;
    }

    return ODSY_PLL_SUCCESS;
}

/*
 * odsy_i2cProbe: Probe for I2C Monitor, try to read EDID
 */
#if 0
int odsy_i2cProbe2b(odsy_data *bdata)
{
    unsigned int i, ack;
    odsy_ddinfo *info = &bdata->info.dd_info;
    /* XXX change all references to monitor*_edid */
    ddc_edid_t *edidp = &info->monitor_edid;	/* XXX - dual channel */
    odsy_i2cinfo_t *i2cinfo = &bdata->i2cinfo;
    unsigned char *ecp = (unsigned char *) edidp;
    unsigned char firstdata;
    int num_start_retries, num_readEDID_retries;
    int allBytesAreSame;

    /* Clear EDID structure */
    bzero(edidp, sizeof(ddc_edid_t));

#ifdef DEBUG
    if (ody_dbgi2c)
	printf("odsy_i2cProbe\n");
#endif /* DEBUG */

    num_readEDID_retries = 0;

    if (bdata)
	mutex_lock(&bdata->i2c_lock, PZERO); 

    odsy_i2cReset(bdata);

retry_readEDID:
    if (++num_readEDID_retries > MAX_READ_EDID_ATTEMPTS) {
	goto bad_rtn;
    }
    allBytesAreSame = 1;

    num_start_retries = 0;
retry_start:
    if (++num_start_retries > MAX_START_ATTEMPTS) {
	goto bad_rtn;
    }

    /* Start condition */
    i2c_start(bdata);

    /* Send slave address, direction = write */
    if (!i2c_sendbyte(bdata, 0xa0)) {
	odsy_i2cFixupAfterErr(bdata);
	goto retry_start;
    }

    /* Send start address = 0x00 */
    if (!i2c_sendbyte(bdata, 0x00)) {
	odsy_i2cFixupAfterErr(bdata);
	goto retry_start;
    }

    /* Repeat Start condition */
    i2c_start(bdata);

    /* Send slave address, setting read bit */
    if (!i2c_sendbyte(bdata, (0xa0 | 0x01))) {
	odsy_i2cFixupAfterErr(bdata);
	goto retry_start;
    }

    /* Receive EDID structure */
    for (i = 0; i < 128; i++) {
	if (i == 127)
	    ack = 0;
	else
	    ack = 1;
	if (!i2c_master_recvbyte(bdata, &ecp[i], ack)) {
	    odsy_i2cFixupAfterErr(bdata);
	    goto bad_rtn;
	}
	if (i == 0) {
	    firstdata = ecp[0];
	} else if (i < 16) {
	    if (ecp[i] != firstdata)
		allBytesAreSame = 0;
	    if (i == 15 && allBytesAreSame) {
#ifdef DEBUG
		if (ody_dbgi2c) {
		    printf("EDID data: %02x %02x %02x ...\n",
				firstdata, firstdata, firstdata);
		}
#endif /* DEBUG */
		odsy_i2cFixupAfterErr(bdata);
		goto retry_readEDID;
	    }
	}
    }

    /* Stop condition */
    i2c_stop(bdata);
    /*
     * We don't DELAY after the i2c stop here, because we assume that
     * there will be some time before another i2c start is done.
     */

    if (bdata)
	mutex_unlock(&bdata->i2c_lock); 

    i2cinfo->cksum = odsy_EdidCksum(edidp);

#ifdef DEBUG
    if (ody_dbgi2c) {
	printf("EDID:\n");
	for (i = 0; i < sizeof(ddc_edid_t); i++) {
	    printf("%02x ", ecp[i]);
	    if ((i & 0xf) == 0xf)
		printf("\n");
	}
	printf("EDID cksum = 0x%02x\n", i2cinfo->cksum);
    }
#endif /* DEBUG */

    /*
     * So far so good.  Now examine the EDID to see if it's valid.
     * Set ddc2Bstate accordingly.
     */
    odsy_validate_ddc_data(bdata);

    return 1;				/* good return */

bad_rtn:
    if (bdata)
	mutex_unlock(&bdata->i2c_lock);
#ifdef DEBUG
    if (ody_dbgi2c)
	printf("Read EDID failure\n");
#endif /* DEBUG */
    return 0;
}

#else

int odsy_i2cProbe2b(port_handle *port)
{
    unsigned ack;
    odsy_ddinfo *info = &port->bdata->info.dd_info;
    /* XXX change all references to monitor*_edid */
    odsy_i2cinfo_t *i2cinfo = &port->bdata->i2cinfo;
    ddc_edid_t *edidp = i2cinfo->main_edid_ptr;    /* XXX - dual channel */
    unsigned char *ecp = (unsigned char *) edidp;
    int num_start_retries = 0;
    int num_readEDID_retries = 0;
    int i;

    /* Clear EDID structure */
    bzero(edidp, sizeof(ddc_edid_t));

    if(port->bdata)
    {
        ODSY_I2C_GET_LOCK(port);
    }

    odsy_i2cReset(port);

    while (num_readEDID_retries < MAX_READ_EDID_ATTEMPTS)
    {
        while (num_start_retries < MAX_START_ATTEMPTS)
        {
            /*XXX just guess on last two args */
            if (odsy_i2c_start(port,0xa0,ODSY_I2C_ACK_ASSERT) != 0)
            {
                odsy_i2cFixupAfterErr(port);
                num_start_retries++;
                continue;
            }

            /*XXX just guess on last two args */
            if (odsy_i2c_sendbyte(port,0x00,ODSY_I2C_ACK_ASSERT,ODSY_I2C_STOP_ASSERT) != 0)
            {
                odsy_i2cFixupAfterErr(port);
                num_start_retries++;
                continue;
            }

            /*XXX just guess on last two args */
            if (odsy_i2c_start(port,0xa1,ODSY_I2C_ACK_ASSERT) != 0)
            {
                odsy_i2cFixupAfterErr(port);
                num_start_retries++;
                continue;
            }

            break;
        }

        if (num_start_retries > MAX_START_ATTEMPTS)
        {
            ODSY_I2C_RELEASE_LOCK(&port);
            return 0; /* bad return */
        }

        if (odsy_i2c_long_recvbyte(port,ODSY_I2C_ACK_ASSERT,ODSY_I2C_STOP_NULL,&ecp[0]) != 0)
        {
            odsy_i2cFixupAfterErr(port);
            num_readEDID_retries++;
            continue;
        }

	for (i = 1; i < 127; i++)
	{
            if (odsy_i2c_short_recvbyte(port,&ecp[i]) != 0)
            {
                odsy_i2cFixupAfterErr(port);
                num_readEDID_retries++;
                continue;
            }
        }

        if (odsy_i2c_long_recvbyte(port,ODSY_I2C_ACK_ASSERT,ODSY_I2C_STOP_ASSERT,&ecp[i]) != 0)
        {
            odsy_i2cFixupAfterErr(port);
            num_readEDID_retries++;
            continue;
        }
    }

    ODSY_I2C_RELEASE_LOCK(port);

    i2cinfo->cksum = odsy_EdidCksum(edidp);
    odsy_validate_ddc_data(port->bdata);

    return 1; /* good return */
}
#endif

#define SONY_17(prodCode1)  ((prodCode1) == 0x10 || (prodCode1) == 0x12)
#define SONY_20(prodCode1)  ((prodCode1) == 0x01 || (prodCode1) == 0x02)
#define SONY_24(prodCode1)  ((prodCode1) == 0x11)

void odsy_validate_ddc_data(odsy_data *bdata)
{
    odsy_i2cinfo_t *i2cinfo = &bdata->i2cinfo;
    ddc_edid_t *edidp = i2cinfo->main_edid_ptr;
    volatile int err2b = 0;		/* only used for -DDEBUG */

    if (odsy_i2cValidEdid(edidp, i2cinfo)) {
	i2cinfo->ddc2Bstate = ODSY_DDC_STATE_GOOD;
	if (sgiEdid(edidp))
	    odyFixupSony24(edidp);
#ifdef DEBUG
	odyPrintEdid(edidp);
#endif
	return;
    } else {
#ifdef DEBUG
	char *tmpstr;

	odyPrintEdid(edidp);
	odyPrintBadEdid(edidp, i2cinfo);
#endif
	if (sgiEdid(edidp)) {

	    int maxHSiz, maxVSiz, pc0, pc1;

	    /*
	     * Probably an SGI monitor, but we can't tell for sure,
	     * since there's an error in the EDID data.
	     *
	     * If importatnt info is OK in EDID, then
	     * hack up EDID data so that X will see good EDID info.
	     *
	     * NOTE: Sony 20" N2 GDM 20E21 monitors are KNOWN to return
	     * unreliable EDID data.
	     */

	    odyFixupSony24(edidp);

	    maxHSiz = edidp->MaxHImageSize;
	    maxVSiz = edidp->MaxVImageSize;
	    pc0 = (int) edidp->ProductCode[0];
	    pc1 = (int) edidp->ProductCode[1];

	    if (pc0 != 0x00 ||
		(!SONY_17(pc1) && !SONY_20(pc1) && !SONY_24(pc1))) {
#ifdef DEBUG
		cmn_err(CE_CONT, "Invalid EDID SGI ProductCode\n");
#endif

	    } else if ( (SONY_17(pc1) && (maxHSiz != 0x21 || maxVSiz != 0x18))
		 || (SONY_20(pc1) && (maxHSiz != 0x26 || maxVSiz != 0x1d))
		 || (SONY_24(pc1) && (maxHSiz != 0x30 || maxVSiz != 0x1f)) )
	    {
#ifdef DEBUG
		cmn_err(CE_CONT, "EDID might contain bad monitor size data\n");
		/* Actually, size might be right, but product code wrong */
#endif

	    } else {
		/*
		 * EDID not perfect, but good enough.
		 */
		if (!(edidp->EstablishedTimings2 & 0x01)) {
		    /* HACK - Fix up bad EstablishedTimings2 */
		    edidp->EstablishedTimings2 |= 0x01;
		}
		if (!i2cinfo->main_edid_stat) {
		    /* HACK - Fix up bad EDID hdr */
		    static unsigned char valid_EDID_hdr[8] =
			{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
		    bcopy(valid_EDID_hdr, &edidp->Header[0], 8);
		}

		i2cinfo->ddc2Bstate = ODSY_DDC_STATE_GOOD;
		err2b = 1;
	    }
	}
#ifdef DEBUG
	tmpstr = err2b ? ", but usable" : "";
	cmn_err(CE_CONT, "DDC Monitor probe found bad%s EDID data\n", tmpstr);
#endif
    }
    return;
}

unsigned short odsy_EdidCksum(ddc_edid_t *edidp)
{
    int i;
    unsigned char *ccp = (unsigned char *) edidp;
    unsigned char cksum = 0;

    for (i = 0; i < 128; i++) {
	cksum += ccp[i];
    }
    return (unsigned short) cksum;
}

int odsy_i2cValidEdid(ddc_edid_t *edidp, odsy_i2cinfo_t *i2cinfo)
{
    unsigned char *hdr = (unsigned char *) &edidp->Header[0];
    short validHdr;

    if (hdr[0] == 0x00 &&
	hdr[1] == 0xff &&
	hdr[2] == 0xff &&
	hdr[3] == 0xff &&
	hdr[4] == 0xff &&
	hdr[5] == 0xff &&
	hdr[6] == 0xff &&
	hdr[7] == 0x00) {
	    validHdr = 1;
    } else {
	    validHdr = 0;
    }
    i2cinfo->validHdr = validHdr;

    if (validHdr && i2cinfo->cksum == 0x00)
	return 1;
    return 0;
}

int sgiEdid(ddc_edid_t *edidp)
{
    unsigned char *mfg = (unsigned char *) &edidp->ManufacturerName[0];

    /* If mfg == SGX or SGI ... */
    if (mfg[0] == 0x4c && (mfg[1] == 0xf8 || mfg[1] == 0xe9))
	return 1;
    return 0;
}

#ifdef DEBUG
static void odyPrintEdid(ddc_edid_t *edidp)
{
	if (ody_dbgi2c) {
	    unsigned int i;
	    unsigned char *chp = (unsigned char *) edidp->ManufacturerName;
	    long MoM = edidp->WeekOfManufacture * 7;
	    static int DiM[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

	    for (i = 0; (MoM > 0 && i < 12); i++)
		MoM -= DiM[i];
	    MoM = i;

	    cmn_err(CE_CONT,
 "%c%c%c DDC Monitor (ver %d.%d), prod code %d, ser# %d, %d/%d (%dx%d mm)\n",
		((chp[0] & 0x7c) >> 2) + 64,
		(((chp[0] & 0x03) << 3) | ((chp[1] & 0xe0) >> 5)) + 64,
		(chp[1] & 0x1f) + 64,
		edidp->Version,
		edidp->Revision,
		(u_long) edidp->ProductCode[0] +
		((u_long) edidp->ProductCode[1] << 8) & 0xff00,
		((u_long) edidp->SerialNum[0] & 0xff) |
		(((u_long) edidp->SerialNum[1] << 8) & 0xff00) |
		(((u_long) edidp->SerialNum[2] << 16) & 0xff0000) |
		(((u_long) edidp->SerialNum[3] << 24) & 0xff000000),
		MoM,
		((u_long) edidp->YearOfManufacture) + 1990,
		((u_long) edidp->MaxHImageSize) * 10,
		((u_long) edidp->MaxVImageSize) * 10);
	} else {
	    cmn_err(CE_CONT, "DDC Monitor found, serial# %d\n",
		((u_long) edidp->SerialNum[0] & 0xff) |
		(((u_long) edidp->SerialNum[1] << 8) & 0xff00) |
		(((u_long) edidp->SerialNum[2] << 16) & 0xff0000) |
		(((u_long) edidp->SerialNum[3] << 24) & 0xff000000));
	}
}

/* Print critical EDID info which might be wrong */
static void odyPrintBadEdid(ddc_edid_t *edidp, odsy_i2cinfo_t *i2cinfo)
{
	unsigned int i;
	unsigned char *hdr = (unsigned char *) edidp->Header;
	unsigned char *mfg = (unsigned char *) edidp->ManufacturerName;
	unsigned char *pc  = (unsigned char *) edidp->ProductCode;

	if (!i2cinfo->validHdr) {
		cmn_err(CE_CONT, "EDID header: ");
		for (i = 0; i < 8; i++) {
			cmn_err(CE_CONT," 0x%02x", hdr[i]);
		}
		cmn_err(CE_CONT, "\n");
	}
	cmn_err(CE_CONT, "EDID Manuf: 0x%02x 0x%02x (%c%c%c)", mfg[0], mfg[1],
		((mfg[0] & 0x7c) >> 2) + 64,
		( ((mfg[0] & 0x03) << 3) | ((mfg[1] & 0xe0) >> 5) ) + 64,
		(mfg[1] & 0x1f) + 64);
	cmn_err(CE_CONT, ", prodCode: 0x%02x 0x%02x (%d)", pc[0], pc[1],
		(u_long) pc[0] + (((u_long) pc[1]) << 8));
	if (!ody_dbgi2c) {
		cmn_err(CE_CONT, ", size: (%dx%d mm)",
			edidp->MaxHImageSize * 10, edidp->MaxVImageSize * 10);
	}
	cmn_err(CE_CONT, ", cksum: 0x%02x", i2cinfo->cksum);
	if (!(edidp->EstablishedTimings2 & 0x01)) {
		cmn_err(CE_CONT, ", EstTimings2: 0x%02x",
					edidp->EstablishedTimings2);
	}
	cmn_err(CE_CONT, "\n");
}

#endif /* DEBUG */

/*
 * The first 100 seial nos. of the Sony 24" monitor have a bad product code.
 * The prod code was "0x00 0x01", but should be "0x00 0x11".
 */
void odyFixupSony24(ddc_edid_t *edidp)
{
    if (edidp->ProductCode[0] == 0x00 && edidp->ProductCode[1] == 0x01 &&
	/* 0x30 = 48 cm, which is width of 24" monitor */
	edidp->MaxHImageSize == 0x30) {
	    unsigned int cksum_incr = 0x11 - 0x01;
	    edidp->ProductCode[1] = 0x011;
	    /*
	     * Alter EDID->Checksum byte; computed checksum remains same.
	     */
	    edidp->Checksum -= cksum_incr;
#ifdef DEBUG
	    cmn_err(CE_CONT,
	    "Early Sony 24\" EDID product code found, changing to 0x00 0x11\n");
#endif /* DEBUG */
#if defined(DEBUG) && DEBUG_FIXUP24
	    {
		unsigned int new_cksum = odsy_EdidCksum(edidp);
		cmn_err(CE_CONT, "new EDID prod code = 0x%02x 0x%02x\n",
			edidp->ProductCode[0], edidp->ProductCode[1]);
		cmn_err(CE_CONT, "new EDID Checksum byte = 0x%02x\n",
							edidp->Checksum);
		/* new_cksum should be the same as i2cinfo->cksum */
		cmn_err(CE_CONT, "new EDID computed cksum = 0x%02x\n",
							new_cksum);
	    }
#endif /* DEBUG && DEBUG_FIXUP24 */
    }
}

unsigned char odsy_i2cRcvBuf[MAX_DDC2AB_MSGLEN + 1];
unsigned char odsy_i2cCmdBuf[MAX_DDC2AB_MSGLEN + 1];

int odsy_copyin_cmd(struct odsy_brdmgr_i2c_args *ap, unsigned char **bufptr)
{
    int len = ap->cmd_len;

    if (len <= 0 || len > MAX_DDC2AB_MSGLEN)
	return -EINVAL;

    if (len == 1) {
	odsy_i2cCmdBuf[0] = *ap->cmd;
    } else {
	if (!ap->cmd)
	    return -EINVAL;
	if (copyin(ap->cmd, odsy_i2cCmdBuf, len)) {
	    return -EFAULT;
	}
    }
    *bufptr = &odsy_i2cCmdBuf[0];
    return 0;
}

#ifdef DEBUG
static int force2abInitRtn = 0;
static int force2abCmdRtn = 0;
#endif

int odsy_i2c_cmd(odsy_data *bdata, struct odsy_brdmgr_i2c_args *ap, void *arg)
{
    unsigned char *cmdptr;
    int rval = 0, n, nbytes, max_bytes, ack;
    int cmdtype = ap->cmd_type;
    port_handle port;
    unsigned char odsy_i2cRecvBuf[MAX_I2C_MSGLEN + 1];

    /*
     * Init port handle. For right now set it to OPT port.
     * Eventually we want to check to see if the 'dc' board
     * is installed then pick a port based on that.
     */
     odsy_i2c_init_port_handle(&port, bdata, ODSY_I2C_MAIN_PORT);

    /*
     * Don't allow a command to come thru here that depends on
     * ok_to_stretch_clk, esp. if ODSY_2AB_INITIALIZE is in progress.
     */
    ODSY_I2C_GET_LOCK(&port);

    switch(cmdtype) {
	case ODSY_I2C_RESET:
	    odsy_i2cReset(&port);
	    break;

	case ODSY_I2C_START:
	    if ((rval = odsy_copyin_cmd(ap, &cmdptr)) < 0)
            {
                break;
            }

	    odsy_i2c_start(&port, cmdptr[0], ap->ack);
	    break;

	case ODSY_I2C_SENDBYTES:
	    if ((rval = odsy_copyin_cmd(ap, &cmdptr)) < 0)
            {
                break;
            }
	    nbytes = ap->cmd_len;

	    for (n = 0; n < nbytes - 1; n++)
            {
	        /*XXX- sending ack assert and stop null by default. Eventually need to change */
	        if ((odsy_i2c_sendbyte(&port, cmdptr[n], ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL)) != 0)
	        {
	            break;
	        }
            }

            if (odsy_i2c_sendbyte(&port, cmdptr[n], ap->ack, ODSY_I2C_STOP_NULL) != 0)
	    {
	        break;
	    }

	    rval = (n < nbytes) ? -EIO : nbytes;
	    break;

	case ODSY_I2C_RECVBYTES:
	    max_bytes = 128;

	    if (nbytes <= 0 || !ap->reply || nbytes > max_bytes) 
            {
                rval = -EINVAL;
                break;
            }

	    for (n = 0; n < nbytes - 1; n++)
	    {
	        if (odsy_i2c_long_recvbyte(&port, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL, &odsy_i2cRecvBuf[n]) != 0)
	        {
	            break;
	        }
	    }

	    if (odsy_i2c_long_recvbyte(&port, ap->ack, ODSY_I2C_STOP_NULL, &odsy_i2cRecvBuf[n]) != 0)
	    {
	        break;
	    }

	    if (n > 0 && copyout(odsy_i2cRecvBuf, ap->reply, n)) 
	    {
                rval = -EFAULT;
                break;
            }
	    ap->reply_len = n;

            /* copy new value of ap->reply_len to user's space */
            if (copyout(ap, arg, sizeof(*ap))) {
                rval = -EFAULT;
                break;
            }
            rval = (n < nbytes) ? -EIO : nbytes;

	    break;

	case ODSY_I2C_BECOME_SLAVE_RECV:
	    /*XXX- not sure what should go here right now */
	    break;

	case ODSY_I2C_STOP:
	    odsy_i2c_stop(&port);
	    break;

	case ODSY_2B_PROBE:
	    /* Get EDID */

	    /* XXX - */

	    odsy_i2cProbe2b(&port);
	    break;

	case ODSY_2AB_INITIALIZE:
#ifdef DEBUG
	    if (force2abInitRtn) {
		rval = -force2abInitRtn;
		break;
	    }
#endif
	    if (!odsy_Probe2AB(&port, odsy_i2cRcvBuf)) {
		rval = -EIO;
	    }
	    break;

	case ODSY_2AB_CMD:
#ifdef DEBUG
	    if (force2abCmdRtn) {
		rval = -force2abCmdRtn;
		break;
	    }
#endif
	    /* Send cmd, and get possible reply */
	    if ((rval = odsy_copyin_cmd(ap, &cmdptr)) < 0)
		break;
	    rval = odsy_2abCmdWrapper(&port, ap, cmdptr, odsy_i2cRcvBuf);

	    if (ap->reply) {
		int rlen = ap->reply_len;

		/* copy actual reply to user's space */
		if (rlen > 0 && copyout(odsy_i2cRcvBuf, ap->reply, rlen)) {
		    rval = -EFAULT;
		    break;
		}

		/* copy new value of ap->reply_len to user's space */
		if (copyout(ap, arg, sizeof(*ap))) {
		    rval = -EFAULT;
		    break;
		}
	    }
	    break;
#ifdef NOTYET
	case ODSY_2BI_INITIALIZE:
	    break;
	case ODSY_2BI_CMD:
	    break;
#endif
    }

    ODSY_I2C_RELEASE_LOCK(&port);

    return rval;
}

#ifdef DEBUG
static void printRawReply(unsigned char *s, int n)
{
    int i;
    printf("DDC2AB response:");
    for (i = 0; i < n; i++) {
	printf(" %02x", s[i]);
	if ((i & 0x0f) == 0xf)
	    printf("\n");
    }
    if (((i-1) & 0x0f) != 0xf)
	printf("\n");
}

static void printReply(unsigned char *s, int n)
{
    if (s[0] == DDC2AB_ATTENTION)
	printf("DDC2AB ATTENTION!\n");
    else if (s[0] == DDC2AB_ID_REPLY)
	printf("ID reply: %s\n", &s[1]);
    else
	printRawReply(s, n);
}
#endif /* DEBUG */

static unsigned char hostAddr2ab = 0x50;
static unsigned char monitorAddr2ab = DDC2AB_DEFAULT_MONITOR_ADDR;

static int probe_delay = 5000;		/* 1/200 sec */

#define MAX_TICKS	30

int odsy_Probe2AB(port_handle *port, unsigned char *rcvBuf)
{
	int n, j, k, ticks, replyLen, len;
	unsigned char *cmdBuf = &odsy_i2cCmdBuf[0];
	unsigned char id_reply[MAX_DDC2AB_MSGLEN + 1];
	int id_len, newAddr, changeStretchOK;

	odsy_i2cReset(port);

	for (ticks = 0; ticks < MAX_TICKS; ) {
	    *cmdBuf = DDC2AB_RESET;
	    for (j = 0; j < 3; j++) {
		ticks++;
		if (odsy_2abSendCmd(port, 1, cmdBuf, 0))
		    break;
	    }
	    if (j == 3) {	/* If none of above SendCmd's succeeded */
		odsy_i2c_stop(port);
		DELAY(probe_delay);
		ticks++;
		continue;
	    }
	    n = odsy_2abGetReply(port, rcvBuf, &replyLen);
	    if (n <= 0) {
#ifdef DEBUG
		char *msg = n < 0 ? "Error" : "Timeout";
		if (ody_dbgi2c >= 2)
		    printf("%s receiving DDC2AB response\n", msg);
#endif
		odsy_i2c_stop(port);
		DELAY(probe_delay);
		if (n < 0)
		    ticks++;
		continue;
	    }
	    /* Here n > 0.  I.e. we got a reply */
#ifdef DEBUG
	    if (ody_dbgi2c >= 2)
		printReply(rcvBuf, replyLen);
#endif

	    if (replyLen == 1 && rcvBuf[0] == DDC2AB_ATTENTION) {
		/*
		 * Reset implies addr is now 0x6e.
		 */
		if (monitorAddr2ab == DDC2AB_MONITOR_ADDR)
		    monitorAddr2ab = DDC2AB_DEFAULT_MONITOR_ADDR;
		/*
		 * Get DDC2AB ID, and
		 * do ASSIGN_ADDRESS to change monitor addr from 0x6e.
		 */
		DELAY(probe_delay);
		for (j = 0; j < 2; j++) {   /* retry once if not successful */

		    DELAY(probe_delay);
		    ticks++;

		    cmdBuf[0] = DDC2AB_ID_REQUEST;
		    id_len = 0;
		    if (odsy_2abSendCmd(port, 1, &cmdBuf[0], 0)) {
			n = odsy_2abGetReply(port, rcvBuf, &replyLen);
			    if (n > 0 && n <= MAX_DDC2AB_MSGLEN) {
				id_len = n - 1;
				bcopy(&rcvBuf[1], id_reply, id_len);
#ifdef DEBUG
				if (ody_dbgi2c >= 2)
				    printReply(rcvBuf, id_len);
#endif
			    }
		    }
		    if (id_len && rcvBuf[0] == DDC2AB_ID_REPLY) {
			/*
			 * ID reply received
			 */
			DELAY(probe_delay);
			ticks++;
			cmdBuf[0] = DDC2AB_ASSIGN_ADDRESS;
			bcopy(id_reply, &cmdBuf[1], id_len);
			len = id_len + 2;
			newAddr = DDC2AB_MONITOR_ADDR;
			cmdBuf[len - 1] = newAddr;
			if (odsy_2abSendCmd(port, len, cmdBuf, 1)) {
			    monitorAddr2ab = newAddr;
			    /*
			     * Although we got here OK, future replies from a
			     * Sony N2 monitor will likely fail if we stretch
			     * CLK low when receiving stream of bytes as slave.
			     */
			    if (changeStretchOK) {
				// ok_to_stretch_clk = 1;
				for (k = 0; k < NUM_N2_MONITORS; k++) {
				    if (strncmp(n2_reply_n2[k],
						(char *) &id_reply[0],
						ID_REPLY_SIZE_N2) == 0) {
					// ok_to_stretch_clk = 0;
					break;
				    }
				}
			    }
			    return 1;
			}
		    }
		} /* for (.. j < 2 ...) */

		if (ticks < MAX_TICKS)
		    DELAY(probe_delay);
	    }
	}

	monitorAddr2ab = DDC2AB_DEFAULT_MONITOR_ADDR;

	return 0;
}

static int maxWrapAttempts = 4;

/*
 * Send ddc2ab cmd, possibly get reply
 */
int odsy_2abCmdWrapper(port_handle *port,
    struct odsy_brdmgr_i2c_args *ap, unsigned char *cmdptr, unsigned char *rcvBuf)
{
    int n, rval, replyLen, sendStop;
    int num_attempts = 0;

    do {
	rval = 0;		/* initialize return value */
	ap->reply_len = 0;	/* initialize */

	sendStop = ap->reply ? 0 : 1;
	if (odsy_2abSendCmd(port, ap->cmd_len, cmdptr, sendStop)) {
	    /*
	     * command succeeded
	     */
	    if (ap->reply) {
		n = odsy_2abGetReply(port, rcvBuf, &replyLen);
		ap->reply_len = replyLen;
		if (n > 0) {
		    rval = n;		/* GetReply succeeded */
		} else if (n <= 0) {
		    if (n == 0)		/* GetReply returns 0 to mean timeout */
			rval = -ETIMEDOUT;
		    else
			rval = -EIO;
		    /*
		     * Reply failed.  Retry sending cmd / getting reply,
		     * but do so at most maxWrapAttempts times.
		     */
		    if (num_attempts > maxWrapAttempts)
			break;
		    else if (num_attempts == 0)
			DELAY(delayBefore2abRetry);
		    else if (num_attempts == 1)
			DELAY(delayBefore2abRetry1);
		    else
			DELAY(delayBefore2abRetry2);
		}
	    }
	} else {
	    /*
	     * Command failed, retry at most once
	     */
	    rval = -EIO;
	    if (num_attempts > 0)
		break;
	}
	++num_attempts;
    } while (rval < 0);

    return rval;
}

int odsy_2abSendCmd(port_handle *port, int len, unsigned char *cmd, int sendStop)
{
    unsigned char lenb, cksum, tmpb;
    int i;
    int num_start_retries = 0, num_nacks = 0;

#ifdef DEBUG
    if (ody_dbgi2c >= 2) {
	printf("2abSendCmd (begin) %02x %02x %02x %02x\n",
	     monitorAddr2ab, hostAddr2ab, 0x80 + len, cmd[0]);
    }
#endif

/*XXX will unmark as soon as i2c_wait_for_bus has been implemented in the driver */
#if 0
    if (!i2c_wait_for_bus(port->bdata))
	return 0;
#endif

retry_start:
    if (++num_start_retries > 3) {
	unsigned char ab_cmd = cmd[0];	/* we know len >= 1 */
	if (num_nacks == 3 && ab_cmd == DDC2AB_RESET) {
	    /*
	     * If we don't do the following, we'd we'd have to power off
	     * the monitor (to reset it to addr 0x6e) on every reboot.
	     */
	    if (monitorAddr2ab == DDC2AB_MONITOR_ADDR) {
		monitorAddr2ab = DDC2AB_DEFAULT_MONITOR_ADDR;
	    } else {
		monitorAddr2ab = DDC2AB_MONITOR_ADDR;
	    }
	    num_start_retries = 0;
	    goto retry_start;
	}
#ifdef DEBUG
	if (ody_dbgi2c >= 2)
	    printf("2abSendCmd failed\n");
#endif
	return 0;
    }

	/* Send start */
    if (odsy_i2c_start(port,monitorAddr2ab,ODSY_I2C_ACK_ASSERT) != 0)
    {
#ifdef DEBUG
	if (ody_dbgi2c >= 2)
	    printf("2abSendCmd: error sending monitor addr\n");
#endif
	num_nacks++;
	odsy_i2cFixupAfterErr(port);
	goto retry_start;
    }

	/* Send host addr */
    if (odsy_i2c_sendbyte(port, hostAddr2ab, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2)
	    printf("2abSendCmd: error sending host addr\n");
#endif
	odsy_i2cFixupAfterErr(port);
	goto retry_start;
    }

	/* Send length */
    lenb = 0x80 + len;
    if (odsy_i2c_sendbyte(port, lenb, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2)
	    printf("2abSendCmd: error sending length\n");
#endif
	odsy_i2cFixupAfterErr(port);
	goto retry_start;
    }

    cksum = monitorAddr2ab ^ hostAddr2ab;
    cksum = cksum ^ lenb;

	/* Send cmd body */
    for (i = 0; i < len; i++) {
	tmpb = cmd[i];
	if (odsy_i2c_sendbyte(port, tmpb, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0) {
#ifdef DEBUG
	    if (ody_dbgi2c >= 2)
		printf("2abSendCmd: error sending cmd body\n");
#endif
	    odsy_i2cFixupAfterErr(port);
	    goto retry_start;
	}
#ifdef DEBUG
	if (i > 0 && ody_dbgi2c >= 2) {
	    printf(" %02x", tmpb);
	    if ((i & 0x0f) == 0xf)
		printf("\n");
	}
#endif
	cksum = cksum ^ tmpb;
    }

#ifdef DEBUG
    if (ody_dbgi2c >= 2) {
	printf(" %02x (end)\n", cksum);
    }
#endif
	/* Send cksum */
    if (odsy_i2c_sendbyte(port, cksum, ODSY_I2C_ACK_ASSERT, ODSY_I2C_STOP_NULL) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2)
	    printf("2abSendCmd: error sending cksum\n");
#endif
	odsy_i2cFixupAfterErr(port);
	goto retry_start;
    }

    /*
     * If cmd takes a reply, stop is sent by 2abGetReply.
     */
    if (sendStop)
	odsy_i2c_stop(port);

    return 1;
}

#ifdef DEBUG

#define TICK_TYPE_N	0
#define TICK_TYPE_STOP	1
static int tickType;
static int rbitnum;
#define STEPS_AFTER_ACK	1
static int nticks[MAX_DDC2AB_MSGLEN * (2 * 9 + STEPS_AFTER_ACK)];
static int stopPhase;
static int stopticks[2];

/*
 * printTicks tells how many iterations slave_receive took in looking for
 * CLK low, CLK hi, etc.  One line is printed for each byte.
 */
static void printTicks(void)
{
    int i, j, b;

    if (rbitnum == 0) {
	printf("no ticks\n");
    } else {
	printf("nticks:\n");
	for (b = 0, i = 0; i < MAX_DDC2AB_MSGLEN; i++) {
	    for (j = 0; j < (2 * 9) + STEPS_AFTER_ACK; j++) {
		if (j > 0 && (j & 0x1) == 0)
		    printf(" /");
		printf(" %d", nticks[b]);
		if (++b >= rbitnum) {
		    printf("\n");
		    goto next;
		}
	    }
	    printf("\n");
	}
    }
next:
    if (stopPhase > 0) {
	printf("stop ticks: %d", stopticks[0]);
	if (stopPhase > 1)
	    printf(" %d", stopticks[1]);
    }
    printf("\n");
    return;
}
#endif /* DEBUG */

/*
 * odsy_2abGetReply
 *  returns:	-1 if error
 *		 0 if no reply (i.e. timeout)
 *		 reply_len if good reply received
 *  NOTE: partial reply_len argument is returned even if return val = -1
 */
int odsy_2abGetReply(port_handle *port, unsigned char *bodyp, int *reply_len)
{
    unsigned char tmp, cksum;
    int i, j, len, err_stat;
#ifdef DEBUG
    int stopFound;
    unsigned char hostAddrByte, monAddrByte, lenSave;

    rbitnum = 0;
    stopPhase = 0;
#endif

    /*
     * Send stop at end of previous command.
     *
     * It's important that there be no delay after the following i2c stop,
     * since 2abGetReply nees to start looking for bytes right away.
     */
    odsy_i2c_stop(port);

    *reply_len = 0;

    /* get host (i2c slave) addr */

    if ((err_stat = (odsy_i2c_becomeSlaveRecv(port, ODSY_I2C_ACK_ASSERT,(HOST2ABADDR >> 1), &tmp))) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2) {
	    printf("2abGetReply: error reading host addr\n");
	    printTicks();
	}
#endif
	odsy_i2cFixupAfterErr(port);
	if (err_stat == ODSY_I2C_TIMEOUT)
	    return 0;
	else	/* err_stat == STATUS_ERR */
	    return -1;
    }
    if (tmp & 0x1) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2) {
	    printf("2abGetReply: received write slave addr = 0x%02x\n", tmp);
	    printTicks();
	}
#endif
	odsy_i2cFixupAfterErr(port);
	return -1;
    }
#ifdef DEBUG
    hostAddrByte = tmp;
#endif
    cksum = tmp;

    /* get monitor addr */
    if (odsy_i2c_slave_recvbyte(port, ODSY_I2C_ACK_ASSERT, (short)1, &tmp) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2) {
	    printf("2abGetReply: host addr = 0x%02x\n", hostAddrByte);
	    printf("2abGetReply: error reading monitor addr, stat = %d\n", err_stat);
	    printTicks();
	}
#endif
	odsy_i2cFixupAfterErr(port);
	return -1;
    }
    
#ifdef DEBUG
    monAddrByte = tmp;
#endif
    cksum = cksum ^ tmp;

    /* get length */
    /*XXX-128 is just a guess and ack_assert may need to be ack_null?*/
    if (odsy_i2c_slave_recvbyte(port, ODSY_I2C_ACK_ASSERT,128, &tmp) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2) {
	    printf("2abGetReply: host addr = 0x%02x\n", hostAddrByte);
	    printf("2abGetReply: monitor addr = 0x%02x\n", monAddrByte);
	    printf("2abGetReply: error reading len, stat = %d\n", err_stat);
	    printTicks();
	}
#endif
	odsy_i2cFixupAfterErr(port);
	return -1;
    }
    /*
     * If (tmp & 0x80) == 0), this is a "device data stream msg."
     */
    cksum = cksum ^ tmp;
    len = (int) (tmp & 0x7f);
#ifdef DEBUG
    lenSave = tmp;
#endif

    /* get message body */
    for (i = 0; i < len; i++) {
	/* read byte */
        /*
         * XXX may not need to do a for loop here, since slave_recv reads all the bytes into
         * the buffer. Also ack_assert may need to be ack_null.
         */
	if (odsy_i2c_slave_recvbyte(port, ODSY_I2C_ACK_ASSERT, len, &tmp) != 0) {
#ifdef DEBUG
	    if (ody_dbgi2c >= 2) {
		printf("2abGetReply: error reading data byte, stat = %d\n", err_stat);
		printf("2abGetReply: bytes read: %02x %02x %02x\n",
					hostAddrByte, monAddrByte, lenSave);
		for (j = 0; j < i; j++) {
		    printf(" %02x", bodyp[j]);
		    if ((j & 0x0f) == 0xf)
			printf("\n");
		}
		printf(" %02x\n", tmp);
		printTicks();
	    }
#endif
	    odsy_i2cFixupAfterErr(port);
	    return -1;
	}
	cksum = cksum ^ tmp;
	bodyp[i] = tmp;
	(*reply_len)++;
    }

    /* Now get the checksum */
    /*
     *XXX just guessing at the length of the buffer (I think that's right though)
     * and ack_assert may need to be ack_null.
     */
    if (odsy_i2c_slave_recvbyte(port, ODSY_I2C_ACK_ASSERT, 1, &tmp) != 0) {
#ifdef DEBUG
	if (ody_dbgi2c >= 2) {
	    printf("2abGetReply: error reading cksum, stat = %d\n", err_stat);
	    printTicks();
	}
#endif
	odsy_i2cFixupAfterErr(port);
	return -1;
    }


#ifdef DEBUG
    stopFound = 1; /*totally bogus value (1). Should be = to wait_for_stop. */
#endif
/*XXX Will unmark as soon as wait_for_stop is implemented in the odsy driver */
#if 0
    wait_for_stop(bdata);
#endif

    DELAY(delayAfterGetReply);		/* in case stop not found */

#ifdef DEBUG
    if (ody_dbgi2c >= 2) {
	printf("2abGetReply: host addr = 0x%02x\n", hostAddrByte);
	printf("2abGetReply: monitor addr = 0x%02x\n", monAddrByte);
	printf("2abGetReply: protocol/length = 0x%02x\n", lenSave);
	printRawReply(bodyp, len);
	printf("2abGetReply: cksum = 0x%02x, computed cksum = 0x%02x (end)\n",
								tmp, cksum);
	if (stopFound) {
	    int totTicks = stopticks[0] + stopticks[1];
	    printf("2abGetReply: stop detected after %d ticks\n", totTicks);
	} else
	    printf("2abGetReply: stop not detected\n");
    }
    if (ody_dbgi2c > 2)
	printTicks();
#endif

    if (cksum != tmp)
	return -1;

    return *reply_len;
}

#endif  /* !defined( _STANDALONE ) */
