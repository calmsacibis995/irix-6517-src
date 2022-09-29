/* 
 * thread-library independent code for the sthr library 
 */

#include "trc.h"
#include <assert.h>
#include <mutex.h>
#include <unistd.h>
#include <time.h>
#include "sthr.h"

void sthr_yield(void)
{
	sginap(0);
}


/*
 * Barriers
 * 
 * Barriers spin for a while, then do exponential backoff, until they see that
 * the barrier count has fallen low enough. This doesn't work as quickly as
 * you'd like for large numbers of threads.
 */

int sthr_barrier_init(sthr_barrier_t* b, ulong_t count)
{
	b->b_count = count;
	return 0;
}

#define NSPINS 1000
void sthr_barrier_join(sthr_barrier_t* b)
{
	int ii;
	register nretries = 0;
	volatile ulong_t* bp = &b->b_count;
	struct timespec nano;

	trc_vrb("barrier_join: count == %d\n", b->b_count);
	if (add_then_test(&b->b_count, -1UL))
		while (*bp) {
			if (! (++ii % NSPINS)) {
				nretries++;
				nano.tv_nsec = 200000 * (1 << (nretries % 13));
				nano.tv_sec = nretries / 13;
				nanosleep(&nano, 0);
			}
		}

	assert(b->b_count == 0);
}

void sthr_barrier_destroy(sthr_barrier_t* b)
{ }

