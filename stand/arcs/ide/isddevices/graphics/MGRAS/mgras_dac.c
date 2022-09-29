/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  DAC- ADV 7161  Diagnostics.
 *
 *  $Revision: 1.73 $
 */

#include <math.h>
#include "uif.h"
#include <libsc.h>
#include <libsk.h>
#include "sys/mgrashw.h"
#include "ide_msg.h"
#include "mgras_diag.h"

static GammaSize	Red[256],    Green[256],    Blue[256];
static GammaSize	RedRcv[256], GreenRcv[256], BlueRcv[256];

#ifdef MFG_USED
static _DacInternal_Reg_Info _DAC_Internal_Reg [] = {
	{"MGRAS_DAC_ID_ADDR",		MGRAS_DAC_ID_ADDR, 		0xff},
	{"MGRAS_DAC_REV_ADDR",		MGRAS_DAC_REV_ADDR,		0xff},

	{"MGRAS_DAC_PIXMASK_ADDR",	MGRAS_DAC_PIXMASK_ADDR,		0xff},
	{"MGRAS_DAC_CMD1_ADDR",		MGRAS_DAC_CMD1_ADDR,		0xff},
	{"MGRAS_DAC_CMD2_ADDR",		MGRAS_DAC_CMD2_ADDR,		0xff},
	{"MGRAS_DAC_CMD3_ADDR",		MGRAS_DAC_CMD3_ADDR,		0xff},
	{"MGRAS_DAC_CMD4_ADDR",		MGRAS_DAC_CMD4_ADDR,		0xff},
};
#endif

static ushort_t dac_patrn[] = {
        0x35a, 0x23c, 0x1f0, 0x3a5, 
	0x2c3, 0x10f, 0x0ff,
        0x35a, 0x23c, 0x1f0, 0x3a5, 
	0x2c3, 0x10f, 0x0ff,
};

static ushort_t dac_walking_patrn[] = {
        0x001,   0x002,   0x004,   0x008,
        0x010,   0x020,   0x040,   0x080,
	0x100,	 0x200,	 
        0x0FE,   0x0FD,   0x0FB,   0x0F7,
        0x0EF,   0x0DF,   0x0BF,   0x07F,
	0x2FF,	 0x1FF,

};

#if HQ4
extern __uint32_t *dcbdma_buffer_ptr;
extern void _mgras_create_buffer();
extern void _fill_buffers (uchar_t *, uchar_t *, __uint32_t, __uint32_t);
extern __uint32_t _mgras_rttexture_setup(uchar_t, uchar_t, __uint32_t, __uint32_t *);
#endif

/**********************************************************************/
void
mgras_WriteDac( ushort_t addr, GammaSize *red, GammaSize *green, 
	 	GammaSize *blue,  ushort_t length,  ushort_t mask)
{
	 ushort_t i ;
	 ushort_t dac_data[1000];

	mgras_dacSetAddr16(mgbase, addr);
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

	if (_mgras_dcbdma_enabled ) {
#if HQ4
#define DAC_DCB_DEV 6
	for (i = 0; i < length ;  i++) {
	/*
	 * We need to always keep an eye on the macro mgras_dacSetRGB defined 
	 * in mgrashq.h becos the following code is lifted from the macro. 
	 */
	dac_data[i*3] = ((((red[i].channel & mask) << 6) & 0xff00) | ((red[i].channel & mask) & 0x3)) ;
	dac_data[i*3+1] = ((((green[i].channel & mask) << 6) & 0xff00) | ((green[i].channel & mask) & 0x3)) ;
	dac_data[i*3+2] = ((((blue[i].channel & mask) << 6) & 0xff00) | ((blue[i].channel & mask) & 0x3)) ;
	}
	_mgras_create_buffer();
	_fill_buffers ((uchar_t *)dcbdma_buffer_ptr, (uchar_t *)dac_data, length*3*2, length*3*2);
	mgbase->dcb_dma_con =
		((DAC_DCB_DEV << 6) | (1 << 3) | (1 << 2) | 0) ;
	_mgras_rttexture_setup(TEX_DCB_IN, 0, length*3*2, dcbdma_buffer_ptr);
#endif
	} else {

          for (i = 0; i < length ; i++) {
                mgras_dacSetRGB(mgbase, (red[i].channel & mask), (green[i].channel & mask), (blue[i].channel &mask) );
		msg_printf(6, "Writing i = %x, r = %x g = %x b = %x\n" ,i, (red[i].channel & mask), (green[i].channel & mask), (blue[i].channel &mask) );
                if (i & 0x3)
                {
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
          }
	} /* else */
}

void
mgras_ReadDac( ushort_t addr, GammaSize *red, GammaSize *green, 
				     GammaSize *blue,  ushort_t length)
{
	 ushort_t i;

	mgras_dacSetAddr16(mgbase, addr);
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	for(i=0; i< length; i++) {
		mgras_dacGetRGB(mgbase, (red[i].channel), (green[i].channel), (blue[i].channel) );
		msg_printf(6, "Read i = %x, r = %x g = %x b = %x\n" ,*(red+i), *(green+i), *(blue+i));
                if (i & 0x3)
                {
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
	}
}

__uint32_t
mgras_DacCompare(char *TestName, GammaSize *exp,  GammaSize *rcv,  
		 	 ushort_t length,  ushort_t mask)
{
	 ushort_t i;
	 ushort_t err = 0;

	for(i=0; i<length; i++) {
		if ((exp[i].channel & mask) != (rcv[i].channel & mask) ) {
			++err;
			msg_printf(ERR, "%s i=%d, exp=%x, rcv=%x\n" 
			     ,TestName,i,(exp[i].channel & mask), (rcv[i].channel & mask) );
		}
		msg_printf(DBG, "%s i=%d, exp=%x, rcv=%x\n" 
			    ,TestName, i,(exp[i].channel & mask), (rcv[i].channel & mask) );
	}
	return (err);
}

__uint32_t 
mgras_ClrPaletteAddrUniqTest(void) 
{
	ushort_t i;
	__uint32_t errors = 0;
	
	MGRAS_GFX_CHECK();

	msg_printf(VRB, "Color Palette Address Uniquness Test\n");

	/* First Check the lower 8 Bits */
	for (i = 0; i < 256 ; i++){
		Red[i].channel = i; Green[i].channel = i; Blue[i].channel = i;
	}
	
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );

#ifdef DEBUG
	msg_printf(DBG, "Next: Check the Lower Bits (7:0)\n");
#endif
	mgras_WriteDac(0x00, Red,    Green,    Blue,    0xff, 0x3ff);
	mgras_ReadDac(0x00,  RedRcv, GreenRcv, BlueRcv, 0xff);

	errors  = mgras_DacCompare("Red Palette Addr Uniq Test", Red,   RedRcv,   0xff, 0x3ff);
	errors += mgras_DacCompare("Grn Palette Addr Uniq Test", Green, GreenRcv, 0xff, 0x3ff);
	errors += mgras_DacCompare("Blu Palette Addr Uniq Test", Blue,  BlueRcv,  0xff, 0x3ff);

#ifdef DEBUG
	msg_printf(DBG, "Next: Check the Upper Bits (9:8)\n");
#endif
	for (i = 0; i < 4 ; i++){
		Red[i].channel = ((i << 8) | 0x55) ; Green[i].channel = ((i << 8) | 0xaa); Blue[i].channel = ((i << 8) | 0xcc);
	}
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );
	mgras_WriteDac(0x00, Red,    Green,    Blue,    0x4, 0x3ff);
	mgras_ReadDac(0x00,  RedRcv, GreenRcv, BlueRcv, 0x4);
	errors  = mgras_DacCompare("Red Palette Addr Uniq Test", Red,   RedRcv,   0x4, 0x3ff);
	errors += mgras_DacCompare("Grn Palette Addr Uniq Test", Green, GreenRcv, 0x4, 0x3ff);
	errors += mgras_DacCompare("Blu Palette Addr Uniq Test", Blue,  BlueRcv,  0x4, 0x3ff);

	REPORT_PASS_OR_FAIL((&BackEnd[DAC_CLR_PLLT_ADDR_UNIQ_TEST]), errors);
	
}

__uint32_t 
mgras_ClrPaletteWalkBitTest(void) 
{
	 ushort_t i;
	 __uint32_t errors = 0;
	 ushort_t length = 256;

	MGRAS_GFX_CHECK();
	msg_printf(VRB, "Color Palette Walking Bit: DataBus Test\n");
	for (i = 0; i < length ; i++){
		Red[i].channel = Green[i].channel = Blue[i].channel = dac_walking_patrn[i%20];
	}

	bzero(RedRcv,   sizeof(RedRcv) );
        bzero(BlueRcv,  sizeof(BlueRcv) );
        bzero(GreenRcv, sizeof(GreenRcv) );

	mgras_WriteDac(0x00, Red,   Green,   Blue,    length, 0x3ff);
	mgras_ReadDac(0x00,  RedRcv, GreenRcv, BlueRcv, length);

	errors  = mgras_DacCompare("Red Palette Data WalkBit Test", Red,   RedRcv,   length,	0x3ff);
	errors += mgras_DacCompare("Grn Palette Data WalkBit Test", Green, GreenRcv, length,	0x3ff);
	errors += mgras_DacCompare("Blu Palette Data WalkBit Test", Blue,  BlueRcv,  length,	0x3ff);

	REPORT_PASS_OR_FAIL((&BackEnd[DAC_CLR_PLLT_WALK_BIT_TEST]), errors);
}

__uint32_t 
mgras_ClrPalettePatrnTest(void) 
{
	 ushort_t i;
	 ushort_t loop;
	 __uint32_t errors;
	 ushort_t *PatrnPtr;

	MGRAS_GFX_CHECK();
	msg_printf(VRB, "Color Pallette Pattern Test\n");
	for(loop=0; loop<7; loop++) {
		PatrnPtr = dac_patrn + loop;
		msg_printf(DBG, "Loop= %d, Filling GAMMA RAM with:\n", loop);
		msg_printf(DBG, "%x %x %x %x %x %x %x\n",
			*(PatrnPtr),    *(PatrnPtr+1),  *(PatrnPtr+2),
			*(PatrnPtr+3),  *(PatrnPtr+4),  *(PatrnPtr+5), *(PatrnPtr+6) );

		for (i = 0; i < 256 ; i++){
			Red[i].channel = Green[i].channel = Blue[i].channel = *(PatrnPtr + (i%7) ) ;
		}

		bzero(RedRcv,   sizeof(RedRcv) );
		bzero(BlueRcv,  sizeof(BlueRcv) );
		bzero(GreenRcv, sizeof(GreenRcv) );
	
		mgras_WriteDac(0x00, Red, Green, Blue, 0xff, 0x3ff);
		mgras_ReadDac(0x00,  RedRcv, GreenRcv, BlueRcv, 0xff);
	
		errors  = mgras_DacCompare("Red Palette Patrn Test", Red,   RedRcv,   0xff,	0x3ff);
		errors += mgras_DacCompare("Grn Palette Patrn Test", Green, GreenRcv, 0xff,	0x3ff);
		errors += mgras_DacCompare("Blu Palette Patrn Test", Blue,  BlueRcv,  0xff,	0x3ff);
	}

	
	REPORT_PASS_OR_FAIL((&BackEnd[DAC_CLR_PLLT_PATRN_TEST]), errors);
}


__uint32_t 
_mgras_DacInternalRegisterTest(char *str,  __uint32_t RegId,  __uint32_t BitMask)
{
	 __uint32_t errors = 0;
	 ushort_t i, rcv;
	 ushort_t orig;

	/* Save the current value before the test */
	mgras_dacGetAddrCmd16(mgbase, RegId, orig);
	
	for(i=0; i < sizeof(dac_walking_patrn); i++) {
		mgras_dacSetAddrCmd16(mgbase, RegId, (dac_walking_patrn[i]& 0xff) );
		rcv = 0xbeef;
		CORRUPT_DCB_BUS();
		mgras_dacGetAddrCmd(mgbase, RegId, rcv);
		COMPARE(str, RegId, (dac_walking_patrn[i] & 0xff), rcv, BitMask, errors);
	}

	/* Restore the original value before leaving */
	mgras_dacSetAddrCmd16(mgbase, RegId, orig);

	errors += mgras_DacReset();

	return (errors);
}

#ifdef MFG_USED
__uint32_t 
mgras_DacCtrlRegTest(void) 
{
	 ushort_t i;
	 ushort_t rcv;
	 ushort_t exp;
	 __uint32_t errors = 0;
	
	MGRAS_GFX_CHECK();
	msg_printf(VRB, "DAC Control Register Test\n");
	for(i=0; i<(sizeof(_DAC_Internal_Reg)/sizeof(_DAC_Internal_Reg[0])); i++) {
		msg_printf(DBG, "Testing Internal Register Test %s 0x%x\n" ,_DAC_Internal_Reg[i].str, _DAC_Internal_Reg[i].RegName);
		switch (_DAC_Internal_Reg[i].RegName) {
			case MGRAS_DAC_ID_ADDR:
				rcv = 0xdead;
				exp = DAC_ID;
				CORRUPT_DCB_BUS();
				mgras_dacGetAddrCmd(mgbase, _DAC_Internal_Reg[i].RegName, rcv);
				if (exp != rcv) {
					if (exp == DAC_ID_OLD)  {
						msg_printf(VRB, "WARNING -- OLDER DAC in Use!\n");
						exp = DAC_ID_OLD;
					}
				}
				COMPARE(_DAC_Internal_Reg[i].str, _DAC_Internal_Reg[i].RegName, exp, rcv, 0xff, errors);
				break;
			case MGRAS_DAC_REV_ADDR:
				rcv = 0xbeef;
				exp = DAC_REVISION;
				CORRUPT_DCB_BUS();
				mgras_dacGetAddrCmd(mgbase, _DAC_Internal_Reg[i].RegName, rcv);
				if (exp != rcv) {
					if (exp == DAC_REVISION_OLD)  {
						msg_printf(VRB, "WARNING -- OLDER DAC in Use!\n");
						exp = DAC_REVISION_OLD;
					}
				}
				COMPARE(_DAC_Internal_Reg[i].str, _DAC_Internal_Reg[i].RegName, exp, rcv, 0xff, errors);
				break;

			case MGRAS_DAC_PIXMASK_ADDR:
#ifdef YOU_SHOULD_NOT_CHECK_THIS_REGISTER
			case MGRAS_DAC_CMD1_ADDR:
			case MGRAS_DAC_CMD2_ADDR:
			case MGRAS_DAC_CMD3_ADDR:
			case MGRAS_DAC_CMD4_ADDR:
#endif
				errors += _mgras_DacInternalRegisterTest(_DAC_Internal_Reg[i].str, _DAC_Internal_Reg[i].RegName, _DAC_Internal_Reg[i].mask);
				break;

		}
	}
	REPORT_PASS_OR_FAIL((&BackEnd[DAC_CONTROL_REG_TEST]), errors);
}
#endif /* MFG_USED */

__uint32_t 
mgras_DacModeRegTest(void) 
{
	 ushort_t i;
	 ushort_t orig;
	 ushort_t rcv;
	 __uint32_t errors = 0;

	MGRAS_GFX_CHECK();
	msg_printf(VRB, "DAC Mode Register Test\n");
	mgras_dacGetMode(mgbase, orig );	/* Get the mode register */
	for(i=0; i < sizeof(dac_walking_patrn); i++) {
		mgras_dacSetMode(mgbase, (dac_walking_patrn[i] & 0xff) );
		CORRUPT_DCB_BUS();
		mgras_dacGetMode(mgbase, rcv );
		COMPARE("Dac Mode Reg Test" ,0, dac_walking_patrn[i], rcv, 0xff, errors);
	}
	mgras_dacSetMode(mgbase, orig);		/* Set the original value */
	REPORT_PASS_OR_FAIL((&BackEnd[DAC_MODE_REG_TEST]), errors);
}

__uint32_t 
mgras_DacAddrRegTest(void) 
{
	 ushort_t i 	= 0;
	 __uint32_t errors 	= 0;
	 ushort_t rcv 	= 0xbeef;
	MGRAS_GFX_CHECK();
	msg_printf(VRB, "DAC Address Register Test\n");
	for(i=0; i < sizeof(dac_patrn); i++) {
		mgras_dacSetAddr(mgbase, dac_patrn[i]);
		mgras_dacGetAddr(mgbase, rcv);
		COMPARE("Dac Addr Reg Test (8-Bit)" ,0,dac_patrn[i], rcv,0xff, errors);
	}

	for(i=0; i < sizeof(dac_patrn); i++) {
		mgras_dacSetAddr16(mgbase, dac_patrn[i]);
		mgras_dacGetAddr16(mgbase, rcv);
		COMPARE("Dac Addr Reg Test (16-Bit)" ,0,dac_patrn[i], rcv,0xff, errors);
	}
	REPORT_PASS_OR_FAIL((&BackEnd[DAC_ADDR_REG_TEST]), errors);
}

#ifdef MFG_USED
int
mgras_PeekClrPalette(int argc, char **argv)
{
	 __uint32_t addr 	= 0x00;
	 __uint32_t length	= 0x1;
	GammaSize _Red, _Green, _Blue;

	MGRAS_GFX_CHECK() ;
	if (argc != 2) {
		msg_printf(SUM, "usage: %s Addr \n" , argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&addr);

	mgras_ReadDac(addr, &_Red, &_Green, &_Blue, length);
	msg_printf(SUM, "Reading Dac Pallate Addr %x returns R %x G %x B %x\n" ,addr, _Red.channel, _Green.channel, _Blue.channel);

	return 0;
}

int
mgras_PokeClrPalette(int argc, char **argv)
{
	 __uint32_t addr 	= 0x00;
	 __uint32_t RedVal 	= 0xff;
	 __uint32_t BluVal	= 0xff;
	 __uint32_t GrnVal	= 0xff;
	 __uint32_t length	= 0x1;
	 __uint32_t mask 	= 0x3ff;
	GammaSize _Red, _Green, _Blue;

	MGRAS_GFX_CHECK() ;
	if (argc != 5) {
		msg_printf(SUM, "usage: %s Addr RedVal GreenVal BlueVal\n" , argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&addr);
	atohex(argv[2], (int*)&RedVal);
	atohex(argv[3], (int*)&GrnVal);
	atohex(argv[4], (int*)&BluVal);

	_Red.channel = RedVal; _Green.channel = GrnVal; _Blue.channel = BluVal;
	mgras_WriteDac(addr, &_Red, &_Green, &_Blue, length, mask);
	msg_printf(SUM, "Writing Dac Pallate Addr %x with R %x G %x B %x\n" ,addr, RedVal, GrnVal, BluVal);

	return 0;
}
#endif /* MFG_USED */

int
mgras_PokeDacCtrl(int argc, char **argv)
{
	 __uint32_t value = 0xdeadbeef;
	 __uint32_t addr;

	if (argc != 2) {
		msg_printf(SUM, "usage %s  AddrValue\n" ,argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&value);
	msg_printf(SUM, "value = 0x%x\n", value);
	mgras_dacGetAddr16(mgbase, addr);
	msg_printf(SUM, "Dac Addr Reg =  0x%x\n" ,addr);
	mgras_dacSetCtrl(mgbase, value);

	return 0;
}

__uint32_t
mgras_PeekDacCtrl(void)
{
	 __uint32_t addr = 0xdeadbeef;
	 __uint32_t Rcv;

	mgras_dacGetAddr16(mgbase, addr);
	msg_printf(SUM, "Dac Addr Reg = 0x%x\n" ,addr);
	mgras_dacGetCtrl(mgbase, Rcv);
	msg_printf(SUM, "Ctrl value = 0x%x\n", Rcv);

	return 0;
}

#ifdef MFG_USED
int
mgras_PokeDacAddr(int argc, char **argv)
{
	 __uint32_t AddrValue = 0xdeadbeef;

	if (argc != 2) {
		msg_printf(SUM, "usage %s  AddrValue\n" ,argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&AddrValue);
	msg_printf(SUM, "Dac Addr Reg = 0x%x\n" ,AddrValue);
	mgras_dacSetAddr(mgbase, AddrValue); 
	msg_printf(SUM, "Wrote Dac Addr Reg with 0x%x\n" ,AddrValue);

	return 0;
}

__uint32_t
mgras_PeekDacAddr(void)
{
	 __uint32_t rcv = 0xdeadbeef;

	mgras_dacGetAddr(mgbase, rcv);
	msg_printf(SUM, "Reading Dac Addr Reg returns 0x%x\n" ,rcv);
	return 0;
}

int
mgras_PokeDacAddr16(int argc, char **argv)
{
	 __uint32_t AddrValue = 0xdeadbeef;

	if (argc != 2) {
		msg_printf(SUM, "usage %s  AddrValue\n" ,argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&AddrValue);
	msg_printf(SUM, "Dac Addr Reg = 0x%x\n" ,AddrValue);
	mgras_dacSetAddr16(mgbase, AddrValue); 
	msg_printf(SUM, "Wrote Dac Addr Reg with 0x%x\n" ,AddrValue);

	return 0;
}

__uint32_t
mgras_PeekDacAddr16(void)
{
	 __uint32_t rcv = 0xdeadbeef;

	mgras_dacGetAddr16(mgbase, rcv);
	msg_printf(SUM, "Reading Dac Addr Reg returns 0x%x\n" ,rcv);

	return 0;
}

#endif /* MFG_USED */

void
mgras_DacPLLInit(__uint32_t argc, char **argv)
{
    __uint32_t rval, vval, cval = 0;
	__uint32_t bad_arg = 0;

    msg_printf(DBG, "Entering mgras_DacPLLInit...\n");

   /* get the args */
   argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'r':
			if (argv[0][2]=='\0') {
				atobu(&argv[1][0], &rval);
				argc--; argv++;
			} else {
				atobu(&argv[0][2], &rval);
			}
			break;
		case 'v':
			if (argv[0][2]=='\0') {
				atobu(&argv[1][0], &vval);
				argc--; argv++;
			} else {
				atobu(&argv[0][2], &vval);
			}
			break;
		case 'c':
			if (argv[0][2]=='\0') {
				atobu(&argv[1][0], &cval);
				argc--; argv++;
			} else {
				atobu(&argv[0][2], &cval);
			}
			break;
		default: bad_arg++; break;
	}
	argc--; argv++;
   }

   if ( bad_arg || argc ) {
	msg_printf(SUM,
	 "Usage: mg_dacpll -r[R value] -v[V value] -c[CTRL value]\n");
	 return;
   }
   msg_printf(DBG, "R = 0x%x; V = 0x%x; CTRL = 0x%x\n", rval, vval, cval);

   mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   rval);
   mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   vval);
   mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, cval);

   msg_printf(DBG, "DAC PLL values are initialized\n");

   msg_printf(DBG, "Leaving mgras_DacPLLInit...\n");
}

__uint32_t
_mgras_DacPLLInit(__uint32_t TimingSelect)
{
	msg_printf (DBG, "Entering _mgras_DacPLLInit...\n");

#if HQ4
	/* Bit #3 is DGTL_SYNC_OUT */
    /* mgbase->bdvers.bdvers0 = (0x1 << DGTL_SYNC_OUT_BIT) | MGRAS_BC1_REPP_RESET_BIT ; 	 */

	msg_printf(DBG, "Bit 3 is set to 1 -> DGTL_SYNC_OUT pulled up\n");
#endif

	/* initialize to 60hz timing since some boards don't seem to be able
	 * to switch from 72hz timin to MAX timing without this setting.
	 */
 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x4);
 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x91);
	DELAY(50000);

	/* PLL Initialize */
	        switch(TimingSelect) {
			case	_1280_1024_76 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x41);
			break;
			case	_1280_1024_72 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0xd);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0xd5);
			break;
			case	_1280_1024_60 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x91);
			break;
			case	_1280_1024_50 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x6);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x11);
			break;
			case	_1920_1035_60FI : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0xb);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0xc);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0xd1);
			break;
			case	_1280_492_120ST : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x91);
			break;
			case	_1600_1200_60 : 
				/* Dac Frequency of 159.9 */
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0xb);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0xd);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0xc1);
			break;
			case	_1600_1200_60_SI : 
				/* Dac Frequency of 150.0 */
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x7);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x8);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x41);
			break;
			case	_1280_1024_30I : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x3);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x6);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x21);
			break;
			case	_1024_768_60 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x3);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x7);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0xd5);
			break;
			case	_1024_768_60p : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x5);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x21);
			break;
			case	_1024_768_100ST : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x91);
			break;
			case	_1280_959_30RS343 : 
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x7);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x61);
			break;
			case	NTSC_VCO_VAL : 
				/*
 			     * XXX: These values are not right.
				 * Phil or Mark should give us the values 
				 */
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x0);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x0);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x0);
			break;
			case	PAL_VCO_VAL : 
				/*
 			     * XXX: These values are not right.
				 * Phil or Mark should give us the values 
				 */
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x0);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x0);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x0);
			break;
                	case    _CMAP_SPECIAL :
			default:
				msg_printf(VRB, "DacPLLInit 1280_1024_60\n");
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_R_ADDR,   0x2);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_V_ADDR,   0x4);
			 	mgras_dacSetAddrCmd16(mgbase, MGRAS_DAC_PLL_CTL_ADDR, 0x91);
                        break;
		}
		DELAY(30);
		DELAY(50000);
#if HQ4
#if 0
        	mgbase->bdvers.bdvers0 = (((0x1 << DGTL_SYNC_OUT_BIT) | MGRAS_BC1_REPP_RESET_BIT |
				(0x1 << BD_VC3_RESET_BIT)) << 24);

		msg_printf(DBG, "Writing config register with 0x%x\n",
			((0x1 << DGTL_SYNC_OUT_BIT) | MGRAS_BC1_REPP_RESET_BIT | (0x1 << BD_VC3_RESET_BIT)));
#endif
#else
		mgbase->bdvers.bdvers0 = (BD_VC3_RESET << 24);
#endif

		mgras_vc3SetReg(mgbase,VC3_CONFIG, 0x1);

		msg_printf(DBG, "VC3 Config register is set to 1 \n");

		msg_printf(DBG, "DacPLLInit done\n");
	return 0;
}

__uint32_t
mgras_DacReset(void)
{
	 ushort_t rcv, IDReg, DacRev;
	 ushort_t DisableCurs, PixMask, cmd1, cmd2, cmd3, cmd4;
	 __uint32_t errors = 0;
	 __uint32_t OldDac = 0;

	/* XXXXX Dac seems to power up 7150 Mode.  We need to do some more hacking for init in 7161/2 Mode.	*/
	mgras_dacSetMode(mgbase, 0x3);	/* DAC Reset = 1 */
	mgras_dacSetMode(mgbase, 0x2);	/* Assert -  DAC Reset */
	mgras_dacSetMode(mgbase, 0x3);	/* Release - DAC Reset & set Dac MPU Port to 8 Bit */

	mgras_dacSetAddr(mgbase, 0x5);	/* Addr-Lo  ONLY - DO NOT WRITE TO THE UPPER YET! */
	mgras_dacSetCtrl(mgbase, 0x89); /* Enables Access to Addr Hi, Single Write only */
	
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CMD5_ADDR, 0x40);		/* Must be set on for PLL  - at power */

	rcv = 0xdead;
	mgras_dacSetAddr16(mgbase, 0); 	/* Addr. Inter. reg = 0 */
	mgras_dacGetAddr16(mgbase, rcv);
	if ((rcv & 0xff) != 0) {
		++errors;
		msg_printf(ERR, "mgras_DacReset Addr Reg 0 Exp = 0x0 Rev= 0x%x\n",rcv&0xff);
	}

	/* DAC_ID */
	IDReg = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_ID_ADDR, IDReg);

	if ((IDReg & 0xff) == DAC_ID_OLD)  {
			++OldDac;
			msg_printf(VRB, "WARNING -- OLDER DAC in Use!\n");
	}

	if ((IDReg & 0xff) != DAC_ID)  {
		if ((IDReg & 0xff) != DAC_ID_OLD) {
			++errors;
			msg_printf(ERR, "mgras_DacReset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MGRAS_DAC_ID_ADDR, DAC_ID, (IDReg & 0xff));
		}
	}

	/* DAC_REV */
	DacRev = 0x0;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_REV_ADDR, DacRev);
	if (OldDac) {
		if ((DacRev & 0xff) != DAC_REVISION_OLD) {
			++errors;
			msg_printf(ERR, "mgras_DacReset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MGRAS_DAC_REV_ADDR, DAC_REVISION, (DacRev & 0xff));
		}
	} else {
		if ((DacRev & 0xff) != DAC_REVISION) {
			++errors;
			msg_printf(ERR, "mgras_DacReset Addr Reg %x Exp = 0x%x Rev= 0x%x\n" ,MGRAS_DAC_REV_ADDR, DAC_REVISION, (DacRev & 0xff));
		}
	}

	cmd1 = 0x89;
	cmd2 = 0xe4;
	cmd3 = 0xc0;
	cmd4 = 0x6;
	PixMask = 0xff;
	DisableCurs = 0x1;

	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_PIXMASK_ADDR, 0xff);		/* RGB */
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CMD1_ADDR, cmd1);		/* Bypass Mode */
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CMD2_ADDR, cmd2);		/* 7.5IRE? */
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CMD3_ADDR, cmd3);		/* 4:1 pix. mux. */
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CMD4_ADDR, cmd4);		/* Sync set-up */
	mgras_dacSetAddrCmd16(mgbase,MGRAS_DAC_CURS_ADDR, DisableCurs);		/* Disable Cursor */


	/* Read Back & verify to see if we have initialized the dac properly */
	rcv = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_PIXMASK_ADDR, rcv);
	COMPARE("DAC RESET :- MGRAS_DAC_PIXMASK_ADDR", 0, PixMask, rcv, 0xff, errors);

	rcv = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_CMD1_ADDR, rcv);
	COMPARE("DAC RESET :- MGRAS_DAC_CMD1_ADDR", 0, cmd1, rcv, 0xff, errors);

	rcv = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_CMD2_ADDR, rcv);
	COMPARE("DAC RESET :- MGRAS_DAC_CMD2_ADDR", 0, cmd2, rcv, 0xff, errors);

	rcv = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_CMD3_ADDR, rcv);
	COMPARE("DAC RESET :- MMGRAS_DAC_CMD3_ADDR", 0, cmd3, rcv, 0xff, errors);

	rcv = 0xdead;
	mgras_dacGetAddrCmd16(mgbase, MGRAS_DAC_CMD4_ADDR, rcv);
	COMPARE("DAC RESET :- MGRAS_DAC_CMD4_ADDR", 0, cmd4, rcv, 0xff, errors);

	mgras_dacGetAddrCmd16(mgbase,MGRAS_DAC_CURS_ADDR, rcv);
	COMPARE("DAC RESET :- MGRAS_DAC_CURS_ADDR", 0, DisableCurs, rcv, 0xff, errors);

#ifdef DOES_NOT_WORK
	GammaTableLoad(LINEAR_RAMP);
#endif /* DOES_NOT_WORK */

	return (errors);
}

#ifdef MFG_USED
int
mgras_PokeDacMode(int argc, char **argv)
{
	 __uint32_t DacMode = 0xdeadbeef;

	if (argc != 2) {
		msg_printf(SUM, "usage %s Value\n" ,argv[0]);
		return -1;
	}
	atohex(argv[1], (int*)&DacMode);

	msg_printf(SUM, "Writing DacMode Register with 0x%x\n" ,DacMode);
	mgras_dacSetMode(mgbase, DacMode);

	return 0;
}

__uint32_t
mgras_PeekDacMode(void)
{
	 __uint32_t DacMode = 0xdeadbeef;

	mgras_dacGetMode(mgbase, DacMode);
	msg_printf(SUM, "Reading DacMode Register returns 0x%x\n" ,DacMode);

	return 0;
}
#endif /* MSG_USED */
