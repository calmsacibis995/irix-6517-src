/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * This file defines the two entry-points kona_probe() and
 * kona_install() as required in the configuration file
 */

#if defined(IP19) || defined(IP25)

#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/kona.h>
#include "arcs/hinv.h"
#include "arcs/io.h"
#include "stand_htport.h"

#include <ktport.h>
#include <libsc.h>
#include <libsk.h>

unsigned long *gfxvaddr;
int konadebug;
static int tport_pipe = KONA_TPORT_PIPE;
static struct _pipeinfo {
  unsigned long *base;
  unsigned int window;
  unsigned int adapter;
  int slot;
} gfxpipe[KONA_MAXPIPES + 1];

static int init_graphics(unsigned long *gfxbase,
			 int slot,
			 unsigned int window,
			 unsigned int adapter);
static unsigned int get_eeprom_state(unsigned long *gfxbase);
static int find_konapipe(int pipenum,
			 unsigned int *window,
			 unsigned int *adapter);
static int initialize_fchip(unsigned int slot, unsigned int adapter);
static void *kona_setgfxbase(unsigned int window, 
			     unsigned int adapter);

#define IO4_ADAP_HIP1A  0x30
#define IO4_ADAP_HIP1B  0x31
	
/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0, and we return the base of the first KONA board.  If fncs
 * is != 0, we return the functions for the graphics board at base.
 *
 * Also initialize all pipes.
 */
void *kona_probe(struct htp_fncs **funcs)
{
  extern struct htp_fncs kona_tpfuncs;
  struct _pipeinfo *g = &gfxpipe[tport_pipe];

  verbose("In kona_probe(fncs = %x)\n", funcs);
  if (funcs) {
    *funcs = &kona_tpfuncs;
    return 0;
  }

  /* Initialize graphics */
  if (init_graphics(g->base, g->slot, g->window, g->adapter) < 0)
    return 0;

  return (void *) g->base;
}


/*
 * Entry point:
 * 
 * Probe for all KONA pipes in the system. Add them to the hardware
 * inventory. A pipe is not added to the inventory if its GFX prom is
 * corrupted, and an appropriate message is printed on the serial
 * console. We also save the base-addresses of all the pipes in the
 * bases[] array, which is used by kona_probe() later.
 * Return value: #pipes found.
 */
int kona_install(COMPONENT *root)
{
  register int i, pipenum;
  unsigned int window, adapter;
  int slot, status, npipes;
  COMPONENT konatmpl, *tmp;
  char idbuf[80];		/* machine-identifier string */
  char *p;
  unsigned long *gfxbase;
  extern int _gfx_strat();
  extern int clogo_w, clogo_h;
  extern int kona_logo_w, kona_logo_h, kona_logo_size; 
  extern unsigned char clogo_data[], kona_logo_data[];

  verbose("In kona_install()\n");

  /*
   * Use the console environment variable for debug settings, and
   * selecting appropriate textport pipe.
   */
  if (p = getenv("console")) {
    if (*p == 'g' && p[1] >= '0' && p[1] <= '9') {
      tport_pipe = p[1] - '0';
      if (tport_pipe > KONA_MAXPIPES-1)
	tport_pipe = KONA_TPORT_PIPE;
    }

    if (p[1] == 'K')		/* hack!! */
      konadebug = 2;
    else if (p[1] == 'k')
      konadebug = 1;
    else
      konadebug = 0;
  }
  
  npipes = 0;
  for (i = 0; i < KONA_MAXPIPES; i++)
    gfxpipe[i].base = 0;

  for (pipenum = 0; pipenum < KONA_MAXPIPES; pipenum++) {
    struct _pipeinfo *g = &gfxpipe[pipenum];

    if ((slot = find_konapipe(pipenum, &window, &adapter)) < 0) {
      /*
       * This means that the system has 'pipenum-1' #pipes. Re-check
       * that the textport-pipe lies in this range. If not, we did not
       * register it. Hence, repeat the probe falling back to default 
       * pipe-number.
       */
      if (tport_pipe > pipenum-1 && (tport_pipe != KONA_TPORT_PIPE)) {
	tport_pipe = KONA_TPORT_PIPE;
	pipenum = tport_pipe-1;
	continue;
      }
      else
	return npipes;		/* no more KONA pipes */
    }

    npipes++;
    
    verbose("kona_install: pipe @ (win, adap)=(0x%x, 0x%x)\n",
	    window, adapter);

    /* Get KONA pipe base address */
    g->window = window;
    g->adapter = adapter;
    g->slot = slot;
    g->base = gfxbase = 
      (unsigned long *) kona_setgfxbase(window, adapter);

    verbose("base[%d] = %x\n", pipenum, gfxbase);

    /*
     * Check the sanity of this pipe. It should already be alive and
     * running. If not, check its EEPROM state. If neither of these
     * works, do not register this pipe in the hardware inventory, and
     * print a message to the console.
     */

    status = kona_alive((void *) gfxbase, 100);
    if (status < 0) {
      /*
       * Pipe did not respond. Check eeprom state
       */
      
      status = get_eeprom_state(gfxbase);
      if (status != ES_GOOD) {
	ttyprintf("Warning: Found Bad KONA eeprom: 0x%x\n", status);
	ttyprintf("       : Not registering pipe\n");
	continue;
      }
    }

    if (pipenum == tport_pipe) {
      verbose("Found tport pipe (logical pipe %d)\n", pipenum);
    }

    konatmpl.Class = ControllerClass;
    konatmpl.Type = DisplayController;
    konatmpl.Flags = ConsoleOut | Output;
    konatmpl.Version = SGI_ARCS_VERS;
    konatmpl.Revision = SGI_ARCS_REV;
    konatmpl.Key = 0;
    konatmpl.AffinityMask = 0x01;
    konatmpl.ConfigurationDataSize = 0;
    konatmpl.Identifier = idbuf;
    
    /* 
     * Get KONA specific information and construct the Identifier
     * string. // konatmpl.idbuf = f(h/w probe, base)
     */
    strcpy(idbuf, "SGI-InfiniteReality Graphics");

    konatmpl.IdentifierLength = strlen(konatmpl.Identifier);
    tmp = AddChild(root, &konatmpl, (void*) NULL);
    if ((tmp == (COMPONENT *) NULL))
      verbose("KONA");
    tmp->Key = 0;
    RegisterDriverStrategy(tmp, _gfx_strat);

    /*
     * Store logo into the global logo-structure. This is used by
     * higher level IO4prom routines later. (Fix this so that download
     * does not occur for every pipe!!)
     */
    
      clogo_w = kona_logo_w;
      clogo_h = kona_logo_h;
      bcopy(kona_logo_data, clogo_data, kona_logo_size);
  }
  
  verbose("Exiting kona_install()\n");
  return npipes;
}


/* 
 * Find the n'th logical KONA pipe. Return the slot number, window
 * and adapter if found, else return -1
 */
static int find_konapipe(int pipenum, unsigned int *window,
			 unsigned int *adapter)
{
  int  slot;			/* physical slot */
  unsigned int padap;		/* physical adapter id within the board */
  evioacfg_t *evioa;
  evbrdinfo_t *board;
  int pipe_found = 0;
  unsigned int win;

  verbose("In find_konapipe(%d)\n", pipenum);

  for (slot = EV_MAX_SLOTS-1; slot > 0; slot--) {
    
    board = &(EVCFGINFO->ecfg_board[slot]);
    if ((board->eb_type & (EVCLASS_MASK | EVTYPE_MASK)) != EVTYPE_IO4
	|| !board->eb_enabled)
      continue;
    
    /* Found an enabled IO4 board */
    verbose("Found enabled IO4 in slot 0x%x\n", slot);

    evioa = &board->eb_un.ebun_io.eb_ioas[1];
    win = board->eb_un.ebun_io.eb_winnum;

    for (padap = 1; padap < IO4_MAX_PADAPS; padap++, evioa++) {
      
      if  (evioa->ioa_type == IO4_ADAP_FCHIP) { /* adapter is F chip */
	unsigned int master_id;	
	unsigned int *swin;
	
	swin = (unsigned int *) SWIN_BASE(win, padap);
	master_id = (unsigned int)
		EV_GET_REG((void *)((ulong)swin + FCHIP_MASTER_ID));

	switch (master_id) {
	  
	case IO4_ADAP_HIP1A:
	case IO4_ADAP_HIP1B:
	  verbose("Found KONA pipe at (slot, adap) = (0x%x, 0x%x)\n",
		  slot, padap);
	  if (pipe_found == pipenum) { /* this is the pipe we are */
				       /* looking for */
	    *window = board->eb_un.ebun_io.eb_winnum;
	    *adapter = padap;
	    return slot;
	  } else
	    pipe_found++;
	  break;
	  
	default:        /* Not a HIP .. ignore it*/
	  break; 
	}
      }	/* endif - Fchip */
    }
    
  } /* end - forall slots */
  
  return -1;
}

/*
 * Reset the graphics board. Come out of reset with the EEPROM
 * remapped so that the ARM boots from it. In case of HIP1A, load a
 * small piece of bootstrap-code which copies EEPROM contents to SDRAM
 * and jumps to the entry point
 */
static int init_graphics(unsigned long *gfxbase,
			 int slot,
			 unsigned int window,
			 unsigned int adapter)
{
  int status;

  verbose("In init_graphics( base = 0x%x)\n", gfxbase);

  gfxvaddr = gfxbase = kona_setgfxbase(window, adapter);

#if defined(EVEREST)  
  initialize_fchip(slot, adapter);
#endif

  /*
   * First try talking to the pipe. If it responds, we do not
   * additional resettting (and  consequent screen flashing).
   */ 
  status = kona_alive((void *) gfxbase, 100);
  if (status < 0) {
    /* 
     * Reset the HIP with ARM_START bit set. When un-reset, ARM should
     * come up and start booting from EEPROM lcoation 0
     */
    hh_write(HH_INTR_INFO_REG, 0x0);
    hh_write(HH_RESET_REG, 0x3);
    (void) hh_read(HH_RESET_REG);
    hh_write(HH_RESET_REG, 0x2);
    sginap(10);
  }

  /*
   * Initialize textport ucode.
   */

  verbose("Started HIP ... send tport initialization command\n");
  
  if (konadebug == 2)
    kona_debug_enable();
    
  /*
   * Initialize the pipe for further textport commands. Get graphics
   * information to determine screen size
   */
  kona_initpipe(gfxbase);
  return kona_alive((void *) gfxbase, 0);
}


static int initialize_fchip(unsigned int slot, unsigned int adapter)
{
  unsigned int ioconfig_reg;
  
  verbose("In initialize_fchip()\n");
  ioconfig_reg = (unsigned int) IO4_GETCONF_REG(slot, 2);
  ioconfig_reg |= (0x10101 << adapter);
  IO4_SETCONF_REG(slot, 2, ioconfig_reg);
  
  return 0;
}

static unsigned int get_eeprom_state(unsigned long *gfxbase)
{
  unsigned int address, limit, *p;
  bhdr b;
  
  verbose("Checking out EEPROM\n");

  gfxvaddr = gfxbase;
  hh_write(HH_RESET_REG, 0x1);
  (void) hh_read(HH_RESET_REG);
  hh_write(HH_RESET_REG, 0x0);
  sginap(15);

  hh_write(HH_ARM_CTRL_REG, ARM_CTRL_HOST_ACCESS_MASK);
  ha_write(HA_REMAP_EPROM_REG, 0x0);
  address = HA_EEPROM + BOOTHDR_OFFSET;
  limit = (unsigned int) (address + sizeof(bhdr));

  verbose("address = %x, limit = %x\n", address, limit);
  p = (unsigned int *) &b;
  while (address < limit) {
    ha_read(address, *p++);
    address += 4;
    DELAY(2);
  }

  verbose("EEPROM state = %x\n", b.state);
  return b.state;
}

static void *kona_setgfxbase(unsigned int window, 
			     unsigned int adapter)
{

    
#if defined(IP19) && defined(IP19_32BIT_MODE)

  /*
   * IP19 can deal with only 32-bit IO4prom. 
   */

  unsigned old_pgmask;
  unsigned int pteevn, pteodd;
  caddr_t va;
  
#define IOMAP_TLBPAGEMASK       0x1ffe000

  verbose("using 32bit mapping\n");
  va = (caddr_t)(__psint_t)(unsigned) 0xf0000000;
  
  pteevn = ((LWIN_PFN(window,adapter) ) << 6) | (2 << 3) |
    (1 << 2) | (1 << 1) | 1;
  pteodd = 0;
  
  old_pgmask = get_pgmask();
  set_pgmask(IOMAP_TLBPAGEMASK);
  
  /* Use entry 0: it is reserved for graphics */
  
  tlbwired(0, 0, (caddr_t) va, pteevn, pteodd);
  set_pgmask(old_pgmask);

  return ((void *) va);
  
#else  /* 64-bit mode */
  
  verbose("using 64bit mapping\n");
  return (void *) 
    (PHYS_TO_K1(((long) LWIN_PFN(window, adapter)) << 12));

#endif
}

#elif defined(IP21)

/*
 * For IP21 - we define thee entrypoints as nops so that 1 config
 * file can be used by all proms.
 */

int kona_install(void *root)
{
    return(0);
}

void *kona_probe(struct htp_fncs **funcs)
{
    *funcs = 0;
    return(0);
}

#endif


    
#ifdef SN0
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>   /* config stuff */
#include <sys/SN/slotnum.h>
#include <sys/hwgraph.h>
#include <arcs/cfgdata.h>

#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/kona.h>
#include "arcs/hinv.h"
#include "arcs/io.h"
#include "stand_htport.h"
#include "promgraph.h"
#include <pgdrv.h>

#include <libsc.h>
#include <libsk.h>
#include <ktport.h>

unsigned long *gfxvaddr;
int konadebug;

static int tport_pipe = KONA_TPORT_PIPE;
static unsigned long *tport_pipe_base;

void   sginap(int ticks);

extern int _gfx_strat(COMPONENT *, IOBLOCK *);

static char *gid = "SGI-InfiniteReality2 Graphics" ;

static int init_graphics(unsigned long *gfxbase);

static unsigned int get_eeprom_state(unsigned long *gfxbase);

static void initialize_xg(__psunsigned_t);


void
sginap(int n) {
        DELAY(n * 1500);
}


/*
 * Probe routine set by .cf file and used by the textport.  The tp calls
 * with fncs=0, and we return the base of the first KONA board.  If fncs
 * is != 0, we return the functions for the graphics board at base.
 *
 * Also initialize all pipes.
 */
void *kona_probe(struct htp_fncs **funcs)
{
  extern struct htp_fncs kona_tpfuncs;

#if 0
  ttyprintf("In kona_probe(fncs = %x)\n", funcs);
#endif

  if (funcs) {
    *funcs = &kona_tpfuncs;
    return 0;
  }

  /* Initialize graphics */
  if (init_graphics(tport_pipe_base) < 0)
    return 0;

  return (void *) tport_pipe_base;
}

/*
 * Entry point:
 * 
 *ifdef  SN0
 *
 *	 kl_graphics_install() registers the pipe without regard to
 *	 if its GFX prom is corrupted, previous to this entry point
 *	 within the ip27prom (iodiscovery). 
 * IO6prom
 * 	 However, if GFX prom is corrupted, kl_graphics_AddChild() is 
 *	 is not allowed and a message is printed on the serial console.
 *
 * 	Pipes are found 2 ways :
 *	 Search of all gfx boards in board class ( KLCLASS_IO|KLTYPE_GFX ) 
 *	   Sub_search : matching gfx board type  ( KLCLASS_GFX|KLTYPE_GFX_*) 
 *	    -or-  Probe for all KONA pipes in the system. 
 *
 *	 We also save the base-addresses of all the pipes in the
 * 	 bases[] array, which is used by kona_probe() later.
 *	Return value: #pipes found = 1;
 *else
 * 	Probe for all KONA pipes in the system. 
 *	 Add them to the hardware inventory. 
 *	 A pipe is not added to the inventory if its GFX prom is
 *	 corrupted, and an appropriate message is printed on the serial
 *	 console. We also save the base-addresses of all the pipes in the
 * 	 bases[] array, which is used by kona_probe() later.
 *	Return value: #pipes found.
 * endif
 */

#ifdef SN_PDI
int
kona_install(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
#else
int
kona_install(COMPONENT * c)
#endif
{
    int			status;
    __psunsigned_t	wid_register_base;	/* PIO space for widget registers */
    ULONG		*gfxbase;
    extern int clogo_w, clogo_h, clogo_data_size;
    extern int kona_logo_w, kona_logo_h, kona_logo_size; 
    extern unsigned char clogo_data[], kona_logo_data[];

#ifdef SN_PDI
    wid_register_base = GET_WIDBASE_FROM_KEY(pdipDmaParm(pdip)) ;
#else
    /* Get the widget base address from the c->Key field */
    wid_register_base = GET_WIDBASE_FROM_KEY(c->Key);
#endif

    gfxvaddr = gfxbase = (ULONG *)wid_register_base;
	   
    /*
     * GFX Detach - disables credits sent back to host for texport
     *            - else crash pipe
     */
    XG_REG_WRITE(XG_GFX_DET,  0x23);

    tport_pipe_base = (ULONG *)wid_register_base;
     
    status = kona_alive((void *) gfxbase, 100);
    if (status < 0) {
	/*
	 * Pipe did not respond. Check eeprom state
	 */
	status = get_eeprom_state(gfxbase);
	if (status != ES_GOOD) {
	    ttyprintf("Warning: Found Bad KONA eeprom: 0x%x\n", status);
	    ttyprintf("       : Not Adding gfx device - gfxpipe\n");
	    return 0 ;
	}
    } 

    /* finish up component initialization */
    c->IdentifierLength = strlen(gid);
    c->Identifier = malloc(strlen(gid)+1);
    strcpy(c->Identifier, gid) ;

    /*
     * Store logo into the global logo-structure. This is used by
     * higher level IOXprom routines later. (Fix this so that download
     * does not occur for every pipe!!)
     */
    
    clogo_w = kona_logo_w;
    clogo_h = kona_logo_h;
    /* check <logo/onyx.h> Spans for Calligraphy logos */
    if (clogo_data_size >= kona_logo_size)		
	bcopy(kona_logo_data, clogo_data, kona_logo_size);
    else
	ttyprintf(" ERROR: Failed bcopy (kona_logo_data) \n");
    
    return 1 ;
}



/*
 * Reset the graphics board. Come out of reset with the EEPROM
 * remapped so that the ARM boots from it. In case of HIP1A, load a
 * small piece of bootstrap-code which copies EEPROM contents to SDRAM
 * and jumps to the entry point
 */
static int init_graphics(unsigned long *gfxbase)
{
  int status;

#if 0
  ttyprintf("In init_graphics( base = 0x%x)\n", gfxbase);
#endif

  gfxvaddr = gfxbase ;

#if 0
  initialize_xg(wid_register_base); 
#endif


  /*
   * First try talking to the pipe. If it responds, we do not
   * additional resettting (and  consequent screen flashing).
   */ 
  status = kona_alive((void *) gfxbase, 100);

  if (status < 0) {
    /* 
     * Reset the HIP with ARM_START bit set. When un-reset, ARM should
     * come up and start booting from EEPROM lcoation 0
     */
    hh_write(HH_INTR_INFO_REG, 0x0);
    hh_write(HH_RESET_REG, 0x3);
    (void) hh_read(HH_RESET_REG);
    hh_write(HH_RESET_REG, 0x2);
    sginap(10);
  }

  /*
   * Initialize textport ucode.
   */
#if 0
  ttyprintf("Started HIP ... send tport initialization command\n");
#endif

#if 0  
  if (konadebug == 2)
    kona_debug_enable();
#endif
    
  /*
   * Initialize the pipe for further textport commands. Get graphics
   * information to determine screen size
   */
  kona_initpipe(gfxbase);
  return kona_alive((void *) gfxbase, 0);
}



static unsigned int get_eeprom_state(unsigned long *gfxbase)
{
  unsigned int address, limit, *p;
  bhdr b;
  
  ttyprintf("Checking out EEPROM\n");

  gfxvaddr = gfxbase;
  hh_write(HH_RESET_REG, 0x1);
  (void) hh_read(HH_RESET_REG);
  hh_write(HH_RESET_REG, 0x0);
  sginap(15);

  hh_write(HH_ARM_CTRL_REG, ARM_CTRL_HOST_ACCESS_MASK);
  ha_write(HA_REMAP_EPROM_REG, 0x0);
  address = HA_EEPROM + BOOTHDR_OFFSET;
  limit = (unsigned int) (address + sizeof(bhdr));

  ttyprintf("address = %x, limit = %x\n", address, limit);
  p = (unsigned int *) &b;
  while (address < limit) {
    ha_read(address, *p++);
    address += 4;
    DELAY(2);
  }

  ttyprintf("EEPROM state = %x\n", b.state);
  return b.state;
}

#endif /* ifdef SN0 */
