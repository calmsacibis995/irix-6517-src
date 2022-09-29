#ident "arcs/ide/EVEREST/lib/IO4tlib.c $Revision: 1.1 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/addrs.h>
#include <setjmp.h>
#include "uif.h"
#include <io4_tdefs.h>

/*
 * names of all IO4 registers
 */
static Dev_Regname IO4_Regname[]={
{IO4_CONF_LW,		"IO4_CONF_LW"},
{IO4_CONF_SW,		"IO4_CONF_SW"},
{IO4_CONF_ADAP,		"IO4_CONF_ADAP"},
{IO4_CONF_INTRVECTOR,	"IO4_CONF_INTRVECTOR"},
{IO4_CONF_GFXCOMMAND,	"IO4_CONF_GFXCOMMAND"},
{IO4_CONF_ETIMEOUT,	"IO4_CONF_ETIMEOUT"},
{IO4_CONF_RTIMEOUT,	"IO4_CONF_RTIMEOUT"},
{IO4_CONF_INTRMASK,	"IO4_CONF_INTRMASK"},
{IO4_CONF_IODEV0,	"IO4_CONF_IODEV0"},
{IO4_CONF_IODEV1,	"IO4_CONF_IODEV1"},
{IO4_CONF_IBUSERROR,	"IO4_CONF_IBUSERROR"},
{IO4_CONF_IBUSERRORCLR,	"IO4_CONF_IBUSERRORCLR"},
{IO4_CONF_EBUSERROR,	"IO4_CONF_EBUSERROR"},
{IO4_CONF_EBUSERRORCLR,	"IO4_CONF_EBUSERRORCLR"},
{IO4_CONF_EBUSERROR1,	"IO4_CONF_EBUSERROR1"},
{IO4_CONF_EBUSERROR2,	"IO4_CONF_EBUSERROR2"},
{IO4_CONF_RESET,	"IO4_CONF_RESET"},
{IO4_CONF_ENDIAN,	"IO4_CONF_ENDIAN"},
{IO4_CONF_ETIMEOUT,	"IO4_CONF_ETIMEOUT"},
{IO4_CONF_RTIMEOUT,	"IO4_CONF_RTIMEOUT"},
{IO4_CONF_INTRMASK,	"IO4_CONF_INTRMASK"},
{IO4_CONF_CACHETAG0L,	"IO4_CONF_CACHETAG0L"},
{IO4_CONF_CACHETAG0U,	"IO4_CONF_CACHETAG0U"},
{IO4_CONF_CACHETAG1L,	"IO4_CONF_CACHETAG1L"},
{IO4_CONF_CACHETAG1U,	"IO4_CONF_CACHETAG1U"},
{IO4_CONF_CACHETAG2L,	"IO4_CONF_CACHETAG2L"},
{IO4_CONF_CACHETAG2U,	"IO4_CONF_CACHETAG2U"},
{IO4_CONF_CACHETAG3L,	"IO4_CONF_CACHETAG3L"},
{IO4_CONF_CACHETAG3U,	"IO4_CONF_CACHETAG3U"},
{ (-1),			"Unknown Register"}
};

/*
 * save buffer for io4 configuration register contents
 */
static IO4cf_regs	io4_saveregs[]={
{IO4_CONF_LW,            0},
{IO4_CONF_SW,            0},
{IO4_CONF_ADAP,          0},
{IO4_CONF_INTRVECTOR,    0},
{IO4_CONF_GFXCOMMAND,    0},
{IO4_CONF_ETIMEOUT,      0},
{IO4_CONF_RTIMEOUT,      0},
{IO4_CONF_INTRMASK,      0},
{ (-1), 0}
};


/*
 * passed the number of an io4 register, returns a pointer to the name string
 * simplifies writing consistant log messages
 */
char *
io4_regname(int regnum)
{
    Dev_Regname *curreg;

    curreg = IO4_Regname;
    
    while (curreg->reg_no != regnum) {
	if (curreg->reg_no == -1)
	    break;
	curreg++;
    }

    return (curreg->name);
}

/*
 * save the current IO4 configuration register contents
 */
void
save_io4config(int slot)
{
    IO4cf_regs	*ioregs;

    for (ioregs = io4_saveregs; ioregs->reg_no != (-1); ioregs++){
	/*
	 * kludge to work around the way the SW config register leftshifts
	 * values fed to it by 8 bits and puts board ID in low 8 bits
	 */
	if (ioregs->reg_no != IO4_CONF_SW)
	    ioregs->bitmask0 = EV_GET_CONFIG(slot, ioregs->reg_no);
	else
	    ioregs->bitmask0 = EV_GET_CONFIG(slot, ioregs->reg_no) >> 8;
    }
}

/*
 * restores the IO4 configuration register contents saved by save_io4config()
 *    WARNING - save_io4config() must have been called first!
 */
void
restore_io4config(int slot)
{
    IO4cf_regs	*ioregs;

    for (ioregs = io4_saveregs; ioregs->reg_no != (-1); ioregs++)
	EV_SET_CONFIG(slot, ioregs->reg_no, ioregs->bitmask0);
}
