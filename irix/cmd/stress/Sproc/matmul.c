#include "sys/types.h"
#include "stdlib.h"
#include "unistd.h"
#include "task.h"
#include "stdio.h"

/*
 * matmul - static sched example from Sequent
 */
#define SIZE 10

float a[SIZE][SIZE];
float b[SIZE][SIZE];
float c[SIZE][SIZE];
void init_matrix(float a[][SIZE], float b[][SIZE]);
void matmul(float a[][SIZE], float b[][SIZE], float c[][SIZE]);
void print_mats(float a[][SIZE], float b[][SIZE], float c[][SIZE]);

int
main(int argc, char **argv)
{
	int nprocs;
	if (argc < 2) {
		fprintf(stderr, "Usage:%s nprocs\n", argv[0]);
		exit(-1);
	}
	nprocs = atoi(argv[1]);

	init_matrix(a, b);
	m_set_procs(nprocs);
	printf("Using %d processes\n", nprocs);

	m_fork(matmul, a, b, c);
	m_kill_procs();
	print_mats(a, b, c);
	return 0;
}

void
init_matrix(float a[][SIZE], float b[][SIZE])
{
	register int i, j;

	for (i = 0; i < SIZE; i++) {
		for (j = 0; j < SIZE; j++) {
			a[i][j] = (float) i + j;
			b[i][j] = (float) i - j;
		}
	}
}

void
matmul(float a[][SIZE], float b[][SIZE], float c[][SIZE])
{
	register int i, j, k, nprocs;

	nprocs = m_get_numprocs();

	for (i = m_get_myid(); i < SIZE; i += nprocs) {
		for (j = 0; j < SIZE; j++) {
			for (k = 0; k < SIZE; k++)
				c[i][k] += a[i][j] * b[j][k];
		}
	}
}

void
print_mats(float a[][SIZE], float b[][SIZE], float c[][SIZE])
{
	register int i, j;

	for (i = 0; i < SIZE; i++) {
		for (j = 0; j < SIZE; j++) {
			printf("[%d][%d] a %3.2f b %3.2f c %3.2f\n",
				i, j, a[i][j], b[i][j], c[i][j]);
		}
	}
}
