/*
 * master.c - fire off and synchronize mutex tests
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex.h>
#include <abi_mutex.h>
#include "shmem.h"

static void WaitForEveryone(void);
static void BeginAll(int test);

int num_procs;

main(int argc, char *argv[])
{
	key_t key = 0xdcbb;
	int id;
	char idbuf[16], shmidbuf[16];


	if (argc > 1)
		num_procs = atoi(argv[1]);
	if ((num_procs < 1) || (num_procs > 32))
		num_procs = 2;

	if (argc > 3)
		key = atoi(argv[3]);

	shmid = createShm(key);
	shmem = initializeShm(shmid);

	if (argc > 2)
		shmem->num_iters = atoi(argv[2]);
	else
		shmem->num_iters = 25;


	shmem->working = 0;

	sprintf(shmidbuf, "%d", shmid);
	for (id = 0; id < num_procs; id++) {
		start_work(id);

		sprintf(idbuf, "%d", id);

		if (fork() == 0) {
			if (id % 2)
				execl("./mutex32", "mutex32", idbuf, shmidbuf, NULL);
			else
				execl("./mutex64", "mutex64", idbuf, shmidbuf, NULL);
		}
	}
	/* let everyone's fork complete */
	WaitForEveryone();




	if (init_lock(&(shmem->abilock)))
		perror("init_lock");
	BeginAll(ABIM);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	*(volatile __uint64_t *) &shmem->shared64 = 0;
	BeginAll(TSET);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	*(volatile __uint64_t *) &shmem->shared64 = 0;
	BeginAll(TLOG);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	*(volatile __uint64_t *) &shmem->shared64 = 0;
	BeginAll(TLOG);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	*(volatile __uint64_t *) &shmem->shared64 = 0;
	shmem->crit_var_32 = 0;
	shmem->crit_var_64 = 0;
	if (init_lock(&(shmem->abilock)))
		perror("init_lock");
	BeginAll(TNLG);
	WaitForEveryone();
	if (shmem->crit_var_32 != 0)
		fprintf(stderr, "TNLg() failed for 32-bit: crit_var_32 = %d\n",
				shmem->crit_var_32);
	if (shmem->crit_var_64 != 0)
		fprintf(stderr, "TNLg() failed for 64-bit: crit_var_64 = %d\n",
				shmem->crit_var_64);

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	*(volatile __uint64_t *) &shmem->shared64 = 0;
	BeginAll(TADD);
	WaitForEveryone();
	if ((volatile __uint32_t) shmem->shared32 !=
		(12 * 2 * 1000 * shmem->num_iters * (num_procs/2)))
		fprintf(stderr, "TAdd() failed for 32-bit: shared32 = %d\n",
				(volatile __uint32_t) shmem->shared32);
	if ((volatile __uint64_t) shmem->shared64 !=
		(12 * 2 * 1000 * shmem->num_iters * ((num_procs + 1)/2)))
		fprintf(stderr, "TAdd() failed for 64-bit: shared64 = %d\n",
				(volatile __uint64_t) shmem->shared64);



	*(volatile __uint32_t *) &shmem->shared32 = 0;
	BeginAll(TSET32);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	BeginAll(TLOG32);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	BeginAll(TLOG32);
	WaitForEveryone();

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	shmem->crit_var_32 = 0;
	if (init_lock(&(shmem->abilock)))
		perror("init_lock");
	BeginAll(TNLG32);
	WaitForEveryone();
	if (shmem->crit_var_32 != 0)
		fprintf(stderr, "TNLg32() failed: crit_var_32 = %d\n",
				shmem->crit_var_32);

	*(volatile __uint32_t *) &shmem->shared32 = 0;
	BeginAll(TADD32);
	WaitForEveryone();
	if ((volatile __uint32_t) shmem->shared32 !=
		(12 * 2 * 1000 * shmem->num_iters * num_procs))
		fprintf(stderr, "TAdd32() failed: shared32 = %d\n",
				(volatile __uint32_t) shmem->shared32);



	BeginAll(DONE);
	destroyShm(shmid);

	return 0;
}

static void
BeginAll(int test)
{
	int id;

	shmem->test = test;

	for (id = 0; id < num_procs; id++)
		start_work(id);
}

static void
WaitForEveryone(void)
{
	while (shmem->working)
		sginap(1);
}

