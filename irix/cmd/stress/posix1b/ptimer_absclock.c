#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

#define NUM_ITERATIONS 1000

clockid_t rt_timer;

timespec_t requested [NUM_ITERATIONS];
timespec_t actual    [NUM_ITERATIONS];

timespec_t   period = {0L, 10000000L};  /* 10mSec */
itimerspec_t next_time = {0L, 0L, 0L, 0L};

volatile int iteration = 0;

/* ARGSUSED */
void
handler (int sig, siginfo_t *sip, void *up) {


   if ( clock_gettime (CLOCK_REALTIME, &actual[iteration]) ) {
      perror ("FAILED: clock_gettime");
      exit (-1);
   }

   if (iteration++ == NUM_ITERATIONS) {
      return;
   }

   /* Add period to last requested timeout to determine next timeout */

   next_time.it_value.tv_nsec = 
      requested[iteration-1].tv_nsec + period.tv_nsec;
   next_time.it_value.tv_sec = 
      requested[iteration-1].tv_sec + period.tv_sec;

   if (next_time.it_value.tv_nsec > NSEC_PER_SEC) {
      next_time.it_value.tv_sec++;
      next_time.it_value.tv_nsec -= NSEC_PER_SEC;
   }

   requested[iteration].tv_sec  = next_time.it_value.tv_sec;
   requested[iteration].tv_nsec = next_time.it_value.tv_nsec;

   if (timer_settime (rt_timer, TIMER_ABSTIME, &next_time, NULL)) {
      perror ("FAILED: timer_settime");
      exit (-1);
   }

}


void
main (int argc, char **argv) {

   sigevent_t  evt;
   sigaction_t act;

   int i;
   int failed = 0;

   evt.sigev_notify = SIGEV_SIGNAL;
   evt.sigev_notifyinfo.nisigno = SIGRTMIN;
   evt.sigev_value.sival_int = 0;

   if (timer_create (CLOCK_REALTIME, &evt, &rt_timer)) {
      perror ("FAILED: timer_create");
      exit (-1);
   }

   act.sa_flags = SA_RESTART;
   act.sa_handler = 0;
   (void) sigemptyset (&act.sa_mask);
   act.sa_sigaction = handler;

   if (sigaction (SIGRTMIN, &act, NULL)) {
      perror ("FAILED: sigaction");
      exit (-1);
   }

   if (clock_gettime (CLOCK_REALTIME, &next_time.it_value) ) {
      perror ("FAILED: clock_gettime");
      exit (-1);
   }

   next_time.it_value.tv_nsec += period.tv_nsec;
   next_time.it_value.tv_sec  += period.tv_sec;

   if (next_time.it_value.tv_nsec > NSEC_PER_SEC) {
      next_time.it_value.tv_sec++;
      next_time.it_value.tv_nsec -= NSEC_PER_SEC;
   }

   requested[iteration].tv_sec  = next_time.it_value.tv_sec;
   requested[iteration].tv_nsec = next_time.it_value.tv_nsec;

   if (timer_settime (rt_timer, TIMER_ABSTIME, &next_time, NULL)) {
      perror ("FAILED: timer_settime");
      exit (-1);
   }

   printf("ptimer_absclock: testing TIMER_ABSTIME...\n");

   while (iteration < NUM_ITERATIONS) {
      sleep (1);
   }

   for (i=0; i<NUM_ITERATIONS; i++) {

      if ( (actual[i].tv_sec < requested[i].tv_sec) ||
             (actual[i].tv_sec == requested[i].tv_sec &&
              actual[i].tv_nsec < requested[i].tv_nsec) ) {

	 printf ("FAILED: iteration %d: Requested = %d.%06ld Actual = %d.%06ld\n", i,
	    requested[i].tv_sec, requested[i].tv_nsec/1000L,
	    actual[i].tv_sec,    actual[i].tv_nsec/1000L);

	 failed = 1;
      }
   }

   if (!failed)
     printf("ptimer_absclock: PASSED\n");
   else
     printf("WARNING: make sure timed is not running on this system\n");

   exit(0);
}
