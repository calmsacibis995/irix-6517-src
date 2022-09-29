/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  Bt473 DAC Diagnostics
 *  $Revision: 1.11 $
 */

#include <sys/types.h>
#include <libsc.h>
#include <uif.h>
#include <math.h>
#include "sys/mgrashw.h"
#include "ide_msg.h"
#include "mco_diag.h"

#define COLORPALETTE	1
#define OVERLAY		2

#define COLORPALETTESIZE	0x100
#define OVERLAYSIZE		0x10

typedef struct _GammaSize{
         unsigned channel : 10 ;
}GammaSize;

/* XXX From mgras_diag.h */
typedef struct _DacInternal_Reg_Info {
		char *str;
		ushort_t RegName;
                ushort_t mask;
} _DacInternal_Reg_Info ;

extern mgras_hw *mgbase;
extern void mco_set_dcbctrl(mgras_hw *, int crs);

static GammaSize	Red[COLORPALETTESIZE];
static GammaSize	Green[COLORPALETTESIZE];
static GammaSize	Blue[COLORPALETTESIZE];
static GammaSize	RedRcv[COLORPALETTESIZE];
static GammaSize	GreenRcv[COLORPALETTESIZE];
static GammaSize	BlueRcv[COLORPALETTESIZE];

static ushort_t dac_patrn[] = {
        0x5a, 0x3c, 0xf0, 0xa5, 
	0xc3, 0x0f, 0xff,
        0x5a, 0x3c, 0xf0, 0xa5, 
	0xc3, 0x0f, 0xff
};

static ushort_t dac_walking_patrn[] = {
        0x001,   0x002,   0x004,   0x008,
        0x010,   0x020,   0x040,   0x080,
        0x0FE,   0x0FD,   0x0FB,   0x0F7,
        0x0EF,   0x0DF,   0x0BF,   0x07F

};

/*
 *	Forward References 
 */
void mco_WriteDac473( uchar_t,ushort_t,ushort_t,GammaSize *red,GammaSize *green,GammaSize *blue, ushort_t);

void mco_ReadDac473( uchar_t, ushort_t, ushort_t, GammaSize *, GammaSize *, GammaSize *,  ushort_t);

int mco_Dac473Compare(char *, uchar_t, GammaSize *,  GammaSize *,  ushort_t);

int mco_473ClrPalettePatrnTest(uchar_t);
int mco_473ClrPaletteWalkBitTest(uchar_t);
__uint32_t _mco_Dac473RAMAddrRegTest(uchar_t);
__uint32_t _mco_Dac473OverlayAddrRegTest(uchar_t);
__uint32_t _mco_Dac473ClrPaletteAddrUniqTest(uchar_t);
__uint32_t _mco_Dac473OvrlayPaletteAddrUniqTest(uchar_t);
__uint32_t _mco_Dac473OvrlayPalettePatrnTest(uchar_t);
__uint32_t _mco_Dac473OvrlayPaletteWalkBitTest(uchar_t);

/*
 * mco_WriteDac473 -- Fills 473 Color Palette RAM or Overlay Palette with
 * RGB data
 */

void
mco_WriteDac473(uchar_t device,	    /* Specifies MCO_ADV473A or MCO_ADV473B */
	ushort_t palette,   /* Specifies Color Palette RAM or Overlay Palette */
	ushort_t addr,      /* Specifies Address in Palette */
	GammaSize *red,   
	GammaSize *green,   
	GammaSize *blue,   
	ushort_t length)        /* # of RGB values to write */
{
	ushort_t i;

	switch (palette) {
	    case COLORPALETTE:
		/* Set 473 Address Register to Color Palette addr */
		MCO_ADV473_SETCPALWR(mgbase, device, addr);

		/* Set MCO Index Register to point to Color Palette RAM */
		MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CPRAM);
		break;
	    case OVERLAY:
		/* Set 473 Address Register to Overlay addr */
		MCO_ADV473_SETOVRLAYWR(mgbase, device, addr);

		/* Set MCO Index Register to point to Overlay Registers */
		MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_OREG);
		break;
	}

	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

	for (i = 0; i < length ; i++)
        {
	    mco_473SetRGB(mgbase, red[i].channel, green[i].channel, blue[i].channel );
	    msg_printf(DBG, "Writing i = %x, r= %x g = %x b = %x\n" ,i, red[i].channel, green[i].channel, blue[i].channel);
	    if (i & 0x3) {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    }
	}
}

/*
 * mco_ReadDac473 -- Reads RGB data from 473 Color Palette RAM or Overlay 
 * Palette
 */

void
mco_ReadDac473(uchar_t device,	    /* Specifies MCO_ADV473A or MCO_ADV473B */
		ushort_t palette,   /* Specifies Color Palette RAM or Overlay */
				    /* Palette */
		ushort_t addr,      /* Specifies Address in Palette */
		GammaSize *red,   
		GammaSize *green,   
		GammaSize *blue,   
		ushort_t length)        /* # of RGB values to read */
{
	ushort_t i;

	switch (palette) {
	    case COLORPALETTE:
		/* Set 473 Address Register to Color Palette addr */
		MCO_ADV473_SETPALRD(mgbase, device, addr);

		/* Set MCO Index Register to point to Color Palette RAM */
		MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CPRAM);
		break;
	    case OVERLAY:
		/* Set 473 Address Register to Overlay addr */
		MCO_ADV473_SETOVRLAYRD(mgbase, device, addr);

		/* Set MCO Index Register to point to Overlay Registers */
		MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_OREG);
		break;
	}

	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

	for (i=0; i< length; i++) {
	    mco_473GetRGB(mgbase, (red[i].channel), (green[i].channel), (blue[i].channel) );
	    msg_printf(DBG, "Read i = %x, r= %x g = %x b = %x\n",
	    *(red+i), *(green+i), *(blue+i));
	    if (i & 0x3)
	    {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	    }
	}
}

/*
 * mco_Dac473Compare -- Compares two arrays (exp and rcv) and reports 
 * 			errors for any differences
 */

/*ARGSUSED2*/
int
mco_Dac473Compare(char *TestName,    /* Test name for error messages */
		uchar_t device,      /* Specifies MCO_ADV473A or MCO_ADV473B */
		GammaSize *exp,      /* Array of expected values */
		GammaSize *rcv,      /* Array of received (actual) values */
		ushort_t length)     /* # of RGB values to compare */
{
	ushort_t i;
	int err = 0;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "ADV473A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "ADV473B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "ADV473C");
		break;
	    default:
		sprintf(devicename, "ADV473?");
		break;
#endif	/* IP30 */
	}


	for(i=0; i<length; i++) {
	    if (exp[i].channel != rcv[i].channel ) {
		++err;
		msg_printf(ERR, "%s %s i=%d, exp= %x, rcv=%x \n",
		devicename, TestName,i,exp[i].channel, rcv[i].channel );
	    }
	    msg_printf(DBG, "%s %s i=%d, exp= %x, rcv=%x \n",
	    devicename, TestName, i,exp[i].channel, rcv[i].channel );
	}
	return (err);
}

/************************************************************************
 * mco_send473LUT will send out a 473 LUT and verify that it was 
 * written correctly. *
 * This function is currently limited in that it will set red,
 * green, and blue values to the same thing.
 ************************************************************************/

int 
mco_send473LUT(uchar_t device, /* Specifies MCO_ADV473A or MCO_ADV473B */
	    unsigned short pixels[256][3]) 
{
    int i;
    int flag = 0;
    __uint32_t readpix[256][3];

#ifdef NOTYET
    for (i = 0; i < COLORPALETTESIZE ; i++) {	/* Now send the LUT */
	msg_printf(VRB,"mco_send473LUT: R = 0x%x, G = 0x%x, B = 0x%x\n",
	(pixels[i][0] & 0xff), (pixels[i][1] & 0xff), (pixels[i][2] & 0xff));
    }
#endif /* NOTYET */

    /* Set 473 Address Register to Color Palette addr */
    MCO_ADV473_SETCPALWR(mgbase, device, 0x00);

    /* Set MCO Index Register to point to Color Palette RAM */
    MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CPRAM);

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

    for (i = 0; i < COLORPALETTESIZE ; i++) {	/* Now send the LUT */
	MCO_IO_WRITE(mgbase, (pixels[i][0] & 0xff)); /* Red */
	MCO_IO_WRITE(mgbase, (pixels[i][1] & 0xff)); /* Green */
	MCO_IO_WRITE(mgbase, (pixels[i][2] & 0xff)); /* Blue */

	if (i & 0x3) {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	}
    }

    /* Read back the Gamma Tables to a be sure. */

    /* Initialize readpix[][] array */
    for (i = 0; i < COLORPALETTESIZE ; i++) {
	readpix[i][0] = 0;
	readpix[i][1] = 0;
	readpix[i][2] = 0;
    }

    /* Set 473 Address Register to Color Palette addr */
    MCO_ADV473_SETPALRD(mgbase, device, 0x00);

    /* Set MCO Index Register to point to Color Palette RAM */
    MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CPRAM);

    mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

    for (i = 0; i < 256 ; i++) {
	MCO_IO_READ(mgbase, readpix[i][0]);     /* RED data */
	MCO_IO_READ(mgbase, readpix[i][1]);     /* GREEN data */
	MCO_IO_READ(mgbase, readpix[i][2]);     /* BLUE data */

	if (i & 0x3) {
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	}
    }

    for (i = 0; i < 256 ; i++) {
	if (readpix[i][0] != pixels[i][0]) {
	    msg_printf(ERR, "473 Red LUT Verify Failed. (Addr = %d, Wrote = %d, Read = %d)\n", 
		 i, pixels[i][0], readpix[i][0]);
	    flag = -1;
	}
         
	if (readpix[i][1] != pixels[i][1]) {
	    msg_printf(ERR, "473 Green LUT Verify Failed. (Addr = %d, Wrote = %d, Read = %d)\n", 
		 i, pixels[i][1], readpix[i][1]);
	    flag = -1;
	}
         
	if (readpix[i][2] != pixels[i][2]) {
	    msg_printf(ERR, "473 Blue LUT Verify Failed. (Addr = %d, Wrote = %d, Read = %d)\n", 
		 i, pixels[i][2], readpix[i][2]);
	    flag = -1;
	}
    }

#ifdef NOTYET
#endif /* NOTYET */

    return(flag);
}

/* 
 * Bt473 Probe routine - probes for 473 DAC and initializes it.
 */
int
mco_Dac473probe(uchar_t device) 
{
	uchar_t rcv=0xff;
	int probe_errs = 0;

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	/* Initialize the dac */
	MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CMD);  /* Set CMD Addr */
	MCO_IO_WRITE(mgbase, MCO_ADV473_CMD_INIT);

	/* Now read back the 473 CMD Register and verify it is okay */
	MCO_IO_READ(mgbase, rcv);

	if (rcv != MCO_ADV473_CMD_INIT) {
	    msg_printf(VRB, "473 Cmd Reg Write Failed. (%d != %d)\n",
	    rcv, MCO_ADV473_CMD_INIT);
	    probe_errs++;
	}

	return(probe_errs);
}

int
mco_Dac473Probe(int argc, char **argv)
{
	int errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
		msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
#else	/* IP30 */
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
#endif	/* IP30 */
		argv[0]);
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = mco_Dac473probe(device); 
	if (errors) {
		msg_printf(ERR,"MCO 473%c Probe Failed\n", argv[1][0]);
	}
	else {
		msg_printf(VRB,"MCO 473%c Probe Completed\n", argv[1][0]);
	}
	return (errors);
}

/* 
 * Bt473 Initialization routine - probes for 473 DAC and initializes it.
 */
int
_mco_init473(uchar_t device, ramDAC473Def *dacvals) 
{
	uchar_t rcv=0xff;
	int init_errs = 0;

	char dacname[10];

	switch(device) {
		case MCO_ADV473A:
		    sprintf(dacname,"473A");
		    break;
		case MCO_ADV473B:
		    sprintf(dacname,"473B");
		    break;
#ifdef IP30
		case MCO_ADV473C:
		    sprintf(dacname,"473C");
		    break;
#endif	/* IP30 */
	}

#ifdef NOTYET
	int i, j;

	msg_printf(VRB,"mco_init473: MCO 473 Addr Reg = 0x%x\n", 
	dacvals->addr);
	msg_printf(VRB,"mco_init473: MCO 473 Cmd Reg = 0x%x\n", 
	dacvals->cmd);
#endif /* NOTYET */
#ifdef NOTYET
	for (i=0; i<256; i++) {
	    msg_printf(VRB,"mco_init473: DAC 473 LUT[%d][0-2] = ", i);
	    for (j=0; j<3; j++) {
		msg_printf(VRB,"(%3d),\t", *(dacvals->lut+i+j));
	    }
	    msg_printf(VRB,"\n");
	}
#endif /* NOTYET */


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	/* Initialize the dac */
	msg_printf(VRB, "Initializing the %s DAC\n", dacname);
	MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_CMD);  /* Set CMD Addr */
	MCO_IO_WRITE(mgbase, dacvals->cmd);

	/* Now read back the 473 CMD Register and verify it is okay */
	MCO_IO_READ(mgbase, rcv);

	if (rcv != dacvals->cmd) {
	    msg_printf(VRB, "473 Cmd Reg Write Failed. (%d != %d)\n",
	    rcv, dacvals->cmd);
	    init_errs++;
	}

	/* Set PIXRDREG Addr */
	MCO_ADV473_SETADDR(mgbase, device, MCO_ADV473_PIXRDREG); 
	MCO_IO_WRITE(mgbase, 0xFF);

	init_errs = mco_send473LUT(device, dacvals->lut); /* XXX fix lut type */

	return(init_errs);
}

/* 
 * Bt473 RAM Address Register Test -- Writes and compares the values 0 - 0xFF
 * to the Bt473 RAM Address Register (uses RAM Write Mode Address Register
 * and RAM Read Mode Address Register)
 */
__uint32_t
_mco_Dac473RAMAddrRegTest(uchar_t device) 
{
	uchar_t data = 0;
	__uint32_t errors = 0;
	uchar_t rcv = 0xef;
	char errstring[60];
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s DAC RAM Address Register Test (Write Mode)\n", devicename);
	for(data=0; data <= COLORPALETTESIZE-1; data++) {

		/* Write data to Color Palette Write Mode Address Register */
		MCO_ADV473_SETCPALWR(mgbase, device, data);

		/* Read data from Color Palette Address Write Mode Register */
		MCO_SETINDEX(mgbase, device, MCO_ADV473_RAMWR);
		MCO_IO_READ(mgbase, rcv);
		if (rcv != data) {
		    msg_printf(VRB, "ADV473%s RAM Addr Reg Test ERROR: exp = 0x%x, actual = 0x%x\n",
		    devicename, data, rcv);
		    msg_printf(VRB, "ADV473%s RAM Addr Reg Test: Let's try another way...\n", devicename);
		    /* Set the RAM Rd Address Register */
		    MCO_SETINDEX(mgbase, device, MCO_ADV473_RAMRD);
		    MCO_IO_WRITE(mgbase, data);

		    MCO_IO_READ(mgbase, rcv);
		    if (rcv != data) {
			msg_printf(VRB, "ADV473%s RAM Addr Reg Test ERROR: exp = 0x%x, actual = 0x%x\n",
			devicename, data, rcv);
		    }
		}


		sprintf(errstring, "ADV473%s Dac RAM Addr Reg Test", devicename);
		COMPARE(errstring, 0, data, rcv, 0xff, errors);
	}
 
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_ADDR_REG_TEST]), errors);
}

__uint32_t
mco_Dac473RAMAddrRegTest(int argc, char **argv) 
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
		msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
#else	/* IP30 */
		msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
#endif	/* IP30 */
		argv[0]);
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473RAMAddrRegTest(device);
	return (errors);
}


/* 
 * Bt473 Overlay Address Register Test -- Writes and compares the values 
 * 0 - 0xFF to the Bt473 Overlay Address Register (uses Overlay Write Mode 
 * Address Register and Overlay Read Mode Address Register)
 */
__uint32_t
_mco_Dac473OverlayAddrRegTest(uchar_t device) 
{
	uchar_t data = 0;
	__uint32_t errors = 0;
	uchar_t rcv = 0xef;
	char errstring[60];
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473 DAC Overlay Address Register Test (Write Mode)\n");
	sprintf(errstring, "ADV473%s Dac Overlay Addr Reg Test", devicename);
	for(data=0; data < OVERLAYSIZE; data++) {

		/* Write data to Color Palette Write Mode Address Register */
		MCO_ADV473_SETOVRLAYWR(mgbase, device, data);

		/* Read data from Color Palette Address Write Mode Register */
		MCO_IO_READ(mgbase, rcv);

		COMPARE(errstring, 0, data, rcv, 0xff, errors);
	}
 
	msg_printf(VRB, "ADV473%s DAC Overlay Address Register Test (Read Mode)\n", devicename);

	for(data=0; data < OVERLAYSIZE; data++) {

		/* Write data to Color Palette Read Mode Address Register */
		MCO_ADV473_SETOVRLAYRD(mgbase, device, data);

		/* Read data from Color Palette Read Mode Address Register */
		MCO_IO_READ(mgbase, rcv);

		COMPARE(errstring, 0, data, rcv, 0xff, errors);
	}
 
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_ADDR_REG_TEST]), errors);
}

__uint32_t
mco_Dac473OverlayAddrRegTest(int argc, char **argv) 
{
	__uint32_t errors = 0;
	char devicename[40];

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
	    argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
	    argv[0]);
#endif	/* IP30 */
		return -1;
	}

	msg_printf(DBG, "argv[1] = %s\n", argv[1]);
	switch (argv[1][0]) {
	    case 'A':
#ifdef NOTYET
		device =  MCO_ADV473A;
#endif
		sprintf(devicename, "A");
		break;
	    case 'B':
#ifdef NOTYET
		device =  MCO_ADV473B;
#endif
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case 'C':
#ifdef NOTYET
		device =  MCO_ADV473C;
#endif
		sprintf(devicename, "C");
		break;
#endif	/* IP30 */
	    default:
#ifdef NOTYET
		device =  MCO_ADV473A;
#endif
		sprintf(devicename, "A");
		break;
	}
#ifdef NOTYET
	errors = _mco_Dac473OverlayAddrRegTest(device);
#endif /* NOTYET */
	msg_printf(VRB,"MCO 473 DAC Overlay Address Register Test not available\n");
	return (errors);
}


/* 
 * Bt473 Color Palette Address Uniqueness Test -- Writes and compares unique 
 * values to the Bt473 Color Palette RAM 
 */
__uint32_t
_mco_Dac473ClrPaletteAddrUniqTest(uchar_t device) 
{
	ushort_t i;
	__uint32_t errors = 0;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Color Palette Address Uniquness Test\n", devicename);

	for (i = 0; i < COLORPALETTESIZE ; i++){
		Red[i].channel = i; Green[i].channel = i; Blue[i].channel = i;
	}
	
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );

	mco_WriteDac473(device, COLORPALETTE, 0x00,  Red,    Green,    Blue,    COLORPALETTESIZE);
	mco_ReadDac473(device, COLORPALETTE, 0x00,   RedRcv, GreenRcv, BlueRcv, COLORPALETTESIZE);

	errors  = mco_Dac473Compare("Red Palette Addr Uniq Test", device, Red,   RedRcv, COLORPALETTESIZE);
	errors += mco_Dac473Compare("Grn Palette Addr Uniq Test", device, Green, GreenRcv, COLORPALETTESIZE);
	errors += mco_Dac473Compare("Blu Palette Addr Uniq Test", device, Blue,  BlueRcv, COLORPALETTESIZE);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_CLR_PLLT_ADDR_UNIQ_TEST]), errors);
	
}

__uint32_t
mco_Dac473ClrPaletteAddrUniqTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s [A|B|C])\n", argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s [A|B])\n", argv[0]);
#endif	/* IP30 */
		return -1;
	}

	msg_printf(DBG, "argv[1] = %s\n", argv[1]);
	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473ClrPaletteAddrUniqTest(device); 
	return (errors);
}

__uint32_t 
_mco_Dac473ClrPaletteWalkBitTest(uchar_t device) 
{
	 ushort_t i;
	 __uint32_t errors = 0;
	 ushort_t length = COLORPALETTESIZE;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Color Palette Walking Bit: DataBus Test\n", devicename);
	for (i = 0; i < length ; i++){
		Red[i].channel = Green[i].channel = Blue[i].channel = dac_walking_patrn[i%16];
	}

	bzero(RedRcv,   sizeof(RedRcv) );
        bzero(BlueRcv,  sizeof(BlueRcv) );
        bzero(GreenRcv, sizeof(GreenRcv) );

	mco_WriteDac473(device, COLORPALETTE, 0x00, Red, Green, Blue, length);
	mco_ReadDac473(device, COLORPALETTE, 0x00, RedRcv, GreenRcv, BlueRcv, length);

	errors  = mco_Dac473Compare("Red Palette Data WalkBit Test", device, Red,   RedRcv,   length);
	errors += mco_Dac473Compare("Grn Palette Data WalkBit Test", device, Green, GreenRcv, length);
	errors += mco_Dac473Compare("Blu Palette Data WalkBit Test", device, Blue,  BlueRcv,  length);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_CLRPAL_WALK_BIT_TEST]), errors);
}

__uint32_t
mco_Dac473ClrPaletteWalkBitTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
	    argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
	    argv[0]);
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473ClrPaletteWalkBitTest(device); 
	return (errors);
}

__uint32_t 
_mco_Dac473ClrPalettePatrnTest(uchar_t device) 
{
	 ushort_t i;
	 ushort_t loop;
	 __uint32_t errors;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}

	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Color Palette Pattern Test\n", devicename);

	for(loop=0; loop<7; loop++) {
	    for (i = 0; i < COLORPALETTESIZE ; i++){
		Red[i].channel = dac_patrn[loop + (i%7)]  ;
		Green[i].channel = dac_patrn[loop + (i%7)]  ;
		Blue[i].channel = dac_patrn[loop + (i%7)]  ;
	    }

	    bzero(RedRcv,   sizeof(RedRcv) );
	    bzero(BlueRcv,  sizeof(BlueRcv) );
	    bzero(GreenRcv, sizeof(GreenRcv) );
	
	    mco_WriteDac473(device, COLORPALETTE, 0x00, Red, Green, Blue, COLORPALETTESIZE);
	    mco_ReadDac473(device, COLORPALETTE, 0x00, RedRcv, GreenRcv, BlueRcv, COLORPALETTESIZE);
	
	    errors  = mco_Dac473Compare("Red Palette Patrn Test", device, Red,   RedRcv,   COLORPALETTESIZE);
	    errors += mco_Dac473Compare("Grn Palette Patrn Test", device, Green, GreenRcv, COLORPALETTESIZE);
	    errors += mco_Dac473Compare("Blu Palette Patrn Test", device, Blue,  BlueRcv,  COLORPALETTESIZE);
	}

	
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_CLR_PLLT_PATRN_TEST]), errors);
}

__uint32_t
mco_Dac473ClrPalettePatrnTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
	    argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
	    argv[0]);
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473ClrPalettePatrnTest(device); 
	return (errors);
}

/* 
 * Bt473 Overlay Palette Address Uniqueness Test -- Writes and compares unique 
 * values to the Bt473 Overlay Palette RAM 
 */
__uint32_t
_mco_Dac473OvrlayPaletteAddrUniqTest(uchar_t device) 
{
	ushort_t i;
	__uint32_t errors = 0;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Overlay Palette Address Uniquness Test\n", devicename);

	for (i = 0; i < OVERLAYSIZE ; i++){
		Red[i].channel = i; Green[i].channel = i; Blue[i].channel = i;
	}
	
	bzero(RedRcv,   sizeof(RedRcv) );
	bzero(BlueRcv,  sizeof(BlueRcv) );
	bzero(GreenRcv, sizeof(GreenRcv) );

	mco_WriteDac473(device, OVERLAY, 0x00,  Red,    Green,    Blue,    OVERLAYSIZE);
	mco_ReadDac473(device, OVERLAY, 0x00,   RedRcv, GreenRcv, BlueRcv, OVERLAYSIZE);

	errors  = mco_Dac473Compare("Red Palette Addr Uniq Test", device, Red,   RedRcv, OVERLAYSIZE);
	errors += mco_Dac473Compare("Grn Palette Addr Uniq Test", device, Green, GreenRcv, OVERLAYSIZE);
	errors += mco_Dac473Compare("Blu Palette Addr Uniq Test", device, Blue,  BlueRcv, OVERLAYSIZE);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_OVRLY_PLLT_ADDR_UNIQ_TEST]), errors);
	
}

__uint32_t
mco_Dac473OvrlayPaletteAddrUniqTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
		msg_printf(SUM, "usage %s [A|B])\n",
		argv[0]);
#ifdef IP30
	    msg_printf(SUM, "usage %s [A|B|C])\n", argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s [A|B])\n", argv[0]);
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473OvrlayPaletteAddrUniqTest(device); 
	return (errors);
}

__uint32_t 
_mco_Dac473OvrlayPaletteWalkBitTest(uchar_t device) 
{
	ushort_t i;
	__uint32_t errors = 0;
	ushort_t length = OVERLAYSIZE;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Overlay Palette Walking Bit: DataBus Test\n", devicename);
	for (i = 0; i < length ; i++){
		Red[i].channel = Green[i].channel = Blue[i].channel = dac_walking_patrn[i%16];
	}

	bzero(RedRcv,   sizeof(RedRcv) );
        bzero(BlueRcv,  sizeof(BlueRcv) );
        bzero(GreenRcv, sizeof(GreenRcv) );

	mco_WriteDac473(device, OVERLAY, 0x00, Red, Green, Blue, length);
	mco_ReadDac473(device, OVERLAY, 0x00, RedRcv, GreenRcv, BlueRcv, length);

	errors  = mco_Dac473Compare("Red Palette Data WalkBit Test", device, Red,   RedRcv,   length);
	errors += mco_Dac473Compare("Grn Palette Data WalkBit Test", device, Green, GreenRcv, length);
	errors += mco_Dac473Compare("Blu Palette Data WalkBit Test", device, Blue,  BlueRcv,  length);

	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_OVRLY_PLLT_WALK_BIT_TEST]), errors);
}

__uint32_t
mco_Dac473OvrlayPaletteWalkBitTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
	    argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
	    argv[0]);
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473OvrlayPaletteWalkBitTest(device); 
	return (errors);
}

__uint32_t 
_mco_Dac473OvrlayPalettePatrnTest(uchar_t device) 
{
	ushort_t i;
	ushort_t loop;
	__uint32_t errors;
	char devicename[40];

	switch (device) {
	    case MCO_ADV473A:
		sprintf(devicename, "A");
		break;
	    case MCO_ADV473B:
		sprintf(devicename, "B");
		break;
#ifdef IP30
	    case MCO_ADV473C:
		sprintf(devicename, "C");
		break;
	    default:
		sprintf(devicename, "?");
		break;
#endif	/* IP30 */
	}


	/* Set DCBCTRL correctly for MCO CRS(2) and CRS(3) addresses */
	mco_set_dcbctrl(mgbase, 2);

	msg_printf(VRB, "ADV473%s Overlay Pallette Pattern Test\n", devicename);
	for(loop=0; loop<7; loop++) {
	    for (i = 0; i < OVERLAYSIZE ; i++){
		Red[i].channel = dac_patrn[loop + (i%7)]  ;
		Green[i].channel = dac_patrn[loop + (i%7)]  ;
		Blue[i].channel = dac_patrn[loop + (i%7)]  ;
	    }

	    bzero(RedRcv,   sizeof(RedRcv) );
	    bzero(BlueRcv,  sizeof(BlueRcv) );
	    bzero(GreenRcv, sizeof(GreenRcv) );
	
	    mco_WriteDac473(device, OVERLAY, 0x00, Red, Green, Blue, OVERLAYSIZE);
	    mco_ReadDac473(device, OVERLAY, 0x00, RedRcv, GreenRcv, BlueRcv, OVERLAYSIZE);
	
	    errors  = mco_Dac473Compare("Red Palette Patrn Test", device, Red,   RedRcv,   OVERLAYSIZE);
	    errors += mco_Dac473Compare("Grn Palette Patrn Test", device, Green, GreenRcv, OVERLAYSIZE);
	    errors += mco_Dac473Compare("Blu Palette Patrn Test", device, Blue,  BlueRcv,  OVERLAYSIZE);
	}

	
	REPORT_PASS_OR_FAIL((&MCOBackEnd[MCO_473_OVRLY_PLLT_PATRN_TEST]), errors);
}

__uint32_t
mco_Dac473OvrlayPalettePatrnTest(int argc, char **argv)
{
	__uint32_t errors = 0;
	uchar_t device = MCO_ADV473A;

	if (argc != 2) {
#ifdef IP30
	    msg_printf(SUM, "usage %s Device (where Device is A, B, or C)\n",
	    argv[0]);
#else	/* IP30 */
	    msg_printf(SUM, "usage %s Device (where Device is A or B)\n",
	    argv[0]);
#endif	/* IP30 */
		return -1;
	}

	switch (argv[1][0]) {
	    case 'A':
		device =  MCO_ADV473A;
		break;
	    case 'B':
		device =  MCO_ADV473B;
		break;
#ifdef IP30
	    case 'C':
		device =  MCO_ADV473C;
		break;
#endif	/* IP30 */
	    default:
		device =  MCO_ADV473A;
		break;
	}
	errors = _mco_Dac473OvrlayPalettePatrnTest(device); 
	return (errors);
}

