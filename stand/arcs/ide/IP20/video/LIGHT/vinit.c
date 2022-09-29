#include "uif.h"
#include "sv.h"
#include "ide_msg.h"

unsigned char svc1_revid;  /* SVC1 chip ID and revision number */

/****************************************************************************
 * DMSD initialization
 ****************************************************************************/

				     /* ----------------------- */
static struct	aDMSD		     /* I2Caddr - NTSC  -  PAL	*/
	{			     /* ----------------------- */
	long	dmsdRegIDEL;		/* 0x00 - 0x50	- 	*/
	long	dmsdRegHSYb50;		/* 0x01 - 0x30	- 	*/
	long	dmsdRegHSYe50;		/* 0x02 - 0x00	- 	*/
	long	dmsdRegHCLb50;		/* 0x03 - 0xE8	- 	*/
	long	dmsdRegHCLe50;		/* 0x04 - 0xB6	- 	*/
	long	dmsdRegHSP50;		/* 0x05 - 0xF4	- 	*/
	long	dmsdRegLUMA;		/* 0x06 - 0x01	- 	*/
	long	dmsdRegHUE;		/* 0x07 - 0x00	- 	*/
	long	dmsdRegCKqam;		/* 0x08 - 0xF8	- 	*/
	long	dmsdRegCKsecam;		/* 0x09 - 0xF8	- 	*/
	long	dmsdRegSENpal;		/* 0x0A - 0x90	- 	*/
	long	dmsdRegSENsecam;	/* 0x0B - 0x90	- 	*/
	long	dmsdRegGC0;		/* 0x0C - 0x00	- 	*/
	long	dmsdRegSTDmode;		/* 0x0D - 0x00	- 	*/
	long	dmsdRegIOclk;		/* 0x0E - 0x79	- 	*/
	long	dmsdRegCTRL3;		/* 0x0F - 0x91	- 	*/
	long	dmsdRegCTRL4;		/* 0x10 - 0x00	- 	*/
	long	dmsdRegCHCV;		/* 0x11 - 0x2C	- 0x59	*/

	long	dmsdTerminate1;		/* XXXX - 0xFFFF	*/

	long	dmsdRegHSYb60;		/* 0x14 - 0x34	- 	*/
	long	dmsdRegHSYe60;		/* 0x15 - 0x0A	- 	*/
	long	dmsdRegHCLb60;		/* 0x16 - 0xF4	- 	*/
	long	dmsdRegHCLe60;		/* 0x17 - 0xCE	- 	*/
	long	dmsdRegHSP60;		/* 0x18 - 0xF4	- 	*/

	long	dmsdTerminate2;		/* XXXX - 0xFFFF	*/

	long	videoRate;		/* XXXX			*/
	long	videoStandard;		/* XXXX			*/
	} Dmsd_Regs = 
		{
		0x50, 0x30, 0x00, 0xE8,
		0xB6, 0xF4, 0x01, 0x00,
		0xF8, 0xF8, 0x90, 0x90,
		0x00, 0x00, 0x79, 0x91,

		0x00, 0x2C,

		0xFFFFFFFF,

		0x34, 0x0A, 0xF4, 0xCE,
		0x21,
		0xFFFFFFFF,

		0x00, 0x00,		
		};

/*
 * Transmit a -1 terminated array on the I2C bus by writing
 * each word, checking each time that the previous write
 * terminated.   If any write times out, return status so
 * that the caller can recover.
 */
static int
send_I2C_array(long *array)
{
    int	i;
    int	status;

    for (i = 0; array[i] != 0xffffffff; i++) {
	if ((status = writeI2Cdata(array[i])) != NO_ERROR) {
	    return (status);
	}
    }

    return (NO_ERROR);
}

/*
 * Send a message to an I2C bus slave, by setting up the address
 * and subaddress of the slave, and then transmitting the array.
 * If either the array transmission times out, or fails to
 * terminate correctly, return bad status.
 */
send_slave_I2C_msg(long addr, long subaddr, long *data)
{
    int     status;

    /*
     * Set up the address of the I2C slave.
     */
    writeI2Cctrl(SV_I2C_SLV_ADDR_REG);
    writeI2CdataNP(addr);	/* set I2C slave address */ 

    /*
     * Set up the subaddress of the I2C slave.
     */
    writeI2Cctrl(SV_I2C_SUB_ADDR_REG);
    if ((status = writeI2Cdata(subaddr)) != NO_ERROR) {
	return (status);
    }

    /*
     * Send the data to the slave.
     */
    if ((status = send_I2C_array(data)) != NO_ERROR) {
       return (status);
    }
    if ((status = pollI2C()) != NO_ERROR) { 
	return (status);
    }
    /*
     * Set STOP.
     */
    writeI2Cctrl(SV_I2C_STOP);

    return (NO_ERROR);
}


/*
 * initalise the Digital MultiStandard Decoder by sending
 * it its initial frequency.
 */
int
initDMSD(void)
{
    int	status;

    if ((status = send_slave_I2C_msg(DMSD_ADDR, DMSD_PAGE1,
	    &(Dmsd_Regs.dmsdRegIDEL))) != NO_ERROR) {
	    return (status);
    }

    if ((status = send_slave_I2C_msg(DMSD_ADDR, DMSD_PAGE2,
	    &(Dmsd_Regs.dmsdRegHSYb60))) != NO_ERROR) {
	    return (status);
    }

    return (NO_ERROR);
}


/****************************************************************************
 * video board initialization
 ****************************************************************************/

/*
 * Default initial setting for sv1.
 */
void
init_SV1_regs(void)
{
    writeParallelData(SV_FUNC_CTL, SV_FUNC_CTL_INIT);
    writeParallelData(SV_CLOCK_MODE, SV_CLOCK_INIT_RESET);
    writeParallelData(SV_CLOCK_MODE, SV_CLOCK_INIT_GO);
    writeParallelData(SV_BUS_CTL, SV_BUS_INIT);
    writeParallelData(SV_DMACTL, SV_DMA_INIT);
}

/*
 *  probe I2C device
 */
int
I2Cprobe(long theAddr)
{
    int tmp;

    writeI2Cctrl(SV_I2C_SLV_ADDR_REG);
    writeI2CdataNP(theAddr);	/* set I2C slave address */ 

    writeI2Cctrl(SV_I2C_SUB_ADDR_REG);
    writeI2Cdata(0);	/* set arbitrary I2C slave sub-address */

    writeI2Cdata(0xff);	/* send dummy data */
    tmp = pollI2C();	/* wait 'till last data gone */
    if (tmp)
	return tmp;

    tmp = theSVboard[SV_I2C_CS_CTRL];
    
#ifdef I2C_DEBUG
    status_poll[poll_num++] = (unsigned char)tmp;
#endif

    writeI2Cctrl(SV_I2C_STOP);		/* set STOP condition */

    return (tmp & SV_I2C_LRB);
}

 
/*
 *  initialize the Starter Video board including the SV1 controller ASIC
 *  and the I2C bus
 */
int
initsv(void)
{
    long index, dummydat;
    int errorflag = 0;

    /*
     *  init the SV1 control ASIC
     */
    msg_printf(VRB, "Initializing Starter Video board ");
    /* reset board */
    theSVboard[SV_CONFIG] = SV_CONFIG_RESET;
    delayWritedata(GIO_BUS_DELAY);	/* dummy for GIO bus */ 
    theSVboard[SV_CONFIG] = SV_CONFIG_CLEAR;
    delayWritedata(50000);

    /* must do dummy read after every reset */
    dummydat = theSVboard[SV_CONFIG]; /* clear bus, dummy read */

    /* get board ID */
    svc1_revid = (unsigned char)theSVboard[SV_SVCID];
    msg_printf(VRB, "(revision: ");
    if (svc1_revid == SV_REV_P1)
	msg_printf(VRB, "P1)\n");
    else
	msg_printf(VRB, "P0)\n");

    /* initialize the SVC1 (controller) */
    init_SV1_regs();

    /*
     *  init the I2C chip
     */
    writeI2Cctrl(SV_I2C_OWNADDR_REG);	/* set own addr reg pointer */
    writeI2CdataNP(SV_I2C_8584_ADDR);	/* set I2C slave address */ 

    writeI2Cctrl(SV_I2C_CLK_REG);	/* select clock speed reg */
    writeI2CdataNP(SV_I2C_CLK_8MHZ);	/* set clock to 8 MHz */ 

    /*
     *  Probe for the Philips chips
     */
    msg_printf(VRB, "\nProbing for Philips chips ...\n");
    if (I2Cprobe(DMSD_ADDR)) {
	msg_printf(ERR, "DMSD not found - unable to probe for other chips on I2C bus\n");
	errorflag = 1;
    }
    else
	msg_printf(VRB, "DMSD found\n");

    if (!errorflag) {
	if ((errorflag = initDMSD()) != NO_ERROR) {
	    msg_printf(SUM,"Error detected during DMSD initialization.\n");
	    return errorflag;
	}

	if (I2Cprobe(CSC_ADDR)) {
	    msg_printf(ERR, "CSC not found\n");
	    errorflag = 1;
	} else {
	    msg_printf(VRB, "CSC found\n");
	}

	if (I2Cprobe(DENC_ADDR)) {
	    msg_printf(ERR, "DENC not found\n");
	    errorflag = 1;
	} else {
	    msg_printf(VRB, "DENC found\n");
	}
    }

#ifdef I2C_DEBUG
    {
	int ndx;

	msg_printf(DBG, "\nI2C Controller Status:\n");
	for (ndx=0; ndx<poll_num; ndx++) {
	    msg_printf(DBG, "  Poll #%2d   Status: 0x%02x",
		    ndx, status_poll[ndx]);
	    if (status_poll[ndx] & 0x01)
		msg_printf(DBG, " - Bus Free");
	    if ( (status_poll[ndx] & 0x01) == 0 )
		msg_printf(DBG, " - Bus Busy");
	    if (status_poll[ndx] & 0x80)
		msg_printf(DBG, " - Start Xmit");
	    if (status_poll[ndx] & 0x20)
		msg_printf(DBG, " - External STOP");
	    if (status_poll[ndx] & 0x10)
		msg_printf(DBG, " - BUS ERROR");
	    if ( (status_poll[ndx] & 0x08) && !(status_poll[ndx] & 0x04) )
		msg_printf(DBG, " - Waiting for Ack");
	    if (status_poll[ndx] & 0x04)
		msg_printf(DBG, " - Rcv from host");
	    msg_printf(DBG, "\n");
	}
    }
#endif

    if (errorflag) {
	msg_printf(SUM, "Error detected in probe of Philips video chip set.\n");
	return ERROR;
    }

    /* init pixkey */
    for(index = 0 ; index < 0x100 ; index++)
	writeParallelData((SV_PIXKEY_BASE + index), 0xFFFFFFFF);


    return NO_ERROR;
}
