#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/fchip.h>
#include "standcfg.h"
#include "arcs/hinv.h"
#include "arcs/folder.h"
#include "arcs/cfgdata.h"
#include "arcs/cfgtree.h"
#include "arcs/prom_callout.h"

#define	PROM_HIG_1	0xdd
#define	PROM_HIG_2	0xee
#define	HIG_PROM_ID	0x000011ee


int addr32bit();
void oldaddr(int);
void *alloc_gfxspace(int, int);

extern int get_SR();
extern void set_SR(int);
extern int ttyprintf(const char *, ...);
extern unsigned get_pgmask(void);
extern void set_pgmask(unsigned);
extern void tlbwired(int, int, caddr_t, unsigned int, unsigned int);
extern char *strcpy(char *, const char *);
extern char *strcat(char *, const char *);
extern int strlen(const char *);
extern COMPONENT *AddChild(COMPONENT *, COMPONENT *, void *);
extern long RegisterDriverStrategy(COMPONENT *, STATUS (*)(COMPONENT *self, IOBLOCK *));
extern int _gfx_strat(COMPONENT *, IOBLOCK *);
extern int cpuid(void);
extern void us_delay(int);
extern long long load_double(long long *);
extern void store_double(long long *, long long);
extern long long __ll_lshift(long long, long long);
extern void bzero(void *, long);
extern void bcopy(const void *, void *, long);

extern unsigned char clogo_data[];
extern int clogo_w;
extern int clogo_h;

#define MAXPIPENUM	2	/* maximum three pipes, 0, 1, and 2 */

#if 0
#define xprintf ttyprintf
#else
#define xprintf
#endif

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
find_io4(int pipe_num, int *window, int *adapter)
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
    for ( slot=EV_MAX_SLOTS ; slot-- ; ) {
	brd = &(cfginfo->ecfg_board[slot]);
	if  ( (brd->eb_type & (EVCLASS_MASK|EVTYPE_MASK)) == EVTYPE_IO4
	   && (brd->eb_enabled)
	    ) {
xprintf("found enabled io4 in slot %d\n",slot);
	    /* Found an IO4 board and it is enabled. 
	     * Now look at all the adapters in the IO4, and look
	     * for the F chip.  If a F chip is found, see if it is
	     * connected to graphics.  If it is, this IO4 is connected
	     */
	    for (adap=1; adap<=7; adap++) { /* adapter numbers from 1 to 7 */
		if (brd->eb_un.ebun_io.eb_ioas[adap].ioa_type == IO4_ADAP_FCHIP) {
			masterid = brd->eb_un.ebun_io.eb_ioas[adap].ioa_subtype;
			if (masterid == 0x20) {
				/* Master id == 0x2X, must be graphics */
xprintf("found pipe %d, looking for pipe %d\n",pipe_found,pipe_num);
				if (pipe_found == pipe_num) {
					/* This is the pipe we are looking for */
					*window = brd->eb_un.ebun_io.eb_winnum;
					*adapter = adap;
					return(slot);
				}
				else {
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
 * function to find the instantiation node of the generic graphics node.
 */
static struct standcfg *
find_specific_gfx(int (*p_func)())
{
    extern struct standcfg standcfg[];
    struct standcfg *p, *c;

    for ( p=standcfg ; p->install ; p++ )
	if  (p->gfxprobe == p_func)  {
	    return ++p;
	}

    return 0;
}

/*
 * functions to read eprom from a hig
 */
static unsigned long
promload8(int addr, int type, long long *base)
{
    switch (type)  {
    case PROM_HIG_2:
        return (load_double(base + addr) & 0xff);

    default:
        return 0xffffffff;
    }
}

static unsigned long
promload32(int addr, int type, long long *base)
{
    return   promload8(addr + 0, type, base) << 24
           | promload8(addr + 1, type, base) << 16
           | promload8(addr + 2, type, base) <<  8
           | promload8(addr + 3, type, base);
}

int
generic_gfx_probe(int x)
{
xprintf("generic_gfx_probe\n");
	return 0;
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
int
probe_for_gfx(COMPONENT *root, int pipe_num)
{
    static prom_callout_t hig_callout;
    struct standcfg *spec_gfx;
    long long *higidreg;
    long long *prom_base;
    long long *window_base;
    unsigned long *ramaddress, word;
    unsigned long prom_graphics_probe, prom_graphics_install;
    long idreg;
    unsigned int prom_cksum;
    unsigned int prom_csum=0;
    int prom_ident, prom_size, prom_rev;
    int io4slot;
    int interface_type;
    int prom_addr;
    int prom_offset;
    int sr;
    int window,adapter;

xprintf("probe_for_gfx(0x%x,0x%x)\n",root,pipe_num);

    sr = addr32bit();

    /* find the everest bus slot with IO4 that has the graphics pipe we want. */
    if  ((io4slot=find_io4(pipe_num,&window,&adapter)) < 0) {
	/* Not found */
	oldaddr(sr);
	return 0;
    }
xprintf("probe_for_gfx: io4slot=0x%x, window=0x%x, adapter=0x%x\n",io4slot,window,adapter);

    window_base = (long long *)alloc_gfxspace(window,adapter);

    /* look for hardware-independant-graphics id register*/
    higidreg = window_base;


    higidreg += 0x1000;
xprintf("probe_for_gfx: higidreg=0x%x\n",higidreg);

    /* something found; see if it matches hardware-indep. protocol */
    idreg = load_double(higidreg);
xprintf("probe_for_gfx: idreg=0x%x\n",idreg);
    if  (((idreg>>24) & 0xff) == PROM_HIG_1) {
	interface_type = PROM_HIG_1;
    }
    else if (((idreg>>24) & 0xff) == PROM_HIG_2) {
	interface_type = PROM_HIG_2;
    }
    else {
xprintf("probe_for_gfx: unknown protocol idreg=0x%x\n",idreg);
	oldaddr(sr);
	return 0;			/* unknow protocol */
    }

    /* found something; see if it is indeed a hig board */
xprintf("idreg = 0x%x\n", idreg);
    prom_offset = idreg & 0xffffff;
xprintf("prom_offset = 0x%x\n", prom_offset);
    prom_base = window_base;
xprintf("prom_address = 0x%x\n", prom_base);
    prom_base += prom_offset;

    /* now check out the eprom */
xprintf("prom_address = 0x%x\n", prom_base);
    prom_ident = promload32(0, interface_type, prom_base);
xprintf("prom_ident = 0x%x\n", prom_ident);
    if  ((prom_ident & 0xffff) != HIG_PROM_ID) {
	/*
	 * ERROR: this is _not_ our type of eprom.
	 * assume that we do _not_ have hig.
	 */
xprintf("probe_for_gfx: unknown 0x%x\n",prom_ident);
	oldaddr(sr);
	return 0;
    }

    /*
     * have the callout structure ready 
     */
    hig_callout.get_pgmask = get_pgmask;
    hig_callout.set_pgmask = set_pgmask;
    hig_callout.tlbwired = tlbwired;
    hig_callout.strcpy = strcpy;
    hig_callout.strcat = strcat;
    hig_callout.strlen = strlen;
    hig_callout.AddChild = AddChild;
    hig_callout.RegisterDriverStrategy = RegisterDriverStrategy;
    hig_callout._gfx_strat = _gfx_strat;
    hig_callout.cpuid = cpuid;
    hig_callout.load_double = load_double;
    hig_callout.store_double = store_double;
    hig_callout.__ll_lshift = __ll_lshift;
    hig_callout.bzero = bzero;
    hig_callout.bcopy = bcopy;
    hig_callout.get_SR = get_SR;
    hig_callout.set_SR = set_SR;
    hig_callout.us_delay= us_delay;
    hig_callout.printf = ttyprintf;
    hig_callout.clogo_data_addr = clogo_data;
    hig_callout.clogo_w_addr = &clogo_w;
    hig_callout.clogo_h_addr = &clogo_h;

    /*
     * we have hig - copy over eprom to ram for execution.
     * 1) prom id
     * 2) prom size
     * 3) prom checksum
     * 4) prom revision
     * 5) address of the graphics-specific probe routine
     * 6) address of the graphics-specific install routine
     */
    ramaddress = (unsigned long *) GFXPROM_BASE;
    *ramaddress++ = prom_ident;
    prom_size  = promload32(4, interface_type, prom_base);
    *ramaddress++ = prom_size;
    prom_cksum = promload32(8, interface_type, prom_base);
    *ramaddress++ = prom_cksum;
    prom_rev   = promload32(12, interface_type, prom_base);
    *ramaddress++ = prom_rev;
    /*
     * we start from address 16 so that we can share flashprom
     * code with other host architecture.
     */
    prom_graphics_probe = promload32(16, interface_type, prom_base);
    *ramaddress++ = prom_graphics_probe;
    prom_graphics_install = promload32(20, interface_type, prom_base);
    *ramaddress++ = prom_graphics_install;
    *ramaddress++ = &hig_callout;

    for ( prom_addr=16 ; prom_addr<prom_size  ; prom_addr+=4 ) {
        word = promload32(prom_addr, interface_type, prom_base);
        prom_csum += (word & 0x000000ff);
        prom_csum += (word & 0x0000ff00) >> 8;
        prom_csum += (word & 0x00ff0000) >> 16;
        prom_csum += (word & 0xff000000) >> 24;
    }


    if (prom_csum != prom_cksum) {
	/* ERROR: checksum failure. Uh-oh. */
	xprintf("gfx: prom checksum failure\n");
	xprintf("PROM's checksum = 0x%x, computed chksum = 0x%x\n",prom_cksum,prom_csum); 
	oldaddr(sr);
	return 0;
    }

    /* Copy the PROM into local memory */
    for ( prom_addr=28 ; prom_addr<prom_size  ; prom_addr+=4 ) {
        word = promload32(prom_addr, interface_type, prom_base);
	*ramaddress++ = word;
    }

    /*
     * successfully read the flashprom.  Now the tricky code:
     * replace the generic probe and install routines with the
     * ones from the graphics PROM code.
     */
    if  (! (spec_gfx = find_specific_gfx(generic_gfx_probe)))  {
	xprintf("can't find specific gfx in standcfg\n");
	oldaddr(sr);
	return 0;
    }


    spec_gfx->gfxprobe = (int (*)())prom_graphics_probe,
    spec_gfx->install = (int (*)())prom_graphics_install;
xprintf("prom_graphics_probe addr = 0x%x\n",prom_graphics_probe);
xprintf("prom_graphics_install addr = 0x%x\n",prom_graphics_install);
xprintf("probe_for_gfx returns 0\n");
    oldaddr(sr);
    return 1;
}

int
generic_gfx_install(COMPONENT *root)
{	
	int pipe_num;
xprintf("generic_gfx_install\n");
	for (pipe_num = MAXPIPENUM; pipe_num >= 0; pipe_num--) {
		probe_for_gfx(root,pipe_num);
	}
}



int
specific_gfx_probe(void)
{
xprintf("specific_gfx_probe returns 0\n");
	return(0);
}

int
specific_gfx_install(void)
{
xprintf("specific_gfx_install returns 0\n");
	return(1);
}

#ifdef IP19
#undef SR_KX
#define SR_KX	0x80	
addr32bit()
{
	int status;

	status = get_SR();
	set_SR(status & ~SR_KX);
	return(status);
}

void
oldaddr(int status)
{
	set_SR(status);
}
#else
addr32bit()
{
}

void oldaddr(int status)
{
}
#endif

/* This routine maps virtual address 0xc0000000 to 0xc0ffffff to
 * first 16Mb of the big window space of given window number and adapter
 * number.
 * Returns:
 *	virtual address that maps onto the large window
 */
void
*alloc_gfxspace(int window, int adapter)
{
	unsigned 	va,old_pgmask;
	unsigned int	pteevn, pteodd;
	#define IOMAP_TLBPAGEMASK	0x1ffe000
	va = (unsigned)0xc0000000;

	pteevn = ((LWIN_PFN(window,adapter) ) << 6) | (2 << 3) | (1 << 2) | (1 << 1) | 1;
	pteodd = 0;

	old_pgmask = get_pgmask();
	set_pgmask(IOMAP_TLBPAGEMASK);
	tlbwired(0,0,(caddr_t)va,pteevn,pteodd);  /* Use entry 0 only, it is reserved for graphics */
	set_pgmask(old_pgmask);

	return((void *)va);
}

