#ident	"IP22diags/video/VINO/vino_i2ctest.c:  $Revision: 1.9 $"

#include "sys/types.h"
#include <libsk.h>
#include "uif.h"

#include "./vino_diags.h"
#include "./vino.h"

extern	int		vino_probe();
extern	int		vinoI2cReadStatus();
extern	int		vinoI2cWriteBlock();
extern	int		vinoI2cWriteReg();
extern	int		vinoI2cReadReg();
extern	int		i2cForceBusIdle();

extern	uchar		board_initialized;
extern	uchar		mc_initialized;
extern	uchar		CDMC_initialized;
extern	uchar		DMSD_initialized;
extern	uchar		DMSD_no_sources;

extern	ulong		CDMC_addr;  /* 0x56 if asic, 0xae otherwise */

extern	ulong		vino_test_vec[];

extern	VinoBoard	vino_boards[];
extern	VinoRegs	regs_base;
extern	caddr_t		phys_base;

uchar	vino_pal_input = NO_SOURCE;
uchar	vino_ntsc_input = NO_SOURCE;

int vino_camera_init(void);

/*
 *  Table of default register values to initialize the 7191 DMSD.
 *
 *  The values are single byte quantities.  A END_BLOCK is used to
 *  terminate the default values list.
 */

int DMSD_defaults [] = {

    0x50,	/* 0x00 dmsdRegIDEL 	*/
    0x30,	/* 0x01 dmsdRegHSYb50	*/
    0x00,	/* 0x02 dmsdRegHSYe50	*/
    0xE8,	/* 0x03 dmsdRegHCLb50	*/
    0xB6,	/* 0x04 dmsdRegHCLe50	*/
    0xF4,	/* 0x05 dmsdRegHSP50	*/
    0x01,	/* 0x06 dmsdRegLUMA	*/
    0x00,	/* 0x07 dmsdRegHUE	*/
    0xF8,	/* 0x08 dmsdRegCKqam	*/
    0xF8,	/* 0x09 dmsdRegCKsecam	*/
    0x90,	/* 0x0A dmsdRegSENpal	*/
    0x90,	/* 0x0B dmsdRegSENsecam	*/
    0x00,	/* 0x0C dmsdRegGC0	*/
    0x80,	/* 0x0D dmsdRegSTDmode	*/
    0x78,	/* 0x0E dmsdRegIOclk	*/
    0x99,	/* 0x0F dmsdRegCTRL3	*/
    0x00,	/* 0x10 dmsdRegCTRL4	*/
    0x2c,	/* 0x11 dmsdRegCHCV	*/
    0x00,	/* 0x12 not used	*/
    0x00,	/* 0x13 not used	*/
    0x34,	/* 0x14 dmsdRegHSYb60	*/
    0x0A,	/* 0x15 dmsdRegHSYe60	*/
    0xF4,	/* 0x16 dmsdRegHCLb60	*/
    0xCE,	/* 0x17 dmsdRegHCLe60	*/
    0xF4,	/* 0x18 dmsdRegHSP60	*/

    END_BLOCK,	/* termination value	*/
};


/*
 *  Table of register default values used to initialize the Camera.
 *
 *  The values are single byte quantities.  A END_BLOCK is used to
 *  terminate the default values list.  A SKIP_RO_REG is used to
 *  skip any Read Only registers.
 */

int CDMC_defaults [] = {

    0x01,	/* 0x00 CameraControlStatus	*/
    0x00,	/* 0x01 CameraShutterSpeed	*/
    0x80,	/* 0x02 CameraGain		*/

    SKIP_RO_REG, /* 0x03 CameraBrightness	*/

    0x80,	/* 0x04 CameraRedBalance	*/
    0x80,	/* 0x05 CameraBlueBalance	*/
    0x80,	/* 0x06 CameraRedSaturation	*/
    0x80,	/* 0x07 CameraBlueSaturation	*/

    END_BLOCK,	/* termination value		*/
};


/*
 * vino_7191_inputs() - switch between the composite and
 * s-video inputs.
 */

int
vino_7191_inputs(uchar which)

{
    int	   val;
    int	    delay_loop;
    
    val = vinoI2cReadReg(vino_boards, DMSD_ADDR, DMSD_REG_LUMA);
    
    if ((val & (1<<DMSD_BYPS_SHIFT)) == which)
	return 0;

    val = val & DMSD_BYPS_MASK;	/* clear D7 */
    
    if (which == SVIDEO)
	val = val | which;
     
    msg_printf(DBG, "vino_7191_inputs : subaddress DMSD_REG_LUMA to 0x%x\n", val);
    
    if (vinoI2cWriteReg(vino_boards, DMSD_ADDR, DMSD_REG_LUMA, val) == FAILURE) {
	msg_printf(ERR, "vino_7191_inputs : Cannot toggle DMSD at subaddr DMSD_REG_LUMA\n");
	return 1;
    }
    
    /*
     * toggle D0 and D1 and D2 of DMSD_REG_IOCK
     * 
     * set D0 clear D1 = s-video
     * set D2 - get chroma from chroma input CHR7 to CHR0
     */
    val = vinoI2cReadReg(vino_boards, DMSD_ADDR, DMSD_REG_IOCK);
    
    val = val & DMSD_CHRS_MASK;
    val = val & DMSD_GPSW_MASK;
    
    if (which == SVIDEO)
	val = val | DMSD_CHRS_SVID | DMSD_GPSW_SVID;
     
    msg_printf(DBG, "vino_7191_inputs : subaddress DMSD_REG_IOCK to 0x%x\n", val);

    if (vinoI2cWriteReg(vino_boards, DMSD_ADDR, DMSD_REG_IOCK, val) == FAILURE)
	msg_printf(ERR, "vino_7191_inputs : Cannot toggle DMSD at subaddr DMSD_REG_IOCK\n");

    for (delay_loop = 0; delay_loop<MAX_DELAY_LOOP; delay_loop++)
	us_delay(TIME_1_SECOND);
    
    return 0;
}


void
vino_inputs(int argc, char **argv)

{
    if (argc < 2) {
	msg_printf(ERR, "vino_inputs : USAGE vino_inputs c | s\n");
	return;
    }
    if (argv[1][0] == 's') {
	msg_printf(DBG, "vino_inputs %c\n", argv[1][0]);
	vino_7191_inputs(SVIDEO);
    }
    else if (argv[1][0] == 'c') {
	msg_printf(DBG, "vino_inputs %c\n", argv[1][0]);
	vino_7191_inputs(COMPOSITE);
    }
    else
	msg_printf(DBG, "vino_inputs - bad arg %c\n", argv[1][0]);
}


/*
 * vino_status confirmation
 */

int
vino_status_unstable(ulong org_status)

{
    ulong   cur_status;
    int	    loop;
    int	    delay_loop;
    
    for (loop = 0; loop<3; loop++) {
	cur_status = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
	if (cur_status == FAILURE) {
	    msg_printf(ERR, "vino_status_unstable : Cannot read staus register of 7191\n");
	    return 1;		/* return not stable */
	}
	
	if (cur_status == org_status)
	    us_delay(TIME_1_SECOND);
	else {			/* let 7191 settle down */
	    for (delay_loop = 0; delay_loop<MAX_DELAY_LOOP; delay_loop++)
		us_delay(TIME_1_SECOND);
	    return 1;		/* return not stable */
	}
    }
    
    return 0;	/* status is stable */
}


/*
 * Initialize the Philips 7191 DMSD-SQP Decoder to
 * a known state. See Philip's 1992 Desktop Video
 * Data Handbook p 3-169 for values and an explanation.
 * 
 * 07.20.93 - added a probe to find the jack with an
 * ntsc signal and the jack with a pal signal.
 */

int
vino_7191_init()

{
    ulong   statusReg;
    int	    errcount = 0;
    int	    DMSD_defaults_start = 0x0;
    
    if (board_initialized == FALSE) {
	errcount += vino_probe();
    }
    
    vino_pal_input = NO_SOURCE;
    vino_ntsc_input = NO_SOURCE;

    if (vinoI2cWriteBlock(vino_boards, DMSD_ADDR,
	    DMSD_defaults_start, DMSD_defaults) == FAILURE) {
	errcount++;
	msg_printf(ERR, "vino_7191_init : Cannot initialize 7191\n");
    }
    else
	DMSD_initialized = TRUE;
    
    /*
     * Check the composite jack to see what kind of signal
     * (NTSC or PAL) is coming through if any. This allows
     * user flexibility.
     */
    vino_7191_inputs(COMPOSITE);
    statusReg = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
    if (statusReg == FAILURE) {
	msg_printf(ERR, "vino_7191_init : Cannot read staus register of 7191\n");
	return 0;
    }
    else {
	if (vino_status_unstable(statusReg)) {
	    statusReg = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
	    if (statusReg == FAILURE) {
		msg_printf(ERR, "vino_7191_init : Cannot read staus register of 7191\n");
		return 0;
	    }
	}
	
	if (statusReg&(1<<6)) {
	    msg_printf(INFO, "vino_7191_init : DMSD detecting HPLL unlocked\n");
	    msg_printf(INFO, "vino_7191_init : need to connect video source to COMPOSITE input\n");
	}
	else {
	    msg_printf(DBG, "vino_7191_init : DMSD detecting HPLL locked\n");
	    if (statusReg&(1<<5))
		vino_ntsc_input = COMPOSITE;
	    else
		vino_pal_input = COMPOSITE;
	}
    }

    vino_7191_inputs(SVIDEO);
    statusReg = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
    if (statusReg == FAILURE) {
	msg_printf(ERR, "vino_7191_init : Cannot read staus register of 7191\n");
	return 0;
    }
    else {
	if (vino_status_unstable(statusReg)) {
	    statusReg = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
	    if (statusReg == FAILURE) {
		msg_printf(ERR, "vino_7191_init : Cannot read staus register of 7191\n");
		return 0;
	    }
	}

	if (statusReg&(1<<6)) {
	    msg_printf(INFO, "vino_7191_init : DMSD detecting HPLL unlocked\n");
	    msg_printf(INFO, "vino_7191_init : need to connect video source to SVIDEO input\n");
	}
	else {
	    msg_printf(DBG, "vino_7191_init : DMSD detecting HPLL locked\n");
	    if (statusReg&(1<<5))
		vino_ntsc_input = SVIDEO;
	    else
		vino_pal_input = SVIDEO;
	}
    }

    /*
     * Sanity check time.
     */
    if ((vino_ntsc_input == NO_SOURCE) || (vino_pal_input == NO_SOURCE)) {
	if (vino_ntsc_input == NO_SOURCE)
	    msg_printf(ERR, "vino_7191_init : need a NTSC source connected to video a input\n");
	else
	    msg_printf(ERR, "vino_7191_init : need a PAL source connected to video a input\n");
	
	DMSD_no_sources = TRUE;
	errcount++;
    }
    else {
	msg_printf(DBG, "vino_7191_init : source NTSC = %d\tsource PAL = %d\n", vino_ntsc_input,
		    vino_pal_input);
	DMSD_no_sources = FALSE;
    }
    
    if (errcount != 0)
	sum_error ("VINO I2C 7191 Init");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C 7191 Initialization test for VINO
     * 
     */    
}

/*
 * Print value of the 7191 status register
 */

int
vino_7191_status()

{
    int	    errcount = 0;
    int	    statusReg = 0;
    
    if (DMSD_initialized == FALSE) {
	errcount += vino_7191_init();
    }
    
    statusReg = vinoI2cReadStatus(vino_boards, DMSD_ADDR);
    if (statusReg == FAILURE) {
	errcount++;
	msg_printf(ERR, "i2c_7191_status Cannot read staus register of 7191\n");
    }
    else {
	msg_printf(INFO, "i2c_7191_status Status Register of 7191 is 0x%x\n", statusReg);
	if (statusReg&(1<<7))
	    msg_printf(INFO, "DMSD detecting VCR time constant\n");
	else
	    msg_printf(INFO, "DMSD detecting TV time constant\n");
	    
	if (statusReg&(1<<6))
	    msg_printf(INFO, "DMSD detecting HPLL unlocked\n");
	else
	    msg_printf(INFO, "DMSD detecting HPLL locked\n");
	    
	if (statusReg&(1<<5))
	    msg_printf(INFO, "DMSD detecting 60Hz system\n");
	else
	    msg_printf(INFO, "DMSD detecting 50Hz system\n");
	    
	if (statusReg&1)
	    msg_printf(INFO, "DMSD detecting color\n");
	else
	    msg_printf(INFO, "DMSD detecting no color\n");
    }

    if (errcount != 0)
	sum_error ("VINO I2C 7191 Status Register");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C 7191 Status Register test for VINO
     * 
     */    
}

/*
 * Check to see that a video signal is coming from the 7191
 * by checking that the field counter register increments
 */

int
vino_7191_signal()

{
    int	    errcount = 0;
    int	    wait_count;
    ulong   field_counter_s;

    if (DMSD_initialized == FALSE) {
	errcount += vino_7191_init();
    }
    
    /*
     * Set up VINO for 7191
     */
    VINO_REG_WRITE(regs_base, control, 0x0);
    VINO_REG_WRITE(regs_base, a.clip_start, 0x80400);	/* X=0 Ye=1 Yo=1 */
    VINO_REG_WRITE(regs_base, a.clip_end, 0x783c27f);	/* X=639 Ye=240 Yo=240 */

    /*
     * grab the value of field_counter, wait a bit, and
     * grab the value again. make sure that the second
     * value is bigger than the first.
     */
    field_counter_s = VINO_REG_READ(regs_base, a.field_counter);
    for (wait_count=100; wait_count; wait_count--)
	/* do nothing */;
    if (VINO_REG_READ(regs_base, a.field_counter) <= field_counter_s) {
	errcount++;
	msg_printf(ERR, "i2c_7191_signal ChA field_counter not incrementing\n");
    }

    if (errcount != 0)
	sum_error ("VINO I2C 7191 Signal");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C 7191 Video Signal test for VINO
     * 
     */    
}

/*
 * Toggle the GPSW1 and GPSW2 bits to enable/disable pins
 * 25 and 24 (respectively). This was used in conjunction
 * with a logic analyzer to verify that data was being
 * sent across the I2C bus correctly.
 */

int
vino_7191_switch()

{
    int	    errcount = 0;
    ulong   val;
    
    if (DMSD_initialized == FALSE) {
	errcount += vino_7191_init();
    }
    
    val = vinoI2cReadReg(vino_boards, DMSD_ADDR, DMSD_REG_IOCK);
    
    if (val == FAILURE) {
	msg_printf(ERR, "Cannot read DMSD at DMSD_REG_IOCK\n");
	errcount++;
	i2cForceBusIdle(regs_base);
    }
    
    val ^= 0x3;
    msg_printf(VRB, "toggled DMSD_REG_IOCK of DMSD to 0x%x\n", val);
    if (vinoI2cWriteReg(vino_boards, DMSD_ADDR, DMSD_REG_IOCK, val) == FAILURE) {
	i2cForceBusIdle(regs_base);
	msg_printf(ERR, "Cannot toggle DMSD at subaddr DMSD_REG_IOCK\n");
	errcount++;
    }

    if (errcount != 0)
	sum_error ("VINO I2C 7191 Switch");
    else
	okydoky ();

    return (errcount);
}


/*
 * Given a sub address of the camera and the range of bits to
 * test, this function does a walking one's test. The return
 * value is the bit(s) at which the test failed.
 */

ulong
reg_test_camera(int start, int stop, ulong addr)

{
    int	    loop= 0L;
    ulong   result = 0L;
    ulong   val;
    ulong   mask = 0L;
    
    if (board_initialized == FALSE) {
	msg_printf(ERR, "reg_test_camera : vino_boards not initialized\n");
	return 1;
    }
    msg_printf(DBG, "in reg_test_camera\n");
    for (loop=start; loop<=stop; loop++)
	mask |= 1<<loop;

    if (vinoI2cWriteReg(vino_boards, CDMC_addr, addr, 0x0) == FAILURE) {
	msg_printf(ERR, "Cannot write 0x0 to 0x%x at 0x%x\n", CDMC_addr, addr);
	msg_printf(ERR, "reg_test_camera : write FAILURE zero ctrl 0x%x\n", VINO_REG_READ(regs_base, i2c_control));
	i2cForceBusIdle(regs_base);
	return 0x1;
    }

    val = vinoI2cReadReg(vino_boards, CDMC_addr, addr);
    if ((val&mask) != 0x0 || val == FAILURE) {
	if (val == FAILURE) {
	    msg_printf(ERR, "i2c FAILURE - read zero ctrl 0x%x\n", VINO_REG_READ(regs_base, i2c_control));
	    i2cForceBusIdle(regs_base);
	}
	msg_printf(ERR, "Cannot read 0x0 from 0x%x (%x)\n", addr, val&mask);
	return 0x1;
    }
    
    for (loop=start; loop<=stop; loop++) {
	if (vinoI2cWriteReg(vino_boards, CDMC_addr, addr, vino_test_vec[loop]) == FAILURE){
	    msg_printf(ERR, "reg_test_camera : write FAILURE loop (%d) ctrl 0x%x\n", loop, VINO_REG_READ(regs_base, i2c_control));
	    result |= 0x1<<loop;
	    i2cForceBusIdle(regs_base);
	    continue;
	}

	val = vinoI2cReadReg(vino_boards, CDMC_addr, addr);
	if ((val&mask) != vino_test_vec[loop] || val == FAILURE) {
	    if (val == FAILURE) {
		msg_printf(ERR, "i2c FAILURE - read (%d) ctrl 0x%x\n",
			    loop, VINO_REG_READ(regs_base, i2c_control));
		i2cForceBusIdle(regs_base);
	    }
	    result |= 0x1<<loop;
	}
    }
    return result;
}


/*
 * Do a walking one's test of the camera's registers. Make
 * sure that AGC is off when testing the gain register. If
 * AGC is on,  the value in the gain register has no meaning.
 */
int
vino_camera()

{
    int	    errcount=0;
    ulong   err_val;
    
    if (CDMC_initialized == FALSE)
	errcount = vino_camera_init();

    /*
     * Do not test the Control/Status register
     */
    
    
    err_val = reg_test_camera(0, 2, CDMC_REG_SHUTTER_SPEED);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_SHUTTER_SPEED failed 0x%x\n", err_val);
	errcount++;
    }

    if (vinoI2cWriteReg(vino_boards, CDMC_addr, CDMC_REG_CTRL_STATUS, 0x0) == FAILURE){
	msg_printf(ERR, "camera : could not set CDMC_REG_CTRL_STATUS to 0x0\n");
	i2cForceBusIdle(regs_base);
    }
    err_val = reg_test_camera(0, 7, CDMC_REG_GAIN);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_GAIN failed 0x%x\n", err_val);
	errcount++;
    }

    err_val = reg_test_camera(0, 7, CDMC_REG_RED_BAL);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_RED_BAL failed 0x%x\n", err_val);
	errcount++;
    }
    
    err_val = reg_test_camera(0, 7, CDMC_REG_BLUE_BAL);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_BLUE_BAL failed 0x%x\n", err_val);
	errcount++;
    }
    
    err_val = reg_test_camera(0, 7, CDMC_REG_RED_SAT);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_RED_SAT failed 0x%x\n", err_val);
	errcount++;
    }
    
    err_val = reg_test_camera(0, 7, CDMC_REG_BLUE_SAT);
    if (err_val) {
	msg_printf(ERR, "CDMC_REG_BLUE_SAT failed 0x%x\n", err_val);
	errcount++;
    }


    if (errcount != 0)
	sum_error ("VINO I2C Camera");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C Camera test
     * 
     */
}


/*
 * Reset the camera by writing to addr CDMC_REG_MASTER_RESET (0xF).
 * Verify that registers reset to correct values.
 */

int
vino_camera_reset()

{
    int	    errcount=0;
    int	    reg_val;
    
    if (board_initialized == FALSE) {
	errcount = vino_probe();
    }
    
    /* any write to the reset register 0xF, causes the camera to reset */
    if (vinoI2cWriteReg(vino_boards, CDMC_addr, CDMC_REG_MASTER_RESET, 0x0) == FAILURE) {
	i2cForceBusIdle(regs_base);
	errcount++;
	msg_printf(ERR, "i2c err writing to CDMC\n");
    }

    /*
     * Bit 0 resets to 1 and bit 1 resets to 0. Bit 7
     * will vary according to whether the current field
     * is even or odd.
     */
    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_CTRL_STATUS);
    reg_val = reg_val & 0x3;
    if (!(reg_val & 0x1) || (reg_val & 0x2) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_CTRL_STATUS -> 0x%x NOT RESETING TO 0x1\n",
		reg_val);
	i2cForceBusIdle(regs_base);
    }


    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_SHUTTER_SPEED);
    if ((reg_val != 0x0) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_SHUTTER_SPEED -> 0x%x NOT RESETING TO 0x0\n",
		reg_val);
	i2cForceBusIdle(regs_base);
    }
    
    
    /*
     * Gain register does not reset
     */
    
    
    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_RED_BAL);
    if ((reg_val != 0x80) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_RED_BAL -> 0x%x NOT RESETING TO 0x80\n",
		reg_val);
	i2cForceBusIdle(regs_base);
    }
    

    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_BLUE_BAL);
    if ((reg_val != 0x80) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_BLUE_BAL -> 0x%x NOT RESETING TO 0x80\n",
		reg_val);
	i2cForceBusIdle(regs_base);
    }
    

    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_RED_SAT);
    if ((reg_val != 0x80) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_RED_SAT -> 0x%x NOT RESETING TO 0x80\n",
		reg_val);
 	i2cForceBusIdle(regs_base);
   }
    

    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_BLUE_SAT);
    if ((reg_val != 0x80) || (reg_val == FAILURE)) {
	errcount++;
	msg_printf(ERR, "CDMC_REG_BLUE_SAT -> 0x%x NOT RESETING TO 0x80\n",
		reg_val);
	i2cForceBusIdle(regs_base);
    }
    
    if (errcount != 0)
	sum_error ("VINO I2C Camera Reset");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C Camera Reset test
     * 
     */
}

/*
 * Print out the status of the CDMC_REG_CTRL_STATUS register
 */

int
vino_camera_status()

{
    int	    errcount = 0;
    ulong   statusReg;
    
    if (CDMC_initialized == FALSE)
	errcount += vino_camera_init();

    statusReg = vinoI2cReadStatus(vino_boards, CDMC_addr);
    if (statusReg == FAILURE) {
	errcount++;
	msg_printf(ERR, "CDMC_status Cannot read staus register of CDMC 0x%x\n", CDMC_addr);
    }
    else
	msg_printf(INFO, "status 0x%x\n", statusReg);
    
    if (errcount != 0)
	sum_error ("VINO I2C Camera Status");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C Camera Status test
     * 
     */
}

/*
 * Read the value of all the CDMC registers
 */

int
vino_camera_get()

{
    int	    loop;
    ulong   errcount = 0L;
    ulong   regReturn;
    
    if (board_initialized == FALSE) {
	errcount = vino_probe();
    }
    
    for (loop=CDMC_REG_CTRL_STATUS; loop<=CDMC_REG_READ_ADDR_MAX; loop++) {
	regReturn = vinoI2cReadReg(vino_boards, CDMC_addr, loop);
	if (regReturn == FAILURE) {
	    msg_printf(INFO, "cannot read 0x%x addr 0x%x\n", CDMC_addr, loop);
	    i2cForceBusIdle(regs_base);
	}
	else
	    msg_printf(INFO, "0x%x addr 0x%x -> 0x%x\n", CDMC_addr, loop, regReturn);
    }
    return errcount;
}

/*
 * DEBUG TEST
 */
int
vino_camera_wr()

{
    ulong   val;
    
    if (vinoI2cWriteReg(vino_boards, CDMC_addr, 0x4, 0xfa) == FAILURE){
	msg_printf(ERR, "writeReg fail\n");
	i2cForceBusIdle(regs_base);
    }

    val = vinoI2cReadReg(vino_boards, CDMC_addr, 0x4);
    if (val != 0xfa || val == FAILURE) {
	if (val == FAILURE)
	    msg_printf(ERR, "i2c failure\n");
	else
	    msg_printf(ERR, "readReg fail (%x)\n", val);
    }
}


/*
 * Initialize the CDMC D1 camera to a known state. The
 * values are those that the camera automatically resets
 * itself to. This test is equivalent to writing a value
 * to sub address 0xF of the camera.
 * 
 * 06.29.93 - added support for asic version of the cameras
 */

int
vino_camera_init(void)

{
    int	    errcount = 0;
    int	    CDMC_defaults_start = 0x0;
    ulong   reg_val;
    
    if (board_initialized == FALSE) {
	errcount = vino_probe();
    }
    
    CDMC_addr = CDMC_ADDR_ASIC;
    
    reg_val = vinoI2cReadReg(vino_boards, CDMC_addr, CDMC_REG_VERSION);
    
    if (reg_val != 0x10) {
	CDMC_addr = CDMC_ADDR_FPGA;
    }
    
    if (vinoI2cWriteBlock(vino_boards, CDMC_addr,
	    CDMC_defaults_start, CDMC_defaults) == FAILURE) {
	errcount++;
	msg_printf(ERR, "Cannot initialize CDMC\n");
    }
    else
	CDMC_initialized = TRUE;
    
    if (errcount != 0)
	sum_error ("VINO I2C CDMC Init");
    else
	okydoky ();

    return (errcount);
    /*
     * I2C CDMC Initialization test for VINO
     * 
     */    
}
