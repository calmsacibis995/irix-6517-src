#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "sys/ng1hw.h"
#include <fault.h>

#include "ev1_I2C.h"
#include "ide_msg.h"
#include "regio.h"

#include "uif.h"
#include "libsc.h"
#include "libsk.h"

struct reg_set {
        int reg;
        int val;
};


/* CC1 - INDIRECTLY ADDRESSED HOST REGS :: Initialize to Safe Default Values */
struct reg_set cc_reg_default[] = {

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
	
	32	,2,
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


extern Rex3chip *REX;
extern struct gr2_hw  *base;

struct ev1_info {
	unsigned char BoardRev;
	unsigned char CC1Rev;
	unsigned char AB1Rev;
	unsigned char CC1dRamSize;
};


#if defined(IP22)
extern int Ng1ProbeAll(char **);
#endif

int	GVType;

#define 	GALILEO_JR	3

int
Galileo_hw_probe(struct gr2_hw *base, struct ev1_info *info)
{
	int GFXversion, Videoversion;
#if defined(IP22)
	char *tmp[4];

	if (!is_fullhouse ()) {

		int t;
		jmp_buf faultbuf;

		if (setjmp (faultbuf)) {
			return (-1);
		}
		nofault = faultbuf;
		CC_W1 (base, indrct_addreg, 128);
		Videoversion = CC_R1 (base, indrct_datareg);
		nofault = 0;

		GVType = GVType_ng1;
		return (0);
	}
#endif

	
	if (base->hq.mystery == 0xdeadbeef)  {
		/* gfx board found */
		msg_printf(DBG, "base: 0x%x\n", base);
		 msg_printf(DBG, "base->bdvers.rd0: 0x%x\n", base->bdvers.rd0);
		GFXversion = ~(base->bdvers.rd0) & 0xf;
		GFXversion = GFXversion;
		msg_printf(DBG, "GFX Board, GFXversion= 0x%x \n" ,GFXversion);
		GVType = GVType_gr2;
		if (GFXversion < 4) 
			Videoversion = (base->bdvers.rd2 & 0x30) >> 4;
		else
			Videoversion = (base->bdvers.rd1 & 0xC) >> 2;

		Videoversion = ~Videoversion & 0x3 ;
	}
	else if (base->hq.mystery != 0xdeadbeef) {
#if defined(IP22)
		if (is_fullhouse()) {
  			msg_printf(DBG, "rex3chip addr: 0x%x\n", REX);
			if (!(Ng1ProbeAll(tmp))) {
				msg_printf(SUM, "Graphics Board not found.\n");
				GVType = 0;
				return(-1);
			}

			/* Check for Newport Gfx here */
			lg3BdVersGet(REX, GFXversion );
			Videoversion = ~GFXversion & 0x3 ;
			msg_printf(DBG, "GFX Board, GFXversion= 0x%x \n" ,GFXversion);
		}
#endif
	}


	info->BoardRev = Videoversion ; /* Start from rev. 1 */
	msg_printf(DBG, "GFXversion= 0x%x, Videoversion= 0x%x\n" ,GFXversion, info->BoardRev);

       	if (Videoversion == 0) {
		msg_printf(VRB,"video board not installed \n");
		return -1;
	}

	if (info->BoardRev == GALILEO_JR) 
		msg_printf(DBG, "GALILEO Junior Board Found\n");

	return 0;
}

static int
_Forever_Print_I2C_Ctl(struct gr2_hw *base)
{
	int status;

	status = CC_R1 (base, I2C_bus);
       	msg_printf(SUM,"Forever I2C_Bus Control status=0x%x\n", status);
	DELAY(1000);

        return(0);
}

static int
_Print_I2C_Ctl(struct gr2_hw *base)
{
	int status;

	status = CC_R1 (base, I2C_bus);
        msg_printf(DBG,"I2C_Bus Control status=0x%x\n", status);

        return(0);
}

int
_trigger(struct gr2_hw *base)
{
	CC_W1 (base, indrct_addreg, 7);
	CC_W1 (base, indrct_addreg, 37);
	CC_W1 (base, indrct_datareg, 0x14);

	return 0;
}

static int
_I2C_force_idle(struct gr2_hw *base)
{
	int status;

	_Print_I2C_Ctl(base);

	CC_W1 (base, I2C_bus, (I2C_RELEASE|I2C_FORCE_IDLE |I2C_128 | I2C_WRITE));
	DELAY(100);
	_Print_I2C_Ctl(base);

	status = CC_R1(base, I2C_bus);

	if (status & I2C_ERR){
		_trigger(base);
                msg_printf(ERR,"_I2C_force_idle: I2C_ERR is set, status=0x%x\n", status);
		return -1;
	}
	return (0);
}

static int
_I2C_wait_for_not_idle(struct gr2_hw *base)
{
	int status;
        int idle_wait = 0;

	while (!((status = (CC_R1(base, I2C_bus))) & I2C_NOT_IDLE)){
		if (idle_wait > MAX_IDLE_WAIT) 
			break;

		idle_wait++;
		DELAY(5);
	}	


	/* Is the I2C Bus STUCK ? */
	if (idle_wait > MAX_IDLE_WAIT) {
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_not_idle: SLAVE Device holding on to I2C Bus - TIMEOUT...status=0x%x\n" ,status);
                return(-1);
        }

	if (status & I2C_ERR){
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_not_idle: I2C_ERR is set, but I2C is not idle okay, status= 0x%x\n", status);
		return -1;
	}

        return(0);
}

#if 0	/* currently unused */
static int
_I2C_wait_for_idle(struct gr2_hw *base)
{
	int status;
        int idle_wait = 0;

	while ((status = (CC_R1 (base, I2C_bus))) & I2C_NOT_IDLE) {
		if (idle_wait > MAX_IDLE_WAIT)
			break;

		idle_wait++;
		DELAY(5);
	}	

	if (idle_wait > MAX_IDLE_WAIT) {
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_idle: cannot get idle I2C bus - TIMEOUT...status=0x%x\n", status);
                return(-1);
        }

	if (status & I2C_ERR){
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_idle: I2C_ERR is set, but I2C idled okay, status=0x%x\n", status);
		return -1;
	}

        return(0);
}
#endif

static int
_I2C_wait_for_ack(struct gr2_hw *base)
{
	int status;
	int ack_wait = 0;

	while ((status = (CC_R1 (base, I2C_bus))) & I2C_NOT_ACK) {
		if (ack_wait > MAX_ACK_WAIT)
			break;

		ack_wait++;
		DELAY(10);
	}

	if (ack_wait > MAX_ACK_WAIT) {
		_trigger(base);
		msg_printf(ERR,"_I2C_wait_for_ack: Acknowledge not received - TIMEOUT...status=0x%x\n", status);
		return -1;
	}
	if (status & I2C_ERR) {
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_ack: I2C_ERR is set, but I2C acked okay, status=0x%x\n", status);
		return -1;
	}
	return 0;
}

static int
_I2C_wait_for_xfer(struct gr2_hw *base)
{
	int status;
	int xfer_wait = 0;

	while ((status = (CC_R1 (base, I2C_bus))) & I2C_XFER_BUSY) {
		if (xfer_wait > MAX_XFER_WAIT) 
			break;

		xfer_wait++;
		DELAY(1);
	}

	if(xfer_wait > MAX_XFER_WAIT) {
		_trigger(base);
		msg_printf(ERR,"_I2C_wait_for_xfer: transfer still busy - TIMEOUT...status=0x%x\n", status);
		return -1;
	}

	if (status & I2C_ERR) {
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_xfer: I2C_ERR is set, but I2C xfered okay, status=0x%x\n", status);
		return -1;
	}

	return 0;
}

#define MAX_LONGXFER_WAIT 250000
static int
_I2C_wait_for_longxfer(struct gr2_hw *base)
{
	int status;
	int xfer_wait = 0;

	DELAY(MAX_LONGXFER_WAIT/5);
	while ((status = (CC_R1 (base, I2C_bus))) & I2C_XFER_BUSY) {

		if (xfer_wait > MAX_LONGXFER_WAIT)
			break;

		xfer_wait++;
		DELAY(5);
	}
	if(xfer_wait > MAX_LONGXFER_WAIT) {
		_trigger(base);
		msg_printf(ERR,"_I2C_wait_for_longxfer: transfer still busy - TIMEOUT...status=0x%x\n", status);
		return -1;
	}

	if (status & I2C_ERR) {
		_trigger(base);
                msg_printf(ERR,"_I2C_wait_for_longxfer: I2C_ERR is set, but I2C long xfered okay..status=0x%x\n", status);
		return -1;
	}

	return 0;
}

static int
I2C_status(struct gr2_hw *base, char addr ) 
{
	unsigned char val;
	int retry;
        int idle_wait = 0;

	_Print_I2C_Ctl(base);
        /* force idle if necessary */
	if (CC_R1 (base, I2C_bus) & I2C_NOT_IDLE) {
                if (_I2C_force_idle(base))
			return -1;
        }

        /* Setup for WRITE */
	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));

        /* write to slave_address, and specify the read opertion */
	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_data, (addr | 0x1)); /* 1 for read, 0 for write */

        if (_I2C_wait_for_xfer(base))
		return -1;
        if (_I2C_wait_for_ack(base))
		return -1;

        /* Toggle the read bit. */
	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_bus, (I2C_HOLD |I2C_128 | I2C_READ | I2C_NOT_IDLE));	
        if (_I2C_wait_for_xfer(base))
		return -1;

	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_bus, (I2C_HOLD |I2C_128 | I2C_WRITE | I2C_NOT_IDLE));	

        /* Status read back from Philips */
	_Print_I2C_Ctl(base);
	val = CC_R1 (base, I2C_data);

repeat_read:

	idle_wait = 0;
	++retry;
	while (!(CC_R1(base, I2C_bus) & I2C_NOT_IDLE)) {
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
        	if (_I2C_wait_for_xfer(base))
			return -1;
        	if (_I2C_wait_for_ack(base))
			return -1;

        	/* Toggle the read bit. */
		_Print_I2C_Ctl(base);
		CC_W1 (base, I2C_bus, (I2C_HOLD |I2C_128 | I2C_READ | I2C_NOT_IDLE));	
        	if (_I2C_wait_for_xfer(base))
			return -1;
	
		_Print_I2C_Ctl(base);
		CC_W1 (base, I2C_bus, (I2C_HOLD |I2C_128 | I2C_WRITE | I2C_NOT_IDLE));	

        	/* Status read back from Philips */
		_Print_I2C_Ctl(base);
		val = CC_R1 (base, I2C_data);

		if (retry < 10)  {
			msg_printf(SUM, "I2C_status, %d try pass\n" ,retry);
			goto repeat_read;
		}
		else {
			msg_printf(ERR, "I2C_status, failed after %d tries\n" ,retry);
			return -1;
		}
	}
	/*NOTREACHABLE*/
}




/* Encoder defaults */
struct reg_set DENC_default[] = {       /* defaults */
            0, 0xAE, 
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
struct reg_set DMSD_default[] = {
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
};


/* 7192 - Color Space Conversion */
struct reg_set CSC_default[] = {
	0,	0x3A,	
	-1, -1,
};

/* Digital to Analog conversion */
struct reg_set DAC_default[] = {
                0,      0,
                1,      0x19,
                2,      0x18,
                3,      0xe,
                4,      20,
                5,      20,
                6,      20,
                7,      20,
                -1, -1,
};



/*  Initialize 8574 */
struct reg_set BUS_default0[] = {
	0, 0x86,
	-1,-1,
};

struct reg_set BUS_default1[] = {
	0, 0x6,
	-1,-1,
};


/***	8574 Init for GALILEO JUNIOR ONLY */
struct reg_set Junior_BUS_default0[] = {
	0, 0x8E,
	-1,-1,
};

struct reg_set Junior_BUS_default1[] = {
	0, 0xE,
	-1,-1,
};


/***/
/*
 * Initialize 7151 - SQP
 */

struct reg_set D1_default[] = {
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
};


static int
I2C_dev_init(struct gr2_hw *base, char addr, struct reg_set *data)
{
	_Print_I2C_Ctl(base);
	/* force idle if necessary */
	if (CC_R1 (base, I2C_bus) & I2C_NOT_IDLE) {
		if ((_I2C_force_idle(base))  == -1)
			return -1;	 
	}

	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));

	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_data, (addr|0)); /* 0 => Write, 1=> read (not supported) */
	msg_printf(DBG,"I2C_DEV= 0x%x \n", (addr|0));

	if ((_I2C_wait_for_not_idle(base)) == -1)
		return -1;
	if ((_I2C_wait_for_xfer(base)) == -1)
		return -1;
	if ((_I2C_wait_for_ack(base)) == -1)
		return -1;

	if (addr == DENC_ADDR) {
		_Print_I2C_Ctl(base);
		CC_W1 (base, I2C_data, 2);
		msg_printf(DBG,"DENC_ADDR, data =  0x%x \n", 2);
		if ((_I2C_wait_for_not_idle(base)) == -1)
			return -1;
		if ((_I2C_wait_for_xfer(base)) == -1)
			return -1;
		if ((_I2C_wait_for_ack(base)) == -1)
			return -1;
	}


	if (addr != BUS_CONV_ADDR){
	    msg_printf(DBG,"data reg =0x%x\n", data->reg);
	    _Print_I2C_Ctl(base);
	    CC_W1 (base, I2C_data, data->reg);
	    if ((_I2C_wait_for_not_idle(base)) == -1)
		return -1;

	    if ((_I2C_wait_for_xfer(base)) == -1)
		return -1;

	    if ((_I2C_wait_for_ack(base)) == -1)
		return -1;
	}

	/* Last Byte on the Bus, stop autoInc */
	if (((data+1)->reg) == -1){
		msg_printf(DBG, "Single Transaction Only...\n");
	        _Print_I2C_Ctl(base);
		CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
		msg_printf(DBG, "Single Transaction Only...setup Okay\n");
	        _Print_I2C_Ctl(base);
	}
	msg_printf(DBG,"data val =0x%x\n", data->val);
	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_data, data->val);
	if ((_I2C_wait_for_not_idle(base)) == -1)
		return -1;
	if (addr == BUS_CONV_ADDR) {
		if ((_I2C_wait_for_longxfer(base)) == -1) {
			return -1;
		}
	}else {
		if ((_I2C_wait_for_xfer(base)) == -1) {
			return -1;
		}
	}
	if ((_I2C_wait_for_ack(base)) == -1)
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
			_Print_I2C_Ctl(base);
			CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
			msg_printf(DBG, "Last Byte on the Bus...setup completed\n");
	        	_Print_I2C_Ctl(base);
		}

		msg_printf(DBG,"data val =0x%x\n", data->val);
		_Print_I2C_Ctl(base);
	        CC_W1 (base, I2C_data, data->val);
		if ((_I2C_wait_for_not_idle(base)) == -1)
			return -1;
		if ((_I2C_wait_for_xfer(base)) == -1)
			return -1;
		if ((_I2C_wait_for_ack(base)) == -1)
			return -1;


		data++;

	}
	if ((data->reg != -1) && ((data-1)->reg + 1 != data->reg)){
		msg_printf(SUM,"I2C_dev_init: auto inc mode - noncontiguous regs\n");
	}

#if 0
	/* NOT NECESSARY ANYMORE... */
	/* stop auto inc mode */
	if ((_I2C_force_idle(base)) == -1)
		return -1;
#endif

	return 0;
}




static int
I2C_VLUT(struct gr2_hw *base, char addr)
{
	int x;

	/* There are 2 ways to init VLUTS.
	 * 1. Write to R,G,B seperately
	 *	To Write to R :: Write to ADDR = 1
	 *	To Write to B :: Write to ADDR = 2
	 *	To Write to G :: Write to ADDR = 3
	 * 2. Write to RGB at the same time.
	 *	To Write to RGB :: Write to ADDR = 4
	 */

	_Print_I2C_Ctl(base);
	/* force idle if necessary */
	if (CC_R1 (base, I2C_bus) & I2C_NOT_IDLE) {
		if (_I2C_force_idle(base))
			return -1;
	}


	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_HOLD|I2C_WRITE|I2C_128));

	_Print_I2C_Ctl(base);
	CC_W1 (base, I2C_data, addr|0);	/* Select addr reg, Write Mode */
	if (_I2C_wait_for_not_idle(base))
		return -1;
	if (_I2C_wait_for_xfer(base))
		return -1;
	if (_I2C_wait_for_ack(base))
		return -1;

	/* Write to RGB all the same time */
	_Print_I2C_Ctl(base);
        CC_W1 (base, I2C_data, 4); 
	if (_I2C_wait_for_not_idle(base))
		return -1;
	if (_I2C_wait_for_xfer(base))
		return -1;
	if (_I2C_wait_for_ack(base))
		return -1;

	/* Start from VLUT Addr 0 */
	_Print_I2C_Ctl(base);
        CC_W1 (base, I2C_data, 0);
	if (_I2C_wait_for_not_idle(base))
		return -1;
	if (_I2C_wait_for_xfer(base))
		return -1;
	if (_I2C_wait_for_ack(base))
		return -1;

	/* Initialize the color ramp */
	/* Just send the data, addr is auto-inc */
	for (x = 0; x < 256; x++){
		/* Last Byte on the Bus , stop autoInc*/
		if (x == 255){
			msg_printf(DBG, "Last Byte on the Bus...\n");
			_Print_I2C_Ctl(base);
			CC_W1 (base, I2C_bus, (I2C_NOT_IDLE|I2C_RELEASE|I2C_WRITE|I2C_128));
		}
		_Print_I2C_Ctl(base);
		CC_W1 (base, I2C_data, x);
		if (_I2C_wait_for_not_idle(base))
			return -1;
		if (_I2C_wait_for_xfer(base))
			return -1;
		if (_I2C_wait_for_ack(base))
			return -1;

	}
	
	_Print_I2C_Ctl(base);
#if 0
	/* NOT NECESSARY ANYMORE... */
	/* stop auto inc mode */
	if (_I2C_force_idle(base))	 
		return -1;

#endif
	return(0);
}

#ifdef IP22
static int
initI2C_for_indy(struct gr2_hw *base, struct ev1_info *info)
{
	register int i; 
	extern struct reg_set DENC_default[], DMSD_default[]; 

	msg_printf(SUM,"Initialize I2C devices\n");


	CC_W1 (base, sysctl, 8);

	/* Initialize  Encoder Registers */
	msg_printf(SUM, "\nInitialize  (7199) Encoder Registers..");
	if (I2C_dev_init(base, DENC_ADDR,  DENC_default)) {
		msg_printf(ERR,"(7199) DENC init. failed\n");
		_Forever_Print_I2C_Ctl(base);
		return -1;
	}

	CC_W1 (base, indrct_addreg, 33);
	CC_W1 (base, indrct_datareg, 0x61);
	CC_W1 (base, indrct_addreg, 34);
	CC_W1 (base, indrct_datareg, 0x34);
	CC_W1 (base, indrct_addreg, 35);
	CC_W1 (base, indrct_datareg, 12);

	/* Initialize  Decoder Registers */
	msg_printf(SUM, "\nInitialize  (7191) Square Pixel Decoder Registers..");
	if (I2C_dev_init(base, DMSD_ADDR,  DMSD_default)) {
		msg_printf(ERR,"(7191) Square Pixel DMSD init. failed\n");
		_Forever_Print_I2C_Ctl(base);
		return -1;
	}

	return(0);
}
#endif

static int
initI2C(struct gr2_hw *base, struct ev1_info *info)
{
	extern struct reg_set DENC_default[], DMSD_default[]; 
	extern struct reg_set CSC_default[], DAC_default[];

	msg_printf(SUM,"Initialize I2C devices\n");

	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
	if (info->BoardRev != GALILEO_JR) {
		/* Initialize 8574 Registers */
		msg_printf(SUM,"Initialize 8574 Registers  \n");
		if (I2C_dev_init(base, BUS_CONV_ADDR,  BUS_default0)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}
		
		
		if (I2C_dev_init(base, BUS_CONV_ADDR,  BUS_default1)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}
	} else 
	{
		/* Initialize 8574 Registers */
		msg_printf(SUM,"Initialize 8574 Registers  \n");
		if (I2C_dev_init(base, BUS_CONV_ADDR,  Junior_BUS_default0)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}
		
		
		if (I2C_dev_init(base, BUS_CONV_ADDR,  Junior_BUS_default1)) {
			msg_printf(ERR,"IO - 8574  init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}
	}


	CC_W1 (base, sysctl, 8);

	/* Initialize  Encoder Registers */
	msg_printf(SUM, "\nInitialize  (7199) Encoder Registers..");
	if (I2C_dev_init(base, DENC_ADDR,  DENC_default)) {
		msg_printf(ERR,"(7199) DENC init. failed\n");
		_Forever_Print_I2C_Ctl(base);
		return -1;
	}

	CC_W1 (base, indrct_addreg, 33);
	CC_W1 (base, indrct_datareg, 0x61);
	CC_W1 (base, indrct_addreg, 34);
	CC_W1 (base, indrct_datareg, 0x34);
	CC_W1 (base, indrct_addreg, 35);
	CC_W1 (base, indrct_datareg, 12);



	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
	if (info->BoardRev != GALILEO_JR) {
		/* Initialize Color Space Converter Registers */
		msg_printf(SUM, "\nInitialize (7192) Color Space Converter Registers..");
		if (I2C_dev_init(base, CSC_ADDR,  CSC_default)) {
			msg_printf(ERR,"(7192) CSC init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}
	
		/* Initialize 7151 Registers */
		msg_printf(SUM, "\nInitialize (7151) Registers..");
			if (I2C_dev_init(base, D1_ADDR,  D1_default)) {
				msg_printf(ERR,"7151 - init. failed\n");
				_Forever_Print_I2C_Ctl(base);
				return -1;
			}

	}


	/* Initialize  Decoder Registers */
	msg_printf(SUM, "\nInitialize  (7191) Square Pixel Decoder Registers..");
	if (I2C_dev_init(base, DMSD_ADDR,  DMSD_default)) {
		msg_printf(ERR,"(7191) Square Pixel DMSD init. failed\n");
		_Forever_Print_I2C_Ctl(base);
		return -1;
	}

	/* GALILEO_JR does not have 7151 + 7192 + 7169 */
	if (info->BoardRev != GALILEO_JR) {
		/* Initialize DAC Registers */
		msg_printf(SUM, "\nInitialize (7169) DAC Registers..");
		if (I2C_dev_init(base, DAC_ADDR,  DAC_default)) {
			msg_printf(ERR,"(7169)DAC init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}

		/* Initialize VLUTs */
		msg_printf(SUM, "\nInitialize (7192) VLUTs.. CSC's");
		if (I2C_VLUT(base, CSC_ADDR)) {
			msg_printf(ERR, "(7192) VLUTs - CSC's init. failed\n");
			_Forever_Print_I2C_Ctl(base);
			return -1;
		}

	}
	return(0);
}

static void
initCC1(struct gr2_hw *base, struct ev1_info *info)
{
	register int i;
	extern struct reg_set cc_reg_default[];
#if 0
	int tmp;
#endif

	msg_printf(SUM,"Initialize CC1\n");

	/* Initialize CC1 SYSTEM CONTROL REG */
	CC_W1 (base, sysctl, 8);

	/* Initialize INDIRECTLY ADDRESSED HOST REGS */
	i = 0;
	while (cc_reg_default[i].reg != -1) {
		CC_W1 (base, indrct_addreg, cc_reg_default[i].reg);
		CC_W1 (base, indrct_datareg, cc_reg_default[i++].val);	
	}

	CC_W1 (base, fbsel, 0x0);

#if 0
	for (i=0; i<3; i++) {
		CC_W1 (base, sysctl, 0x48); /* Double Buffer */
		while ((tmp = CC_R1(base, sysctl)) & 0x40) {
			DELAY (1);
		}
	}
#endif

	CC_W1 (base, indrct_addreg, 128);
	info->CC1Rev = CC_R1 (base, indrct_datareg);
}

static void
initAB1(struct gr2_hw *base, struct ev1_info *info)
{
	register int i;
	extern struct reg_set ab_int_default[];

	msg_printf(SUM,"Initialize AB1's\n");

	AB_W1 (base, rgb_sel, 0x10); 	/* First Chip ID 0x10 */
	AB_W1 (base, rgb_sel, 0x21); 	/* second Chip ID 0x21 */
	AB_W1 (base, rgb_sel, 0x42); 	/* Third Chip ID 0x42 */
	AB_W1 (base, sysctl, 0x9); 	

	AB_W1 (base, high_addr, 0);
	AB_W1 (base, low_addr, 0);
	AB_W1 (base, tst_reg, 0x4); 

	i = 0;
	while (ab_int_default[i].reg != -1) {
		AB_W1 (base, high_addr, 0);
		AB_W1 (base, low_addr, ab_int_default[i].reg & 0xff);
		AB_W1 (base, intrnl_reg, ab_int_default[i++].val);
	}

#if 0
	for (i=0; i<3; i++) {
		CC_W1 (base, sysctl, 0x48); /* Double Buffer */
		while (CC_R1(base, sysctl) & 0x40) {
			DELAY (1);
		}
	}
#endif

	/* Get AB1 rev. */
	AB_W1 (base, high_addr, 0);
	AB_W1 (base, low_addr, 3);
	info->AB1Rev = AB_R1 (base, tst_reg) & 0xff;
}


/*ARGSUSED*/
int
ev1_initialize(int argc, char *argv[])
{
	struct ev1_info *info, ev1info;
	int videoinfo;
	int timeout;
	int err_count = 0;
	int tmp;

	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));
	
	if (Galileo_hw_probe(base, info) != 0) {
		err_count++;
		printf("Galileo Video  Not Found\n");
		return(-1);
	}



#if IP22 || IP26 || IP28
#if IP22
	msg_printf(DBG, "%s version - Galileo Video ...\n", is_fullhouse () ? 
		"Indigo-2" : "Indy");
#else
	msg_printf(DBG, "Indigo-2 version - Galileo Video ...\n");
#endif

	if (info->BoardRev == GALILEO_JR) 
		msg_printf(SUM, "GALILEO Junior Board Found\n");
#else
	msg_printf(DBG, "Indigo version  - Galileo Video ...\n");
#endif

	CC_W1 (base, sysctl, 8);

#if defined(IP20)
	if (initI2C(base, info)) {
		err_count++;
		return -1;
	}
#elif defined(IP22)
	if (is_fullhouse ()) {
		if (initI2C(base, info)) {
			err_count++;
			return -1;
		}
	} 
	else {
		/* This is INDY VIDEO */
		if (initI2C_for_indy(base, info)) {
			err_count++;
			return -1;
		}
	}
#elif defined(IP26) || defined(IP28)
	if (initI2C(base, info)) {
		err_count++;
		return -1;
	}
#endif

	/* checking whether I2C bus is responding by reading back the status */
	videoinfo = I2C_status(base, DMSD_ADDR);
	if (videoinfo == -1)  {
		msg_printf(ERR,"ev1_init: I2C Check Status failed\n");
		return -1;
	}
		
        if (videoinfo & 0x20)
                printf("NTSC 60Hz\n");
        else
                printf("PAL 50Hz\n");


	CC_W1 (base, indrct_addreg, 38);
	CC_W1 (base, indrct_datareg, 0x20);

	while ((tmp = (CC_R1 (base, indrct_datareg))) & 0x40) {
		DELAY(1);
		++timeout;
		msg_printf(SUM, "CCIND38= 0x%x\n", tmp);
		if (timeout > 10000){
			msg_printf(ERR, "CCIND38 bit 6 is set \n");
			return -1;
		}
	}

	CC_W1 (base, indrct_addreg, 54);
	CC_W1 (base, indrct_datareg, 0x00);
	CC_W1 (base, indrct_addreg, 55);
	CC_W1 (base, indrct_datareg, 0x00);
	CC_W1 (base, indrct_addreg, 56);
	CC_W1 (base, indrct_datareg, 0x30);
	CC_W1 (base, indrct_addreg, 57);
	CC_W1 (base, indrct_datareg, 0x0);
	CC_W1 (base, indrct_addreg, 58);
	CC_W1 (base, indrct_datareg, 0x0);


	CC_W1 (base, fbctl, 0x40);
	CC_W1 (base, fbsel, 0);

	initCC1(base, info);

#if defined(IP22)
	if (!(is_fullhouse ())) {
		CC_W1 (base, indrct_addreg, 37);          /* reset AB1 (Indy) */
		CC_W1 (base, indrct_datareg, 0x00);
		CC_W1 (base, indrct_addreg, 37);
		CC_W1 (base, indrct_datareg, 0x01);
	}
#endif

	AB_W1 (base, high_addr, 0);
	AB_W1 (base, low_addr, 0);
	AB_W1 (base, tst_reg, 4);

	AB_W1 (base, sysctl, 1);

	initAB1(base, info);

	if (err_count)
		sum_error ("ev1 initialization");
	else
		okydoky();
	return(err_count);
}

#define  VLUT 	CSC_ADDR + 1

int
i2c_initalize(int device, struct gr2_hw *base, struct ev1_info *info)
{
	if (Galileo_hw_probe(base, info) != 0) {
		printf("Galileo Video  Not Found\n");
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
			if (I2C_dev_init(base, D1_ADDR,  D1_default)) {
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
				if (I2C_dev_init(base, BUS_CONV_ADDR,  BUS_default0)) {
					msg_printf(ERR,"8754  init. failed\n");
					return -1;
				}

				if (I2C_dev_init(base, BUS_CONV_ADDR,  BUS_default1)) {
					msg_printf(ERR,"8754  init. failed\n");
					return -1;
				}
			}
			else {
				if (I2C_dev_init(base, BUS_CONV_ADDR,  Junior_BUS_default0)) {
					msg_printf(ERR,"8754  init. failed\n");
					return -1;
				}

				if (I2C_dev_init(base, BUS_CONV_ADDR,  Junior_BUS_default1)) {
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
			if (info->BoardRev == GALILEO_JR) {
				msg_printf(VRB, "7169 does not exist on GALILEO JUNIOR\n");
				return 0;
			}

			msg_printf(VRB, "Initialize (7169) DAC Registers..\n");
			if (I2C_dev_init(base, DAC_ADDR,  DAC_default)) {
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
			if (I2C_dev_init(base, CSC_ADDR,  CSC_default)) {
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
			if (I2C_VLUT(base, CSC_ADDR)) {
				msg_printf(ERR, "VLUTs - CSC's init. failed\n");
				return -1;
			}

			break;
		case DENC_ADDR: /* Initialize  Encoder Registers */
			msg_printf(VRB, "Initialize  (7199) Encoder Registers..\n");
			if (I2C_dev_init(base, DENC_ADDR,  DENC_default)) {
				msg_printf(ERR,"DENC init. failed\n");
				return -1;
			}
			break;

		case DMSD_ADDR: /* 7199 - Initialize  Decoder Registers */
			msg_printf(VRB, "Initialize  (7191) Decoder Registers..\n");
			if (I2C_dev_init(base, DMSD_ADDR,  DMSD_default)) {
				msg_printf(ERR,"DMSD init. failed\n");
				return -1;
			}
			break;
		
	}

	return 0;
}


/*ARGSUSED*/
int
i2c(int argc, char *argv[])
{
	char optstr[10];
	int device_addrs;
	volatile int errs;
        struct ev1_info *info, ev1info;

        info = &ev1info;
        bzero((void *)info, sizeof(struct ev1_info));

	
	sprintf(optstr, "%s", argv[1]);
	if ((strcasecmp(optstr,  "VLUT")) == 0) {
		device_addrs = CSC_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}

	else if ((strcasecmp(optstr,  "ENC")) == 0) {
		device_addrs = DENC_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}

	else if ((strcasecmp(optstr,  "CSC")) == 0) {
		device_addrs = CSC_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}

	else if ((strcasecmp(optstr,  "DENC")) == 0) {
		device_addrs = DENC_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}

	else if ((strcasecmp(optstr,  "DAC")) == 0) {
		device_addrs = DAC_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}
	else if ((strcasecmp(optstr,  "IO")) == 0) {
		device_addrs = BUS_CONV_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}
	else if ((strcasecmp(optstr,  "d1")) == 0) {
		device_addrs = D1_ADDR;
		errs = i2c_initalize(device_addrs, base, info);
	}
	else {
		msg_printf(VRB, "i2c <I2C_DEVICES>\n");
		msg_printf(VRB, "I2C_DEVICES ::\n");
		msg_printf(VRB, "\tenc \t Chooses 7199 - ENCODER \n");
		msg_printf(VRB, "\tdenc \t Chooses 7191 - DECODER \n");
		msg_printf(VRB, "\tcsc \t Chooses 7192 - CSC \n");
		msg_printf(VRB, "\tvlut \t Chooses7192 - CSC - LUT \n");
		msg_printf(VRB, "\tdac \t Chooses 7169 - DAC \n");
		msg_printf(VRB, "\tio \t Chooses 8574  - IO \n");
		msg_printf(VRB, "\td1 \t Chooses 7151 - D1 \n");
		return 0;
	}

	return 0;
}
