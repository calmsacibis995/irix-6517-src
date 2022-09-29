#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <ulocks.h>

static struct timeval	then;

struct {
    char	*op;
    int		enable;
    long	ctr_last;
    long	ctr;
} control[] = {
    { "FP ops",		0,	0,	0 },
    { "1 D-cache miss (read)",	0,	0,	0 },
    { "2 D-cache miss (write)",	0,	0,	0 },
    { "Store cond",	0,	0,	0 },
};

static int ncontrol = sizeof(control) / sizeof(control[0]);

double	x = 0.0;
char	data[64 * 1024 * 1024 + 128];
char	c;
int	miss1 = 0;
int	miss2 = 128;
int	size2cache = 1024 * 1024;
int	size1cache = 32 * 1024;
ulock_t	mylock;
usptr_t	*handle;

/*ARGSUSED*/
void
onalarm(int dummy)
{
    struct timeval	now;
    double		secs;
    double		rate;
    int			i;

    gettimeofday(&now);
    secs = now.tv_sec - then.tv_sec + (double)(now.tv_usec - then.tv_usec) / 1000000;
    for (i = 0; i < ncontrol; i++) {
	if (control[i].enable) {
	    rate = (control[i].ctr - control[i].ctr_last) / secs;
	    printf("%.0f %s per sec\n", rate, control[i].op);
	    control[i].ctr_last = control[i].ctr;
	}
    }
    fflush(stdout);

    then = now;

    signal(SIGALRM, onalarm);
    alarm(10);
}
    
main(int argc, char **argv)
{
    int		i;
    int		c;
    double	f = 1.25;

    while ((c = getopt(argc, argv, "fdls")) != -1) {
	switch (c) {
	    case 'f':
		control[0].enable = 1;
		break;
	    case 'd':
		control[1].enable = 1;
		break;
	    case 'l':
		control[3].enable = 1;
		unlink("arena");
		handle = usinit("arena");
		mylock = usnewlock(handle);
		break;
	    case 's':
		control[2].enable = 1;
		break;
	}
    }

    gettimeofday(&then);
    signal(SIGALRM, onalarm);
    alarm(10);

    for ( ; ; ) {
	for (i = 0; i < ncontrol; i++) {
	    if (control[i].enable) {
		switch (i) {
		    case 0:	/* FP */
			    x += f;
			    break;
		    case 1:	/* 1 D-cache miss */
			    c = data[miss1];
			    miss1 += size1cache;
			    if (miss1 > size2cache) miss1 = 0;
			    break;
		    case 2:	/* 2 D-cache miss, 8 Mb stride */
			    data[miss2] = 42;
			    miss2 += 8 * 1024 * 1024;
			    if (miss2 > sizeof(data)) miss2 = 128;
			    break;
		    case 3:	/* Store conditional, 2 per call */
			    ussetlock(mylock);
			    usunsetlock(mylock);
			    control[i].ctr++;
			    control[i].ctr++;
			    control[i].ctr++;
			    break;
		}
		control[i].ctr++;
	    }
	}
    }
}

