#include <sys/types.h>
#include <limits.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/EVEREST/IP19.h>

#define SEC 1000000

int newdelay(int time)
{
    static inited = 0;
    volatile unsigned *rtc;
    unsigned int prevtime, currtime; 
    int waittime;
    int fd;

    if (!inited) {
        if ((fd = open("/dev/mmem", O_RDONLY)) < 0) {
            perror("Could not open /dev/mmem");
            exit(1);
        }

        rtc = (volatile unsigned*) mmap(0, 4096, PROT_READ, MAP_SHARED, fd,
                                        EV_RTC);
        if (rtc == (volatile unsigned*) -1) {
            perror("mmap failed");
            exit(1);
        }
        rtc++;
    }

    /* Calculate amount of time to wait */
    waittime = time * 47;
    prevtime = *rtc;
    while (waittime > 0) {
        currtime = *rtc;
/* printf("ct = %u, pt = %u, wt = %u\n", currtime, prevtime, waittime); */
        if (currtime < prevtime)
            waittime -= (UINT_MAX - prevtime) + currtime;
        else
            waittime -= currtime - prevtime;

	prevtime = currtime;
    }
}

main()
{
	printf("Delay for one second...\n");
	newdelay(10*SEC);

	printf("End delay\n");
}
