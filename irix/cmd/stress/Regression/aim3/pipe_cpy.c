/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) pipe_cpy.c:3.2 5/30/92 20:18:43" };
#endif



#include <signal.h>
#include <sys/types.h>
#include <sys/times.h>

#ifdef T_TIMES
#include <time.h>
#else   /* NOT SYSTEM V or just wanted to use the ftime() instead */
#include <sys/timeb.h>
#include <sys/time.h>
#endif

#include "suite3.h"

/*
**  PIPE THROUGH PUTTER
*/

#define PIPELOOP	1000

#ifdef STAND_ALONE
#define CPUTIME(x)     ((long) times(&t[0]),t[0].tms_utime+t[0].tms_stime+t[0].tms_cstime+t[0].tms_cutime)
#endif

#ifdef STAND_ALONE
main(argc,argv)
int argc;
char **argv;
#else
pipe_cpy( argv, res )
char *argv;
Result *res;
#endif

#ifdef STAND_ALONE
{
	int	j;

	for (j=0;j<100;j++) time_pipe();
}

time_pipe()
#endif
{

	register int	i, count;
	int		pd[2];
	char		BUF[2*512];

#ifdef STAND_ALONE
	long		rmtsec();
	double          JPMHold;                        /* JPM = Jobs Per Minute */
	long            t1, t2, ct1, ct2, rt1, rt2;     /* time variables */
	struct tms      t[2];
	double		child_time,
			real_time,
			cpu_time;
	FILE		*fp, *fopen();			/* file pointer */

	/* open results file */
        if ((fp = fopen("results", "a")) == NULL) {
                perror("open_results_file");
                fprintf(stderr, "Can not open results data file.. EXIT\n");
                exit(3);
        }

	/* start timers */
	t1 = CPUTIME(0);  ct1 = CHILDHZ(0); rt1 = RTMSEC(FALSE);
#endif

	/* open pipe once */
	if ( pipe(pd)<0 ) { /* deleted two extra check */
		fprintf(stderr, "pipe_cpy - can't open pipe\n");
		return(-1);
	}

	if (fork())
	      {
		/* parent process */
		for (i=0; i<PIPELOOP; i++) {
			count = 0;
			while ((write(pd[1], BUF, sizeof(BUF)) != sizeof(BUF)) && (count++ < 8));
			if (count >= 8) {
				fprintf(stderr, "pipe_cpy - can't write pipe\n");
				return(-1);
			}
		}
		close(pd[0]);
		close(pd[1]);
		wait(0);
		/* this should be uncommented at the next major release */
		/* close(pd[0]); */
	      }
	else
	      {	
		/* child process */
		/*
		for (i=0;i<PIPELOOP;i++)
			count = 0;
			while ( (read(pd[0],BUF,sizeof(BUF)) != sizeof(BUF)) && count++<8);
			if (count >= 8) {
				perror("pipe_cpy()");
				fprintf(stderr, "pipe_cpy - can't read pipe\n");
				exit(-1);
			}
		*/
		count = 0;
		for (i=PIPELOOP*sizeof(BUF);i>0;i-=count) {
			if ( (count = read(pd[0],BUF,sizeof(BUF))) <= 0)
				break;
		}
		if ((i != 0) || (count <= 0)) {
			fprintf(stderr, "pipe_cpy - can't read pipe\n");
			exit(-1);
		}
		close(pd[0]);
		close(pd[1]);
		/* this should be uncommented at the next major release */
		/* close(pd[1]); */
		exit(0);
	      }

#ifdef STAND_ALONE
        t2 = CPUTIME(0);  ct2 = CHILDHZ(0); rt2 = RTMSEC(FALSE);
        real_time = (rt2 - rt1) / 1000.0;
        cpu_time = (t2 - t1) / (double)HZ;
        child_time = (ct2 - ct1) / (double)HZ;
        printf( " %6.1lf real  %6.1lf cpu  %6.1lf child\n", real_time, cpu_time, child_time);
        fflush(stdout);
        fprintf(fp,"  %6.1lf real  %6.1lf cpu  %6.1lf child\n", real_time, cpu_time, child_time);
	fclose(fp);

#endif

#ifndef STAND_ALONE
	res->i = i;
	return(0);
#endif
}

