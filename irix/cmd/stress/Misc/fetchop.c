/*
 * Test fetchop hardware atomic operations
 * It's only available in MIPS ABI n32 and 64 modes
 */

#if (_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64)

#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/prctl.h>
#include <string.h>
#include <fmtmsg.h>
#include <stdlib.h> /* exit() */
#include <sys/wait.h> /* wait() */
#include <dlfcn.h> /* sgidladd */
#include <assert.h>
#include <malloc.h>
#include <unistd.h>
#include <fetchop.h>

#define NUM_OF_THREADS 		7
#define NUM_OF_FETCHOP_VARS 	16

#define	FETCHOP_INITIAL_VALUE	11

#define	MAX_ITERATIONS		10000

fetchop_var_t *fetchop_vars[NUM_OF_FETCHOP_VARS];

/*
 * Array where slaves save the 'count' added to a specific fetchop variable
 */
int	slave_count_array[NUM_OF_THREADS][NUM_OF_FETCHOP_VARS];

extern void	fetchop_incdec_test(void);
extern void	slave(void *);

int	verbose;
int
main(void)
{

	fetchop_incdec_test();
	return(0);
}

void
fetchop_incdec_test(void)
{

	fetchop_reservoir_t fetchop_reservoir;
	int 	i;
	int	error=0;

	fetchop_reservoir = fetchop_init(USE_DEFAULT_PM, 
					 NUM_OF_FETCHOP_VARS);
	if (fetchop_reservoir == NULL) {
		printf("fetchop_init fails\n");
		exit(1);
	}
	else {
		if (verbose)
			printf("fetchop_init succeeds\n");
	}

	for (i = 0; i < NUM_OF_FETCHOP_VARS; i++) {
		
		fetchop_vars[i] = fetchop_alloc(fetchop_reservoir);
		if (fetchop_vars[i] == NULL) {
			printf("fetchop_alloc fails\n");
			exit(1);
		}
		else {
			if (verbose)
				printf("fetchop_alloc succeeds\n");
		}
		storeop_store(fetchop_vars[i], FETCHOP_INITIAL_VALUE); 
		if (verbose)
			printf("fetchop_var[%d] initialized to  %ld\n", 
					i, fetchop_load(fetchop_vars[i]));
	}

	for (i = 0; i < NUM_OF_THREADS; i++) {
		if (sproc(slave, PR_SALL, i) < 0) {
			 perror("sproc");
			 exit(1);
		}
	}

	/* Wait until all threads die */
	for (i = NUM_OF_THREADS; i > 0; i--) {
                pid_t pid;
                int statptr;
                pid = wait(&statptr);
		if (verbose) 
			printf("Child [%d] is done\n", pid);
        }

	for (i = 0; i < NUM_OF_FETCHOP_VARS; i++) {
		int	j;
		int	final_count = FETCHOP_INITIAL_VALUE;
		for (j = 0; j < NUM_OF_THREADS; j++) {
			if (verbose){
				printf("slave_count_array[%d][%d] = %d\n", 
						j, i, slave_count_array[j][i]);
			}
			final_count += slave_count_array[j][i];
		}

		/*
		 * Check if final count matches the one in the fetchop variable.
		 */
		if (final_count != fetchop_load(fetchop_vars[i])) {
			printf( "fetchop_var[%d] final result incorrect. Expected %d actual %ld\n",
			  i, final_count, fetchop_load(fetchop_vars[i]));
			error++;
		}
		fetchop_free(fetchop_reservoir, fetchop_vars[i]);
	}

	if (error == 0) {
		printf("Fetchop Inc/Dec test completed successfully\n");
	}

}

void 
slave(void	*arg)
{
	int 	i;
	int	iterations;
	int	*count;
	int	indx = (int)(long)arg;

	count  = &slave_count_array[indx][0];

	srand(getpid());
	do {
		iterations = rand() % MAX_ITERATIONS;
	} while (!iterations);

	/*
	 * Initialize our count 
	 */
	for (i = 0; i < NUM_OF_FETCHOP_VARS; i++) {
		slave_count_array[indx][i] = 0;
		count[i] = 0;
	}

	while (iterations--) {
		for (i = 0; i < NUM_OF_FETCHOP_VARS; i++) {
			if (rand() & 1) {
				count[i]++;
				fetchop_increment(fetchop_vars[i]);
			} else {
				count[i]--;
				fetchop_decrement(fetchop_vars[i]);
			}
		}
	}
#if 0
	for (i = 0; i < NUM_OF_FETCHOP_VARS; i++) {
		slave_count_array[indx][i] = count[i];
	}
#endif
}	

#else 

void main() {}

#endif /* (_MIPS_SIM == _ABIN32) || (_MIPS_SIM == _ABI64) */



