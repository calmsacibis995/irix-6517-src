

/*
 * random2 min max homany
 *
 * print a 'howmay' unique random integers between 'min' and 'max', inclusive
 */
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>

main(int argc, char **argv)
{
	int min, max, number, howmany, t0, t1;
	struct timeval tp;
	struct timezone tz;
	int *nums;

	if ( argc < 3 ) {
		fprintf(stderr, "usage: random min max [howmany]\n");
		exit(1);
	}

	min = atoi(argv[1]);
	max = atoi(argv[2]);
	if ( argc > 3 )
		howmany = atoi(argv[3]);
	else
		howmany = 1;

	nums = (int*)calloc(howmany, sizeof(int));
	if ( nums == NULL ) {
		fprintf(stderr, "random2: calloc(%d, int) failed\n", howmany);
		exit(1);
	}

	gettimeofday(&tp, &tz);
	srandom(tp.tv_usec);

	for ( t0 = 0; t0 < howmany; t0++ ) {
pickagain:
		number = (float)(max - min) * random() / INT_MAX + min;
		for ( t1 = 0; t1 <= t0; t1++ )
			if ( nums[t1] == number )
				goto pickagain;
		nums[t0] = number;
		printf("%d\n", number);
	}
}
