#include    <sys/time.h>
#include    <errno.h>

/*
 * On Sprite, an unopened stream returns an error of SYS_INVALID_ARG,
 * which maps to EINVAL. On UNIX, select will return EBADF.
 */
#ifdef Sprite
#define CLOSED	  EINVAL
#else
#define CLOSED EBADF
#endif /* Sprite */
main()
{
    int	    	  	i;
    unsigned int  	mask;
    struct timeval	timeout;

    timeout.tv_sec = timeout.tv_usec = 0;
    
    printf("Open streams:");
    for (i = 0; i < 32; i++) {
	mask = 1 << i;
	if (select(i + 1, &mask, &mask, &mask, &timeout) >= 0) {
	    printf(" %d", i);
	} else if (errno != CLOSED) {
	    printf(" error on %d: %s", i, strerror(errno));
	}
    }
    printf("\n");
    exit(0);
}
