/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#include <Tst.h>

/* ------------------------------------------------------------------ */

void *
system_test(void *arg)
{
	int	ret = system("ls / > /dev/null");

	ChkExp( ret == 0, ("unexpected return from system() %d [%d]\n",
		ret, 0) );
	return (0);
}


TST_BEGIN(systemTest)
{
	int		p;
	pthread_t	*pts;
	void		*ret;

	void		*system_test(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, system_test, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise threaded C library" )

	TST( systemTest, "Verify system() commands",
		"Create threads which invoke system with some command"
		"verify return status is as expected" ),

TST_FINISH

