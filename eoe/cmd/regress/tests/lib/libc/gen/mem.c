#include        <stdio.h>
#include        <stdlib.h>
#include        <sys/types.h>
#include        <time.h>
#include        <math.h>

#define DEF_NUM_ROUNDS  	100000
#define DEF_MALLOC_RANGE  	0xff
#define DEF_NUM_BUCKETS		64

void
usage(cmd)
{
	fprintf(stderr,
		"usage: %s [-r<rounds>] [-d<range>] [-b<buckets>]\n", cmd);
}

int
main(int argc, char **argv)
{
        int		  i, c, size, bucket;
	extern char	 *optarg;
	int 		  num_rounds = DEF_NUM_ROUNDS;
	int		  malloc_range = DEF_MALLOC_RANGE;
	int 		  num_buckets = DEF_NUM_BUCKETS;
	void		**buckets;

	while ((c = getopt(argc, argv, "r:d:b:h")) != EOF)
		switch (c) {
		    case 'h':
			    usage(argv[0]);
			    exit(0);
		    case 'r':
			    num_rounds = atoi(optarg);
			    if (num_rounds <= 0)
				    num_rounds = DEF_NUM_ROUNDS;
			    break;
		    case 'd':
			    malloc_range = atoi(optarg);
			    if (malloc_range <= 0)
				    malloc_range = DEF_MALLOC_RANGE;
			    break;
		    case 'b':
			    num_buckets = atoi(optarg);
			    if (num_buckets <= 0)
				    num_buckets = DEF_NUM_BUCKETS;
			    break;
		    case '?':
			    usage(argv[0]);
			    exit(1);
		}

	/* Create num_bucket buckets all set to zero */
	buckets = (void *)calloc(num_buckets, sizeof(*buckets));
	if (buckets == NULL) {
		fprintf(stderr, "ERROR: Not enough memory available.\n");
		exit(1);
	}

	fprintf(stdout, "Rounds = %d   Range = 0x%x   Buckets=%d\n",
		num_rounds, malloc_range, num_buckets);
	
	/* set up random number generators */
	srandom(time(NULL));	/* for sizes */
	srand(time(NULL));	/* for coin flip */
	
	for (i = 1; i <= num_rounds; i++) {
		bucket = random() % num_buckets;
		size = (rand() & malloc_range) + 1;
		if (buckets[bucket] == NULL) {
			if (rand() % 2)
				buckets[bucket] = (void *)malloc(size);
			else
				buckets[bucket] = (void *)memalign(16, size);
		} else {
			if (rand() % 2)
				buckets[bucket] = (void *)realloc(buckets[bucket], size);
			else {
				free(buckets[bucket]);
				buckets[bucket] = NULL;
			}
		}
	}
	exit(0);
}
