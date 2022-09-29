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

/*
 * Support for Presenter I2C controls.
 *
 * $Revision: 1.5 $
 */

#if defined(IP22) || defined(IP24) || defined(IP26) || defined(IP28)

#include "sys/gr2hw.h"
#include "sys/ng1hw.h"
#include "libsc.h"
#include "libsk.h"
#include "sys/pcd.h"

#if I2_DEBUG
#define I2_PRINTF(arg)	printf arg
#else
#define I2_PRINTF(arg)
#endif

#ifndef	DELAY
#define	DELAY(n)	us_delay(n)
#endif	/* DELAY */

static struct gr2_hw *GR2;
static rex3Chip *REX;
static int corona_18_bit;

#define PCD_DCB_PROTOCOL (DCB_DATAWIDTH_1 | DCB_ENASYNCACK |    \
                        (13 << DCB_CSWIDTH_SHIFT) |             \
                        ( 6 << DCB_CSHOLD_SHIFT)  |             \
                        ( 1 << DCB_CSSETUP_SHIFT) |             \
                        (DCB_PCD8584 << DCB_ADDR_SHIFT))


#if 0
static void i2cRead(int, int *);
static void i2cWrite(int, int );
#endif
static int i2cStart(int);
static int i2cReceive(int *);
static int i2cSend(int);
static int i2cStop(void);
static int i2cPortRead(int, int *);
static int i2cPortWrite(int , int);
static int i2cAudioVolLeft(int);
static int i2cAudioVolRight(int);
static int i2cAudioBass(int);
static int i2cAudioTreble(int);
static int i2cAudioSwitch(int);
static int i2cSetBit(int);
void i2cPanelOff(void);
static void dcb_fifowait(void);

extern void (*fp_poweroff)();
     
static int i2cBlUp(int);
#if 0
static int i2cBlDown(int);
#endif

/*
 * On gr2 systems, we 
 * access i2c bus via one of two address spaces.
 * The first is for transfers with hardware acknowledge,
 * the second is without acknowledge.
 * The nack address space is just used when probing for
 * the i2c bus.
 */

#define pcdDataAck ((volatile unsigned char *)(&GR2->ab1.dRam))[0]
#define pcdControlAck ((volatile unsigned char *)(&GR2->ab1.dRam))[4]
#define pcdDataNack GR2->cc1.sysctl
#define pcdControlNack GR2->cc1.indrct_addreg

void (*_i2cRead)(int, int *), (*_i2cWrite)(int, int);
#define i2cRead(crs,pdata) (*_i2cRead)(crs,pdata)
#define i2cWrite(crs,data) (*_i2cWrite)(crs,data)

static void
gr2_i2cRead(int crs, int *data)
{
        if (crs == PCD8584_CTRLCRS)
                *data = pcdControlAck;
        else {
                *data = pcdDataAck;
        }
        I2_PRINTF(("i2cRead %d -> %02x\n", crs, *data));
}

static void
gr2_i2cWrite(int crs, int data)
{
        if (crs == PCD8584_CTRLCRS)
                pcdControlAck = data;
        else {
                pcdDataAck = data;
        }
        I2_PRINTF (("i2cWrite %d <- %02x\n", crs, data));
}

int
gr2_i2cProbe(struct gr2_hw *gr2)
{
        volatile int i;

        GR2 = gr2;

	pcdDataNack = 0x0; 	/* set pcd interface mode */

	pcdDataNack = 0x55;
	i = 0;
	pcdDataNack;		/* flush the fifo */
	if (pcdDataNack != 0x55)
		return 0;	/* probe failed */
	pcdDataNack = 0xaa;
	i = 0;
	pcdDataNack;		/* flush the fifo */
	if (pcdDataNack != 0xaa)
		return 0;	/* probe failed */

	_i2cRead = &gr2_i2cRead;
	_i2cWrite = &gr2_i2cWrite;
	return 1;
}

static void
ng1_i2cRead(int crs, int *data)
{
        BFIFOWAIT (REX);
        REX->set.dcbmode = (crs << DCB_CRS_SHIFT) | PCD_DCB_PROTOCOL;
        *data = REX->set.dcbdata0.bybyte.b3;
        I2_PRINTF (("i2cRead %d -> %02x\n", crs, *data));
}

static void
ng1_i2cWrite(int crs, int data)
{
        BFIFOWAIT (REX);
        REX->set.dcbmode = (crs << DCB_CRS_SHIFT) | PCD_DCB_PROTOCOL;
        REX->set.dcbdata0.bybyte.b3 = data;
        I2_PRINTF (("i2cWrite %d <- %02x\n", crs, data));
}

int
ng1_i2cProbe(struct rex3chip *rex3)
{
        int i;

        REX = rex3;

        ng1_i2cWrite(PCD8584_DATACRS, 0);
        for (i = PCD8584_REGTIME; i; i--)
                if (!(REX->p1.set.status & BACKBUSY))
			break;
        if (i == 0) {
                REX->p1.set.dcbreset = 0;
                I2_PRINTF (("pcd8584 not found\n"));
		return 0;
        }
	_i2cRead = &ng1_i2cRead;
	_i2cWrite = &ng1_i2cWrite;

        return 1;
}

int
i2cSetup(void)
{
	/* Set own address to 7f */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_ADRREG);
	i2cWrite(PCD8584_DATACRS, PCD8584_OWNADR);

	/* Set clock 12MHz, baud 90 KHz */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_CLKREG);
	i2cWrite(PCD8584_DATACRS, PCD8584_CLK12MHZ | PCD8584_BAUD90KHZ);

	/* Enable I2C bus */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG);

	return 1;
}


static int
i2cStart(int addr)
{
	int i, status;

	/* Enable I2C bus */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG);

	/* Verify no interrupt and bus not busy */
	i2cRead(PCD8584_CTRLCRS, &status);
	if (status != (PCD8584_STAT_PIN | PCD8584_STAT_BB)) {
		I2_PRINTF (("i2cStart: bus busy\n"));
		i2cStop();
		return 0;
	}

	/* Set address of slave */
	i2cWrite(PCD8584_DATACRS, addr);

	/* Generate I2C START and send address */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG | PCD8584_CTL_STA);

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
		I2_PRINTF (("i2cStart: address transmit timeout\n"));
		i2cStop();
		return 0;
	}

	/* Check that slave ack'd */
	if (status & PCD8584_STAT_LRB) {
		I2_PRINTF (("i2cStart: address transmit nack\n"));
		i2cStop();
		return 0;
	}

	/* If read, start i2c read sequence (register value is garbage) */
	if (addr & PCD8584_DATA_READ)
		i2cRead(PCD8584_DATACRS, &status);

	return 1;
}


static int
i2cReceive(int *data)
{
	int i, status;

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
		I2_PRINTF (("i2cReceive: timeout\n"));
		i2cStop();
		return 0;
	}

	/* Check for no errors */
	if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
	    PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
		I2_PRINTF (("i2cReceive: error\n"));
		i2cStop();
		return 0;
	}

	/* Read byte */
	i2cRead(PCD8584_DATACRS, data);
	return 1;
}


static int
i2cSend(int data)
{
	int i, status;

	/* Write byte */
	i2cWrite(PCD8584_DATACRS, data);

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
		I2_PRINTF (("i2cSend: timeout\n"));
		i2cStop();
		return 0;
	}

	/* Check for no errors */
	if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
	    PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
		I2_PRINTF (("i2cSend: error\n"));
		i2cStop();
		return 0;
	}

	/* Check that destination ack'd */
	if (status & PCD8584_STAT_LRB) {
		I2_PRINTF (("i2cSend: nack\n"));
		i2cStop();
		return 0;
	}

	return 1;
}


static int
i2cStop(void)
{
	int i, status;

	/* Generate I2C STOP and reset interrupt */
	i2cWrite(PCD8584_CTRLCRS, PCD8584_CTL_PIN | PCD8584_CSDREG | 
	    PCD8584_CTL_STO);
	for (i = PCD8584_STOPTIME; i; i--) {
		i2cRead(PCD8584_CTRLCRS, &status);
		if (status == (PCD8584_STAT_PIN | PCD8584_STAT_BB))
			break;
	}
	if (i == 0) {
		I2_PRINTF (("i2cStop: timeout\n"));
		return 0;
	}
	return 1;
}


static int
i2cPortRead(int addr, int *data)
{
	return i2cStart(addr | PCD8584_DATA_READ) &&
	    i2cReceive(data) &&
	    i2cStop();
}


static int
i2cPortWrite(int addr, int data)
{
	I2_PRINTF (("i2cPortWrite(%02x, %02x)\n", addr, data));
	return i2cStart(addr | PCD8584_DATA_WRITE) &&
	    i2cSend(data) &&
	    i2cStop();
}

static void
dcb_fifowait(void)          /* wait for dcb fifo empty */
{
	int timeout;

	/* Only check DCB fifo on GR2 graphics */

	if (_i2cWrite == &gr2_i2cWrite)
    	{
	    /* Wait for XMAP external fifo to empty
	     * to avoid bus contention
	     */
		for (timeout = 0; timeout < 1000; timeout++) {
		/*
		 * fifostatus & 1 => not empty,
		 *              2 => not half full
		 */
		if (GR2->xmap[0].fifostatus & 1)
			DELAY(2);
		else
			break;
		}
	}
}

int
i2cPanelID(int *id)
{
	dcb_fifowait();
	if (i2cPortRead(PCD8574_OWNADR, id) == 0)
		return 0;
	*id = (*id >> PCD8574_IDSHIFT) & PCD8574_IDMASK;
	return 1;
}

int
i2cPanelOn(void)
{
  fp_poweroff = i2cPanelOff;
	dcb_fifowait();
  return i2cPortWrite(PCD8574_OWNADR,
		      PCD8574_ID | 
		      PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT | 
		      PCD8574_ON) &&
	 i2cAudioVolLeft(0xf6) &&
	 i2cAudioVolRight(0xf6) &&
	 i2cAudioBass(0xf6) &&
	 i2cAudioTreble(0xf6) &&
	 i2cAudioSwitch(0xce) &&
	 i2cBlUp(100) &&
	 i2cSetBit(1);
}

static int
i2cAudioVolLeft(int volLeft)
{
	return i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cSend(PCD8425_VLADR) &&
	    i2cSend(volLeft) &&
	    i2cStop();
}


static int
i2cAudioVolRight(int volRight)
{
	return i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cSend(PCD8425_VRADR) &&
	    i2cSend(volRight) &&
	    i2cStop();
}

static int
i2cAudioBass(int bass)
{
	return i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cSend(PCD8425_BSADR) &&
	    i2cSend(bass) &&
	    i2cStop();
}

static int
i2cAudioTreble(int treble)
{
	return i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cSend(PCD8425_TRADR) &&
	    i2cSend(treble) &&
            i2cStop();
}

static int
i2cAudioSwitch(int flags)
{
	return i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    i2cSend(PCD8425_SWADR) &&
	    i2cSend(flags) &&
	    i2cStop();
}

void
i2cPanelOff(void)
{
	dcb_fifowait();
  i2cPortWrite(PCD8574_OWNADR, 
	       PCD8574_ID |
	       PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
	       PCD8574_OFF);
}


static int
i2cSetBit(int bit)
{
    /* set to 18 bit emulation */
    if (bit) {
	corona_18_bit = 1;
    	return (i2cPortWrite(PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			     PCD8574_BIT));
    }
    else { /* set to 15 bit emulation */
	corona_18_bit = 0;
	return (i2cPortWrite(PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT));
    }
}


static int
i2cBlUp(int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

	while (steps--) {
		if (!i2cPortWrite(PCD8574_OWNADR, 
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) ||
	      	    !i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INC | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			PCD8574_ON | bit))
			return 0;
	      }
	return 1;
}

#if 0
static int
i2cBlDown(int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

  while (steps--) {
    if (!i2cPortWrite(PCD8574_OWNADR, 
		      PCD8574_ID |
		      PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
		      PCD8574_ON | bit) || 
	!i2cPortWrite(PCD8574_OWNADR,
		      PCD8574_ID |
		      PCD8574_DOWN | PCD8574_INC | PCD8574_CS |
		      PCD8574_ON | bit) || 
	!i2cPortWrite(PCD8574_OWNADR,
		      PCD8574_ID |
		      PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
		      PCD8574_ON | bit) || 
	!i2cPortWrite(PCD8574_OWNADR,
		      PCD8574_ID |
		      PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
		      PCD8574_ON | bit))
      return 0;
  }
  return 1;
}
#endif

#endif	/* IP22 || IP24 || IP26 || IP28 */
