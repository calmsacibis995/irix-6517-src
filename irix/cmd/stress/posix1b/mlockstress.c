#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define TESTFILE "/usr/tmp/memory_test.mem"

#define MAPSIZE 20480000 /* 20 meg */
#define BUFSIZE 1000

int memsize = MAPSIZE;
int wait_a_bit = 0;
int verbose = 0;
int iterations = 50;
int cycles = 200;
int fd = NULL;
int pagesize;

void sig_handler(int, int);

int
main (int argc, char *argv[])
{
  char *addr;
  char buf[BUFSIZE];
  int buf_write;
  int parse;
  int page;
  struct timespec time_stamp;
  struct sigaction act;
  int loop;
  int walkway = 0;

  while ((parse = getopt(argc, argv, "m:i:wvc")) != EOF)
    switch (parse) {
    case 'm':
      memsize = atoi(optarg);
      break;
    case 'c':
      cycles = atoi(optarg);
      break;
    case 'w':
      wait_a_bit = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    }

  act.sa_flags = 0;
  act.sa_handler = sig_handler;
  sigemptyset(&act.sa_mask);
  sigaction(SIGSEGV, &act, NULL);

  pagesize = getpagesize();

  while (iterations-- > 0) {

    if (verbose || wait_a_bit)
      printf("mlockall/munlockall cycle test...\n");
    
    for (loop=cycles; loop > 0; loop--) {
      if (munlockall() < 0) {
	perror("munlockall - cycle");
	exit(1);
      }
      if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
	perror("mlockall - cycle");
	exit(1);
      }
    }

    if (wait_a_bit)
      getchar();

    if (verbose || wait_a_bit)
      printf("zero length file test...\n");

    fd = open(TESTFILE, O_RDWR | O_CREAT | O_EXCL);
    if (fd < 0) {
      perror("open");
      exit(1);
    }
    unlink(TESTFILE);

    /*
     * Ahh, nothing like a good zero length file test to try
     * to break things.
     */
    addr = (char *) mmap(NULL, memsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      perror("FAILED - mmap");
      exit(1);
    }

    /* Establish valid mapping by writing to the file */
    buf_write = memsize / BUFSIZE;
    while (buf_write--)
      write(fd, buf, BUFSIZE);

    switch (walkway) {
    case 0:
      /* touch last page */
      if (verbose || wait_a_bit)
	printf("\ttouching last page\n");
      addr[memsize-1] = 1;
      break;
    case 1:
      /* touch all pages - incrementally */
      if (verbose || wait_a_bit)
	printf("\ttouching all pages++\n");
      for (page = 0; page < (memsize/pagesize); page++)
	addr[page*pagesize] = '0';
      break;
    case 2:
      /* touch all pages - decrementally */
      if (verbose || wait_a_bit)
	printf("\ttouching all pages--\n");
      for (page = memsize/pagesize; page > 0; page--)
	addr[page*pagesize-1] = '0';
      break;
    }

    if (wait_a_bit)
      getchar();

    if (munmap(addr, memsize) < 0) {
      perror("munmap");
      exit(1);
    }

    close(fd);

    /*
     * Autogrow test
     */
    
    if (verbose || wait_a_bit)
      printf("autogrow test...\n");

    fd = open(TESTFILE, O_RDWR | O_CREAT | O_EXCL);
    if (fd < 0) {
      perror("open");
      exit(1);
    }
    unlink(TESTFILE);

    addr = (char *) mmap(NULL, memsize, PROT_READ|PROT_WRITE,
			 MAP_SHARED | MAP_AUTOGROW, fd, 0);
    if (addr == MAP_FAILED) {
      perror("FAILED - mmap autogrow");
      exit(1);
    }

    switch (walkway) {
    case 0:
      /* touch last page */
      if (verbose || wait_a_bit)
	printf("\ttouching last page\n");
      addr[memsize-1] = 1;
      break;
    case 1:
      /* touch all pages - incrementally */
      if (verbose || wait_a_bit)
	printf("\ttouching all pages++\n");
      for (page = 0; page < (memsize/pagesize); page++)
	addr[page*pagesize] = '0';
      break;
    case 2:
      /* touch all pages - decrementally */
      if (verbose || wait_a_bit)
	printf("\ttouching all pages--\n");
      for (page = memsize/pagesize; page > 0; page--)
	addr[page*pagesize-1] = '0';
      break;
    }

    if (wait_a_bit)
      getchar();

    close(fd);

    if (munmap(addr, memsize) < 0) {
      perror("munmap 2");
      exit(1);
    }

    /* This is a special device which get mapped into the VAS of
     * the caller.  It doesn't really get locked down, being a
     * device and all, but it's a good test case to make sure we're
     * handling physical device space properly.
     */

    if (verbose || wait_a_bit)
      printf("device map test...\n");

    if ((clock_gettime(CLOCK_SGI_CYCLE, &time_stamp))== -1) {
      perror("clock_gettime");
      exit(1);
    }

    walkway = ++walkway % 3;

    /* REPEAT */
  }
  
  if (munlockall() < 0) {
    perror("munlockall - exit");
    exit(1);
  }

  printf("PASSED - mlockstress\n");
  exit(0);
}

void
sig_handler(int sig, int code)
{
  int try;

  if (sig == SIGSEGV && code == ENOMEM) {

    if ((try = memsize - 10000000) <= 0)
      if ((try = memsize - 4000000) <= 0)
	if ((try = memsize - 1000000) <= 0)
	  try = 1024;

    printf ("FAILED - SIGSEGV unable to lockdown memory\n\n");
    printf ("         System must have %d free bytes of physical memory.\n",
	    memsize);
    printf ("         Use the -m switch to specify a smaller physical memory\n");
    printf ("         test zone (e.g., -m %d).\n", try);
  }
  else
    printf ("FAILED - Signal caught: sig = %d  code = %d\n", sig, code);

  munlockall();
  close(fd);
  exit(1);
}
