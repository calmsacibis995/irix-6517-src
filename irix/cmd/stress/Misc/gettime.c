#include "unistd.h"
#include "stdlib.h"
#include "stdio.h"
#include "time.h"
#include "sys/types.h"
#include "sys/time.h"

int
main(int argc, char **argv)
{
	struct timeval tv, otv;
	register int i, j;
	long ousec;
	long diffs[20];

	if (gettimeofday(&tv) < 0) {
		perror("gettimeofday");
		exit(-1);
	}
	printf("sec:%d usec:%d\n", tv.tv_sec, tv.tv_usec);

	/* now see how many system calls until usecs change */
	for (j = 0; j < 10; j++) {
		gettimeofday(&tv);
		i = 0;
		do {
			ousec = tv.tv_usec;
			gettimeofday(&tv);
			i++;
		} while (tv.tv_usec == ousec);
		printf("%d calls to gettimeofday until usec chged from %d to %d\n",
			i, ousec, tv.tv_usec);
	}
	/* now see how many system calls in one second */
	for (j = 0; j < 10; j++) {
		gettimeofday(&otv);
		otv.tv_sec++;
		i = 0;
		do {
			gettimeofday(&tv);
			i++;
		} while (timercmp(&otv, &tv, >));
		printf("%d calls to gettimeofday in 1 sec %d uS/call\n",
			i, 1000000/i);
	}
	for (j = 0; j < 20; j++) {
		/* wait till usec changes */
		gettimeofday(&otv);
		for (i = 0; ; i++) {
			gettimeofday(&tv);
			if (tv.tv_usec != otv.tv_usec ||
			    tv.tv_sec != otv.tv_sec)
				break;
		}
		if (tv.tv_usec > otv.tv_usec)
			diffs[j] = tv.tv_usec - otv.tv_usec;
		else
			diffs[j] = otv.tv_usec - tv.tv_usec;
		otv = tv;
	}
	printf("diffs: ");
	for (j = 0; j < 20; j++)
		printf("%d ", diffs[j]);
	printf("\n");

	return 0;
}
