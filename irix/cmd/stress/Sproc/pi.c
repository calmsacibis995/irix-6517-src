/*
 * parallel pi approximation
 */

#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "sys/types.h"
#include "sys/times.h"
#include "ulocks.h"
#include "getopt.h"

/* defaults if no options given */
#define MAXPROC 2
#define MAXRECT 1600000
#define HZ 100
int nprocs = MAXPROC;		/* # processes */
int nrects = MAXRECT;		/* highest # to look for */
int eachpiece;			/* number of iterations that each process does */
double total_pi = 0.0;	/* approximation of pi */
double width;		/* width of each rectangle for approximation */
extern void parse_args(int argc, char **argv);
extern void pieceofpie(void);

int
main(int argc, char **argv)
{
struct tms tim;			/* declare time variables for accounting */
long stime, eltime;

	
	parse_args(argc, argv);		/* parse command line arguments */
	width = 1.0 / nrects;		/* calculate the weight of each iteration*/
	eachpiece = nrects/nprocs;	/* calculate the number of iterations each process should do */

	m_set_procs(nprocs);		/* set the number of parallel processes */
	stime = times(&tim);		/* start timing here */
	m_fork(pieceofpie);		/* start up shared processes */
	eltime = times(&tim) - stime;	/* stop timing here */
	printf(" about to kill pi \n");
	total_pi *= width;		/* scale final summation */

	/* print results */
	printf("elapsed time (real):%d mS\n", (eltime * 1000) / HZ);
	m_kill_procs();			/* kill of all those idle processes - their job is done */
	printf(" The approximation to PI is %lf \n", total_pi);
	return 0;
}

/*
 * pieceofpie process
 *	Uses a global variable "eachpiece" to determine how much work to do
 */
void
pieceofpie(void)
{
double x;			/* variables local to the routine are allocated on the stack, */
				/*     so are distinct for each process */
double mypiece = 0.0;	/* the initial value of my piece of the summation */
int starti;	
int endi;
int i;				/* loop counter */

	
	starti = ( m_get_myid() ) * eachpiece;	/* define starting index */
	endi = starti + eachpiece;
	for ( i = starti; i < endi ; i++ ) {	
		x = ((i - 0.5) * width);	/* calculate this term */
		mypiece += (4.0/(1.0 + x*x));
	}
		
	m_lock();				/* protect adding my piece to the total summation */
	total_pi+=mypiece;
	printf(" HEY, I'm %d - my piece is %lf %g\n", m_get_myid(), mypiece*width, mypiece*width);
	m_unlock();
}

void
parse_args(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "r:n:")) != EOF)
		switch (c) {
		case 'r':	/* number of slices of pi */
			nrects = atoi(optarg);
			break;
		case 'n':	/* number of processes to spawn */
			nprocs = atoi(optarg);
			usconfig(CONF_INITUSERS,nprocs);
			break;
		case '?':
		default:
			fprintf(stderr, "Usage:pi [-r nrects][-n nprocs]\n");
			exit(-1);
		}
	printf("# slave processes:%d nrects:%d\n", nprocs, nrects);
}
