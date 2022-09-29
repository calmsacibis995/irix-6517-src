#include <sys/time.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

double double_time(void)
{
    struct timeval	tv;

    gettimeofday(&tv, 0);

    return (tv.tv_sec + tv.tv_usec * 0.000001);
}

void double_sleep(double sec)
{
    long		usec	= (long) (sec * 1000000.0);
    struct timeval	tv;

    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;

    select(0, 0, 0, 0, &tv);
}

int double_select(int nfds, fd_set *readfds, fd_set *writefds,
		  fd_set *exceptfds, double expire)
{
    double		tmo;
    long		usec;
    struct timeval	tv;

    tmo = expire - double_time();

    if (tmo < 0.0)
	tmo = 0.0;

    usec = (long) (tmo * 1000000.0);

    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;

    return select(nfds, readfds, writefds, exceptfds, &tv);
}

#ifdef TESTING
main()
{
    printf("Start\n");
    double_sleep(0.5);
    printf("Stop\n");
}
#endif
