#include <assert.h>
#include "sim.h"
#include "simSvr.h"
#include <sys/ds17287.h>
#include <string.h>
#include <sys/cpu.h>
#include <unistd.h>


typedef struct{
  unsigned char bank0[128];	/* Base + bank 0 */
  unsigned char bank1[128];	/* Bank1 (first 64 same as bank 0) */
  unsigned char bxtended[128];
  SimAddrMap_t  map;
} Rtc_t;

static Rtc_t* rtc;
static updating;		/* 1=> 1 second periodic update */

#define UNIMPLEMENTED 0

/* RTC bank 0 registers */

#define RTC_SEC         (0x00)
#define RTC_SEC_ALRM    (0x01)
#define RTC_MIN         (0x02)
#define RTC_MIN_ALRM    (0x03)
#define RTC_HR          (0x04)
#define RTC_HR_ALRM     (0x05)
#define RTC_DAY_OF_WEEK (0x06)
#define RTC_DAY_OF_MONTH (0x07)
#define RTC_MONTH       (0x08)
#define RTC_YEAR        (0x09)
#define RTC_REGA        (0x0A)
#define RTC_REGB        (0x0B)
#define RTC_REGC        (0x0C)
#define RTC_REGD        (0x0D)
#define RTC_NVRAMBASE   (0x0E)
#define RTC_BANKBASE    (0x40)

/* RTC bank 1 registers */

#define RTC_MODEL       (0x40)
#define RTC_SERIAL0     (0x41)
#define RTC_SERIAL1     (0x42)
#define RTC_SERIAL2     (0x43)
#define RTC_SERIAL3     (0x44)
#define RTC_SERIAL4     (0x45)
#define RTC_SERIAL5     (0x46)

#define RTC_CRC         (0x47)
#define RTC_CENTURY     (0x48)
#define RTC_DATE_ALRM   (0x49)

/*********
 * running	Returns true if RTC is running
 */
static int
running(void){
  unsigned char ra = rtc->map.base[RTC_REGA];
  return (ra & (DS_REGA_DV1|DS_REGA_DV2))==DS_REGA_DV1;
}

/**************
 * rtcIncrement
 */
static int
rtcIncrement(unsigned char* regs, int reg, int max){
  unsigned char t = regs[reg] + 1;
  regs[reg] = t % max;
  return t==max;
}

/*************
 * alrmHandler	Minimal time update service
 *------------
 *
 * We only update the 24 hour clock.  We don't generate interrupts.  We
 * don't honor the 12/24 clock setting, we assume 24.
 */
static void
alrmHandler(int sig, int code, SigContext_t* sc){
  unsigned char* regs = rtc->map.base;

  /*
   * Set the UIP bit
   *
   * Note: We can't really simulate the UIP logic as is.  UIP is supposed
   *       to be set 244us BEFORE the update begins.  That's too fine an
   *       increment to successfully simulate considering we don't have
   *       a particularly accurate notion of time in the simulator.   We
   *       just turn on UIP when we start, and turn it off when we end.
   */
  regs[RTC_REGA] |= DS_REGA_UIP;

  /*
   * Increment the counter registers
   * Note that we only increment the 24 hour clock, not the dates.
   * Also note that we assume we are in 24 hour mode.
   */
  if (rtcIncrement(regs,RTC_SEC,60))
    if (rtcIncrement(regs,RTC_MIN,60))
      rtcIncrement(regs,RTC_HR,24);

  /* Clear the UIP bit */
  regs[RTC_REGA] &= ~DS_REGA_UIP;

  /* Request another signal. */
  alarm(1);
}


/************
 * regAupdate	RTC regA has been updated
 */
static void
regAupdate(void){
  unsigned char* regs = rtc->map.base;
  int t;

  /*
   * If the clock is "runnable" start the update service if not already
   * active
   */
  if (running() && !updating){
    sigset(SIGALRM,alrmHandler);
    alarm(1);
    updating=1;
  }
  
  /* If the clock is not runnable and we are updating, stop the updates */
  if (!running() && updating){
    alarm(0);
    updating=0;
  }

  /* Check for bank swap */
  t = (regs[RTC_REGA] & DS_REGA_DV0) ? 2 : 0;
  t|= (rtc->map.base== rtc->bank1) ? 1 : 0;
  switch(t){
  case 1:
    /* Have switched from bank1 to bank0 */
    rtc->map.base = rtc->bank0;
    memcpy(rtc->bank0,rtc->bank1,64);
    break;
  case 2:
    /* Have switched from bank0 to bank1 */
    rtc->map.base = rtc->bank1;
    memcpy(rtc->bank1,rtc->bank0,64);
    break;
  }
}


/***************
 * simRtcService	Service forwarded RTC requests
 */
void
simRtcService(void){
  SimAddrMatch_t* am = &simControl->addrTbl[simControl->matchIndex];
  SimAddrMap_t*   map = am->mapping;
  Inst_t          inst = simControl->inst;
  SigContext_t*   sc   = simControl->clSigCtx;
  int             regNum;

  /* Compute the rtc register number */
  regNum = (simControl->paddr & map->mask)>>map->shift;

  /* Note that the only register we are servicing right now is REGA */
  switch(regNum){
  case RTC_REGA:
    regAupdate();
  }
}


/************
 * simRtcInit	Connect RTC simulation to server
 */
void
simRtcInit(void){
  unsigned long regA = (RTC_BASE & ~7)+offsetof(ds17287_clk_t,registera);
  int            targetCode = simRegisterService(simRtcService);

  /* Allocate storage area for RTC registers */
  rtc = (Rtc_t*)usmalloc(sizeof(Rtc_t),arena);
  assert(rtc);
  memset(rtc,0,sizeof(Rtc_t));


  /* Fill in the serial number */
  rtc->bank1[RTC_SERIAL0] = SERIAL0;
  rtc->bank1[RTC_SERIAL1] = SERIAL1;
  rtc->bank1[RTC_SERIAL2] = SERIAL2;
  rtc->bank1[RTC_SERIAL3] = SERIAL3;
  rtc->bank1[RTC_SERIAL4] = SERIAL4;
  rtc->bank1[RTC_SERIAL5] = SERIAL5;

  /*
   * Fill in the map
   *
   * Depending on the state of REGA.dv0, we will switch map.base back and
   * forth between bank0 and bank1 so that read accesses can be serviced
   * in the client.
   */
  rtc->map.base  = rtc->bank0;
  rtc->map.stride= 1;
  rtc->map.mask  = 0xff00;
  rtc->map.shift = 8;
  
  /* register server mediated write access to REGA */
  simRegisterAddrMatch(~0xFF,
		       TO_PHYSICAL(regA) | ADDR_MATCH_STORE,
		       SIM_loadStore_fwd,
		       targetCode,
		       &rtc->map);
  
  /* Register client side read/write processing for entire rtc */
  simRegisterAddrMatch(~0xFFFF & (~ADDR_MATCH_STORE),
		       TO_PHYSICAL(RTC_BASE & ~7),
		       SIM_loadStore,
		       targetCode,
		       &rtc->map);

  /*
   * NOTE: We lack proper "write" support for a variety of registers.
   */
}
