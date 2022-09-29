#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <aio.h>
#include <ulocks.h>
#include <fcntl.h>
volatile int togo;
static void
comp(int x)
{
    togo--;
}

main()
{
    int fd, i;
    aiocb_t list[4];
    char *buf[4];

    buf[0] ="ab";
    buf[1] ="cd";
    buf[2] ="ef";
    buf[3] ="gh";
    fd = open("/usr/tmp/aioapendtest.txt",  O_RDWR|O_APPEND|O_CREAT|O_TRUNC,0666);
    if (fd < 0) {
	perror("open of /usr/tmp/aioapendtest.txt failed");
	exit(1);
    }
	
    for (i = 0; i < 4; i++ ){
	list[i].aio_fildes = fd;
	list[i].aio_sigevent.sigev_notify = SIGEV_CALLBACK;
	list[i].aio_sigevent.sigev_func = (void (*)(union sigval))comp;
	list[i].aio_offset = 0;
	list[i].aio_buf = buf[i];
	list[i].aio_nbytes = 2;
	list[i].aio_reqprio = 0;
    }
    togo = 0;

	/* We need to hold off on callbacks so that the loop below and
	 * the callbacks do not try and change the variable at the same
	 * time.
	 */
    
    aio_hold(AIO_HOLD_CALLBACK);
    for (i = 0; i < 4; i++ ){
	togo++;
	if (aio_write(&list[i]) != 0) {
	    perror("aio_write");
	}
    }
    aio_hold(AIO_RELEASE_CALLBACK);
    while(togo);
#if 0
	{
	    printf("togo now %d\n",togo);
	}
#endif
}
