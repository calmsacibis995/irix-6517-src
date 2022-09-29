
/***********************************************************************\
*	File:		io4_regtest.c  					*
*									*
*	This is a modified & slightly enhanced version of Bhanu's	*
*	power-on io4 iaid tests.					*
*									*
*	Does pattern read/write for IO4 r/w registers			*
*									*
\***********************************************************************/

#ident "arcs/ide/EVEREST/io4/io4_regtest.c $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/addrs.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <ide_msg.h>
#include <io4_tdefs.h>
#include <prototypes.h>

#define IAID_TESTP	"io4_regtest Passed\n"
#define IAID_TESTF	"FAILED: io4_regtest\n"

static int test_patterns[] = {
    0, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A, 0xFFFFFFFF };

#define	NUM_PATS 6

static IO4cf_regs	io4_regs[]={
{IO4_CONF_LW,            0x2ff},
{IO4_CONF_SW,            0x1fffff00},
{IO4_CONF_ADAP,          0x1ffffff},
{IO4_CONF_INTRVECTOR,    0xffff},
{IO4_CONF_GFXCOMMAND,    0xff},
{IO4_CONF_ETIMEOUT,      0xfffff},
{IO4_CONF_RTIMEOUT,      0xfffff},
{IO4_CONF_INTRMASK,      0x1},
{ (-1), 0}
};

#if 0
static unsigned iodev[2];
static jmp_buf     iaid_buf, *ibuf;
#endif

#if 0
static int io4_check_regs(int);
unchar
#else
int io4_check_regs(int);
int
#endif

io4_regtest(int argc, char **argv)
{
    int retval, slot;
    jmp_buf	iaid_buf;

    retval = 0;
    if (io4_select(FALSE, argc, argv))
	return(1);

    msg_printf (INFO,"\nio4_regtest starting. . .\n");

    /*
     * clear any old fault handlers, timers, etc
     */
    clear_nofault();

    if (setjmp(iaid_buf))
    {
	msg_printf (ERR, "ERROR: io4_regtest - unexpected interrupt\n");
    }
    else
    {
	nofault = (int *)&iaid_buf;

	/*
	 * if slot specifed on command line, test just that slot
	 */
	if (io4_tslot)
	    return (io4_check_regs(io4_tslot));

	/*
	 * iterate through all io adapters in the system
	 */
	for (slot = EV_MAX_SLOTS - 1; slot > 0; slot--) {
	    if (board_type(slot) == EVTYPE_IO4)
		retval |= io4_check_regs(slot);
	}
    }

    /*
     * clear fault handlers again
     */
    clear_nofault();

    msg_printf(INFO, (retval == 0) ? IAID_TESTP : IAID_TESTF);

    return(retval);
	
}
int
io4_check_regs(int slot)
{
    IO4cf_regs	*ioregs;
    uint	writeval; 
    __uint64_t 	readval;
    int		i, errcnt;

    errcnt = 0;

    msg_printf(VRB, "io4_regtest: testing io4 in slot %x\n", slot);
     
    /*
     * save the current register contents
     */
    save_io4config(slot);

    /*
     * try all test patterns
     */
    for (i = 0; i < NUM_PATS; i++) {

	/*
	 * write current pattern to all registers
	 */
	for (ioregs = io4_regs; ioregs->reg_no != (-1); ioregs++){
	    if (ioregs->bitmask0 == 0)
                continue;
	    EV_SET_CONFIG(slot, ioregs->reg_no, test_patterns[i]);
	}

	/*
	 * read back and compare
	 */
	for (ioregs = io4_regs; ioregs->reg_no != (-1); ioregs++){
	    if (ioregs->bitmask0 == 0)
                continue;
	    writeval = test_patterns[i] & ioregs->bitmask0;
	    readval = EV_GET_CONFIG(slot,ioregs->reg_no) & ioregs->bitmask0;
	    if (readval != writeval){
		errcnt++;
		msg_printf(ERR, "ERROR: io4_check_regs - slot %d\n", slot);
		msg_printf(INFO, "  register %x (%s) was %x, expected %x\n",
			ioregs->reg_no, io4_regname(ioregs->reg_no),
			readval, writeval);
		/* print error message */
	    }
	}
    }

    /*
     * restore original config register contents
     */
    restore_io4config(slot);

    return(errcnt);
}

