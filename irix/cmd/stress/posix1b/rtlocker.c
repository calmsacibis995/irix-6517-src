#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <rtlock.h>
#include <sys/schedctl.h>
#include <sgidefs.h>
#include <ulocks.h>
#include <errno.h>
#include "sys/times.h"

rtlock_t rt_lock;
ulock_t  u_lock;

int prio_reserve[] = {{31},{32},{33},{34},{35}};

int prio_index = 0;
int prio_size = sizeof(prio_reserve)/sizeof(int);
rtlock_t prio_lock;

int parent_pid;
clock_t start, stop, ti;
struct tms tm;
char *afile;

int graph_mode = 0;
int verbose = 0;
int prio_test = 0;
int ulock_test = 0;
int children = 2;
int loops = 10000;

int children_alive;

int cr_weight;
int non_cr_weight;

void child();

main(int argc, char *argv[])
{
  int x;
  int parse;
  usptr_t *hdr;
  int spin_attempts = 600;
 
  int cr_load = 5;
  int load_weight = 50;

  while ((parse = getopt(argc, argv, "c:l:upa:w:r:vg")) != EOF)
    switch (parse) {
    case 'c':
      children = atoi(optarg);
      break;
    case 'l':
      loops = atoi(optarg);
      break;
    case 'u':
      ulock_test = 1;
      break;
    case 'p':
      prio_test = 1;
      break;
    case 'a':
      spin_attempts = atoi(optarg);
      break;
    case 'w':
      load_weight = atoi(optarg);
      break;
    case 'r':
      cr_load = atoi(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'g':
      graph_mode = 1;
      break;
    }

  cr_weight = cr_load * load_weight;
  non_cr_weight = (100 - cr_load) * load_weight;

  usconfig(CONF_ARENATYPE, US_SHAREDONLY);
  if (usconfig(CONF_INITUSERS, children+1) < 0) {
    perror("usconfig:");
    exit(1);
  }
  afile = tempnam(NULL, "rtlocker.mem");
  if ((hdr = usinit("/usr/tmp/rtlocker.mem")) == NULL) {
    perror("usinit:");
    exit(1);
  }

  children_alive = children;

  parent_pid = get_rpid();
  rtlock_init(&prio_lock, RTLOCK_DEFAULT_SPINS);

  u_lock = usnewlock(hdr);
  rtlock_init(&rt_lock, spin_attempts);

  schedctl(NDPRI, 0, 30);

  if (ulock_test)
    ussetlock(u_lock);
  else
    rtlock_acquire(&rt_lock);

  for (x=0; x < children; x++) {
    if (sprocsp(child, PR_SALL, 0, NULL, 8192) == -1) {
      perror("sproc failed");
      printf("child %d\n", x);
      exit(1);
    }
  }

  sleep(5);

  start = times(&tm);
  if (ulock_test)
    usunsetlock(u_lock);
  else
    rtlock_release(&rt_lock);

  blockproc(parent_pid);

  ti = (ti*1000)/(clock_t)CLK_TCK;

  if (!graph_mode) {

    if (ulock_test)
      printf("RTLOCKER   (uslock)   %d ms\n", ti);
    else
      printf("RTLOCKER   (rtlock)   %d ms\n", ti);

    printf("=====================================\n");

    if (prio_test)
      printf("scheduling policy  : real-time\n");
    else
      printf("scheduling policy  : timeshare\n");

    printf("children           : %d\n", children);
    printf("lock/unlock cycles : %d\n", loops);
    printf("critical region    : %d%%\n", cr_load);
    printf("non-critical work  : %d%%\n", 100 - cr_load);
    printf("spin attempts      : %u\n", spin_attempts);
    printf("load weight        : %d\n", load_weight);
    printf("\n");
  } else {
    printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", ulock_test, ti, children,
	   cr_load, 100-cr_load, prio_test, loops, spin_attempts);
  }

  exit(1);
}

void
child()
{
  int pid = getpid();
  int my_prio;
  int busy_spin;
  int cycles;

  if (prio_test) {
    if (rtlock_acquire(&prio_lock) < 0) {
      perror("rtlock_aquire: 1");
      exit(2);
    }
    my_prio = prio_reserve[prio_index];
    prio_index = (prio_index+1) % prio_size;
    if (rtlock_release(&prio_lock) < 0) {
      perror("rtlock_release: 1");
      exit(2);
    }

    schedctl(NDPRI, 0, my_prio);

    my_prio = schedctl(GETNDPRI, 0);
    if (verbose)
      printf ("pid=%d prio=%d\n",pid, my_prio);
  }

  cycles = loops; 

  while (cycles > 0) {
    if (ulock_test == 1) {
      if (ussetlock(u_lock) < 0) {
	perror("ussetlock");
	exit(1);
      }
    } else {
      if (rtlock_acquire(&rt_lock) < 0) {
	perror("rtlock_acquire");
	exit(1);
      }
    }

    cycles--;

    for (busy_spin = 0; busy_spin < cr_weight; busy_spin++);

    if (ulock_test == 0) {
      if (rtlock_release(&rt_lock) < 0) {
	perror("rtlock_release");
	exit(1);
      }
    } else {
      if (usunsetlock(u_lock) < 0) {
	perror("usunsetlock");
	exit(1);
      }
    }

    for (busy_spin = 0; busy_spin < non_cr_weight; busy_spin++);
  }

  if (ulock_test == 1)
    ussetlock(u_lock);
  else
    rtlock_acquire(&rt_lock);

  if (--children_alive == 0) {
    stop = times(&tm);
    ti = stop - start;
    unblockproc(parent_pid);
  }

  if (ulock_test == 0)
    rtlock_release(&rt_lock);
  else
    usunsetlock(u_lock);

  exit(0);
}
