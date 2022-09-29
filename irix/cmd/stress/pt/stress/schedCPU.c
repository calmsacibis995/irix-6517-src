/*
** Purpose: Exercise CPU bind for pthreads
**
** Details:
**	Find out what CPUs are available.
**	Create system scope pthreads.
**	Each thread spins on a CPU before hopping to the next.
**	Sample the PRDA t_cpu counter to find the current CPU.
** 
** NOTE: t_cpu appears unreliable - it "sticks" to the initial CPU but
**	thereafter follows the thread changes
*/

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/pda.h>
#include <sys/sysmp.h>
#include <task.h>
#include <errno.h>
#include <unistd.h>

#include <Tst.h>


int	spin = 0;	/* cpu spin */
int	loops = 100;
int	changes = 20;
int	threads;
int	*cpuTargets;
int	ncpus;
int	maxcpus;

pthread_t		*ptIds;

main(int argc, char *argv[])
{
#define	ARGS	"b:c:l:r:t::O:U"
#define	USAGE	"Usage: %s [-b #][-c #][-l #][-r #][-t #][-U(sage)]\n"
	int		c, errflg = 0;

	int		cpu;
	struct pda_stat	*cpuPda;
	char		*opts = 0;
	void		*schedRunAll();

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, ARGS)) != EOF)
		switch (c) {
		case 'b'	:
			spin = strtol(optarg, 0, 0);
			break;
		case 'c'	:
			changes = strtol(optarg, 0, 0);
			break;
		case 'l'	:
			loops = strtol(optarg, 0, 0);
			break;
		case 'r'	:
			changes = strtol(optarg, 0, 0);
			break;
		case 't'	:
			threads = strtol(optarg, 0, 0);
			break;
		case 'O'	:
			opts = optarg;
			break;
		case 'U'	:
			printf(USAGE, argv[0]);
			printf(
				"\t-b #\t- cpu spin for thread [yield]\n"
				"\t-l #\t- cpu loops [%d]\n"
				"\t-r #\t- cpu changes [%d]\n"
				"\t-t #\t- threads [2*nCPUs]\n"
				"\t-U\t- usage message\n",
				loops,
				changes
			);
			exit(0);
		default :
			errflg++;
		}
	if (errflg || optind < argc) {
		fprintf(stderr, USAGE, argv[0]);
		exit(0);
	}
	TstSetOutput(opts);

	ChkInt( maxcpus = sysmp(MP_NPROCS), > 0 );
	ChkPtr( cpuPda = malloc(maxcpus * sizeof(struct pda_stat)), != 0 );
	ChkPtr( cpuTargets = malloc(maxcpus * sizeof(int)), != 0 );

	ChkInt( sysmp(MP_STAT, cpuPda), == 0 );
	for (cpu = 0; cpu < maxcpus; cpu++) {
		if (cpuPda[cpu].p_flags & PDAF_ENABLED) {
			cpuTargets[ncpus++] = cpuPda[cpu].p_cpuid;
		}
	}
	free(cpuPda);

	if (!threads) {
		threads = 2 * ncpus;
	}
	TstInfo("threads %d, cpus %d, changes %d, spins %d\n\n",
		threads, ncpus, changes, spin);

	{
		pthread_attr_t		pa;
		struct sched_param	sp;
		pthread_t		p;
		int			pol = SCHED_FIFO;

		sp.sched_priority = sched_get_priority_min(pol) + 1;

		ChkInt( pthread_attr_init(&pa), == 0 );
		ChkInt( pthread_attr_setdetachstate(&pa,
					PTHREAD_CREATE_DETACHED), == 0 );
		ChkInt( pthread_attr_setscope(&pa, PTHREAD_SCOPE_SYSTEM),
			== 0 );
		ChkInt( pthread_attr_setschedpolicy(&pa, pol), == 0 );
		ChkInt( pthread_attr_setschedparam(&pa, &sp), == 0 );

		ChkInt( pthread_create(&p, &pa, schedRunAll, 0), == 0 );

		ChkInt( pthread_attr_destroy(&pa), == 0 );
	}

	pthread_exit(0);
}

void *
schedRunAll(void *arg)
{
	int			p;
	pthread_attr_t		pa;
	struct sched_param	sp;
	int			pol = SCHED_RR;
	void			*ret;

	void		*threadMain();

	TstInfo("Main: Create threads\n");

	sp.sched_priority = sched_get_priority_min(pol);

	ChkInt( pthread_attr_init(&pa), == 0 );
	ChkInt( pthread_attr_setscope(&pa, PTHREAD_SCOPE_SYSTEM), == 0 );
	ChkInt( pthread_attr_setschedpolicy(&pa, pol), == 0 );
	ChkInt( pthread_attr_setschedparam(&pa, &sp), == 0 );

	ChkPtr( ptIds = malloc(threads * sizeof(pthread_t)), != 0 );

	for (p = 0; p < threads; p++) {
		ChkInt( pthread_create(ptIds+p, &pa, threadMain, ptIds+p),
			== 0 );
	}

	TstInfo("Main: Join all threads...\n");
	for (p = 0; p < threads; p++) {
		ChkInt( pthread_join(ptIds[p], &ret), == 0 );
		ChkExp( ret == 0, ("bad join %#x %#x\n", ret, 0) );
	}

	TstInfo("Main: done\n");
}


void	*
threadMain(void *arg)
{
	int		id = (pthread_t *)arg - ptIds;
	volatile int	i;
	int		l;
	int		s;
	int		c;
	int		nextCpu = id % ncpus;
	int		curCpu;
	int		len;
	int		*cpuStats;
	char		buf[1024];

	TstTrace("[0x%08lx]%3d threadMain Start: [%d]\n",
		pthread_self(), id, get_cpu());

	ChkPtr( cpuStats = calloc(1, maxcpus * sizeof(int)), != 0 );

	/* Use of cancellation is a bit of a stretch.
	 */
	for (s = 0; s < changes; s++) {

		nextCpu = (nextCpu + 1) % ncpus;
		ChkInt( pthread_setrunon_np(cpuTargets[nextCpu]), == 0 );

		for (l = 0; l < loops; l++) {

			cpuStats[get_cpu()]++;

			TstTrace("[0x%08lx]%3d threadMain Start: [%d]\n",
				pthread_self(), id, get_cpu());

			if (spin) {
				for (i = 0; i < spin; i++);
			} else {
				sched_yield();
			}
		}

		ChkInt( pthread_getrunon_np(&curCpu), == 0 );
		ChkExp( curCpu == cpuTargets[nextCpu],
			("bad cpu %d %d\n", curCpu, cpuTargets[nextCpu]) );
	}

	for (c = 0, len = 0; c < maxcpus; c++) {
		len += sprintf(buf + len, " %3d:%5d, ", c, cpuStats[c]);
	}
	TstAdmin("[0x%08lx%3d]> %s\n", pthread_self(), id, buf);

	return (0);
}
