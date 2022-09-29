#ident  "IP22diags/video/vino_regtest.c:  $Revision: 1.6 $"

#include "sys/types.h"
#include "sys/immu.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"

#include "vino_diags.h"
#include "vino.h"

#define VINO_PHYS_BASE1	0x00080000
#define VINO_PHYS_BASE2	0

int	num_boards = 0;		/* Number of Vino boards */

VinoBoard   vino_boards[1];	/* stucture of the hardware */

VinoRegs    regs_base;		/* pointer to the PIO registers
				 * of the hardware structure
				 */
caddr_t	    phys_base;		/* another convience pointer */

/*
 * These global variables are used to keep track of the
 * state of the hardware. All of the procedures make sure
 * that the hardware they are about to touch has been
 * initialized.
 */
uchar	board_initialized = FALSE;
uchar	mc_initialized = FALSE;
uchar	DMSD_initialized = FALSE;
uchar	CDMC_initialized = FALSE;

/*
 * The dma tests need to know if both NTSC and PAL are
 * being inputed. If not, the dma tests will fail. This
 * variable is set in vino_7191_init().
 */
uchar	DMSD_no_sources = TRUE;

ulong	CDMC_addr = CDMC_ADDR_ASIC;

/*
 * The test vectors are used by both VINO and the CDMC
 * digital camera to do a "walking one's" test of the
 * registers. This vector has enough values for a 32 bit
 * register.
 */
ulong   vino_test_vec[] = {
    0x1, 0x2, 0x4, 0x8,
    0x10, 0x20, 0x40, 0x80, 
    0x100, 0x200, 0x400, 0x800, 
    0x1000, 0x2000, 0x4000, 0x8000, 
    0x10000, 0x20000, 0x40000, 0x80000, 
    0x100000, 0x200000, 0x400000, 0x800000, 
    0x1000000, 0x2000000, 0x4000000, 0x8000000, 
    0x10000000, 0x20000000, 0x40000000, 0x80000000, 
};


/*
 * common_test - Preforms the walking ones test of a
 * physical address given by addr. This function tests
 * bits start through and including stop. A vector is
 * returned indicating a failed bit by setting the cor-
 * responding bit of the returned value.
 */
ulong
common_test(int start, int stop, caddr_t addr)

{
    int	    loop;
    ulong   result = 0L;
    
    VINO_REG_WRITE_PHYS(addr, 0x0);
    if (VINO_REG_READ_PHYS(addr)&1 != 0x0) {
	msg_printf(ERR, "Cannot write 0x0 at 0x%x\n", addr);
	return 0x1;
    }
    
    for (loop=start; loop<=stop; loop++) {
	VINO_REG_WRITE_PHYS(addr, vino_test_vec[loop]);
	if (VINO_REG_READ_PHYS(addr) != vino_test_vec[loop]) {
	    result |= 0x1<<loop;
	}
    }
    return result;
}


/*
 * vino_probe - Check for the existance of a VINO board by
 * reading 0xb0 from physical address 0x80000. If the MC has
 * not been initialized, vino_initmc() is called. If all tests
 * complete successfully, board_initialized is set to TRUE (1).
 * 
 * 06.18.93 - added a badaddr_val check to cover systems without
 * VINO's. doen't compile
 */
int
vino_probe()
{
    int	    id;
    int	    errcount = 0;
    caddr_t addr = (caddr_t) PHYS_TO_K1(VINO_PHYS_BASE1);
    
    if (mc_initialized == FALSE)
	errcount += vino_initmc();

    vino_boards[num_boards].hw_addr = (VinoRegs) addr;
    
    regs_base = vino_boards[num_boards].hw_addr;
    phys_base  = (caddr_t)regs_base;

    msg_printf(DBG, "Probe for VINO Board addr 0x%x phys_base 0x%x regs_base 0x%x\n",
			addr, phys_base, regs_base);

    /*
     * check for existance of VINO by checking RevID number
     */
    if (badaddr(addr + 4,  sizeof(int)) != 0) {
	msg_printf(ERR, "vino_probe - no device at this address 0x%x\n", addr);
	errcount++;
    }
    else {
	id = VINO_ID_VALUE(VINO_REG_READ_PHYS(phys_base+TEST_REG_REV_ID));
    
	if (id  != VINO_CHIP_ID) {
	    msg_printf(ERR, "Vino board not found at this address 0x%x\n", phys_base);
	    errcount++;
	}
	else
	    board_initialized = TRUE;
    }

    if (errcount != 0)
	sum_error ("VINO probe");
    else
	okydoky ();

    return (errcount);
    /*
     * probe for VINO
     * 
     */
}

/*
 * test all usable bits of the writable PI/O registers
 * this is a shell for the individual tests
 * 
 * Modified 05.28.93 doesn't write to the DMA enable bits
 * of the control register
 */
 
int
vino_regtest()

{
    int	    errcount = 0;
    ulong   err_val = 0x0;
    int	    loop;
    ulong   theMax;
    
    if (board_initialized == FALSE) {
	vino_probe();
    }
    
    /*
     * It would be a VERY VERY BAD thing to set the DMA
     * enable bits as the system would start dma'ing
     * data to random locations in the system's memory
     * posibly making very bad things happen. There for
     * we tip-toe around D7 and D19
     */
    err_val = common_test(0, 6, phys_base+TEST_REG_CONTROL);
    err_val |= common_test(8, 18, phys_base+TEST_REG_CONTROL);
    err_val |= common_test(20, 30, phys_base+TEST_REG_CONTROL);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CONTROL failed 0x%x\n", err_val);
	errcount++;
    }
    
    
    /*
     * Can't test the I2C_CONTROL register because that would start
     * an I2C transaction. The functionality of this register will
     * be verified if the I2C tests pass.
     */


    err_val = common_test(0, 7, phys_base+TEST_REG_CH_A_ALPHA);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_ALPHA failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(0, 7, phys_base+TEST_REG_CH_B_ALPHA);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_ALPHA failed 0x%x\n", err_val);
	errcount++;
    }


    err_val = common_test(0, 27, phys_base+TEST_REG_CH_A_CLIP_START);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_CLIP_START failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(0, 27, phys_base+TEST_REG_CH_B_CLIP_START);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_CLIP_START failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(0, 27, phys_base+TEST_REG_CH_A_CLIP_END);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_CLIP_END failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(0, 27, phys_base+TEST_REG_CH_B_CLIP_END);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_CLIP_END failed 0x%x\n", err_val);
	errcount++;
    }


    err_val = common_test(0, 12, phys_base+TEST_REG_CH_A_FRAME_RATE);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_FRAME_RATE failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(0, 12, phys_base+TEST_REG_CH_B_FRAME_RATE);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_FRAME_RATE failed 0x%x\n", err_val);
	errcount++;
    }


    err_val = common_test(3, 11, phys_base+TEST_REG_CH_A_LINE_SIZE);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_LINE_SIZE failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(3, 11, phys_base+TEST_REG_CH_B_LINE_SIZE);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_LINE_SIZE failed 0x%x\n", err_val);
	errcount++;
    }


    err_val = common_test(3, 11, phys_base+TEST_REG_CH_A_LINE_COUNT);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_LINE_COUNT failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(3, 11, phys_base+TEST_REG_CH_B_LINE_COUNT);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_LINE_COUNT failed 0x%x\n", err_val);
	errcount++;
    }
    
    
    err_val = common_test(3, 11, phys_base+TEST_REG_CH_A_PAGE_INDEX);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_PAGE_INDEX failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(3, 11, phys_base+TEST_REG_CH_B_PAGE_INDEX);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_PAGE_INDEX failed 0x%x\n", err_val);
	errcount++;
    }
    
    /*
     * Can't test next_4_desc because that would initiate
     * a descriptor fetch.
     */
    
    err_val = common_test(4, 31, phys_base+TEST_REG_CH_A_DESC_TBL_PTR);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_DESC_TBL_PTR failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(4, 31, phys_base+TEST_REG_CH_B_DESC_TBL_PTR);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_DESC_TBL_PTR failed 0x%x\n", err_val);
	errcount++;
    }
    
    /*
     * We have to test desc_0 differently than the other descriptors.
     * Setting the jump bit (D30) causes a fetch to be preformed. This
     * is not what we want to happen right now. Test all other bits
     * though.
     */
    err_val = common_test(0, 29, phys_base+TEST_REG_CH_A_DESC_0);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_DESC_0 failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(31, 31, phys_base+TEST_REG_CH_A_DESC_0);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_DESC_0 failed 0x%x\n", err_val);
	errcount++;
    }
    
    err_val = common_test(0, 29, phys_base+TEST_REG_CH_B_DESC_0);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_DESC_0 failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(31, 31, phys_base+TEST_REG_CH_B_DESC_0);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_DESC_0 failed 0x%x\n", err_val);
	errcount++;
    }
    /*
     * The stop bits of ChA and ChB desc_0 are the last bits
     * tested. This results in the interrupt/status register
     * being set. We need to write some value to desc_0 without
     * bits 30 or 31 set and clear the intr_status register.
     * It would be very bad to enable dma at this point.
     */
    VINO_REG_WRITE_PHYS(phys_base+TEST_REG_CH_A_DESC_0, 0x0);
    VINO_REG_WRITE_PHYS(phys_base+TEST_REG_CH_B_DESC_0, 0x0);
    VINO_REG_WRITE_PHYS(phys_base+TEST_REG_INTR_STATUS, 0x0);
    
    /*
     * Test all bits of the other descriptors in both channels
     */
    for (loop=1; loop<4; loop++) {
	err_val = common_test(0, 31, phys_base+TEST_REG_CH_A_DESC_0+((uchar)(0x8*loop)));
	if (err_val) {
	    msg_printf(ERR, "TEST_REG_CH_A_DESC_%d failed 0x%x\n", loop, err_val);
	    errcount++;
	}
	err_val = common_test(0, 31, phys_base+TEST_REG_CH_B_DESC_0+((uchar)(0x8*loop)));
	if (err_val) {
	    msg_printf(ERR, "TEST_REG_CH_B_DESC_%d failed 0x%x\n", loop, err_val);
	    errcount++;
	}
    }
    
    
    err_val = common_test(3, 9, phys_base+TEST_REG_CH_A_FIFO_THRSHLD);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_A_FIFO_THRSHLD failed 0x%x\n", err_val);
	errcount++;
    }
    err_val = common_test(3, 9, phys_base+TEST_REG_CH_B_FIFO_THRSHLD);
    if (err_val) {
	msg_printf(ERR, "TEST_REG_CH_B_FIFO_THRSHLD failed 0x%x\n", err_val);
	errcount++;
    }

    if (errcount != 0)
	sum_error ("VINO PI/O register");
    else
	okydoky ();

    return (errcount);
    /*
     * PI/O register test for VINO
     * 
     */
}


/*
 * vino_get - Return the value of a register specified on
 * the command line. The address is the physical address
 * (0x80XXX) minus the base address (0x80000). If this
 * command is involked with option 'all', all registers are
 * printed out. NOTE - this function does not check to see
 * if the board has been initialized. If a vino_get is ex-
 * ecuted before the board is initialized, a bus error occurs.
 */
int
vino_get(int argc, char **argv)

{
    int	    theReg, theVal;
    int	    loop, stoploop, startloop;
    
    if (argv[1][0] == 'a' &&  argv[1][1] == 'l' && argv[1][2] == 'l') {
	startloop = TEST_REG_REV_ID;
	stoploop = TEST_REG_CH_B_FIFO_WRITE;
    }
    else
	startloop = stoploop = atoi(argv[1]) + TEST_REG_BASE;
    
    msg_printf(DBG, "theReg 0x%x\n", atoi(argv[1]));
    msg_printf(DBG, "startloop 0x%x stoploop 0x%x\n", startloop, stoploop);
    
    for (loop=startloop; loop<=stoploop; loop+=8) {
	theVal = VINO_REG_READ_PHYS(phys_base + loop);
	msg_printf(INFO, "register 0x%x -> 0x%x\n", loop-TEST_REG_BASE, theVal);

    }
    return 0;
}


/*
 * vino_set - Sets the value of a register specified on
 * the command line. The address is the physical address
 * (0x80XXX) minus the base address (0x80000). NOTE -
 * this function does not check to see if the board has
 * been initialized. If a vino_get is executed before the
 * board is initialized, a bus error occurs.
 */
int
vino_set(int argc, char **argv)

{
    int	    theReg, theVal;
    
    theReg = atoi(argv[1]);
    theVal = atoi(argv[2]);
    
    msg_printf(DBG, "theReg is 0x%x theVal is 0x%x\n", theReg, theVal);
    msg_printf(DBG, "the address is 0x%x\n", (phys_base + theReg));
    
    VINO_REG_WRITE_PHYS(phys_base + theReg + TEST_REG_BASE, theVal);
		
    return 0;
}
