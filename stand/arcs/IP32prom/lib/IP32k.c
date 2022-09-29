
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(IP32SIM)
#define Sim 1
#else
#undef Sim
#endif

#include <arcs/hinv.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/crime.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/mace.h>
#include <sys/IP32flash.h>
#include <sys/ds17287.h>
#include <sys/param.h>
#include <arcs/folder.h>
#include <arcs/cfgtree.h>
#include <fault.h>
#include <assert.h>
#include <libsc.h>
#include <trace.h>
#include <flash.h>

#include <sys/types.h>
#include <sys/debug.h>
#if 0
#include <sys/systm.h>		/* for us_delay() */
#include <sys/param.h>		/* for DELAY() */
#endif

#include <sys/crime.h>		/* from irix/kern, for CRM_BASEADDR */
#include <sys/crimedef.h>
#include <sys/crimereg.h>
#include <sys/crime_gfx.h>
#include <sys/crime_gbe.h>
#include <sys/mips_addrspace.h>	/* for PHYS_TO_K1 */
#include <hw_copy.h>
#include <crm_stand.h>


#if !defined(__inttypes_INCLUDED)
typedef	unsigned char		uint8_t;
typedef	unsigned short		uint16_t;
typedef	unsigned int		uint32_t;
typedef	unsigned long long int	uint64_t;
typedef unsigned long long int	uintmax_t;
typedef unsigned long int	uintptr_t;
typedef	signed long long int	int64_t;
#endif

extern unsigned _dcache_size;
extern unsigned _icache_size;
extern unsigned _sidcache_size;
extern unsigned _dcache_linesize;
extern unsigned _icache_linesize;
extern unsigned _scache_linesize;
extern unsigned _r4600sc_sidcache_size;

extern __dcache_inval(void*,int);
extern void ds2502_get_eaddr(char*)  ;
extern void DPRINTF (char *, ...) ;
extern void usecwait(int);
extern void us_delay(int);
extern void flushbus(void);
extern int ticksper1024inst(void);
extern int cpuclkper100ticks(unsigned int*);
unsigned int cpu_get_memsize(void);
void         cpu_mem_init   (void);
extern uint32_t r4k_getticker(void);

#if 0
#define C0_IMP_R10000		0x9
#endif

extern struct gbechip *crm_GBE_Base;
extern int i2cfp_PanelOff(struct gbechip *);
extern int usingFlatPanel;

/*****************************************************************************
 *                        Misc. IP32 routines                                *
 *****************************************************************************/

#if 0
int cachewrback = 1;		/* Mark cache as writeback */
#endif

/*****************************************************************************
 *                     Misc. IP32 cpu_xxx routines                           *
 *****************************************************************************/

/*************
 * cpu_errputc
 *------------
 * Call appropriate errputc.
 */
void
cpu_errputc(char c)
{
  extern void mace_errputc(char);
  mace_errputc(c);
}

/*************
 * cpu_acpanic
 *------------
 */
void
cpu_acpanic(char * str)
{
  printf("Cannot malloc %s component of config tree.\n",str);
  printf("Incomplete config tree -- cannot continue.\n");
}

/******************
 * cpu_get_disp_str
 *-----------------
 * Return display path
 */
char*
cpu_get_disp_str(void)
{
  return "video()";
}

/****************
 * cpu_get_serial
 *---------------
 * Return serial path
 */
char*
cpu_get_serial(void)
{
  return "serial(0)";
}

/*****************
 * cpu_get_kbd_str
 *----------------
 * Return keyboard path
 */
char*
cpu_get_kbd_str(void)
{
  return "keyboard()";
}

/***************
 * cpu_get_mouse
 *--------------
 */
char*
cpu_get_mouse(void)
{
  return "pointer()";
}

/***************
 * cpu_get_eaddr
 *--------------
 * Return sether address in the form of
 * 0800aabbccdd (08:00:aa:bb:cc:dd)
 */
void
cpu_get_eaddr(char* eaddr)
{
  ds2502_get_eaddr(eaddr) ;
  return ;
}

/******
 * htoa
 *-----
 * convert 4 digit hex numbet to ascii
 */
char
htoa(short digit)
{
  char num ;
  if (digit > 9)
    num = (digit - 10) + 0x61 ;
  else
    num = digit + 0x30 ;
  return num ;
}

/*******************
 * cpu_get_eaddr_str
 *------------------
 * Return sether address in the form of
 * 0800aabbccdd (08:00:aa:bb:cc:dd)
 */
void
cpu_get_eaddr_str(char* eaddr)
{
  unsigned char addr [6], tmp ;
  int           idx1, idx2 ; 
  ds2502_get_eaddr(addr) ;
  for (idx1=0,idx2=0; idx2<6; idx1+=3,idx2+=1) {
    eaddr[idx1]   = htoa((addr[idx2] & 0xf0)>> 4) ;
    eaddr[idx1+1] = htoa(addr[idx2] & 0x0f) ;
    if (idx1 < 15) eaddr[idx1+2] = ':' ;
  }
  return ;
}

extern void crm_hardreset(void);
extern void crm_softreset(void);

/***************
 * cpu_hardreset
 * -------------
 * Write to crime control register hardreset bit
 * to cause system wide hardreset.
 */
void
cpu_hardreset(void)
{
  crm_hardreset() ;
  panic ("cpu_harreset: Can not reset the system\n");
  
}
  

/***************
 * cpu_softreset
 * -------------
 * Write to crime control register softreset bit
 * to cause system wide softreset.
 */
void
cpu_softreset(void)
{
  crm_softreset();
  panic ("cpu_softreset: Can not reset the system\n");
  
}
  
/***********
 * cpu_reset
 * ---------
 * This function was called by _init_saio to initialize
 * MP spinlock subsystem, we'll have to figure this out for
 * MP MOOSEHEAD system.  For now simply return is sufficient.
 */
void
cpu_reset(void)
{
  return;
}

/**********
 * rtc_init
 * --------
 * make sure the clock is in the right mode:
 *     Kickstart and wakeup flags cleared
 *     Kickstart interrupt enabled
 *     Wakeup interrupt disabled
 */
extern int _rtcinitted ; 
void
cpu_rtcinit(void)
{
  volatile register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
  int rega;
  
  rega = clock->registera & ~DS_REGA_DV2;  /* clear rega dv2 */
  rega |= DS_REGA_DV1;                     /* set   rega dv1 */
  clock->registera = rega | DS_REGA_DV0;   /* select bank1   */
  flushbus();
  clock->ram[DS_BANK1_XCTRLB] &= ~DS_XCTRLB_WIE;
  clock->ram[DS_BANK1_XCTRLB] |= (DS_XCTRLB_KSE|DS_XCTRLB_ABE);
  clock->ram[DS_BANK1_XCTRLA] &= ~(DS_XCTRLA_KF|DS_XCTRLA_WF);
  clock->registera &= ~DS_REGA_DV0;
  flushbus();
  _rtcinitted = 1;
  return ;
}

/***************
 * cpu_powerdown
 * -------------
 * shut down the power supply.  Dallas docs indicate that the following
 * conditions must hold for kickstart to work:
 *
 *	dv 2/1/0 in register a must have a value of 01xb
 *	((kf & kse) != 1) && ((wf & wie) != 1)
 *      kse == 1 to enable powerup via the power switch
 *
 * I make sure that these conditions hold here and then set pab to 1
 * which should shutdown the power supply.
 */
extern int _rtcinitted ;
void
cpu_powerdown(int setwie)
{
  volatile register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
  int rega;

  rega = clock->registera & ~DS_REGA_DV2;  /* clear rega dv2 */
  rega |= DS_REGA_DV1;  /* set dv1, and set DV0 to select bank1 */
  while (1) {
    clock->registera = rega | DS_REGA_DV0; 
    flushbus();
    if (setwie != 0x0)
      clock->ram[DS_BANK1_XCTRLB] |= DS_XCTRLB_WIE|DS_XCTRLB_ABE|DS_XCTRLB_KSE;
    else {
      clock->ram[DS_BANK1_XCTRLB] &= ~DS_XCTRLB_WIE;
      clock->ram[DS_BANK1_XCTRLB] |= DS_XCTRLB_ABE|DS_XCTRLB_KSE;
    }
    do {
#if defined(DEBUG)
      _rtcinitted = 0;  /* turned off the scandev checking. */
      printf (" *** Clearing the KF and WF flags ***\n");
#endif      
      clock->ram[DS_BANK1_XCTRLA] &= ~(DS_XCTRLA_KF|DS_XCTRLA_WF);
      flushbus();
      DELAY(1000000);
    } while((clock->ram[DS_BANK1_XCTRLA] & (DS_XCTRLA_KF|DS_XCTRLA_WF))!=0) ;

    if (usingFlatPanel)
    {
       i2cfp_PanelOff( crm_GBE_Base );
    }
      
    clock->ram[DS_BANK1_XCTRLA] |= DS_XCTRLA_PAB; /* shut down power */
    clock->registera = rega;
    flushbus();
    DELAY(1000000);
    printf(" *** Power supply did not shut down ***\n");
    DELAY(1000000);
  }
}

/**************
 * cpu_scandevs
 * ------------
 * Check the power switch once a while and turn the power off 
 * if power switch interrupt was detected.
 */
void
cpu_scandevs(void)
{
  volatile register ds17287_clk_t *clock = RTDS_CLOCK_ADDR;
  int xctrla;

  /*
   * Check the power switch.
   */
  if (_rtcinitted) {
    if ((clock->registerc & DS_REGC_IRQF) != 0x0) {
      /* clear interrupt condition */
      clock->registera |= DS_REGA_DV0;
      flushbus();
      xctrla = clock->ram[DS_BANK1_XCTRLA];
      clock->ram[DS_BANK1_XCTRLA] &= ~(DS_XCTRLA_KF|DS_XCTRLA_WF);
      clock->registera &= ~DS_REGA_DV0;
      flushbus();
      if ((xctrla & DS_XCTRLA_KF) != 0x0) {
#if defined(DEBUG)      
        printf ("System power off interrupt detected\n");
#endif      
        cpu_powerdown (0);
      }
#if defined(DEBUG)      
      else {
        /* Ignore all rest of interrupt conditions. !? */
        printf ("RTC irq received w/o kickstart condition.\n");
        printf ("Clock interrupt status %x\n", xctrla);
      }
#endif
    }
  }
  return ;
}

/*****************************************************************************
 *                          IP32 CPU frequency related stuff.                *
 *****************************************************************************/

/*
 * this is the table of known cpu freq that we support 
 * table is in the order of increasing freq.
 * (Found in the kern tree).
 */
static int freq[] = {
  1000000,
  2000000,
  4000000,
  8000000, 
  12000000, 
  16000000, 
  20000000, 
  25000000,
  30000000,
  33000000,
  36000000,
  40000000,
  50000000,
  66666000,	/* round to 1000 so we get integer divide */
  75000000,
  87370000,
  90000000,
  97500000,
  100000000,
  120000000
} ;

/*
 * following routines are in IP32kasm.s
 */
#ifdef  MASTER_FREQ
#undef  MASTER_FREQ
#endif
#define MASTER_FREQ 66000000    /* Crime upcounter frequency */

/********************
 * cpu_get_freq(void)
 *-------------------
 * find the frequency of the running CPU
 * was findcpufreq_raw() in kern tree.
 *-------------------
 * NOTE:::
 *   This routine works if we are running in cached 
 * mode, so please check config register before jump
 * on conclusion that this routine does not work.
 */
int
cpu_get_freq(void)
{
  int ticks;
  int i, closest,tmp,clock, freq_ret;
  int fcount= 0;
  int freqtbl_siz = sizeof(freq) / sizeof(int);
  unsigned int cpu_count, tick_count;
  
  cpu_count = cpuclkper100ticks(&tick_count);
  /* do division first to avoid overflow */
  ticks = cpu_count * (MASTER_FREQ / tick_count);	/* to HZ */
  closest = -1;
  
  for (i=0; i<(sizeof(freq)/sizeof(int)); i++) {
    if ( ticks > freq[i] )
      tmp = ticks - freq[i];
    else
      tmp = freq[i] - ticks;
    if ( closest == -1 ) {
      clock = i;
      closest = tmp;	
      continue;
    }
    if ( tmp < closest ) {
      closest = tmp;
      clock = i;
    }
  }
  freq_ret = freq[clock];
  
  /*
   * if the calculated freq is greater than the max that we know about
   * and if the margin is more than 4Mhz then lets use the calculated
   * freq instead of rounding it down to what's in the table
   */
  if (ticks > freq[freqtbl_siz-1] &&
      (ticks - freq[freqtbl_siz-1]) > 4000000)
    freq_ret = ticks;
  return(freq_ret);
}


/************************
 * cpu_get_freq_str(void)
 *-----------------------
 * return cpu freq in ascii string format
 *-------------------
 */
char *
cpu_get_freq_str(void)
{
  static char cpu_freq_str[4]; /* Seems big enough for now! */
  int freq = ((cpu_get_freq()*2)/1000000);
  sprintf(&cpu_freq_str[0],"%d", freq);
  return &cpu_freq_str[0];
}

  
/********************
 * GetRelativeTime(void)
 *-------------------
 * The IP32 version use T5/R5K internal free runnung counter. If
 * the time between calls is longer than a single overflow of the
 * counter, then this won't be accurate.  If you want to time
 * something that takes a real long time, then make sure this
 * gets called periodically, or use the GetTime() function.
 *-------------------
 */
uint32_t
GetRelativeTime(void)
{
  static int        lasttime;	/* last returned value */
  static unsigned   cummdelta;	/* cummulative fraction of a sec. */
  static unsigned   lasttick;	/* last value of count register */
  static int        tickhz;	/* ticker speed in Hz */
  unsigned          thistick;	/* current value of count register */

  if ( ! lasttick ) {
    tickhz = cpu_get_freq() ;
    lasttick = r4k_getticker();
    return 0;
  }
  
  thistick = r4k_getticker();
  
  if ( thistick < lasttick )
    cummdelta += (0xffffffff - lasttick) + thistick;
  else
    cummdelta += thistick - lasttick;
  
  lasttick = thistick;

  while ( cummdelta > tickhz ) {
    cummdelta -= tickhz;
    lasttime += 1;
  }
        
  return lasttime;
}
  
#if 0
int
findcpufreq(void)
{
  return(findcpufreq_raw()/1000000);
}
#endif

/*****************************************************************************
 *                          IP32 FLASH routines                              *
 *****************************************************************************/

/*
 * these defines ought to be in a header file, well, later ...
 */
#if !defined(Sim)
#define FWriteWait  2048
#else
#define FWriteWait  4
#endif 
#define FSecSize    256

/************
 * flashWrite
 * ----------
 * Write a long to FLASH
 * Note:::
 *   we assume that the caller know what he is doing,
 *   the flash can only be update in a sector base, and the
 *   the write have to be sequentally done for all 256 bytes -
 *   the whole sector.
 * ----------
 */
static void
flashWrite(long* ptr, long data, long* bytecount) 
{
  register int  byteidx ;
  register long bytewritten = *bytecount ;
  register unsigned char currChar ;
  volatile unsigned char *dst = (unsigned char*)ptr ;

  for (byteidx = 24 ; byteidx >= 0 ; byteidx -= 8, dst++) {
    *dst = currChar = (unsigned char) ((data >> byteidx) & 0xff) ;
    bytewritten += 1 ;
    while (bytewritten == FSecSize) {
      /* have to loop here until flash is happy */
      for (bytewritten = FWriteWait; bytewritten > 0; bytewritten -= 1) ;
      if (*dst != currChar)
        bytewritten = FSecSize ;
      else {
        bytewritten = 0 ;
        break ;
      }
    }
  }
  /* Update the bytecount and return. */
  *bytecount = bytewritten ; 
  return ;  
}

/************
 * flashFlush
 * ----------
 * Flush write to flash
 * ----------
 */
static void
flashFlush(void)
{}

/*************
 * flashEnable
 * -----------
 * Enable writes to the flash
 * -----------
 */
static void
flashEnable(int e)
{
  long long *fe = (long long*)PHYS_TO_K1(ISA_FLASH_NIC_REG);
  if (e)
    *fe |= ISA_FLASH_WE;
  else
    *fe &= ~ISA_FLASH_WE;
}

/*
 * Environment variable FLASH object (the only one we have for IP32).
 */
FlashROM envFlash = {
  (FlashSegment*)PHYS_TO_K1(FLASH_PHYS_BASE),
  (FlashSegment*)PHYS_TO_K1(FLASH_PHYS_BASE+FLASH_SIZE),
  FLASH_PAGE_SIZE,
  FLASH_SIZE,
  flashWrite,
  flashFlush,
  flashEnable,
};


/*****************************************************************************
 *                  IP32 specific configuration data                         *
 *****************************************************************************/

#define SYSBOARDID "SGI-IP32"
#define SYSBOARDIDLEN 9

static COMPONENT IP32tmpl = {
	SystemClass,            /* Class */
	ARC,                    /* Type */
	0,                      /* Flags */
	SGI_ARCS_VERS,          /* Version */
	SGI_ARCS_REV,           /* Revision */
	0,                      /* Key */
	0,                      /* Affinity */
	0,                      /* ConfigurationDataSize */
	SYSBOARDIDLEN,          /* IdentifierLength */
	SYSBOARDID              /* Identifier */
};

static COMPONENT cputmpl = {
	ProcessorClass,         /* Class */
	CPU,                    /* Type */
	0,                      /* Flags */
	SGI_ARCS_VERS,          /* Version */
	SGI_ARCS_REV,           /* Revision */
	0,                      /* Key */
	0x01,                   /* Affinity */
	0,                      /* ConfigurationDataSize */
	11,                     /* IdentifierLength */
	"MIPS-R4000"            /* Identifier */
};
static COMPONENT fputmpl = {
	ProcessorClass,         /* Class */
	FPU,                    /* Type */
	0,                      /* Flags */
	SGI_ARCS_VERS,          /* Version */
	SGI_ARCS_REV,           /* Revision */
	0,                      /* Key */
	0x01,                   /* Affinity */
	0,                      /* ConfigurationDataSize */
	14,                     /* IdentifierLength */
	"MIPS-R4000FPC"         /* Identifier */
};
static COMPONENT cachetmpl = {
	CacheClass,             /* Class */
	PrimaryICache,          /* Type */
	0,                      /* Flags */
	SGI_ARCS_VERS,          /* Version */
	SGI_ARCS_REV,           /* Revision */
	0,                      /* Key */
	0x01,                   /* Affinity */
	0,                      /* ConfigurationDataSize */
	0,                      /* IdentifierLength */
	NULL                    /* Identifier */
};
static COMPONENT memtmpl = {
	MemoryClass,            /* Class */
	Memory,                 /* Type */
	0,                      /* Flags */
	SGI_ARCS_VERS,          /* Version */
	SGI_ARCS_REV,           /* Revision */
	0,                      /* Key */
	0x01,                   /* Affinity */
	0,                      /* ConfigurationDataSize */
	0,                      /* Identifier Length */
	NULL                    /* Identifier */
};


/******
 * log2
 * ----
 * Some funny little routine.
 */
static int
log2(int x){
  int n,v;
  for(n=0,v=1;v<x;v<<=1,n++) ;
  return(n);
}

/******
 * crc 
 * ----
 * Some funny crc generator for ds2502
 */
void
CRCengine(uint8_t inbit, uint8_t *CRCptr)
{
  register uint8_t CRC, bit08, bit34, bit45 ;

  /*
   * Generate bit 0/8
   */
  CRC = *CRCptr ;
  bit08 = (inbit & 0x1) ^ (CRC & 0x1) ;

  /*
   * Generate bit 3/4
   */
  bit34 = ((CRC >> 3) & 0x1) ^ bit08 ;  
  
  /*
   * Generate bit 4/5
   */
  bit45 = ((CRC >> 4) & 0x1) ^ bit08 ;

  /*
   * Shift all bits into their corresponding position.
   */
  bit08 = bit08 << 7 ;
  bit34 = bit34 << 2 ;
  bit45 = bit45 << 3 ;
  CRC   = (CRC >> 1) & 0x73 ;
  CRC   = CRC | bit08 | bit34 | bit45 ;

  /*
   * Update global CRC
   */
  *CRCptr = CRC & 0xff ;
  return ;
}

/*****************
 * setCPUSpecifics
 * ---------------
 * CPU specific data structures.
 */
extern uint32_t Read_C0_PRId (void) ;
static void
setCPUSpecifics(COMPONENT* cpu)
{
  uint32_t PRId ;
  
  PRId = Read_C0_PRId () ;

  /*
   * Check the processor implementation #
   */
  switch (PRId >>C0_IMPSHIFT) {
    
  case C0_IMP_TRITON:
    cpu->Identifier = "MIPS-R5000" ;
    break ;
    
  case C0_IMP_R10000:
    cpu->Identifier = "MIPS-R10000" ;
    break ;
    
  case C0_IMP_R4700:
    cpu->Identifier = "MIPS-R4700" ;
    break ;
    
  case C0_IMP_R4600:
    cpu->Identifier = "MIPS-R4600" ;
    break ;
    
  case C0_IMP_R4400:
    cpu->Identifier = "MIPS-R4400" ;
    break ;
    
  default:
    cpu->Identifier = "MIPS-?????" ;
  }
    
  /*
   * set the CPU freguence, make sure the cpu->Key == 0, 
   * so that the cpufreq will return a correct value to hinv_cmd.c
   * FIXED THE cpu_get_freq in fw/stubs.c,
   * it always return 50 MHz - 50000000.
   */
  cpu->Key = 0 ;
  return ; 
}


/*****************
 * setFPUSpecifics
 *----------------
 * maybe we need to do something here, but I really
 * don't know much about it.
 */
static void
setFPUSpecifics(COMPONENT* fpu)
{
  switch (Read_C0_PRId() >>C0_IMPSHIFT) {
    
  case C0_IMP_TRITON:
    fpu->Identifier = "MIPS-R5000FPC" ;
    break ;
    
  case C0_IMP_R10000:
    fpu->Identifier = "MIPS-R10000FPC" ;
    break ;
    
  case C0_IMP_R4700:
    fpu->Identifier = "MIPS-R4700FPC" ;
    break ;
    
  case C0_IMP_R4600:
    fpu->Identifier = "MIPS-R4600FPC" ;
    break ;
    
  case C0_IMP_R4400:
    fpu->Identifier = "MIPS-R4400FPC" ;
    break ;
    
  default:
    fpu->Identifier = "MIPS-?????FPC" ;
  }
  return ; 
}

/********************
 * setMemorySpecifics
 *-------------------
 * Determind the size of memory here, check the
 * data structure to figure out what the hack is
 * this.
 */
static void
setMemorySpecifics(COMPONENT* mem) {
#if defined(May28NinetySix)
  /* We should not call cpu_mem_init here, _saio_init already did
     it, the result is losting yet another 4M of address space */
  cpu_mem_init () ;
#endif
  mem->Key = (ULONG) (cpu_get_memsize() / _PAGESZ) ;
}

/*******************
 * setCacheSpecifics
 *------------------
 * Seup primary, secondary cache size, block size
 * and line size, and ..., etc.
 */
static void
setCacheSpecifics(COMPONENT* cache)
{
  union key_u k;
  
  /* NOTE: Setting the cache specifics is processor dependent */

  switch(cache->Type){
  case PrimaryICache:
    k.cache.c_bsize = 1;
    k.cache.c_lsize = log2(_icache_linesize);
    k.cache.c_size = log2(_icache_size>>12);
    break;
  case PrimaryDCache:
    k.cache.c_bsize = 1;
    k.cache.c_lsize = log2(_dcache_linesize);
    k.cache.c_size = log2(_dcache_size>>12);
    break;
  case SecondaryCache:
    k.cache.c_bsize = 1;
    k.cache.c_lsize = log2(_scache_linesize);
    k.cache.c_size = log2(_sidcache_size>>12);
    break;
  default:
    assert(0);
  }
  cache->Key = k.FullKey;
  return ; 
}

/*****************
 * cpu_makecfgroot
 *----------------
 * Its a little different from the one I have seen in
 * arcs/lib/libsk/lib 
 */
cfgnode_t*
cpu_makecfgroot(void)
{
  COMPONENT* root;
  root = AddChild(0,&IP32tmpl,0);
  assert(root);
  return (cfgnode_t*)root;
}


/******************
 * IP32_cpu_install
 *-----------------
 * Add CPU, FPU,  cache, and  memory to configuration
 * Each has a generic template which we add first.  We then call setXSpecifics
 * to add platform specific data.
 */
void
IP32_cpu_install(COMPONENT* root)
{
  COMPONENT* cpu;
  COMPONENT* fpu;
  COMPONENT* cache;
  COMPONENT* mem;
  COMPONENT* pci;

  /* Add CPU to the configuration */
  cpu = AddChild(root,&cputmpl,0);
  assert(cpu);
  setCPUSpecifics(cpu);

  /* Add FPU to the configuration */
  fpu = AddChild(cpu,&fputmpl,0);
  assert(fpu);
  setFPUSpecifics(fpu);

  /* Add PI and PD CACHE to the configuration */
  cache = AddChild(cpu,&cachetmpl,0);
  assert(cache);
  setCacheSpecifics(cache);

  cache = AddChild(cpu,&cachetmpl,0);
  assert(cache);
  cache->Type = PrimaryDCache;
  setCacheSpecifics(cache);

  /* Optionally add secondary cache */
  /*
   * NOTE: The 4600sc is a class of PC CPU's with a SC grafted
   * on after the fact.  A more generic configuration rule would be nice.
   */
  if (_sidcache_size!=0 || _r4600sc_sidcache_size!=0){
    cache = AddChild(cpu,&cachetmpl,0);
    assert(cache);
    cache->Type = SecondaryCache;
    setCacheSpecifics(cache);
  }

  /* Add Memory to the configuration */
  mem = AddChild(cpu,&memtmpl,0);
  assert(mem);
  setMemorySpecifics(mem);
}

/*****************************************************************************
 *               IP32 memory configuration related functions.                *
 *****************************************************************************/

/*****************
 * cpu_get_membase
 * ---------------
 * Return base address of memory.
 */
static unsigned int
cpu_get_membase(void)
{
  return (unsigned int)PHYS_RAMBASE;
}

/*****************
 * cpu_get_memsize
 * ---------------
 * Dynamically figure out the memory size, post1
 * should have the Bank Control register setup
 * correctly.
 */
#define M32     32*1024*1024
#define M128    128*1024*1024

uint32_t
cpu_get_memsize (void)
{
  register int      i ;
  register uint64_t BankCNTL, tmp ; 
  register uint32_t CRIMEreg, size ; 

  CRIMEreg = (uint32_t) (CRM_MEM_BANK_CTRL(0)|K1BASE) ;
  size = M32 ;                      /* Default to 32M   */
  tmp = * (uint64_t *) CRIMEreg ;   /* remember bank 0  */
  if ((tmp & CRM_MEM_BANK_CTRL_SDRAM_SIZE) != 0x0) size = M128 ;
  for (i = 1, CRIMEreg += 8 ; i < 8 ; i++) {
    BankCNTL = * (uint64_t *) CRIMEreg ; 
    if (BankCNTL != tmp) {          /* There is SIMM in this bank. */
      if ((BankCNTL & CRM_MEM_BANK_CTRL_SDRAM_SIZE) != 0x0)
        size += M128 ;
      else
        size += M32 ;
    }
    CRIMEreg += 8 ;
  }
  return size ;
}

/**************
 * cpu_mem_init
 * ------------
 * Initialize the arcs memory data structure ????
 */

#define NPHYS0_PAGES 2
void
cpu_mem_init(void) {
  md_add(cpu_get_membase()+ptob(NPHYS0_PAGES),
	 btop(_min(cpu_get_memsize(),0x10000000)) - NPHYS0_PAGES,
	 FreeMemory);
  md_add(0,NPHYS0_PAGES,FreeMemory);
}

/***************
 * alloc_memdesc
 * -------------
 * Add what we know to be the prom's stack and bss.
 * Text and data are safe in prom in I/O space.
 */
/* #define DEBUG 1 */
void
alloc_memdesc(void)
{
#if defined(DEBUG)
  MEMORYDESCRIPTOR *m, *mem_getblock(void);
#endif

  /*
   * allocate the stack and bss addresses so we don't end up giving it
   * away.
   */
  md_alloc(KDM_TO_PHYS(PROM_RAMBASE), 
           arcs_btop(KDM_TO_PHYS(PROM_TILE_BASE)-KDM_TO_PHYS(PROM_RAMBASE)),
	   FirmwarePermanent);

  /* This also include the free tiles pool */
  md_alloc(KDM_TO_PHYS(PROM_TILE_BASE),
           arcs_btop(KDM_TO_PHYS(PROM_STACK)-KDM_TO_PHYS(PROM_TILE_BASE)),
	   FirmwareTemporary);

  /* This is for PROM frame buffer  */
  md_alloc(KDM_TO_PHYS(PROM_FB_TILE_START),
           arcs_btop(KDM_TO_PHYS(PROM_FB_TILE_LIMIT) -
                     KDM_TO_PHYS(PROM_FB_TILE_START)),
	   FirmwareTemporary);
  
#if defined(DEBUG)
  m = mem_getblock();
  printf ("Base Page addr = 0x%08lx, pages = %04ld, TopAddr = 0x%08x\n",
          arcs_ptob(m->BasePage), m->PageCount,
          arcs_ptob(m->BasePage + m->PageCount));
  mem_list();
#endif
/* #undef DEBUG   */
}

#if 0  /* all mte functions are now in lib/libsk/ml */
/*****************************************************************************
 *                            IP32 mte clean functions                       *
 *****************************************************************************/

/* These stubs imply that if there is no gfx.a, CRIME is idle except
 * for calls from ml/MOOSEHEAD/mte_copy.c  -wsm12/18/95.
 */
int crmSavePP () { return 0; }
void crimeRestore() { }
int crimeLockOut() { return 0; }

/* Useful Macros
typedef unsigned long int       uintptr_t;
 */

#define CRM_SRCBUF_LINEAR_A	(4<<5)
#define CRM_SRCBUF_LINEAR_B	(5<<5)
#define CRM_DSTBUF_LINEAR_A	(4<<2)
#define CRM_DSTBUF_LINEAR_B	(5<<2)
#define MTE_SRCECC		(1 << 1)
#define MTE_DSTECC		(1)
#define CRMSTAT_MTE_FLUSHED 	(1 << 25)

/* Make these real once the hardware comes back */
#define MTE_COST_PER_REGWRITE  0x100 /* XXX */
#define MTE_COST_PER_PRIMITIVE (MTE_COST_PER_REGWRITE * 3) /* XXX */

/* Assumes CRM_PAGE_SIZE == 0x1000 */
#define CRM_PAGE_SHIFT 12

#if USE_64BIT_C
# define WRITE_REG64(reg, value)  *(volatile _crmreg_t *)(reg) = (value);
#else
/* from IP32asm.s */
#if defined(WRITE_REG64)
#undef WRITE_REG64
# define WRITE_REG64(reg, value)  write_reg64((value), (reg))
#endif
#endif

#define CRM_REG(reg)	((uintptr_t)crime+(reg))
#define CRIME_SET32(reg, value) (*(volatile __uint32_t *)CRM_REG(reg) = (value))
#define CRIME_SET32_AND_GO(reg, value) CRIME_SET32((reg)|CRM_START_OFFSET, (value))

#define CRIME_SET64(reg, value) WRITE_REG64(CRM_REG(reg), (value))

#define MTE_REG(reg)	((reg)|CRM_MTE_BASE)
#define MTE_SET32(reg, value)        CRIME_SET32(MTE_REG(reg), (value))
#define MTE_SET32_AND_GO(reg, value) CRIME_SET32_AND_GO(MTE_REG(reg), (value))

#define countof(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

/* LOCAL declarations */

static void *crime = (void *) PHYS_TO_K1(CRM_BASEADDR + 0x1000000);

static int mte_set_tlb(paddr_t addr, int bcount, uint_t tlb, int *nbytes);
static void mte_fifo_wait(int nentries);
static void mte_spin(void);
extern int get_crm_rev(void);


/* 
 * EXPORTS
 */

void
mte_zero(paddr_t addr, size_t bcount)
{
    const __uint32_t ZERO_OP = ( MTE_CLEAR | CRM_DSTBUF_LINEAR_A 
				 | MTE_DSTECC);

    int tlba;

    if (get_crm_rev() < 1)
	return;

    /*
     * MTE can only handle 128K max at one time.  Break
     * the request up into chunks which are no larger than
     * 128K
     */
    while (bcount) {
	int count;
	int nbytes;

	count = (bcount > 0x20000) ? 0x20000 : bcount;
	tlba |= mte_set_tlb(addr, count, CRM_TLB_LINEAR_A_OFFSET, &nbytes);
	count = MIN(count, nbytes);

	/* Now do the zero */
	mte_fifo_wait(5);
	/*
	 * we use addr + count - 1 because MTE copies and zero
	 * operations are inclusive.  We actually want to clea
	 * count bytes starting at addr.
	 */
	MTE_SET32(CRM_MTE_DST0_REG, (uintptr_t)addr);
	MTE_SET32(CRM_MTE_DST1_REG, (uintptr_t)((addr+count) - 1));
	MTE_SET32(CRM_MTE_FGVALUE_REG, 0x0);
	MTE_SET32(CRM_MTE_BYTEMASK_REG, 0xffffffff);
	MTE_SET32_AND_GO(CRM_MTE_MODE_REG, ZERO_OP);

	bcount -= count;
	addr += count;

	/* wait for the xfer to finish */
	mte_spin();
    }
}

/* Estimated number of usecs before an MTE operation will be acted on.
 *
 * The returned value may have an arbitrary amount of error.  (I.e., we
 * are free to sacrifice accuracy for ease of computation.)
 */
static int
mte_delay()
{
    uint_t status;

    if (crimeLockOut(crime))
	return(HW_COPY_NOT_AVAILABLE);

    status = *(volatile uint_t *)CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG);

    return(status & CRMSTAT_MTE_FLUSHED ? 0 :
	   (MTE_COST_PER_PRIMITIVE + 
	    CRMSTAT_IB_LEVEL(status) * MTE_COST_PER_REGWRITE));
}


/* Spin until MTE is idle */
static void
mte_spin()
{
    while (!*(volatile uint_t *)
	   CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG) & CRMSTAT_MTE_FLUSHED)
	DELAY(1);
}


/*
 * LOCAL definitions
 */
#define MAX_SPAN (0x20000 - 1)

static int
mte_set_tlb(paddr_t addr, int bcount, uint_t tlb_offset, int *nbytes)
{
    u_long page;
    u_long page_last;
    paddr_t addr_end = ((u_long) addr + bcount - 1);
    int indices = 0;
    int index;

    page = (u_long)addr >> CRM_PAGE_SHIFT;
    page_last  = (u_long)addr_end >> CRM_PAGE_SHIFT;
    *nbytes = 0;

    /*
     * check to see if we start on an odd page.  
     * Since TLB entries are paired, the first page
     * mapped must be an even page.
     */
    if (page & 0x1)	{ 
	--page;
	addr = (page << CRM_PAGE_SHIFT);
	*nbytes -= CRM_PAGE_SIZE;
    }

    /*
     * now that we have the starting page calculated, check for
     * overflow.  Adjust ending page if we would overflow the
     * the mte page table.
     */
    if ((addr_end - (page << CRM_PAGE_SHIFT)) > MAX_SPAN) {
	page_last = ((page<<CRM_PAGE_SHIFT) + MAX_SPAN) >> CRM_PAGE_SHIFT;
    }
    

    /* Make sure there's enough room in the command fifo for our writes */
    mte_fifo_wait((page_last - page)/2);

    for (; page <= page_last; page += 2) {
	/* just map vaddr to paddr */
	index = (page & 0x1f) >> 1;

	CRIME_SET64(CRM_TLB_BASE+tlb_offset+(index*sizeof(_crmreg_t)),
		    ((_crmreg_t)(1<<31 | page) << 32) | 
		    (_crmreg_t)(1<<31 | page+1));
	indices |= 1<<index;

	*nbytes += (CRM_PAGE_SIZE * 2);
    }

    return(indices);
}


/* We don't want to take a Crime-FIFO-hiwater-mark interrupt
 * for a variety of reasons (it happens before the fifo actually fills;
 * it's expensive; and we may have the interrupt masked, anyway, etc.)
 * so we just make sure there's enough room in the fifo before we write
 * something.  Poll, waiting for it to drain, if there isn't.
 * 
 * This assumes that no one can come in at interrupt and write it the fifo, 
 * which is true because hw_copy.c only lets one caller in here at a time.
 */

static void
mte_fifo_wait(int nentries)
{
    while (CRIME_FIFO_DEPTH - 
	   CRMSTAT_IB_LEVEL(*(volatile uint_t *)
			    CRM_REG(CRM_STATUS_BASE|CRM_STATUS_REG)) <
	   nentries)
	DELAY(1);
}

#endif  /* mte functions    */

/*****************************************************************************
 *                            IP32 miscellaneous                             *
 *****************************************************************************/

/**************
 * jumper_off()
 *-------------
 */
int
jumper_off(void) {
  uint64_t Jumper_value = *(uint64_t*)(PHYS_TO_K1(ISA_FLASH_NIC_REG))
    & ISA_PWD_CLEAR;
  return (Jumper_value == 0) ? 1 : 0; 
}

/***************
 * showException
 *--------------
 * NOTE: Temporary
 */
void
showException(ulong sr, ulong cause, ulong badvaddr, ulong epc){
  char buf[80];
  sprintf(buf,"%08x %08x %08x %08x\n", sr, cause, badvaddr, epc);
  _errputs(buf);
}

/**********
 * __assert
 *---------
 * NOTE: Probably shouldn't be here.
 */
void
__assert(const char* ex, const char* file, int line){
  char buf[128];
  sprintf(buf,
	  "assertion failure (%s) in file %s at line %d\n",
	  ex, file, line);

  panic(buf);
}

/*****************************************************************************
 *                       Find and get the version on flash                   *
 *****************************************************************************/

/* Flash ROM definition */

#define IMPOSSIBLE    0
char*
getversion(void)
{
  FlashSegment* Seg = (FlashSegment*)0;
  char *Ptr;
  int i;

  while (Seg = nextSegment(&envFlash,Seg)) {
#if 0    
    printf ("Current segment = %s\n", Seg->name);
#endif    
    
    if(strcmp(Seg->name, "version") == 0x0) {
#if 0      
      printf ("the version segment address = 0x%0x\n", Seg);
      printf ("the version body    address = 0x%0x\n", body(Seg));
#endif
      
      for (Ptr=(char*)body(Seg),i=0;i < 1024;i++,Ptr+=1)
        if (strcmp(Ptr,"VSEGOK") == 0x0) {
#if 0          
          printf ("Found VSEGOK at location 0x%0x\n", Ptr+8);
#endif          
          return Ptr+8;
        }
      
      /* We have an version segment but we can not find the first
         non blank string */
      assert(IMPOSSIBLE);
    }
  }
  assert(IMPOSSIBLE);
}
