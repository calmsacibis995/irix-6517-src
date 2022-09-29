/*
 * driver.c - sample code for Mountaingate deck control driver
 *
 * this sample code assumes 
 *   - running on O2 with "IRIX 6.3 for O2 Including R10000"
 *     plus a set of patches agreed upon by SGI and Mountaingate
 *   - kernel exports certain custom, demo-only tserialio hooks
 *     as part of SGI <--> Mountaingate agreement
 * 
 * this driver will not work on any other IRIX release,
 * even if it did not use the tserialio hooks.  future IRIX
 * releases will place stronger locking requirements on drivers which
 * this driver happily ignores.
 */

/* exported device driver header files */
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/mload.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* shipped but unsupported headers, for kvpalloc/kvpfree */
#include <sys/immu.h>
#include <sys/pfdat.h>

#include "common.h"

#ifdef TS
/*
 * hooks exported from tserialio as part of
 * SGI <--> Mountaingate agreement
 */
typedef void (*callback_t)(int minor, stamp_t now);
void tsio_register_callback(int minor, callback_t funcptr);
int tsio_uart_read(unsigned char *buf, int atmost);
int tsio_uart_write(unsigned char *buf, int atmost);
#endif

/* per-serial-port state ----------------------------------------------- */

typedef struct deckport
{
  int isopen;                   /* see deckopen()/deckclose() */
  int tick_should_touch;        /* see deckmap()/deckunmap() */
  mappedstructure *mapmem;      /* pointer to user-mapped memory */
} deckport;

/*
 * this sample code assumes O2.
 *
 * deck driver uses same O2 minor device numbers as tserialio:
 *   1 for port 1, and 2 for port 2.
 */
deckport deckports[2];

deckport *getdeckport(int minor)
{
  if (minor == 1)
    return &deckports[0];
  else if (minor == 2)
    return &deckports[1];
  else
    return NULL;
}

/* 1 millisecond interrupt function ------------------------------------ */

int mgcause;
int checkit = 1;

stamp_t last = -1;
stamp_t last2 = -1;

stamp_t zenbuf[1024];
int zenptr = 0;

stamp_t shortbuf[1024];
int shortptr = 0;

int useless[50000];

void
reset_debugging_stuff(void)
{
  last = -1;
  last2 = -1;
}

void
deck_1ms_tick(int minor, stamp_t now)
{
  deckport *p = getdeckport(minor);
  if (!p)
    {
      mgcause = 3;
      debug("ring");
    }
  if (!p->tick_should_touch)
    return;

  zenbuf[zenptr] = now;
  zenptr++;
  if (zenptr==1024) zenptr=0;

  /* user should see foo incr about 1000 per second */
  p->mapmem->foo++;

#if 1
  p->mapmem->foo++;
  
  bcopy(p->mapmem->useless, p->mapmem->useless, 4096*15-1);
#else
  /* poke mapmem like crazy */
  {
    while (1)
      {
        stamp_t ust;
        
        p->mapmem->foo++;

        bcopy(p->mapmem->useless, p->mapmem->useless, 
              4096*15-1);

        update_ust();
        get_ust_nano((unsigned long long *)(&ust));
        if (ust - now > 500000LL)
          break;
      }
  }
#endif

  if (checkit)
    {
      if (last > 0)
        {
          if (now - last > 2000000LL)  /* too long since last intr */
            {
              p->mapmem->bar++;
              mgcause = 1;
              debug("ring");
            }
          else if (now - last < 200000LL) /* too short since last intr */
            {
              shortbuf[shortptr] = last;
              shortptr++;
              if (shortptr==1024) shortptr=0;
              shortbuf[shortptr] = now;
              shortptr++;
              if (shortptr==1024) shortptr=0;
              if (shortptr == 10)
                {
                  mgcause = 2;
#if 0
                  debug("ring");
#endif
                }
            }
        }
      last = now;
    }

#ifdef TS
  /* this code assumes tsio_uart_write will not fail.
   * since the source of data is the serial port, we know we 
   * will not attempt to write faster than the baud rate.  
   * so the maximum amount of buffering the output could need 
   * is about 2ms of chars, which we cover easily:
   *
   *   deck == 38400 sym/sec / (1start+8data+1par+1stop)=11 sym/byt
   *        == 3491... byt/sec
   *   so 2ms == about 8 bytes.  O2 chip has 16 byte fifo plus
   *   many (>1KB) byte DMA buffer.
   *
   * depending on how you structure your output code, you may be
   * able to make a similar assumption.
   */
  {
    unsigned char c;
    while (1 == tsio_uart_read(&c, 1))
      {
        int rc;
        rc = tsio_uart_write(&c, 1);
        ASSERT(rc == 1);
      }
  }
#endif
}

#ifndef TS
void
mg_tick(void)
{
  stamp_t ust;
  update_ust();
  get_ust_nano((unsigned long long *)(&ust));
  
  deck_1ms_tick(2, ust);
}

extern int atomicSetUint(uint *, uint);
extern int atomicClearUint(uint *, uint);
extern int fastclock; /* from ml/timer.c */
extern void (*midi_timercallback_ptr)(void); /* from ml/timer.c */

void
start_mg_ticks(void)
{
  midi_timercallback_ptr = (void (*)(void))mg_tick;
  atomicSetUint(&fastick_callback_required_flags,
                FASTICK_CALLBACK_REQUIRED_MIDI_MASK);
  if (!fastclock)
    enable_fastclock();
}

void
stop_mg_ticks(void)
{
  atomicClearUint(&fastick_callback_required_flags,
                  FASTICK_CALLBACK_REQUIRED_MIDI_MASK);
  midi_timercallback_ptr = NULL;
}
#endif

/* driver init ----------------------------------------------------------- */

int deckdevflag = D_MP;        /* required for all drivers */
char *deckmversion = M_VERSION;/* Required for dynamic loading */

int
deckinit()
{
  cmn_err(CE_NOTE, "deckinit");
  return 0;
}

/* driver unload --------------------------------------------------------- */

int
deckunload()
{
  cmn_err(CE_NOTE, "deckunload");
  return 0;
}

/* driver open and close ------------------------------------------------- */

int
deckopen(dev_t *dev, int oflag, int otyp, cred_t *crp) 
{
  deckport *p;
  int minor;

  minor = getminor(*dev);
  cmn_err(CE_NOTE, "deckopen minor=%d", minor);
  if (NULL == (p=getdeckport(minor)))
    return ENODEV;
  
  if (p->isopen)
    {
      /* only one open per port */
      return EBUSY;
    }

  p->isopen = 1;

  return 0;
}

int
deckclose(dev_t dev, int flag, int otyp, cred_t *crp)
{
  deckport *p;
  int minor;

  minor = getminor(dev);
  cmn_err(CE_NOTE, "deckclose minor=%d", minor);
  p = getdeckport(minor);
  ASSERT(p);

  /*
   * if the user exits or does close() without doing
   * an unmap(), the OS automatically does the unmap
   * (calling deckunmap()) first.
   */
  
  p->isopen = 0;

  return 0;
}

/* driver ioctl ---------------------------------------------------------- */

int
deckioctl(dev_t dev, int cmd, int arg, int mode, 
          struct cred *cred, int *rval)
{
  deckport *p;
  int minor;

  minor = getminor(dev);
  cmn_err(CE_NOTE, "deckioctl minor=%d cmd=%d", minor, cmd);
  p = getdeckport(minor);
  ASSERT(p);

  return 0;
}

/* driver map/unmap ----------------------------------------------------- */

int
deckmap(dev_t dev,       /* device number */
        vhandl_t *vt,    /* handle to caller's virtual address space */
        off_t off,       /* offset into device */
        int len,         /* number of bytes to map */
        int prot)        /* protections */
{
  int err;
  deckport *p;
  int minor;

  minor = getminor(dev);
  cmn_err(CE_NOTE, "deckmap minor=%d", minor);
  p = getdeckport(minor);
  ASSERT(p);

  /* sanity check that user code is in sync with us */
  if (len != sizeof(mappedstructure))
    return EINVAL;

  /* can't map twice */
  if (p->mapmem)
    return EINVAL;

  /* allocate pages of k2seg kernel virtual memory for mappedstructure.
   * physical pages behind this virtual address are always resident.
   * use the proper page color to avoid virtual coherency problems.
   */
#ifdef _VCE_AVOIDANCE
  if (vce_avoidance)
    p->mapmem = (mappedstructure *)kvpalloc(btoc(len),VM_VACOLOR,
                                        colorof(v_getaddr(vt)));
  else
#endif /* _VCE_AVOIDANCE */
    p->mapmem = (mappedstructure *)kvpalloc(btoc(len),0,0);

cmn_err(CE_NOTE, "^mapmem=%lX", (__uint32_t)p->mapmem);
  /* map the memory into the user address space */
  if (err=v_mapphys(vt, p->mapmem, len))
    {
      /* error - free the memory */
      kvpfree(p->mapmem,btoc(len));
      p->mapmem = NULL;
      return(err);
    }

  /* NOTE: deckmap() should do all other operations which may fail
   * _before_ it does v_mapphys.  there is a bug in IRIX 6.3
   * such that if v_mapphys() succeeds, but the xxxxmap() routine
   * returns err != 0, kernel bookkeeping will do the wrong
   * thing and you may panic later in unmap.  there is a posted
   * SGI internal bug about this, 429745.  you will not hit the
   * bug if deckmap() returns err != 0 without calling 
   * v_mapphys().  you will not hit the bug if v_mapphys() 
   * fails and then deckmap() returns err != 0.
   */

  /* put the fields of p->mapmem into some good initial state
   * before the 1ms callback starts messing with them
   */
  p->mapmem->foo = 1;
  p->mapmem->bar = 2;
  p->mapmem->baz = 3;

  reset_debugging_stuff();

  /* tell the 1ms callback to go ahead and mess with this port */
  p->tick_should_touch = TRUE;

#ifdef TS
  /* activate the 1ms callbacks on this port */
  tsio_register_callback(minor, deck_1ms_tick);
#else
  start_mg_ticks();
#endif

  return 0;
}

int
deckunmap(dev_t dev,       /* device number */
          vhandl_t *vt)    /* handle to caller's virtual address space */
{
  deckport *p;
  int minor;

  minor = getminor(dev);
/*  cmn_err(CE_NOTE, "deckunmap minor=%d", minor); */
  p = getdeckport(minor);
  ASSERT(p);

  /* should have mapped already */
  if (!p->mapmem)
    return EINVAL;

  /*
   * tell the 1ms tick to stop messing with this port
   * NOTE: this code assumes we're on an SP system where
   *       interrupts always block out toplevel routines
   */
  p->tick_should_touch = FALSE;

  /* deactivate the 1ms callbacks on this port */
#ifdef TS
  tsio_register_callback(minor, NULL);
#else
  stop_mg_ticks();
#endif
  
  /* now the 1ms callback will not mess with this port */

  /* the kernel has already removed the user address space mapping */

  /* free the formerly mapped memory */
  kvpfree(p->mapmem,btoc(sizeof(struct mappedstructure)));
  p->mapmem = NULL;

  return 0; 
}


