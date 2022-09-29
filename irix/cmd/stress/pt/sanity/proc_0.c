/*
** Purpose: Test
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>

#include <Tst.h>


/* ------------------------------------------------------------------ */

pthread_once_t	fork_handler_init = PTHREAD_ONCE_INIT;


void prepare0(void);
void prepare1(void);
void child0(void);
void child1(void);
void parent0(void);
void parent1(void);

#define FORKFUNC(name)	void name(void) \
		{ TstTrace("Running " #name "\n"); }

FORKFUNC(prepare0)
FORKFUNC(prepare1)
FORKFUNC(parent0)
FORKFUNC(parent1)
FORKFUNC(child0)
FORKFUNC(child1)

void
fork_handlers(void)
{
	ChkInt( pthread_atfork(prepare1, parent1, child1), == 0 );
	ChkInt( pthread_atfork(prepare0, parent0, child0), == 0 );
}

/* ------------------------------------------------------------------ */

static void *
forker(void *arg)
{
	int	i;
	int	status;
	pid_t	pid;

	for (i = 0; i < TST_TRIAL_LOOP; i++) {

		TstTrace("Forking %d\n", i);
		ChkInt( pid = fork(), != -1 );

		if (!pid) {

			/* Child
			 */
			TstTrace("Fork running %d\n", getpid());
			exit(5);
		}

		/* Parent
		 */
		ChkInt( waitpid(pid, &status, 0), == pid );
		ChkExp( WIFEXITED(status) && WEXITSTATUS(status) == 5,
			("bad wait status %#x [5]\n", status) );
	}
	return (0);
}


TST_BEGIN(fork_wait)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void	*forker(void *);

	ChkInt( pthread_once(&fork_handler_init, fork_handlers), == 0 );

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, forker, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

static void
sprocee(void *arg)
{
	TstTrace("Sproc running %d\n", getpid());
	ChkExp( arg == (void *)5, ("bad sproc arg %p [5]\n", arg) );
	exit(5);
}


static void *
sprocer(void *arg)
{
	int	i;
	int	status;
	int	pid;

	for (i = 0; i < TST_TRIAL_LOOP; i++) {

		TstTrace("Sprocing %d\n", i);
#ifdef	SPROC_IS_ALLOWED
		ChkInt( pid = sproc(sprocee, PR_SALL, (void *)5), != -1 );

		/* Parent
		 */
		ChkInt( waitpid(pid, &status, 0), == pid );
		ChkExp( WIFEXITED(status) && WEXITSTATUS(status) == 5,
			("bad wait status %#x [5]\n", status) );
#else	/* SPROC_IS_ALLOWED */
		ChkInt( pid = sproc(sprocee, PR_SALL, (void *)5), == -1 );
		ChkExp( errno == ENOTSUP,
			("Bad errno %d [%d]\n", errno, ENOTSUP) );
#endif	/* SPROC_IS_ALLOWED */
	}
	return (0);
}


TST_BEGIN(sproc_wait)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void	*sprocer(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sprocer, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

static void
sprocspee(void *arg, size_t size)
{
	TstTrace("Sprocsp running %d %d\n", size, getpid());
	ChkExp( arg == (void *)5, ("bad sprocsp arg %p [5]\n", arg) );
	exit(5);
}


static void *
sprocsper(void *arg)
{
#	define	STKSIZE	0x2000
	int	i;
	int	status;
	int	pid;
	char	*sp;

	ChkPtr( sp = malloc(STKSIZE), != 0 );
	for (i = 0; i < TST_TRIAL_LOOP; i++) {

		TstTrace("Sprocsping %d\n", i);
#ifdef	SPROC_IS_ALLOWED
		ChkInt( pid = sprocsp(sprocspee, PR_SALL, (void *)5, sp, STKSIZE),
			!= -1 );

		/* Parent
		 */
		ChkInt( waitpid(pid, &status, 0), == pid );
		ChkExp( WIFEXITED(status) && WEXITSTATUS(status) == 5,
			("bad wait status %#x [5]\n", status) );
#else	/* SPROC_IS_ALLOWED */
		ChkInt( pid = sprocsp(sprocspee, PR_SALL, (void *)5, sp, STKSIZE),
			== -1 );
		ChkExp( errno == ENOTSUP,
			("Bad errno %d [%d]\n", errno, ENOTSUP) );
#endif	/* SPROC_IS_ALLOWED */
	}
	free(sp);
	return (0);
}


TST_BEGIN(sprocsp_wait)
{
	int		p;
	pthread_t	*pts;
	void		*ret;
	void	*sprocsper(void *);

	ChkPtr( pts = malloc(TST_TRIAL_THREADS * sizeof(pthread_t)), != 0 );
	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_create(pts+p, 0, sprocsper, 0), == 0 );
	}

	for (p = 0; p < TST_TRIAL_THREADS; p++) {
		ChkInt( pthread_join(pts[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	free(pts);

	TST_RETURN(0);
}

/* ------------------------------------------------------------------ */

TST_START( "Test process creation" )

	TST( fork_wait, "fork() + waitpid()",
		"Set up fork handlers."
		"Create a pool of threads which call fork()"
		"and then waitpid() for child which exit()s." ),

	TST( sproc_wait, "sproc() + waitpid()",
		"Create a pool of threads which call sproc(PR_SALL)"
		"and then waitpid() for child which exit()s.\n" ),

	TST( sprocsp_wait, "sprocsp() + waitpid()",
		"Create a pool of threads which call sprocsp(PR_SALL)"
		"and then waitpid() for child which exit()s.\n" ),
TST_FINISH
