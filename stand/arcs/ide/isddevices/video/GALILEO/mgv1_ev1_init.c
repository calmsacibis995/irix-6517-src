/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       ev1_init.c
**      $Revision: 1.1 $
*/

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include <fault.h>
#include "mgv1_ev1_I2C.h"
#include "ide_msg.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

/*
** External Variables and Function Declarations
*/

extern mgras_hw *mgbase;
extern unsigned sysctlval;

/*
** Local Variables and Function Declarations
*/

void mgras_init_dcbctrl(); /*t mgras_hw  *mgrasbase);*/

#define NOT_COMMON 0
#define HQ3

#define DCBCTRL_CC1_OFFSET    0x68030
#define DCBCTRL_AB1_OFFSET    0x6802C
                              /*set in mgras_init_dcb */
#define HQ3_CC1_PROTOCOL \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(1) |  \
   MGRAS_DCB_CSWIDTH(2) | DCB_NOACK )
/*#define HQ3_CC1_PROTOCOL \
  (MGRAS_DCB_CSSETUP(4) | MGRAS_DCB_CSHOLD(4) |  \
   MGRAS_DCB_CSWIDTH(4) | DCB_NOACK ) */

#define HQ3_CC1_PROTOCOL_SYNC_FRAME \
  (MGRAS_DCB_CSSETUP(0) | MGRAS_DCB_CSHOLD(0) |  \
   MGRAS_DCB_CSWIDTH(0) | MGRAS_DCB_ENSYNCACK)

/*#define HQ3_AB1_PROTOCOL \
  (MGRAS_DCB_CSSETUP(2) | MGRAS_DCB_CSHOLD(2) |  \
   MGRAS_DCB_CSWIDTH(2) | MGRAS_DCB_ENASYNCACK)*/

#define HQ3_AB1_PROTOCOL \
  (MGRAS_DCB_CSSETUP(4) | MGRAS_DCB_CSHOLD(4) |  \
   MGRAS_DCB_CSWIDTH(4) | MGRAS_DCB_ENASYNCACK)


struct reg_set {
        int reg;
        int val;
};


/* CC1 - INDIRECTLY ADDRESSED HOST REGS :: Initialize to Safe Default Values */
struct reg_set mgv1_cc_reg_default[] = {

	0	,0x0,
	1	,0xa,
	2	,0,
	3	,0xa,
	4	,0,
	
	8	,0,
	9	,0xff,	
	10	,0xc,
	11	,0,
	12	,0xff,	
	13	,0,
	14	,130,

	16	,0,
	17	,0,
	18	,0x10,
	19	,0,
	20	,0,
	21	,0,
	22	,0,

	24	,0,
	25	,0,
	26	,0x10,
	27	,0,
	28	,0,
	29	,0,
	30	,0,
	
	32	,0x0A,  /*0x02,   */
	33	,0x61,
	34	,0x34,
	35	,0xC,

	36	,0,	
	37	,0x10,
	
	38	,0x20, 
	39	,0,
	40	,0,
	41	,0,
	42	,0,
	43	,0,
	44	,0,
	45	,0,
	46	,0,
	47	,0,
	48	,0,
	49	,0,
	50	,0,  
	51	,0x80,
	52	,0x80,
	53	,0x80,
	54	,0,
	55	,0,
	56	,0x30,
	57	,0,
	58	,0,
	59	,5,
	
	64	,0,
	65	,0,
	66	,0,
	67	,0,
	68	,0,
	69	,0,
	70	,0,
	71	,0,
	72	,0,
	73	,0,
	74	,0,
	75	,0,
	76	,0,
	77	,0,
	78	,0,
	79	,0,
	80	,0,
	-1, -1
	
};

#if NOT_COMMON

/* 
	AB internel regs default 
*/
struct reg_set ab_int_default[] = {
	0x0, 0,
	0x1, 0,
	0x2, 0,
	0x3, 0, 
	0x4, 0x20,
	0x5, 0,
	0x6, 3, 	/* LEAVE THIS ALONE - DILIP - FOR DIAG FREEZE BIT*/
	0x7, 0, 

	0x10, 0,
	0x11, 0,
	0x12, 0,
	0x13, 0,
	0x14, 0,
	0x15, 2,
	0x16, 3,	/* LEAVE THIS ALONE - DILIP - FOR DIAG FREEZE BIT*/
	0x17, 0,

	0x20, 0,
	0x21, 0,
	0x22, 0,
	0x23, 0,
	0x24, 0,
	0x25, 2,
	0x26, 1,	/* LEAVE THIS ALONE - DILIP - FOR DIAG FREEZE BIT*/
	-1,-1
};
#endif

#if defined(IP22)
extern int Ng1ProbeAll(char **);
#endif

extern int	GVType;

#define 	GALILEO_JR	3

static int
mgv1_Galileo_hw_probe(mgras_hw *mgbase, struct ev1_info *info)
{
	char *tmp[4];
	int GFXversion, Videoversion;

#ifndef HQ3	
#if defined(IP22)
	if (!is_fullhouse ()) {

		int t;
		jmp_buf faultbuf;

		if (setjmp (faultbuf)) {
			return (-1);
		}
		nofault = faultbuf;
		CC_W1 (mgbase, indrct_addreg, 128);
		Videoversion = CC_R1 (mgbase, indrct_datareg);
		nofault = 0;
		GVType = GVType_ng1;
		return (0);
	}
#endif
	if (base->hq.mystery == 0xdeadbeef)  {
		/* gfx board found */
		GFXversion = ~(base->bdvers.rd0) & 0xf;
		GFXversion = GFXversion;
		msg_printf(VRB, "GFX Board, GFXversion= 0x%x \n" ,GFXversion);
		GVType = GVType_gr2;
		if (GFXversion < 4) 
			Videoversion = (base->bdvers.rd2 & 0x30) >> 4;
		else
			Videoversion = (base->bdvers.rd1 & 0xC) >> 2;

		Videoversion = ~Videoversion & 0x3 ;
	} else if (base->hq.mystery != 0xdeadbeef) {


#if defined(IP22)
		if (is_fullhouse()) {

			Videoversion = GALILEO_JR;
			GVType = GVType_ng1;
			msg_printf(DBG, "rex3chip addr: 0x%x, base: 0x%x, GVType: %d, Videoversion: %d\n", REX, base, GVType, Videoversion);

  			msg_printf(DBG, "rex3chip addr: 0x%x\n", REX);
#ifdef DOOM
			if (!(Ng1ProbeAll(tmp))) {
				msg_printf(SUM, "Graphics Board not found.\n");
				GVType = 0;
				return(-1);
			}

			/* Check for Newport Gfx here */
			lg3BdVersGet(REX, GFXversion );
			Videoversion = ~GFXversion & 0x3 ;
			msg_printf(DBG, "GFX Board, GFXversion= 0x%x \n" ,GFXversion);
#endif
		}
#endif
	}

#endif

#ifdef HQ3

	Videoversion = GALILEO_JR;
	GVType = GVType_ng1;
#endif

	info->BoardRev = Videoversion ; /* Start from rev. 1 */
	msg_printf(VRB, "GFXversion= 0x%x, Videoversion= 0x%x\n" ,GFXversion, info->BoardRev);

       	if (Videoversion == 0) {
		msg_printf(SUM,"video board not installed \n");
		return -1;
	}

	if (info->BoardRev == GALILEO_JR) 
		msg_printf(VRB, "GALILEO Board Found\n");

	return 0;
}

static int
_Forever_Print_I2C_Ctl(mgras_hw *mgbase)
{
	int status;

	status = CC_R1 (mgbase, I2C_bus);
       	msg_printf(VRB,"Forever I2C_Bus Control status=0x%x\n", status);
	DELAY(1000);

        return(0);
}

static int
_Print_I2C_Ctl(mgras_hw *mgbase)
{
	int status;

	status = CC_R1 (mgbase, I2C_bus);
        msg_printf(DBG,"I2C_Bus Control status=0x%x\n", status);

        return(0);
}

static
_trigger(mgras_hw *mgbase)
{
	CC_W1 (mgbase, indrct_addreg, 7);
	CC_W1 (mgbase, indrct_addreg, 37);
	CC_W1 (mgbase, indrct_datareg, 0x14);
}

static int
_I2C_force_idle(mgras_hw *mgbase)
{
	int status;

	_Print_I2C_Ctl(mgbase);

	CC_W1 (mgbase, I2C_bus, (I2C_RELEASE|I2C_FORCE_IDLE |I2C_128 | I2C_WRITE));
	DELAY(100);
	_Print_I2C_Ctl(mgbase);
status = CC_R1 (mgbase, I2C_bus);
	if (status & I2C_ERR){
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_force_idle: I2C_ERR is set, status=0x%x\n", status);
		return -1;
	}
	return (0);
}

static int
_I2C_wait_for_not_idle(mgras_hw *mgbase)
{
	int status;
        int idle_wait = 0;

	while (!((status = (CC_R1(mgbase, I2C_bus))) & I2C_NOT_IDLE)){
		if (idle_wait > MAX_IDLE_WAIT) 
			break;

		idle_wait++;
		DELAY(5);
	}	


	/* Is the I2C Bus STUCK ? */
	if (idle_wait > MAX_IDLE_WAIT) {
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_not_idle: SLAVE Device holding on to I2C Bus - TIMEOUT...status=0x%x\n" ,status);
                return(-1);
        }

	if (status & I2C_ERR){
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_not_idle: I2C_ERR is set, but I2C is not idle okay, status= 0x%x\n", status);
		return -1;
	}

        return(0);
}

static int
_I2C_wait_for_idle(mgras_hw *mgbase)
{
	int status;
        int idle_wait = 0;

	while ((status = (CC_R1 (mgbase, I2C_bus))) & I2C_NOT_IDLE) {
		if (idle_wait > MAX_IDLE_WAIT)
			break;

		idle_wait++;
		DELAY(5);
	}	

	if (idle_wait > MAX_IDLE_WAIT) {
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_idle: cannot get idle I2C bus - TIMEOUT...status=0x%x\n", status);
                return(-1);
        }

	if (status & I2C_ERR){
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_idle: I2C_ERR is set, but I2C idled okay, status=0x%x\n", status);
		return -1;
	}

        return(0);
}

static int
_I2C_wait_for_ack(mgras_hw *mgbase)
{
	int retry, status;
	int ack_wait = 0;

	while ((status = (CC_R1 (mgbase, I2C_bus))) & I2C_NOT_ACK) {
		if (ack_wait > MAX_ACK_WAIT)
			break;

		ack_wait++;
		DELAY(10);
	}

	if (ack_wait > MAX_ACK_WAIT) {
		_trigger(mgbase);
		msg_printf(ERR,"_I2C_wait_for_ack: Acknowledge not received - TIMEOUT...status=0x%x\n", status);
		return -1;
	}
	if (status & I2C_ERR) {
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_ack: I2C_ERR is set, but I2C acked okay, status=0x%x\n", status);
		return -1;
	}
	return 0;
}

static int
_I2C_wait_for_xfer(mgras_hw *mgbase)
{
	int status;
	int xfer_wait = 0;

	while ((status = (CC_R1 (mgbase, I2C_bus))) & I2C_XFER_BUSY) {
		if (xfer_wait > MAX_XFER_WAIT) 
			break;

		xfer_wait++;
		DELAY(1);
	}

	if(xfer_wait > MAX_XFER_WAIT) {
		_trigger(mgbase);
		msg_printf(ERR,"_I2C_wait_for_xfer: transfer still busy - TIMEOUT...status=0x%x\n", status);
		return -1;
	}

	if (status & I2C_ERR) {
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_xfer: I2C_ERR is set, but I2C xfered okay, status=0x%x\n", status);
		return -1;
	}

	return 0;
}

#define MAX_LONGXFER_WAIT 250000
static int
_I2C_wait_for_longxfer(mgras_hw *mgbase)
{
	int status;
	int xfer_wait = 0;

	DELAY(MAX_LONGXFER_WAIT/5);
	while ((status = (CC_R1 (mgbase, I2C_bus))) & I2C_XFER_BUSY) {

		if (xfer_wait > MAX_LONGXFER_WAIT)
			break;

		xfer_wait++;
		DELAY(5);
	}
	if(xfer_wait > MAX_LONGXFER_WAIT) {
		_trigger(mgbase);
		msg_printf(ERR,"_I2C_wait_for_longxfer: transfer still busy - TIMEOUT...status=0x%x\n", status);
		return -1;
	}

	if (status & I2C_ERR) {
		_trigger(mgbase);
                msg_printf(ERR,"_I2C_wait_for_longxfer: I2C_ERR is set, but I2C long xfered okay..status=0x%x\n", status);
		return -1;
	}

	return 0;
}

static int
I2C_status(mgras_hw *mgbase, char addr ) 
{
	unsigned char val;
	int retry;
	int status;
        int idle_wait = 0;


	_Print_I2C_Ctl(mgbase);
        /* force idle if necessary */
	if (CC_R1 (mgbase, I2C_bus) & I2C_NOT_IDLE) {
                if (_I2C_force_idle(mgbase))
			return -1;
        }

        /* Setup for WRITE */
	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));

        /* write to slave_address, and specify the read opertion */
	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_data, (addr | 0x1)); /* 1 for read, 0 for write */

        if (_I2C_wait_for_xfer(mgbase))
		return -1;
        if (_I2C_wait_for_ack(mgbase))
		return -1;

        /* Toggle the read bit. */
	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_bus, (I2C_HOLD |I2C_128 | I2C_READ | I2C_NOT_IDLE));	
        if (_I2C_wait_for_xfer(mgbase))
		return -1;

	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_bus, (I2C_HOLD |I2C_128 | I2C_WRITE | I2C_NOT_IDLE));	

        /* Status read back from Philips */
	_Print_I2C_Ctl(mgbase);
	val = CC_R1 (mgbase, I2C_data);

repeat_read:

	idle_wait = 0;
	++retry;
	while (!((status = (CC_R1(mgbase, I2C_bus))) & I2C_NOT_IDLE)){
		if (idle_wait > MAX_IDLE_WAIT) 
			break;

		idle_wait++;
		DELAY(5);
	}	


	/* Is the I2C Bus STUCK ? */
	if (idle_wait > MAX_IDLE_WAIT) {
		/* This is PASS condition here! */
		/* DO nothing */
		return (val);

        } else {
		/* This is an error situation  - here only!*/
		/* We will do something to fix it */
        	if (_I2C_wait_for_xfer(mgbase))
			return -1;
        	if (_I2C_wait_for_ack(mgbase))
			return -1;

        	/* Toggle the read bit. */
		_Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_bus, (I2C_HOLD |I2C_128 | I2C_READ | I2C_NOT_IDLE));	
        	if (_I2C_wait_for_xfer(mgbase))
			return -1;
	
		_Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_bus, (I2C_HOLD |I2C_128 | I2C_WRITE | I2C_NOT_IDLE));	

        	/* Status read back from Philips */
		_Print_I2C_Ctl(mgbase);
		val = CC_R1 (mgbase, I2C_data);

		if (retry < 10)  {
			msg_printf(DBG, "I2C_status, %d try pass\n" ,retry);
			goto repeat_read;
		}
		else {
			msg_printf(ERR, "I2C_status, failed after %d tries\n" ,retry);
			return -1;
		}
	}


        return(val);
}




/* Encoder defaults */
static struct reg_set DENC_default[] = {       /* defaults */
            0, 0xAD,	/* was 0xae, which is not standalone mode */ 
            1, 0x00, 
            2, 0x00, 
            3, 0x00, 
            4, 0x60, 
            5, 0x2e, 
            6, 0x52, 
            7, 0x3a, 
            8, 0x0,
            9, 0x0,
            10, 0x0,
            11, 0x0,
            12, 0x0,
            13, 0x0,
            14, 0xc,
            -1, -1,

};


/* Decoder */
extern struct reg_set DMSD_default[];
/*  = {
            0x00, 0x50,
            0x01, 0x30,
            0x02, 0x0, 
            0x03, 0xe8,
            0x04, 0xb6,
            0x05, 0xf4,
            0x06, 0x2, 
            0x07, 0x00,
            0x08, 0xF8,
            0x09, 0xF8,
            0x0A, 0x90,
            0x0B, 0x90,
            0x0C, 0x00,
            0x0D, 0xc,  
            0x0E, 0x78,
            0x0F, 0x98,
            0x10, 0,  
            0x11, 0x20,
            0x12, 0x0 ,
            0x13, 0x0,
            0x14, 0x44,
            0x15, 0x0a,
            0x16, 0x0,
            0x17, 0xd2,
            0x18, 0xe6,
            -1, -1,
}; */


/* 7192 - Color Space Conversion */
extern struct reg_set CSC_default[];
/* = { 0,      0x3A,
        -1, -1,
};
*/


/* Digital to Analog conversion */
static struct reg_set DAC_default[] = {  /*old values*/
                0,      0,      /* 0 */
                1,      0x19,   /* 0x19 */
                2,      0x18,   /* 0x18,*/
                3,      0xe,    /* 0xe, */
                4,      0x7f,      /* 20,*/
                5,      0x7f,      /* 20,*/
                6,      0x7f,      /* 20,*/
                7,      0x7f,      /* 20, */
                -1, -1,
};


/* Digital to Analog conversion */
struct reg_set INDYCAM_default[] = {  
  		0, 0x01,	/* 0x00 CameraControlStatus	*/
    		1, 0x00,	/* 0x01 CameraShutterSpeed	*/
    		2, 0x80,	/* 0x02 CameraGain		*/
    		3, 0x00, 	/* 0x03 CameraBrightness	*/
    		4, 0x18,	/* 0x04 CameraRedBalance	*/
    		5, 0xA4,	/* 0x05 CameraBlueBalance	*/
    		6, 0x80,	/* 0x06 CameraRedSaturation	*/
    		7, 0xC0,	/* 0x07 CameraBlueSaturation	*/
    		-1, -1	/* termination value		*/
	
};



/*  Initialize 8574 */
extern struct reg_set BUS_default0[];
/* = {
	0, 0x86,
	-1,-1,
}; */

extern struct reg_set BUS_default1[];
/*  = {
	0, 0x6,
	-1,-1,
}; */


/***	8574 Init for GALILEO JUNIOR ONLY */
extern struct reg_set Junior_BUS_default0[];
/* = {
	0, 0x8E,
	-1,-1,
}; */

extern struct reg_set Junior_BUS_default1[];
/*  = {
	0, 0xE,
	-1,-1,
}; */


/***/
/*
 * Initialize 7151 - SQP
 */

extern struct reg_set D1_default[];
/*  = {
                0,      0x4d,
                1,      0x3d,
                2,      0xd,
                3,      0xf3,
                4,      0xc6,
                5,      0xfb,
                6,      0x2,
                7,      0,
                8,      0xa9,
                9,      0xc0,
                10,     0x4d,
                11,     0x40,
                12,     0x40,
                13,     0x60,
                14,     0xb0,
                15,     0xd5,
                16,     0xc0,
                17,     0x3d,
                18,     0xc0,
                -1, -1,
}; */


static int
I2C_dev_init(mgras_hw *mgbase, char addr, struct reg_set *data)
{
	int i;

        

	_Print_I2C_Ctl(mgbase);
	/* force idle if necessary */
	if (CC_R1 (mgbase, I2C_bus) & I2C_NOT_IDLE) {
		if ((_I2C_force_idle(mgbase))  == -1)
			return -1;	 
	}

	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));
	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_data, (addr|0)); /* 0 => Write, 1=> read (not supported) */
	msg_printf(DBG,"I2C_DEV= 0x%x \n", (addr|0));

	if ((_I2C_wait_for_not_idle(mgbase)) == -1)
		return -1;
	if ((_I2C_wait_for_xfer(mgbase)) == -1)
		return -1;
	if ((_I2C_wait_for_ack(mgbase)) == -1)
		return -1;
	if (addr == DENC_ADDR) {
		_Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_data, 2);
		msg_printf(DBG,"DENC_ADDR, data =  0x%x \n", 2);
		if ((_I2C_wait_for_not_idle(mgbase)) == -1)
			return -1;
		if ((_I2C_wait_for_xfer(mgbase)) == -1)
			return -1;
		if ((_I2C_wait_for_ack(mgbase)) == -1)
			return -1;
	}


	if (addr != BUS_CONV_ADDR){
		msg_printf(DBG,"data reg =0x%x\n", data->reg);
		_Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_data, data->reg);
		if ((_I2C_wait_for_not_idle(mgbase)) == -1)
			return -1;

		if ((_I2C_wait_for_xfer(mgbase)) == -1)
			return -1;

		if ((_I2C_wait_for_ack(mgbase)) == -1)
			return -1;
	}

	/* Last Byte on the Bus, stop autoInc */
	if (((data+1)->reg) == -1){
		msg_printf(DBG, "Single Transaction Only...\n");
	        _Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
		msg_printf(DBG, "Single Transaction Only...setup Okay\n");
	        _Print_I2C_Ctl(mgbase);
	}
	msg_printf(DBG,"data val =0x%x\n", data->val);
	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_data, data->val);
	if ((_I2C_wait_for_not_idle(mgbase)) == -1)
		return -1;
	if (addr == BUS_CONV_ADDR) {
		if ((_I2C_wait_for_longxfer(mgbase)) == -1) {
			return -1;
		}
	}else {
		if ((_I2C_wait_for_xfer(mgbase)) == -1) {
			return -1;
		}
	}
	if ((_I2C_wait_for_ack(mgbase)) == -1)
		return -1;

	data++;

	/* 
	 * Only write the data, if the registers we are programing are
	 * sequential (register address are auto-incremented).
	 */ 
	while ((data->reg != -1) && ((data-1)->reg + 1 == data->reg)){
		/* Last Byte on the Bus , stop autoInc*/
		if (((data+1)->reg) == -1){
			msg_printf(DBG, "(Loop) Last Byte on the Bus...\n");
			_Print_I2C_Ctl(mgbase);
			CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
			msg_printf(DBG, "Last Byte on the Bus...setup completed\n");
	        	_Print_I2C_Ctl(mgbase);
		}

		msg_printf(DBG,"data val =0x%x\n", data->val);
		_Print_I2C_Ctl(mgbase);
	        CC_W1 (mgbase, I2C_data, data->val);
		if ((_I2C_wait_for_not_idle(mgbase)) == -1)
			return -1;
		if ((_I2C_wait_for_xfer(mgbase)) == -1)
			return -1;
		if ((_I2C_wait_for_ack(mgbase)) == -1)
			return -1;


		data++;

	}
	if ((data->reg != -1) && ((data-1)->reg + 1 != data->reg)){
		msg_printf(DBG,"I2C_dev_init: auto inc mode - noncontiguous regs\n");
	}

#if 0
	/* NOT NECESSARY ANYMORE... */
	/* stop auto inc mode */
	if ((_I2C_force_idle(mgbase)) == -1)
		return -1;
#endif

	return 0;
}




static int
I2C_VLUT(mgras_hw *mgbase, char addr)
{
	int i;
	int x;
	int err_count = 0;


	/* There are 2 ways to init VLUTS.
	 * 1. Write to R,G,B seperately
	 *	To Write to R :: Write to ADDR = 1
	 *	To Write to B :: Write to ADDR = 2
	 *	To Write to G :: Write to ADDR = 3
	 * 2. Write to RGB at the same time.
	 *	To Write to RGB :: Write to ADDR = 4
	 */

	_Print_I2C_Ctl(mgbase);
	/* force idle if necessary */
	if (CC_R1 (mgbase, I2C_bus) & I2C_NOT_IDLE) {
		if (_I2C_force_idle(mgbase))
			return -1;
	}


	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));

	_Print_I2C_Ctl(mgbase);
	CC_W1 (mgbase, I2C_data, addr|0);	/* Select addr reg, Write Mode */
	if (_I2C_wait_for_not_idle(mgbase))
		return -1;
	if (_I2C_wait_for_xfer(mgbase))
		return -1;
	if (_I2C_wait_for_ack(mgbase))
		return -1;

	/* Write to RGB all the same time */
	_Print_I2C_Ctl(mgbase);
        CC_W1 (mgbase, I2C_data, 4); 
	if (_I2C_wait_for_not_idle(mgbase))
		return -1;
	if (_I2C_wait_for_xfer(mgbase))
		return -1;
	if (_I2C_wait_for_ack(mgbase))
		return -1;

	/* Start from VLUT Addr 0 */
	_Print_I2C_Ctl(mgbase);
        CC_W1 (mgbase, I2C_data, 0);
	if (_I2C_wait_for_not_idle(mgbase))
		return -1;
	if (_I2C_wait_for_xfer(mgbase))
		return -1;
	if (_I2C_wait_for_ack(mgbase))
		return -1;

	/* Initialize the color ramp */
	/* Just send the data, addr is auto-inc */
	for (x = 0; x < 256; x++){
		/* Last Byte on the Bus , stop autoInc*/
		if (x == 255){
			msg_printf(DBG, "Last Byte on the Bus...\n");
			_Print_I2C_Ctl(mgbase);
			CC_W1 (mgbase, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
		}
		_Print_I2C_Ctl(mgbase);
		CC_W1 (mgbase, I2C_data, x);
		if (_I2C_wait_for_not_idle(mgbase))
			return -1;
		if (_I2C_wait_for_xfer(mgbase))
			return -1;
		if (_I2C_wait_for_ack(mgbase))
			return -1;

	}
	
	_Print_I2C_Ctl(mgbase);
#if 0
	/* NOT NECESSARY ANYMORE... */
	/* stop auto inc mode */
	if (_I2C_force_idle(mgbase))	 
		return -1;

#endif
	return(0);
}

static int
initI2C_for_indy(mgras_hw *mgbase, struct ev1_info *info)
{
	register int i; 
	extern struct reg_set DENC_default[], DMSD_default[]; 

	msg_printf(VRB,"Initialize I2C devices\n");


	CC_W1 (mgbase, sysctl, 8);

	/* Initialize  Encoder Registers */
	msg_printf(VRB, "\nInitialize  (7199) Encoder Registers..");
	if (I2C_dev_init(mgbase, DENC_ADDR,  DENC_default)) {
		msg_printf(ERR,"(7199) DENC init. failed\n");
		_Forever_Print_I2C_Ctl(mgbase);
		return -1;
	}

	CC_W1 (mgbase, indrct_addreg, 33);
	CC_W1 (mgbase, indrct_datareg, 0x61);
	CC_W1 (mgbase, indrct_addreg, 34);
	CC_W1 (mgbase, indrct_datareg, 0x34);
	CC_W1 (mgbase, indrct_addreg, 35);
	CC_W1 (mgbase, indrct_datareg, 12);

	/* Initialize  Decoder Registers */
	msg_printf(VRB, "\nInitialize  (7191) Square Pixel Decoder Registers..");
	if (I2C_dev_init(mgbase, DMSD_ADDR,  DMSD_default)) {
		msg_printf(ERR,"(7191) Square Pixel DMSD init. failed\n");
		_Forever_Print_I2C_Ctl(mgbase);
		return -1;
	}

	return(0);
}

static int
initI2C(mgras_hw *mgbase, struct ev1_info *info)
{
	register int i, tmp, timeout;
	extern struct reg_set DENC_default[], DMSD_default[]; 
	extern struct reg_set CSC_default[], DAC_default[];

	msg_printf(VRB,"Initialize I2C devices\n");

	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
	if (info->BoardRev != GALILEO_JR) {
		/* Initialize 8574 Registers */
		msg_printf(VRB,"Initialize 8574 Registers  \n");
		if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  BUS_default0)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}
		
		
		if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  BUS_default1)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}
	} else 
	{
		/* Initialize 8574 Registers */
		msg_printf(VRB,"******Initialize 8574 Registers  \n");
		if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  Junior_BUS_default0)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}
		
		if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  Junior_BUS_default1)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}
	}

	CC_W1 (mgbase, sysctl, 8);

	/* Initialize  Encoder Registers */
	msg_printf(VRB, "\nInitialize  (7199) Encoder Registers..");
	if (I2C_dev_init(mgbase, DENC_ADDR,  DENC_default)) {
		msg_printf(ERR,"(7199) DENC init. failed\n");
		_Forever_Print_I2C_Ctl(mgbase);
		return -1;
	}

	CC_W1 (mgbase, indrct_addreg, 33);
	CC_W1 (mgbase, indrct_datareg, 0x61);
	CC_W1 (mgbase, indrct_addreg, 34);
	CC_W1 (mgbase, indrct_datareg, 0x34);
	CC_W1 (mgbase, indrct_addreg, 35);
	CC_W1 (mgbase, indrct_datareg, 12);



	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
	if (info->BoardRev != GALILEO_JR) {
		/* Initialize Color Space Converter Registers */
		msg_printf(VRB, "\nInitialize (7192) Color Space Converter Registers..");
		if (I2C_dev_init(mgbase, CSC_ADDR,  CSC_default)) {
			msg_printf(ERR,"(7192) CSC init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}
	
		/* Initialize 7151 Registers */
		msg_printf(VRB, "\nInitialize (7151) Registers..");
			if (I2C_dev_init(mgbase, D1_ADDR,  D1_default)) {
				msg_printf(ERR,"7151 - init. failed\n");
				_Forever_Print_I2C_Ctl(mgbase);
				return -1;
			}

	}


	/* Initialize  Decoder Registers */
	msg_printf(VRB, "\nInitialize  (7191) Square Pixel Decoder Registers..");
	if (I2C_dev_init(mgbase, DMSD_ADDR,  DMSD_default)) {
		msg_printf(ERR,"(7191) Square Pixel DMSD init. failed\n");
		_Forever_Print_I2C_Ctl(mgbase);
		return -1;
	}


	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
		/* Initialize DAC Registers */
		msg_printf(VRB, "\nInitialize (7169) DAC Registers..");
		if (I2C_dev_init(mgbase, DAC_ADDR,  DAC_default)) {
			msg_printf(ERR,"(7169)DAC init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		      }

	if (info->BoardRev != GALILEO_JR) {
		/* Initialize VLUTs */
		msg_printf(VRB, "\nInitialize (7192) VLUTs.. CSC's");
		if (I2C_VLUT(mgbase, CSC_ADDR)) {
			msg_printf(ERR, "(7192) VLUTs - CSC's init. failed\n");
			_Forever_Print_I2C_Ctl(mgbase);
			return -1;
		}

	      }
	return(0);
}

static void
initCC1(mgras_hw *mgbase, struct ev1_info *info)
{
	register int i, tmp;
	extern struct reg_set mgv1_cc_reg_default[];

	msg_printf(VRB,"Initialize CC1\n");

	/* Initialize CC1 SYSTEM CONTROL REG */
	CC_W1 (mgbase, sysctl, 8);

	/* Initialize INDIRECTLY ADDRESSED HOST REGS */
	i = 0;
	while (mgv1_cc_reg_default[i].reg != -1) {
		CC_W1 (mgbase, indrct_addreg, mgv1_cc_reg_default[i].reg);
		CC_W1 (mgbase, indrct_datareg, mgv1_cc_reg_default[i++].val);	
	}

	CC_W1 (mgbase, fbsel, 0x0);

	CC_W1 (mgbase, indrct_addreg, 128);
	info->CC1Rev = CC_R1 (mgbase, indrct_datareg);
}

static void
initAB1(mgras_hw *mgbase, struct ev1_info *info)
{
	register int i;
	extern struct reg_set ab_int_default[];

	msg_printf(VRB,"Initialize AB1's\n");
	AB_W1 (mgbase, rgb_sel, 0x10); 	/* First Chip ID 0x10, red ab1 */
	msg_printf(DBG, "After first AB init\n");
	AB_W1 (mgbase, rgb_sel, 0x21); 	/* second Chip ID 0x21, green ab1 */
	msg_printf(DBG, "After second AB init\n");
	AB_W1 (mgbase, rgb_sel, 0x42); 	/* Third Chip ID 0x42, blue ab1 */
	msg_printf(DBG, "After third AB init\n");
/*	AB_W1 (mgbase, rgb_sel, 0x83);*//* Fourth Chip ID 0x83, alpha ab1 */
/*      msg_printf(DBG, "After fourth AB init\n");*/
	AB_W1 (mgbase, sysctl, 0x9); 	
	AB_W1 (mgbase, high_addr, 0);
	AB_W1 (mgbase, low_addr, 0);
	AB_W1 (mgbase, tst_reg, 0x4); 

	i = 0;
	msg_printf(DBG, "Before default AB init\n");
	while (ab_int_default[i].reg != -1) {
		AB_W1 (mgbase, high_addr, 0);
		AB_W1 (mgbase, low_addr, ab_int_default[i].reg & 0xff);
		AB_W1 (mgbase, intrnl_reg, ab_int_default[i++].val);
	}
	/* Get AB1 rev. */
	AB_W1 (mgbase, high_addr, 0);
	AB_W1 (mgbase, low_addr, 3);
	info->AB1Rev = AB_R1(mgbase, tst_reg) & 0xff;
	
	msg_printf(DBG, "Very End AB1 init\n");
}

mgv1_ev1_initialize(int argc, char *argv[])
{
	struct ev1_info *info, ev1info;
	int videoinfo;
	int timeout, tmp;
	int err_count = 0;
	int i;
	info = &ev1info;

	bzero((void *)info, sizeof(struct ev1_info));
	msg_printf(DBG, "ev1_init mgbase 0x%x  \n ", mgbase);

	mgras_init_dcbctrl();

	msg_printf(DBG, "mgbase is 0x%x\n", mgbase);
	if (mgv1_Galileo_hw_probe(mgbase, info) != 0) {
		err_count++;
		msg_printf(ERR, "Galileo Video Not Found \n");
		return(-1);
	}

#ifdef IP22
	msg_printf(VRB, "%s version - Galileo Video ...\n", is_fullhouse () ? 
		"Indigo-2" : "Indy");

	if (info->BoardRev == GALILEO_JR) 
		msg_printf(VRB, "GALILEO Board Found\n");
#else
	msg_printf(VRB, "Indigo version  - Galileo Video ...\n");
#endif
	msg_printf(DBG, "mgbase is 0x%x\n", mgbase);

    	CC_W1 (mgbase, sysctl, 8);
	CC_W1 (mgbase, sysctl, 8);
	
	for (i = 1;i < 5; i++) {
		CC_W1(mgbase, indrct_addreg,i);
		CC_W1(mgbase, sysctl, 8);
		msg_printf(DBG, "CC READ should be %d %d \n",
			i, CC_R1(mgbase,indrct_addreg));
	}  
#if defined(IP20)
	if (initI2C(mgbase, info)) {
		err_count++;
		return -1;
	}
#else 

#if defined(IP22)
	if (is_fullhouse ()) {
		if (initI2C(mgbase, info)) {
			err_count++;
			return -1;
		}
	} 
	else {
		/* This is INDY VIDEO */
		if (initI2C_for_indy(mgbase, info)) {
			err_count++;
			return -1;
		}
	}

#endif

#endif
	/* checking whether I2C bus is responding by reading back the status */
	videoinfo = I2C_status(mgbase, DMSD_ADDR);
	if (videoinfo == -1)  {
		msg_printf(ERR,"ev1_init: I2C Check Status failed\n");
		return -1;
	}
		
        if (videoinfo & 0x20)
                msg_printf(VRB, "NTSC 60Hz\n");
        else
                msg_printf(VRB, "PAL 50Hz\n");

	CC_W1 (mgbase, indrct_addreg, 38);
	CC_W1 (mgbase, indrct_datareg, 0x20);

	while ((tmp = (CC_R1 (mgbase, indrct_datareg))) & 0x40) {
		DELAY(1);
		++timeout;
		msg_printf(DBG, "CCIND38= 0x%x\n", tmp);
		if (timeout > 10000){
			msg_printf(ERR, "CCIND38 bit 6 is set \n");
			return -1;
		}
	}
	CC_W1 (mgbase, indrct_addreg, 54);
	CC_W1 (mgbase, indrct_datareg, 0x00);
	CC_W1 (mgbase, indrct_addreg, 55);
	CC_W1 (mgbase, indrct_datareg, 0x00);
	CC_W1 (mgbase, indrct_addreg, 56);
	CC_W1 (mgbase, indrct_datareg, 0x30);
	CC_W1 (mgbase, indrct_addreg, 57);
	CC_W1 (mgbase, indrct_datareg, 0x0);
	CC_W1 (mgbase, indrct_addreg, 58);
	CC_W1 (mgbase, indrct_datareg, 0x0);


	CC_W1 (mgbase, fbctl, 0x40);
	CC_W1 (mgbase, fbsel, 0);

	initCC1(mgbase, info);

#if defined(IP22)
	if (!(is_fullhouse ())) {
		CC_W1 (mgbase, indrct_addreg, 37);  /* reset AB1 (Indy) */
		CC_W1 (mgbase, indrct_datareg, 0x00);
		CC_W1 (mgbase, indrct_addreg, 37);
		CC_W1 (mgbase, indrct_datareg, 0x01);
	}
#endif
	for (i = 1;i < 5;i++) {
		CC_W1 (mgbase, indrct_addreg,i);
		CC_W1 (mgbase, indrct_datareg,i + 20);
		msg_printf(DBG,"CC READ should be %d %d \n",
				i, CC_R1(mgbase,indrct_addreg));
	}

	msg_printf(VRB, "Now trying AB test reg R/W \n");
	for (i = 1;i < 5;i++) {
		AB_W1 (mgbase, low_addr, i);
		CC_W1 (mgbase, indrct_addreg,i + 20);
		msg_printf(DBG,"AB READ should be %d %d \n", i, AB_R1(mgbase,low_addr));
        }

	AB_W1 (mgbase, high_addr, 0);
	AB_W1 (mgbase, low_addr, 0);
	AB_W1 (mgbase, tst_reg, 4);


	AB_W1 (mgbase, sysctl, 1);

	initAB1(mgbase, info);

	if (err_count)
		sum_error ("ev1 initialization");
	else
		/*okydoky();*/
       		msg_printf(VRB, "Video Board Initialized\n");
	return(err_count);

}

ev1_initialize_ni2c(int argc, char *argv[])
{
	struct ev1_info *info, ev1info;
	int videoinfo;
	int timeout, tmp;
	int err_count = 0;
	int i;
	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));
	if (mgv1_Galileo_hw_probe(mgbase, info) != 0) {
		err_count++;
		msg_printf(SUM, "Galileo Video Not Found \n");
		return(-1);
	}


#ifdef IP22
	msg_printf(VRB, "%s version - Galileo Video ...\n", is_fullhouse () ? 
		"Indigo-2" : "Indy");

	if (info->BoardRev == GALILEO_JR) 
		msg_printf(VRB, "GALILEO Junior Board Found\n");
#else
	msg_printf(VRB, "Indigo version  - Galileo Video ...\n");
#endif

	CC_W1 (mgbase, sysctl, 8);

/*for(i=1;i<5;i++)
	{CC_W1 (mgbase, indrct_addreg,i);
         msg_printf(DBG, "CC READ should be %d %d \n",i,CC_R1(mgbase,indrct_addreg));
}  */
#if defined(IP20)
	/*if (initI2C(mgbase, info)) {
		err_count++;
		return -1;
	}*/
#else 

#if defined(IP22)
	if (is_fullhouse ()) {
		/*if (initI2C(mgbase, info)) {
			err_count++;
			return -1;
		}*/
	}
	else {
		/* This is INDY VIDEO */
		/*if (initI2C_for_indy(mgbase, info)) {
			err_count++;
			return -1;
		}*/
	}

#endif

#endif
	/* checking whether I2C bus is responding by reading back the status */
/*	videoinfo = I2C_status(mgbase, DMSD_ADDR);
	if (videoinfo == -1)  {
		msg_printf(ERR,"ev1_init: I2C Check Status failed\n");
		return -1;
	}
		*/
/*        if (videoinfo & 0x20)
                msg_printf(VRB, "NTSC 60Hz\n");
        else
                msg_printf(VRB, "PAL 50Hz\n");
*/
	CC_W1 (mgbase, indrct_addreg, 38);
	CC_W1 (mgbase, indrct_datareg, 0x20);

	while ((tmp = (CC_R1 (mgbase, indrct_datareg))) & 0x40) {
		DELAY(1);
		++timeout;
		msg_printf(DBG, "CCIND38= 0x%x\n", tmp);
		if (timeout > 10000){
			msg_printf(ERR, "CCIND38 bit 6 is set \n");
			return -1;
		}
	}
	CC_W1 (mgbase, indrct_addreg, 54);
	CC_W1 (mgbase, indrct_datareg, 0x00);
	CC_W1 (mgbase, indrct_addreg, 55);
	CC_W1 (mgbase, indrct_datareg, 0x00);
	CC_W1 (mgbase, indrct_addreg, 56);
	CC_W1 (mgbase, indrct_datareg, 0x30);
	CC_W1 (mgbase, indrct_addreg, 57);
	CC_W1 (mgbase, indrct_datareg, 0x0);
	CC_W1 (mgbase, indrct_addreg, 58);
	CC_W1 (mgbase, indrct_datareg, 0x0);


	CC_W1 (mgbase, fbctl, 0x40);
	CC_W1 (mgbase, fbsel, 0);

	initCC1(mgbase, info);

#if defined(IP22)
	if (!(is_fullhouse ())) {
		CC_W1 (mgbase, indrct_addreg, 37);          /* reset AB1 (Indy) */
		CC_W1 (mgbase, indrct_datareg, 0x00);
		CC_W1 (mgbase, indrct_addreg, 37);
		CC_W1 (mgbase, indrct_datareg, 0x01);
	}
#endif
	for (i = 1;i < 5; i++) {
		CC_W1 (mgbase, indrct_addreg,i);
		CC_W1 (mgbase, indrct_datareg,i + 20);

		msg_printf(DBG, "CC READ should be %d %d \n",
				i, CC_R1(mgbase,indrct_addreg));
	}

	msg_printf(VRB, "Now trying AB test reg R/W \n");
	for (i = 1;i < 5; i++) {
		AB_W1 (mgbase, tst_reg, i);
		CC_W1 (mgbase, indrct_addreg,i+20);
		msg_printf(DBG, "AB READ should be %d %d \n", i, AB_R1(mgbase,tst_reg));
        }

	AB_W1 (mgbase, high_addr, 0);
	AB_W1 (mgbase, low_addr, 0);
	AB_W1 (mgbase, tst_reg, 4);


	AB_W1 (mgbase, sysctl, 1);

	initAB1(mgbase, info);

	if (err_count)
		sum_error ("ev1 initialization");
	else
		okydoky();
	return(err_count);

}

#define  VLUT 	CSC_ADDR + 1

__int32_t
mgv1_i2c_initalize(int device, mgras_hw *mgbase, struct ev1_info *info)
{
	if (mgv1_Galileo_hw_probe(mgbase, info) != 0) {
		msg_printf(ERR, "Galileo Video  Not Found\n");
		return(-1);
	}

	switch (device) {
	    case D1_ADDR: /* D1 - 7151 */
#if defined(IP22)
		if (!(is_fullhouse ())) {
			msg_printf(VRB, "7151 does not exist on INDY GALILEO\n");
			return 0;
		}
#endif
		/* GALILEO_JR does not have 7151 + 7192 + 7169 */
		if (info->BoardRev == GALILEO_JR) {
			msg_printf(VRB, "7151 does not exist on GALILEO JUNIOR\n");
			return 0;
		}

		msg_printf(VRB,"Initialize 7151 Registers  \n");
		if (I2C_dev_init(mgbase, D1_ADDR,  D1_default)) {
			msg_printf(ERR,"7151  init. failed\n");
			return -1;
		}

		break;
	    case BUS_CONV_ADDR: /* 8574 */
#if defined(IP22)
		if (!(is_fullhouse ())) {
			msg_printf(VRB, "8574 does not exist on INDY GALILEO\n");
			return 0;
		}
#endif
		msg_printf(VRB,"Initialize 8754 Registers  \n");
		if (info->BoardRev != GALILEO_JR) {
			if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  BUS_default0)) {
				msg_printf(ERR,"8754  init. failed\n");
				return -1;
			}

			if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  BUS_default1)) {
				msg_printf(ERR,"8754  init. failed\n");
				return -1;
			}
		} else {
			if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  Junior_BUS_default0)) {
				msg_printf(ERR,"8754  init. failed\n");
				return -1;
			}

			if (I2C_dev_init(mgbase, BUS_CONV_ADDR,  Junior_BUS_default1)) {
				msg_printf(ERR,"8754  init. failed\n");
				return -1;
			}
		}
		break;
	    case DAC_ADDR: /* 7169 */
#if defined(IP22)
		if (!(is_fullhouse ())) {
			msg_printf(VRB, "7169 does not exist on INDY GALILEO\n");
			return 0;
		}
#endif

		/* GALILEO_JR does not have 7151 + 7192 + 7169 */
		/*if (info->BoardRev == GALILEO_JR) {
			msg_printf(VRB, "7169 does not exist on GALILEO JUNIOR\n");
			return 0;
		}*/

		msg_printf(VRB, "Initialize (7169) DAC Registers..\n");
		if (I2C_dev_init(mgbase, DAC_ADDR,  DAC_default)) {
			msg_printf(ERR,"DAC init. failed\n");
			return -1;
		}
		break;
	    case CSC_ADDR: /* 7192 */
#if defined(IP22)
		if (!(is_fullhouse ())) {
			msg_printf(VRB, "7192 does not exist on INDY GALILEO\n");
			return 0;
		}
#endif
		/* GALILEO_JR does not have 7151 + 7192 + 7169 */
		if (info->BoardRev == GALILEO_JR) {
			msg_printf(VRB, "7192 does not exist on GALILEO JUNIOR\n");
			return 0;
		}
		msg_printf(VRB, "Initialize (7192) Color Space Converter Registers..\n");
		if (I2C_dev_init(mgbase, CSC_ADDR,  CSC_default)) {
			msg_printf(ERR,"CSC init. failed\n");
			return -1;
		}
		break;
	    case VLUT: /* 7192 */
#if defined(IP22)
		if (!(is_fullhouse ())) {
			msg_printf(VRB, "7192 does not exist on INDY GALILEO\n");
			return 0;
		}
#endif
		/* GALILEO_JR does not have 7151 + 7192 + 7169 */
		if (info->BoardRev == GALILEO_JR) {
			msg_printf(VRB, "7192 does not exist on GALILEO JUNIOR\n");
			return 0;
		}

		msg_printf(VRB, "Initialize (7192) VLUTs.. CSC's\n");
		if (I2C_VLUT(mgbase, CSC_ADDR)) {
			msg_printf(ERR, "VLUTs - CSC's init. failed\n");
			return -1;
		}

		break;
	    case DENC_ADDR: /* Initialize  Encoder Registers */
		msg_printf(VRB, "Initialize  (7199) Encoder Registers..\n");
		if (I2C_dev_init(mgbase, DENC_ADDR,  DENC_default)) {
			msg_printf(ERR,"DENC init. failed\n");
			return -1;
		}
		break;

	    case INDYCAM_ADDR: /* Initialize  Indycam */
		msg_printf(VRB, "Initialize  Indycam..\n");
		if (I2C_dev_init(mgbase, INDYCAM_ADDR,  INDYCAM_default)) {
			msg_printf(ERR,"INDYCAM init. failed\n");
			return -1;
		}
		break;

	    case DMSD_ADDR: /* 7199 - Initialize  Decoder Registers */
		msg_printf(VRB, "Initialize  (7191) Decoder Registers..\n");
		if (I2C_dev_init(mgbase, DMSD_ADDR,  DMSD_default)) {
			msg_printf(ERR,"DMSD init. failed\n");
			return -1;
		}
		break;
		
	}
}

/****************************************************************************
*
*  i2c  - New functionality was added to this function including
*       initializiation of indycam, svideo, setting up (slave)
*       and tearing down (unslave) modes, initializing 7191, and 8444 DAC.
*
*  jfg 5/95
******************************************************************************/


void
mgv1_i2c(int argc, char *argv[])
{
	char optstr[10];
	int device_addrs , data, errs;
        struct ev1_info *info, ev1info;
        unsigned int newreg, newval;  /*for resetting DAC*/
        info = &ev1info;
        bzero((void *)info, sizeof(struct ev1_info));
	
	sprintf(optstr, "%s", argv[1]);
	if ((strcasecmp(optstr,  "VLUT")) == 0) {
		device_addrs = CSC_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "ENC")) == 0) {
		device_addrs = DENC_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "CSC")) == 0) {
		device_addrs = CSC_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "DENC")) == 0) {
         		device_addrs = DENC_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "SVIDEO")) == 0) {
		device_addrs = DMSD_ADDR;
                if (argc > 3) {
			newreg = atoi(argv[2]);
			atobu(argv[3],&newval);
			DMSD_default[newreg].val = newval;
			msg_printf(SUM, "Changed the DMSD register %d to 0x%x \n",
					newreg, newval);
		} else {
			msg_printf(VRB, "Using DMSD-SVIDEO defaults \n");
		}

		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	}
	else if ((strcasecmp(optstr,  "DMSD")) == 0) {
                if (argc > 3) {
			newreg = atoi(argv[2]);
			atobu(argv[3],&newval);
			DMSD_default[newreg].val = newval;
			msg_printf(SUM, "Changed the DMSD register %d to 0x%x \n",
						newreg, newval);
		} else { 
			DMSD_default[6].val = 0x02;
			DMSD_default[12].val = 0x0;/*to svideo (below), change*/
			DMSD_default[14].val = 0x78 ;
			DMSD_default[15].val = 0x98;  /*back to composite*/
			DMSD_default[17].val = 0x40;
			DMSD_default[24].val = 0xe6;}
			device_addrs = DMSD_ADDR;
			errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
		}

	else if ((strcasecmp(optstr,  "DAC")) == 0) {
                if (argc > 3) {
			newreg = atoi(argv[2]);
			atobu(argv[3],&newval);
			DAC_default[newreg].val = newval;
			msg_printf(SUM, "Changed the DAC register %d to 0x%x \n",
						newreg, newval);
		} else 
			msg_printf(VRB, "Using DAC defaults \n");
                device_addrs = DAC_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "INDYCAM")) == 0) {
		device_addrs = INDYCAM_ADDR;
                if (argc > 3) {
			newreg = atoi(argv[2]);
			atobu(argv[3],&newval);
			INDYCAM_default[newreg].val = newval;
			msg_printf(SUM, "Changed the INDYCAM register %d to 0x%x \n",
						newreg, newval);
		} else 
			msg_printf(VRB,"Using DAC defaults \n");

		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "IO")) == 0) {
		device_addrs = BUS_CONV_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else if ((strcasecmp(optstr,  "d1")) == 0) {
		device_addrs = D1_ADDR;
		errs = mgv1_i2c_initalize(device_addrs, mgbase, info);
	} else {
		msg_printf(SUM, "i2c <I2C_DEVICES>\n");
		msg_printf(SUM, "I2C_DEVICES ::\n");
		msg_printf(SUM, "\tenc \t Chooses 7199 - ENCODER \n");
		msg_printf(SUM, "\tcsc \t Chooses 7192 - CSC \n");
		msg_printf(SUM, "\tvlut \t Chooses7192 - CSC - LUT \n");
		msg_printf(SUM, "\tdac [reg val] \t Chooses 7169 (8444) - DAC \n");
		msg_printf(SUM, "\tio \t Chooses 8574  - IO \n");
		msg_printf(SUM, "\td1 \t Chooses 7151 - D1 \n");
		msg_printf(SUM, "\tindycam [reg val] \t Chooses IndyCam \n");
                /*msg_printf(SUM, "\tdenc \t Chooses 7191 - DECODER \n");*/
		msg_printf(SUM, "\tdmsd [reg val] \t Chooses 7191 - DMSD \n");
                msg_printf(SUM, "\t   2=ycgain       3=cgain \n");
                msg_printf(SUM, "\t   4=bright fine  5=bright coarse \n");
                msg_printf(SUM, "\t   6=contrast fine 7=contrast coarse \n");

		return;
	}
}


/****************************************************************************
*
*  i2c_print  - Allows the current settings of several i2c devices to be
*       printed.
*
*  jfg 5/95
******************************************************************************/


__int32_t
i2c_print(int argc, char *argv[])
{
	int i;
	char optstr[10];

	sprintf(optstr, "%s", argv[1]);
        if ((strcasecmp(optstr,  "DAC")) == 0) {
		msg_printf(SUM, "for DAC \n");
		for (i = 0;i <= 7;i++) {
			msg_printf(SUM, "reg %d value 0x%x \n", 
					DAC_default[i].reg, DAC_default[i].val);
		}
	} else if ((strcasecmp(optstr,  "INDYCAM")) == 0) {
		msg_printf(SUM, "for INDYCAM \n");
		for (i = 0;i <= 7;i++) {
			msg_printf(SUM, "reg %d value 0x%x \n", 
				INDYCAM_default[i].reg, INDYCAM_default[i].val);
		}
	} else if ((strcasecmp(optstr,  "DMSD")) == 0) {
		msg_printf(SUM, "for DMSD \n");
		for (i = 0;i <= 9; i++) {
			msg_printf(SUM, "reg %d value 0x%x reg %d value 0x%x \n ", 
				DMSD_default[2*i].reg, DMSD_default[2*i].val, 
				DMSD_default[1+(2*i)].reg, 
				DMSD_default[1+(2*i)].val);
		}
	} else {
		msg_printf(SUM, "\tdac \t Prints 7169 (8444) - DAC \n");
		msg_printf(SUM, "\t \t including recently changed registers \n");
		msg_printf(SUM, "\tindycam \t Prints IndyCam \n");
		msg_printf(SUM, "\t \t including recently changed registers \n");
		msg_printf(SUM, "\tdmsd \t Prints DMSD \n");
		msg_printf(SUM, "\t   2=ycgain       3=cgain \n");
		msg_printf(SUM, "\t   4=bright fine  5=bright coarse \n");
		msg_printf(SUM, "\t   6=contrast fine 7=contrast coarse \n");
		return(0);
      }
}




void
sv2ga()
{
   struct ev1_info *info, ev1info;
   info = &ev1info;

	bzero((void *)info, sizeof(struct ev1_info));

	v2ga();

	DMSD_default[6].val = 0x82 ;
	DMSD_default[12].val = 0x20 ;
	DMSD_default[14].val = 0x79 ;
	DMSD_default[15].val = 0x9e;
	DMSD_default[17].val = 0x0;
	DMSD_default[24].val = 0xe7;

	mgv1_i2c_initalize(DMSD_ADDR,mgbase,info);
}

/****************************************************************************
*
*  composite  - Reprogramming the decoder so that it is set for composite
*       and not svideo.  This function merely changes the registers changed
*       when going into svideo mode.
*
*  jfg 5/95
******************************************************************************/
void
composite()
{
   struct ev1_info *info, ev1info;


   	info = &ev1info;
   	bzero((void *)info, sizeof(struct ev1_info));
	DMSD_default[6].val = 0x02 ;
	DMSD_default[12].val = 0x0 ;
	DMSD_default[14].val = 0x78 ;
	DMSD_default[15].val = 0x98;
	DMSD_default[17].val = 0x40;
	DMSD_default[24].val = 0xe6;

	mgv1_i2c_initalize(DMSD_ADDR,mgbase,info);
}

void
slave()
{
	struct ev1_info *info, ev1info;
	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));

	DENC_default[9].val = 0x01 ;
	DENC_default[4].val = 0x20 ;
	DENC_default[0].val = 0xAE ;
	DENC_default[13].val = 0x01;
	mgv1_i2c_initalize(DENC_ADDR,mgbase,info);
	Junior_BUS_default1[0].val = 0x1E;
	mgv1_i2c_initalize(BUS_CONV_ADDR,mgbase,info);
	CC_W1 (mgbase, sysctl, 0);
	CC_W1 (mgbase, indrct_addreg, 33);    
	CC_W1 (mgbase, indrct_datareg,0x0);	
	CC_W1 (mgbase, indrct_addreg, 34);    
	CC_W1 (mgbase, indrct_datareg,0x1e);	
	CC_W1 (mgbase, indrct_addreg, 35);   
	CC_W1 (mgbase, indrct_datareg,0x4);	
}

void
unslave()
{
	struct ev1_info *info, ev1info;
	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));

	DENC_default[9].val = 0x0 ;
	DENC_default[4].val = 0x60 ;
	DENC_default[0].val = 0xAD ;
	DENC_default[13].val = 0x00;
	mgv1_i2c_initalize(DENC_ADDR,mgbase,info);
	Junior_BUS_default1[0].val = 0xE;
	mgv1_i2c_initalize(BUS_CONV_ADDR,mgbase,info);
	CC_W1 (mgbase, sysctl, 8);
	CC_W1 (mgbase, indrct_addreg, 33);    
	CC_W1 (mgbase, indrct_datareg,0x61);	
	CC_W1 (mgbase, indrct_addreg, 34);    
	CC_W1 (mgbase, indrct_datareg,0x34);	
	CC_W1 (mgbase, indrct_addreg, 35);   
	CC_W1 (mgbase, indrct_datareg,0xC);	
}



/******************************************************************************
*
*  mgras_init_dcb() -- Routine to setup registers in HQ3 for DCB access
*                            for ab1s and cc1s
*                      Also it establishes where the addresses are for
*                      the ab1 and cc1.
*  inputs:  base address of mgras hardware
*  returns: void
*  side effects:
*  notes:  this is a slightly modified version of grant's mgv_init_dcbctrl
*
******************************************************************************/
extern int addprnt;
void 
mgras_init_dcbctrl()  /*(mgras_hw  *mgrasbase)*/
{
	mgbase->dcbctrl_cc1 = HQ3_CC1_PROTOCOL;
	if (addprnt)
		msg_printf(SUM, "mgras_dcb_init dcbctrl_cc1 0x%x val 0x%x\n", &(mgbase->dcbctrl_cc1), HQ3_CC1_PROTOCOL);
	if(DCB_VAB1_PXHACK  ==   (0xA << DCB_ADDR_SHIFT)) {
		mgbase->dcbctrl_ab1 = HQ3_AB1_PROTOCOL;
		if (addprnt)
			msg_printf(SUM, "mgras_dcb_init dcbctrl_ab1 0x%x val 0x%x\n", &(mgbase->dcbctrl_ab1), HQ3_AB1_PROTOCOL);
	} else if(DCB_VAB1_PXHACK  ==   (0xB << DCB_ADDR_SHIFT)) {
		mgbase->dcbctrl_11 = HQ3_AB1_PROTOCOL;
		if (addprnt)
			msg_printf(SUM, "mgras_dcb_init dcbctrl_ab1 0x%x val 0x%x\n", &(mgbase->dcbctrl_11), HQ3_AB1_PROTOCOL);
	} else
		msg_printf(SUM, "ERROR AB1 does not have a DCB address of 10 or 11 \n");
}



/******************************************************************************
*
*  mcontrast, mbright  - These routines toggle the manual contrast/brightness
*                        capabilities of the board.
*
* jfg 5/95
******************************************************************************/

void mcontrast()
{
	mgv1_cc_reg_default[37].val ^=  0x20;
       	CC_W1 (mgbase, indrct_addreg, 37);
	CC_W1 (mgbase, indrct_datareg, mgv1_cc_reg_default[37].val);	
	CC_W1 (mgbase, sysctl, sysctlval);
	msg_printf(SUM, "mgv1_cc_reg_default[37].reg %d mgv1_cc_reg_default[37].val 0x%x && 0x20 0x%x \n",mgv1_cc_reg_default[37].reg, mgv1_cc_reg_default[37].val, mgv1_cc_reg_default[37].val & 0x20);
	if((mgv1_cc_reg_default[37].val & 0x20) == 0x20){
		msg_printf(SUM, "Manual Contrast is now on \n");
		msg_printf(SUM, "Use  >i2c dac 6 | dac 7    to set fine|course contrast \n");
		msg_printf(SUM, "Use  >i2c_print dac 6 | dac 7    to print fine|course contrast \n");}
	else
		msg_printf(SUM, "Manual Contrast is now off \n");

}


void mbright()
{

   struct ev1_info *info, ev1info;

	info = &ev1info;

	bzero((void *)info, sizeof(struct ev1_info));

	DMSD_default[13].val ^= 0x02;
	mgv1_i2c_initalize(DMSD_ADDR, mgbase, info);

	msg_printf(SUM, "DMSD_default 13 val 0x%x anded 0x02 0x%x \n",DMSD_default[13].val, DMSD_default[13].val & 0x02);
	if((DMSD_default[13].val & 0x02) == 0x02){
		msg_printf(SUM, "Manual Brightness is now on \n");
		msg_printf(SUM, "Use   >i2c dac 4 | dac 5   to set fine|course brightness \n");
		msg_printf(SUM, "Use   >i2c_print dac 4 | dac 5   to print fine|course brightness \n");
	} else
		msg_printf(SUM, "Manual Brightness is now off \n");

}


void input(int argc, char *argv[])
{

   unsigned int newval;
   struct ev1_info *info, ev1info;

   	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));

	if(argc > 1){
		atobu(argv[1],&newval);
		if((newval>3) || (newval < 0)){
			msg_printf(SUM, "The value 0x%x is invalid, valid input is 0-3 \n",newval);
			return;
		}
		DMSD_default[14].val &= 0xfc;
		DMSD_default[14].val |= newval;
		mgv1_i2c_initalize(DMSD_ADDR, mgbase, info);
	}

	msg_printf(SUM, "Input parameter set to 0x%x \n",DMSD_default[14].val & 0x03);

   return;

}
