
/***********************************************************************\
*	File:		pod_iaid.c   					*
*									*
*	Contains the routines used to initialize the IO4 boards and 	*
*	associated IO Adapters (such as the EPC, SCSI, and F chip	*
*	adaptors).  							*
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_iaid.c $Revision: 1.20 $"

#include "pod_fvdefs.h"
#include "libsc.h"
#include "libsk.h"


/* Forward declarations */
int	find_adap(int);
void	io4_saveconf(int);
void	io4_restoreconf(int);
uint	getendian(void);


IO4cf_regs	io4_regs[]={
#if 0
{IO4_CONF_LW,            0x7},
{IO4_CONF_SW,            0xff},
{IO4_CONF_ADAP,          0x1ffffff},
#endif
{IO4_CONF_INTRVECTOR,    0x7fff},
{IO4_CONF_GFXCOMMAND,    0xff},
{IO4_CONF_ETIMEOUT,      0xfffff},
{IO4_CONF_RTIMEOUT,      0xfffff},
{IO4_CONF_INTRMASK,      0x3},
{ (-1), 0}
};

unsigned iodev[2];

#define	PGRP_MASTER	64
#define	IO4ERR_LEVEL	0x3
#define IO4ERR_VECTOR   ((IO4ERR_LEVEL << 8) |  PGRP_MASTER)


/*
 * Function : check_io4
 * Description :
 *	Look at the various configuration registers of the IO4, and 
 *	see if they are in good shape.
 * Parameters:
 *	slot   --  Slot containing the IO4 board.
 *	window --  The small and large window number Where it is located.
 */
static  jmp_buf	iaid_buf;
static	int	saved_io4 = 0;

int
check_io4(unsigned window, unsigned slot)
{
    int result  = 0, level;

    setup_globals(window, slot, 0);
    message(IAID_TEST);
    Nlevel++;

    if ((level = setjmp(iaid_buf)) == 0){
	set_nofault(iaid_buf);
        io4_check_regs(slot); result++;
        io4_check_errpio(slot, window); 
	result = SUCCESS;
    }
    else { /* Returned here due to an error */
	if (level == 1){
	    io4_restoreconf(slot);
	    message("Exception while testing IA/ID registers in IO4 slot %d\n",
		slot);
	    show_fault();
	}
	else {
	    switch(result){
	        case 0 : message(IAID_REGTESTF); break;
	        case 1 : message(IAID_EREGTESTF); break;
	        default: message(IAID_UNKN_TEST); break;
	    }
	}
	result = FAILURE;
    }

    Nlevel--;
    clear_nofault();

#ifndef	NEWTSIM
    (result == SUCCESS) ? message(IAID_TESTP) : message(IAID_TESTF);
#endif

    Return(result);
	
}

int
io4_check_regs(int slot)
{
    IO4cf_regs	*ioregs;
    int		i,j;
    evreg_t	value, readv;

    message(IAID_REGTEST);

    io4_saveconf(slot);
    for (ioregs = io4_regs,i=0; ioregs->reg_no != (-1); ioregs++,i++){
        if (ioregs->bitmask0 == 0)
                continue;
	for (j=0, value=1; j < sizeof(unsigned); j++, value <<=1){
	    if (!(ioregs->bitmask0 & value))
		continue;
	    IO4_SETCONF_REG(slot, ioregs->reg_no, value);
	    readv = IO4_GETCONF_REG(slot, ioregs->reg_no);

	    if (readv != value){
		io4_restoreconf(slot);
		mk_msg("Testing IO4 Config regs %d: Expected=0x%x Got: 0x%x\n", 
					ioregs->reg_no, value, readv);
		analyze_error(1);
	    }
        }
    }
    io4_restoreconf(slot);

    message(IAID_REGTESTP);

    Return(0);
}


/*
 * Function : io4_check_timeout
 * Description :
 *	Do some invalid PIO operation, and check the error bits which
 *	get set. Do the invalid PIO again, and see if the sticky bit 
 *	gets set.
 */
int
io4_check_errpio(int slot, int window)
{
    evreg_t  	ebus_error, int_level, olvl;

#define	exp_error 	(IO4_EBUSERROR_BADIOA | IO4_EBUSERROR_STICKY)

    message(IAID_EREGTEST);

    min_io4config(slot, window);

    IO4_GETCONF_REG_NOWAR(slot, IO4_CONF_EBUSERRORCLR);

    olvl = IO4_GETCONF_REG(slot, IO4_CONF_INTRVECTOR);
    IO4_SETCONF_REG(slot, IO4_CONF_INTRVECTOR, IO4ERR_VECTOR);


    WR_REG(SWIN_BASE(window, 0), 0);	/* NON_EXISTANT_IOA	*/

    /* This should set the sticky bit 	*/
    WR_REG(SWIN_BASE(window, 0), 0);	

    ebus_error = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERRORCLR);

    int_level = RD_REG(EV_HPIL);

    if (ebus_error != exp_error){
	mk_msg(IAID_INV_BITSET, exp_error, ebus_error);
	analyze_error(1);
    }
    if (RD_REG(EV_IP0) == 0){
	mk_msg(IAID_NOINT_ERR, DIAG_INT_LEVEL);
	analyze_error(1);

    }
    if (int_level  != IO4ERR_LEVEL) {
        mk_msg(IAID_INTR_ERR, DIAG_INT_LEVEL, int_level);
        WR_REG(EV_CIPL0, 0);
	analyze_error(1);
    }

    IO4_SETCONF_REG(slot, IO4_CONF_INTRVECTOR, olvl);

    message(IAID_EREGTESTP);

    return(SUCCESS);

#undef 	exp_error
}

/*
 * Function : min_io4config
 * Description : Do minimal IO4 configuration
 */
void
min_io4config(int slot, int window)
{
    /* First do minimal IO4 configuration	*/
    if (window == 1) /* Already done...*/
	return;
    IO4_SETCONF_REG(slot, IO4_CONF_LW,  window);
    IO4_SETCONF_REG(slot, IO4_CONF_SW, window);
    IO4_SETCONF_REG(slot, IO4_CONF_ENDIAN, !getendian());
    IO4_GETCONF_REG_NOWAR(slot, IO4_CONF_IBUSERRORCLR);
    IO4_GETCONF_REG_NOWAR(slot, IO4_CONF_EBUSERRORCLR);
    IO4_SETCONF_REG(slot, IO4_CONF_INTRMASK, 0x2);

    iodev[0] = (uint)GET_REG(slot, IO4_CONF_IODEV0);
    iodev[1] = (uint)GET_REG(slot, IO4_CONF_IODEV1);
}

int
find_adap(int	adap_type)
{
    int	i;

    for (i=0; i < IO4_MAX_PADAPS; i++)
    	if ((iodev[i >> 2] >> ((i&0x3) * 8)) == adap_type)
            return(i);
	
    return (-1);
}

unsigned	config_regs[16];

void io4_saveconf(int slot)
{
    unsigned	i;
    IO4cf_regs	*ioregs; 

    for (ioregs = io4_regs, i=0; ioregs->reg_no != -1; ioregs++, i++)
	config_regs[i] = (uint)IO4_GETCONF_REG(slot, ioregs->reg_no);
    saved_io4 = 1;
}

void io4_restoreconf(int slot)
{
    unsigned	i;
    IO4cf_regs	*ioregs; 

    if (saved_io4 == 0)  /* Doomed */{
	min_io4config(slot, 1);
	message("Warning: IO4 Board %d window got Reconfigured\n");
	message("Please reset the system ASAP\n");
    }
    for (ioregs = io4_regs, i=0; ioregs->reg_no != -1; ioregs++, i++)
	IO4_SETCONF_REG(slot, ioregs->reg_no, config_regs[i]);

}

uint getendian(void)
{
#if R4000 || R10000
    return ((r4k_getconfig() >> CONFIG_BE_SHFT) & 1);
#endif
#if TFP
    extern k_machreg_t tfp_getconfig(void);
 
    return ((uint)((tfp_getconfig() >> CONFIG_BE_SHFT) & 1));
#endif
}
