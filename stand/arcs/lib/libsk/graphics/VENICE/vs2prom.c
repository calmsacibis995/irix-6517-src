/*
 * prom.c
 *
 *	VS2 (aka MCO) specific functions for the boot PROM and kernel textport.
 *
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

#define PROM_VARIABLE_SET 1 

#include <sys/videoaddr.h>
#include <gl/xilinx.h>
#include <sys/fcntl.h>
#include <dirs.h>
#include <mc.out.h>
#include <sys/types.h>


/* phony defines so we can include venice.h without needing gfx.h */
#if 0
struct gfx_info { int dummy; };
struct gfx_data { int dummy; };
struct RRM_ValidateClip { int dummy; };
#endif

#undef _KERNEL
#include <vs2.h>
#include <sys/venice.h>
#include <sys/debug.h>
#include <gl/voftypedefs.h>
#include <vofdefs.h>
#include <vs2_eeprom.h>


#include <sys/mman.h>

typedef int Boolean;
#define	TRUE	1
#define	FALSE	0


extern int load_4000_serial_byte(void (*)(int), int (*)(void), void (*)(int),
		int (*)(void), int(*)(void), int, unsigned char*);

/*
 * Forward Routine Declarations
 */
extern void sginap(int ticks);
extern void vs2GetHeadInfo(int, venice_info_t *, venice_vof_file_info_t *);
extern void vs2Initialize(int, int);
extern int vs2Installed(int);
extern void vs2ManagedArea(int, int *, int *);
static void vs2ReadEEPROMBlock(VS2Dev *, off_t, u_char *, int);
static void setEEPROMPageNumber(VS2Dev *, int);
static void vs2ReadEEPROMCode(VS2Dev *, u_char *);
static int vs2ReadEEPROMData(VS2Dev *, venice_vs2_eeprom_t *);
static void venice_vs2init(VS2Dev *, int);
static void vs2InitDACs(VS2Dev *, int, venice_vs2_eeprom_t *, Boolean);
static void vs2InitRegs(VS2Dev *, int, venice_vs2_eeprom_t *, Boolean);
static void vs2InitShadow(VS2Dev *, int, venice_vs2_eeprom_t *, Boolean);
static void vs2MapScreenToChannel(VS2Dev *, int, int);
static u_char vs2ReadByte(VS2Dev *, off_t);
static void vs2RegisterClearBit(VS2Dev *, off_t, u_char);
static void vs2RegisterSetBit(VS2Dev *, off_t, u_char);
static u_char vs2ReadStatus(VS2Dev *, off_t);
/* static void vs2ToggleSerialClock(VS2Dev *); */
static void vs2WriteByte(VS2Dev *, off_t, u_char);
static void vs2WriteProgClock(VS2Dev *, off_t, unsigned char);
static void vs2WriteXilinxReg(VS2Dev *, off_t, off_t, unsigned char);
static void vs2WriteDACAlternateRAM(VS2Dev *, paddr_t, int, int, u_char *);
static void vs2WriteDACCommandRegister(VS2Dev *, paddr_t, paddr_t, u_char);
static void vs2WriteDACPrimaryRAM(VS2Dev *, paddr_t, int, int, u_char *);
static void writeDACAddress(VS2Dev *, paddr_t, int);
/* static void vs2DPOTSet(u_char); */
static void venice_clock_vs2_serial_dataBit(VS2Dev *, int);
static void set_edgeMem_for_edge(VS2Dev *, int, int, unsigned short, off_t);
static void venice_load_vs2_vof_edgeMem(VS2Dev *, off_t, vof_data_t *);
static void venice_load_vs2_vof_verticalInfo(VS2Dev *, off_t, vof_data_t *);
static void venice_load_vs2_clockDivider(VS2Dev *, int);
static int venice_loadsection_vs2_top(VS2Dev *, off_t, int, int, int, unsigned int *);
static int venice_loadsection_vs2_body(VS2Dev *, off_t, int, int, int, vof_data_t *);
static int loadVS2VOFFromData(VS2Dev *, off_t, int, int, int, unsigned int *, vof_data_t *);
static void venice_clock_vs2_serial_dataByte(int);
static int venice_wait_vs2_vif0_configMem_clear(void);
static void venice_write_vs2_vif0_program(int);
static int venice_check_vs2_vif0_frame_status(void);
static int venice_read_vs2_vif0_done(void);
static int venice_load_vs2_vif0_xilinx(unsigned char *);
static int venice_wait_vs2_vif1_configMem_clear(void);
static void venice_write_vs2_vif1_program(int);
static int venice_check_vs2_vif1_frame_status(void);
static int venice_read_vs2_vif1_done(void);
static int venice_load_vs2_vif1_xilinx(unsigned char *);
static int venice_wait_vs2_vof0_configMem_clear(void);
static void venice_write_vs2_vof0_program(int);
static int venice_check_vs2_vof0_frame_status(void);
static int venice_read_vs2_vof0_done(void);
static int venice_load_vs2_vof0_xilinx(unsigned char *);
static int venice_wait_vs2_vof1_configMem_clear(void);
static void venice_write_vs2_vof1_program(int);
static int venice_check_vs2_vof1_frame_status(void);
static int venice_read_vs2_vof1_done(void);
static int venice_load_vs2_vof1_xilinx(unsigned char *);
static int venice_load_vs2_xilinx(VS2Dev *, vs2_xilinx_t, unsigned char *);
static int vs2Close(VS2Dev *);
static int vs2Delay(int);
static VS2Dev *vs2Open(unsigned);
static paddr_t physicalConnect(unsigned pipeNumber, VS2Dev *vd);

static VS2DACDesc initDACTable[] =
{
 {V_VS2_DAC_0_RED, isRed, 0},
 {V_VS2_DAC_0_GRN, isGreen, 0},
 {V_VS2_DAC_0_BLU, isBlue, 0},
 {V_VS2_DAC_1_RED, isRed, 1},
 {V_VS2_DAC_1_GRN, isGreen, 1},
 {V_VS2_DAC_1_BLU, isBlue, 1},
 {V_VS2_DAC_2_RED, isRed, 2},
 {V_VS2_DAC_2_GRN, isGreen, 2},
 {V_VS2_DAC_2_BLU, isBlue, 2},
 {V_VS2_DAC_3_RED, isRed, 3},
 {V_VS2_DAC_3_GRN, isGreen, 3},
 {V_VS2_DAC_3_BLU, isBlue, 3},
 {V_VS2_DAC_4_RED, isRed, 4},
 {V_VS2_DAC_4_GRN, isGreen, 4},
 {V_VS2_DAC_4_BLU, isBlue, 4},
 {V_VS2_DAC_5_RED, isRed, 5},
 {V_VS2_DAC_5_GRN, isGreen, 5},
 {V_VS2_DAC_5_BLU, isBlue, 5},
};

VS2AllDACDesc vs2AllDACs = {initDACTable, ARRAY_COUNT(initDACTable)};

extern unsigned char g1_7[];

Boolean		vs2eepromValid;
unsigned char	vs2eepromcode[V_VS2_EEPROM_CODE_LENGTH];
venice_vs2_eeprom_t vs2eepromdata;

static unsigned char   vs2_vof0_clock_value;
static unsigned char   vs2_vof1_clock_value;
static unsigned char   vs2_vif0_sync_width_internal;
static unsigned char   vs2_vif0_gensrc_cntrl_internal;
static unsigned char   vs2_vif0_bporch_clamp_internal;
static unsigned char   vs2_vif1_sync_width_internal;
static unsigned char   vs2_vif1_gensrc_cntrl_internal;
static unsigned char   vs2_vif1_bporch_clamp_internal;
static unsigned char   vs2_register_7_shadow;
static unsigned char   vs2_vof0_control_reg_value;
static unsigned char   vs2_vof1_control_reg_value;


/*
 * Function: vs2GetHeadInfo
 *
 * Description: Populates a venice_info_t structure with offset
 * and generic information about each format.
 *
 *
 * Parameters:
 *
 * pipeNumber - the pipe number of the vs2 in question
 *
 * venInfo - pointer to a venice_info_t structure
 *
 *
 * Return:  (void)
 */
/* ARGSUSED */
extern void
vs2GetHeadInfo(int pipeNumber, venice_info_t *venInfo, venice_vof_file_info_t *vofDisplayDG2)
{
    int	headIndex;

    for (headIndex = 0; headIndex < VENICE_VS2_MAX_HEADS; headIndex++) {
	int formatIndex = vs2eepromdata.formatParameters.headInfo[headIndex].vofIndex;

	venInfo->vof_head_info[headIndex].active = vs2eepromdata.formatParameters.headInfo[headIndex].isActive;
	venInfo->vof_head_info[headIndex].pan_x = vs2eepromdata.formatParameters.headInfo[headIndex].offset.x;
	venInfo->vof_head_info[headIndex].pan_y = vs2eepromdata.formatParameters.headInfo[headIndex].offset.y;

	/* Copy the entire vof_file_info structure */
	venInfo->vof_head_info[headIndex].vof_file_info =
	    vs2eepromdata.format[formatIndex].vof.vof_file_info;

	/*
	 * The calculated cursor fudge values in the individual VOFs of
	 * the different VS2 formats are not correct.  Instead,  take the
	 * cursor fudge from the original DG2 format.
	 */
	venInfo->vof_head_info[headIndex].vof_file_info.cursor_fudge_x =
	    vofDisplayDG2->cursor_fudge_x;
	venInfo->vof_head_info[headIndex].vof_file_info.cursor_fudge_y =
	    vofDisplayDG2->cursor_fudge_y;
    }
}


/*
 * Function: vs2Initialize
 *
 * Description: initialize the vs2 board
 *
 * Parameters:	pipe number (graphics head; normally 0)
 *		verbose flag
 *
 * Return: none
 */
/* ARGSUSED */
extern void
vs2Initialize(int pipeNumber, int verbose)
{
    VS2Dev         *vd;

    vd = vs2Open(pipeNumber);
    vs2ReadEEPROMCode(vd, vs2eepromcode);
    vs2ReadEEPROMData(vd, &vs2eepromdata);
    venice_vs2init(vd, verbose);

    vs2Close(vd);
}

/*
 * Function: vs2Installed
 *
 * Description: tells whether or not a VS2 is physically present
 *		in the system. A present-but-defective VS2 should
 *		probably return `not installed'.  If EEPROM not
 *		initialized with format path, return 'not installed'.
 *
 * Parameters: pipe number (graphics head; normally 0)
 *
 * Return: int, 0 if not installed, 1 if installed
 */
/* ARGSUSED */
extern int
vs2Installed(int pipeNumber)
{
    extern struct venice_info VEN_info;

    return(0);
}

/*
 * Function: vs2ManagedArea
 *
 * Description: Query the VS2 for the desired managed area
 *
 * Parameters:
 *
 * pipe number (graphics head; normally 0)
 *
 * height, width - by reference.  The returned height and width of the
 * desired managed area.
 *
 * Return:  (void)
 */
/* ARGSUSED */
extern void
vs2ManagedArea(int pipeNumber, int *height, int *width)
{
    *height = vs2eepromdata.formatParameters.displaySurfaceSize.height;
    *width = vs2eepromdata.formatParameters.displaySurfaceSize.width;
}

/*
 * Function: vs2ReadByte
 *
 * Description:  Reads the byte from the specified VS2 address
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * offset - the offset from which to read a byte
 *
 *
 * Return:  The value at the specified byte.
 */
static u_char
vs2ReadByte(VS2Dev *vd, off_t vmeOffset)
{
    return(*((u_char *) (vd->mappedBase + vmeOffset)));
}


/*
 * Function: vs2RegisterClearBit
 *
 * Description:  Clears the single bit in the specified VS2 Register
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * offset - the offset of the register to read
 *
 * bitval - the value of the bit to clear.
 *
 *
 * Return:  (void)
 */
static void
vs2RegisterClearBit(VS2Dev *vd, off_t vmeOffset, u_char bitval)
{
    u_char	regValue;

    regValue = vs2ReadByte(vd, vmeOffset);
    regValue = regValue & (~bitval);
    vs2WriteByte(vd, vmeOffset, regValue);
}

/*
 * Function: vs2RegisterSetBit
 *
 * Description:  Sets the single bit in the specified VS2 Register
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * offset - the offset of the register to write
 *
 * bitval - the value of the bit to set.
 *
 *
 * Return:  (void)
 */
static void
vs2RegisterSetBit(VS2Dev *vd, off_t vmeOffset, u_char bitval)
{
    u_char	regValue;

    regValue = vs2ReadByte(vd, vmeOffset);
    regValue |= bitval;
    vs2WriteByte(vd, vmeOffset, regValue);
}


/*
 * Function: vs2ReadStatus
 *
 * Description:  Reads the status byte from the specified VS2 address.
 * This is a special routine required because the VS2 status registers
 * require they be read twice.  If the value between the two samples
 * differs, the register is read again.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * offset - the offset from which to read a byte
 *
 *
 * Return:  The value at the specified byte.
 */
static u_char
vs2ReadStatus(VS2Dev *vd, off_t vmeOffset)
{
    u_char          val1, val2;

    val1 = vs2ReadByte(vd, vmeOffset);
    val2 = vs2ReadByte(vd, vmeOffset);
    if (val1 != val2)
	val2 = vs2ReadByte(vd, vmeOffset);

    return (val2);
}

#if 0
/*
 * Function: vs2ToggleSerialClock
 *
 * Description:  Toggles the vs2 serial clock register
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 *
 * Return:  (void)
 */
static void
vs2ToggleSerialClock(VS2Dev *vd)
{
    vs2WriteByte(vd, V_VS2_SERIAL_CLOCK, 0x1);
}
#endif

/*
 * Function: vs2WriteByte
 *
 * Description:  Writes the byte to the specified VS2 address
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * offset - the offset from which to read a byte
 *
 * newValue - the value to place in the specified address.
 *
 *
 * Return:  (void)
 */
static void
vs2WriteByte(VS2Dev *vd, off_t vmeOffset, u_char newValue)
{
    *((u_char *) (vd->mappedBase + vmeOffset)) = newValue;
}

/*
 * Function: vs2WriteProgClock
 *
 * Description: Writes a byte to the specified programmable clock, using
 *		the xilinxBase arg to indicate which of two should be written
 *		to.
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * xilinxBase - the base address of the vof/vif whose clock is to be programmed
 *
 * data - the value to be placed in the programmable clock.
 *
 * Return:  (void)
 */
static void
vs2WriteProgClock(VS2Dev *vd, off_t xilinxBase, unsigned char data)
{
    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	vs2WriteByte(vd, V_VS2_PROGCLK0_SEL, data);
    } else {
	vs2WriteByte(vd, V_VS2_PROGCLK1_SEL, data);
    }
}

/*
 * Function: vs2WriteXilinxReg
 *
 * Description:  Writes a byte to the specified VOF or VIF xilinx.
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * xilinxBase - the base address of the vof/vif to be programmed
 *
 * regOffset - the register offset within the xilinx (even numbers from
 *             0 to 0xe)
 *
 * data - the value to place in the specified address.
 *
 * Return:  (void)
 */
static void
vs2WriteXilinxReg(VS2Dev *vd, off_t xilinxBase, off_t regOffset,
		unsigned char data)
{
    *((u_char *) (vd->mappedBase + xilinxBase + regOffset)) = data;
}

/*
 * Function: vs2ReadEEPROMBlock
 *
 * Description:  Reads a series of EEPROM data bytes.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * sourceEEPROM - the offset into the EEPROM at which to read.
 *
 * dest - a pointer to the destination into which to place the data.
 *
 * cnt - the number of bytes to transfer.
 *
 *
 * Return:  (void)
 */
static void
vs2ReadEEPROMBlock(VS2Dev *vd, off_t sourceEEPROM, u_char *dest, int cnt)
{
    /* Calculate the page number and offset needed for the first access */
    int             offsetThisPage = (int)(sourceEEPROM % V_VS2_EEPROM_PAGE_LENGTH);
    int             newPageNumber = (int)(sourceEEPROM / V_VS2_EEPROM_PAGE_LENGTH);

    /* Continue transferring bytes until all transferred */
    while (cnt) {

	/*
	 * This block handles all bytes needed within a single page.  First
	 * calculate the number of bytes required for this page and the VME
	 * address needed to access the first byte of this page's transfer.
	 */
	int             bytesThisPage = MIN(cnt, ABS(offsetThisPage - V_VS2_EEPROM_PAGE_LENGTH));
	u_char         *ptrEEPROM =((u_char *) vd->mappedBase) + V_VS2_EEPROM_BASE +
	    offsetThisPage * V_VS2_CONSECUTIVE_BYTE_SKIP;

	/*
	 * The transfer in the following loop will account for the needed
	 * bytes in this page.
	 */
	cnt -= bytesThisPage;

	/* Set the page number of the EEPROM to the one needed here */
	setEEPROMPageNumber(vd, newPageNumber);

	/* Read all the bytes required for this page. */
	while (bytesThisPage--) {
	    *dest++ = *ptrEEPROM;
	    ptrEEPROM += V_VS2_CONSECUTIVE_BYTE_SKIP;
	}

	/*
	 * If needed, the transfer at the beginning of the next page starts
	 * at the beginning of the page.
	 */
	offsetThisPage = 0;
	newPageNumber++;
    }

    /*
     * Must return to page 0
     */
    setEEPROMPageNumber(vd, 0);
}

/*
 * Function: setEEPROMPageNumber
 *
 * Description:  Sets the VS2 to map a desired page of the EEPROM
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * pageNumber - the page number
 *
 *
 * Return:  (void)
 */
static void
setEEPROMPageNumber(VS2Dev *vd, int pageNumber)
{
    u_char          newContents;

    /*
     * This register is write-only, so just write the contents of the
     * caller's page number without reading in advance.
     */
    newContents =
	vs2_register_7_shadow & ~V_VS2_EES_SEL_MASK |
	pageNumber & V_VS2_EES_SEL_MASK;
    vs2WriteByte(vd, V_VS2_REGISTER7, newContents);
}

/*
 * Function: vs2ReadEEPROMCode
 *
 * Description:  Reads the entire VS2 EEPROM code space into a buffer.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * dest - pass a pointer to the buffer which is to receive the contents
 * of the EEPROM.  The buffer should be V_VS2_EEPROM_CODE_LENGTH bytes
 * long.
 *
 *
 * Return:  (void)
 */
static void
vs2ReadEEPROMCode(VS2Dev *vd, u_char *dest)
{
    vs2ReadEEPROMBlock(vd,
		    V_VS2_EEPROM_CODE_OFFSET,
		    dest,
		    V_VS2_EEPROM_CODE_LENGTH);
}

/*
 * Function: vs2ReadEEPROMData
 *
 * Description:  Reads the entire VS2 EEPROM data space into a buffer.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * dest - pass a pointer to the buffer which is to receive the contents
 * of the EEPROM.  The buffer should be sizeof(venice_vs2_eeprom_t) bytes
 * long.
 *
 *
 * Return:  TRUE if EEPROM is valid, FALSE if bad.
 */
static int
vs2ReadEEPROMData(VS2Dev *vd, venice_vs2_eeprom_t *dest)
{
    int             returnStatus = TRUE;

    vs2ReadEEPROMBlock(vd,
		    V_VS2_EEPROM_DATA_OFFSET,
		    (u_char *) dest,
		    sizeof(venice_vs2_eeprom_t));

    return (returnStatus);
}


/*
 * The calibration information for the DPOTS is always derived from
 * the user configuration set.
 */
static struct {
    int		    channelNumber;
    enum {
	DPOT_COLOR_RED, 
	DPOT_COLOR_GREEN, 
	DPOT_COLOR_BLUE
    }		    dpotColor;
    off_t	    reg;
    u_char	    bit;
    off_t	    offset;
} regToDPOT[] =
{
    {
	0, DPOT_COLOR_RED, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_RED_CH_0, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[0].valuesDPOT.r)
    },
    {
	0, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_GRN_CH_0, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[0].valuesDPOT.g)
    },
    {
	0, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_BLU_CH_0, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[0].valuesDPOT.b)
    },
    {
	1, DPOT_COLOR_RED, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_RED_CH_1, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[1].valuesDPOT.r)
    },
    {
	1, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_GRN_CH_1, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[1].valuesDPOT.g)
    },
    {
	1, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_BLU_CH_1, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[1].valuesDPOT.b)
    },
    {
	2, DPOT_COLOR_RED, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_RED_CH_2, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[2].valuesDPOT.r)
    },
    {
	2, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER0, V_VS2_SEL_DPOT_GRN_CH_2, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[2].valuesDPOT.g)
    },
    {
	2, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_BLU_CH_2, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[2].valuesDPOT.b)
    },
    {
	3, DPOT_COLOR_RED, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_RED_CH_3, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[3].valuesDPOT.r)
    },
    {
	3, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_GRN_CH_3, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[3].valuesDPOT.g)
    },
    {
	3, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_BLU_CH_3, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[3].valuesDPOT.b)
    },
    {
	4, DPOT_COLOR_RED, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_RED_CH_4, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[4].valuesDPOT.r)
    },
    {
	4, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_GRN_CH_4, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[4].valuesDPOT.g)
    },
    {
	4, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_BLU_CH_4, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[4].valuesDPOT.b)
    },
    {
	5, DPOT_COLOR_RED, 
	V_VS2_REGISTER1, V_VS2_SEL_DPOT_RED_CH_5, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[5].valuesDPOT.r)
    },
    {
	5, DPOT_COLOR_GREEN, 
	V_VS2_REGISTER2, V_VS2_SEL_DPOT_GRN_CH_5, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[5].valuesDPOT.g)
    },
    {
	5, DPOT_COLOR_BLUE, 
	V_VS2_REGISTER2, V_VS2_SEL_DPOT_BLU_CH_5, 
	(off_t) offsetof(venice_vs2_eeprom_t, calibrationInfo[VS2ConfigurationSet_User].head[5].valuesDPOT.b)
    },
};

/*
 * Function: venice_vs2init
 *
 * Description:  Performs initialization of VS2 board.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * verbose - if non-zero, prints messages relating to VS2 initialization
 *
 *
 * Return: (void)
 */
static void
venice_vs2init(VS2Dev *vd, int verbose)
{
    vs2InitRegs(vd, verbose, &vs2eepromdata, vs2eepromValid);
    vs2InitDACs(vd, verbose, &vs2eepromdata, vs2eepromValid);
    vs2InitShadow(vd, verbose, &vs2eepromdata, vs2eepromValid);

    /*
     * Load xilii individually for now; eventually, we will gang program the
     * vof pair, vif pair.
     */
    venice_load_vs2_xilinx(vd, VS2_VIF0_XILINX, vs2eepromcode);
    venice_load_vs2_xilinx(vd, VS2_VIF1_XILINX, vs2eepromcode);
    venice_load_vs2_xilinx(vd, VS2_VOF0_XILINX, vs2eepromcode);
    venice_load_vs2_xilinx(vd, VS2_VOF1_XILINX, vs2eepromcode);

    /*
     * Load the VOF edge tables.  This comes directly from the EEPROM.
     */
    {
	int             vofIndex;
	static struct {
	    unsigned xilinxAddr;
	}   vofVariant[] =
	{
	    { V_VS2_VIF0_XILINX_BASE },
	    { V_VS2_VIF1_XILINX_BASE },
	};

	for (vofIndex = 0; vofIndex < VS2_MAX_FORMATS; vofIndex++) {
	    int		    isMasterOfVS2;
	    int		    doGenlock;

	    isMasterOfVS2 =
		(vofIndex == vs2eepromdata.genlockMasterOfVS2);
	    /*
	     * If we are the genlock master of the VS2 (as specified by setmon),
	     * we will lock to an internal hrate reference UNLESS setmon also
	     * instructs us to lock to an external sync reference (that comes
	     * in to the VS2 VIF for this channel).
	     */
	    if (isMasterOfVS2) {
		doGenlock = vs2eepromdata.doGenlock;
	    } else {
		/*
		 * Slave VOF always does an external TTL lock to the master.
		 */
		doGenlock = 1;
	    }
	    loadVS2VOFFromData(vd,
			       vofVariant[vofIndex].xilinxAddr,
			       isMasterOfVS2,
			       doGenlock,
			       vs2eepromdata.genlockTTL,
			       vs2eepromdata.format[vofIndex].vofParams,
			       &vs2eepromdata.format[vofIndex].vof);
	}
    }
}

/*
 * Function: vs2InitDACs
 *
 * Description:  Performs initialization of VS2 DACs.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * verbose - if non-zero, prints messages relating to VS2 initialization
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2InitDACs(VS2Dev *vd,
	    int verbose,
	    venice_vs2_eeprom_t *eeprom,
	    Boolean eepromValid)
{
    /* Initialize DACs */
    VS2DACDesc	   *initDAC;
    int             indexDACs;
    u_char          alternateRAMRampBuf[V_VS2_DAC_ALTERNATE_RAM_LENGTH];

    /*
     * Generate the simple ramps that will be used as the default primary and
     * alternate RAM gamma tables.
     */
    {
	int             cellValue;
	u_char         *cellPtr;

	/* Note that the ramps are set up in incrementing direction. */
	for (cellPtr = alternateRAMRampBuf, cellValue = 0;
	     cellValue < ARRAY_COUNT(alternateRAMRampBuf);
	     cellValue++)
	    *cellPtr++ = cellValue;
    }

    /* Initialize each of the DACs in the table */
    for (initDAC = vs2AllDACs.array, indexDACs = 0;
	 indexDACs < vs2AllDACs.count;
	 initDAC++, indexDACs++) {

	/*
	 * The following steps are performed for each of the DACs that
	 * require initialization.  For more details on the meaning behind
	 * the code below, refer to the Brooktree 461/462 specification.
	 */
	u_char          cmd0Options, cmd1Options, cmd2Options;

	/*
	 * Command Register 0  has 5:1 multiplexing and enables the alternate
	 * palette.
	 */
	cmd0Options =
	    V_VS2_DAC_CR0_MULTIPLEX_5_1 |
	    V_VS2_DAC_CR0_ALT_ENABLE;
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_COMMAND_REG_0,
				   cmd0Options);

	/* CommandRegister 1 specifies no panning. */
	cmd1Options =
	    V_VS2_DAC_CR1_PAN_0_PIXELS;
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_COMMAND_REG_1,
				   cmd1Options);

	/*
	 * CommandRegister 2 enables sync.  PLL output uses blank input for
	 * generating PLL information.
	 */
	cmd2Options =
	    V_VS2_DAC_CR2_SYNC_ENABLE |
	    V_VS2_DAC_CR2_PLL_BLANK;
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_COMMAND_REG_2,
				   cmd2Options);

	/* Pixel Read Mask enables all bit planes. */
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_PIXEL_READ_MASK_REG_LOW,
				   0xff);
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_PIXEL_READ_MASK_REG_HIGH,
				   0xff);

	/* Pixel Blink Mask disables all bit planes. */
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_PIXEL_BLINK_MASK_REG_LOW,
				   0x0);
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_PIXEL_BLINK_MASK_REG_HIGH,
				   0x0);


	/*
	 * Disable RGB overlay blink mode.
	 */
	vs2WriteDACCommandRegister(vd,
				   initDAC->idDAC,
				   V_VS2_DAC_OVERLAY_BLNK_MASK_REG,
				   0x0);

	/* Load the primary and alternate rams with a linear ramp */
	vs2WriteDACPrimaryRAM(vd,
			      initDAC->idDAC,
			      0,
			      V_VS2_DAC_PRIMARY_RAM_LENGTH,
			      g1_7);
	/* Load the alternate ram with a linear ramp */
	vs2WriteDACAlternateRAM(vd,
				initDAC->idDAC,
				0,
				ARRAY_COUNT(alternateRAMRampBuf),
				alternateRAMRampBuf);


	/* Use the overlays only when using channel 0 */
	{
	    u_char          readMask;

	    readMask = 0;

	    /*
	     * Enable RGB overlay planes (Overlay Read Mask Register). 
	     */
	    vs2WriteDACCommandRegister(vd,
				       initDAC->idDAC,
				       V_VS2_DAC_OVERLAY_READ_MASK_REG,
				       readMask);
	}
    }
}


/*
 * Function: vs2InitRegs
 *
 * Description:  Performs initialization of VS2 registers.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * verbose - if non-zero, prints messages relating to VS2 initialization
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2InitRegs(VS2Dev *vd,
			int verbose,
			venice_vs2_eeprom_t *eeprom,
			Boolean eepromValid)
{
    /* Clear access to all DPOTS */
    {
	int             dpotIndex;

	for (dpotIndex = 0; dpotIndex < ARRAY_COUNT(regToDPOT); dpotIndex++) {
	    vs2RegisterSetBit(vd, regToDPOT[dpotIndex].reg, regToDPOT[dpotIndex].bit);
	}
    }

    /* Hrate divider enable should be low when inactive */
    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_HDIV_EN);

    /* Blank all the channels initially (milo, 9/22/92). */
    vs2RegisterSetBit(vd, V_VS2_REGISTER6, V_VS2_PWR_BLNK);


    /*
     * Configure the VS2 based on which VOF is the genlock master.
     */
    {
	typedef struct {
	    struct {
		off_t           registerName;
		u_char          bitValue;
	    }               externalGenlockSet;
	    struct {
		off_t           registerName;
		u_char          bitValue;
	    }               internalTimeBaseSet;
	}               CompositeSettings;

	static CompositeSettings byVOFSetting[VS2_MAX_FORMATS] =
	{
	/* VOF 0 Values */
	 {
	  {V_VS2_REGISTER2, V_VS2_GENLK_VIF0},
	  {V_VS2_REGISTER4, V_VS2_HRATE0_SEL},
	  },
	/* VOF 1 Values */
	 {
	  {V_VS2_REGISTER2, V_VS2_GENLK_VIF1},
	  {V_VS2_REGISTER4, V_VS2_HRATE1_SEL},
	  }
	};
	CompositeSettings *vs2GenlockMaster;

	{
	    CompositeSettings *csptr;
	    int             vofIndex;

	    /* By default, enable nothing. */
	    for (vofIndex = 0, csptr = byVOFSetting;
		 vofIndex < VS2_MAX_FORMATS;
		 vofIndex++, csptr++) {
		/* Disable external genlock */
		vs2RegisterClearBit(vd,
				    csptr->externalGenlockSet.registerName,
				    csptr->externalGenlockSet.bitValue);
		/* Disable internal timebase */
		vs2RegisterClearBit(vd,
				    csptr->internalTimeBaseSet.registerName,
				    csptr->internalTimeBaseSet.bitValue);
	    }
	}

	/* Program the board based on genlock status */
	vs2GenlockMaster = &byVOFSetting[eeprom->genlockMasterOfVS2];
	if (eeprom->doGenlock) {

	    /*
	     * If the board is set to external genlock, enable external
	     * genlock for the VS2's genlock master.
	     */
	    vs2RegisterSetBit(vd,
			  vs2GenlockMaster->externalGenlockSet.registerName,
			      vs2GenlockMaster->externalGenlockSet.bitValue);
	} else {

	    /*
	     * If the board is set to internal horizontal time base genlock
	     * (i.e., opposite of normal genlock), set HRATE appropriately.
	     */
	    vs2RegisterSetBit(vd,
			 vs2GenlockMaster->internalTimeBaseSet.registerName,
			    vs2GenlockMaster->internalTimeBaseSet.bitValue);
	}
    }

    /*
     * This makes the appropriate VOF's CSYNC available to the DG2 so it can
     * lock(slave) to the VS2 VOF if the DG2 is configured to do so.  Note
     * that the genlock master of the DG2 can be a different VOF than the
     * genlock master of the VS2.
     */
    {
	if (eeprom->formatParameters.genlockMasterOfDG2 == 0) {
	    vs2RegisterSetBit(vd, V_VS2_REGISTER3, V_VS2_OPSYNC0_SEL);
	} else {
	    vs2RegisterClearBit(vd, V_VS2_REGISTER3, V_VS2_OPSYNC0_SEL);
	}
    }

    /*
     * Map all valid channels to the screen number specified in the EEPROM.
     * Also, map channels (6 and 7) to display to nothing.
     */
    vs2MapScreenToChannel(vd, 0, 0x3f);
    vs2MapScreenToChannel(vd, 1, 0);
    vs2MapScreenToChannel(vd, 2, 0);
    vs2MapScreenToChannel(vd, 3, 0);
    vs2MapScreenToChannel(vd, 4, 0);
    vs2MapScreenToChannel(vd, 5, 0);
    vs2MapScreenToChannel(vd, 6, 0);
    vs2MapScreenToChannel(vd, 7, 0);

    /* XILINX enables should be held high except when selected */
    vs2RegisterSetBit(vd, V_VS2_REGISTER5, V_VS2_VOF0_PROG_L);
    vs2RegisterSetBit(vd, V_VS2_REGISTER5, V_VS2_VOF1_PROG_L);
    vs2RegisterSetBit(vd, V_VS2_REGISTER5, V_VS2_VIF0_PROG_L);
    vs2RegisterSetBit(vd, V_VS2_REGISTER5, V_VS2_VIF1_PROG_L);

    /* XILINX enables should be low high except when selected */
    vs2RegisterClearBit(vd, V_VS2_REGISTER5, V_VS2_VOF0_RB_TRIG);
    vs2RegisterClearBit(vd, V_VS2_REGISTER5, V_VS2_VOF1_RB_TRIG);
    vs2RegisterClearBit(vd, V_VS2_REGISTER5, V_VS2_VIF0_RB_TRIG);
    vs2RegisterClearBit(vd, V_VS2_REGISTER5, V_VS2_VIF1_RB_TRIG);

    /* For normal operation of ADCs */
    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_ADC0_SEL_1);
    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_ADC1_SEL_1);

    /* Normal operation is with AGC enabled */
    vs2RegisterClearBit(vd, V_VS2_REGISTER6, V_VS2_ADC0_SEL_1);
    vs2RegisterClearBit(vd, V_VS2_REGISTER6, V_VS2_ADC1_SEL_1);
    vs2RegisterClearBit(vd, V_VS2_REGISTER6, V_VS2_AGC0_OFF);
    vs2RegisterClearBit(vd, V_VS2_REGISTER6, V_VS2_AGC1_OFF);

    /*
     * Blank any screen that does not output video in this format.
     */
    {
	int             headIndex;

	for (headIndex = 0; headIndex < VENICE_VS2_MAX_HEADS; headIndex++) {
	    if (!eeprom->formatParameters.headInfo[headIndex].isActive) {
		/* something */
	    }
	}
    }


    /*
     * To handle an unexpected condition in one of the parts on the board, we
     * must tailor operation based on the frequency of the format, setting
     * V_VS2_FREQ_SEL when the frequency is less than 125MHz.  The companion
     * bit, known as both V_VS2_DVI_TEST and V_VS2_FORMAT_LESS_THAN_75MHZ,
     * needs to be shadowed and is set elsewhere. 
     */
    {
	int             registerName = V_VS2_REGISTER6;
	int             bitName = V_VS2_FREQ_SEL;

	if (eeprom->formatParameters.formatPixelRateHz < 125000000) {
	    vs2RegisterSetBit(vd,
			      registerName,
			      bitName);
	} else {
	    vs2RegisterClearBit(vd,
				registerName,
				bitName);
	}
    }
}

/*
 * Function: vs2InitShadow
 *
 * Description:  Performs initialization of VS2 shadow structure.
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * verbose - if non-zero, prints messages relating to VS2 initialization
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2InitShadow(VS2Dev *vd,
			int verbose,
			venice_vs2_eeprom_t *eeprom,
			Boolean eepromValid)
{

    /*
     * No need to proceed if the information in the EEPROM isn't valid
     * (that's how we initialize the shadow). 
     */
    if (eepromValid) {

	/*
	 * Set the shadow of register 7 to optionally contain the value of
	 * the so-called "V_VS2_DVI_TEST" bit (known here as
	 * V_VS2_FORMAT_LESS_THAN_75MHZ).  The companion bit called
	 * V_VS2_FREQ_SEL is set elsewhere. 
	 */
	vs2_register_7_shadow =
	    (eeprom->formatParameters.formatPixelRateHz < 75000000) ?
	    V_VS2_FORMAT_LESS_THAN_75MHZ : 0;
    }
}

/*
 * Function: vs2MapScreenToChannel
 *
 * Description:  Maps a screen to a channel
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * screenNumber - the screen number
 *
 * channelVal - the channel numbers, expressed with OR'd expressions
 * using the V_VS2_VSC_CHANNEL_VAL(channel) macro.  For one channel (e.g.,
 * channel 3),  use "V_VS2_VSC_CHANNEL_VAL(3)".  For two channels (e.g.,
 * channels 3 and 5), use "V_VS2_VSC_CHANNEL_VAL(3) | V_VS2_VSC_CHANNEL_VAL(5)".
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2MapScreenToChannel(VS2Dev *vd, int screenNumber, int channelVal)
{
    u_char          vscMode;

    /*
     * The VSC mode register contains the screen number which is to be
     * mapped.
     *
     * Enable setting of screen-to-channel assignments while setting the
     * screen number.
     */
    vscMode =
	V_VS2_VSC_SCRN_TBL_MODE_ENABLE |
	((screenNumber << V_VS2_VSC_SCRN_TO_CH_SHIFT) & V_VS2_VSC_SCRN_TO_CH_MASK);
    vs2WriteByte(vd, V_VS2_VSC_MODE_REG, vscMode);

    /* The channel value is placed in the channel map register */
    vs2WriteByte(vd, V_VS2_VSC_CHANNEL_MAP, channelVal);

    /* Disable setting of screen-to-channel assignments */
    vs2RegisterClearBit(vd, V_VS2_VSC_MODE_REG, V_VS2_VSC_SCRN_TBL_MODE_ENABLE);
}


/*
 * Function: vs2WriteDACAlternateRAM
 *
 * Description:  Writes the specified values into DAC's alternate RAM
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * idDAC - specify here the DAC to address.  For example, V_VS2_DAC_0_RED.
 *
 * startCell - the first cell in the primary RAM which should receive
 * the values.  Normally, this is 0.
 *
 * cellCount - the number of cells to write.
 *
 * value - the address of the contiguous set of values to write to the
 * DAC register.  You must supply a pointer to cellCount bytes to write.
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2WriteDACAlternateRAM(VS2Dev *vd,
				    paddr_t idDAC,
				    int startCell,
				    int cellCount,
				    u_char *value)
{
    /* Write the cell number into the address registers */
    writeDACAddress(vd, idDAC, startCell);

    /* Write the requested number of cells */
    while (cellCount--) {
	/*
	 * Write the cell data into the DAC.  Note the auto-increment to look
	 * at the next item of user data.  The DAC itself will auto-increment
	 * the address of the RAM location to be written.
	 */
	vs2WriteByte(vd, (off_t)(idDAC + V_VS2_DAC_ALTERNATE_TABLE), *value++);
    }
}

/*
 * Function: vs2WriteDACCommandRegister
 *
 * Description:  Writes the specified value into the specified DAC MPU register
 *
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * idDAC - specify here the DAC to address.  For example, V_VS2_DAC_0_RED.
 *
 * regDAC - specify here the register within the DAC which should
 * receive the value.
 *
 * value - the value to write to the DAC register
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2WriteDACCommandRegister(VS2Dev *vd,
					paddr_t idDAC,
					paddr_t regDAC,
					u_char value)
{
    writeDACAddress(vd, idDAC, (int)regDAC);
    vs2WriteByte(vd, (off_t)(idDAC + V_VS2_DAC_REGISTER), value);
}

/*
 * Function: vs2WriteDACPrimaryRAM
 *
 * Description:  Writes the specified values into DAC's primary RAM
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * idDAC - specify here the DAC to address.  For example, V_VS2_DAC_0_RED.
 *
 * startCell - the first cell in the primary RAM which should receive
 * the values.  Normally, this is 0.
 *
 * cellCount - the number of cells to write.
 *
 * value - the address of the contiguous set of values to write to the
 * DAC register.  You must supply a pointer to cellCount bytes to write.
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
vs2WriteDACPrimaryRAM(VS2Dev *vd,
				    paddr_t idDAC,
				    int startCell,
				    int cellCount,
				    u_char *value)
{
    /* Write the cell number into the address registers */
    writeDACAddress(vd, idDAC, startCell);

    /* Write the requested number of cells */
    while (cellCount--) {
	/*
	 * Write the cell data into the DAC.  Note the auto-increment to look
	 * at the next item of user data.  The DAC itself will auto-increment
	 * the address of the RAM location to be written.
	 */
	vs2WriteByte(vd, (off_t)(idDAC + V_VS2_DAC_GAMMA_TABLE), *value++);
    }
}

/*
 * Function: writeAddress
 *
 * Description:  Writes the address into the the DAC's ADDR_LOW and ADDR_HIGH
 * registers.
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * idDAC - specify here the DAC to address.  For example, V_VS2_DAC_0_RED.
 *
 * addr - the 16-bit address to write
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static void
writeDACAddress(VS2Dev *vd,  paddr_t idDAC,  int addr)
{
    vs2WriteByte(vd, (off_t)(idDAC + V_VS2_DAC_ADDR_LOW), addr & 0xff);
    vs2WriteByte(vd, (off_t)(idDAC + V_VS2_DAC_ADDR_HIGH), (addr >> 8) & 0xff);
}


typedef struct {
    VS2Dev         *vd;
    off_t           reg;
    u_char          bit;
    u_char          currentSetting;
}               DPOTDesc;


#if 0
/*
 * Caller can access only one dpot at a time.  This boolean assures
 * open/close pairings.
 */
static DPOTDesc openDPOT;



/*
 * Function: vs2DPOTSet
 *
 * Description:  Sets the DPOT to the specified value.  The DPOT must have
 * been accessed with vs2DPOTSelect
 *
 *
 * Parameters:
 *
 * newValue - the new value of the DPOT.  Must be in [0..V_VS2_DPOT_RANGE].
 *
 *
 *
 * Return:  (void)
 */
/* ARGSUSED */
static void
vs2DPOTSet(u_char newValue)
{
    int             delta = newValue - openDPOT.currentSetting;
    int             absDelta = ABS(delta);

    /* Set the UP/DOWN direction depending on what is required. */
    if (delta > 0)
	/* The DPOT value must be increased */
	vs2RegisterClearBit(openDPOT.vd, V_VS2_REGISTER2, V_VS2_DPOT_UD);
    else
	/* The DPOT value must be decreased */
	vs2RegisterSetBit(openDPOT.vd, V_VS2_REGISTER2, V_VS2_DPOT_UD);

    /* Wait the hardware-mandated time before proceeding.  (See VS2 spec.) */
    vs2Delay(3);

    /* Move the DPOT in the required direction */
    while (absDelta--)
	vs2ToggleSerialClock(openDPOT.vd);

    /* Update the current value */
    openDPOT.currentSetting = newValue;
}
#endif

static unsigned char edgeAddr[] = {
    VS2_VOF_CSYNC_0,
    VS2_VOF_CSYNC_1,
    VS2_VOF_CSYNC_2,
    VS2_VOF_CSYNC_3,
    VS2_VOF_CSYNC_4,
    VS2_VOF_CSYNC_5,
    VS2_VOF_CSYNC_6,
    VS2_VOF_CBLANK_0,
    VS2_VOF_CBLANK_1,
    VS2_VOF_CBLANK_2,
    VS2_VOF_STEREO,
    VS2_VOF_HCURSOR_0,
    VS2_VOF_HCURSOR_1,
    VS2_VOF_HCURSOR_2,
    VS2_VOF_HCURSOR_3,
    VS2_VOF_VCURSOR_0,
    VS2_VOF_VCURSOR_1,
    VS2_VOF_VID_ADDR_REQ_0,
    VS2_VOF_VID_ADDR_REQ_1,
    VS2_VOF_ACTIVE_VIDEO_0,
    VS2_VOF_ACTIVE_VIDEO_1,
    VS2_VOF_VWALK_0,
    VS2_VOF_VWALK_1,
    VS2_VOF_HWALK_0,
    VS2_VOF_HWALK_1,
};

/*
 * Function:	 venice_clock_vs2_serial_dataBit()
 *
 * Returns: 	 void
 *
 * Clocks the given parameter (a '1' or a '0') into the VS2's serial data
 * pipeline by first latching it into the serial data register, then by forcing
 * a low -> high transition on the VS2's serial clock input with a write to
 * the dedicated register V_VS2_SERIAL_CLOCK.
 *
 * Writes to V_VS2_SERIAL_CLOCK will strobe the clock low for 5 microseconds,
 * causing any xilinx in configuration mode to accept the serial data presented
 * on its serial data input.
 */
/* ARGSUSED */
static void
venice_clock_vs2_serial_dataBit(VS2Dev *vd, int value)
{
    /*
     * Latch the data, then write to the serial clock (value written to the
     * clock doesn't matter).
     */
    vs2WriteByte(vd, (off_t)V_VS2_SERIAL_DATA, value);
    vs2WriteByte(vd, (off_t)V_VS2_SERIAL_CLOCK, 1);
}

/*
 * Configure a signal edge in VOF edge memory (if it is active).
 *
 * The vofc compiler will set the uppermost bit of the edge hcount
 * if the signal edge does not exist on this line for this format.
 */
/* ARGSUSED */
static void
set_edgeMem_for_edge(VS2Dev *vd, int line, int edge, unsigned short hcount,
			off_t xilinxBase)
{
    unsigned char highbyte;

    if (hcount != DEFHCNT) {

	/*
	 * Write 13 bits of edge memory address (lower byte, then upper
	 * byte).  Then write to the edge memory and edge data
	 * registers to latch this signal edge into memory at the
	 * desired locations for this (linetype, hcount).
	 */

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		(VS2_EDGE_LOW_EDGEMEM_ADDR | VS2_VOF_STOP));

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VERT_SEQ_COUNTER,
		(hcount & 0xff));

	highbyte = ( (line << 1) | ((hcount >> 8) & 0x1) );

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		(DG_EDGE_HIGH_EDGEMEM_ADDR | DG_VOF_STOP));

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VERT_SEQ_COUNTER,
		highbyte);

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_EDGE_MEMORY_ADDR,
		edgeAddr[edge]);

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_EDGE_MEMORY_DATA,
		0xff);
    }
}

/*
 * Load the edge memory.
 */
/* ARGSUSED */
static void
venice_load_vs2_vof_edgeMem(VS2Dev *vd, off_t xilinxBase,
					vof_data_t *vofData)
{
    int line, edge, length;
    unsigned char highbyte;

    /*
     * Write all possible edge position values (compiler will set hcount to
     * 0x8000 for any line types's unused edges).  Do the following algorithm
     * to clear edge memory:
     *
     * for all possible line types (0..15)
     *
     *  for all possible line lengths (0..511)
     *
     *    setup 13 bit edge memory address with a pair of writes to the
     *    V_VS2_VERT_SEQ_COUNTER register.  The edge memory address is
     *    composed as:
     *
     *    upper byte <4..0> = line type <3..0>, hcount <8>
     *    lower byte <7..0> = hcount <7..0>
     *
     *    The upper byte, lower byte edge memory address register latching is
     *    controlled by a bit in the V_VS2_VOF_CONTROL_REG register.
     *
     *    for all possible signal edges (0..24)
     *
     *	    write edge memory address register with signal edge value (0..24)
     *      if (edge is active for this line type, signal edge, & hcount)
     *	      write 0xff to edge memory data register
     *      else
     *	      write 0x0 to edge memory data register
     *    end
     *  end
     *
     * Then load specific edges by writing their hcount values (and the line
     * types where they occur) into the edge memory directly.
     */

    /*
     * Clear the edge memories first.
     */
    for (line = 0; line < VOF_EDGE_DEFINITION_SIZE; line++) {
      for (length = 0; length <= 511; length++) {
	/*
	 * Write 13 bits of edge memory address (lower byte, then upper byte).
	 */
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		(VS2_EDGE_LOW_EDGEMEM_ADDR | VS2_VOF_STOP));
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VERT_SEQ_COUNTER,
		(length & 0xff));

	highbyte = ( (line << 1) | ((length >> 8) & 0x1) );

	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		(VS2_EDGE_HIGH_EDGEMEM_ADDR | VS2_VOF_STOP));
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VERT_SEQ_COUNTER, highbyte);

	for (edge = 0; edge < 25; edge++) {
	    /*
	     * The vofc compiler will set the uppermost bit of the edge hcount
	     * if the signal edge does not exist on this line for this format.
	     *
	     * Thus, we can simply test for equality between the signal edge
	     * hcount (read in from the VOF ucode file) and the length count
	     * indexed above).  If they are equal, we have an edge, and we need
	     * to set the edge memory bit.  Otherwise, we don't, and we must
	     * clear the edge memory bit.
	     *
	     * We only call vs2WriteXilinxReg to latch in an edge's hcount
	     * when an active edge is encountered.
	     */

	    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_EDGE_MEMORY_ADDR,
		edgeAddr[edge]);
	    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_EDGE_MEMORY_DATA, 0);
	}
      }
    }

    /*
     * Now set the active edges.
     */
    for (line = 0; line < VOF_EDGE_DEFINITION_SIZE; line++) {

	for (edge = 0; edge < 25; edge++) {
	    set_edgeMem_for_edge(vd, line, edge,
		vofData->signal_edge_table[line][edge].edge[0], xilinxBase);
	    set_edgeMem_for_edge(vd, line, edge,
		vofData->signal_edge_table[line][edge].edge[1], xilinxBase);
	    set_edgeMem_for_edge(vd, line, edge,
		vofData->signal_edge_table[line][edge].edge[2], xilinxBase);
	    set_edgeMem_for_edge(vd, line, edge,
		vofData->signal_edge_table[line][edge].edge[3], xilinxBase);

	}
    }
}

/*
 * Load:	the vertical state duration table,
 * 		the line type table,
 * 		the line length table.
 *
 * V_VS2_VERT_SEQ_COUNTER serves as the address register for the line type
 * lut memory.
 *
 * The line length memory is addressed by the output of the line type
 * memory, which is written to via the V_VS2_VERT_SEQ_COUNTER register.
 *
 * So, we first use the line type lut to indirectly address the line
 * length lut memory, then write the line type memory entry.
 *
 * Afterwards, we load the line type lut memory (whose contents have been
 * trashed by the line length memory load) and the vertical state memory.
 *
 * There are VOF_STATE_TABLE_SIZE possible vertical states, but only
 * VOF_EDGE_DEFINITION_SIZE line types,
 * so there are:
 *
 *    VOF_STATE_TABLE_SIZE possible vertical state durations,
 *    VOF_STATE_TABLE_SIZE possible line type lut entries,
 *    VOF_EDGE_DEFINITION_SIZE possible line lengths.
 */
/* ARGSUSED */
static void
venice_load_vs2_vof_verticalInfo(VS2Dev *vd, off_t xilinxBase,
					vof_data_t *vofData)
{
    int i;
    unsigned char low, high;

    /*
     * Make sure that writes to the V_VS2_VERT_SEQ_COUNTER go to the correct
     * address latch (should be the 'low' byte).
     */
    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
	(VS2_EDGE_LOW_EDGEMEM_ADDR | VS2_VOF_STOP));

    for (i = 0; i < VOF_STATE_TABLE_SIZE; i++) {
	/*
	 * Setup address into the line type memory.
	 */
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VERT_SEQ_COUNTER, i);

	/*
	 * Load line length memory first.
	 */
	if (i < VOF_EDGE_DEFINITION_SIZE) {
	    /*
	     * Setup address into the line length memory by writing into the
	     * line type memory.
	     */
	    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_TYPE_LUT, i);
	    /*
	     * Finally, write the line length info into both chunks.
	     */
	    {
                int             lineLength;

                /*
                 * First subtract one (1) vid clock to account for the XILINX
                 * implementation that counts from zero (instead of one).
                 *
                 * Do not make this change for any zero length line lengths.
                 */
                lineLength = vofData->line_length_table[i];
                if (lineLength != 0)
                    lineLength--;
                high = ((lineLength >> 8) & 0x1);
                low = (lineLength & 0xff);
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_LENGTH_LOW, low);
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_LENGTH_HIGH, high);
            }

	    if (i < 8) {
		/*
		 * Write the display_screen look up table, by flipping a
		 * control register bit.  XXX use a real #define from venice.h
		 * XXXX is this necessary for VS2?
		 */
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		  (VS2_DSLUT_SELECT|VS2_EDGE_LOW_EDGEMEM_ADDR|VS2_VOF_STOP));

		high = ((vofData->display_screen_table[i] >> 8) & 0xf);
		low = (vofData->display_screen_table[i] & 0xff);
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_LENGTH_LOW, low);
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_LENGTH_HIGH, high);
		/*
		 * Restore the register bit setting, so that the next
		 * line length lookup table write will succeed.
		 */
		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG,
		    (VS2_EDGE_LOW_EDGEMEM_ADDR | VS2_VOF_STOP));
	    }
	}
	/*
	 * Now write the real line type lut value for the i'th location,
	 * followed by the line duration value (number of times to repeat this
	 * line definition when iterating through a video state transition
	 * sequence).
	 */
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_TYPE_LUT,
	    (vofData->line_type_table[i] & 0xf));
	vs2WriteXilinxReg(vd, xilinxBase, V_VS2_LINE_DURATION,
	    vofData->state_duration_table[i] );
    }
}

/*
 * Base pixel clock period, in picoseconds (1/107.352Mhz)
 *
 * We use a dual modulus prescaler, which divides the raw frequency by
 * DIVIDE_VAL.
 */
#define BASE_PERIOD 9315
#define ACOUNTERSIZE 7	/* # of bits in divide by A counter */
#define NCOUNTERSIZE 10	/* # of bits in divide by N counter */

#define DIVIDE_VAL      20

/*
 * The VS2 only has one programmable clock for standalone genlock operation;
 * either channel 0 or channel 1 may designate itself as the "master" and lock
 * to it; the other channel must framelock to the master in a "slave" genlock
 * mode.
 */
/* ARGSUSED */
static void
venice_load_vs2_clockDivider(VS2Dev *vd, int hrate_picosec)
{
    int dividerVal, Nval, Aval;
    int i;

    dividerVal = (hrate_picosec + (BASE_PERIOD / 2)) / BASE_PERIOD;
    Nval = dividerVal / DIVIDE_VAL;
    Aval = dividerVal - (Nval * DIVIDE_VAL);

    /*
     * The clock modulus value 'Aval' can be no greater than the value to be
     * loaded into the divider register 'Nval'.
     */
    if (Aval > Nval) {
	if (((Aval - Nval)) > (DIVIDE_VAL / 2)) {
	    Nval++;
	    Aval = 0;
	} else {
	    /*
	     * The part would clamp Aval to Nval anyway, even if we programmed
	     * a higher value.
	     */
	    Aval = Nval;
	}
    }

    /*
     * Disable the clock divider latch while we shift in data.
     */
    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_DG_H_DIV_EN);

    for (i = NCOUNTERSIZE; i > 0; i--)
    {
	if(Nval & (1<<(i-1))) {
	    venice_clock_vs2_serial_dataBit(vd, 1);
	} else {
	    venice_clock_vs2_serial_dataBit(vd, 0);
	}

    }

    for (i = ACOUNTERSIZE; i > 0; i--)
    {
	if(Aval & (1<<(i-1))) {
	    venice_clock_vs2_serial_dataBit(vd, 1);
	} else {
	    venice_clock_vs2_serial_dataBit(vd, 0);
	}
    }
    /*
     * Clock in a control bit indicating that we wish to load the
     * divide by N, divide by A registers.
     */
    venice_clock_vs2_serial_dataBit(vd, 0);

    /*
     * Set, then clear the clock divider's latch control bit to latch the data.
     */
    vs2RegisterSetBit(vd, V_VS2_REGISTER2, V_DG_H_DIV_EN);
    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_DG_H_DIV_EN);
}

/*
 * Loads VS2 board specific parameters into various VS2 registers.
 *
 * Note - to appropriately configure all VS2 registers (e.g.,
 * HRATE_SEL, VS2 genlock master), you must also call vs2InitRegs
 * with a correctly-populated eeprom.
 */
/* ARGSUSED */
static int
venice_loadsection_vs2_top(VS2Dev *vd, off_t xilinxBase, int isMasterOfVS2,
			int doGenlock, int genlockTTL,
			unsigned int *param_block)
{

    /*
     * Set the programmable clock to the standard 1280 x 1024 60hz value,
     * because the edge memory write cycles must use a video clock whose
     * period*5 is >= 300ns.  Most clock rates are okay, only NTSC is slow
     * enough (40ns * 5 = 200ns) to cause edge memory loading problems.
     *
     * Wait for a minimum of 10 msec so that the internal PLL can settle.
     *
     * Once the edge memory has been loaded, we set the clock frequency
     * back to the proper value before enabling video.
     */
    vs2WriteProgClock(vd, xilinxBase, 0x14);
    sginap(2);

    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	vs2_vof0_clock_value = param_block[DG2_PROG_CLK];
    } else {
	vs2_vof1_clock_value = param_block[DG2_PROG_CLK];
    }

    /*
     * Reset VS2 dac clocks.  VSC xilinx will generate a pulse during the
     * vertical interval (data value is a don't care).
     */
    vs2WriteByte(vd, (off_t)V_VS2_DAC_CLOCK_RESET, 0);

    /*
     * Program the clock divider chip with the two divider values
     * required for this format.  There is only one clock divider, to be used
     * by the master vof.
     */
    if (isMasterOfVS2) {
	venice_load_vs2_clockDivider(vd, param_block[DG2_N_DIVIDER_LINERATE]);
    }

    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	vs2_vof0_control_reg_value = param_block[DG2_VOF_CONTROL_REG];
    } else {
	vs2_vof1_control_reg_value = param_block[DG2_VOF_CONTROL_REG];
    }

    /*
     * Program the VIF registers with values read in from the board
     * parameter section.  Shadow all VIF register settings in the kernel,
     * so that users can go back and forth from internal to external lock.
     *
     * Program the VIF as if we were going to do an external genlock.
     *
     * We don't make the decision to go internal vs external until
     * venice_loadsection_vs2_body() gets invoked, so we shadow the register
     * values in the kernel, and in variables local to this module.
     *
     * The slave VS2 vof must always do an external TTL level genlock to the
     * master VS2 vof.
     *
     */
    if (!isMasterOfVS2) {
	param_block[DG2_VIF_GENSRC_CNTRL] |= DG2_4V_LEVEL_SYNC_SOURCE;
    }

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_SYNC_WIDTH,
	param_block[DG2_VIF_SYNC_WIDTH]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_HPHASE_LOOPGAIN,
	param_block[DG2_VIF_HPHASE_LOOPGAIN]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_MCOUNTER,
	param_block[DG2_VIF_MCOUNTER]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_HORZ_LENGTH,
	param_block[DG2_VIF_HORZ_LENGTH]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_GENSRC_CNTRL,
	param_block[DG2_VIF_GENSRC_CNTRL]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_BPORCH_CLAMP,
	param_block[DG2_VIF_BPORCH_CLAMP]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_FINE_PHASE_ADJ,
	param_block[DG2_VIF_FINE_PHASE_ADJ]);

    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_HORZ_LENGTH_8MSB,
	param_block[DG2_VIF_MAGIC_HIGH]);

    /*
     * Save the internal genlock params to be used when
     * venice_loadsection_vs2_body() makes the decision to tweak  the vif
     * registers to go from external genlock (which we've set up here) to
     * internal genlock.
     */

    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	vs2_vif0_sync_width_internal = param_block[DG2_VIF_SYNC_WIDTH_INT];
	vs2_vif0_gensrc_cntrl_internal = param_block[DG2_VIF_GENSRC_CNTRL_INT];
	vs2_vif0_bporch_clamp_internal = param_block[DG2_VIF_BPORCH_CLAMP_INT];
    } else {
	vs2_vif1_sync_width_internal = param_block[DG2_VIF_SYNC_WIDTH_INT];
	vs2_vif1_gensrc_cntrl_internal = param_block[DG2_VIF_GENSRC_CNTRL_INT];
	vs2_vif1_bporch_clamp_internal = param_block[DG2_VIF_BPORCH_CLAMP_INT];
    }


    return(0);
}

/* ARGSUSED */
static int
venice_loadsection_vs2_body(VS2Dev *vd, off_t xilinxBase, int isMasterOfVS2,
				int doGenlock, int genlockTTL,
				vof_data_t *vofData)
{
    /*
     * Set the VOFSTOP bit in the control register; this should be done on
     * the VS2 vlist to avoid screwing up the video fetches from the FIFOs.
     */
    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG, DG_VOF_STOP);

    /*
     * Load the edge memory.
     */
    venice_load_vs2_vof_edgeMem(vd, xilinxBase, vofData);

    /*
     * Load:	the vertical state duration table,
     * 		the line type table,
     * 		the line length table.
     */
    venice_load_vs2_vof_verticalInfo(vd, xilinxBase, vofData);

    /*
     * Load in the VIF register settings read in from the VOF.  Choose one of
     * (internally genlocked)(externally genlocked) based upon the incoming
     * vof_file_info.flags word.  Users can subsequently use a libvs2 function
     * to go back and forth from internal to external genlock, since we
     * shadow the necessary vif settings (via a kernel ioctl) below.
     */
    if (doGenlock) {
	vofData->vof_file_info.flags |= VOF_F_DO_GENLOCK;
    }

    /* 
     * A note from Jeff Milo:
     * 
     * 
     * THIS IS THE WAY IT IS .....
     *
     * IF externally genlocking (only master can) AND input is 4V sync THEN set high
     * the genlk_vifN  bit where N=0 or 1
     *
     * IF internally locking (to horizontal timebase or slaving to master) set low the
     * genlk_vifN bit  where N=0 or 1
     *
     *
     *                              Jeff
     */
    if (isMasterOfVS2) {
	if (vofData->vof_file_info.flags & VOF_F_DO_GENLOCK) {
	    /*
	     * lock to incoming sync
	     */
	    if (genlockTTL) {
		if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
		    vs2RegisterSetBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF0);
		} else {
		    vs2RegisterSetBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF1);
		}
	    } else {
		if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
		    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF0);
		} else {
		    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF1);
		}
	    }
	} else {
	    /*
	     * lock to internal hrate
	     */
	    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
		vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF0);
		/*
		 * Write the three VIF registers that differentiate
		 * between internal and external lock.
		 */

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_SYNC_WIDTH,
		    vs2_vif0_sync_width_internal);

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_GENSRC_CNTRL,
		    vs2_vif0_gensrc_cntrl_internal);

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_BPORCH_CLAMP,
		    vs2_vif0_bporch_clamp_internal);
	    } else {
		vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF1);
		/*
		 * Write the three VIF registers that differentiate
		 * between internal and external lock.
		 */

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_SYNC_WIDTH,
		    vs2_vif1_sync_width_internal);

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_GENSRC_CNTRL,
		    vs2_vif1_gensrc_cntrl_internal);

		vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VIF_BPORCH_CLAMP,
		    vs2_vif1_bporch_clamp_internal);
	    }
	}
    } else {
	/*
	 * Slave VOF does a lock to the master.
	 */
	if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF0);
	} else {
	    vs2RegisterClearBit(vd, V_VS2_REGISTER2, V_VS2_GENLK_VIF1);
	}
    }

    /*
     * Set the clock value to its real value after the edge memories
     * have been loaded, but before the vof is started.  Wait for
     * a minimum of 10 msec so that the internal PLL can settle.
     */
    if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	vs2WriteProgClock(vd, xilinxBase, vs2_vof0_clock_value);
    } else {
	vs2WriteProgClock(vd, xilinxBase, vs2_vof1_clock_value);
    }
    sginap(2);

    /*
     * Only one vof can be master, and thus lock to the external sync and/or
     * the internal hrate reference.
     */
    if (isMasterOfVS2) {
	if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	    vs2RegisterSetBit(vd, V_VS2_REGISTER4, V_VS2_HRATE0_SEL);
	    vs2RegisterClearBit(vd, V_VS2_REGISTER4, V_VS2_HRATE1_SEL);
	} else {
	    vs2RegisterClearBit(vd, V_VS2_REGISTER4, V_VS2_HRATE0_SEL);
	    vs2RegisterSetBit(vd, V_VS2_REGISTER4, V_VS2_HRATE1_SEL);
	}
    } else {
	if (xilinxBase == V_VS2_VIF0_XILINX_BASE) {
	    vs2RegisterClearBit(vd, V_VS2_REGISTER4, V_VS2_HRATE0_SEL);
	    vs2RegisterSetBit(vd, V_VS2_REGISTER4, V_VS2_HRATE1_SEL);
	} else {
	    vs2RegisterSetBit(vd, V_VS2_REGISTER4, V_VS2_HRATE0_SEL);
	    vs2RegisterClearBit(vd, V_VS2_REGISTER4, V_VS2_HRATE1_SEL);
	}
    }

    return 0;
}

/*
 * Function: loadVS2VOFFromData
 *
 * Description:  Loads a VS2 VOF from data supplied to it.
 *
 * Parameters:
 *
 * vd - the VS2DEV structure containing information about the opened VS2 board.
 *
 * xilinxBase - the XILINX to load
 *
 * isMasterOfVS2 - boolean specifying whether this VOF is the genlock master.
 *
 * doGenlock - boolean specifying whether genlock should be performed.
 *
 * voftop - pointer to the first element of the vof/vif parameter array.
 *
 * vofbody - pointer to the edge table
 *
 * Return:  (void)
 */
/* ARGSUSED */
static int
loadVS2VOFFromData(VS2Dev *vd, off_t xilinxBase, int isMasterOfVS2,
			      int doGenlock, int genlockTTL, unsigned int *voftop, vof_data_t *vofbody)
{
    /*
     * Stop the VOF.  Eventually, this must be done on the VS2 vlist!
     */
    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG, VS2_VOF_STOP);

    venice_loadsection_vs2_top(vd, xilinxBase, isMasterOfVS2, doGenlock, genlockTTL, voftop);

    /*
     * NOTE: Venice doesn't have an explicit TTL sync control (yet); it's
     * part of the VIF registers which are in the vof board param section.
     */
    venice_loadsection_vs2_body(vd, xilinxBase, isMasterOfVS2, doGenlock, genlockTTL, vofbody);

    /*
     * Again, this should be done on the VS2 vlist; we should also use the
     * vof_control_reg value stored in the board parameter section.
     */
    vs2WriteXilinxReg(vd, xilinxBase, V_VS2_VOF_CONTROL_REG, VS2_VOF_RUN);

    return 0;
}


#define CONFIG_MEM_TIMEOUT	1000

/*
 * Allocate a static VS2Dev pointer, so that functions within this module
 * can access the VS2 device without requiring a VS2Dev * parameter.  This is
 * done because the functions herein must be passed in as function pointers to
 * the existing libxilinx interface, which does not support the VS2Dev concept.
 *
 * The pointer gets set whenever a top level routine (such as load/verify)
 * gets called.
 */
static VS2Dev global_vd;
static VS2Dev *global_vdp;

/*
 * Function:	 venice_clock_vs2_serial_dataByte()
 *
 * Returns: 	 void
 *
 * Clocks 8 bits in from the given parameter into the VS2's serial data
 * pipeline by first latching it into the serial data register, then by forcing
 * a low -> high transition on the VS2's serial clock input (wired up to all
 * xilii's CCLK) with a write to the dedicated register V_VS2_SERIAL_CLOCK.
 *
 * Writes to V_VS2_SERIAL_CLOCK will strobe the clock low for 5 microseconds,
 * causing any xilinx in configuration mode to accept the serial data presented
 * on its serial data input.
 *
 * Only those xilii which have been put into the 'program' state will accept
 * the configuration data.  This function is the same for all xilii on the VS2.
 */
/* ARGSUSED */
static void
venice_clock_vs2_serial_dataByte(int value)
{
    int i;

    for(i = 0; i < 8; i++) {
	/*
	 * Latch the data, then write to the serial clock.
	 */
        vs2WriteByte(global_vdp, (off_t)V_VS2_SERIAL_DATA, (value & 0x1));
	vs2WriteByte(global_vdp, (off_t)V_VS2_SERIAL_CLOCK, 1);
	value >>= 1;
    }
}

/*
 * Function:	 venice_wait_vs2_vif0_configMem_clear()
 *
 * Returns: 	 1 if configuration memory clearing is successful,
 *		-1 otherwise.
 *
 * We poll this 4000 series xilinx part until the INIT pin goes high,
 * which indicates that the memory clear operation has completed.
 *
 * Also, after the INIT pin goes high, you must wait 4 usec before issuing the
 * first CCLK to the 4000 series parts (this is NOT in the data book yet).
 */
static int
venice_wait_vs2_vif0_configMem_clear(void)
{
    int timeout = CONFIG_MEM_TIMEOUT;

    /*
     * We might want to put in a timer mechanism here, but it would have to
     * work for both operational sw & prom sw.  The idea would be to time out
     * the config memory clear operation, so that this loop doesn't hang
     * forever.
     */
    while (!(vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VIF0_INH_L) && --timeout) {
	vs2Delay(2);
    }
    if (timeout == 0)
	return(XILINX_CONFIG_CLEAR_ERR);
    vs2Delay(5);
    return(1);
}

/*
 * Function:	 venice_write_vs2_vif0_program()
 *
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VS2 VIF0's PROG pin.
 */
/* ARGSUSED */
static void
venice_write_vs2_vif0_program(int value)
{
    if (value) {
	vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF0_PROG_L);
    } else {
	vs2RegisterClearBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF0_PROG_L);
    }
}

/*
 * Function:	 venice_check_vs2_vif0_frame_status()
 *
 * Returns: 	 int
 *
 * Reads from the VS2 VIF0's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame has been latched
 * successfully.
 */
static int
venice_check_vs2_vif0_frame_status(void)
{
    if (vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VIF0_INH_L) {
	return(1); /* INIT (frame okay) is an active high signal */
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_read_vs2_vif0_done()
 *
 * Returns: 	 int
 *
 * Reads the VS2 VIF0's DONE status, and returns this value.
 */
static int
venice_read_vs2_vif0_done(void)
{
    unsigned char doneStatus;

    doneStatus = vs2ReadStatus(global_vdp, V_VS2_STATUS_0);

    if (doneStatus & V_VS2_VIF0_DONEP) {
	return(1);
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_load_vs2_vif0_xilinx()
 *
 * Returns: 	 error code values from xilinx.h
 *
 * The VIF (Video Input Formatter) is a 4003 series part.
 *
 * This function is declared as a static; all xilinx loads should be done
 * by calling the top level function venice_load_vs2_xilinx().
 */
/* ARGSUSED */
static int
venice_load_vs2_vif0_xilinx(unsigned char *buffer)
{
    /*
     * Park the RDBK.TRIG input high before initiating the load; readbacks
     * are triggered by a high to low transition on this pin.
     */
    vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF0_RB_TRIG);

    return load_4000_serial_byte(
	venice_write_vs2_vif0_program,
	venice_wait_vs2_vif0_configMem_clear,
	venice_clock_vs2_serial_dataByte,
	venice_check_vs2_vif0_frame_status,
	venice_read_vs2_vif0_done,
	XC4003,
	buffer);
}

/*
 * Function:	 venice_wait_vs2_vif1_configMem_clear()
 *
 * Returns: 	 1 if configuration memory clearing is successful,
 *		-1 otherwise.
 *
 * We poll this 4000 series xilinx part until the INIT pin goes high,
 * which indicates that the memory clear operation has completed.
 *
 * Also, after the INIT pin goes high, you must wait 4 usec before issuing the
 * first CCLK to the 4000 series parts (this is NOT in the data book yet).
 */
static int
venice_wait_vs2_vif1_configMem_clear(void)
{
    int timeout = CONFIG_MEM_TIMEOUT;

    /*
     * We might want to put in a timer mechanism here, but it would have to
     * work for both operational sw & prom sw.  The idea would be to time out
     * the config memory clear operation, so that this loop doesn't hang
     * forever.
     */
    while (!(vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VIF1_INH_L) && --timeout) {
	vs2Delay(2);
    }
    if (timeout == 0)
	return(XILINX_CONFIG_CLEAR_ERR);
    vs2Delay(5);
    return(1);
}

/*
 * Function:	 venice_write_vs2_vif1_program()
 *
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VS2 VIF1's PROG pin.
 */
/* ARGSUSED */
static void
venice_write_vs2_vif1_program(int value)
{
    if (value) {
	vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF1_PROG_L);
    } else {
	vs2RegisterClearBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF1_PROG_L);
    }
}

/*
 * Function:	 venice_check_vs2_vif1_frame_status()
 *
 * Returns: 	 int
 *
 * Reads from the VS2 VIF1's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame has been latched
 * successfully.
 */
static int
venice_check_vs2_vif1_frame_status(void)
{
    if (vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VIF1_INH_L) {
	return(1); /* INIT (frame okay) is an active high signal */
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_read_vs2_vif1_done()
 *
 * Returns: 	 int
 *
 * Reads the VS2 VIF1's DONE status, and returns this value.
 */
static int
venice_read_vs2_vif1_done(void)
{
    unsigned char doneStatus;

    doneStatus = vs2ReadStatus(global_vdp, V_VS2_STATUS_0);

    if (doneStatus & V_VS2_VIF1_DONEP) {
	return(1);
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_load_vs2_vif1_xilinx()
 *
 * Returns: 	 error code values from xilinx.h
 *
 * The VIF (Video Input Formatter) is a 4003 series part.
 *
 * This function is declared as a static; all xilinx loads should be done
 * by calling the top level function venice_load_vs2_xilinx().
 */
/* ARGSUSED */
static int
venice_load_vs2_vif1_xilinx(unsigned char *buffer)
{
    /*
     * Park the RDBK.TRIG input high before initiating the load; readbacks
     * are triggered by a high to low transition on this pin.
     */
    vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VIF1_RB_TRIG);

    return load_4000_serial_byte(
	venice_write_vs2_vif1_program,
	venice_wait_vs2_vif1_configMem_clear,
	venice_clock_vs2_serial_dataByte,
	venice_check_vs2_vif1_frame_status,
	venice_read_vs2_vif1_done,
	XC4003,
	buffer);
}

/*
 * Function:	 venice_wait_vs2_vof0_configMem_clear()
 *
 * Returns: 	 1 if configuration memory clearing is successful,
 *		-1 otherwise.
 *
 * We poll this 4000 series xilinx part until the INIT pin goes high,
 * which indicates that the memory clear operation has completed.
 *
 * Also, after the INIT pin goes high, you must wait 4 usec before issuing the
 * first CCLK to the 4000 series parts (this is NOT in the data book yet).
 */
static int
venice_wait_vs2_vof0_configMem_clear(void)
{
    int timeout = CONFIG_MEM_TIMEOUT;

    /*
     * We might want to put in a timer mechanism here, but it would have to
     * work for both operational sw & prom sw.  The idea would be to time out
     * the config memory clear operation, so that this loop doesn't hang
     * forever.
     */
    while (!(vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VOF0_INH_L) && --timeout) {
	vs2Delay(2);
    }
    if (timeout == 0)
	return(XILINX_CONFIG_CLEAR_ERR);
    vs2Delay(5);
    return(1);
}

/*
 * Function:	 venice_write_vs2_vof0_program()
 *
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VS2 VOF0's PROG pin.
 */
/* ARGSUSED */
static void
venice_write_vs2_vof0_program(int value)
{
    if (value) {
	vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF0_PROG_L);
    } else {
	vs2RegisterClearBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF0_PROG_L);
    }
}

/*
 * Function:	 venice_check_vs2_vof0_frame_status()
 *
 * Returns: 	 int
 *
 * Reads from the VS2 VOF0's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame has been latched
 * successfully.
 */
static int
venice_check_vs2_vof0_frame_status(void)
{
    if (vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VOF0_INH_L) {
	return(1); /* INIT (frame okay) is an active high signal */
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_read_vs2_vof0_done()
 *
 * Returns: 	 int
 *
 * Reads the VS2 VOF0's DONE status, and returns this value.
 */
static int
venice_read_vs2_vof0_done(void)
{
    unsigned char doneStatus;

    doneStatus = vs2ReadStatus(global_vdp, V_VS2_STATUS_0);

    if (doneStatus & V_VS2_VOF0_DONEP) {
	return(1);
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_load_vs2_vof0_xilinx()
 *
 * Returns: 	 error code values from xilinx.h
 *
 * The VOF (Video Output Formatter) is a 4005 series part.
 *
 * This function is declared as a static; all xilinx loads should be done
 * by calling the top level function venice_load_vs2_xilinx().
 */
/* ARGSUSED */
static int
venice_load_vs2_vof0_xilinx(unsigned char *buffer)
{
    /*
     * Park the RDBK.TRIG input high before initiating the load; readbacks
     * are triggered by a high to low transition on this pin.
     */
    vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF0_RB_TRIG);

    return load_4000_serial_byte(
	venice_write_vs2_vof0_program,
	venice_wait_vs2_vof0_configMem_clear,
	venice_clock_vs2_serial_dataByte,
	venice_check_vs2_vof0_frame_status,
	venice_read_vs2_vof0_done,
	XC4005,
	buffer);
}

/*
 * Function:	 venice_wait_vs2_vof1_configMem_clear()
 *
 * Returns: 	 1 if configuration memory clearing is successful,
 *		-1 otherwise.
 *
 * We poll this 4000 series xilinx part until the INIT pin goes high,
 * which indicates that the memory clear operation has completed.
 *
 * Also, after the INIT pin goes high, you must wait 4 usec before issuing the
 * first CCLK to the 4000 series parts (this is NOT in the data book yet).
 */
static int
venice_wait_vs2_vof1_configMem_clear(void)
{
    int timeout = CONFIG_MEM_TIMEOUT;

    /*
     * We might want to put in a timer mechanism here, but it would have to
     * work for both operational sw & prom sw.  The idea would be to time out
     * the config memory clear operation, so that this loop doesn't hang
     * forever.
     */
    while (!(vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VOF1_INH_L) && --timeout) {
	vs2Delay(2);
    }
    if (timeout == 0)
	return(XILINX_CONFIG_CLEAR_ERR);
    vs2Delay(5);
    return(1);
}

/*
 * Function:	 venice_write_vs2_vof1_program()
 *
 * Returns: 	 void
 *
 * Writes the given parameter (a '1' or a '0') to the VS2 VOF1's PROG pin.
 */
/* ARGSUSED */
static void
venice_write_vs2_vof1_program(int value)
{
    if (value) {
	vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF1_PROG_L);
    } else {
	vs2RegisterClearBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF1_PROG_L);
    }
}

/*
 * Function:	 venice_check_vs2_vof1_frame_status()
 *
 * Returns: 	 int
 *
 * Reads from the VS2 VOF1's INIT pin, and returns its value.  A 'High' state
 * indicates that the current configuration memory frame has been latched
 * successfully.
 */
static int
venice_check_vs2_vof1_frame_status(void)
{
    if (vs2ReadStatus(global_vdp, V_VS2_STATUS_1) & V_VS2_VOF1_INH_L) {
	return(1); /* INIT (frame okay) is an active high signal */
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_read_vs2_vof1_done()
 *
 * Returns: 	 int
 *
 * Reads the VS2 VOF1's DONE status, and returns this value.
 */
static int
venice_read_vs2_vof1_done(void)
{
    unsigned char doneStatus;

    doneStatus = vs2ReadStatus(global_vdp, V_VS2_STATUS_0);

    if (doneStatus & V_VS2_VOF1_DONEP) {
	return(1);
    } else {
	return(0);
    }
}

/*
 * Function:	 venice_load_vs2_vof1_xilinx()
 *
 * Returns: 	 error code values from xilinx.h
 *
 * The VOF (Video Output Formatter) is a 4005 series part.
 *
 * This function is declared as a static; all xilinx loads should be done
 * by calling the top level function venice_load_vs2_xilinx().
 */
/* ARGSUSED */
static int
venice_load_vs2_vof1_xilinx(unsigned char *buffer)
{
    /*
     * Park the RDBK.TRIG input high before initiating the load; readbacks
     * are triggered by a high to low transition on this pin.
     */
    vs2RegisterSetBit(global_vdp, V_VS2_REGISTER5, V_VS2_VOF1_RB_TRIG);

    return load_4000_serial_byte(
	venice_write_vs2_vof1_program,
	venice_wait_vs2_vof1_configMem_clear,
	venice_clock_vs2_serial_dataByte,
	venice_check_vs2_vof1_frame_status,
	venice_read_vs2_vof1_done,
	XC4005,
	buffer);
}

/*
 * Function:	venice_load_xilinx()
 *
 * Returns:	XILINX_OKAY if load is successful,
 *		encoded error return value (from xilinx.h) otherwise.
 *
 * Pass in the VS2 xilinx to be loaded as a parameter; choose from one of the
 * following enumerated types in vs2.h:
 *
 * VS2_VOF0_XILINX
 * VS2_VOF1_XILINX
 * VS2_VIF0_XILINX
 * VS2_VIF1_XILINX
 * VS2_VOF_GANG_XILII
 * VS2_VIF_GANG_XILII
 *
 * You must also pass in a pair of function pointers; one which will return
 * a pointer to an array containing the xilinx configuration data (when passed
 * the enumerated xilinxType), and one to free the data structure when this
 * function is done with the data.
 */
/* ARGSUSED */
static int
venice_load_vs2_xilinx(VS2Dev *vs2dev, vs2_xilinx_t xilinxType,
    unsigned char	*buf)
{
    int retval;
    unsigned int ptr0, ptr1, ptr2;

    /*
     * Squirrel away a copy of the VS2Dev struct pointer that got passed in,
     * so that functions within this module may use it internally.
     */
    global_vdp = vs2dev;

    ptr0 = *(unsigned int *)(buf + 24);
    ptr1 = *(unsigned int *)(buf + ptr0 + 0);
    ptr2 = *(unsigned int *)(buf + ptr0 + 8);

    switch (xilinxType) {
	case VS2_VOF0_XILINX:
	    retval = venice_load_vs2_vof0_xilinx(buf + ptr2);
	    break;
	case VS2_VOF1_XILINX:
	    retval = venice_load_vs2_vof1_xilinx(buf + ptr2);
	    break;
	case VS2_VIF0_XILINX:
	    retval = venice_load_vs2_vif0_xilinx(buf + ptr1);
	    break;
	case VS2_VIF1_XILINX:
	    retval = venice_load_vs2_vif1_xilinx(buf + ptr1);
	    break;
	case VS2_VOF_GANG_XILII:
	case VS2_VIF_GANG_XILII:
	    retval = -1;
	    break;
	default:
	    return (XILINX_OKAY);
	    /*NOTREACHED*/
	    break;
    }
    return(retval);
}

/*
 * Function: physicalConnect
 *
 * Description:  Performs the physical open of the vs2 device by opening
 * the /dev/mmem fd and maps the device.
 *
 *
 * Parameters:
 *
 * pipeNumber - specifies which graphics pipeline (numbered [0..(maxPipes-1)]
 * in the same manner as SGI uses the X-windows "screen:pipe" construction).
 *
 * vd - the VS2DEV structure to populate with the status of the open.
 *
 *
 * Return:
 *
 * If successful, sets the mmemFD and mappedBase fields in the vd structure;
 * if not successful, vd->mappedBase will be -1 and errno will be set.
 * Returns vd->mappedBase.
 */
/* ARGSUSED */
static paddr_t
physicalConnect(unsigned pipeNumber, VS2Dev *vd)
{
    off_t           vmeAddress;
    switch (pipeNumber) {
    case 0:
    default:
	vmeAddress = VIDEO_VS2_A24_NP_BASE_PIPE0;
	break;
    case 1:
	vmeAddress = VIDEO_VS2_A24_NP_BASE_PIPE1;
	break;
    }

    vd->vmeAddress = vmeAddress;
    vd->mappedBase = vmeAddress;

    return (vd->mappedBase);
}

/*
 * Function: vs2Close
 *
 * Description:  Closes the VS2 device, freeing memory associated with open.
 *
 *
 * Parameters:
 *
 * vmeAddress - VME base address of the VS2 board to open
 *
 *
 * Return:
 *
 * If successful, returns 0.  If not successful, returns -1 and errno
 * will be set.
 */
/* ARGSUSED */
static int
vs2Close(VS2Dev *vd)
{
    int	    returnStatus = 0;

    return(returnStatus);
}

#define VS2_PROM_LOCAL_CLK_TCK	100

/*
 * Function: vs2Delay
 *
 * Description:  Causes a delay of the specified number of usec.
 *
 *
 * Parameters:
 *
 * usec - the number of microseconds to delay
 *
 *
 * Return: (void)
 */
/* ARGSUSED */
static int
vs2Delay(int usec)
{
    int           ticks;

    /*
     * The delay mechanism used here is very coarse, but can be improved
     * later if necessary.  We use a local definition for clock ticks, 
     * because the system defintion now invokes a function, instead of the
     * hardcoded '100' that existed in 4.0.x.
     */
    ticks = MIN(2, usec * 1000000 / VS2_PROM_LOCAL_CLK_TCK);
    sginap(ticks);
    
    return 0;
}

/*
 * Function: vs2Open
 *
 * Description:  Opens the video splitter device and performs
 * minimally-necessary initialization of data structures.  No device
 * initialization is performed.
 *
 * See vs2PROMOpen for an entry point to be used when in PROM.
 *
 *
 * Parameters:
 *
 * pipeNumber - specifies which graphics pipeline (numbered [0..(maxPipes-1)]
 * in the same manner as SGI uses the X-windows "screen:pipe" construction).
 *
 *
 *
 * Return:
 *
 * If successful, returns the address of a VS2Dev structure.  If not
 * successful, returns NULL and errno will be set.
 */
/* ARGSUSED */
static VS2Dev *
vs2Open(unsigned pipeNumber)
{
    VS2Dev         *vd = NULL;

    if (pipeNumber >= V_VS2_MAX_BOARDS)
	pipeNumber = 0;
    vd = &global_vd;
    vd->mappedBase = NULL;
    vd->mmemFD = 0;

    /* Map the device */
    (void) physicalConnect(pipeNumber, vd);
    if (vd->mappedBase == (paddr_t) - 1) {
	vd = NULL;
    }

    return (vd);
}


