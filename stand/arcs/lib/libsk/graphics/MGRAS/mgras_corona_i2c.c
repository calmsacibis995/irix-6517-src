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
 * Support for flat panel I2C controls.
 *
 * $Revision: 1.8 $
 */

#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/param.h"
#include "arcs/errno.h"
#include "libsk.h"
#include "sys/mgrashw.h"
#include "sys/fpanel.h"
#include "sys/pcd.h"

static struct mgras_hw *MG;

static void mgras_i2cRead(int, int *);
static void mgras_i2cWrite(int, int );
static int mgras_i2cStart(int);
static int mgras_i2cReceive(int *);
static int mgras_i2cSend(int);
static int mgras_i2cStop(void);
static int mgras_i2cPortRead(int, int *);
static int mgras_i2cPortWrite(int , int);
static int mgras_i2cBlUp(int);
static int mgras_i2cBlDown(int);
static int mgras_i2cAudioVolLeft(int);
static int mgras_i2cAudioVolRight(int);
static int mgras_i2cAudioBass(int);
static int mgras_i2cAudioTreble(int);
static int mgras_i2cAudioSwitch(int);
static int mgras_i2cSetBit(int);

static int corona_bl;
static int corona_left;
static int corona_right;
static int corona_bass;
static int corona_treble;
static int corona_mute;
static int corona_18_bit;

int mgras_i2cPanelOff(void);

extern int (*fp_poweroff)();

/*
 * Access i2c bus via one of two address spaces.
 * The first is for transfers with hardware acknowledge,
 * the second is without acknowledge.
 * The nack address space is just used when probing for
 * the i2c bus.
 */

#define pcdData ((volatile unsigned char *)(&MG->mco_mpa1.mpa1.crs0))[0]
#define pcdControl ((volatile unsigned char *)(&MG->mco_mpa1.mpa1.crs1))[0]


#define DCB_FIFOWAIT

static void
mgras_i2cRead(int crs, int *data)
{
	DELAY (1);
	DCB_FIFOWAIT;
	if (crs == PCD8584_CTRLCRS)
		*data = pcdControl;
	else {
		ASSERT (crs == PCD8584_DATACRS);
		*data = pcdData;
	}
#if defined(CORONA_DEBUG) && 1
	printf ("mgras_i2cRead %d -> %02x\n", crs, *data);
#endif 
}

static void
mgras_i2cWrite(int crs, int data)
{
	DELAY (1);
	DCB_FIFOWAIT;
	if (crs == PCD8584_CTRLCRS)
		pcdControl = data;
	else {
		ASSERT (crs == PCD8584_DATACRS);
		pcdData = data;
	}
#if defined(CORONA_DEBUG) && 1
	printf ("mgras_i2cWrite %d <- %02x\n", crs, data);
#endif
}

int
mgras_i2cProbe(struct mgras_hw *mg)
{
        volatile int i;

	if  (MG && mg != MG) return 0;
	MG = mg;

	MG->dcbctrl_11 = HQ3_MPA1_PROTOCOL_NOACK;


	pcdData = 0;		/* set pcd interface mode */
	DELAY (1);
	pcdControl = 0x40;	/* set ES0 (not refer to s0 prime ) */
	DELAY (1);
        pcdData = 0x55;
	DELAY (1);
	pcdControl = 0x40;	/* an innocuous write */
	DELAY(1);
        i = 0;                  /* put other values on system bus */
	
	pcdControl;		/* read the previous write */
	DELAY (1);

        if (pcdData != 0x55)
                goto fail;       /* probe failed */
	DELAY(1);
        pcdData = 0xaa;
	DELAY(1);
	pcdControl = 0x40;	/* an innocuous write */
	DELAY(1);
        i = 0;                  /* put other values on system bus */
	
	pcdControl;		/* read the previous write */
	DELAY (1);
        if (pcdData != 0xaa)
                goto fail;       /* probe failed */

	MG->dcbctrl_11 = HQ3_MPA1_PROTOCOL_ASYNCACK;
        return 1;

fail:
	MG = NULL;
	return 0;
}

int
mgras_i2cSetup(void)
{
	/* Set own address to 7f */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_ADRREG);
	mgras_i2cWrite(PCD8584_DATACRS, PCD8584_OWNADR);

	/* Set clock 12MHz, baud 90 KHz */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CLKREG);
	mgras_i2cWrite(PCD8584_DATACRS, PCD8584_CLK12MHZ | PCD8584_BAUD90KHZ);

	/* Enable I2C bus */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG);

	return 1;
}


static int
mgras_i2cStart(int addr)
{
	int i, status;

	/* Enable I2C bus */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG);

	/* Verify no interrupt and bus not busy */
	mgras_i2cRead(PCD8584_CTRLCRS, &status);
	if (status != (PCD8584_STAT_PIN | PCD8584_STAT_BB)) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cStart: bus busy\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Set address of slave */
	mgras_i2cWrite(PCD8584_DATACRS, addr);

	/* Generate I2C START and send address */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CSDREG | PCD8584_CTL_STA);

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		mgras_i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cStart: address transmit timeout\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Check that slave ack'd */
	if (status & PCD8584_STAT_LRB) {
#if defined(CORONA_DEBUG) && 0
		printf ("mgras_i2cStart: address transmit nack\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* If read, start mgras_i2c read sequence (register value is garbage) */
	if (addr & PCD8584_DATA_READ)
		mgras_i2cRead(PCD8584_DATACRS, &status);

	return 1;
}


static int
mgras_i2cReceive(int *data)
{
	int i, status;

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		mgras_i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cReceive: timeout\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Check for no errors */
	if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
	    PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cReceive: error\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Read byte */
	mgras_i2cRead(PCD8584_DATACRS, data);
	return 1;
}


static int
mgras_i2cSend(int data)
{
	int i, status;

	/* Write byte */
	mgras_i2cWrite(PCD8584_DATACRS, data);

	/* Poll with timeout for PIN bit clearing */
	for (i = PCD8584_XFRTIME; i; i--) {
		mgras_i2cRead(PCD8584_CTRLCRS, &status);
		if ((status & PCD8584_STAT_PIN) == 0) break;
	}
	if (i == 0) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cSend: timeout\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Check for no errors */
	if ((status & (PCD8584_STAT_STS | PCD8584_STAT_BER | PCD8584_STAT_AAS |
	    PCD8584_STAT_LAB | PCD8584_STAT_BB))) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cSend: error\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	/* Check that destination ack'd */
	if (status & PCD8584_STAT_LRB) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cSend: nack\n");
#endif
		mgras_i2cStop();
		return 0;
	}

	return 1;
}


static int
mgras_i2cStop(void)
{
	int i, status;

	/* Generate I2C STOP and reset interrupt */
	mgras_i2cWrite(PCD8584_CTRLCRS, PCD8584_CTL_PIN | PCD8584_CSDREG | 
	    PCD8584_CTL_STO);
	for (i = PCD8584_STOPTIME; i; i--) {
		mgras_i2cRead(PCD8584_CTRLCRS, &status);
		if (status == (PCD8584_STAT_PIN | PCD8584_STAT_BB))
			break;
	}
	if (i == 0) {
#if defined(CORONA_DEBUG) && 1
		printf ("mgras_i2cStop: timeout\n");
#endif
		return 0;
	}
	return 1;
}


static int
mgras_i2cPortRead(int addr, int *data)
{
	return mgras_i2cStart(addr | PCD8584_DATA_READ) &&
	    mgras_i2cReceive(data) &&
	    mgras_i2cStop();
}


static int
mgras_i2cPortWrite(int addr, int data)
{
#if defined(CORONA_DEBUG) && 1
	printf ("mgras_i2cPortWrite(%02x, %02x)\n", addr, data);
#endif
	return mgras_i2cStart(addr | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(data) &&
	    mgras_i2cStop();
}

int
mgras_i2cPanelID(int *id)
{
	DCB_FIFOWAIT;
	if (mgras_i2cPortRead(PCD8574_OWNADR, id) == 0)
		return 0;
	*id = (*id >> PCD8574_IDSHIFT) & PCD8574_IDMASK;
	return 1;
}

int
mgras_i2cPanelOn(void)
{
        corona_bl = 0;
	corona_mute = 1;
	corona_left = 0;
	corona_right = 0;
	corona_bass = 0;
	corona_treble = 0;
	corona_18_bit = 0;

	fp_poweroff = mgras_i2cPanelOff;
	
	DCB_FIFOWAIT;
	return mgras_i2cPortWrite(PCD8574_OWNADR,
		   PCD8574_ID | 
		   PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT | 
		   PCD8574_ON) &&
		mgras_i2cAudioVolLeft(0xf6) &&
		mgras_i2cAudioVolRight(0xf6) &&
		mgras_i2cAudioBass(0xf6) &&
		mgras_i2cAudioTreble(0xf6) &&
		mgras_i2cAudioSwitch(0xce) &&
		mgras_i2cBlUp(CORONA_BL_MAX) &&
	        mgras_i2cBlDown(10) &&
		mgras_i2cSetBit(0);
}

int
mgras_i2cPanelOff(void)
{
	if (!MG) return 1;

	DCB_FIFOWAIT;
    	return mgras_i2cPortWrite(PCD8574_OWNADR, 
		 PCD8574_ID |
		 PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
		 PCD8574_OFF);
}

static int
mgras_i2cBlUp(int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

	while (steps--) {
		if (!mgras_i2cPortWrite(PCD8574_OWNADR, 
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) ||
	      	    !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INC | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			PCD8574_ON | bit))
			return 0;
		if (corona_bl < CORONA_BL_MAX) corona_bl++;
	      }
	return 1;
}

static int
mgras_i2cBlDown(int steps)
{
	int bit = 0;
	if (corona_18_bit)
		bit = PCD8574_BIT;

	while (steps--) {
		if (!mgras_i2cPortWrite(PCD8574_OWNADR, 
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INC | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_DOWN | PCD8574_INCNOT | PCD8574_CS |
			PCD8574_ON | bit) || 
	            !mgras_i2cPortWrite(PCD8574_OWNADR,
			PCD8574_ID |
			PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			PCD8574_ON | bit))
			return 0;
		if (corona_bl > CORONA_BL_MIN) corona_bl--;
	}
	return 1;
}


static int
mgras_i2cAudioVolLeft(int volLeft)
{
        corona_left = volLeft;
	return mgras_i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(PCD8425_VLADR) &&
	    mgras_i2cSend(volLeft) &&
	    mgras_i2cStop();
}


static int
mgras_i2cAudioVolRight(int volRight)
{
        corona_right = volRight;
	return mgras_i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(PCD8425_VRADR) &&
	    mgras_i2cSend(volRight) &&
	    mgras_i2cStop();
}

static int
mgras_i2cAudioBass(int bass)
{
        corona_bass = bass;
	return mgras_i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(PCD8425_BSADR) &&
	    mgras_i2cSend(bass) &&
	    mgras_i2cStop();
}

static int
mgras_i2cAudioTreble(int treble)
{
        corona_treble = treble;
	return mgras_i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(PCD8425_TRADR) &&
	    mgras_i2cSend(treble) &&
            mgras_i2cStop();
}

static int
mgras_i2cAudioSwitch(int flags)
{
        if (flags & 0x20) corona_mute = 1;
	else corona_mute = 0;
	return mgras_i2cStart(PCD8425_OWNADR | PCD8584_DATA_WRITE) &&
	    mgras_i2cSend(PCD8425_SWADR) &&
	    mgras_i2cSend(flags) &&
	    mgras_i2cStop();
}

static int
mgras_i2cSetBit(int bit)
{
    /* set to 18 bit emulation */
    if (bit) {
	corona_18_bit = 1;
    	return (mgras_i2cPortWrite(PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT |
			     PCD8574_BIT));
    }
    else { /* set to 15 bit emulation */
	corona_18_bit = 0;
	return (mgras_i2cPortWrite(PCD8574_OWNADR,
			     PCD8574_ID | 
			     PCD8574_UP | PCD8574_INCNOT | PCD8574_CSNOT));
    }
}

int
mgras_i2c_command(struct I2C_CMD_ARG *cmd_arg)
{
  if (MG == NULL)
	return ENODEV;

  DCB_FIFOWAIT;	/* XXX not needed in some cases */
  switch(cmd_arg->cmd)
    {
    case I2C_PANEL_ON:
      cmd_arg->return_val = mgras_i2cPanelOn();
      break;
    case I2C_PANEL_OFF:
      cmd_arg->return_val = mgras_i2cPanelOff();
      break;
    case I2C_BL_UP:
      cmd_arg->return_val = mgras_i2cBlUp(*((int *)cmd_arg->args));
      break;
    case I2C_BL_DOWN:
      cmd_arg->return_val = mgras_i2cBlDown(*((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_VOL_LEFT:
      cmd_arg->return_val = mgras_i2cAudioVolLeft(*((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_VOL_RIGHT:
      cmd_arg->return_val = mgras_i2cAudioVolRight(*((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_BASS:
      cmd_arg->return_val = mgras_i2cAudioBass(*((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_TREBLE:
      cmd_arg->return_val = mgras_i2cAudioTreble(*((int *)cmd_arg->args));
      break;
    case I2C_AUDIO_SWITCH:
      cmd_arg->return_val = mgras_i2cAudioSwitch(*((int *)cmd_arg->args));
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
      cmd_arg->return_val = mgras_i2cSetBit(*((int *)cmd_arg->args));
      break;
    case I2C_BIT_VAL:
      cmd_arg->return_val = corona_18_bit;
      break;
    default :
      cmn_err (CE_WARN, "mgras_i2c: Unknown command %d\n", cmd_arg->cmd);
      return EINVAL;
  }
  return 0;
}




