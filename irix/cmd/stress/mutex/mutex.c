/*
 * mutex.c - in cooperation with master.c and other mutex.c
 *           programs, this will test the routines in
 *           mutex.h to verify that operations are atomic;
 *           assumes that functions work properly.
 */

#if ((_MIPS_SZLONG != 32) && (_MIPS_SZLONG != 64))
	THIS TEST NEEDS TO BE MODIFIED TO HANDLE OTHER SIZED LONGS;;;;;;;
#endif

#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>
#include <mutex.h>
#include <abi_mutex.h>
#include "shmem.h"

static void Wait(void);

static void AbiM(void);

static void TSet(void);
static void TLog(void);
static void TNLg(void);
static void TAdd(void);

static void TSet32(void);
static void TLog32(void);
static void TNLg32(void);
static void TAdd32(void);

static int id;

main(int argc, char *argv[])
{
	id = atoi(argv[1]);
	shmid = atoi(argv[2]);

	shmem = initializeShm(shmid);
	end_work(id);
	Wait();

	while (shmem->test != DONE) {
		switch (shmem->test) {
			case ABIM: AbiM(); break;
			case TSET: TSet(); break;
			case TLOG: TLog(); break;
			case TNLG: TNLg(); break;
			case TADD: TAdd(); break;
			case TSET32: TSet32(); break;
			case TLOG32: TLog32(); break;
			case TNLG32: TNLg32(); break;
			case TADD32: TAdd32(); break;
		}
		end_work(id);
		Wait();
	}

	return 0;
}

static void
Wait(void)
{
	while ((shmem->working & set_mask(id)) == 0)
		sginap(1);
}

static void
AbiM()
{
	int i;
	__uint32_t foo;

	for (i = 0; i < shmem->num_iters; i++) {
		while (acquire_lock(&(shmem->abilock)))
			;

		shmem->crit_var_32 = 0;
		foo = 0;

		while (foo < 10000) {
			if (foo != shmem->crit_var_32)
				fprintf(stderr, "%s %d %s %s %d %s %d\n",
					"Error in AbiM: fork", id,
					"does not have exclusive access:",
					"foo = ", foo, "; crit_var_32 = ",
					shmem->crit_var_32);

			foo++;
			shmem->crit_var_32++;
			if (stat_lock(&(shmem->abilock)) == 0)
				perror("stat_lock");
		}

		if (release_lock(&(shmem->abilock)))
			perror("release_lock");

		for (foo = 0; foo < 10000; foo++)
			;
	}

}

/* critical section implemented using test_and_set */
static void
TSet()
{
	int i;
	unsigned long foo;

	for (i = 0; i < shmem->num_iters; i++) {

#if _MIPS_SZLONG == 32
		while (test_and_set((unsigned long *) &(shmem->shared32), 1) != 0)
			;	/* spin-lock wait */

		shmem->crit_var_32 = 0;
		foo = 0;

		while (foo < 10000) {
			if (foo != shmem->crit_var_32)

				fprintf(stderr, "%s %d %s %s %d %s %ld\n",
					"Error in TSet (32-bit): fork", id,
					"does not have exclusive access:",
					"foo = ", foo, "; crit_var_32 = ",
					shmem->crit_var_32);
			foo++;
			shmem->crit_var_32++;
		}

		test_and_set((unsigned long *) &(shmem->shared32), 0);

#else

		while (test_and_set((unsigned long *) &(shmem->shared64), 1) != 0)
			;	/* spin-lock wait */

		shmem->crit_var_64 = 0;
		foo = 0;

		while (foo < 10000) {
			if (foo != shmem->crit_var_64)

				fprintf(stderr, "%s %d %s %s %d %s %ld\n",
					"Error in TSet (64-bit): fork", id,
					"does not have exclusive access:",
					"foo = ", foo, "; crit_var_64 = ",
					shmem->crit_var_64);
			foo++;
			shmem->crit_var_64++;
		}

		test_and_set((unsigned long *) &(shmem->shared64), 0);
#endif



		for (foo = 0; foo < 10000; foo++)
			;
	}

}

/* the idea here is to change your bit from a 0 to a 1 to a 0 to a 1 ...
   while not changing anyone else's bit.  If mutex isn't internally
   provided, the 0 to 1 to 0 to 1 ... flow will be broken.
*/
static void
TLog()
{
	int i;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

#if _MIPS_SZLONG == 32

		if ((test_then_or((unsigned long *) &(shmem->shared32),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (32-bit): fork", id,
				"did not get the correct value for",
				"test_then_or");

		if ((test_then_and((unsigned long *) &(shmem->shared32),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (32-bit): fork", id,
				"did not get the correct value for",
				"test_then_and");

		if ((test_then_xor((unsigned long *) &(shmem->shared32),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (32-bit): fork", id,
				"did not get the correct value for",
				"test_then_xor");

		if ((test_then_and((unsigned long *) &(shmem->shared32),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (32-bit): fork", id,
				"did not get the correct value for",
				"test_then_and (#2)");

#else

		if ((test_then_or((unsigned long *) &(shmem->shared64),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (64-bit): fork", id,
				"did not get the correct value for",
				"test_then_or");

		if ((test_then_and((unsigned long *) &(shmem->shared64),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (64-bit): fork", id,
				"did not get the correct value for",
				"test_then_and");

		if ((test_then_xor((unsigned long *) &(shmem->shared64),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (64-bit): fork", id,
				"did not get the correct value for",
				"test_then_xor");

		if ((test_then_and((unsigned long *) &(shmem->shared64),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog (64-bit): fork", id,
				"did not get the correct value for",
				"test_then_and (#2)");

#endif

	}

}

/* invert all the bits and test the number of times everyone gets
   a zero, and the number of times everyone gets all 1's.  They
   should be equal.
*/
static void
TNLg(void)
{
	int i;
	unsigned long oldval;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

#if _MIPS_SZLONG == 32

		oldval = test_then_not((unsigned long *)&(shmem->shared32), 0L);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nand((unsigned long *) &(shmem->shared32),
					~(0L));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nor((unsigned long *)&(shmem->shared32), 0L);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_xor((unsigned long *) &(shmem->shared32),
					~(0L));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));


#else

		oldval = test_then_not((unsigned long *)&(shmem->shared64), 0L);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_64++;
		else
			shmem->crit_var_64--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nand((unsigned long *) &(shmem->shared64),
					~(0L));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_64++;
		else
			shmem->crit_var_64--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nor((unsigned long *)&(shmem->shared64), 0L);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_64++;
		else
			shmem->crit_var_64--;
		release_lock(&(shmem->abilock));

		oldval = test_then_xor((unsigned long *) &(shmem->shared64),
					~(0L));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_64++;
		else
			shmem->crit_var_64--;
		release_lock(&(shmem->abilock));

#endif

	}



}

/* keep adding 12 and check overall value at the end */
static void
TAdd(void)
{
	int i;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

#if _MIPS_SZLONG == 32

		test_then_add((unsigned long *)&(shmem->shared32), 12L);
		add_then_test((unsigned long *)&(shmem->shared32), 12L);

#else

		test_then_add((unsigned long *)&(shmem->shared64), 12L);
		add_then_test((unsigned long *)&(shmem->shared64), 12L);

#endif

	}



}










/* critical section implemented using test_and_set */
static void
TSet32()
{
	int i;
	__uint32_t foo;

	for (i = 0; i < shmem->num_iters; i++) {

		while (test_and_set32(&(shmem->shared32), 1) != 0)
			;	/* spin-lock wait */

		shmem->crit_var_32 = 0;
		foo = 0;

		while (foo < 10000) {
			if (foo != shmem->crit_var_32)

				fprintf(stderr, "%s %d %s %s %d %s %d\n",
					"Error in TSet32: fork", id,
					"does not have exclusive access:",
					"foo = ", foo, "; crit_var_32 = ",
					shmem->crit_var_32);
			foo++;
			shmem->crit_var_32++;
		}

		test_and_set32(&(shmem->shared32), 0);

		for (foo = 0; foo < 10000; foo++)
			;
	}

}

/* the idea here is to change your bit from a 0 to a 1 to a 0 to a 1 ...
   while not changing anyone else's bit.  If mutex isn't internally
   provided, the 0 to 1 to 0 to 1 ... flow will be broken.
*/
static void
TLog32()
{
	int i;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

		if ((test_then_or32(&(shmem->shared32),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog32: fork", id,
				"did not get the correct value for",
				"test_then_or32");

		if ((test_then_and32(&(shmem->shared32),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog32: fork", id,
				"did not get the correct value for",
				"test_then_and32");

		if ((test_then_xor32(&(shmem->shared32),
					set_mask(id)) | clear_mask(id))
					!= clear_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog32: fork", id,
				"did not get the correct value for",
				"test_then_xor32");

		if ((test_then_and32(&(shmem->shared32),
					clear_mask(id)) & set_mask(id))
					!= set_mask(id))
			fprintf(stderr, "%s %d %s %s\n",
				"Error in TLog32: fork", id,
				"did not get the correct value for",
				"test_then_and32 (#2)");
	}

}

/* invert all the bits and test the number of times everyone gets
   a zero, and the number of times everyone gets all 1's.  They
   should be equal.
*/
static void
TNLg32(void)
{
	int i;
	__uint32_t oldval;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

		oldval = test_then_not32(&(shmem->shared32), 0);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nand32(&(shmem->shared32),
					~(0));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_nor32(&(shmem->shared32), 0);

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

		oldval = test_then_xor32(&(shmem->shared32),
					~(0));

		while (acquire_lock(&(shmem->abilock)))
			;
		if (oldval)
			shmem->crit_var_32++;
		else
			shmem->crit_var_32--;
		release_lock(&(shmem->abilock));

	}



}

/* keep adding 12 and check overall value at the end */
static void
TAdd32(void)
{
	int i;

	for (i = 0; i < shmem->num_iters * 1000; i++) {

		test_then_add32(&(shmem->shared32), 12);
		add_then_test32(&(shmem->shared32), 12);

	}

}

