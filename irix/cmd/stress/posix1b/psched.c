/*
 * File: psched.c
 *
 * Function: psched exercises posix scheduling functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>

main(int argc, char *argv[])
{
  struct sched_param sparams;
  int sched_val;
  struct timespec start_stamp;

  printf("\t\tPosix Scheduler Intefaces\n\n");
  printf("psched: %-38s = %d\n","sched_get_priority_max(SCHED_FIFO)",
				sched_get_priority_max(SCHED_FIFO));
  printf("psched: %-38s = %d\n","sched_get_priority_min(SCHED_FIFO)",
				sched_get_priority_min(SCHED_FIFO));
  printf("psched: %-38s = %d\n","sched_get_priority_max(SCHED_RR)",
				sched_get_priority_max(SCHED_RR));
  printf("psched: %-38s = %d\n","sched_get_priority_min(SCHED_RR)",
				sched_get_priority_min(SCHED_RR));
  printf("psched: %-38s = %d\n","sched_get_priority_max(SCHED_OTHER)",
				sched_get_priority_max(SCHED_OTHER));
  printf("psched: %-38s = %d\n","sched_get_priority_min(SCHED_OTHER)",
				sched_get_priority_min(SCHED_OTHER));

  /*
   * SCHED_RR
   */
  sparams.sched_priority = sched_get_priority_max(SCHED_RR);
  if (sched_setscheduler(0, SCHED_RR, &sparams) < 0) {
	  if (errno == EPERM) {
		  printf("You must be root to run this test\n");
		  exit(0);
	  }
	  perror("psched: ERROR - sched_setscheduler");
	  exit(1);
  }
  printf("\npsched: %-33s%d%s   = %d\n",
			"sched_setscheduler(0, SCHED_RR, ",
			sparams.sched_priority,")",0);
  if ((sched_val = sched_getscheduler(0))  < 0) {
	  perror("psched: ERROR - sched_getscheduler");
	  exit(1);
  }
  if (sched_val != SCHED_RR) {
      perror("psched: ERROR - sched_getscheduler != SCHED_RR");
    exit(1);
  }
  printf("psched: %-39s = SCHED_RR\n","sched_getscheduler(0)");
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  sparams.sched_priority = sched_get_priority_min(SCHED_RR);
  if (sched_setparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_setparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_setparam: sched_priority",
				sparams.sched_priority);
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  if (sched_rr_get_interval(0, &start_stamp)  < 0) {
      perror("psched: ERROR - sched_rr_get_interval");
    exit(1);
  }
  printf("psched: %-39s = %u sec %u nsec\n",
			"sched_rr_get_interval(0)",
			start_stamp.tv_sec, start_stamp.tv_nsec);
  /*
   * SCHED_FIFO
   */
  sparams.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &sparams) < 0) {
      perror("psched: ERROR - sched_setscheduler");
    exit(1);
  }
  printf("\npsched: %-33s%d%s  = %d\n",
			"sched_setscheduler(0, SCHED_FIFO, ",
			sparams.sched_priority,")",0);
  if ((sched_val = sched_getscheduler(0))  < 0) {
      perror("psched: ERROR - sched_getscheduler");
    exit(1);
  }
  if (sched_val != SCHED_FIFO) {
      perror("psched: ERROR - sched_getscheduler != SCHED_FIFO");
    exit(1);
  }
  printf("psched: %-39s = SCHED_FIFO\n","sched_getscheduler(0)");
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  sparams.sched_priority = sched_get_priority_min(SCHED_FIFO);
  if (sched_setparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_setparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_setparam: sched_priority",
				sparams.sched_priority);
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  /*
   * SCHED_OTHER
   */
  sparams.sched_priority = sched_get_priority_max(SCHED_OTHER);
  if (sched_setscheduler(0, SCHED_OTHER, &sparams) < 0) {
      perror("psched: ERROR - sched_setscheduler");
    exit(1);
  }
  printf("\npsched: %-33s%d%s = %d\n",
			"sched_setscheduler(0, SCHED_OTHER, ",
			sparams.sched_priority,")",0);
  if ((sched_val = sched_getscheduler(0))  < 0) {
      perror("psched: ERROR - sched_getscheduler");
    exit(1);
  }
  if (sched_val != SCHED_OTHER) {
      perror("psched: ERROR - sched_getscheduler != SCHED_OTHER");
    exit(1);
  }
  printf("psched: %-39s = SCHED_OTHER\n","sched_getscheduler(0)");
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  sparams.sched_priority = sched_get_priority_min(SCHED_OTHER);
  if (sched_setparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_setparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_setparam: sched_priority",
				sparams.sched_priority);
  if (sched_getparam(0, &sparams) < 0) {
      perror("psched: ERROR - sched_getparam");
    exit(1);
  }
  printf("psched: %-39s = %d\n",
			"sched_getparam: sched_priority",
				sparams.sched_priority);
  return(0);
}
