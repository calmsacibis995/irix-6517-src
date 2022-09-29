#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * do NOT change this to stderr.  if you do that, this program
 * generates TOTALLY USELESS output because you cannot tell which
 * error goes with which time measurement.
 *
 * we print a short error message to stderr for the stderr-diehards.
 */
FILE *errstream = stdout; /* do NOT change this to stderr */

void error(char *format, ...)
{
  va_list ap;

  fprintf( errstream, "testclocks:ERROR:");

  va_start(ap, format);
  vfprintf( errstream, format, ap );
  va_end(ap);

  fprintf( errstream, "\n" );

  fprintf(stderr, "ERROR: see stdout\n");
}

/* think anyone will notice this? ---------------------------------------- */

char *bigfail =
"===============================================\n"
"#######    #      ###   #       ####### ######\n"
"#         # #      #    #       #       #     #\n"
"#        #   #     #    #       #       #     #\n"
"#####   #     #    #    #       #####   #     #\n"
"#       #######    #    #       #       #     #\n"
"#       #     #    #    #       #       #     #\n"
"#       #     #   ###   ####### ####### ######\n"
"===============================================\n";

char *bigpass =
"===============================================\n"
"######     #     #####   #####  ####### ######\n"
"#     #   # #   #     # #     # #       #     #\n"
"#     #  #   #  #       #       #       #     #\n"
"######  #     #  #####   #####  #####   #     #\n"
"#       #######       #       # #       #     #\n"
"#       #     # #     # #     # #       #     #\n"
"#       #     #  #####   #####  ####### ######\n"
"===============================================\n";

/* o2 cyclecounter workaround ------------------------------------------- */

#undef O2_WAR

#ifdef O2_WAR

#include <invent.h>

int is_o2(void)
{
  inv_state_t *is = NULL;
  inventory_t *item;
  setinvent_r(&is);
  while (NULL != (item=getinvent_r(is)))
    {
      if (item->inv_class == INV_PROCESSOR &&
          item->inv_type == INV_CPUBOARD &&
          item->inv_state == INV_IP32BOARD)
        return 1;
    }
  return 0;
}

#endif

/* cycle counter utility ------------------------------------------------ */

/*
 * code stolen outright from 
 * rev 1.6 kudzu/isms/irix/lib/libc/src/sys/clock_gettime.c
 * simplified (removed timespec, seconds conversion)
 * handle 32->64 bit wrap in 32 bit timer case.
 *
 * XXX NOTE neither the original POSIX code nor my mods are MP-safe
 *
 * This uses the SYSSGI call SGI_CYCLECNTR_SIZE to write code that
 * should run on all SGI machines. The biggest problem is that on some
 * machines you read the counter into a 32 bit value, and on other
 * machines you read the counter into a 64 bit value. This code does
 * not try and handle the wrap as that would add limitations to how
 * often the user would have to call this function for correctness 
 */

typedef unsigned long long iotimer64_t;
typedef unsigned int iotimer32_t;

struct cyclecounter {
    volatile iotimer64_t *iotimer_addr64;
    volatile iotimer32_t *iotimer_addr32;
    unsigned int cycleval;
    ptrdiff_t iotimer_size;
    iotimer64_t counter_value64;
    double seconds_per_cycle;
};

struct cyclecounter *cc = NULL;

/*
 * take a cycle value from somewhere on the machine other than 
 * read_cycle_counter().  if necessary, fill in its upper 32 bits
 * so that you can compare it with a value from read_cycle_counter().
 * 
 * on machines wtih a 32 bit cyclecounter, this routine assumes that 
 * t represents a time within half the cyclecounter wrap period 
 * of the value last returned from read_cycle_counter().
 */
iotimer64_t
fix_cycle_value(iotimer64_t t)
{
  if (cc->iotimer_size == 64)
    return t;
  else
    {
      iotimer64_t u64;

      /* we start with
       *   - passed-in "user" 32 
       *   - previously snapped "counter" 64
       * goal is to compute user 64.
       */
      unsigned int u32 = (unsigned int)t;
      
      unsigned int c64_lo = (unsigned int)cc->counter_value64;
      unsigned int c64_hi = *((unsigned int *)(&cc->counter_value64));
      
      /* low 32 bits of u64 are the same (the easy part!) */
      
      u64 = (unsigned long long)u32;
      
      /* fill in high 32 bits of u64 (the hard part)
       *
       * we have two 32-bit cyclecounter values: c64_lo (the snapped
       * counter) and u32 (the users's counter).  we need to 
       * determine the temporal order of these two counter values.  
       * the assumption is that both c64_lo and u32 represent times
       * within 1/2 of the 32-bit counter wrap time.
       * therefore we can tell whether c64_lo is later or u32 is
       * later by seeing which of (c64_lo-u32) or (u32-c64_lo) falls
       * within that interval.  note that we can NOT determine which is later
       * just by looking at (c64_lo < u32).
       *
       * then, once we know the temporal order of u32 and c64_lo,
       * we then check to see if the two values are separated by a wrap
       * about the full 32-bit range.  if not, then the upper 32 bits of
       * u64 should match those of c64_val_snap.  if so, the upper 32 
       * bits of u64 should differ from the upper 32 bits of 
       * c64_val_snap by 1 or -1.
       *
       * this code could undoubtedly be collapsed into one clever and
       * utterly incomprehensible expression involving xors and stuff.
       */
      if (c64_lo - u32 < 2147483648U) /* this is 2^31, ~2000 seconds */
        {
          /* u32 is temporally first, then c64_lo */
          if (c64_lo >= u32) /* u32->c64_lo does not wrap */
            *((unsigned int *)&u64) = c64_hi;
          else /* u32->c64_lo wraps */
            *((unsigned int *)&u64) = c64_hi - 1;
        }
      else
        {
          /* c64_lo is temporally first, then u32 */
          if (u32 >= c64_lo) /* c64_lo->u32 does not wrap */
            *((unsigned int *)&u64) = c64_hi;
          else /* c64_lo->u32 wraps */
            *((unsigned int *)&u64) = c64_hi + 1;
        }

      return u64;
    }
}

/*
 * on machines with a 32 bit cyclecounter, you must call this routine
 * at least once per cyclecounter wrap period.
 */
iotimer64_t
read_cycle_counter(void)
{
#if (_MIPS_SIM == _ABIO32)
  unsigned int high, low;
#endif

  if (!cc) 
    {
      ptrdiff_t phys_addr, raddr;
      int poffmask;
      int fd;
      cc = (struct cyclecounter *)malloc (sizeof (struct cyclecounter));
      cc->iotimer_size = syssgi(SGI_CYCLECNTR_SIZE);
      poffmask = getpagesize() - 1;
      phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cc->cycleval);
      /* cc->cycleval in picoseconds per cycle! */
#ifdef O2_WAR
      if (is_o2() && cc->cycleval != 15000)
        {
          printf("**** O2 WORKAROUND ****\n");
          printf("using 15000picos/tick for cyclecounter instead of "
                 "reported %u.\n", cc->cycleval);
          cc->cycleval = 15000;
        }
#endif
      cc->seconds_per_cycle = cc->cycleval / (double)1.E12;
      raddr = phys_addr & ~poffmask;
      fd = open("/dev/mmem", O_RDONLY);
      printf("cycle counter on this machine is %d bits\n", cc->iotimer_size);
      printf("cycle counter frequency on this machine is %u"
             " picoseconds/tick\n", cc->cycleval);
      if(cc->iotimer_size == 64)
        {
          cc->iotimer_addr64 = (volatile iotimer64_t *)mmap(0,
                                                            poffmask,
                                                            PROT_READ,
                                                            MAP_PRIVATE,
                                                            fd,
                                                            (__psint_t)raddr);
          if (MAP_FAILED == cc->iotimer_addr64)
            {
              error("cycle counter mmap() failed");
              printf("%s\n", bigfail);
              exit(2);
            }
          cc->iotimer_addr64 = 
            (iotimer64_t *)((__psunsigned_t)(cc->iotimer_addr64) + 
                            (phys_addr & poffmask));
        } 
      else 
        {
          cc->iotimer_addr32 = (volatile iotimer32_t *)mmap(0,
                                                            poffmask,
                                                            PROT_READ,
                                                            MAP_PRIVATE,
                                                            fd,
                                                            (__psint_t)raddr);
          if (MAP_FAILED == cc->iotimer_addr32)
            {
              error("cycle counter mmap() failed");
              printf("%s\n", bigfail);
              exit(2);
            }
          cc->iotimer_addr32 = 
            (iotimer32_t *)((__psunsigned_t)(cc->iotimer_addr32) + 
                            (phys_addr & poffmask));
          
          cc->counter_value64 = 0; /* start with high bits 0 */
        }
      close(fd);
    }
  
  if(cc->iotimer_size == 64)
    {
      /*
       * Only for O32 do we have to do two reads. N32 and N64 both can
       * read the clock in one try.
       */
#if (_MIPS_SIM == _ABIO32)
      
      do 
        {
          high = *(unsigned int *)(cc->iotimer_addr64);
          low = *(unsigned int *)(((unsigned int *)(cc->iotimer_addr64)) + 1);
        } 
      while (high != *(unsigned int *)(cc->iotimer_addr64));
      cc->counter_value64 = ((long long)high << 32) + low;
      
#else 
      cc->counter_value64 = *(cc->iotimer_addr64);
#endif /* _MIPS_SZLONG == 32 */
    } 
  else 
    {
      /*
       * 32 bit timer - synthesize upper 32 bits
       * XXX NOTE! this only works if we call this function at least
       * XXX every 32-bit wrap period (ok for testclocks.c, but
       * XXX no doubt this code will be copied elsewhere!!)
       */
      iotimer32_t oldcounter_value32 = *(((int *)&cc->counter_value64) + 1);
      iotimer32_t newcounter_value32 = *(cc->iotimer_addr32);
      if (newcounter_value32 < oldcounter_value32)
        (*(((int *)&cc->counter_value64) + 0))++;
      
      *(((int *)&cc->counter_value64) + 1) = newcounter_value32;
    }

  return cc->counter_value64;
}

#define SECONDS(cycles) (((double)(cycles))*cc->seconds_per_cycle)
#define CYCLES(seconds) ((iotimer64_t)((seconds)/cc->seconds_per_cycle))

void
init_seconds_per_cycle(void)
{
  read_cycle_counter();
}

/* array of clocks ------------------------------------------------------ */

#define NCLOCKS 3

typedef struct clockmeasurement
{
  double us;                    /* time in microseconds */
  union {
    struct { stamp_t val; } ust;
    struct { struct timeval tv; } tod;
    struct { iotimer64_t val; } cyc;
  } u;
} clockmeasurement;

#define UST 0                   /* UST clock */
#define TOD 1                   /* gettimeofday() */
#define CYC 2                   /* cycle counter from syssgi() */

char *clockname[NCLOCKS] = {"UST", "gettimeofday()", "cycle counter"};
char *clockshortname[NCLOCKS] = {"ust", "tod", "cyc"};

double rateerror_ppm[NCLOCKS];

int need_to_continue_timedaemon = 0;

void initializeclocks(void)
{
  init_seconds_per_cycle();
  
  /* so far we've used at least a 100ppm crystal for all these */

  rateerror_ppm[UST] = 100.0;
  rateerror_ppm[TOD] = 100.0;
  rateerror_ppm[CYC] = 100.0;

  /* sanity check UST support */
  {
    stamp_t ust1, ust2;
    struct timeval tv;
    
    if (syssgi(SGI_GET_UST, &ust1, &tv) < 0)
      {
        error("syssgi(SGI_GET_UST, &ust, &tv) failed!");
        printf("%s\n", bigfail);
        exit(2);
      }
    
    /* UST clock resolution is at least 1us -- sleep at least 2us */
    sginap((CLK_TCK*2+1000000-1)/1000000);
    
    if (syssgi(SGI_GET_UST, &ust2, NULL) < 0) /* bug 518123 */
      {
        error("syssgi(SGI_GET_UST, &ust, NULL) failed!");
        printf("%s\n", bigfail);
        exit(2);
      }
    
    if (ust2 <= ust1)
      {
        error("UST clock not increasing over 2us: %lld->%lld!", 
               ust1, ust2);
        printf("%s\n", bigfail);
        exit(2);
      }
  }

  /* suspend timedeamon if it was running */
  {
    int tries;
    struct timeval delta;
    struct timeval olddelta;
    printf("timed/ntpd should not be running while this test is running:\n"
           "we want to test the local gettimeofday() implementation.\n"
           "sending STOP signal to any running timed or ntpd:\n");
    if (0 != geteuid())
      {
        printf("you must run testclocks as root\n");
        goto timedaemonsucks;
      }
    system("killall -STOP timed");
    system("killall -STOP ntpd");
    printf("now waiting until time settles down\n");
    delta.tv_sec = 0;
    delta.tv_usec = 0;
    tries = 0;
    for(;;)
      {
        if (adjtime(&delta, &olddelta) < 0)
          {
            error("adjtime: %s", strerror(oserror()) );
            goto timedaemonsucks;
          }
        printf("current adjtime() adjustment: %dsec %ldusec\n",
               olddelta.tv_sec,
               olddelta.tv_usec);
        if (olddelta.tv_sec==0 && olddelta.tv_usec==0)
          break;
        tries++;
        if (tries > 60)
          {
            printf("too many tries.\n");
            goto timedaemonsucks;
          }
        printf("time not settled yet...waiting and trying again\n");
        sleep(1);
      }
    printf("OK: time seems to be settled.\n");
  }
  
  return;

 timedaemonsucks:
  error("timedaemon is running and we cannot suspend it!");
  printf("%s\n", bigfail);
  exit(2);
}

void shutdownclocks(void)
{
  printf("sending CONT signal to any running timed or ntpd\n");
  system("killall -CONT timed");
  system("killall -CONT ntpd");
}

/* 
 * sample the clocks as simultaneously as possible.
 * returns upper bound on non-simultaneity in microseconds.
 */
double sampleclocks(clockmeasurement v[NCLOCKS])
{
  double cyc_us_2;

  /* sanity check that gettimeofday() is not being adjusted */
  {
    struct timeval delta;
    struct timeval olddelta;
    delta.tv_sec = 0;
    delta.tv_usec = 0;
    if (adjtime(&delta, &olddelta) < 0)
      {
        error("adjtime: %s", strerror(oserror()) );
        printf("%s\n", bigfail);
        shutdownclocks();
        exit(2);
      }
    else if (olddelta.tv_sec!=0 || olddelta.tv_usec!=0)
      {
        error("adjtime() reports gettimeofday adjustment!\n"
              "during this test, timed/ntp cannot be running and no one\n"
              "should be adjusting time.   somehow, these daemons have\n"
              "restarted or someone has manually adjusted the time.");
        printf("%s\n", bigfail);
        shutdownclocks();
        exit(2);
      }
  }

  /* grab clocks.  grab cyclecounter before _and_ after others */
  v[CYC].u.cyc.val = read_cycle_counter();
  if (syssgi(SGI_GET_UST, &v[UST].u.ust.val, &v[TOD].u.tod.tv) < 0)
    {
      error("syssgi(SGI_GET_UST, &ust, &tv) failed intermittently!");
      printf("%s\n", bigfail);
      shutdownclocks();
      exit(2);
    }
  cyc_us_2 = SECONDS(read_cycle_counter()) * 1.E6;

  v[UST].us = (double)(v[UST].u.ust.val) / 1.E3;
  v[TOD].us = 
    (1.E6 * v[TOD].u.tod.tv.tv_sec) + (double)v[TOD].u.tod.tv.tv_usec;
  v[CYC].us = SECONDS(v[CYC].u.cyc.val) * 1.E6;
  
  assert(cyc_us_2 > v[CYC].us);
  return cyc_us_2 - v[CYC].us;
}

void printclock(clockmeasurement v[NCLOCKS], int i)
{
  switch (i)
    {
    case UST:
      printf("0x%016llx ns", v[UST].u.ust.val);
      break;
    case TOD:
      printf("(%d s,%d us)", 
             v[TOD].u.tod.tv.tv_sec, v[TOD].u.tod.tv.tv_usec);
      break;
    case CYC:
      printf("0x%016llx cyc", v[CYC].u.cyc.val);
      break;
    default:
      error("bad clock");
      printf("%s\n", bigfail);
      shutdownclocks();
      exit(2);
    }
}

void printclocks(clockmeasurement v[NCLOCKS])
{
  int i;
  for(i=0; i < NCLOCKS; i++)
    {
      printf("  %s=", clockshortname[i]);
      printclock(v, i);
      printf("\n");
    }
}

/* clock test ------------------------------------------------------ */

typedef struct analysis
{
  int id;
  clockmeasurement data[NCLOCKS];
  double rate_ratio[NCLOCKS];
  double rate_ratio_error_ppm[NCLOCKS];
} analysis;


void printanalysis(analysis *a)
{
  int i;
  for(i=0; i < NCLOCKS; i++)
    { 
      printf("  %s/%s=%7.3lf error=%+7.2lfppm %s=", 
             clockshortname[i],
             clockshortname[0],
             a->rate_ratio[i],
             a->rate_ratio_error_ppm[i],
             clockshortname[i]);
      printclock(a->data, i);
      printf("\n");
    }
}

void main(int argc, char **argv)
{
  int c;
  clockmeasurement first[NCLOCKS];
  analysis last;
  double lastreport;
  int tries;
  int measurementcount;
  int i;
  double bestrateerror_ppm;
  double maximum_measurement_error_allowed;
  double test_duration_us = 1.E6 * 60;
  int keepgoing = 0;

  while ((c = getopt(argc, argv, "d:k")) != EOF) 
    {
      switch(c) 
	{
        case 'd':
          test_duration_us = atof(optarg)*1.E6;
          break;
        case 'k':
          keepgoing = 1;
          break;
        }
    }

  printf("---------------------------------------------------------------\n");
  printf("testclocks: test UST, gettimeofday(), and cycle counter clocks.\n");
  printf("\n");
  if (test_duration_us < 0)
    printf("program will run indefinitely");
  else
    printf("program will run for %.1f seconds", 
           test_duration_us/1.E6);
  printf(" and verify that clocks have the\n"
         "correct rate ratio over the long term, as well as behaving sanely\n"
         "in the short term (ie, no huge jumps, no backwards jumps).\n");
  printf("  -k makes the program keep going on errors.\n");
  printf("  -d <seconds> says how long to run for, negative means forever.\n");
  printf("\n");

  initializeclocks();

  /* compute best rate accuracy of all our clocks */

  bestrateerror_ppm = rateerror_ppm[0];
  for(i=1; i < NCLOCKS; i++)
    if (rateerror_ppm[i] < bestrateerror_ppm)
      bestrateerror_ppm = rateerror_ppm[i];

#define MAXTRIES 10000
  /*
   * we're trying to measure the rates of crystals with at best
   * bestrateerror_ppm parts per million error.  we do this
   * by snapping the clocks into "first," and then, 2 seconds
   * or more later, snapping them again into "cur."
   *
   * we can't snap all the clocks perfectly simultaneously,
   * but we do have an upper bound on the non-simultaneity of
   * our clock values in each snap.
   *
   * when doing a snap, we repeatedly try the snap until the
   * snapped clock values are close enough to accurately 
   * measure a rate error of bestrateerror_ppm over a >=2 
   * second interval.  the maximum allowed error of
   * the "first" and "cur" measurements together is:
   *
   *   = (2.E6 us) * (bestrateerror_ppm / 1.E6)
   *   = (2 * bestrateerror_ppm) us
   *
   * we make another conservative simpification and require
   * both the "first" and "cur" measurements to fall within half
   * this simultaneity.
   *
   * if it takes us more than MAXTRIES tries, then one or more of these
   * is true:
   * - one or more of the clocks are way broken
   * - the system is overburdened
   * - we need to increase the 2 second minimum snap difference
   * - our clocks are just too accurate for the current minimum snap difference
   */
  maximum_measurement_error_allowed = bestrateerror_ppm;
  printf("what is going on:\n");
  printf("  this program takes a series of measurements.  each measurement\n"
         "  consists of a simultaneous snap from each clock.  when taking a\n"
         "  measurement, we will repeat the snaps until we get a set of\n"
         "  clock values which are within %lf us of each other, or until"
         "  we have exhausted %d tries (which signals an error).\n",
         maximum_measurement_error_allowed, MAXTRIES);
  
#define sample_within_error_bound(clocks) \
  tries = 0; \
  while (sampleclocks(clocks) > maximum_measurement_error_allowed) \
    if (++tries > MAXTRIES) \
      { \
        error("too many retries to get simultaneous clock snap"); \
        printf("%s\n", bigfail); \
        if (!keepgoing) \
          { \
            shutdownclocks(); \
            exit(2); \
          } \
      } \
  if (tries > 1) printf("%d\n", tries);

  /* grab the first snap of clocks */

  sample_within_error_bound(first);
  printf("initial clock measurement:\n");
  printclocks(first);

  /* first and cur are at least 2 seconds apart */
  sleep(2);

  measurementcount = 1;
  lastreport = -1;
  for(;;)
    {
      analysis cur;
      int bad = 0;
      int last_is_valid = (measurementcount > 1);

      /* take a measurement of each clock */

      cur.id = measurementcount;
      sample_within_error_bound(cur.data);

      for(i=0; i < NCLOCKS; i++)
        { 
          /* XXX is this right thing if i==0 ? */
          double acceptable_rate_ratio_error_ppm = 
            rateerror_ppm[i] + rateerror_ppm[0];
          
          /* compute the long-term rate ratio of each clock vs clock 0 */
          
          cur.rate_ratio[i] = 
            (cur.data[i].us-first[i].us) / (cur.data[0].us-first[0].us);
          cur.rate_ratio_error_ppm[i] = (cur.rate_ratio[i] - 1.0) * 1.E6;

          /* do clocks meet their nominal rate ratio? */

          if (fabs(cur.rate_ratio_error_ppm[i]) > 
              acceptable_rate_ratio_error_ppm)
            {
              error("clock rate ratio error (see below).\n"
                    "the ratio between the rate of the %s clock\n"
                    "and the rate of the %s clock is off by more than\n"
                    "the %lfppm tolerance from the nominal ratio.\n"
                    "the code for one or both of these clocks is in error.\n"
                    "probably this is due to a mismatch between the\n"
                    "rate for which the kernel software is written and\n"
                    "the rate of the actual crystals used in the\n"
                    "hardware.  please assign or update a bug.",
                    clockname[i],
                    clockname[0],
                    acceptable_rate_ratio_error_ppm);
              bad = 1;
            }
          
          /* are clocks sane? */
          
          if (last_is_valid)
            {
              double jump = cur.data[i].us - last.data[i].us;
              if (jump < 0)
                { 
                  error("%s clock went backwards", clockshortname[i]); 
                  bad = 1; 
                }
              else if (jump >= 1.E6)
                { 
                  error("%s clock jumped forward by >= 1 second",
                        clockshortname[i]); 
                  bad = 1; 
                }
            }
          
        }

      if (bad || lastreport < 0 || cur.data[0].us > lastreport + 1.E6)
        {
          if (bad)
            {
              if (last_is_valid)
                {
                  printf("previous measurement %d (may contain failure):\n", 
                         last.id);
                  printanalysis(&last);
                }
              
              printf("current measurement %d (contains failure):\n", cur.id);
            }
          else
            printf("measurement %d:\n", cur.id);
          
          printanalysis(&cur);

          if (bad)
            {
              printf("%s\n", bigfail);
              
              if (!keepgoing)
                {
                  shutdownclocks();
                  exit(2);
                }
            }

          lastreport = cur.data[0].us;
        }
      
      if (test_duration_us > 0 &&
          (cur.data[0].us - first[0].us) >= test_duration_us)
        break;
      
      bcopy(&cur, &last, sizeof(analysis));
      measurementcount++;
    }

  printf("%s\n", bigpass);

  printf("PASSED\n");

  shutdownclocks();
  exit(0);
}
