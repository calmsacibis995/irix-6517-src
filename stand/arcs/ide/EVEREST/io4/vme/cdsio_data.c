#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/invent.h>
#include <sys/vmereg.h>
#include <setjmp.h>
#include <varargs.h>
#include <prototypes.h>
#include "vmedefs.h"
#include "cdsio.h"
#include <libsc.h>

#ident	"arcs/ide/IP19/io/io4/vme/cdsio_data.c $Revision: 1.14 $"

#define BAUD_RATES		(sizeof(baud_rate) / sizeof(unsigned short))
#define	DATA_SIZE		(sizeof(data_pattern) /	 sizeof(unsigned char))
#define	TIME_LIMIT			500 /* Was  1000000 */

volatile struct CDSIO_port		*port;
static volatile unsigned int		refresh_timer;

#if _MIPS_SIM != _ABI64
volatile unsigned vmecc_addr;
volatile unsigned char	*cdsio_base_addr;
volatile unsigned char	*vme_base_addr;
volatile unsigned char	*command_port;
volatile unsigned char	*status_port;
volatile unsigned short *version_number;
#else
volatile paddr_t vmecc_addr;
volatile paddr_t vme_base_addr;
volatile paddr_t cdsio_base_addr;
volatile paddr_t command_port;
volatile paddr_t status_port;
volatile paddr_t version_number;
int ide_badaddr(volatile void *, int);
#endif


#define	USAGE "Usage: %s [ -[i|e](0-5)] [io4_slot, ioanum]\n"

extern int cdsio_vmesetup(int, int);
extern int io4_tslot, io4_tadap;

extern	jmp_buf		vmebuf;
static	int	cdsio_loc[4];

#ifdef IP25
jmp_buf	locbuf;
jmpbufptr locptr;
#endif

/*
 * initiate a CDSIO board command
 */
int
CDSIO_command (int port_number, unsigned char command, unsigned short data)
{
   /*
      status port must indicate ready before initiating any command
   */
   refresh_timer = 0x0;
   while (!(*(volatile unsigned char *)status_port & CDSIO_READY) &&
	    (refresh_timer++ < TIME_LIMIT))
	ms_delay(5);

   if (!(*(volatile unsigned char *)status_port & CDSIO_READY)) {
      msg_printf(ERR, "CDSIO board busy, Command: 0x%x, Data: 0x%x, Status: 0x%x\n", command, data, *(volatile unsigned char *)status_port);
      return (0);
   }
      
   /*
      initiate command
   */
   port->command_code = command;
   port->command_data = data;
   port->command_status = 0xff;
   *(volatile unsigned char *)command_port = 0x1 << port_number;

   /*
      wait for command status
   */
   refresh_timer = 0x0;
   while ((port->command_status == 0xff) && (refresh_timer++ < TIME_LIMIT))
      ms_delay(5);

   if (port->command_status) {
	msg_printf(ERR, "CDSIO board command not initiated, Command: 0x%02x, Data: 0x%04x, Status: 0x%02x\n", command, data, port->command_status);
	if (refresh_timer >= TIME_LIMIT) {
	msg_printf(ERR, "CDSIO board command timed out\n");
	    return (0);
	}
   }   /* if */
   else
       msg_printf(DBG, "CDSIO board command initiated, Command: 0x%02x, Data: 0x%04x, Status: 0x%02x\n", command, data, port->command_status);

   return (1);
}   /* CDSIO_command */


/*
 * cdsio_vmesetup:
 *    Do the setup work needed to access the CDSIO board . Does following
 *    1. Using the slot/adap num, gets the VMECC base address.
 *    2. Makes an entry in TLB with 16 MB page size so that the VMECC large
 *       window is accessible using the virtual address.
 *    3. Make an entry in the VMECC PIOREG to map the Upper 9 bits of address
 *       to CDSIO board range of address.
 *    4. Sets up the globals cdsio_base_addr, status_port and command_port
 *       to map into the PIOREG(1) area.
 *       properly
 *
 * NOTE: 
 *    This routines assumes that VMECC_PIOREG(1) can be used .
 */

cdsio_vmesetup(int slot, int anum)
{

#if _MIPS_SIM != _ABI64
    caddr_t	vaddr;
#else
    paddr_t	vaddr;
    volatile short dummy;
#endif
    uint	i;

    msg_printf(DBG, "cdsio_vmesetup: slot: %d anum: %d\n", slot, anum);

    if ((vmecc_addr = get_vmebase(slot, anum)) == 0)
	return(TEST_SKIPPED);

#if _MIPS_SIM != _ABI64
    /* Make the tlb entry and PIO map entry for this board */
    ide_init_tlb();

    vaddr = tlbentry(WINDOW(vmecc_addr), anum, 0);
#else
    vaddr = LWIN_PHYS(WINDOW(vmecc_addr), anum);
    vme_base_addr = vaddr;
#endif
    init_fchip(vmecc_addr);
    init_vmecc(vmecc_addr);

    WR_REG((vmecc_addr+VMECC_PIOREG(1)), 
	((CDSIO_BASE_ADDRESS >> VMECC_PIOREG_ADDRSHIFT)|
	 (A24_SUAM << VMECC_PIOREG_AMSHIFT)));

    vaddr += VMECC_PIOREG_MAPSIZE + 
	     (CDSIO_BASE_ADDRESS & (VMECC_PIOREG_MAPSIZE-1));

    for (i=0; i < 4; i++){ /* Look for 4 possible boards */
        cdsio_base_addr = vaddr+ (i * 0x10000);
	msg_printf(DBG,"probing cdsio_base_addr 0x%x\n", cdsio_base_addr);
#if !defined(TFP) && !defined(IP25)
	if (badaddr((volatile void *)cdsio_base_addr, sizeof(short)) == 0)
	    break;
#elif TFP
	clear_err_intr(slot, anum);
	dummy = *(volatile short *)cdsio_base_addr;
	if (!(getcause() & CAUSE_BE))
	{
	    clear_err_intr(slot, anum);
	    break;
	}
#elif IP25
	locptr=nofault;
	if (setjmp(locbuf))
	{
	    clear_err_intr(slot, anum);
	    set_nofault(locptr);
	    msg_printf(DBG,"End IP25 no CDSIO yet case\n");
	}
	else
	{
	    clear_err_intr(slot, anum);
	    msg_printf(DBG,"SR = %llx\n", (long long) get_SR());
	    set_nofault(locbuf);
	    dummy = *(volatile short *)cdsio_base_addr;
	    if (!(getcause() & SRB_ERR))
	    {
		clear_err_intr(slot, anum);
		set_nofault(locptr);
		msg_printf(DBG,"End IP25 found CDSIO case\n");
		break;
	    }
	    else
	    {
		msg_printf(DBG,"End IP25 no buserr no cdsio case???\n");
	    }
	    set_nofault(locptr);
	}
#endif
	clear_err_intr(slot, anum);
	io_err_clear(slot, (1 << anum));
    }

    if (i == 4){
	msg_printf(ERR,
		"No CDSIO board found in VME bus on IO4 slot %d padap %d\n",
		slot, anum);
#if _MIPS_SIM != _ABI64
	tlbrmentry(vaddr);
#endif
	return(TEST_SKIPPED);
    }

    /*
    cdsio_base_addr =(volatile unsigned char *)
	     (vaddr+0x00800000+(CDSIO_BASE_ADDRESS &(VMECC_PIOREG_MAPSIZE-1)));
    */

    status_port  = cdsio_base_addr + STATUS_PORT;
    command_port = cdsio_base_addr + COMMAND_PORT;
    version_number = (cdsio_base_addr + FIRMWARE_REVISION_NUMBER);
    return(0);
}


/*
 * CDSIO board internal/external data loopback test
 * format
 * cdsio_data io4_slot, ioa_num, [ -i(0-5) ]
 *
 */

int
CDSIO_data_loopback (int argc, char **argv)
/*
 * Looks like we DONOT have the command name in argv, and thus argc is 1 less
 * than the standard C format.
 * So we should start using from argc = 0
 */
{
    unsigned char	loopback;
    int			port_number = 0;
    int			slot, anum, err_level;
    int			fail=0, found=0;
    int			retval=0, tskip=0, trun=0;
    char		*atob (), *opt;
    char		*argv0;
    int			error_count = 0;




#if 0
    /*
     * Get the slot and adap num specified.
     */
    if (argc < 2){
        msg_printf(ERR, "invalid no of parameters '%d' passed \n", argc);
        msg_printf(ERR, USAGE, argv[0]);
        return(1);
    }
    if ((atob(argv[1], &slot) == (char *)0) || 
	(atob(argv[2], &anum) == (char *)0))
	return(1);

    argv += 3;
    argc -= 3;
#endif

    argv0 = argv[0];
    opt = argv[1];
    loopback=0x10; /* Default internal loopback */
    if ((argc > 1) && (*opt++ == '-')){
        switch (*opt++){
	 case 'e':
	    loopback = 0; /* Fall through */
	 case 'i':
	    (void)atob(opt, &port_number);
	    if ((port_number < 0) || (port_number >= NUMBER_OF_PORTS))
	       error_count++;
	    else {
	       argc--; 
	       argv++;
	    }
	    break;

	 default:
	    error_count++;
	    break;
	 }   /* switch */
    }

    if (error_count) {
       msg_printf(ERR, USAGE, argv0);
       return (1);
    }   /* if */

    if (argc > 1){
        if (io4_select(1, argc, argv)){
	    msg_printf(ERR, USAGE, argv0);
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

        setup_err_intr(slot, anum);
        cdsio_loc[0] = slot;        cdsio_loc[1] = anum; 
	cdsio_loc[2] = port_number; cdsio_loc[3] = -1;

        if (err_level = setjmp(vmebuf)){
	    switch(err_level){
	    default:
	    case 1: err_msg(CDSIO_GEN, cdsio_loc); 
		    show_fault();  break;
	    case 2: io_err_log(slot, anum); 
		    io_err_show(slot, anum);
		    break;
	    }
	    fail = err_level;
        }
        else{
	    set_nofault(vmebuf);
	    retval = cdsio_check(slot, anum, port_number, loopback);
	    if (retval != TEST_SKIPPED) {
		fail |= retval;
		trun++;
	    } else
		tskip++;
        }

        clear_nofault();
        clear_err_intr(slot, anum);

#if _MIPS_SIM != _ABI64
	if (cdsio_base_addr)
	    tlbrmentry(cdsio_base_addr);

        ide_init_tlb();
#endif
    }
    if (!found) {
	if (argc > 1) {
	    msg_printf(ERR, "Invalid Slot/Adapter number\n");
	    msg_printf(ERR, USAGE, argv0);
	}
	fail = TEST_SKIPPED;
    }
    if (fail)
        io4_search(0, &slot, &anum);    /* reset variables for future use */
    else if (tskip && !trun)
	fail = TEST_SKIPPED;

    return (fail);
}

cdsio_check(int slot, int anum, int port_number, int loopback)
{

    static unsigned short 	baud_rate[] = {
				   0x0002, 	/* 19200 */
				   0x0006, 	/* 9600 */
				   0x000e, 	/* 4800 */
				   0x001e, 	/* 2400 */
				   0x003e, 	/* 1200 */
				   0x007e, 	/* 600 */
				   0x00fe 	/* 300 */
					};

    static unsigned char	data_pattern[] = {
				   0x00,
				   0xaa,
				   0xcc,
				   0xf0,
				   0xff
				};

    unsigned char		expected_data;
    unsigned short		wr14, wr3, wr4, wr5;
    int 			error_count;
    unsigned			i,j, count;

    if (error_count = cdsio_vmesetup(slot, anum))
	return(error_count);
    
    if (loopback)
       msg_printf(VRB,"Central Data serial i/o internal loopback test\n");
    else
       msg_printf(VRB,"Central Data serial i/o external loopback test\n");

    clear_vme_intr(vmecc_addr);
    /*
     * if CDSIO board is ready to accept command, initiate a board reset
     */

    refresh_timer = 0x0;
    while (!(*(volatile unsigned char *)status_port & CDSIO_READY) &&
	    (refresh_timer++ < TIME_LIMIT))
       ms_delay(200);

    if (!(*(volatile unsigned char *)status_port & CDSIO_READY)) {
        error_count++;
	err_msg(CDSIO_RDY, cdsio_loc, *(volatile unsigned char *)status_port);
	longjmp(vmebuf, 2);
    }
    else{
       msg_printf(DBG,"cdsio_data: Resetting board \n");
       *(volatile unsigned char *)command_port = 0x0;
       ms_delay(200);

	/*
	 * exit at this point if not old firmware
	 */
	if (*(volatile unsigned short *)version_number >= SGI_FIRMWARE) {
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

       port = (struct CDSIO_port *)(cdsio_base_addr + (port_number * BLK_SIZE));

       msg_printf(DBG, "Setting up the CDSIO for data transfer. port: 0x%x\n", 
		port);
       /* set up port, no interrupt */
       wr3 = 0xc1 << 8;
       wr4 = 0x44 << 8;
       wr5 = 0xea << 8;
       wr14 = (loopback | 0x3) << 8;
       if (!CDSIO_command (port_number, SET_SCC_REGISTER, 0x0 | 3) ||
           !CDSIO_command (port_number, SET_SCC_REGISTER, wr4 | 4) ||
           !CDSIO_command (port_number, SET_SCC_REGISTER, wr5 | 5) ||
           !CDSIO_command (port_number, SET_SCC_REGISTER, wr14 | 14) ||
           !CDSIO_command (port_number, SET_SCC_REGISTER, wr3 | 3) ||
           !CDSIO_command (port_number, SET_INTR_VECTOR_INPUTS, 0x0) ||
	   !CDSIO_command (port_number, SET_INTR_VECTOR_OUTPUTS, 0x0) ||
	   !CDSIO_command (port_number, SET_INTR_VECTOR_STATUS, 0x0) ||
	   !CDSIO_command (port_number, SET_INTR_VECTOR_PARITY_ERR, 0x0))

          error_count++;

       /*
 	 set up test patterns
       */
       msg_printf(DBG, "Copying data pattern .........................\n");
       for (i = 0; (i < DATA_SIZE) && !error_count; i++) {
          port->dat.cd.output_buf0[i] = data_pattern[i];
          port->dat.cd.output_buf1[i] = ~data_pattern[i];
       }   /* for */
    }   /* else */

    /*
       loopback test with different baud rate
    */
    for (i = 0; (i < BAUD_RATES) && !error_count; i++) {
       msg_printf(DBG, "Testing at baudrate code %d\n", baud_rate[i]);
       if (!CDSIO_command (port_number, SET_BAUD_RATE_REGISTERS, baud_rate[i])){
	  error_count++;
	  break;
       }   /* if */

       /*
	  send blocks 0 & 1 and wait for completion
       */
       for (j = 0; (j < 2) && !error_count; j++) {
          port->output_busy = 0xff;
          if (!CDSIO_command (port_number, SEND_BLOCK0 + j, DATA_SIZE)) {
	     error_count++;
	     break;
          }   /* if */

	  /* NOTE: THIS MAY NEED MORE DELAY/TUNING */
          refresh_timer = 0x0;
          while (port->output_busy && (refresh_timer++ < TIME_LIMIT))
	     ms_delay(5);

          if (port->output_busy) {
	     err_msg(CDSIO_ERR, cdsio_loc, j);
	     longjmp(vmebuf, 2);
          }   /* if */	

	  /* Wait till all the characters comeback */
          refresh_timer = 0x0;
          while ((port->filling_ptr != (DATA_SIZE * (j + 1))) &&
	        (refresh_timer++ < TIME_LIMIT))
	    ms_delay(5);

          if (port->filling_ptr != (DATA_SIZE * (j + 1))) {
	     error_count++;
	     err_msg(CDSIO_ERR1,cdsio_loc,j,DATA_SIZE*(j+1), port->filling_ptr);
	     longjmp(vmebuf, 2);
          }   /* if */
       }   /* for */

       if (error_count)
	  break;

       if (port->ram_parity_err == 0xff) {
	  error_count++;
	  msg_printf(ERR, "Parity error(s) detected on dual port RAM, 0x%02x\n",
			port->ram_parity_err);
	  break;
       }   /* if */

       if (port->ovfl_count) {
	  error_count++;
	  msg_printf(ERR, "Input buffer overflow count is nonzero, 0x%04x\n", 
			port->ovfl_count);
	  break;
       }   /* if */

       /*
	*  check data
        */
       for (j=0, count=port->emptying_ptr; j < (DATA_SIZE * 2); j++, count++){

	  expected_data = (j < DATA_SIZE) ?
	    data_pattern[j] : ~data_pattern[j - DATA_SIZE] & 0xff;


	  if (port->dat.cd.input_buf[count] != expected_data) {
	      error_count++;
	      err_msg(CDSIO_DERR, cdsio_loc, count, expected_data, 
		port->dat.cd.input_buf[count]);
	      longjmp(vmebuf, 2);
	  }   /* if */


	  if (port->dat.cd.err_buf[count] &
		(FRAMING_ERR | OVERRUN_ERR | PARITY_ERR)){
	     error_count++;
	     msg_printf(ERR,"Framing/Overrun/Par error at %4x, Status: %2x\n",
		port->emptying_ptr, port->dat.cd.err_buf[port->emptying_ptr]);
	  }   /* if */
       }   /* for */
    }   /* for */

    if (error_count)
	 return (1);
    else
	 return (0);
}   /* cdsio_check */

