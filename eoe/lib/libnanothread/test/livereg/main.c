/* #include <sys/syssgi.h> */
#define _KMEMUSER
#include <sys/types.h>
#include <sys/prctl.h>
#undef _KMEMUSER
#include <nanothread.h>
#include <assert.h>

extern void single_context(kusharena_t *kusp);

void thread_start(void)
{
   single_context(kusp);
}

int
main(int argc, char *argv[])
{
   int parallelism;
   if ((argc != 2) || ((parallelism = atoi(argv[1])) <= 0)) {
      printf("%s: parallelism\n", argv[0]);
      return -1;
   }
   if (set_num_processors(parallelism) == -1) {
      printf("Unable to initalize thread count.\n");
      return -1;
   }
   if (start_threads((nanostartf_t)thread_start, NULL) == -1) {
      printf("Unable to start thread.\n");
      return -1;
   }
   /*   syssgi(SGI_SETVPID, 1); */
   assert(PRDA->t_sys.t_nid == 1);
   single_context(kusp);
}

void
failtest(short nid, int64_t i, int64_t j, int64_t iteration)
{
   if (j < -2) {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) register a2=%d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
   } else if (j == -2) {
      printf("fail_test: rbit set for running nid %d PRDA->t_sys.t_nid %d\n", nid, PRDA->t_sys.t_nid);
   } else if (j == -1) {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) nid ?= a0(%d) a1(%d)\n", iteration, PRDA->t_sys.t_nid, nid, i);
   } else if (j < 32) {
      printf("fail_test iter %d: nid %d difference %d register %d\n", iteration, nid, i-nid, j);
   } else if (j < 64) {
      printf("fail_test iter %d: nid %d difference %d register fp%d\n", iteration, nid, i-nid, j-32);
   } else {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) register a2=%d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
   }
   exit(-1);
}
