/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

void *
init_destroy(void *arg)
{
	int		i;
	pthread_cond_t	cv0 = PTHREAD_COND_INITIALIZER;
	pthread_cond_t	cv1;

	for (i = 0; i < TST_TRIAL_LOOP; i++) {
		ChkInt( pthread_cond_destroy(&cv0), == 0 );
		ChkInt( pthread_cond_init(&cv0, 0), == 0 );
		ChkInt( pthread_cond_init(&cv1, 0), == 0 );
		ChkInt( pthread_cond_destroy(&cv1), == 0 );
	}
	ChkInt( pthread_cond_destroy(&cv0), == 0 );

	return (0);
}


TST_BEGIN(initDestroy)
{
	int		p;
	pthread_t	*pts;
	void		*ret;

	void		*init_destroy(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, init_destroy, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "cv testing" )

	TST( initDestroy, "Test create destroy paths", 0 ),

TST_FINISH

