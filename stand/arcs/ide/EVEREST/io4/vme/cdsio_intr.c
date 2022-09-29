
/*
 * io/cdsio_intr.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "arcs/ide/IP19/io/io4/vme/cdsio_intr.c $Revision: 1.13 $"

#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/vmereg.h>
#include <fault.h>
#include "vmedefs.h"
#include "cdsio.h"
#include <prototypes.h>
#include <libsk.h>

#define	BAUD_RATE_9600			0x6
#define	DATA_PATTERN			0x55
#define	INTR_ENABLED			0x8
#define	ROAK_INTR_MODE			0x80

#define	USAGE	"Usage: cdsio_intr io4_slot adapnum \n"
/*
 * bump it up as an experiment - was 10000000
 */
#define	TIME_LIMIT			0x80

#define INT_MASK			0x1000000

extern int io4_tslot, io4_tadap;

extern jmp_buf         vmebuf;

int	cdsio_loc[4];

#ifdef TFP
extern void tfp_clear_gparity_error();
#endif
extern int CDSIO_command (int, unsigned char, unsigned short);

/*
 * clear all pending VME interrupts
 */



/*
 * check status after receiving a CDSIO board interrupt
 */
int
check_vmeintr_status(int level, short vector, int port_number, short intr_type)
{

    int 	error_count = 0;
    unsigned 	char vector_read;
    volatile 	unsigned int refresh_timer;


    /* clear interrupt mask */
    WR_REG(vmecc_addr+VMECC_INT_ENABLECLR, (1 << level));

    msg_printf(DBG,"VMECC intr, intr lev: %x vec: %x pnum: %x int_typ: %x\n", 
	level, vector, port_number, intr_type);
    ms_delay(5);
    refresh_timer = 0x0;

    while (((RD_REG(vmecc_addr+VMECC_INT_REQUESTSM) & (1 << level)) == 0) && 
		refresh_timer++ < TIME_LIMIT)
	;

    if (refresh_timer >= TIME_LIMIT)
    {
	error_count++;
	msg_printf(ERR,"VME Interrupt Status: Expected: 0x%x Got 0x%x\n", 
		(1 << level), RD_REG(vmecc_addr+VMECC_INT_REQUESTSM));
    }

    if (port->port_intr_status != (0x1 << port_number))
    {
	error_count++;
	msg_printf(ERR, "Port interrupt status, Expected: %02x, Actual: %02x\n", 			0x1 << port_number, port->port_intr_status);
    }

    if (port->intr_source != intr_type)
    {
	error_count++;
	msg_printf(ERR,"Interrupt source flag, Expected: %x, Actual: %x\n",
			intr_type, port->intr_source);
    }

#ifdef NEVER
    vector_read = RD_REG(vmecc_addr+VMECC_IACK(level));
    if (vector_read != vector & 0xff)
    {
	error_count++;
	msg_printf(ERR,"VME interrupt vector, Expected: %x, Actual: %x\n", 
			vector & 0xff, vector_read);
    }				/* if */
#endif

    return (error_count);
}				/* check_vmeintr_status */


check_cdsio_intr()
{
    int error_count = 0;
    volatile unsigned int refresh_timer;


    refresh_timer = 0x0;

    while (!(*(volatile unsigned char *)status_port & INTR_PENDING) &&
	    refresh_timer++ < TIME_LIMIT)
	;

    if (!(*(volatile unsigned char *)status_port & INTR_PENDING)){
	error_count++;
	msg_printf(ERR, "CDSIO board status shows NO interrupt pending, %x\n",
			*(volatile unsigned char *)status_port);
    }

    return (error_count);
}


/*
 * verify the interrupt ability of the CDSIO board
 */

int
CDSIO_interrupt(int argc, char **argv)
{
    char 	    *atob();
    char	    *pnum;
    int             port_number = 0;
    unsigned 	    slot, anum;
    unsigned        level, error_count=0;
    unsigned	    found=0, fail=0;
    int		    retval=0, tskip=0, trun=0;


    /* Port Number 0 will be used for this test by default */
    if (argc > 1){
	if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv[0]);
	    return(1);
	}
    }

    while(!fail && io4_search(IO4_ADAP_FCHIP, &slot, &anum)){
        if ((argc > 1) && ((slot != io4_tslot) || (anum != io4_tadap)))
	    continue;

        if (fmaster(slot, anum) != IO4_ADAP_VMECC)
	    continue;

	found = 1;

        io_err_clear(slot, (1<< anum));

        ide_init_tlb();
        setup_err_intr(slot, anum);

        if (level = setjmp(vmebuf)){

	    switch(level){
	        default:
	        case 1: err_msg(CDSIO_IGEN, cdsio_loc); fail = 1; break;
	        case 2: break;
	    }
	    show_fault();

	    io_err_log(slot, anum);
	    io_err_show(slot, anum);

        }
        else{
	    set_nofault(vmebuf);

	    cdsio_loc[0] = slot; cdsio_loc[1] = anum; 
	    cdsio_loc[2] = port_number; cdsio_loc[3] = -1;
	    retval = cdsio_intr(slot, anum, port_number);
	    if (retval != TEST_SKIPPED) {
		fail |= retval;
		trun++;
	    } else
		tskip++;
        }

        clear_nofault();

        clear_err_intr(slot, anum);

#if _MIPS_SIM != _ABI64
	tlbrmentry(cdsio_base_addr);
#endif
    }

    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv[0]);
	}
	fail = TEST_SKIPPED;
    }

    if (fail)
	io4_search(0, &slot, &anum);
    else if (tskip && !trun)
	fail = TEST_SKIPPED;
    
    return(fail);

}
/* Base Interrupt level for IRQs */

#define	EVINTR_VMECC_IRQ		0x8

cdsio_intr(int slot, int anum, int port_number)
{

    unsigned short  	cmd_data;
    int             	error_count = 0;
    unsigned		oldSR;
    unsigned short  	vector;
    unsigned short  	wr14;
    unsigned short  	wr3;
    unsigned short  	wr4;
    unsigned short  	wr5;
    int             	level, ilevel, exp_level;
    volatile unsigned int vmepend;
    volatile unsigned int refresh_timer;

    msg_printf(VRB, "cdsio_intr called for port %d\n", port_number);

    if (error_count = cdsio_vmesetup(slot, anum))
	return(error_count);

    msg_printf(VRB, "Central Data serial i/o interrupt test\n");

    clear_vme_intr(vmecc_addr);

    /*
     * if the CDSIO board is ready for a command, initiate a board reset 
     */
    refresh_timer = 0x0;

    while (!(*(volatile unsigned char *)status_port & CDSIO_READY) && 
	    (refresh_timer++ < TIME_LIMIT))
	ms_delay(100);

    if (!(*(volatile unsigned char *)status_port & CDSIO_READY))
    {
	error_count++;
	msg_printf(ERR, "CDSIO board is busy, Returning\n");
	return 0;
    }

    msg_printf(DBG,"cdsio_intr: Resetting board\n");
    *(volatile unsigned char *)command_port = 0x0;
    ms_delay(1000); /* About a second of delay */

    /*
     * exit at this point if not an old-firmware board
     */
    if (*(volatile unsigned short*)version_number >= SGI_FIRMWARE) {
	msg_printf (VRB,
	    "CDSIO Board at slot %d, adap %d has new firmware, level 0x%x\n",
	    slot, anum, *(volatile unsigned short *)version_number);
	msg_printf (VRB, "Skipping . . .\n");
	return (TEST_SKIPPED);
    } else {
	msg_printf (VRB,
	    "CDSIO Board at slot %d, adap %d has old firmware, level 0x%x\n",
	    slot, anum, *(volatile unsigned short *)version_number);
    }

    port = (struct CDSIO_port *)(cdsio_base_addr + port_number * BLK_SIZE);

    /*
     * set up to do internal loopback, with interrupt 
     */
    msg_printf(DBG,"Setting up the board for interrupt test \n");
    wr3 = 0xc1 << 8;
    wr4 = 0x44 << 8;
    wr5 = 0xea << 8;
    wr14 = 0x13 << 8;
    if (!CDSIO_command(port_number, SET_SCC_REGISTER, 0x0 | 3) ||
        !CDSIO_command(port_number, SET_SCC_REGISTER, wr4 | 4) ||
        !CDSIO_command(port_number, SET_SCC_REGISTER, wr5 | 5) ||
        !CDSIO_command(port_number, SET_SCC_REGISTER, wr14 | 14) ||
        !CDSIO_command(port_number, SET_SCC_REGISTER, wr3 | 3) ||
	!CDSIO_command(port_number, SET_BAUD_RATE_REGISTERS, BAUD_RATE_9600))
        error_count++;

    port->dat.cd.output_buf0[0] = DATA_PATTERN;

    clear_err_intr(slot, anum);

    /* Disable interrupt. We will poll for it */
    oldSR = GetSR();
    SetSR(0);
    /* SetSR((oldSR & ~(SR_EXL|SR_ERL|SR_IE))|SR_BEV| SR_IEC | SR_IBIT3);  */

    /* Clear the currently pending Intr registers */
    while (level = EV_GET_LOCAL(EV_HPIL))
	EV_SET_LOCAL(EV_CIPL0, level);

    EV_SET_LOCAL(EV_ILE, EV_EBUSINT_MASK); /* Enable the CC intr register */

    /*
     * test all level of interrupts, walk a 1 through the interrupt vector 
     */
    for (level = 1; (level <= 7) && !error_count; level++)
    {
	/* Setup the vector for this level of IRQ */
	exp_level = (EVINTR_VMECC_IRQ+level);

	/* VECTORERROR is 8 bytes before IRQ1 */
	WR_REG(vmecc_addr+VMECC_VECTORERROR+(level*8), 
		EVINTR_VECTOR(exp_level,
		    (RD_REG(EV_SPNUM) & EV_SPNUM_MASK)));


	for (vector = 0x1; (vector < 256) && !error_count; vector <<= 1)
	{
	    msg_printf(VRB,"Testing Intr lvl 0x%x Intr vector 0x%x\n",
			level , vector);

            EV_SET_REG(vmecc_addr+VMECC_INT_ENABLESET, (1<<level));

	    cmd_data = ((ROAK_INTR_MODE | INTR_ENABLED | level) << 8) | vector;
	    if (!CDSIO_command(port_number,SET_INTR_VECTOR_INPUTS, cmd_data) ||
		!CDSIO_command(port_number,SET_INTR_VECTOR_OUTPUTS, cmd_data)||
		!CDSIO_command(port_number,SET_INTR_VECTOR_STATUS, 0x0) ||
		!CDSIO_command(port_number,SET_INTR_VECTOR_PARITY_ERR, 0x0))
	    {
		error_count++;
		break;
	    }			/* if */

	    /* Command to transmit a byte of data in Internal loopback mode */
	    if (!CDSIO_command(port_number, SEND_BLOCK0, 1))
	    {
	        error_count++;
		break;
	    }		/* if */

#if	0
	    refresh_timer = 0;
	    /* Waiting for out done interrupt. check interrupt mask register */
	    do
	    {
	        ms_delay(5);

	        /* check interrupt request register */
	        vmepend = RD_REG(vmecc_addr+VMECC_INT_REQUESTSM);
	    } while (!(vmepend & (1 << level)) && refresh_timer++ < TIME_LIMIT);

	    msg_printf(DBG, "\n");

	    if (refresh_timer >= TIME_LIMIT)
	    {
	        msg_printf(VRB, "No vme interrupt!\n");
	        msg_printf(VRB, "vmecc intr request register 0x%x\n", vmepend);
	        error_count++;
	        longjmp(vmebuf, 2);
	    }

	    error_count += check_cdsio_intr();
#endif
	    /* Wait for interrupt to reach the CC connected to this CPU */
	   refresh_timer = 0x0;
	   while ((refresh_timer++ < TIME_LIMIT) && !cpu_intr_pending());
	        ms_delay(2);

	   if (cpu_intr_pending()){
		ilevel = RD_REG(EV_HPIL);
		msg_printf(VRB, "Got out done intr No 0x%x level: 0x%x\n", 
				ilevel, level);
		
		EV_SET_REG(EV_CIPL0, ilevel);	/* Clear pending intr */
		if (ilevel != exp_level){
		    err_msg(CDSIO_BINT, cdsio_loc, exp_level, ilevel);
		    longjmp(vmebuf, 2);
		}
#ifdef NEVER	/* this has NEVER worked - the vector shows as FF */
		ilevel= (int) (RD_REG(vmecc_addr+VMECC_IACK(level)) & 0xFF);
		if (ilevel != vector){
		    msg_printf(DBG, "VMECC_IACK(%d) was 0x%x\n",
			level, ilevel);
		    err_msg(CDSIO_VECT, cdsio_loc, vector, ilevel);
		    longjmp(vmebuf, 2);
		}
#endif
	   }
	   else{
	        error_count++;
	        msg_printf(ERR, "Did not receive output done interrupt\n");
	        break;
	   }

	   /*
	    * make sure we are set up to handle the next interrupt
	    * before we clear the last one 
	    */

	    EV_SET_REG(vmecc_addr+VMECC_IACK(level), 0); /* Clear vector */
	    EV_SET_REG(vmecc_addr+VMECC_INT_ENABLESET, (1<<level));

	    if (!(*(volatile unsigned char *)status_port & CDSIO_READY))
	    {
	        error_count++;
	        msg_printf(ERR, "Can't clear output done interrupt, CDSIO board is busy\n");
	        break;
	    }
	    else
	        *(volatile unsigned char *)command_port = 0x0;

	    /* Waiting for input done interrupt */
#if 0
	    refresh_timer = 0;
	    /* check interrupt mask register */
	    do
	    {
	        ms_delay(5);

	        /* check interrupt mask register */
	        vmepend = RD_REG(vmecc_addr+VMECC_INT_REQUESTSM);
	    } while (!(vmepend & (1 << level)) && refresh_timer++ < TIME_LIMIT);

	    if (refresh_timer >= TIME_LIMIT)
	    {
	        msg_printf(ERR, "No vme interrupt!\n");
	        msg_printf(ERR, "interrupt mask register 0x%x\n", vmepend);
	        error_count++;
	        break;
	    }

	    error_count += check_cdsio_intr();
#endif
	    refresh_timer = 0x0;
	    while ((refresh_timer++ < TIME_LIMIT) && !cpu_intr_pending() )
	        ms_delay(5);

	    if (cpu_intr_pending()){
		ilevel = RD_REG(EV_HPIL);
		msg_printf(VRB, "Got inp done intr No 0x%x level: 0x%x\n",
			ilevel, level);
		
		EV_SET_REG(EV_CIPL0, ilevel);	/* Clear pending intr */
		if (ilevel != exp_level){
		    err_msg(CDSIO_BINT, cdsio_loc, exp_level, ilevel);
		    longjmp(vmebuf, 2);
		}
#ifdef NEVER
		if ((ilevel=RD_REG(vmecc_addr+VMECC_IACK(level)) != vector)){
		    err_msg(CDSIO_VECT, cdsio_loc, vector, ilevel);
		    longjmp(vmebuf, 2);
		}
#endif
	    }
	    else{
	        error_count++;
	        msg_printf(ERR, "Did not receive input done interrupt\n");
	        break;
	    }			/* if */

	    /*
	     * verify 
	     */
	    if (port->ram_parity_err == 0xff)
	    {
		error_count++;
		msg_printf(ERR,"Parity error(s) found on dual port RAM, 0x%x\n"
			,port->ram_parity_err);
		break;
	    }			/* if */

	    if (port->ovfl_count)
	    {
		error_count++;
		msg_printf(ERR, "Input buffer overflow count = 0x%04x\n"
				, port->ovfl_count);
		break;
	    }			/* if */

	    if ((port->filling_ptr - port->emptying_ptr) != 1)
	    {
		error_count++;
		msg_printf(ERR,"Filling pointer, Expected: 0x%x Got: 0x%x\n",
				level, port->filling_ptr);
		break;
	    }			/* if */

	    if (port->dat.cd.input_buf[port->emptying_ptr] != DATA_PATTERN)
	    {
		error_count++;
		msg_printf(ERR,"Data mismatch, Expected: 0x%x, Got: 0x%x\n",
		    DATA_PATTERN, port->dat.cd.input_buf[port->emptying_ptr]);
	    }			/* if */

	    if (port->dat.cd.err_buf[port->emptying_ptr] &
		(FRAMING_ERR | OVERRUN_ERR | PARITY_ERR))
	    {
		error_count++;
		msg_printf(ERR, "Framing/Overrun/Parity error, Status: 0x%x\n",
			port->dat.cd.err_buf[port->emptying_ptr]);
	    }			/* if */

	    (port->emptying_ptr)++;

	    /* Just check for any pending interrupts */
	    if (cpu_intr_pending()){
		err_msg(CDSIO_SINT,cdsio_loc, RD_REG(EV_IP0), RD_REG(EV_IP1));
		longjmp(vmebuf, 2);
	    }

	    /* Reset the board command port register */
	    if (!(*(volatile unsigned char *)status_port & CDSIO_READY))
	    {
	        error_count++;
	        msg_printf(ERR, "Can't clear output done interrupt, CDSIO board is busy\n");
	        break;
	    }
	    else
	        *(volatile unsigned char *)command_port = 0x0;

	    EV_SET_REG(vmecc_addr+VMECC_IACK(level), 0); /* Clear vector */
	}			/* for */
        EV_SET_REG(vmecc_addr+VMECC_INT_ENABLECLR, (1<<level));
    }				/* for */

    clear_vme_intr(vmecc_addr);
    nofault = 0;
    SetSR(oldSR);

    if (error_count)
	return (1);
    else
	return (0);
}				/* CDSIO_interrupt */

