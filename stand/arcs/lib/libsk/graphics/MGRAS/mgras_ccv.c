/*
 * $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/graphics/MGRAS/RCS/mgras_ccv.c,v 1.8 1996/07/08 21:48:58 jeffs Exp $
 */

#include "sys/types.h"	/* types.h must be before systm.h */
#include "sys/param.h"
#include "sys/cpu.h"
#include "libsk.h"
#include "libsc.h"
#include "sys/invent.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/sema.h"

#include "sys/mgrashw.h"
#include "sys/mgras.h"

#include "mgras_internals.h"
		
/******************************************************************************
 * Constants
 ******************************************************************************/

#define AUTOCC              0x01
#define MANUALCC            0x02

/* mode register bits */
#define RERWMASK            BIT(0)
#define RESHIFT             0
#define DERWMASK            BIT(1)
#define DESHIFT             1
#define ASRWMASK            BIT(2)
#define ASSHIFT             2
#define SKRWMASK            BIT(3)
#define SKSHIFT             3
#define SVRWMASK            BIT(4)
#define SVSHIFT             4
#define PLRWMASK            BIT(5)
#define PLSHIFT             5
#define X2RWMASK            BIT(6)
#define X2SHIFT             6
#define CERWMASK            BIT(7)
#define CESHIFT             7

/* CC value bits/shifts */
#define C0RMASK             BIT(30)
#define C0WMASK             BIT(0)
#define C0SHIFT             (30 - 0)
#define C1RMASK             BIT(22)
#define C1WMASK             BIT(1)
#define C1SHIFT             (22 - 1)
#define C2RMASK             BIT(14)
#define C2WMASK             BIT(2)
#define C2SHIFT             (14 - 2)
#define C3RMASK             BIT(31)
#define C3WMASK             BIT(3)
#define C3SHIFT             (31 - 3)
#define C4RMASK             BIT(23)
#define C4WMASK             BIT(4)
#define C4SHIFT             (23 - 4)
#define C5RMASK             BIT(15)
#define C5WMASK             BIT(5)
#define C5SHIFT             (15 - 5)
#define CRMASK              (C0RMASK|C1RMASK|C2RMASK|C3RMASK|C4RMASK|C5RMASK)
#define CWMASK              (C0WMASK|C1WMASK|C2WMASK|C3WMASK|C4WMASK|C5WMASK)

#define MAXTRYS             10
#define CHECKMASK           0x00ff0000

#define abs(x) ( ((x)<0) ? (-(x)) : (x) )

#define LAST_DITCH_CCV      0x19 /* total hack; empirically guessed at */

/******************************************************************************
 * Globals
 ******************************************************************************/

static mgras_hw          *base;
static __uint32_t  mode_addr;	/* device address for current RDRAM mode reg */
static __uint32_t  data_addr;	/* device address for current RDRAM memory */

/******************************************************************************
 * read/write CCR values
 ******************************************************************************/

static void
writeCC(int cc_val_in, int autoflag)
{
  __uint32_t cc_val, dw;

  dw = (DERWMASK | X2RWMASK);

  cc_val = cc_val_in ^ CWMASK;

  if (autoflag == AUTOCC)
    dw |= CERWMASK;

  dw |= (cc_val & C0WMASK) << C0SHIFT;
  dw |= (cc_val & C1WMASK) << C1SHIFT;
  dw |= (cc_val & C2WMASK) << C2SHIFT;
  dw |= (cc_val & C3WMASK) << C3SHIFT;
  dw |= (cc_val & C4WMASK) << C4SHIFT;
  dw |= (cc_val & C5WMASK) << C5SHIFT;

/*  printf("\twriting CC val 0x%02x : mode reg <- 0x%08x\n", cc_val_in, dw); */

  mgras_re4SetDevice(base, mode_addr, dw);
}

static void
readCC(int *cc_ptr)
{
  __uint32_t dw;
  int rval;

  mgras_re4GetDevice(base, mode_addr, dw);

  rval = 0;
  rval |= (dw & C0RMASK) >> C0SHIFT;
  rval |= (dw & C1RMASK) >> C1SHIFT;
  rval |= (dw & C2RMASK) >> C2SHIFT;
  rval |= (dw & C3RMASK) >> C3SHIFT;
  rval |= (dw & C4RMASK) >> C4SHIFT;
  rval |= (dw & C5RMASK) >> C5SHIFT;

/*  printf("\treading CC mode reg 0x%08x : CC val = 0x%02x\n", dw, rval); */

  *cc_ptr = rval;
}

/******************************************************************************
 * read/write data & count ones
 ******************************************************************************/

static void
writeData(__uint32_t dw)
{
/*  printf("\twriting data 0x%08x\n", dw); */
  mgras_re4SetDevice(base, data_addr, dw);
}

static void
readData(__uint32_t *dw)
{
  __uint32_t rval;
  
  mgras_re4GetDevice(base, data_addr, rval);
/*  printf("\tread data 0x%08x\n", rval); */
  *dw = rval;
}

static int
countOnes(__uint32_t dw)
{
  int count;
  
  for (count = 0; dw; dw >>= 1)
    if (dw & 0x1)
      count++;

  return count;
}

/******************************************************************************
 * search for CC vals
 ******************************************************************************/

#define MULT     43
#define MFAC     20
#define NTRY     10
#define BITS     8
#define TRYBITS  (NTRY * BITS)

int PhaseOne(void)
{
  int pass, ppass, sumv, try;
  int mccv, iccv;
  __uint32_t d;

  /* Phase 1: search for manual cc value (mccv) that gives iol = vref */
  ppass = 0;
  sumv = 0;
  for (mccv = 0; ppass < TRYBITS; mccv++) {
    if (mccv > 63) {
      return -1;
    }
    writeCC(mccv, MANUALCC);
    pass = 0;
    for (try = 0; try < NTRY; try ++) {
      writeData(0xffffffff);
      readData(&d);
      d = ((d >> 0) & 0xff);
      pass += countOnes(d);
    }
    sumv += mccv * (pass - ppass);
    ppass = pass;
  }

  /* ideal floating point iccv = (mult/mfac) * (sumv / TRYBITS - 0.5) */
  iccv = MULT * (sumv - (TRYBITS/2)); /* iccv = mfac*TRYBITS*(target iccv) */

  return iccv;
}

int
PhaseTwo(int iccv)
{
  int rccv, minccv, dccv, delccv, calccv, accv, bestccv; 

  /* Phase 2: auto search for iccv */
  rccv = 0;
  minccv = 0;
  delccv = 999999;
  for (accv = 0; rccv < iccv; accv++) {
    if (accv > 63) {
      return -2;
    }
    writeCC(accv, AUTOCC);
    readCC(&calccv);
    rccv = (MFAC * TRYBITS) * calccv;
    dccv = abs(rccv - iccv);
    if (dccv < delccv) {
      delccv = dccv;
      minccv = accv;
    }
  }
  accv -= 1;

  /* minccv = 1st auto CCValue which produced rccv closest to iccv */
  /* accv = 1st auto CCValue which produced rccv>iccv */
  bestccv = (minccv + accv) / 2;
  
  return bestccv;
}

int
median(int *a, int n)
{
    int i, j, min, tmp;

    for (i = 0; i <= (n / 2); i++) {
	min = i;
	for (j = i+1; j < n; j++)
	    if (a[j] < a[min])
		min = j;
	tmp = a[min];
	a[min] = a[i];
	a[i] = tmp;
    }
    
    if (n % 2)
	return a[n/2];
    else
	return (a[n/2] + a[(n/2)-1]) / 2;
}

int
CCInit(int num_passes)
{
    int i, iccv;
    int *ccvs;
    int bestccv;

    ccvs = (int *) kern_calloc(num_passes, sizeof(int));
    
    for (i = 0; i < num_passes; i++) {
	iccv = PhaseOne();
	ccvs[i] = PhaseTwo(iccv);
    }
    bestccv = median(ccvs, num_passes);
    kern_free(ccvs);
    writeCC(bestccv, AUTOCC);
    return bestccv;
}

/******************************************************************************
 * Outer Loop
 *****************************************************************************/

#define NUMPASSES 51

void MgrasInitCCR(mgras_hw *base_in, mgras_info *info)
{
  int re, pp, ram;
  __uint32_t savedata;
  int prev_cc, new_cc;

  /* record the base for use in utility fcns */
  base = base_in;

  /* put the PP1s in 121212 mode */
  mgras_re4Set(base, pp1fillmode.val, 0x4300);

  for (re = 0; re < info->NumREs; re++) {
    /* select the correct RE */
    mgras_re4Set(base, config.val, re);
    
    for (pp = 0; pp < 2; pp++) {
      for (ram = 0; ram < 3; ram++) {
	/* Clean out the FIFOs */
	mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);
	mgras_CFIFOWAIT(base,HQ3_CFIFO_MAX);

	/* set the correct device addresses */
	mode_addr = (pp == 0) ? MGRAS_RE4_DEVICE_PP1_A : MGRAS_RE4_DEVICE_PP1_B;
	mode_addr |= MGRAS_PP1_RDRAM_N_REG(ram);
	mode_addr |= MGRAS_RDRAM_MODE;
	data_addr = (pp == 0) ? MGRAS_RE4_DEVICE_PP1_A : MGRAS_RE4_DEVICE_PP1_B;
	data_addr |= MGRAS_PP1_RDRAM_N_MEM(ram);
	data_addr += 2;		/* want middle bytes of octbyte */

	/* keep a copy of the bits we'll trash */
	readData(&savedata);

	/* setup a default ccv */
	prev_cc = LAST_DITCH_CCV;
	
	/* find the best CC value */
	new_cc = CCInit(NUMPASSES);

	/* recover if necessary */
	if (new_cc < 0)
	  writeCC(prev_cc, AUTOCC);

	/* restore the trashed data */
	writeData(savedata);
      }
    }
  }

  /* restore RE bcast mode (pp1fillmode corrected in InitRSS) */
  mgras_re4Set(base, config.val, CFG_BCAST);
}
