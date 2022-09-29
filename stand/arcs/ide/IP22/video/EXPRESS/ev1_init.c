#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"

#include "ev1_I2C.h"
#include "ide_msg.h"
#include "uif.h"

struct reg_set {
        int reg;
        int val;
};

/* CC1 - INDIRECTLY ADDRESSED HOST REGS :: Initialize to Safe Default Values */
struct reg_set cc_reg_default[] = {

/* Channel A setup, G2V, 8:8:8:8, begin dbl buffering? */
	0	,0x80,
	/* A 1st line 4+10 = 14.  Video will start 14 lines from top */
	1	,0x4,

/* Channel B setup */
	2	,0,			/* B setup */
	3	,0,			/* B/C 1st line */

/* Channel C setup */
	4	,0,			/* C setup */
	
/* Window A stuff */
	8	,0,			/* A left edge */
	/* A right edge, 127 * 5 = 635. Add 5 for inclusiveness = 640 */
	9	,0x7f,	
	10	,0x4,	/* A blackout,  XXXX MYSTERY */
	11	,0,
	/* Combined with reg 10 A bottom line = 506 */
	12	,0xFA,	
	13	,0,			/* Phase detect */
	14	,0,			/* Phase detect */

/* Window B stuff */
	16	,0,
	17	,0,
	18	,0x10, 			/* Disable B */
	19	,0,
	20	,0,
	21	,0,
	22	,0,

/* Window C stuff */
	24	,0,
	25	,0,
	26	,0x10,			/*Disable C */
	27	,0,
	28	,0,
	29	,0,
	30	,0,
	
/* Overlap and misc */
	32	,0,
	
/* Gen lock */
	33	,0x61,			/* XXX */
	34	,0x2A,			/* XXX Video Horizontal offset */
	35	,0xC,			/* XXX */

	/* 1st unblanked line X 2, should be twice reg 1 +/- 1 */
	36	,0x9,	
	37	,0x30,
	
	38	,0x20, 		/* simulation had A0== video input 0 enabled, 
				   expansion inp 1 enabled */
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
	50	,1,  			/* 0, */
	51	,106, 			/* 0, */
	52	,202 ,			/* 0, */
	53	,222,
	54	,0,
	55	,0,
	56	,0x30,			/* 0x40, Win A to video out */
	57	,0x33,			/* 0, */
	58	,0,
	59	,1,			/* 0, */
	
/* Key generation */
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
	
	/* 128,0, Read only */
};

/* 
	AB internel regs default 
*/
struct reg_set ab_int_default[] = {
/* Win A */
	0x0, 0,		/* X Offset Hi */
	0x1, 0, 	/* X off lo */
	0x2, 0, 	/* Y offset hi */
	0x3, 0, 	/* Y offset lo */
	0x4, 0x20, /* X MOD 5 */
	0x5, 1, 	/* Win A status - Enabled, full size zoom */
	0x6, 2, 	/* Win A info - graph to vid */
	0x7, 0, 	/* Decimation - full size */

/* Window B */
	0x10, 0,		/* X Offset Hi */
	0x11, 0, 	/* X off lo */
	0x12, 0, 	/* Y offset hi */
	0x13, 0, 	/* Y offset lo */
	0x14, 0, 	/* X MOD 5 */
	0x15, 0, 	/* status - Disabled, */
	0x16, 2, 	/* info - graph to vid */
	0x17, 0, 	/* Decimation - full size */

/* Window C */
	0x20, 0,		/* X Offset Hi */
	0x21, 0, 	/* X off lo */
	0x22, 0, 	/* Y offset hi */
	0x23, 0, 	/* Y offset lo */
	0x24, 0, 	/* X MOD 5 */
	0x25, 0, 	/* status - Disabled, */
	0x26, 0, 	/* info - graph to vid */
	-1,-1
};

extern struct gr2_hw  *base;
struct ev1_info {
	unsigned char BoardRev;
	unsigned char CC1Rev;
	unsigned char AB1Rev;
	unsigned char CC1dRamSize;
};


static int
_hw_probe(struct gr2_hw *base, struct ev1_info *info)
{
	int GR2version, Videoversion;
        if (base->hq.mystery != 0xdeadbeef) {
		msg_printf(ERR,"gfx board not found \n");
                return -1;       /* Gr2 board not found */
        }
	
	/* gfx board found */
	GR2version = ~(base->bdvers.rd0) & 0xf;

	if (GR2version < 4) 
		Videoversion = (base->bdvers.rd2 & 0x30) >> 4;
	else
		Videoversion = (base->bdvers.rd1 & 0xC) >> 2;

        if (Videoversion == 3) {
		msg_printf(ERR,"video board not installed \n");
		return -1;
	}
	else {
		/* video board rev. */
		info->BoardRev = ~Videoversion & 0x3; /* Start from rev. 1 */
	}
	return 0;
}



static void
_I2C_force_idle(struct gr2_hw *base)
{
	base->cc1.I2C_bus = 0x8;	/* bit 0 - force idle,
				 * bit 1 - write data,
				 * bit 2 - last byte,
				 * bit 3 - bus speed M_CLK/128,
				 * bit 4 - 7, write has no effect
				 */
}


static int
_I2C_wait_for_idle(struct gr2_hw *base)
{
	int status;
        int idle_wait = 0;

	while ((status = base->cc1.I2C_bus) & I2C_NOT_IDLE) {
		if (idle_wait == MAX_IDLE_WAIT)
			break;
		else {
			idle_wait++;
			DELAY(1);
		}
	}	

	if (idle_wait > MAX_IDLE_WAIT) {
                msg_printf(ERR,"_I2C_wait_for_idle: Timeout, cannot get idle I2C bus!!!!\n");
                return(-1);
        }
        if (status & I2C_ERR){
                msg_printf(ERR,"_I2C_wait_for_idle: I2C bus error\n");
                return(-1);
        }
        return(0);
}

static int
_I2C_wait_for_ack(struct gr2_hw *base)
{
	int status;
	int ack_wait = 0;

	while ((status = base->cc1.I2C_bus) & I2C_NOT_ACK) {
		if (ack_wait == MAX_ACK_WAIT)
			break;
		else {
			ack_wait++;
			DELAY(1);
		}
	}

	if (ack_wait > MAX_ACK_WAIT) {
		msg_printf(ERR,"_I2C_wait_for_ack: Timeout, Acknowledge not received!\n");
		return -1;
	}

	return 0;
}

static int
_I2C_wait_for_xfer(struct gr2_hw *base)
{
	int status;
	int xfer_wait = 0;

	while ((status = base->cc1.I2C_bus) & I2C_XFER_BUSY) {
		if (xfer_wait == MAX_XFER_WAIT)
			break;
		else {	
			xfer_wait++;
			DELAY(1);
		}
	}
	if(xfer_wait > MAX_XFER_WAIT) {
		msg_printf(ERR,"_I2C_wait_for_xfer: Timeout, transfer still busy\n");
		return -1;
	}

	return 0;
}

static int
I2C_status(struct gr2_hw *base, int addr ) 
{
	unsigned char val;

hang_us:
        /* force idle if necessary */
	if (base->cc1.I2C_bus & I2C_NOT_IDLE) {
                _I2C_force_idle(base);
                _I2C_wait_for_idle(base);
        }

        /* Setup for WRITE */
	base->cc1.I2C_bus = I2C_128|I2C_HOLD|I2C_NOT_IDLE;	

        /* wait for idle */
        _I2C_wait_for_idle(base);

        /* write to slave_address, and specify the read opertion */
	base->cc1.I2C_data = addr | 0x1; /* 1 for read, 0 for write */

        _I2C_wait_for_xfer(base);
        if (_I2C_wait_for_ack(base) == -1){
                printf("I2C_status: no ack writing addr to 0x%x\n", (addr | 0x1));
                goto hang_us;
        }

        /* Setup for READ */
	base->cc1.I2C_bus = I2C_128 | I2C_READ | I2C_NOT_IDLE;	

        _I2C_wait_for_xfer(base);

        /* Toggle the read bit, so we are no longer in read state. */
	base->cc1.I2C_bus = I2C_128|I2C_HOLD|I2C_NOT_IDLE;

        /* Status read back from Philips */
	val = base->cc1.I2C_data;
        return(val);
}


/*
	Encoder defaults
*/
struct reg_set DENC_default[] = {       /* defaults */
        /* Encoder defaults */
            0, 0xAC, /* 444 RGB, std chroma, ccir levels, GENLOCK mode */
            1, 0x00, /* trer */
            2, 0x00, /* treg */
            3, 0x00, /* treb */
            4, 0x60, /* CSYNC low, HCL enabled, VCR, interlace, HL disabled */
            5, 0x2e, /* GDC of 0x2c */
            6, 0x52, /* IDEL of 0x52 */
            7, 0x3a, /* PSO of 0x2e */
            8, 0x00, /* enabled, no key, ext clk, LDV phase ref, color on,
                        intr masked */
            9, 0x00, /* reserved */
            10, 0x00,/* reserved */
            11, 0x00,/* reserved */
            12, 0x00,/* CHPS of 0x00 */
            13, 0x00,/* FSC0 of 0x00 */
            14, 0x0c,/* NTSC-M 60 SQP */
            -1, -1,

};


/*
 * Decoder
 */
struct reg_set DMSD_default[] = {
            0x00, 0x50, /* dmsdRegIDEL - 0x51   */
            0x01, 0x30, /* dmsdRegHSYb50 - 0x30 */
            0x02, 0x0,  /* dmsdRegHSYe50 - 0x00 */
            0x03, 0xe8, /* dmsdRegHCLb50 - 0xe8 */
            0x04, 0xb6, /* dmsdRegHCLe50 - 0xb6 */
            0x05, 0xf4, /* dmsdRegHSP50  - 0xf4 */
            0x06, 0x2,  /* dmsdRegLUMA          */
            0x07, 0x00, /* dmsdRegHUE           */
            0x08, 0xF8, /* dmsdRegCKqam         */
            0x09, 0xF8, /* dmsdRegCKsecam       */
            0x0A, 0x90, /* dmsdRegSENpal        */
            0x0B, 0x90, /* dmsdRegSENsecam      */
            0x0C, 0x00, /* dmsdRegGC0           */
            0x0D, 0x4,  /* affected by new layout */
            0x0E, 0x78, /* affected by new layout */
            0x0F, 0x99, /* dmsdRegCTRL3 (0xD1)  */
            0x10, 0,    /* dmsdRegCTRL4*/
            0x11, 0x2c, /* dmsdRegCHCV          */
            0x12, 0 ,
            0x13, 0,
            0x14, 0x34, /* dmsdRegHSYb60        */
            0x15, 0x0a, /* dmsdRegHSYe60        */
            0x16, 0xf4, /* dmsdRegHCLb60        */
            0x17, 0xce, /* dmsdRegHCLe60        */
            0x18, 0xf4, /* dmsdRegHSP60         */
            -1, -1,
};


/*
 * Color Space Conversion
 */
struct reg_set CSC_default[] = {
	0,	0x3A,	/* XXX 4:2:2, matrix bypass, input data to formatter, 
							OE enabled */
	-1, -1,
};

/*
 * Digital to Analog conversion
 */
struct reg_set DAC_default[] = {
                0,      0,
                1,      0,
                2,      0,
                3,      0,
                4,      20,
                5,      20,
                6,      20,
                7,      20,
                -1, -1,
};

/*
 * Initialize 8574
 */
struct reg_set BUS_default[] = {
	0, 0x6,
	-1,-1,
};



/*
 * Initialize 7151 - SQP
 */

/* Not finalized yet, so leave them to all zero's for now.*/
struct reg_set D1_default[] = {
                0,      0,
                1,      0,
                2,      0,
                3,      0,
                4,      0,
                5,      0,
                6,      0,
                7,      0,
                8,      0,
                9,      0,
                10,     0,
                11,     0,
                12,     0,
                13,     0,
                14,     0,
                15,     0,
                16,     0,
                17,     0,
                18,     0,
                -1, -1,
};


static int
I2C_dev_init(struct gr2_hw *base, int addr, struct reg_set *data, int rw)
{
	int i;

	/* force idle if necessary */
	if (base->cc1.I2C_bus & I2C_NOT_IDLE) {
		_I2C_force_idle(base);	 
		if (_I2C_wait_for_idle(base)) return -1;
	}

	while (data->reg != -1){
		if (rw == VID_READ){
			msg_printf(DBG,"Later\n");
			/* ### DEFINE our own shadow register structure */
		} else {
			if (_I2C_wait_for_idle(base)) return -1;

			/* only goes to CC, don't poll */
			base->cc1.I2C_bus = (1 << 2); /* Hold on bus, write */

			/* Select device register to be initialized */
			base->cc1.I2C_data = addr; 
			if (_I2C_wait_for_xfer(base)) return -1;
			if (_I2C_wait_for_ack(base)) {
				msg_printf(ERR,"I2C_dev_init: Device not responding 0x%x\n", addr);
				return -1;
			}
			if (addr == DENC_ADDR) {
				base->cc1.I2C_data = 2;
				if (_I2C_wait_for_xfer(base)) return -1;
	                        if (_I2C_wait_for_ack(base)) {
                                	msg_printf(ERR,"I2C_dev_init: Device not responding 0x%x\n", addr);
                                	return -1;
                        	}
			}

			/* Write to selected device register */
			base->cc1.I2C_data = data->reg;
			if (_I2C_wait_for_xfer(base)) return -1;
			if (_I2C_wait_for_ack(base)) {
				msg_printf(ERR,"I2C_dev_init: Device = 0x%x, Reg = 0x%x not responding\n", addr, data->reg);
				return -1;
			}
			base->cc1.I2C_data = data->val;
			if (_I2C_wait_for_xfer(base)) return -1;
			if (_I2C_wait_for_ack(base)) {
				msg_printf(ERR,"I2C_dev_init: Device = 0x%x, Reg = 0x%x Data = 0x%x not responding\n", addr, data->reg, data->val);
				return -1;
			}
		}
		
		data++;

		/* 
		 * Only write the data, if the registers we are programing are
		 * sequential (register address are auto-incremented).
		 */ 
		while ((data->reg != -1) && ((data-1)->reg + 1 == data->reg)){
			if (rw == VID_READ)
				msg_printf (DBG,"### Later \n");
			else {
				/* write to next data reg, regs auto inc mode*/
			        base->cc1.I2C_data = data->val;
				if (_I2C_wait_for_xfer(base)) return -1;
				if (_I2C_wait_for_ack(base)){
					msg_printf(ERR,"I2C_dev_init:Auto inc, addr = 0x%x Data=0x%x not responding\n", addr,data->reg, data->val);
					return -1;
				}
			}
			data++;
		}
		if ((data->reg != -1) && ((data-1)->reg + 1 != data->reg)){
			msg_printf(DBG,"I2C_dev_init: auto inc mode - noncontiguous regs\n");
		}

		_I2C_force_idle(base);	/* stop auto inc mode */
	}
	return 0;
}

static int
I2C_VLUT(struct gr2_hw *base, int addr, int rw)
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

        /* force idle if necessary */
        if (base->cc1.I2C_bus & I2C_NOT_IDLE) {
                _I2C_force_idle(base);
                if (_I2C_wait_for_idle(base)) return -1;
        }

	if (rw == VID_READ){
		msg_printf(DBG,"### Later \n");
		return(-1);
	} else {
		if (_I2C_wait_for_idle(base)) return -1;

                /* only goes to CC, don't poll */
                base->cc1.I2C_bus = (1 << 2); 	/* Hold on bus, write */
		base->cc1.I2C_data = addr;	/* Select addr reg */
		if (_I2C_wait_for_xfer(base)) return -1;
		if (_I2C_wait_for_ack(base)) {
			msg_printf(ERR,"I2C_VLUT: Writing to addr 0x%x "
				   "not responding\n", addr);
			return -1;
		}
		/* Write to RGB all the same time */
                base->cc1.I2C_data = 4; 
                if (_I2C_wait_for_xfer(base)) return -1;
                if (_I2C_wait_for_ack(base)) {
                        msg_printf(ERR,"I2C_VLUT: Register = 0x%x not responding\n", addr);
			return -1;
		}

		/* Start from Addr 0, auto-inc (automatic) */
                base->cc1.I2C_data = 0;
		if (_I2C_wait_for_xfer(base)) return -1;
		if (_I2C_wait_for_ack(base)){
			msg_printf(ERR,"I2C_VLUT: no ack writing val to addr 0x%x\n", addr);
			return -1;
		}

		/* Initialize the color ramp */
		/* Just send the data, addr is auto-inc */
		for (x = 0; x < 256; x++){
			base->cc1.I2C_data = x;
			if(_I2C_wait_for_xfer(base)) return -1;
			if (_I2C_wait_for_ack(base)){
				msg_printf(ERR,"I2C_VLUT: VLUT not responding to writes \n", x);
				err_count++;
			}
		}
		if (err_count) return -1;
	}
	return(0);
}

static int
initI2C(struct gr2_hw *base, struct ev1_info *info)
{
	register int i;
	extern struct reg_set DENC_default[], DMSD_default[]; 
	extern struct reg_set CSC_default[], DAC_default[];

	msg_printf(DBG,"Initialize 12C devices\n");

	/* Initialize  Encoder Registers */
	if (I2C_dev_init(base, DENC_ADDR,  DENC_default, VID_WRITE)) {
		msg_printf(ERR,"DENC init. failed\n");
		return -1;
	}

	/* Initialize  Decoder Registers */
	if (I2C_dev_init(base, DMSD_ADDR,  DMSD_default, VID_WRITE)) {
		msg_printf(ERR,"DMSD init. failed\n");
		return -1;
	}

	/* Initialize Color Space Converter Registers */
	if (I2C_dev_init(base, CSC_ADDR,  CSC_default, VID_WRITE)) {
		msg_printf(ERR,"CSC init. failed\n");
		return -1;
	}

	/* Initialize DAC Registers */
	if (I2C_dev_init(base, DAC_ADDR,  DAC_default, VID_WRITE)) {
		msg_printf(ERR,"DAC init. failed\n");
		return -1;
	}

	/* Initialize VLUTs */
	if (I2C_VLUT(base, CSC_ADDR, VID_WRITE)) {
		msg_printf(ERR, "VLUTs init. failed\n");
		return -1;
	}

	return(0);
}

static void
initCC1(struct gr2_hw *base, struct ev1_info *info)
{
	register int i;
	extern struct reg_set cc_reg_default[];

	msg_printf(DBG,"Initialize CC1\n");

	/* Initialize CC1 SYSTEM CONTROL REG */
	base->cc1.sysctl = 8;

	/* Initialize INDIRECTLY ADDRESSED HOST REGS */
	i = 0;
	while (cc_reg_default[i].reg != -1) {
		base->cc1.indrct_addreg = cc_reg_default[i].reg;
		base->cc1.indrct_datareg = cc_reg_default[i++].val;	
	}

	base->cc1.fbsel = 0x0;
	/* Get CC1 rev. */
	base->cc1.indrct_addreg = 128;
	info->CC1Rev = base->cc1.indrct_datareg;

	/* dRam size probe - done later */

}

static void
initAB1(struct gr2_hw *base, struct ev1_info *info)
{
	register int i;
	extern struct reg_set ab_int_default[];

	msg_printf(DBG,"Initialize AB1's\n");

	base->ab1.rgb_sel = 0x10; 	/* First Chip ID 0x10 */
	base->ab1.rgb_sel = 0x21; 	/* second Chip ID 0x21 */
	base->ab1.rgb_sel = 0x42; 	/* Third Chip ID 0x42 */
	base->ab1.sysctl = 0x9; 	

	base->ab1.high_addr = 0;
	base->ab1.low_addr = 0;
	base->ab1.tst_reg = 0x4; 

	i = 0;
	while (ab_int_default[i].reg != -1) {
		base->ab1.high_addr = 0;
		base->ab1.low_addr = ab_int_default[i].reg & 0xff;
		base->ab1.intrnl_reg = ab_int_default[i++].val;
	}

	/* Get AB1 rev. */
	base->ab1.high_addr = 0;
	base->ab1.low_addr = 3;
	info->AB1Rev = base->ab1.tst_reg & 0xff;

}

static void
_LoadVC1SRAM(
	register struct gr2_hw	*base,	/* board base address in K1 seg */
	unsigned short *data,
	unsigned int addr,
	unsigned int length )
{
	register int i;

	base->vc1.addrhi = addr >> 8;
	base->vc1.addrlo = addr & 0x00ff;

	for ( i = 0; i < length; i++ )  {
    		base->vc1.sram = data[i] ;
	}
    	return;
}

/*
 * The XMAP initialization.
 */

static void
_XMAPInit(
	register struct gr2_hw	*base) /* board base address in K1 seg */
{
	register int i;
	int pix_mode, pd_mode, pix_pg, video_mode;

	/*
	 * Initialize mode registers
	 */
	
	/* Init XMAP to 8 bit color index mode
	 *	IMPT: Need to set color mode for textport bringup.
	 */

	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;

	/* Only the mode registers of DID 0(default) is initalized */
	pix_mode = 2;	/* 8-bit color index, buffer 0 */
	pd_mode = 0;
	pix_pg = 16;	/* DID 0 only */
	video_mode = 3; /* replace mode */
	base->xmapall.mode =  (((pix_pg << 3) | pix_mode) << 24)  | 
				(((video_mode <<2) | pd_mode) << 16);

	/*
	 * Initialize misc. registers
	 *  XXX X server should  write this when we get ext. fifo 
	 */
	base->xmapall.addrlo = 0;
	base->xmapall.addrhi = 0;

	base->xmapall.misc = 0x0;	/* red component of black out value */
	base->xmapall.misc = 0x0;	/* green component of black out value */
	base->xmapall.misc = 0x0;	/* blue component of black out value */
	
	/* color index GR2_CURSCMAP_BASE+1 will be used as normal cursor color 
         *	for initialization 
	 */
	base->xmapall.misc = 14;	/* pgreg */	
	base->xmapall.misc = 0;		/* cu_reg */

	/* Init. first 8 color map entries and make the rest black */
	base->xmapall.addrhi = (GR2_8BITCMAP_BASE >> 8) & 0xff;
	base->xmapall.addrlo = GR2_8BITCMAP_BASE & 0xff;

	for (i=0; i<8; i++) {
		base->xmapall.clut = 0;
		base->xmapall.clut = 0;
		base->xmapall.clut = 0;
	}
}

unsigned short ev_frtableDID[1024];
unsigned short ev_ltableDID[8];	/* XXX 8  DIDs - for debugging */

static void
initDID(struct gr2_hw *base)
{
	register didnum;
	register i;

#define NPIX_NormMon	1279	/* Must be between 1276-1279, so VC1 will use 
				  same DID # as previous pixel*/

	msg_printf(DBG,"Initialize DID Tables\n");
	/* 
	 * Load DID table
	 */
	for (i=0; i<1023; i++) 
		ev_frtableDID[i] = GR2_DID_LST_BASE;

	/* HACK, bug in VC1.  4 dummy DID needed. */
	ev_frtableDID[i] = GR2_DID_LST_BASE + 4; 

	for(didnum = 0; didnum < 1; didnum++) {
		ev_ltableDID[didnum*8] = 1;
		ev_ltableDID[didnum*8+1] = didnum;	/* Same DID for the whole line */
		/* 
	 	 * HACK, bug in VC1.  4 dummy DID with the previous scanline
	 	 * entry needed at the end of LST.
	 	 */
		ev_ltableDID[didnum*8+2] = ev_ltableDID[didnum*8] + 4;
		ev_ltableDID[didnum*8+3] = ev_ltableDID[didnum*8+1];
		for (i=4; i<=7; i++)
			ev_ltableDID[(didnum*8)+i] = 0;
	}
	_LoadVC1SRAM(base, ev_ltableDID, GR2_DID_LST_BASE, sizeof(ev_ltableDID)/sizeof(short));	
	_LoadVC1SRAM(base, ev_frtableDID, GR2_DID_FRMT_BASE, 1024); 

	/* Load registers of DID table */
	base->vc1.addrhi = (GR2_DID_EP >> 8) & 0xff;
	base->vc1.addrlo = GR2_DID_EP & 0xff;
	base->vc1.cmd0 = GR2_DID_FRMT_BASE;
	base->vc1.cmd0 = GR2_DID_FRMT_BASE + 1024*sizeof(short);
	base->vc1.cmd0 = ((NPIX_NormMon / 5) << 8) | (NPIX_NormMon % 5);

	/*  Enable VC1 and  DID functions
	 * GR2_VC1_VC1 | GR2_VC1_DID
	 */
	base->vc1.sysctl = GR2_VC1_VC1 | GR2_VC1_DID;
	/**********************************************************************
	 * Init XMAPs
	 *********************************************************************/
	/* 
	 * Init. system to be 8-bit color index machine
	 */
	_XMAPInit(base);

	return;
}

ev1_initialize(int argc, char *argv[])
{
	struct ev1_info *info, ev1info;
	int videoinfo;
	int err_count = 0;

	info = &ev1info;
	bzero((void *)info, sizeof(struct ev1_info));
	


	if (_hw_probe(base, info) != 0) {
		err_count++;
		printf("Kaleidoscope  Board Not Found\n");
		return(-1);
	}

#ifdef IP20
	printf("Initialize Starter Board...\n");
	/* ccind37 bit5 RESET */
	base->cc1.indrct_addreg = 37;
	base->cc1.indrct_datareg = 30;

	/* Initialize 8754 Registers */
	if (I2C_dev_init(base, BUS_CONV_ADDR,  BUS_default, VID_WRITE)) {
		msg_printf(ERR,"8754  init. failed\n");
		return -1;
	}
#endif

	initDID(base);
	initCC1(base, info);
	initAB1(base, info);
	if (initI2C(base, info)) err_count++;

	/* checking whether I2C bus is responding by reading back the status */
	videoinfo = I2C_status(base, DMSD_ADDR);
        if (videoinfo & 0x80)
                printf("VCR time constant\n");
        else
                printf("TV time constant\n");
        if (videoinfo & 0x20)
                printf("NTSC 60Hz\n");
        else
                printf("PAL 50Hz\n");
        if (videoinfo & 0x1)
                printf("Color detected\n");
        else
                printf("Monochrome detected\n");

	if (err_count)
		sum_error ("ev1 initialization");
	else
		okydoky();
	return(err_count);

}

