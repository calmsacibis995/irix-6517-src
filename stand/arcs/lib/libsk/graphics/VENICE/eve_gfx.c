#include <stdarg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/fchip.h>
#include <sys/mips_addrspace.h>
#include <arcs/hinv.h>
#include "standcfg.h"
#include "arcs/folder.h"
#include "arcs/cfgdata.h"
#include "arcs/cfgtree.h"

#include "arcs/io.h"


#define	PROM_HIG_1	0xdd
#define	PROM_HIG_2	0xee
#define	HIG_PROM_ID	0x000011ee

#define TEXTPORT_PIPE_NUM 	0	/* Have textport on pipe 0 */

#define COMPATIBLE_PROTOCOL_REV	0xd	/* protocol rev that match this version of IO4 prom */

#define ADDR_OFFSET	0x81700000      /* base address of things in the GE10 prom */
extern long ucode_tab[];


void *alloc_gfxspace(long, long);
void flush_cache(void);
void debug(char *str);
int  probe_for_venice(COMPONENT *root, int pipe_num);

extern int ttyprintf(const char *, ...);
extern int printf(const char *fmt, ...);
extern unsigned get_pgmask(void);
extern void set_pgmask(unsigned);
extern void tlbwired(int, int, caddr_t, unsigned int, unsigned int);
extern char *strcpy(char *, const char *);
extern char *strcat(char *, const char *);
extern COMPONENT *AddChild(COMPONENT *, COMPONENT *, void *);
extern long RegisterDriverStrategy(COMPONENT *, STATUS (*)(COMPONENT *self, IOBLOCK *));
extern int _gfx_strat(COMPONENT *, IOBLOCK *);
extern int cpuid(void);
extern void us_delay(int);
#undef load_double
extern long long load_double(long long *);
extern void store_double_rtn(long long *, long long);
extern long long __ll_lshift(long long, long long);
extern void bzero(void *, long);
extern void bcopy(const void *, void *, long);
extern void ev_flush_caches(void);
extern int _venice_install(COMPONENT *root, int *window_base);
extern void *VeniceProbe(int init_flag, int *window_base);


extern char *GetEnvironmentVariable(char *name);
extern long SetEnvironmentVariable(char *name, char *val);


extern unsigned char clogo_data[];
extern int clogo_w;
extern int clogo_h;
static int tport_window, tport_adap;
#if defined(IP21) || defined(IP25)
void fifocmd(int, int);
void fifodata(int, int);
void wg_flush(void);
#endif /* IP21 || IP25 */

#define MAXPIPENUM	2	/* maximum three pipes, 0, 1, and 2 */


/*#define DEBUG	1*/
#ifdef DEBUG
#define xprintf ttyprintf
#else
/* ARGSUSED */
void xprintf(char *fmt, ...)
{
  /* don't do anything */
}
#endif

#if defined(IP21) || defined(IP25)
volatile union venice_pipe {
	float f;
	unsigned int i;
	long long ul;
} *GE;
#endif	/* IP21 || IP25 */

			    
/*
 * Given a logical pipe number, find the physical slot
 * number of the IO4 attached to the given pipe, and return
 * the window number as well as the adapter number that is
 * associated with the graphics pipe.
 *
 * Returns:
 *	-1	Can't find the IO4 connected to the given graphics
 *		pipe.
 *	Physical slot number of the IO4 connected to the given pipe.
 */
static int
find_io4(int pipe_num, long *window, long *adapter)
{
  evcfginfo_t *cfginfo = EVCFGINFO;
  evbrdinfo_t *brd;
  int slot;
  int pipe_found = 0;
  int adap;
  int masterid;
  /*
   * Start from the highest physical slot number.  The convention
   * is that the first graphics pipe is attached to the IO4 located
   * at the highest physical slot.  The last graphics pipe is attached
   * to the IO4 at the lowest physical slot.
   */
  xprintf("find_io4(0x%x,0x%x,0x%x)\n",pipe_num,window,adapter);
  for ( slot=EV_MAX_SLOTS ; slot-- ; ) 
    {
      brd = &(cfginfo->ecfg_board[slot]);
      if ((brd->eb_type & (EVCLASS_MASK|EVTYPE_MASK)) == EVTYPE_IO4
	  && (brd->eb_enabled)) 
	{
	  xprintf(" found enabled io4 in slot 0x%x\n", slot);
	  
	  /* Found an IO4 board and it is enabled. 
	   * Now look at all the adapters in the IO4, and look
	   * for the F chip.  If a F chip is found, see if it is
	   * connected to graphics.  If it is, this IO4 is connected
	   */
	  for (adap=1; adap<=7; adap++)   /* adapter numbers from 1 to 7 */
	    { 
	      if (brd->eb_un.ebun_io.eb_ioas[adap].ioa_type == IO4_ADAP_FCHIP) 
		{
		  masterid = brd->eb_un.ebun_io.eb_ioas[adap].ioa_subtype;
		  if (masterid == 0x20) 
		    {
		      /* Master id == 0x2X, must be graphics */
		      xprintf(" found pipe 0x%x, looking for pipe 0x%x\n",
			       pipe_found,pipe_num);
		      
		      if (pipe_found == pipe_num) 
			{
			  /* This is the pipe we are looking for */
			  *window = brd->eb_un.ebun_io.eb_winnum;
			  *adapter = adap;
			  return(slot);
			}
		      else 
			{
			  /* We found a graphics pipe, but it
			   * isn't the pipe we are looking for, try again.
			   */
			  
			  pipe_found++;
			}
		    }
		}
	    }
	}
      
    }
  return -1;
}


/*
 * functions to read eprom from a hig
 */
static unsigned int
promload8(int addr, int type, long long *base)
{
    switch (type)  {
    case PROM_HIG_2:
#if defined(IP21) || defined(IP25)
        return (*(base+addr) & 0xff);
#else      
        return (int)(load_double(base + addr) & 0xff);
#endif
    default:
        return 0xffffffff;
    }
}

static int
promload32(int addr, int type, long long *base)
{
    return   promload8(addr + 0, type, base) << 24
           | promload8(addr + 1, type, base) << 16
           | promload8(addr + 2, type, base) <<  8
           | promload8(addr + 3, type, base);
}


int
venice_install(COMPONENT *root)
{	
	int pipe_num,rv=0;

	xprintf("YO YO YO! rtc == 0x%x, wginput == 0x%x\n", EV_RTC, EV_WGINPUT_BASE);
	for (pipe_num = MAXPIPENUM; pipe_num >= 0; pipe_num--) {
		rv |= probe_for_venice(root,pipe_num);
	}

	return rv;
}



/*
 * Hardware Independant Graphics
 *
 * Look for a HIG graphics adapter (i.e. grinch or venice).
 * If it's out there, load the gfx driver code from the graphics
 * board onto local memory.
 *
 * To allow access to the entire BIG WINDOW for the graphics, we
 * temporarily enter 32bit address mode, and use a wired TLB entry
 * to map us the entire BIG WINDOW.
 * 
 * returns 0 if no graphics is found, returns 1 if graphics is found.
 */
long long *tport_prom_base = NULL, *ven_window_base = NULL;
int        interface_type;
int        ven_prom_ident, ven_prom_size, ven_prom_rev;
int	   ven_io4slot;
long	   ven_window, ven_adapter;

int
probe_for_venice(COMPONENT *root, int pipe_num)
{
    long long     *higidreg, *prom_base, *window_base;
    long           prom_offset, window,adapter, idreg;
    int            io4slot;


    /* Don't bother if the NOGFX switch is set */
    if (EVCFGINFO->ecfg_debugsw & VDS_NOGFX)
        return 0;

    xprintf("probe_for_gfx(0x%lx,0x%x)\n", root,pipe_num);

    /* find the everest bus slot with IO4 that has the graphics pipe we want */
    if  ((io4slot=find_io4(pipe_num,&window,&adapter)) < 0) {
	/* Not found */
	return 0;
    }
    xprintf(" probe_for_gfx: io4slot=0x%x, window=0x%x, adapter=0x%x\n",
	    io4slot,window,adapter);

    ven_window_base = window_base = (long long *)alloc_gfxspace(window,adapter);

    /* get address of hardware-independant-graphics id register*/
    higidreg = window_base;
    higidreg += 0x1000;
    xprintf(" probe_for_gfx: higidreg=0x%x\n", higidreg);

    /* see if it matches hardware-indep. protocol */
#if defined(IP21) || defined(IP25)
    idreg = *higidreg;
    idreg &= 0xffffffff;
#else    
    idreg = load_double(higidreg);
#endif    
    xprintf(" probe_for_gfx: idreg=0x%x\n", idreg);
    
    if  (((idreg>>24) & 0xff) == PROM_HIG_1) {
	interface_type = PROM_HIG_1;
    } else if (((idreg>>24) & 0xff) == PROM_HIG_2) {
	interface_type = PROM_HIG_2;
    } else {
	xprintf("probe_for_gfx: unknown protocol  idreg=0x%x\n", idreg);
	return 0;			/* unknow protocol */
    }

    /* found something; see if it is indeed a hig board */
    prom_offset = idreg & 0xffffff;

    prom_base = window_base;
    prom_base += prom_offset;
    xprintf(" prom_address = 0x%x\n",  prom_base);
    

    /* now check out the eprom */
    ven_prom_ident = promload32(0, interface_type, prom_base);
    xprintf(" ven_prom_ident = 0x%x\n",  ven_prom_ident);
    if  ((ven_prom_ident & 0xffff) != HIG_PROM_ID) {
	/*
	 * ERROR: this is _not_ our type of eprom.
	 * assume that we do _not_ have hig.
	 */
	xprintf(" probe_for_gfx: unknown 0x%x\n", ven_prom_ident);
	return 0;
    }

    /* Check to make sure the revision of GE prom is compatible with us */
    ven_prom_rev  = promload32(12, interface_type, prom_base);
    if ((ven_prom_rev & 0xffff) != COMPATIBLE_PROTOCOL_REV) {
	xprintf("GE10prom protocol level %d is incompatible, need level %d\n",
		ven_prom_rev&0xffff,COMPATIBLE_PROTOCOL_REV);
	return 0;
    }
    xprintf("ven_prom_rev = 0x%x\n",ven_prom_rev);


    /* If we just found the pipe on which the textport should come
     * up, mark it so that the device specific textport codes
     * knows of the hardware addresses.
     */
    if (pipe_num == TEXTPORT_PIPE_NUM) {
    	tport_prom_base = prom_base;
	ven_io4slot = io4slot;
	ven_adapter = adapter;
	ven_window  = window;
	xprintf("textport pipe: base 0x%x, io4slot %d, adapter %d, window 0x%x\n",
		prom_base, io4slot, adapter, window);
    }

    /* We have found a graphics pipe. Call the install routine so that the
     * graphics pipe is reset and is know to hinv.
     */
    xprintf("calling venice_install(0x%x, 0x%x)\n", root, window_base);
    _venice_install(root, (int*)window_base);

    return 1;
}

extern struct htp_fncs venice_tp_fncs;

void *
venice_probe(struct htp_fncs **fncs)
{
    long long *prom_base;
    unsigned long _prom_ucode_tab;
    int *prom_ucode_tab, i;
    unsigned int *ramaddress, word;
    unsigned int prom_cksum, prom_csum=0, prom_start, prom_addr;

    if (fncs) {
	*fncs = &venice_tp_fncs;
	return 0;
    }

    prom_base = tport_prom_base;
    if (prom_base == NULL) {
	xprintf("venice_probe: prom_base is NULL\n");
	return 0;
    }
    xprintf("venice_probe: prom_base 0x%x\n", prom_base);

    ramaddress = (unsigned int *) GFXPROM_BASE;
    xprintf(" ramaddress == 0x%x\n",  ramaddress);
    
    *ramaddress++ = ven_prom_ident;
    
    ven_prom_size  = promload32(4, interface_type, prom_base);
    *ramaddress++ = ven_prom_size;
    xprintf(" ven_prom_size = 0x%x\n", ven_prom_size);
    
    prom_cksum = promload32(8, interface_type, prom_base);
    *ramaddress++ = prom_cksum;
    xprintf(" prom_cksum = 0x%x\n", prom_cksum);
    
    *ramaddress++ = ven_prom_rev;
    xprintf(" ven_prom_rev = 0x%x\n", ven_prom_rev);
    
    
    /* we no longer use the probe/install routines or the callout addr */
    *ramaddress++ = (__psint_t)0xbadc0de7;
    *ramaddress++ = (__psint_t)0xdeadc0de;
    *ramaddress++ = (__psint_t)0xdeadbeef;

    _prom_ucode_tab = promload32(28, interface_type, prom_base);
    *ramaddress++   = (unsigned int)_prom_ucode_tab - ADDR_OFFSET;
    xprintf(" prom_ucode_tab = 0x%x\n", _prom_ucode_tab);


    xprintf("checksumming prom....");
    for ( prom_addr=16 ; prom_addr<ven_prom_size  ; prom_addr+=sizeof(int) ) {
      word = promload32(prom_addr, interface_type, prom_base);
      prom_csum += (word & 0x000000ff);
      prom_csum += (word & 0x0000ff00) >> 8;
      prom_csum += (word & 0x00ff0000) >> 16;
      prom_csum += (word & 0xff000000) >> 24;
    }
    xprintf("done.\n");
    
    if (prom_csum != prom_cksum) {
      /* ERROR: checksum failure. Uh-oh. */
      xprintf("prom checksum failure: prom sez = 0x%x, but computed = 0x%x\n",
	      prom_cksum,prom_csum); 
      return 0;
    }
    
    /* Copy the PROM into local memory */
    xprintf("copying data....");

    prom_start = 32;
    for (prom_addr=prom_start; prom_addr < ven_prom_size; prom_addr+=sizeof(int)) {
      word = promload32(prom_addr, interface_type, prom_base);
      *ramaddress++ = word; 
    }
    xprintf("done.\n");
    

    /* now copy and patch up the ucode pointer table */
    prom_ucode_tab = (int *)((ulong)GFXPROM_BASE + ((_prom_ucode_tab & 0xffffffff) - ADDR_OFFSET));
    for(i = 0; i < 24; i++) {
	ucode_tab[i] = (long)((ulong)GFXPROM_BASE + (prom_ucode_tab[i] - ADDR_OFFSET));
	xprintf("ucode_tab[%d] = 0x%x, orig 0x%x\n", i, ucode_tab[i], prom_ucode_tab[i]);
    }
    

#ifdef IP21
    GE = (volatile union venice_pipe *)(0x8000000018300000);
#else
#ifdef IP25
    GE = (volatile union venice_pipe *)(0xbe00000018300000);
#endif
#endif

    xprintf("venice_probe: calling VeniceProbe(1, 0%x)\n", ven_window_base);
    VeniceProbe(1, (int*)ven_window_base);

    xprintf("probe_for_gfx returns 1\n");
    return ven_window_base;

}




/* This routine maps virtual address 0xc0000000 to 0xc0ffffff to
 * first 16Mb of the big window space of given window number and adapter
 * number.
 * Returns:
 *	virtual address that maps onto the large window
 */
/* ARGSUSED */
void
*alloc_gfxspace(long window, long adapter)
{
#if _MIPS_SIM != _ABI64
	unsigned 	old_pgmask;
	unsigned int	pteevn, pteodd;
	caddr_t		va;

        #define IOMAP_TLBPAGEMASK       0x1ffe000
        va = (caddr_t)(__psint_t)(unsigned)0xf0000000;

        pteevn = ((LWIN_PFN(window,adapter) ) << 6) | (2 << 3) | (1 << 2) | (1 << 1) | 1;
        pteodd = 0;

        old_pgmask = get_pgmask();
        set_pgmask(IOMAP_TLBPAGEMASK);
        tlbwired(0,0,(caddr_t)va,pteevn,pteodd);  /* Use entry 0 only, it is reserved for graphics */
        set_pgmask(old_pgmask);

        return((void *)va);
#else
	return (void *)(PHYS_TO_K1(LWIN_PFN(window,adapter) << 12));
#endif

}



/*
 * Initialize Write gatherer hardware on the running CPU.
 * Should be called only once for each cpu.
 * Expects tport_window and tport_adap   to have the values of
 * the window and adapter no of the graphics hardware acting as 
 * the text port.
 */
void init_WG(void)
{
        long long ll;

#ifdef IP19
        /* Set it to Big endian, disabled */
        store_double((long long *)EV_WGCNTRL,(long long)2);
        /* Clear the write gatherer */
        store_double((long long *)EV_WGCLEAR,(long long)0);
#endif
#ifdef IP25
        /* WG: enabled, swapping, iff uncached accelerated */
	ll = 3;
        store_double((long long *)EV_GRCNTL,ll);
#endif

        /* Set up destination of the write gatherer */
        ll = (long long)(LWIN_PFN(tport_window, tport_adap));
        ll <<= 12;
        ll |= (cpuid() << 8) | (1<<15);
#ifdef IP25
        store_double((long long *)EV_GRDST,ll);
#else
        store_double((long long *)EV_WGDST,ll);
#endif


#ifdef IP19
        /* Enable write gatherer */
        store_double((long long *)EV_WGCNTRL,(long long)3);
#endif
}

/* Routine to initialize the Graphics related hardware for slave cpus.
 */
static unchar slaveinit[EV_MAX_CPUS];

void
evslave_gfxinit(int id)
{
        if (slaveinit[id])      /* Already done. */
                return;

        /* Ensure master has done textport init... */
        while (!tport_window && !tport_adap);

        init_WG();
        (void)alloc_gfxspace( tport_window,  tport_adap);
        slaveinit[id] = 1;
}

#if defined(IP21) || defined(IP25)
void
fifocmd(int cmd, int val)
{	
	GE[0].i = cmd;
	GE[0].i = val;
}

/* ARGSUSED */
void
fifodata(int cmd, int val)
{
	GE[0].i = val;
}

void
wg_flush(void)
{
#if defined(IP21)
	GE[4].i = 0;
#elif defined(IP25)
	/* this should just be an uncached load to simply */
	/* flush the T5's uncached accelerated buffer     */
	(void)EV_GET_LOCAL(EV_RTC);
#endif
}
#endif
