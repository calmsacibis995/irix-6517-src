/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Moosehead Graphics I2C interface.
 */

#include <sys/types.h>
#include "sys/systm.h"
#include "sys/sbd.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/pda.h"
#include "sys/sysinfo.h"
#include "sys/ksa.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/htport.h"         /* for -fullwarn */
#include "sys/edt.h"            /* for -fullwarn */
#include "sys/fpanel.h"
#include "sys/pcd.h"
#include "sys/cmn_err.h"

/*
#include "sys/region.h"
#include "sys/pmap.h"
*/
#include "sys/gfx.h"
#include "sys/rrm.h"
#include "sys/crime_gfx.h"
#include "sys/crime_gbe.h"
#include "crmDefs.h"
#include "crm_stand.h"
#include "crm_gfx.h"
#ifdef CRM_DUAL_CHANNEL
#include "dh_i2c.h"
#endif /* CRM_DUAL_CHANNEL */

int crm_dbgi2c = 0;
int crm_dbgi2c_fine = 0;

int crime_i2cMonitorProbe(struct gbechip *, crime_ddc_edid_t *);
int crime_i2cValidEdid( crime_ddc_edid_t *);

/* Each iter. > 40 ns, for 1us about 25 iter.s */
/* #define I2C_DELAY_CNT 25 */
/* #define I2C_DELAY_US(nms) (I2C_DELAY_CNT*nms) */

/* Minimum clock hold times.  - 4.7us Low, 4.0us High, Max to 5 */
/* #define I2C_DELAY() { for (dcnt=(I2C_DELAY_US(400)); dcnt; dcnt--); } */

/* Minimum clock hold times.  - 4.7us Low, 4.0us High, Max to 5 */
#define I2C_DELAY() us_delay(10)


/* Output SDA & SDC values are inverted. */
#define I2C_CLK_LOW_DATA_LOW        	0x3
#define I2C_CLK_LOW_DATA_HIGH       	0x2
#define I2C_CLK_HIGH_DATA_LOW       	0x1
#define I2C_CLK_HIGH_DATA_HIGH      	0x0

/* Real output value, i.e. Not inverted */
#define I2C_REALVAL(val)		((~val)&0x3)

#define I2C_READ(hwp, buf)	gbeGetReg(hwp, i2c, buf); buf=((~buf)&0x3)
#define I2C_WRITE(hwp, val)	(gbeSetReg(hwp,i2c,(val))); I2C_DELAY()

#define I2CFP_READ(hwp, buf)    (gbeGetReg(hwp, i2cfp, buf)); buf=((~buf)&0x3);
#define I2CFP_WRITE(hwp, val)	(gbeSetReg(hwp,i2cfp,(val)));I2C_DELAY()

#define I2C_DATA(b)			(b & 0x1)
#define I2C_CLK(b)			((b & 0x2)>>1)

/* Maximum time receiver can stretch clock low: 20.345us (@ 6KB/s) */
/* #define I2C_CLK_LOW_MAX			21 */
#define I2C_CLK_LOW_MAX			60


/* Negative Acknowledge time (in micro-seconds) */
#define I2C_NEG_ACK			1000

/* Number of sendbyte retries */
#define I2C_NUM_RETRIES 4

#define I2C_MAX_RESET_TRIES	25

int i2cfp_PanelOn(struct gbechip *);
int i2cfp_PanelOff(struct gbechip *);
int i2c7of9_PanelOn(struct gbechip *);
int i2c7of9_PanelOff(struct gbechip *);


/* Local functions. */
static int i2c_start(struct gbechip *);
static int i2c_stop(struct gbechip *);
static int i2c_sync_clk(struct gbechip *);
static int i2c_sendbyte(struct gbechip *, unsigned char);
static int i2c_recvbyte(struct gbechip *, unsigned char *);
static int i2cfp_start(struct gbechip *);
static int i2cfp_stop(struct gbechip *);
static int i2cfp_sync_clk(struct gbechip *);
static int i2cfp_start_addr(struct gbechip *, unsigned char);
static int i2cfp_sendbyte(struct gbechip *, unsigned char);
static int i2cfp_recvbyte(struct gbechip *, unsigned char *);
static int i2cfp_PanelID(struct gbechip *, unsigned char *);

static int i2cfp_BlUp(struct gbechip *, int);
static int i2cfp_AudioVolLeft(struct gbechip *, int);
static int i2cfp_AudioVolRight(struct gbechip *, int);
static int i2cfp_AudioBass(struct gbechip *, int);
static int i2cfp_AudioTreble(struct gbechip *, int);
static int i2cfp_AudioSwitch(struct gbechip *, int);
static int i2cfp_SetBit(struct gbechip *, int);

static int (*fp_poweroff)();

/* Local variables. */
static int corona_bl;
static int corona_left;
static int corona_right;
static int corona_bass;
static int corona_treble;
static int corona_mute;
static int corona_18_bit;



/*
 * crime_i2cMonitorProbe: Probe I2C Monitor for DDC1/2B or DDC1/2AB
 *			   compatibility. Currently only DDC1/2B is implemented.
 *  Retrieve the EDID (Extended Display Identification Data).
 *  Returns 0 upon successful read, otherwise non-zero.
 */
int
crime_i2cMonitorProbe( struct gbechip *gbe, crime_ddc_edid_t *edidp )
{
    unsigned int i;
    unsigned char rbuf;				/* Read buffer */
    unsigned char reset_cnt;			/* I2C bus Reset counter */
    unsigned char *ecp = (unsigned char *)edidp;

    /* Clear EDID structure */
    bzero(edidp, sizeof(crime_ddc_edid_t));


    /*
     * Reset I2C bus.
     */
    reset_cnt = 0;

    i2c_stop(gbe);

    I2C_READ(gbe, rbuf);
    while ( rbuf != (unsigned char)(I2C_REALVAL(I2C_CLK_HIGH_DATA_HIGH)) ) {
	if( ++reset_cnt > I2C_MAX_RESET_TRIES) {
	    return 1;
	}
        i2c_stop(gbe);
        I2C_READ(gbe, rbuf);
    }

    us_delay(100);

    /* Start condition */
    if ( i2c_start(gbe) ) {
	return 1;
    }

    /* Send slave address */
    if ( i2c_sendbyte(gbe, 0xa0) ) goto abort_ret;


    /* Send start address */
    if ( i2c_sendbyte(gbe, 0x00) ) goto abort_ret;


    /* Repeat Start condition */
    if ( i2c_start(gbe) ) goto abort_ret;

    /* Send slave address, setting read bit */
    if ( i2c_sendbyte(gbe, (0xa0 | 0x01)) ) goto abort_ret;



    /* Receive EDID structure */
    for (i=0; i<128; i++) {
	if ( i2c_recvbyte(gbe, &ecp[i]) ) goto abort_ret;
    }

    /* Stop condition */
    i2c_stop(gbe);

    /* Validate header */
    if ( ! crime_i2cValidEdid(edidp) ) {
	return 1;
    }

    return 0;				/* Return OK */

abort_ret:
    i2c_stop(gbe);			/* Stop condition */
    return 1;				/* Return error */
}


int
crime_i2cValidEdid( crime_ddc_edid_t *edidp )
{
    unsigned char *ecp = (unsigned char *)edidp;
    u_int i;
    static unsigned char valid_EDID_hdr[8] =
	    { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };


    /* Validate header */
    for (i=0; i<8; i++) {
	if ( ecp[i] != valid_EDID_hdr[i] ) {
	    return 0;
	}
    }
    return 1;
}



/*
 * crime_i2cFpProbe: Probe the I2C Flat Panel Adapter.
 */
int
crime_i2cFpProbe(struct gbechip *gbe)
{
    unsigned char panel = 0x0;
    inventory_t *panel_inv;
    unsigned char rbuf;		/* Read buffer */
    unsigned int ri;

    if (crm_dbgi2c)
	cmn_err(CE_CONT, "crime_i2cFpProbe: Entered.\n");

    /*
     * Reset I2CFP bus.
     */

    i2cfp_stop(gbe);

    ri = 0;
    I2CFP_READ(gbe, rbuf);
    while ( rbuf != (unsigned char)(I2C_REALVAL(I2C_CLK_HIGH_DATA_HIGH)) ) {
	i2cfp_stop(gbe);
	I2CFP_READ(gbe, rbuf);
	if( ++ri > 25) break;
    }

    if ( rbuf != (unsigned char)(I2C_REALVAL(I2C_CLK_HIGH_DATA_HIGH)) ) {
	return 0;
    }

    if ( i2cfp_PanelID(gbe, &panel) ) {

	switch (panel) {
	    case PANEL_XGA_MITSUBISHI:
	    case (PANEL_XGA_MITSUBISHI-1):
		if ( crm_dbgi2c )
		    cmn_err(CE_CONT,
			"Flat panel found: Mitsubishi Corona.\n");
		return MONITORID_CORONA;
	    case PANEL_EWS_MITSUBISHI:
		if ( crm_dbgi2c )
		    cmn_err(CE_CONT,
			"Flat panel found: Mitsubishi ichiban.\n");
		return MONITORID_ICHIBAN;
	    case PANEL_DMI2_7of9:
		if ( crm_dbgi2c )
		    cmn_err(CE_CONT, "Flat panel found: Mitsubishi 7of9\n");
		return MONITORID_7of9;
	    default:
		if ( crm_dbgi2c )
		    cmn_err(CE_WARN,
			"Unknown panel id %d\n", panel);
		return 0;
	}
    }

    return 0;
}



static int
i2cfp_PortRead(struct gbechip *gbe, int addr, unsigned char *data)
{
	int stat;

        stat = ( ( i2cfp_start_addr(gbe, addr | PCD8584_DATA_READ) )	&&
		 ( i2cfp_recvbyte(gbe, data) )				&&
		 ( i2cfp_stop(gbe) ) );

	return stat;
}



static int
i2cfp_PortWrite(struct gbechip *gbe, int addr, unsigned char data)
{
	int stat;

        stat =	(( i2cfp_start_addr(gbe, addr | PCD8584_DATA_WRITE) )	&&
		 ( i2cfp_sendbyte(gbe, data) )				&&
		 ( i2cfp_stop(gbe) ));

	return stat;
}



static int
i2cfp_PanelID(struct gbechip *gbe, unsigned char *id)
{
        if ( ! i2cfp_PortRead(gbe, PCD8574_OWNADR, id) ) {
                return 0;
	}

        *id = (*id >> PCD8574_IDSHIFT) & PCD8574_IDMASK;

        return 1;
}



#ifdef CRM_DUAL_CHANNEL
/*
 * crime_DualChannelProbe: Probe for any version of the dual display board.
 */
int crime_DualChannelProbe(struct gbechip *gbe)
{
	unsigned char val, orig_val;
	int board_id;

	if (!i2cfp_PortRead(gbe, DH8574_OWNADR, &orig_val))
		return 0;

	/* Set VCORNG to 1 to read MSB's of board id*/
	i2cfp_PortWrite(gbe, DH8574_OWNADR, 0xff);
	i2cfp_PortRead(gbe, DH8574_OWNADR, &val);
	board_id = DH_X_ID(val);

	/* Set VCORNG to 0 to read LSB's of board id*/
	i2cfp_PortWrite(gbe, DH8574_OWNADR, 0xff & ~DH8574_VCORNG);
	i2cfp_PortRead(gbe, DH8574_OWNADR, &val);
	board_id = (board_id << 2) + DH_X_ID(val);

	/* Restore original VCORNG value */
	i2cfp_PortWrite(gbe, DH8574_OWNADR, orig_val | DH_RDMASK);

#if DEBUG
	if (crm_dbgfpi2c)
		cmn_err(CE_CONT, "crime_DualChannelProbe: id = 0x%x\n",
								board_id);
#endif /* DEBUG */

#if 0
	add_to_inventory(INV_DISPLAY, INV_DCD_BOARD, 0, 0, 0);
#endif
	return (board_id + 1);
}



/*
 * crime_DualChannelCommand: Send a byte to the dual display board.
 */
int crime_DualChannelCommand(struct gbechip *gbe, unsigned char command)
{
	return i2cfp_PortWrite(gbe, DH8574_OWNADR, command);
}
#endif /* CRM_DUAL_CHANNEL */



/*
 * i2c_start: Generate the START condition
 */
static int
i2c_start(struct gbechip *gbe)
{
    /* Transition Data High-to-Low with Clock High. */
    /* Bus should be in free state. */
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW);

    /* Pull Clock Low.*/
    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_LOW );

    return 0;
}



/*
 * i2cfp_start: Generate the START condition
 */
static int
i2cfp_start(struct gbechip *gbe)
{
    /* Transition Data High-to-Low with Clock High. */
    /* Bus should be in free state. */
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW);

    /* Pull Clock Low.*/
    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_LOW );

    return 1;
}


/*
 * i2cfp_start_addr: Generate FP I2C Start and send Address.
 */
static int
i2cfp_start_addr(struct gbechip *gbe, unsigned char addr)
{
    /* Start condition */
    if ( ! i2cfp_start(gbe) ) return 0;

    if ( ! i2cfp_sendbyte(gbe, addr) ) goto abort_ret;

    return 1;

abort_ret:
    i2cfp_stop(gbe);			/* Stop condition */
    return 0;				/* Return error */
}


/*
 * i2c_stop: Generate the STOP condition
 */
static int
i2c_stop(struct gbechip *gbe)
{
    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_LOW );
    us_delay(5);	/* Stop setup */

    /* Transition Data Low-to-High with Clock High. */
    /* Bus should be in busy state. */
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW );
    us_delay(5);	/* Stop setup */
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH );

    return 0;
}


/*
 * i2cfp_stop: Generate the STOP condition
 */
static int
i2cfp_stop(struct gbechip *gbe)
{
    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_LOW );

    /* Transition Data Low-to-High with Clock High. */
    /* Bus should be in busy state. */
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW );
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH );

    return 1;
}




/* Synchronize clock. Slow devices hold clock line low. */
static int
i2c_sync_clk(struct gbechip *gbe) 
{
	unsigned char rbuf;
        unsigned char dcnt=0;

	I2C_READ(gbe, rbuf);
	while ( ! I2C_CLK(rbuf) ) {
    	    if ( crm_dbgi2c ) cmn_err(CE_CONT," # ");
	    if ( ++dcnt > I2C_CLK_LOW_MAX ) {
    		if ( crm_dbgi2c )
		    cmn_err(CE_CONT,"i2c_sync_clk: Slow dev.\n");
		return 0;
	    }
	    us_delay(1);
	    I2C_READ(gbe, rbuf);
	}
	return 1;
}



static int
i2cfp_sync_clk(struct gbechip *gbe)
{
	unsigned char rbuf;
        unsigned char dcnt=0;

	I2CFP_READ(gbe, rbuf);
	while ( ! I2C_CLK(rbuf) ) {
	    if ( ++dcnt > I2C_CLK_LOW_MAX ) {
    		if ( crm_dbgi2c )
		    cmn_err(CE_CONT,"i2cfp_sync_clk: Slow dev.\n");
	    	return 0;
	    }
	    us_delay(1);
	    I2CFP_READ(gbe, rbuf);
	}
	return 1;
}






/*
 * i2c_sendbyte: Transmit a byte of data and get acknowledge bit.
 */
static int
i2c_sendbyte(struct gbechip *gbe, unsigned char sbyte)
{
    unsigned char bit;
    unsigned char rbuf;		/* Read buffer */
    unsigned int dcnt;		/* Delay counter */
    int num_retries = 0;

i2c_retry:

    for (bit = 0x80; bit; bit >>=1 )
    {
	if (bit & sbyte) {
	    /* Data high */
	    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
	    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);

	    /* Synchronize clock. Slow devices hold clock line low. */
	    /* if ( ! i2c_sync_clk(gbe) ) goto abort_retry; */
	    { /* Sync Clk */
		unsigned char rbuf;
		unsigned char dcnt=0;

		I2C_READ(gbe, rbuf);
		while ( ! I2C_CLK(rbuf) ) {
		    if ( crm_dbgi2c ) cmn_err(CE_CONT," # ");
		    if ( ++dcnt > I2C_CLK_LOW_MAX ) {
			if ( crm_dbgi2c )
			    cmn_err(CE_CONT,"i2c_sync_clk: Slow dev.\n");
			goto abort_retry;
		    }
		    us_delay(1);
		    I2C_READ(gbe, rbuf);
		}
	    }




	    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
	}
	else {
	    /* Data low */
	    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);
	    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW);

	    /* Synchronize clock. Slow devices hold clock line low. */
	    /* if ( ! i2c_sync_clk(gbe) ) goto abort_retry; */
	    { /* Sync Clk */
		unsigned char rbuf;
		unsigned char dcnt=0;

		I2C_READ(gbe, rbuf);
		while ( ! I2C_CLK(rbuf) ) {
		    if ( crm_dbgi2c ) cmn_err(CE_CONT," # ");
		    if ( ++dcnt > I2C_CLK_LOW_MAX ) {
			if ( crm_dbgi2c )
			    cmn_err(CE_CONT,"i2c_sync_clk: Slow dev.\n");
			goto abort_retry;
		    }
		    us_delay(1);
		    I2C_READ(gbe, rbuf);
		}
	    }

	    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);
	}

    }

    /* Generate ACK clock. Release data line (high).*/
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);

    /* Synchronize clock. Slow devices hold clock line low. */
    /* if ( ! i2c_sync_clk(gbe) ) goto abort_retry; */

    { /* Sync Clk */
	unsigned char rbuf;
        unsigned char dcnt=0;

	I2C_READ(gbe, rbuf);
	while ( ! I2C_CLK(rbuf) ) {
    	    if ( crm_dbgi2c ) cmn_err(CE_CONT," # ");
	    if ( ++dcnt > I2C_CLK_LOW_MAX ) {
    		if ( crm_dbgi2c )
		    cmn_err(CE_CONT,"i2c_sync_clk: Slow dev.\n");
		goto abort_retry;
	    }
	    us_delay(1);
	    I2C_READ(gbe, rbuf);
	}
    }





    /* Wait for ACK. */
    dcnt=0;
    us_delay(10);
    I2C_READ(gbe, rbuf);
    while( I2C_DATA(rbuf) ) {
	    if ( ++dcnt > I2C_NEG_ACK ) {
		goto abort_retry;
	    }
	    us_delay(1);
	    I2C_READ(gbe, rbuf);
    }


    /* Pull clock low */
    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);

    return 0;


abort_retry:
    if (++num_retries < I2C_NUM_RETRIES) {
	    i2c_stop(gbe);
	    i2c_start(gbe);
	    goto i2c_retry;
    }

    i2c_stop(gbe);
    return 1;

}





/*
 * i2cfp_sendbyte: Transmit a byte of data and get acknowledge bit.
 */
static int
i2cfp_sendbyte(struct gbechip *gbe, unsigned char sbyte)
{
    unsigned char bit;
    unsigned char rbuf;		/* Read buffer */
    unsigned int dcnt;		/* Delay counter */
    int num_retries = 0;

i2cfp_retry:

    for (bit = 0x80; bit; bit >>=1 )
    {
	if (bit & sbyte) {
	    /* Data high */
	    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
	    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);

	    /* Synchronize clock. Slow devices hold clock line low. */
	    if ( ! i2cfp_sync_clk(gbe) ) goto abort_retry_fp;

	    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
	}
	else {
	    /* Data low */
	    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);
	    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW);

	    /* Synchronize clock. Slow devices hold clock line low. */
	    if ( ! i2cfp_sync_clk(gbe) ) goto abort_retry_fp;

	    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);
	}

    }

    /* Generate ACK clock. Release data line (high).*/
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);

    /* Synchronize clock. Slow devices hold clock line low. */
    if ( ! i2cfp_sync_clk(gbe) ) goto abort_retry_fp;


    /* Wait for ACK. */
    I2CFP_READ(gbe, rbuf);
    dcnt=0;
    while( I2C_DATA(rbuf) ) {
	    if ( ++dcnt > I2C_NEG_ACK ) {
		goto abort_retry_fp;
	    }
	    us_delay(1);
	    I2CFP_READ(gbe, rbuf);
    }


    /* Clock low */
    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);

    return 1;

abort_retry_fp:
    if (num_retries++ < I2C_NUM_RETRIES) {
	    i2cfp_stop(gbe);
	    i2cfp_start(gbe);
	    goto i2cfp_retry;
    }

    i2cfp_stop(gbe);
    return 0;
}



/*
 * i2c_recvbyte: Receive a byte of data and get acknowledge bit.
 */
static int
i2c_recvbyte(struct gbechip *gbe, unsigned char *bytep)
{
    unsigned int idx;
    unsigned char rbuf;		/* Read buffer */

    *bytep = 0;
    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);

    for (idx=0; idx<8; idx++ ) {
	I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);
	if ( ! i2c_sync_clk(gbe) ) {
	    i2c_stop(gbe);
	    return 1;
	}
	I2C_READ(gbe, rbuf);					\

	*bytep <<= 1;
	*bytep |= I2C_DATA(rbuf);

	I2C_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
    }

    /* Generate ACK */
    I2C_WRITE(gbe, I2C_CLK_HIGH_DATA_LOW);
    if ( ! i2c_sync_clk(gbe) ) {
	i2c_stop(gbe);
	return 1;
    }
    I2C_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);

    return 0;
}




/*
 * i2cfp_recvbyte: Receive a byte of data and get acknowledge bit.
 */
static int
i2cfp_recvbyte(struct gbechip *gbe, unsigned char *bytep)
{
    unsigned int idx;
    unsigned char rbuf;		/* Read buffer */

    *bytep = 0;
    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);

    for (idx=0; idx<8; idx++ ) {
	I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);
	if (! i2cfp_sync_clk(gbe) ) {
	    i2cfp_stop(gbe);
	    return 0;
	}
	I2CFP_READ(gbe, rbuf);

	*bytep <<= 1;
	*bytep |= I2C_DATA(rbuf);

	I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_HIGH);
    }

    /* Generate  ! ACK */
    I2CFP_WRITE(gbe, I2C_CLK_HIGH_DATA_HIGH);

    if (! i2cfp_sync_clk(gbe) ) {
	    i2cfp_stop(gbe);
	    return 0;
    }

    I2CFP_WRITE(gbe, I2C_CLK_LOW_DATA_LOW);

    return 1;
}


int
i2cfp_PanelOn(struct gbechip *gbe)
{
        corona_bl = 0;
	corona_mute = 1;
	corona_left = 0;
	corona_right = 0;
	corona_bass = 0;
	corona_treble = 0;
	corona_18_bit = 0;

	fp_poweroff = i2cfp_PanelOff;
	
	return i2cfp_PortWrite(gbe, PCD8574_OWNADR,
		   PCD8574_ID | 
		   PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT | 
		   PCD8574_ON) &&
		i2cfp_AudioVolLeft(gbe, 0xf6) &&
		i2cfp_AudioVolRight(gbe, 0xf6) &&
		i2cfp_AudioBass(gbe, 0xf6) &&
		i2cfp_AudioTreble(gbe, 0xf6) &&
		i2cfp_AudioSwitch(gbe, 0xce) &&
		i2cfp_BlUp(gbe, CORONA_BL_MAX) &&
	        i2cfp_SetBit(gbe, 0);
}

/*
 * Function: i2c7of9_PanelOn
 *
 * Description: This function designed for the specific case of causing 
 * the 7of9 flatpanel to become active (stop powersaving).
 *
 * I chose to use a separate function since I did not have access to the
 * MONITORID string in this file.  crm_init.c's functions already could
 * access that information, so it was just easier to add another routine
 * in this file and then call it from crm_init.c.
 */

int
i2c7of9_PanelOn(struct gbechip *gbe)
{
  unsigned char reply;

  i2cfp_PortRead(gbe, DMI2_CntlRegAddr, &reply);
  reply &= DMI2_PowersaveMask;
  reply |= DMI2_PowersaveOff;  /* Make sure panel is active */
  
  /* 
   * NOTE: In the PROM, we assume that no changes to the backlight
   * settings will ever be necessary (we hope).  In this vein,
   * we do not worry about initializing the EEPotentiometer here;
   * rather we will use its default power-on values which are stored
   * into Reg 0 on both channels of the X9221 chip inside 7of9.
   */
  
  return i2cfp_PortWrite(gbe, DMI2_CntlRegAddr, reply);
}

int
i2cfp_PanelOff(struct gbechip *gbe)
{
    return i2cfp_PortWrite(gbe, PCD8574_OWNADR, 
		 PCD8574_ID |
		 PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
		 PCD8574_OFF);
}

/*
 * Function: i2c7of9_PanelOff
 *
 * Description: This function designed for the specific case of causing 
 * the 7of9 flatpanel to start powersaving.
 *
 * I chose to use a separate function since I did not have access to the
 * MONITORID string in this file.  crm_init.c's functions already could
 * access that information, so it was just easier to add another routine
 * here and then call it from crm_init.c.
 */

int
i2c7of9_PanelOff(struct gbechip *gbe)
{
  unsigned char reply;
  i2cfp_PortRead(gbe, DMI2_CntlRegAddr, &reply);
  reply &= DMI2_PowersaveMask;
  reply |= DMI2_PowersaveOn;  /* Turn the backlights & display off */
  return i2cfp_PortWrite(gbe, DMI2_CntlRegAddr, reply); 
}

static int
i2cfp_BlUp(struct gbechip *gbe, int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

	while (steps--) {
		if (!i2cfp_PortWrite(gbe, PCD8574_OWNADR, 
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) ||
	      	    !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INC | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			PCD8574_ON | bit))
			return 0;
		if (corona_bl < CORONA_BL_MAX) corona_bl++;
	      }
	return 1;
}

static int
i2cfp_BlDown(struct gbechip *gbe, int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

	while (steps--) {
		if (!i2cfp_PortWrite(gbe, PCD8574_OWNADR, 
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INC | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			PCD8574_ON | bit))
			return 0;
		if (corona_bl > CORONA_BL_MIN) corona_bl--;
	}
	return 1;
}


static int
i2cfp_AudioVolLeft(struct gbechip *gbe, int volLeft)
{
        corona_left = volLeft;
	return i2cfp_start_addr(gbe, PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cfp_sendbyte(gbe, PCD8425_VLADR) &&
	    i2cfp_sendbyte(gbe, volLeft) &&
	    i2cfp_stop(gbe);
}


static int
i2cfp_AudioVolRight(struct gbechip *gbe, int volRight)
{
        corona_right = volRight;
	return i2cfp_start_addr(gbe, PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cfp_sendbyte(gbe, PCD8425_VRADR) &&
	    i2cfp_sendbyte(gbe, volRight) &&
	    i2cfp_stop(gbe);
}

static int
i2cfp_AudioBass(struct gbechip *gbe, int bass)
{
        corona_bass = bass;
	return i2cfp_start_addr(gbe, PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cfp_sendbyte(gbe, PCD8425_BSADR) &&
	    i2cfp_sendbyte(gbe, bass) &&
	    i2cfp_stop(gbe);
}

static int
i2cfp_AudioTreble(struct gbechip *gbe, int treble)
{
        corona_treble = treble;
	return i2cfp_start_addr(gbe, PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cfp_sendbyte(gbe, PCD8425_TRADR) &&
	    i2cfp_sendbyte(gbe, treble) &&
            i2cfp_stop(gbe);
}

static int
i2cfp_AudioSwitch(struct gbechip *gbe, int flags)
{
        if (flags & 0x20)
		corona_mute = 1;
	else
		corona_mute = 0;

	return i2cfp_start_addr(gbe, PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cfp_sendbyte(gbe, PCD8425_SWADR) &&
	    i2cfp_sendbyte(gbe, flags) &&
	    i2cfp_stop(gbe);
}

static int
i2cfp_SetBit(struct gbechip *gbe, int bit)
{
    /* set to 18 bit emulation */
    if (bit) {
	corona_18_bit = 1;
    	return (i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			     PCD8574_BIT));
    }
    else { /* set to 15 bit emulation */
	corona_18_bit = 0;
	return (i2cfp_PortWrite(gbe, PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT));
    }
}

int
crime_i2cfp_command(struct gbechip *gbe, struct I2C_CMD_ARG *cmd_arg)
{
  switch(cmd_arg->cmd)
    {
    case I2C_PANEL_ON:
      cmd_arg->return_val = i2cfp_PanelOn(gbe);
      break;
    case I2C_PANEL_OFF:
      cmd_arg->return_val = i2cfp_PanelOff(gbe);
      break;
    case I2C_BL_UP:
      cmd_arg->return_val = i2cfp_BlUp(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_BL_DOWN:
      cmd_arg->return_val = i2cfp_BlDown(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_VOL_LEFT:
      cmd_arg->return_val = i2cfp_AudioVolLeft(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_VOL_RIGHT:
      cmd_arg->return_val = i2cfp_AudioVolRight(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_BASS:
      cmd_arg->return_val = i2cfp_AudioBass(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_TREBLE:
      cmd_arg->return_val = i2cfp_AudioTreble(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_SWITCH:
      cmd_arg->return_val = i2cfp_AudioSwitch(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_BL_VAL:
      cmd_arg->return_val = corona_bl;
      break;
    case I2C_LEFT_VAL:
      cmd_arg->return_val = corona_left;
      break;
    case I2C_RIGHT_VAL:
      cmd_arg->return_val = corona_right;
      break;
    case I2C_BASS_VAL:
      cmd_arg->return_val = corona_bass;
      break;
    case I2C_TREBLE_VAL:
      cmd_arg->return_val = corona_treble;
      break;
    case I2C_MUTE_VAL:
      cmd_arg->return_val = corona_mute;
      break;
    case I2C_SET_BIT:
      cmd_arg->return_val = i2cfp_SetBit(gbe, *((int *)cmd_arg->args));
      break;
    case I2C_BIT_VAL:
      cmd_arg->return_val = corona_18_bit;
      break;
    default:
      cmn_err (CE_WARN, "crime_i2c: Unknown command %d\n", cmd_arg->cmd);
      return EINVAL;
  }
  return 0;
}
