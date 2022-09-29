/*
 * File: mq_bench.c
 *
 * Function: mq_bench is a benchmark for measuring the performance
 *           of posix.1b and System Five message queues.
 *
 *           The benchmark measures producer/consumer type message
 *           passing contention, or contention free single process
 *           send and receive operations.
 *
 * Arguments:
 *           -s     Use System Five message queues
 *           -p     Use Posix.1b message queues
 *           -n     No contention test (single process)
 *           -m #   Specify message size (in bytes)
 *           -q #   Specify max number of messages a queue can store
 *           -c #   Specify the number of loop cycles
 *           -i #   Specify the number of consecutive operations
 *
 * Default:
 *          Posix.1b contention test (Qsize=100, MsgSize = 32)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <sys/msg.h>
#include <errno.h>
#include <sched.h>

#define PRIORITY 198

#define MQ_NAME "/usr/tmp/mq_bench.mq"
#define SEM_NAME "/usr/tmp/mq_bench.sem"

#define TEST_TYPE_POSIX 1
#define TEST_TYPE_SV    2

#define TRUE 1
#define FALSE 0

int test_type = TEST_TYPE_POSIX;

int queue_size = 100;
int msg_size = 32;
int iterations = 1;
int cycles = 50000;

int no_contention = FALSE;

sem_t *sem;
mqd_t mq;
int mq_sv;

void child_posix(void);
void child_sv(void);
void cleanup_n_exit(int);

main(int argc, char *argv[])
{
  int loop, inner_loop, ops;
  int parse;
  int pid;
  char *msg_buffer = NULL;
  struct  sched_param param;

  unsigned int total_time;
  unsigned int xfer_rate;

  struct timespec start_stamp, end_stamp;
  struct mq_attr mq_attr;
  struct msqid_ds sv_attr;

  while ((parse = getopt(argc, argv, "c:m:q:spvi:n")) != EOF)
    switch (parse) {
    case 'c':
      cycles = atoi(optarg);
      break;
    case 's':
      test_type = TEST_TYPE_SV;
      break;
    case 'p':
      test_type = TEST_TYPE_POSIX;
      break;
    case 'm':
      msg_size = atoi(optarg);
      break;
    case 'q':
      queue_size = atoi(optarg);
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    case 'n':
      no_contention = TRUE;
      break;
    default:
      exit(1);
    }


  setbuf(stdout, NULL);

  if (no_contention && (iterations > queue_size)) {
    printf("Deadlock Warning:\n");
    printf("In a no contention test, the operation iterations\n");
    printf("cannot be greater than the queue size.\n");
    exit(1);
  }

  if ((no_contention == FALSE) && (iterations != 1)) {
    printf("Warning:\n");
    printf("Iteration values are valid for non-contention tests only\n");
    iterations = 1;
  }

  /* init semaphore */
  sem = sem_open(SEM_NAME, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU, 0);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(1);
  }

  if (sem_unlink(SEM_NAME) != 0) {
    perror("sem_unlink");
    exit(1);
  }

  /* init message buffer */
  if ((msg_buffer = (char *) malloc(msg_size)) == NULL) {
    perror("malloc buff");
    exit(1);
  }

  /* int real-time policy */
  param.sched_priority = PRIORITY;
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    if (errno == EPERM)
      printf("You must be root to run this benchmark\n");
    else
      perror("sched_setscheduler");              
    exit(0);                                           
  }

  /* lock down memory */
  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
    perror("mlockall");
    exit(1);
  }

  /* init message queues */
  if (test_type == TEST_TYPE_POSIX) {

    /* Posix Test */
    mq_attr.mq_maxmsg = queue_size;
    mq_attr.mq_msgsize = msg_size;
    mq_attr.mq_flags = 0;

    mq = mq_open(MQ_NAME, O_CREAT|O_RDWR, S_IRWXU, &mq_attr);
    if (mq == (mqd_t) -1) {
      perror("mq_open");
      exit(1);
    }

    if (mq_unlink(MQ_NAME) != 0) {
      perror("mq_unlink");
      exit(1);
    }

    printf("\nPosix Message Transfer Test\n");
    printf("==========================================================\n");

    if (no_contention == FALSE) {
      /* fork child */
      if ((pid = fork()) == -1) {
	perror("fork");
	exit(1);
      }

      if (pid == 0) {
	child_posix();
	exit(0);
      }
    }

  } else {

    /* SV message queue test */
    if ((mq_sv = msgget(IPC_PRIVATE, 0660)) < 0) {
      perror("msgget");
      exit(1);
    }

    /* adjust q size */
    sv_attr.msg_qbytes = queue_size * msg_size;
    if (msgctl(mq_sv, IPC_SET, &sv_attr) < 0) {
	perror("msgctl");
	cleanup_n_exit(1);
    }

    printf("\nSV Message Transfer Test\n");
    printf("==========================================================\n");

    if (no_contention == FALSE) {
      /* fork child */
      if ((pid = fork()) == -1) {
	perror("fork");
	cleanup_n_exit(1);
      }

      if (pid == 0) {
	child_sv();
	exit(0);
      }
    }
  }

  ops = cycles * iterations * 2;

  if (no_contention)
    printf("Test type    = no contention test\n");
  else
    printf("Test type    = 2 process contention test\n");

  printf("Queue size   = %d\t(max number of queued messages)\n", queue_size);
  printf("Message size = %d\t(byte size of each message)\n", msg_size);
  printf("Cycles       = %d\t(per-process test loop cycle)\n", cycles);
  printf("Iterations   = %d\t(consecutive send and consecutive rcv ops)\n",
	 iterations);
  printf("Operations   = %d\t(total number of mq operations)\n", ops);
  printf("\nTransferring...\n");

  loop = cycles;

  if (no_contention == FALSE) {
    /* Wait for child to finish setting up */
    if (sem_wait(sem) != 0) {
      perror("sem_wait");
      cleanup_n_exit(1);
    }
  }

  if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
     perror("clock_gettime");
     cleanup_n_exit(1);
  }

  if (test_type == TEST_TYPE_POSIX) {

    if (no_contention) {
      /* 
       * POSIX No Contention Test
       */
      while (loop-- > 0) {
	for (inner_loop=0; inner_loop < iterations; inner_loop++) {
	  if (mq_send(mq, msg_buffer, msg_size, 1) != 0) {
	    perror("child mq_send");
	    cleanup_n_exit(1);
	  }
	}

	for (inner_loop=0; inner_loop < iterations; inner_loop++) {
	  if (mq_receive(mq, msg_buffer, msg_size, NULL) < 0) {
	    perror("parent mq_receive");
	    cleanup_n_exit(1);
	  }
	}
      }
    } else {
      /* 
       * POSIX Contention test
       */
      while (loop-- > 0) {
	  if (mq_receive(mq, msg_buffer, msg_size, NULL) < 0) {
	    perror("parent mq_receive");
	    cleanup_n_exit(1);
	  }
      }
    }

  } else {

    *msg_buffer = 1;

    if (no_contention) {
      /*
       * SV No Contention Test
       */
      while (loop-- > 0) {
	for (inner_loop=0; inner_loop < iterations; inner_loop++) {
	  if (msgsnd(mq_sv, msg_buffer, msg_size, 0) < 0) {
	    perror("parent msgsnd");
	    cleanup_n_exit(1);
	  }
	}

	for (inner_loop=0; inner_loop < iterations; inner_loop++) {
	  if (msgrcv(mq_sv, msg_buffer, msg_size, 0, 0) != msg_size) {
	    perror("parent msgrcv");
	    cleanup_n_exit(1);
	  }
	}
      }

    } else {
      /*
       * SV Contention Test
       */
      while (loop-- > 0) {
	  if (msgrcv(mq_sv, msg_buffer, msg_size, 0, 0) != msg_size) {
	    perror("parent msgrcv");
	    cleanup_n_exit(1);
	  }
      }
    }
  }

  if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
     perror("clock_gettime");
     cleanup_n_exit(1);
  }  

  end_stamp.tv_sec -= start_stamp.tv_sec;
  if (end_stamp.tv_nsec >= start_stamp.tv_nsec) {
        end_stamp.tv_nsec -= start_stamp.tv_nsec;
  } else {
    if (end_stamp.tv_sec > 0)
      end_stamp.tv_sec--;
    end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec) + 1000000000;
  }

  total_time = (float)(end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000);

  printf("\n%d message operations took %d.%d seconds:\n",
         ops, end_stamp.tv_sec, end_stamp.tv_nsec/1000);

  printf("\tOperation Time = %5.2f uS/operation\n", (float) total_time/ops);

  if (total_time/1000000) {
    xfer_rate = (msg_size * (ops/2)) / (total_time/1000000);
    printf("\tTransfer Rate  = %d bytes/second\n", xfer_rate);
  } else 
    printf("\tTransfer Rate  = NA (sample to small)\n");

  cleanup_n_exit(0);
  /* NOTREACHED */
}

void
child_posix(void)
{
  int loop, inner_loop;
  char *msg_buffer = NULL;

  loop = cycles;
  inner_loop = iterations;

  setbuf(stdout, NULL);

  /* lock down memory */
  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
    perror("child mlockall");
    exit(1);
  }

  /* init message buffers */
  if ((msg_buffer = (char *) malloc(msg_size)) == NULL) {
    perror("malloc child buff");
    exit(1);
  }

  /* wake-up parent to start test */
  if (sem_post(sem) != 0) {
    perror("sem_post 1");
    exit(1);
  }

  while(loop-- > 0) {
    for (inner_loop=0; inner_loop < iterations; inner_loop++) {
	if (mq_send(mq, msg_buffer, msg_size, 1) != 0) {
	  perror("child mq_send");
	  cleanup_n_exit(1);
	}
    }
  }

  mq_close(mq);

  exit(0);
}

void
child_sv(void)
{
  int loop, inner_loop;
  char *msg_buffer = NULL;

  loop = cycles;
  inner_loop = iterations;

  setbuf(stdout, NULL);

  /* lock down memory */
  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
    perror("child mlockall");
    exit(1);
  }

  /* init message buffer */
  if ((msg_buffer = (char *) malloc(msg_size)) == NULL) {
    perror("malloc buff1");
    exit(1);
  }

  /* wake-up parent to start test */
  if (sem_post(sem) != 0) {
    perror("sem_post 1");
    exit(1);
  }

  *msg_buffer = 2;

  while(loop-- > 0) {
    for (inner_loop=0; inner_loop < iterations; inner_loop++) {
      if (msgsnd(mq_sv, msg_buffer, msg_size, 0) < 0) {
	perror("child msgsnd");
	exit(1);
      }
    }
  }

  exit(0);
}

void
cleanup_n_exit(int x)
{  
  if (test_type == TEST_TYPE_POSIX) {
    if (mq_close(mq) < 0)
      perror("mq_close");
  } else {
    if (msgctl(mq_sv, IPC_RMID, 0) < 0)
      perror("msgctl IPC_RMID");
  }

  if (sem_close(sem) < 0)
    perror("sem_close");

  exit(x);
}
