#include "SIM.h"
#include <ds1685.h>
#include <assert.h>
#include <libsc.h>
#include </usr/include/time.h>


#define UPDATE_RATE  5



/*
 * A minimal simulation of the RTC.
 *
 * The basic clock and RAM is simulated but no particular attention is
 * given to some of the control bits.
 */

/*
 * DS1685 block
 */
static unsigned char rtcBank0[128]; /* Base + bank 0 */
static unsigned char rtcBank1[128]; /* Bank 1 (ignore the first 64) */
static unsigned char rtcExtended[128];

/************
 * initSimRTC
 *-----------
 * Zap it all.  Note that this *stops* the clock.
 */
void
initSimRTC(){
  memset(rtcBank0,0,sizeof rtcBank0);
  memset(rtcBank1,0,sizeof rtcBank1);
  memset(rtcExtended,0,sizeof rtcExtended);

  rtcBank1[RTC_SERIAL0] = SERIAL0;
  rtcBank1[RTC_SERIAL1] = SERIAL1;
  rtcBank1[RTC_SERIAL2] = SERIAL2;
  rtcBank1[RTC_SERIAL3] = SERIAL3;
  rtcBank1[RTC_SERIAL4] = SERIAL4;
  rtcBank1[RTC_SERIAL5] = SERIAL5;
}



/************
 * base_nvram	Access base NVRAM
 */
void
base_nvram(Instr_t inst, Ctx_t* sc, int reg){
  if (loadstoreSize(inst)!=1){
    char buf[128];
    sprintf(buf,
	    " rtc, non-byte write, results undefined (nop here )\n"
	    "      reg: %d\n",
	    reg);
    SIM_message(buf);
    advancePC(inst, sc);
  }
  else
    loadstore(inst,sc,&rtcBank0[reg]);
}

/*********
 * running	Returns true if RTC is running
 */
static int
running(void){
  unsigned char ra = rtcBank0[RTC_REGA];
  return (ra & (RTC_A_DV1|RTC_A_DV2))==RTC_A_DV1;
}

/***********
 * increment	BCD clock adder and wrapper
 */
static int
increment(int regNum, int max, int wrap){

  int v = (rtcBank0[regNum]>>4)*10 + (rtcBank0[regNum] & 0xf);
  v++;
  rtcBank0[regNum] = ((v/10)<<4) | (v%10);

  if (rtcBank0[regNum]>max){
    rtcBank0[regNum] = wrap;
    return 1;
  }
  return 0;
}


/********
 * update	Updates time/date one in UPDATE_RATE calls
 */
static void
update(void){
  if (running()){
    static int uipCtr;
    if ((uipCtr = ++uipCtr % UPDATE_RATE)==0){
      rtcBank0[RTC_REGA] |= RTC_A_UIP;
      /* Increment seconds, minutes, hours.  Ignore days, months, ... */
      if (increment(RTC_SEC,0x59,0))
	if (increment(RTC_MIN,0x59,0))
	  increment(RTC_HR,0x23,0);
    }
    else
      rtcBank0[RTC_REGA] &= ~RTC_A_UIP;
  }
}


/*********
 * sim_rtc	Service rtc requests
 */
void
SIM_rtc(Instr_t inst, Saddr_t eadr, Ctx_t* sc, int d){
  int regNum = (eadr>>8) & 0xff;

  /* Dispatch according to register */
  switch(regNum){
  case RTC_SEC:
  case RTC_SEC_ALRM:
  case RTC_MIN:
  case RTC_MIN_ALRM:
  case RTC_HR:
  case RTC_HR_ALRM:
  case RTC_DAY_OF_WEEK:
  case RTC_DAY_OF_MONTH:
  case RTC_MONTH:
  case RTC_YEAR:
    loadstore(inst,sc,&rtcBank0[regNum]);
    break;
  case RTC_REGA:
    update();
  case RTC_REGB:
  case RTC_REGC:
  case RTC_REGD:
    loadstore(inst,sc,&rtcBank0[regNum]);
    break;

  default:
    /* Not one of the base clock/control registers */

    if (regNum >=RTC_NVRAMBASE && regNum<= RTC_BANKBASE)
      base_nvram(inst,sc,regNum);
    else
      if (rtcBank0[RTC_REGA] & RTC_A_DV0){
	switch(regNum){
	case RTC_MODEL:
	case RTC_SERIAL0:
	case RTC_SERIAL1:
	case RTC_SERIAL2:
	case RTC_SERIAL3:
	case RTC_SERIAL4:
	case RTC_SERIAL5:
	  if (isStore(inst))
	    advancePC(inst,sc);
	  else
	    loadstore(inst,sc,&rtcBank1[regNum]);
	  break;
	case RTC_CENTURY:
	  loadstore(inst,sc,&rtcBank1[regNum]);
	  break;
	default:
	  sim_assert(0);
	}
      }
      else
	base_nvram(inst,sc,regNum);
  }
}


