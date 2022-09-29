/*
 * shmem.h - shared memory between processes
 *
 */

#ifndef SHMEM_H
#define SHMEM_H


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <abi_mutex.h>

#define set_mask(i)	(1 << i)
#define clear_mask(i)	(~(1 << i))

#define start_work(id)	(test_then_or32(&(shmem->working), set_mask(id)))
#define end_work(id)	(test_then_and32(&(shmem->working), clear_mask(id)))

/* different types of test */
#define DONE 0
#define ABIM 1
#define TSET 2
#define TLOG 3
#define TNLG 4
#define TADD 5
#define TSET32 6
#define TLOG32 7
#define TNLG32 8
#define TADD32 9

struct shmem_st {
	volatile __uint32_t	num_iters;	/* number of iterations to */
						/* do tests on             */

	__uint32_t		working;	/* on bit indicates that   */
						/* that process is working */

	volatile __uint32_t	test;		/* indicates which test    */

	volatile __uint32_t	crit_var_32;	/* critical variables that */
	volatile __uint64_t	crit_var_64;	/* will need exclusive     */
					 	/* access during the tests */

	__uint32_t	shared32;	/* shared vars for mutex.h */
	__uint64_t	shared64;	/* routines (test_and_set) */

	abilock_t		abilock;	/* shared var for          */
						/* abi_mutex.h routines    */
};

extern struct shmem_st *shmem;
extern int shmid;

/* create a shared memory space based on key */
int createShm(int key);

/* initialize return addr to space of shmem */
struct shmem_st *initializeShm(int shmid);

/* close addr to space of shmem */
void closeShm(struct shmem_st *shmem);

/* remove shared memory space of shmid */
void destroyShm(int shmid);


#endif


