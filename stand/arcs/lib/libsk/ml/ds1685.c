#if IP32 /* whole file */
/*
 * Dallas 1685 RTC chip support code
 *
 * This file replaces the ds1286.c file in lib/libsk.  It's not clear
 * that there are enough changes form ds1286 to warrant a new version but
 * the 1685 is *not* a 1286 so some renaming would need to be done at the
 * minimum.
 */

#include <sys/cpu.h>
#include <sys/clock.h>
#include <sys/ds17287.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <assert.h>
#include <libsc.h>
#include <libsk.h>

/*****************************************************************************
 *                                                                           *
 *                         Clock related routines                            *
 *                                                                           *
 *****************************************************************************/

/* NOTE: RELEASE_DATE is used as the "default" time when reseting the RTC. */
#define RELEASE_DATE 0,0,0,1,1,0x96,0x19,2

/* Clock Access Mode (used by clockAccess) */
enum { readClock, writeClock };


static int clockAccess(int, unsigned char*);
static void   resetClock(void);
static unsigned short fromBcd(unsigned);
static unsigned short toBcd(unsigned);



/*************
 * cpu_set_tod	Set time of day
 */
void
cpu_set_tod(TIMEINFO* t){
  unsigned char buf[8];
  int sec;

  sec =
    t->Seconds +
      t->Minutes * SECMIN +
	t->Hour * SECHOUR +
	  (t->Day-1) * SECDAY;

  buf[0] = toBcd(t->Seconds);
  buf[1] = toBcd(t->Minutes);
  buf[2] = toBcd(t->Hour);
  buf[3] = toBcd(t->Day);
  buf[4] = toBcd(t->Month);
  buf[5] = toBcd(t->Year%100);
  buf[6] = toBcd(t->Year/100);
  buf[7] = toBcd(_rtodc(sec,t->Year,t->Month));
  clockAccess(writeClock,buf);
}


/*************
 * cpu_get_tod	Get time of day and return it in ARCS TIMEINFO format
 */
void
cpu_get_tod(TIMEINFO* t){
  unsigned char buf[8];
  static int tries = 0;		/* Total  */

 repeat:
  if (clockAccess(readClock,buf)){
    if (tries++ >= 6)
      return;
    resetClock();
    goto repeat;
  }
  else{
    t->Milliseconds = 0;
    t->Seconds      = fromBcd(buf[0]);
    t->Minutes      = fromBcd(buf[1]);
    t->Hour         = fromBcd(buf[2]);
    t->Day          = fromBcd(buf[3]);
    t->Month	    = fromBcd(buf[4]);
    t->Year	    = fromBcd(buf[5]) + fromBcd(buf[6])*100;
    
    /* Sanity check */
    if (t->Seconds > 59|| t->Minutes > 59 || t->Hour > 23 ||
	t->Day < 1     || t->Day > 31     || t->Month < 1 ||
	t->Month > 12)
      if (tries++ <6){
	/* We try to recover a few times before giving up */
	printf("Warning: time invalid, resetting clock to epoch.\n");
	resetClock();
      }
  }
  return;
}


/********
 * _rtodc	Return time of day in seconds from beginning of epoch
 *=======
 *
 * From ds1286.c
 */
LONG
_rtodc(int secs,int year,int month)
{
  register int i;
  extern int month_days[12];
  
  /* Count seconds for full months that have elapsed.
   */
  for (i = 0; i < month-1; i++)		/* only count full months */
    secs += month_days[i] * SECDAY;
  
  /*  If operating on a leap year, and Feb has already passed, add in
   * another day.
   */
  if (LEAPYEAR(year) && (month > 2))
    secs += SECDAY;
  
  /* Count previous years back to the epoc.
   */
  while (--year >= YRREF) {
    secs += SECYR;
    if (LEAPYEAR(year))
      secs += SECDAY;
  }
  
  return(secs);
}


#define EXTRA_DAYS      3

/****************
 * get_dayof_week	Return day of week (1==Sunday)
 */
int
get_dayof_week(LONG year_secs)
{
  int days,yr=YRREF;
  int year_day;
  
  days = 0;
  for (;;) {
    register int secyear = SECYR;
    
    if (LEAPYEAR(yr))
      secyear += SECDAY;
    
    if (year_secs >=secyear)
      year_day = secyear;
    else
      year_day = year_secs;
    for (;;) {
      if (year_day < SECDAY)
	break;
      
      year_day -= SECDAY;
      days++;
    }
    
    if (year_secs < secyear)
      break;
    year_secs -= secyear;
    yr++;
  }
  
  if (days > EXTRA_DAYS) {
    days -=EXTRA_DAYS;
    days %= 7;
    days %= 7;
  } else
    days +=EXTRA_DAYS+1;
  return(days+1);
}


/**********
 * fromBcd	Convert 2 bcd digits to binary
 *=========
 *
 * Past clock chips were used in BCD mode even though most supported
 * binary mode (as does the 1685).  We continue to operate in BCD mode
 * so this rtn is needed.
 */
static unsigned short
fromBcd(unsigned b){
  return (b>>4) * 10 + (b &0xf);
}


/********
 * toBcd	Convert number to 2 digit bcd
 */
static unsigned short
toBcd(unsigned b){
  if (b>99)
    return 0x99;
  return (b/10)<<4 | b%10;
}


/**************
 * clock_access
 */
static int
clockAccess (int cmd, unsigned char* buf){
  ds17287_clk_t * clock = (ds17287_clk_t*)RTC_BASE;
  int sanity = 1000000;

  /* Clock must be counting */
  if ((clock->registera & (DS_REGA_DV2|DS_REGA_DV1))!= DS_REGA_DV1)
    return 1;

  /* Switch to bank 1 if necessary */
  if ((clock->registera & DS_REGA_DV0) == 0)
    clock->registera |= DS_REGA_DV0;

  /*
   * To avoid accessing the RTC when it's updating it's registers,
   * we wait for the UIP to be clear.  However, just in case
   * the clock is broken, we have a sanity counter that counts down to zero.
   */
  while (clock->registera & DS_REGA_UIP)
    if (--sanity==0)
      return 1;

  /* Either read or write the clock registers */
  if (cmd == readClock){
    *buf++ = clock->sec;
    *buf++ = clock->min;
    *buf++ = clock->hour;
    *buf++ = clock->date;
    *buf++ = clock->month;
    *buf++ = clock->year;
    *buf++ = clock->ram[DS_BANK1_CENTURY];
    *buf++ = clock->day;
  }
  else{
    clock->sec			= *buf++;
    clock->min		   	= *buf++;
    clock->hour		   	= *buf++;
    clock->date		   	= *buf++;
    clock->month	   	= *buf++;
    clock->year		   	= *buf++;
    clock->ram[DS_BANK1_CENTURY]= *buf++;
    clock->day	           	= *buf++;
  }
  return 0;
}

/************
 * resetClock	Reset clock to a known date and start it.
 */
static void
resetClock(void){
  ds17287_clk_t * clock = (ds17287_clk_t*)RTC_BASE;
  unsigned char buf[8] = {RELEASE_DATE};

  /*
   * Stop the updates.
   * Set the clock to 24 hour time, BCD
   * Enable the oscillator.
   * Reenable updates.
   */
  clock->registerb = (clock->registerb|DS_REGB_SET|DS_REGB_2412) & ~DS_REGB_DM;
  clock->registera = (clock->registera |DS_REGA_DV1) & ~DS_REGA_DV2;
  clock->registerb &= ~DS_REGB_SET;

  /* Note, we assume POST detect battery death */

  clockAccess(writeClock,buf);

  printf("\nInitialized tod clock.\n");
}


/*****************
 * cpu_restart_rtc
 */
int
cpu_restart_rtc(void){
  TIMEINFO t;
  cpu_get_tod(&t);
  return 0;
}




#endif /* IP32 */
