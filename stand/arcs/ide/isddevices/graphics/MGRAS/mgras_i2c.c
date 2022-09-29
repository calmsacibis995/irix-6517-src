/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#include <sys/types.h>
#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "ide_msg.h"
#include <math.h>
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "parser.h"
#include "sys/fpanel.h"
#include "sys/pcd.h"

static void mgras_i2cRead(int, int *);
static void mgras_i2cWrite(int, int );
static int mgras_i2cStart(int);
static int mgras_i2cReceive(int *);
static int mgras_i2cSend(int);
static int mgras_i2cPortRead(int, int *);
static int mgras_i2cPortWrite(int , int);
static int mgras_mi2cPanelID(int *);
static int mgras_mi2cProbe(void);
static int mgras_mi2cSetup(void);

#define MGRAS_MI2C_CRS0 (MGRAS_DCB_CRS(0) | MGRAS_DCB_DATAWIDTH_1)
#define MGRAS_MI2C_CRS1 (MGRAS_DCB_CRS(1) | MGRAS_DCB_DATAWIDTH_1)


#define MY_PCD8584_XFRTIME 1000 

#if HQ4
#define pcdData ((volatile unsigned char *)(&mgbase->mi2c.crs0))[0]
#define pcdControl ((volatile unsigned char *)(&mgbase->mi2c.crs1))[0]
#else
#define pcdData ((volatile unsigned char *)(&mgbase->pad_dcb_15[0]))[MGRAS_MI2C_CRS0]
#define pcdControl ((volatile unsigned char *)(&mgbase->pad_dcb_15[0]))[MGRAS_MI2C_CRS1]
#endif

#ifndef MI2C_PROTOCOL_NOACK		/* for HQ3 */
#define MI2C_PROTOCOL_NOACK         		\
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(2) |	\
   MGRAS_DCB_CSWIDTH(20))
#endif

#define MI2C_PROTOCOL_SYNCACK \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(2) |  \
   MGRAS_DCB_CSWIDTH(20) | MGRAS_DCB_ENSYNCACK)

#define MI2C_PROTOCOL_ASYNCACK \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(2) |  \
   MGRAS_DCB_CSWIDTH(20) | MGRAS_DCB_ENASYNCACK)

int
mgras_i2cStop(void)
{
    int i, status;

        msg_printf (ERR, "<<<<<<MGRAS_I2CsTOP>>>>>>: \n");
    /* Generate I2C STOP and reset interrupt */
    mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CTL_PIN | PCD8584_CSDREG |
        PCD8584_CTL_STO);
    for (i = PCD8584_STOPTIME; i; i--) {
        mgras_i2cRead(PCD8584_CTRLCRS, &status);
        if (status == (PCD8584_STAT_PIN | PCD8584_STAT_BB))
            break;
    }
    if (i == 0) {
        msg_printf (SUM, "mgras_i2cStop: timeout\n");
        return 0;
    }
    return 1;
}

static void
mgras_i2cRead(int crs, int *data)
{
    us_delay (100000);
    msg_printf (DBG, "     MGRAS_I2C_READ :" );

    if (crs == PCD8584_CTRLCRS) {
    /*msg_printf (SUM, "     pcdcontrol 0x%x \n", &pcdControl );*/
        *data = pcdControl;
    msg_printf (DBG, "             CTRL  0x%x\n",  *data);
		}
    else {
        /* ASSERT (crs == PCD8584_DATACRS); */
        *data = pcdData;
    msg_printf (DBG, "             DATA 0x%x\n", *data);
    }
}

static void
mgras_i2cWrite(int crs, int data)
{
    us_delay (10000);
    msg_printf (DBG, "      MGRAS_I2C_WRITE :" );
    if (crs == PCD8584_CTRLCRS) {
        pcdControl = data;
    msg_printf (DBG, "           CTRL 0x%x\n", data);
    }
    else {
        /* ASSERT (crs == PCD8584_DATACRS); */
        pcdData = data;
    msg_printf (DBG, "           DATA 0x%x\n", data);
    }
}

static int
mgras_mi2cProbe()
{
        volatile int i;
#if (defined(IP22) || defined(IP28))
#else
    mgbase->dcbctrl_ddc = MI2C_PROTOCOL_NOACK;
#endif

    msg_printf (DBG, "      pcddata 0x%x \n", &pcdData);
    msg_printf (DBG, "      pcdcontrol 0x%x \n", &pcdControl);
	
    pcdData = 0;        /* set pcd interface mode */
    us_delay (1000);
    pcdControl = 0x40;  /* set ES0 (not refer to s0 prime ) */
    us_delay (1000);
        pcdData = 0x55;
    us_delay (1000);
    pcdControl = 0x40;  /* an innocuous write */
    us_delay(1000);
        i = 0;                  /* put other values on system bus */
   
    i = pcdControl;     /* read the previous write */
	msg_printf(DBG, "      reading from pcdcontrol %x \n", i);
    us_delay (1000);

        if (pcdData != 0x55)
                goto fail;       /* probe failed */
    us_delay(1000);
        pcdData = 0xaa;
    us_delay(1000);
    pcdControl = 0x40;  /* an innocuous write */
    us_delay(1000);
        i = 0;                  /* put other values on system bus */
   
    i = pcdControl;     /* read the previous write */
    us_delay (1000);
        if (pcdData != 0xaa)
                goto fail;       /* probe failed */

    /* mgbase->dcbctrl_ddc = MI2C_PROTOCOL_ASYNCACK;  */
        return 1;

fail:
    
    return 0;
}

#define MY_ADDR PCD8584_OWNADR

static int
mgras_mi2cSetup()
{
	msg_printf(DBG, "Entering mgras_mi2cSetup \n");
    /* Set own address to 7f */
    mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_ADRREG); /*PCD8584_ADRREG = 0*/
    us_delay(10000);
    mgras_i2cWrite(PCD8584_DATACRS, 0x50); /*PCD8584_OWNADR=0x7f*/

    us_delay(10000);
    /* Set clock 12MHz, baud 90 KHz */
    mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CLKREG); /* PCD8584_CLKREG=PCD8584_CTL_ES1=0x20 */
    mgras_i2cWrite(PCD8584_DATACRS, PCD8584_CLK12MHZ | PCD8584_BAUD90KHZ);
    us_delay(10000);
    /* Enable I2C bus */
    mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG );/*PCD8584_CSDREG=PCD8584_CTL_ES0=0x40 */

	msg_printf(DBG, "Leaving mgras_mi2cSetup \n");
    return 1;
}

static int
mgras_i2cStart(int addr)
{
    int i, status;
	msg_printf (SUM, "\n\n---------  MGRAS_I2CSTART  --------- \n");
	
    /* Enable I2C bus */
    mgras_i2cWrite(PCD8584_CTRLCRS, 0x80 | PCD8584_CSDREG | PCD8584_CTL_ACK); 

#if 0
	us_delay(1000000);
    /* Verify no interrupt and bus not busy */
    mgras_i2cRead(PCD8584_CTRLCRS, &status);

    if (status != (PCD8584_STAT_PIN | PCD8584_STAT_BB)) {
        msg_printf (SUM, "mgras_i2cStart: bus busy\n");
        mgras_i2cStop();
        return 0;
    }
	msg_printf (SUM, "   no interrupt and bus not busy \n");
#endif

    /* Set address of slave */
    msg_printf(SUM, " mgras_i2cStart : Setting address of slave \n" );
    mgras_i2cWrite(PCD8584_DATACRS, 0xa0);

	msg_printf (SUM, " mgras_i2cStart Generate I2C START and send address  \n");
    /* Generate I2C START and send address */
    mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_STAT_PIN | PCD8584_CSDREG | PCD8584_CTL_STA | PCD8584_CTL_ACK);

	us_delay(1000000);

    /* Poll with timeout for PIN bit clearing */
    for (i = MY_PCD8584_XFRTIME; i; i--) {
        mgras_i2cRead(PCD8584_CTRLCRS, &status);
        if ((status & PCD8584_STAT_PIN) == 0) break;
    }
    if (i == 0) {
        msg_printf (ERR, "  mgras_i2cStart: address transmit timeout\n");
        mgras_i2cStop();
        return 0;
    }
#if 1
    /* Check that slave ack'd */
    if (status & PCD8584_STAT_LRB) {
        msg_printf (ERR, "mgras_i2cStart: address transmit nack\n");
        mgras_i2cStop();
        return 0;
    }
#endif
	msg_printf (SUM, " mgras_i2cStart : start mgras_i2c read sequence \n");
    /* If read, start mgras_i2c read sequence (register value is garbage) */
    if (0xa0 & PCD8584_DATA_READ)
        mgras_i2cRead(PCD8584_DATACRS, &status);

    return 1;
}

static int
mgras_i2cReceive(int *data)
{
    int i, status;

	msg_printf (SUM, " ------MGRAS_I2C_RECEIVE------ \n");

    /* Poll with timeout for PIN bit clearing */
    for (i = MY_PCD8584_XFRTIME; i; i--) {
        mgras_i2cRead(PCD8584_CTRLCRS, &status);
        if ((status & PCD8584_STAT_PIN) == 0) break;
		us_delay(10000);
    }
    if (i == 0) {
        msg_printf (ERR, "mgras_i2cReceive: timeout\n");
        mgras_i2cStop();
        return 0;
    }

#if 1
	mgras_i2cWrite(PCD8584_DATACRS, 0xa1);
	us_delay(1000);

	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_STAT_PIN | PCD8584_CSDREG | PCD8584_CTL_STA | PCD8584_CTL_ACK);	
#else

	msg_printf (SUM, " mgras_i2cReceive :  Check for no errors \n");

    /* Check for no errors */
    if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
        PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
        msg_printf (ERR, "mgras_i2cReceive: error\n");
        mgras_i2cStop();
        return 0;
    }
#endif
    /* Read byte */
   for (i = 0; i < 128; i++) {
    mgras_i2cRead(PCD8584_DATACRS, data);
	us_delay(100000);
    }
    return 1;
}


static int
mgras_i2cSend(int data)
{
    int i, status;

	msg_printf (SUM, " mgras_i2cSend 1  \n");
    /* Write byte */
    mgras_i2cWrite(PCD8584_DATACRS, data);

    /* Poll with timeout for PIN bit clearing */
    for (i = MY_PCD8584_XFRTIME; i; i--) {
        mgras_i2cRead(PCD8584_CTRLCRS, &status);
        if ((status & PCD8584_STAT_PIN) == 0) break;
    }
    if (i == 0) {
        msg_printf (SUM, "mgras_i2cSend: timeout\n");
        mgras_i2cStop();
        return 0;
    }

    /* Check for no errors */
    if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
        PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
        msg_printf (SUM, "mgras_i2cSend: error\n");
        mgras_i2cStop();
        return 0;
    }

	msg_printf (SUM, " mgras_i2cSend 2  \n");
    /* Check that destination ack'd */
    if (status & PCD8584_STAT_LRB) {
        msg_printf (SUM, "mgras_i2cSend: nack\n");
        mgras_i2cStop();
        return 0;
    }

    return 1;
}




static int
mgras_i2cPortRead(int addr, int *data)
{
	int tmp;
	tmp =  mgras_i2cStart(addr | PCD8584_DATA_READ);
	if (tmp == 0) {
		mgras_i2cStop();
		return(1);
	}
        mgras_i2cReceive(data) ;
        mgras_i2cStop();

	return(0);
}


static int
mgras_i2cPortWrite(int addr, int data)
{
    msg_printf (SUM, "mgras_i2cPortWrite(%02x, %02x)\n", addr, data);
    return mgras_i2cStart(addr | PCD8584_DATA_WRITE) &&
        mgras_i2cSend(data) &&
        mgras_i2cStop();
}

uchar_t tmp_addr = PCD8574_OWNADR;
static int
mgras_mi2cPanelID(int *id)
{
   	tmp_addr = 0xa0; 
    if (mgras_i2cPortRead(tmp_addr, id) == 0) {
		msg_printf(SUM, " mgras_mi2cPanelID returning 0 \n");
        return 0;
	}
    *id = (*id >> PCD8574_IDSHIFT) & PCD8574_IDMASK;
    return 1;
}

__uint32_t
mgras_mi2c_probe ()
{
    int panel = 0xdeadbeef;
#ifdef IP30
    msg_printf(DBG, " MI2C Probing.\n"  );
	/*msg_printf(DBG, "size of _mi2c 0x%x \n", sizeof(_mi2c) );  */
	/* check if I2C is present, then set it up */
    if ( mgras_mi2cProbe() && mgras_mi2cSetup() )
    {
#if 0
		msg_printf (SUM, " mgras_mi2c_probe \n");
        if ( mgras_mi2cPanelID(&panel) )
        {
			if ((panel == 0) || (panel == MONITORID_NOFPD)) {
    		msg_printf(SUM, " MI2C not detecting I2C : 0x%x] \n",panel );
			return(1);
			}
			else{
    		msg_printf(SUM, " MI2C Probing complete. [0x%x] \n",panel );
        	return (panel);
			}
        }
#endif
	msg_printf(SUM, " I2C 8584 Probing Complete \n");
    }
	else
    {
    		msg_printf(SUM, " MI2C Probing Failed. \n");
			return (1);
	}
#endif
	return (0);

}

