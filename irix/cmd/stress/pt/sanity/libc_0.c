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

/* try for ENOENT
 */
void	*
libENOENT(void *arg)
{
	int	i;

	for (i = 0; i < TST_TRIAL_LOOP; i++) {
		errno = 0;
		ChkExp( errno == 0, ("bad errno %d [%d]\n", errno, 0) );

		ChkPtr( fopen("", "r"), == 0 );
		ChkExp( errno == ENOENT,
			("bad errno %d [%d]\n", errno, ENOENT) );
		TstTrace("1");

		TST_SPIN(TST_PAUSE);
		ChkExp( errno == ENOENT,
			("bad errno %d [%d]\n", errno, ENOENT) );
	}
	return (0);
}


/* try for EINVAL
 */
void	*
libEINVAL(void *arg)
{
	int	i;
	char	buf[10];

	for (i = 0; i < TST_TRIAL_LOOP; i++) {
		errno = 0;
		ChkExp( errno == 0, ("bad errno %d [%d]\n", errno, 0) );

		ChkInt( strtol("10", 0, -1), == 0 );
		ChkExp( errno == EINVAL,
			("bad errno %d [%d]\n", errno, EINVAL) );
		TstTrace("2");

		TST_SPIN(TST_PAUSE);
		ChkExp( errno == EINVAL,
			("bad errno %d [%d]\n", errno, EINVAL) );
	}
	return (0);
}


/* try for ESRCH from cerror
 */
void	*
sysESRCH(void *arg)
{
	int	i;
	pid_t	pid = getpid();

	for (i = 0; i < TST_TRIAL_LOOP; i++) {
		errno = 0;
		ChkExp( errno == 0, ("bad errno %d [%d]\n", errno, 0) );

		ChkInt( ptrace(PTRC_RD_I, pid, NULL, 0), == -1 );
		ChkExp( errno == ESRCH,
			("bad errno %d [%d]\n", errno, ESRCH) );
		TstTrace("3");

		TST_SPIN(TST_PAUSE);
		ChkExp( errno == ESRCH, ("bad errno %d [%d]\n", errno, ESRCH) );
	}
	return (0);
}


/* try for EBADF from cerror
 */
void	*
sysEBADF(void *arg)
{
	int	i;
	char	buf[10];

	for (i = 0; i < TST_TRIAL_LOOP; i++) {
		errno = 0;
		ChkExp( errno == 0, ("bad errno %d [%d]\n", errno, 0) );

		ChkInt( write(-1, buf, 1), == -1 );
		ChkExp( errno == EBADF, ("bad errno %d [%d]\n", errno, EBADF) );
		TstTrace("4");

		TST_SPIN(TST_PAUSE);
		ChkExp( errno == EBADF, ("bad errno %d [%d]\n", errno, EBADF) );
	}
	return (0);
}


TST_BEGIN(setErrno)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	int		threads = TST_TRIAL_THREADS;

	void		*libENOENT(void *);
	void		*libEINVAL(void *);
	void		*sysESRCH(void *);
	void		*sysEBADF(void *);

	threads = ((threads + 3) / 4) * 4;
	ChkPtr( pts = malloc(threads * sizeof(pthread_t)), != 0 );

	for (p = 0; p < threads; p++) {

		ChkInt( pthread_create(pts+p++, 0, libENOENT, 0), == 0 );
		ChkInt( pthread_create(pts+p++, 0, libEINVAL, 0), == 0 );
		ChkInt( pthread_create(pts+p++, 0, sysESRCH, 0), == 0 );
		ChkInt( pthread_create(pts+p, 0, sysEBADF, 0), == 0 );
	}

	for (p = 0; p < threads; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Exercise threaded C library" )

	TST( setErrno, "Verify per-thread errno",
		"Create threads which make libc calls to set errno"
		"each thread repeatedly verifies its expected result"
		"emit tag indicating progress with different errnos" ),

TST_FINISH

