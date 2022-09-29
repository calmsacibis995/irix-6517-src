#include <stdio.h>
#include <sys/pmo.h>
#include <fetchop.h>

#define	NUM_ATOMIC	100

static void
ckpt_handler(){}

main(int argc, char *argv[])
{
	int c;
	int ident = 0;
	atomic_res_ident_t res_id = NULL;
	extern char *optarg;
	int numvars = NUM_ATOMIC;
	atomic_reservoir_t reservoir;
	atomic_var_t **atomic;
	long i;

	atcheckpoint(ckpt_handler);

	while ((c = getopt(argc, argv, "in:")) != EOF) {

		switch (c) {

		case 'i':
			ident = 1;
			break;
		case 'n':
			numvars = atoi(argv[1]);
			break;
		}
	}
	printf("Configure %d vars\n", numvars);

	if (ident)
		res_id = atomic_alloc_res_ident(numvars);

	reservoir = atomic_alloc_reservoir(	pm_getdefault(MEM_DATA),
						(size_t)numvars,
						res_id);


	atomic = (atomic_var_t **)malloc(numvars * sizeof(atomic_var_t *));

	for (i = 0; i < numvars; i++) {
		atomic[i] = atomic_alloc_variable(reservoir, NULL);
		atomic_store(atomic[i], (atomic_var_t)i);
	}
	printf("Ready:%lx\n", atomic[0]);
	pause();

	printf("Checking\n");
	for (i = 0; i < numvars; i++) {
		*atomic[i] = atomic_load(atomic[i]);
		if (*atomic[i] != (atomic_var_t)i)
			printf("Mis-match expected %ld got %ld\n", i, *atomic[i]);
	}
	if (ident) {
		for (i = 0; i < numvars; i++)
			atomic_free_variable(reservoir, atomic[i]);
	
		atomic_free_reservoir(reservoir);
	}
	printf("Done\n");
}
