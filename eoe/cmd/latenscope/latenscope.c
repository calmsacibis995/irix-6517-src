#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

#include <sys/syssgi.h>
#include <sys/par.h>
#include <sys/prf.h>
#include <sys/rtmon.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sched.h>

/*
 * Latenscope FAQ
 *
 * What does latenscope measure?
 *
 *    Kernel event response latencies.
 *
 * How does latenscope define a kernel event response latency?
 *
 *    A kernel event response latency is the time difference between
 *    when a kernel event occurs prompting a thread to be runable
 *    and when the thread begins executing instructions in response
 *    to the kernel event.
 *
 *    For real-time applications, an example is the expiration of a timer.
 *    Below is a timeline diagram for a timer event using the nanosleep()
 *    system call. (The times are not to scale.)
 *
 *                              time -->
 *
 *    ----|--------------|-------------|-----------------|---------->
 *
 *        ^              ^             ^                 ^
 *    nanosleep()    timer set   timer expires      nanosleep()
 *    call begins                                   returns &
 *                                                  user instructions
 *                                                  begin executing
 *
 *    In this example, the timer expiring is the kernel event of
 *    interest to the application. The response latency is the time
 *    when nanosleep() completes execution, returning control to the
 *    application control minus the time at which the timer expires.
 *    This latency response time includes any activity by the
 *    kernel scheduler to run the thread.
 *
 */

int cpu = 0;

int debugflag;

/* Utility Routines ----------------------------------------------- */

#define roundup(x, y) ((((x)+((y)-1))/(y))*(y))
#define max(a,b) ( ((a)>(b)) ? (a) : (b))
#define min(a,b) ( ((a)<(b)) ? (a) : (b))

char *dupsprintf(char *buf, char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vsprintf(buf, format, ap );
  va_end(ap);
  
  return strdup(buf);
}

void error_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf(stderr, format, ap );
  va_end(ap);

  fprintf(stderr, "\n" );
  exit(EXIT_FAILURE);
}

void error2(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stdout, format, ap );
  va_end(ap);

  fprintf( stdout, "\n" );
}

void perror_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf(stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": %s\n", strerror(oserror()));
  exit(EXIT_FAILURE);
}

void perror2(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf(stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": %s\n", strerror(oserror()));
}

/*
** OC -- "OS Check".  Wraps a Unix call that returns negative on error.
*/

#define OC(call) \
{ \
  if ( (call) < 0 ) \
    perror_exit(#call); \
}

/*
** OCEINTR -- "OS Check exc. EINTR".  Same but ignores EINTR.
*/

#define OCEINTR(call) \
{ \
  if ( (call) < 0 && oserror() != EINTR ) \
    perror_exit(#call); \
}

/*
** OCEEXIST -- "OS Check exc. EINTR".  Same but ignores EEXIST.
*/

#define OCEEXIST(call) \
{ \
  if ( (call) < 0 && oserror() != EEXIST ) \
    perror_exit(#call); \
}

/*
** ONC -- "OS Null Check".  Wraps a Unix call that returns NULL on error.
*/

#define ONC(call) \
{ \
  if ( (call) == NULL ) \
    perror_exit(#call); \
}

/*
** OSIGC -- "OS SIG_ERR Check".  Wraps a Unix call that returns SIG_ERR 
**                               on error.
*/

#define OSIGC(call) \
{ \
  if ( (call) == SIG_ERR ) \
    perror_exit(#call); \
}

/*
** OMAPFAILC -- "OS MAP_FAILED Check".  Wraps an mmap() that returns
**                                      MAP_FAILED on error.
*/

#define OMAPFAILC(call) \
{ \
  if ( (call) == MAP_FAILED ) \
    perror_exit(#call); \
}

/* namelist fun ------------------------------------------------------- */

#include "prf.h"

int dwarfsyms;

typedef struct 
{
  char		*name;
  symaddr_t	addr;
} syment_t;

static int symcnt;			/* number of symbols */
static int symlistsize;			/* size of malloc'ed memory for list */
static syment_t *symlist;		/* list of symbols */
static syment_t *symnext;		/* next free slot */

#define	LISTINCR	6000

static void
listinit(void)
{
  symlist = (syment_t *)malloc(LISTINCR * sizeof (syment_t));
  if (symlist == NULL)
    error_exit("cannot allocate memory");
  symnext = symlist;
  symcnt = 0;
  symlistsize = LISTINCR;
}

/* ARGSUSED */
static void
listadd(char *name, symaddr_t addr)
{
  if (symcnt >= symlistsize) 
    {
      symlistsize += LISTINCR;
      symlist = (syment_t *)
        realloc(symlist, symlistsize * sizeof(syment_t));
      if (symlist == NULL)
        error_exit("cannot allocate memory");
      symnext = &symlist[symcnt];
    }
  symnext->name = name;
  symnext->addr = addr;
  symnext++;
  symcnt++;
}

int
list_repeated(symaddr_t addr)
{
  register syment_t *sp;
  
  for(sp = symlist; sp < &symlist[symcnt]; sp++)
    if(sp->addr == addr ) 
      return 1;
  return 0;
}

static syment_t *
search(symaddr_t addr)
{
  register syment_t *sp;
  register syment_t *save;
  symaddr_t value;
  
  value = 0;
  save = 0;
  for(sp = symlist; sp < &symlist[symcnt]; sp++) {
    if(sp->addr <= addr && sp->addr > value) {
      value = sp->addr;
      save = sp;
    }
  }
  return(save);
}

extern int  rdsymtab(int, void (*)(char *, symaddr_t));
extern int  rdelfsymtab(int, void (*)(char *, symaddr_t));

int kernfd;

void
init_namelist(char *kernname)
{
  printf("Getting symbols from kernel %s...\n", kernname);
  if((kernfd = open(kernname, O_RDONLY)) < 0)
    error2("cannot open kernel %s to get its symbols", kernname);
  listinit();
  dwarfsyms = rdsymtab(kernfd, listadd);
  if (rdelfsymtab(kernfd, listadd) && dwarfsyms) 
    error_exit("unable to load dwarf/mdebug "
               "or elf symbol table information from %s", kernname);
}
  
/* cycle counter ------------------------------------------------------ */

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
      cc->seconds_per_cycle = cc->cycleval / (double)1.E12;
      raddr = phys_addr & ~poffmask;
      fd = open("/dev/mmem", O_RDONLY);
      if (debugflag)
        printf("io timer size is %d\n", cc->iotimer_size);
      if(cc->iotimer_size == 64){
        cc->iotimer_addr64 = (volatile iotimer64_t *)mmap(0,
                                                          poffmask,
                                                          PROT_READ,
                                                          MAP_PRIVATE,
                                                          fd,
                                                          (__psint_t)raddr);
        cc->iotimer_addr64 = 
          (iotimer64_t *)((__psunsigned_t)(cc->iotimer_addr64) + 
                          (phys_addr & poffmask));
      } else {
        cc->iotimer_addr32 = (volatile iotimer32_t *)mmap(0,
                                                          poffmask,
                                                          PROT_READ,
                                                          MAP_PRIVATE,
                                                          fd,
                                                          (__psint_t)raddr);
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
      cc->counter_value64 = *(cc->iotimer_addr64);
    } 
  else 
    {
      /*
       * 32 bit timer - synthesize upper 32 bits
       * XXX NOTE! this only works if we call this function at least
       * XXX every 32-bit wrap period (ok for latenscope.c, but
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


/* util ---------------------------------------------------------- */

void
printevent(tstamp_event_entry_t *ev)
{
  int j;
  
  printf("event evt=0x%x tstamp=0x%llx jumbocnt=0x%x ",
         ev->evt, ev->tstamp, ev->jumbocnt);
  for(j=0; j < TSTAMP_NUM_QUALS; j++)
    {
      printf("%llx ", ev->qual[j]);
    }
  printf("\n");
}

/* stacks ---------------------------------------------------------- */

tstamp_shared_state_t *state;
tstamp_event_entry_t *events;
int num_events;
int our_index;

#define INDEXADD(index, count) (((index)+(count))%num_events)

/*ARGSUSED*/
void
init_stacks(char *kernname, double buffering)
{
  ptrdiff_t addr;
  int mmemfd;
  char *sbase;
  uint64_t mask;
  int i;

  /*
   * worst case is 1 PRF_MAXSTACK-deep stack every millisecond
   */
  num_events = buffering * 1000.0 * PRF_MAXSTACK;
  if (debugflag)
    printf("creating stack trace buffer with %d entries\n", num_events);
  
#if 0
  {
    /* start /dev/prf capturing stacks */
    
    char strbuf[1024];
    sprintf(strbuf, "killall rtmond");
    printf("executing [%s]\n", strbuf);
    system(strbuf);
    sprintf(strbuf, "prfld %s", kernname);
    printf("executing [%s]\n", strbuf);
    system(strbuf);
    sprintf(strbuf, "prfstat stack");
    printf("executing [%s]\n", strbuf);
    system(strbuf);
  }
#endif

  /* create tstamp buffer to receive stacks */

  OC(mmemfd = open("/dev/mmem", O_RDONLY));
  /*
   * clear the mask to prevent the previous app's messages from
   * coming in, and to allow us to delete and recreate the buffer
   *
   * this will fail if there is no previous tstamp buffer, which is
   * ok.
   */
  syssgi(SGI_RT_TSTAMP_MASK, cpu, (uint64_t) 0, (caddr_t)NULL);
  addr = syssgi(SGI_RT_TSTAMP_CREATE, cpu, num_events);
  if ((int)addr == -1) 
    {
      if (oserror() != EEXIST)
        error_exit("SGI_RT_TSTAMP_CREATE failed");
      
      /*
       * A timestamp buffer already exists (presumably
       * from a previous instance of this server); delete
       * it and install our own buffer.
       */
      OC(syssgi(SGI_RT_TSTAMP_DELETE, cpu));
      addr = syssgi(SGI_RT_TSTAMP_CREATE, cpu, num_events);
      if ((int)addr == -1)
        error_exit("SGI_RT_TSTAMP_CREATE failed");
    }
  OMAPFAILC((void *)(sbase = (char*)mmap(0,
                                         num_events*
                                         sizeof (tstamp_event_entry_t) +
                                         TSTAMP_SHARED_STATE_LEN,
                                         PROT_WRITE, MAP_PRIVATE, 
                                         mmemfd, (off_t)addr)));
  close(mmemfd);

  state = (tstamp_shared_state_t*)sbase;
  events = (tstamp_event_entry_t*)(sbase + TSTAMP_SHARED_STATE_LEN);
  
  /* set the buffers to stop when end_of_buffer is reached */
  OC(syssgi(SGI_RT_TSTAMP_EOB_MODE, cpu, RT_TSTAMP_EOB_STOP, (caddr_t)NULL ));
  
  for (i = 0; i < num_events; i++)
    events[i].evt = 0;

  /* select profile events only */
  
  mask = RTMON_PROFILE;
  OC(syssgi(SGI_RT_TSTAMP_MASK, cpu, mask, (caddr_t)NULL));

  our_index = 0;
}

void scan_stacks(iotimer64_t until, int print, iotimer64_t printrelative)
{
  int our_count = 0;
  int their_count = state->tstamp_counter;

  if (debugflag)
    printf("scan_stacks count=%d, idx=%d, ouridx=%d, lost=%d\n",
           their_count,
           state->curr_index,
           our_index,
           state->lost_counter);

  if (state->lost_counter > 0)
    {
      error_exit("ERROR: we lost some stack tracing data. "
                 "results incomplete.");
      /*
       * when this happens, kernel stops enqueueing.
       * therefore once we've seen lost_counter > 0 we can assume
       * - lost counter will stay  > 0
       * - state->curr_index and state->tstamp_counter are unchanging.
       * - any events[] entries with evt==0 will stay that way
       *   until we make the UPDATE syssgi below.
       *
       * our tstamp_counter value (their_count) may be pre-lost_counter>0
       * so we re-snap it here.
       *
       * XXX should tstamp_counter always be num_events?
       * I suppose not if a jumbo event fails but a small event succeeds.
       *
       * we then read what data we can to clear its evt field.
       *
       * we then re-snap a curr_index below as rtmond does
       * and we consume all the data, even the part with evt==0
       * (XXX could we increment our_index by count instead of
       * re-snapping curr_index?  why does rtmond do it that way?)
       */
      their_count = state->tstamp_counter;
    }
  
  /*
   * eat events.
   * events that show up in their_count may not be filled in
   * yet (event->evt may be zero).
   */
  for(;;)
    {
      tstamp_event_entry_t *ev;

      if (our_count >= their_count)
        {
          if (debugflag)
            printf("scan_stacks bailing: no more entries\n");
          break;
        }
      if (events[our_index].evt == 0)
        {
          if (debugflag)
            printf("scan_stacks bailing: hit an unfilled entry\n");
          break;
        }

      ev = &events[our_index];
      
      /* fill in upper bits if necessary */
      ev->tstamp = (__int64_t)fix_cycle_value(ev->tstamp);
      
      /* bail if we reach timestamp "until". (see above for lost_counter) */
      if (state->lost_counter <= 0 && ev->tstamp >= until)
        {
          if (debugflag)
            printf("scan_stacks bailing on stamp %lfms (0x%016llx)\n", 
                   1000.0*SECONDS(ev->tstamp),
                   ev->tstamp);
          break;
        }
      
      /* printevent(ev); */
      
      if (print)
        {
          if (ev->evt == TSTAMP_EV_PROF_STACK64 ||
              ev->evt == TSTAMP_EV_PROF_STACK32)
            {
              uint64_t *pcs64 = (uint64_t *)ev->qual;
              uint32_t *pcs32 = (uint32_t *)ev->qual;
              int npcs = (ev->evt == TSTAMP_EV_PROF_STACK64
                          ? TSTAMP_NUM_QUALS : 2*TSTAMP_NUM_QUALS);
              int j;

              for (j = 0; j < npcs; j++)
                {
                  symaddr_t pc;
		  int flags;
                  syment_t *sp;

		  if (ev->evt == TSTAMP_EV_PROF_STACK64) 
			  pc = pcs64[j];
		  else 
			  pc = pcs32[j];

                  flags = pc & (PRF_STACKSTART|PRF_STACKEND);

                  pc &= ~(PRF_STACKSTART|PRF_STACKEND);
                  if (flags & PRF_STACKSTART)
                    {
                      printf("  stack: (holdoff%+lfms)\n", 
                             1000.0*SECONDS((uint64_t)ev->tstamp-
                                            (uint64_t)printrelative));
                      if (debugflag)
                        printf("  tstamp==0x%016llx  relative==0x%016llx\n", 
                               ev->tstamp, printrelative);
                    }
                  sp = search(pc);
                  printf("    %s+0x%llx (0x%llx)\n", 
                         (sp ? sp->name : ""), pc - sp->addr, pc);
                  if (flags & PRF_STACKEND)
                    break;
                }
            }
          else if (debugflag)
            printf("other tstamp event: %d\n", ev->evt);
        }
      
      /*
       * Mark entry as available so kernel will reuse it.
       */
      if (ev->jumbocnt != 0) 
        {
          /*
           * Jumbo event, mark extended entries free first.
           * note: profiler events are never jumbo
           */
          int n = ev->jumbocnt;
          int ix = INDEXADD(our_index, n);
          
          do 
            {
              events[ix].jumbocnt = 0;
              events[ix].evt = 0;
              if (--ix < 0)
                ix += num_events;
            } 
          while (--n);
          our_index = INDEXADD(our_index, 1+ev->jumbocnt);
          our_count += 1+ev->jumbocnt;
          ev->jumbocnt = 0;
        } 
      else 
        {
          our_index = INDEXADD(our_index, 1);
          our_count++;
        }
      ev->evt = 0;
    }
  
  if (state->lost_counter > 0)
    {
      /* see lost_counter comment above */
      our_index = state->curr_index;
      our_count = their_count;
      if (debugflag)
        printf("scan_stacks drop: revised count=%d idx=%d\n",
               their_count, our_index);
    }
  
  if (debugflag)
    printf("scan_stacks processed %d\n", our_count);

  if (our_count > 0)
    {
      /*
       * tell kernel we ate our_count entries
       */
      OC(syssgi(SGI_RT_TSTAMP_UPDATE, cpu, our_count));
      /*
       * now state->tstamp_counter counts things we haven't yet seen
       */
    }
}


void stop_stacks(void)
{

  OC(syssgi(SGI_RT_TSTAMP_DELETE, cpu));
  
  /* possibly restore rtmon !! XXX */
}

/* holdoff --------------------------------------------------------- */

typedef struct holdoff
{
  iotimer64_t time;
  iotimer64_t dur;
} holdoff;

int holdoffcompare(const void *a1, const void *a2)
{
  return ((holdoff *)a1)->dur - ((holdoff *)a2)->dur;
}

#define MAXHOLDOFFS 1024
#define TIME 1
#define DUR 2

holdoff holdoffs[MAXHOLDOFFS];
int nholdoffs;
int overflow;

/* main --------------------------------------------------------- */

char *usage =
    "usage: latenscope [ -rscplquHDSjd ]\n"
    "    -r# report-interval(ms)\n"
    "    -s# sleep-interval(ms)\n"
    "    -c# cutoff-interval(ms)\n"
    "    -p# processor\n"
    "    -l# priority level\n"
    "    -v  verbose mode\n"
    "    -u <filename> unix image\n";

void usage_exit(void)
{
  fprintf(stderr, "%s", usage);
  exit(EXIT_FAILURE);
}

void
main(int argc, char **argv)
{
  char *kernname = "/unix";
  iotimer64_t cutoff, reportinterval, sleepinterval;
  iotimer64_t report_last_displayed, report_cur_time;
  iotimer64_t holdoff_start, holdoff_end, holdoff_delta;
  struct timespec ts;
  int hardspin;
  int c;
  int sort;
  int runs;
  int printaverage;
  int justspin;
  int spewflag;
  int quiet;
  int priority_level;
  struct sched_param param;
  int lock_memory = 1;

  priority_level = sched_get_priority_max(SCHED_FIFO);

  init_seconds_per_cycle();

  /*
   * reportinterval must be at least once per cyclecounter wrap/2
   * (typically around 30 secs for 32-bit counters)
   * XXX make sure that other 32-bit counters aren't hugely less.
   *
   * reportinterval must be little enough that the buffering required
   * as the second argument to init_stacks() does not exceed 
   * tstamp's wimpy 32k buffering limit. 0.9 secs is about the limit.
   */
  reportinterval = CYCLES(0.9);
  hardspin = 0;
  cutoff = CYCLES(.003);
  sort = TIME;
  sleepinterval = CYCLES(.001);
  printaverage = 0;
  debugflag = 0;
  spewflag = 0;
  justspin = 0;
  quiet = 1;

  setbuf(stdout, NULL);

  while ((c = getopt(argc, argv, "Hr:c:C:p:s:dAk:u:DjSvl:m")) != EOF)
    {
      switch(c) 
        {
        case 'H':
          hardspin = 1;
          break;
        case 'r':
          reportinterval = CYCLES(atof(optarg)/1000.0);
          break;
        case 'c':
          cutoff = CYCLES(atof(optarg)/1000.0);
          break;
	case 'C':
	case 'p':
	  cpu = atoi(optarg);
	  break;
        case 's':
          sleepinterval = CYCLES(atof(optarg)/1000.0);
          break;
        case 'd':
          sort = DUR;
          break;
        case 'A':
          printaverage = 1;
          break;
        case 'k':
        case 'u':
          kernname = optarg;
          break;
        case 'D':
          debugflag = 1;
          break;
        case 'S':
          spewflag = 1;
          break;
        case 'j':
          justspin = 1;
          break;
        case 'v':
          quiet = 0;
          break;
	case 'l':
	  priority_level = atoi(optarg);
	  break;
	case 'm':
	  lock_memory = 0;
	  break;
        default:
          usage_exit();
        }
    }

  if (sort == DUR)
    justspin = 1;

  /* ===== setup holdoff detection */

  printf("Latenscope Configuration\n");
  printf("========================\n");
  printf("target cpu: %d\n", cpu);
  printf("schedule: FIFO at priority %d\n", priority_level);
  if (hardspin)
    printf("sleep interval: hard spin\n");
  else
    printf("sleep interval: %lfms\n", 1000.0*SECONDS(sleepinterval));
  printf("latency cutoff: %lfms\n", 1000.0*SECONDS(cutoff));
  printf("report interval: %lfms\n", 1000.0*SECONDS(reportinterval));
  printf("sorting results by: %s\n", 
         ((sort==TIME) ? "start time" : "duration"));
  printf("status mode: %s\n", quiet ? "quiet" : "verbose");
  if (!justspin)
    printf("showing kernel stacks at millisecond intervals during holdoffs\n");
  fflush(stdout);
  {
    double sleepsecs = SECONDS(sleepinterval);
    ts.tv_sec = (time_t)sleepsecs;
    ts.tv_nsec = (long)(1.E9 * (sleepsecs - ts.tv_sec));
    if (debugflag)
      printf("sleep tv_sec=%d tv_nsec=%d\n",
             ts.tv_sec, ts.tv_nsec);
  }

  printf("%lld\n", cutoff);
  
  /* ===== start collecton */

  if (!justspin)
    {
      init_namelist(kernname);
      init_stacks(kernname, SECONDS(reportinterval));
    }

  /* ===== set scheduling attributes */
  
  if (sysmp(MP_MUSTRUN, cpu) == -1) {
	  perror("sysmp");
	  exit(1);
  }

  param.sched_priority = priority_level;
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
	  perror("sched_setscheduler");
	  exit(1);
  }
  
  if (lock_memory) {
	  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
		  perror("mlockall");
		  exit(1);
	  }
  }

  /* ===== collect */

  printf("Monitoring cpu(%d) latency...\n", cpu);

  runs = 0;
  nholdoffs = 0;
  overflow = 0;

  for(report_last_displayed = read_cycle_counter();;)
    {
      
      if (!hardspin) {
	/* compute time that a potential holdoff may have started */
	holdoff_start = read_cycle_counter() + sleepinterval;
        if (nanosleep(&ts, NULL) == -1 && errno == EINTR)
	  continue; /* a signal returned nanosleep prematurely */
	holdoff_end = read_cycle_counter();
	report_cur_time = holdoff_end;
      } else
	report_cur_time = read_cycle_counter();

      if (report_cur_time - report_last_displayed >= reportinterval)
        {
          int i;
          
	  if (!quiet) {
		  param.sched_priority = 20;
		  if (sched_setscheduler(0, SCHED_TS, &param) == -1) {
			  perror("sched_setscheduler: TS");
			  exit(1);
		  }
	  }

          if (debugflag)
            printf("tracebuf count=%d, idx=%d, ouridx=%d, lost=%d\n",
                   state->tstamp_counter,
                   state->curr_index,
                   our_index,
                   state->lost_counter);
          
          if (sort == DUR)
            {
              assert(justspin);
              qsort(holdoffs, nholdoffs, sizeof(holdoffs[0]),
                    holdoffcompare);
            }

          for(i=0; i < nholdoffs; i++)
            {
              printf("%03d: %10.2lfms holdoff at %20.2lfms\n", i,
                     1000.0*SECONDS(holdoffs[i].dur),
                     1000.0*SECONDS(holdoffs[i].time));
              if (!justspin)
                {
                  scan_stacks(holdoffs[i].time, spewflag, 0);
                  scan_stacks(holdoffs[i].time + holdoffs[i].dur, 1,
                              holdoffs[i].time);
                }
                          
            }
          if (overflow)
            error_exit("ERROR: so many holdoffs we overflowed!"
                       " results incomplete.");
          
          if (!justspin)
            scan_stacks(report_cur_time, spewflag, 0);

          if (!quiet || nholdoffs > 0)
            {
              printf("%d runs %d holdoffs", runs, nholdoffs);
              if (printaverage)
                printf(" (run on average every %.2lfms)",
                       1000.0*SECONDS(report_cur_time-report_last_displayed)/
		       (double)runs);
              printf("\n");
            }

          if (hardspin)
            { printf("exiting...\n"); fflush(stdout); sleep(1); break; }
          
	  if (!quiet) {
		  param.sched_priority = priority_level;
		  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
			  perror("sched_setscheduler");
			  exit(1);
		  }
	  }

          runs = 0;
          nholdoffs = 0;
          overflow = 0;
          report_last_displayed = read_cycle_counter();
        }

      holdoff_delta = holdoff_end - holdoff_start;

      if (holdoff_delta >= cutoff)
        {

          if (nholdoffs == MAXHOLDOFFS)
            overflow = 1;
          else
            {
              holdoffs[nholdoffs].time = holdoff_start;
              holdoffs[nholdoffs].dur = holdoff_delta;
              nholdoffs++;
            }
        }
      
      runs++;
    }
  
  /* ===== stop collecton */

  if (!justspin)
    stop_stacks();

  exit(0);
}




