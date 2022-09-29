#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <bstring.h>
#include <sys/sysmp.h>
#include "stress.h"

#define	SharedKey	0XAB0DE
#define	Key1		IPC_PRIVATE
#define	Key2		0XDEADDEAD
#define	Key3		0XABADC0FF
#define	Key4		0XC0FFEE
#define	Key5		0XFAB1
#define	Key6		0XCAFEF00D
#define	Key7		0XFACE0FF
#define	Key8		0XACEF0FF

#define	NTESTS	8

void	test1_cleanup(struct timeval *);
void	test2_cleanup(struct timeval *);
void	test3_cleanup(struct timeval *);
void	test4_cleanup(struct timeval *);
void	test5_cleanup(struct timeval *);
void	test6_cleanup(struct timeval *);
void	test7_cleanup(struct timeval *);
void	test8_cleanup(struct timeval *);

int	test1_setup(int), test1(int);
int	test2_setup(int), test2(int);
int	test3_setup(int), test3(int);
int	test4_setup(int), test4(int);
int	test5_setup(int), test5(int);
int	test6_setup(int), test6(int);
int	test7_setup(int), test7(int);
int	test8_setup(int), test8(int);

void	*shmalloc(int);
void	shmfree(void *);
void deltatime(struct timeval *, struct timeval	*, struct timeval *);
void time_divide(struct timeval	*, int, struct timeval	*);
void print_time(char *, struct timeval *, int);
int run_test(int, int);
void test7_child(int iterations, key_t key, int idx, int shid, int semid);
void test6_child(int iterations, key_t key, int idx, int shid, int semid);
void test8_child(int iterations, key_t key, int idx, int shid, int semid);

char	should_test[NTESTS+1] = { 0 };

int	(*setup[])(int) = {
	test1_setup,
	test2_setup,
	test3_setup,
	test4_setup,
	test5_setup,
	test6_setup,
	test7_setup,
	test8_setup
	};

int	(*tests[])(int) = {
	test1,
	test2,
	test3,
	test4,
	test5,
	test6,
	test7,
	test8
	};

void	(*cleanup[])(struct timeval *) = {
	test1_cleanup,
	test2_cleanup,
	test3_cleanup,
	test4_cleanup,
	test5_cleanup,
	test6_cleanup,
	test7_cleanup,
	test8_cleanup
	};

char	*test_names[] = {
	"Test 1 (shmget private)",
	"Test 2 (shmget/rmid)",
	"Test 3 (shmget exist)",
	"Test 4 (shmat/dt)",
	"Test 5 (shmget/at/dt/rmid)",
	"Test 6 (multi key, multi procs get/attach/detach/rmid)",
	"Test 7 (single key, multi procs)",
	"Test 8 (multi key, multi procs get/rmid)"
	};

#define	NPROCS_DEFAULT	10
#define	ITER_DEFAULT	1000

int nprocs = 0;
int ncells = 0;
int mustrun;
char *Argv0;

int
main(int argc, char **argv)
{
	int iterations = 0;
	int i, c, testno;
	int all_tests = 1;
	int shid, semid, idx;
	int amchild = 0;
	key_t key;

	Argv0 = argv[0];
	opterr = 0;
	while ((c = getopt(argc, argv, "MCx:I:k:s:i:p:t:c:")) != EOF) {
		switch (c) {
		case	'M':
			mustrun = 1;
			break;
		case	'i':
			iterations = atoi(optarg);
			break;
		case	'p':
			nprocs = atoi(optarg);
			break;
		case	't':
			testno = atoi(optarg);
			if (testno > 0 && testno <= NTESTS) {
				should_test[testno] = 1;
				all_tests = 0;
			} else {
				printf("Test %d not valid\n", testno);
				opterr++;
			}
			break;
		case 	'x':
			idx = atoi(optarg);
			break;
		case 	's':
			semid = atoi(optarg);
			break;
		case 	'k':
			key = atoi(optarg);
			break;
		case 	'I':
			shid = atoi(optarg);
			break;
		case	'C':
			amchild = 1;
			break;
		case 	'c':
			ncells = atoi(optarg);
			break;
		default:
			opterr++;
			break;
		}
	}

	if (argc - optind != 0)
		opterr++;

	if (opterr) {
		printf("usage: %s [-i iterations][-p nprocs][-t testno][-c ncells]\n",
			argv[0]);
		exit(1);
	}
	if (amchild) {
		if (testno == 7) {
			test7_child(iterations, key, idx, shid, semid);
		} else if (testno == 6) {
			test6_child(iterations, key, idx, shid, semid);
		} else if (testno == 8) {
			test8_child(iterations, key, idx, shid, semid);
		} else {
			errprintf(ERR_EXIT, "unknown test for child");
		}
	}

	if (nprocs == 0) {
		printf("perf: nprocs defaulting to %d\n", NPROCS_DEFAULT);
		nprocs = NPROCS_DEFAULT;
	}
	if (iterations == 0) {
		printf("perf: iterations defaulting to %d\n", ITER_DEFAULT);
		iterations = ITER_DEFAULT;
	}

	for (i = 1; i <= NTESTS; i++)
		if (all_tests || should_test[i])
			run_test(i, iterations);
	return 0;
}

/********************************
 *	Test 1 - shmget		*
 ********************************/

int		test_id;

/* ARGSUSED */
int
test1_setup(int iter)
{
	test_id = -1;
	return 0;
}

int
test1(
	int     iterations)
{
	int		i;

	for (i = 0; i < iterations; i++) {
		if ((test_id = shmget(Key1, 4096, IPC_CREAT|0777)) < 0) {
			perror("Test 1: shmget");
			return(1);
		}
		if (shmctl(test_id, IPC_RMID, 0) < 0) {
			perror("Test 1: shmctl");
			return(1);
		}
	}
	return(0);
}

/* ARGSUSED */
void
test1_cleanup(struct timeval *delta)
{
	if (test_id != -1)
		shmctl(test_id, IPC_RMID, 0);
}

/********************************
 *	Test 2 - shmget/rmid	*
 ********************************/

/* ARGSUSED */
int
test2_setup(int iter)
{
	return 0;
}

int
test2(
	int     iterations)
{
	int		i;

	for (i = 0; i < iterations; i++) {
		if ((test_id = shmget(Key2, 4096, IPC_CREAT|0777)) < 0){
			perror("Test 2: shmget");
			return(1);
		}
		if (shmctl(test_id, IPC_RMID, 0) < 0) {
			perror("Test 2: shmctl");
			return(1);
		}
	}
	return 0;
}

/* ARGSUSED */
void
test2_cleanup(struct timeval *delta)
{
}

/********************************
 *	Test 3 - shmget		*
 ********************************/

/* ARGSUSED */
int
test3_setup(int iter)
{
	if ((test_id = shmget(Key3, 4096, IPC_CREAT|0777)) < 0) {
		perror("Test 3: shmget setup");
		return(1);
	}
	return 0;
}

int
test3(
	int     iterations)
{
	int		i;

	for (i = 0; i < iterations; i++) {
		if ((test_id = shmget(Key3, 4096, 0777)) < 0) {
			perror("Test 3: shmget");
			return(1);
		}
	}
	return(0);
}

/* ARGSUSED */
void
test3_cleanup(struct timeval *delta)
{
	if (test_id != -1)
		shmctl(test_id, IPC_RMID, 0);
}


/********************************
 *	Test 4 - shmat/dt	*
 ********************************/

/* ARGSUSED */
int
test4_setup(int iter)
{
	if ((test_id = shmget(Key4, 4096, IPC_CREAT|0777)) < 0) {
		perror("Test 4: shmget");
		return 1;
	}
	return 0;
}

int
test4(
	int     iterations)
{
	int		i;
	void		*addr;

	for (i = 0; i < iterations; i++) {
		if ((addr = shmat(test_id, (void *) 0, 0)) == (void *)-1L) {
			perror("Test 4: shmat");
			return(1);
		}
		if (shmdt(addr) != 0) {
			perror("Test 4: shmdt");
			return(1);
		}
	}
	return 0;
}

/* ARGSUSED */
void
test4_cleanup(struct timeval *delta)
{
	if (test_id != -1)
		shmctl(test_id, IPC_RMID, 0);
}

/***********************************
 *	Test 5 - shmget/at/dt/rmid *
 ***********************************/

/* ARGSUSED */
int
test5_setup(int iter)
{
	return 0;
}

int
test5(
	int	iterations)
{
	int		i;
	void		*addr;

	for (i = 0; i < iterations; i++) {
		if ((test_id = shmget(Key5, 4096, IPC_CREAT|0777)) < 0){
			perror("Test 5: shmget");
			return(1);
		}
		if ((addr = shmat(test_id, (void *) 0, 0)) == (void *)-1L) {
			perror("Test 5: shmat");
			return(1);
		}
		if (shmdt(addr) != 0) {
			perror("Test 5: shmdt");
			return(1);
		}
		if (shmctl(test_id, IPC_RMID, 0) < 0) {
			perror("Test 5: shmctl");
			return(1);
		}
	}
	return 0;
}

/* ARGSUSED */
void
test5_cleanup(struct timeval *delta)
{
}

/************************************************
 *	Test 6 - Single key, multiple processes *
 ************************************************/

struct test_result {
	pid_t		pid;
	key_t		key;
	struct timeval	time;
};
struct test_result	*test_retval;
int	test_semid;
int	test_child;
int	test_key;

int test6_shid;

int
test6_setup(int iter)
{
	int	i, nbytes;
	union semun	semarg;
	cell_t curcell = 0;

	nbytes = (int)sizeof(struct test_result) * nprocs;
	if ((test6_shid = shmget(IPC_PRIVATE, nbytes, IPC_CREAT|0777)) < 0) {
		perror("test6_setup: shmget");
		return(1);
	}
	if ((test_retval = shmat(test6_shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test6_setup: shmat");
		return(1);
	}
	bzero(test_retval, nbytes);

	if ((test_semid = semget(IPC_PRIVATE, 1, 0777)) < 0) {
		perror("Test 6: semget setup");
		return(1);
	}

	semarg.val = 0;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 6: semctl setup");
		semctl(test_semid, IPC_RMID, 0);
		return(1);
	}

	for (i = 0; i < nprocs; i++) {
		pid_t	pid;

		pid = fork();
		if (pid < 0) {
			perror("Test 6: fork");
			semctl(test_semid, 0, IPC_RMID, 0);
			return(1);
		}
		if (pid == 0) {		/* child */
			char niterations[32];
			char nshid[32];
			char nkey[32];
			char nsemid[32];
			char nidx[32];
			char *sargv[20];
			int j;

			sprintf(niterations, "-i %d", iter);
			sprintf(nshid, "-I %d", test6_shid);
			sprintf(nkey, "-k %d", Key6 + getpid());
			sprintf(nsemid, "-s %d", test_semid);
			sprintf(nidx, "-x %d", i);
			j = 0;
			sargv[j++] = "shm3";
			sargv[j++] = "-C";
			sargv[j++] = "-t 6";
			sargv[j++] = niterations;
			sargv[j++] = nshid;
			sargv[j++] = nkey;
			sargv[j++] = nsemid;
			sargv[j++] = nidx;
			sargv[j] = NULL;
			if (ncells == 0)
				execvp(Argv0, sargv);
			else {
				if (mustrun)
					sysmp(MP_MUSTRUN, curcell);
				rexecvp(curcell, Argv0, sargv);
			}
			errprintf(ERR_ERRNO_EXIT, "exec failed");
			/* NOTREACHED */
		}
		/*
		 * Parent
		 */
		if (ncells && (++curcell >= ncells))
			curcell = 0;
		test_retval[i].pid = pid;
		test_retval[i].key = Key6 + pid;
	}
	semarg.val = nprocs;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 6: semctl(2) setup");
                if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
			perror("Test 6: semctl(2) setup");
                return(1);
        }
	return 0;
}

void
test6_child(int iterations, key_t key, int idx, int shid, int semid)
{
	int	i, id;
	void	*addr;
	struct test_result *test_info;
	struct sembuf sembuf;
	struct timeval	start_time;
	struct timeval	end_time;
	struct timeval	delta_time;

	/* wait for parent */
	sembuf.sem_num = 0;
	sembuf.sem_op = -1;
	sembuf.sem_flg = 0;
	if (semop(semid, &sembuf, 1) < 0) {
		perror("test6_child: semop");
		exit(1);
	}

	if ((test_info = shmat(shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test6_child: shmat");
		exit(1);
	}

	if (test_info[idx].pid != getpid()) {
		perror("test6_child: test_info wrong");
		exit(1);
	}

	gettimeofday(&start_time);

	for (i = 0; i < iterations; i++) {
		if ((id = shmget(key, 4096, IPC_CREAT|0777)) < 0) {
			perror("Test 6: shmget");
			exit(1);
		}
		if ((addr = shmat(id, (void *) 0, 0)) == (void *)-1L) {
			perror("Test 6: shmat");
			exit(1);
		}
		shmdt(addr);
		if (shmctl(id, IPC_RMID, 0) < 0) {
			perror("Test 6: shmctl");
			exit(1);
		}
	}
	gettimeofday(&end_time);
	deltatime(&start_time, &end_time, &delta_time);
	test_info[idx].time = delta_time;

	exit(0);
}

/* ARGSUSED */
int
test6(int iterations)
{
	/* parent doesn't do anything */
	return 0;
}

void
test6_cleanup(struct timeval *delta)
{
	int	i;

	delta->tv_sec = 0;
	delta->tv_usec = 0;

	while (wait(0) >= 0 || errno == EINTR)
		;

	for (i = 0; i < nprocs; i++) {
		long	usecs;

		delta->tv_sec += (test_retval[i].time.tv_sec/nprocs);
		usecs = (test_retval[i].time.tv_sec%nprocs) * 1000000 +
			test_retval[i].time.tv_usec;
		delta->tv_usec += (usecs/nprocs);
	}

	delta->tv_sec += (delta->tv_usec/1000000);
	delta->tv_usec %= 1000000;

	shmdt(test_retval);
	if (shmctl(test6_shid, IPC_RMID, 0) < 0) {
		perror("test6: shmctl");
	}
	if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
		perror("Test 6: semctl(2) setup");
}

int test7_shid;

int
test7_setup(int iter)
{
	int	i, nbytes;
	union semun	semarg;
	cell_t curcell = 0;

	nbytes = (int)sizeof(struct test_result) * nprocs;
	if ((test7_shid = shmget(IPC_PRIVATE, nbytes, IPC_CREAT|0777)) < 0) {
		perror("test7_setup: shmget");
		return(1);
	}
	if ((test_retval = shmat(test7_shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test7_setup: shmat");
		return(1);
	}
	bzero(test_retval, nbytes);

	if ((test_semid = semget(IPC_PRIVATE, 1, 0777)) < 0) {
		perror("Test 7: semget setup");
		return(1);
	}

	semarg.val = 0;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 7: semctl setup");
		semctl(test_semid, IPC_RMID, 0);
		return(1);
	}

	for (i = 0; i < nprocs; i++) {
		pid_t	pid;

		pid = fork();
		if (pid < 0) {
			perror("Test 7: fork");
			semctl(test_semid, 0, IPC_RMID, 0);
			return(1);
		}
		if (pid == 0) {		/* child */
			char niterations[32];
			char nshid[32];
			char nkey[32];
			char nsemid[32];
			char nidx[32];
			char *sargv[20];
			int j;

			sprintf(niterations, "-i %d", iter);
			sprintf(nshid, "-I %d", test7_shid);
			sprintf(nkey, "-k %d", Key7);
			sprintf(nsemid, "-s %d", test_semid);
			sprintf(nidx, "-x %d", i);
			j = 0;
			sargv[j++] = "shm3";
			sargv[j++] = "-C";
			sargv[j++] = "-t 7";
			sargv[j++] = niterations;
			sargv[j++] = nshid;
			sargv[j++] = nkey;
			sargv[j++] = nsemid;
			sargv[j++] = nidx;
			sargv[j] = NULL;
			if (ncells == 0)
				execvp(Argv0, sargv);
			else
				rexecvp(curcell, Argv0, sargv);
			errprintf(ERR_ERRNO_EXIT, "exec failed");
			/* NOTREACHED */
		}
		/*
		 * Parent
		 */
		if (ncells && (++curcell >= ncells))
			curcell = 0;
		test_retval[i].pid = pid;
		test_retval[i].key = Key7;
	}

	if ((test_id = shmget(Key7, 4096, IPC_CREAT|0777)) < 0) {
		perror("Test 7: shmget setup");
		return(1);
	}
	semarg.val = nprocs;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 7: semctl(2) setup");
                if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
			perror("Test 7: semctl(3) setup");
                return(1);
        }
	return 0;
}

/* ARGSUSED */
int
test7(int iterations)
{
	/* parent doesn't do anything */
	return 0;
}

void
test7_child(int iterations, key_t key, int idx, int shid, int semid)
{
	int	i, id;
	void	*addr;
	struct test_result *test_info;
	struct sembuf sembuf;
	struct timeval	start_time;
	struct timeval	end_time;
	struct timeval	delta_time;

	/* wait for parent */
	sembuf.sem_num = 0;
	sembuf.sem_op = -1;
	sembuf.sem_flg = 0;
	if (semop(semid, &sembuf, 1) < 0) {
		perror("test7_child: semop");
		exit(1);
	}

	if ((test_info = shmat(shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test7_child: shmat");
		exit(1);
	}

	if (test_info[idx].pid != getpid()) {
		perror("test7_child: test_info wrong");
		exit(1);
	}

	gettimeofday(&start_time);

	for (i = 0; i < iterations; i++) {
		if ((id = shmget(key, 4096, 0777)) < 0) {
			fprintf(stderr, "test7_child:%d key %d error %s\n",
				getpid(), key, strerror(errno));
			exit(1);
		}
		if ((addr = shmat(id, (void *) 0, 0)) == (void *)-1L) {
			perror("test7_child: shmat");
			exit(1);
		}
		shmdt(addr);
	}
	gettimeofday(&end_time);
	deltatime(&start_time, &end_time, &delta_time);
	test_info[idx].time = delta_time;

	exit(0);
}

void
test7_cleanup(struct timeval *delta)
{
	int	i;
	int	shid;

	delta->tv_sec = 0;
	delta->tv_usec = 0;

	while (wait(0) >= 0 || errno == EINTR)
		;

	if ((shid = shmget(Key7, 4096, 0777)) < 0) {
		perror("Test 7: shmget cleanup");
	} else if (shmctl(shid, IPC_RMID, 0) < 0) {
		perror("Test 7: shmctl Key7 RMID");
	}

	for (i = 0; i < nprocs; i++) {
		long	usecs;

		delta->tv_sec += (test_retval[i].time.tv_sec/nprocs);
		usecs = (test_retval[i].time.tv_sec%nprocs) * 1000000 +
			test_retval[i].time.tv_usec;
		delta->tv_usec += (usecs/nprocs);
	}

	delta->tv_sec += (delta->tv_usec/1000000);
	delta->tv_usec %= 1000000;

	shmdt(test_retval);
	if (shmctl(test7_shid, IPC_RMID, 0) < 0) {
		perror("test7: shmctl");
	}
	if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
		perror("Test 7: semctl(2) setup");
}

int test8_shid;

int
test8_setup(int iter)
{
	int	i, nbytes;
	union semun	semarg;
	cell_t curcell = 0;

	nbytes = (int)sizeof(struct test_result) * nprocs;
	if ((test8_shid = shmget(IPC_PRIVATE, nbytes, IPC_CREAT|0777)) < 0) {
		perror("test8_setup: shmget");
		return(1);
	}
	if ((test_retval = shmat(test8_shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test8_setup: shmat");
		return(1);
	}
	bzero(test_retval, nbytes);

	if ((test_semid = semget(IPC_PRIVATE, 1, 0777)) < 0) {
		perror("Test 8: semget setup");
		return(1);
	}

	semarg.val = 0;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 8: semctl setup");
		semctl(test_semid, IPC_RMID, 0);
		return(1);
	}

	for (i = 0; i < nprocs; i++) {
		pid_t	pid;

		pid = fork();
		if (pid < 0) {
			perror("Test 8: fork");
			semctl(test_semid, 0, IPC_RMID, 0);
			return(1);
		}
		if (pid == 0) {		/* child */
			char niterations[32];
			char nshid[32];
			char nkey[32];
			char nsemid[32];
			char nidx[32];
			char *sargv[20];
			int j;

			sprintf(niterations, "-i %d", iter);
			sprintf(nshid, "-I %d", test8_shid);
			sprintf(nkey, "-k %d", Key8 + getpid());
			sprintf(nsemid, "-s %d", test_semid);
			sprintf(nidx, "-x %d", i);
			j = 0;
			sargv[j++] = "shm3";
			sargv[j++] = "-C";
			sargv[j++] = "-t 8";
			sargv[j++] = niterations;
			sargv[j++] = nshid;
			sargv[j++] = nkey;
			sargv[j++] = nsemid;
			sargv[j++] = nidx;
			sargv[j] = NULL;
			if (ncells == 0)
				execvp(Argv0, sargv);
			else {
				if (mustrun)
					sysmp(MP_MUSTRUN, curcell);
				rexecvp(curcell, Argv0, sargv);
			}
			errprintf(ERR_ERRNO_EXIT, "exec failed");
			/* NOTREACHED */
		}
		/*
		 * Parent
		 */
		if (ncells && (++curcell >= ncells))
			curcell = 0;
		test_retval[i].pid = pid;
		test_retval[i].key = Key8 + pid;
	}
	semarg.val = nprocs;
	if (semctl(test_semid, 0, SETVAL, semarg) < 0) {
		perror("Test 8: semctl(2) setup");
                if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
			perror("Test 8: semctl(2) setup");
                return(1);
        }
	return 0;
}

void
test8_child(int iterations, key_t key, int idx, int shid, int semid)
{
	int	i, id;
	struct test_result *test_info;
	struct sembuf sembuf;
	struct timeval	start_time;
	struct timeval	end_time;
	struct timeval	delta_time;

	/* wait for parent */
	sembuf.sem_num = 0;
	sembuf.sem_op = -1;
	sembuf.sem_flg = 0;
	if (semop(semid, &sembuf, 1) < 0) {
		perror("test8_child: semop");
		exit(1);
	}

	if ((test_info = shmat(shid, (void *) 0, 0)) == (struct test_result *)-1L) {
		perror("test8_child: shmat");
		exit(1);
	}

	if (test_info[idx].pid != getpid()) {
		perror("test8_child: test_info wrong");
		exit(1);
	}

	gettimeofday(&start_time);

	for (i = 0; i < iterations; i++) {
		if ((id = shmget(key, 4096, IPC_CREAT|0777)) < 0) {
			perror("Test 8: shmget");
			exit(1);
		}
		if (shmctl(id, IPC_RMID, 0) < 0) {
			perror("Test 8: shmctl");
			exit(1);
		}
	}
	gettimeofday(&end_time);
	deltatime(&start_time, &end_time, &delta_time);
	test_info[idx].time = delta_time;

	exit(0);
}

/* ARGSUSED */
int
test8(int iterations)
{
	/* parent doesn't do anything */
	return 0;
}

void
test8_cleanup(struct timeval *delta)
{
	int	i;
	long long usec;

	delta->tv_sec = 0;
	delta->tv_usec = 0;

	while (wait(0) >= 0 || errno == EINTR)
		;

	usec = 0LL;
	for (i = 0; i < nprocs; i++) {
		/*
		printf("Pid %d %dS %duS\n", test_retval[i].pid,
				test_retval[i].time.tv_sec,
				test_retval[i].time.tv_usec);
		*/
		usec += test_retval[i].time.tv_usec;
		usec += (test_retval[i].time.tv_sec * 1000000);
	}
	usec /= nprocs;

	delta->tv_sec = usec / 1000000;
	delta->tv_usec = usec % 1000000;

	shmdt(test_retval);
	if (shmctl(test8_shid, IPC_RMID, 0) < 0) {
		perror("test8: shmctl");
	}
	if (semctl(test_semid, 0, IPC_RMID, 0) < 0)
		perror("Test 8: semctl(2) setup");
}


int
run_test(
	int	testno,
	int	iterations)
{
	struct timeval	start_time;
	struct timeval	end_time;
	struct timeval	delta_time;
	int		err;

	setup[testno - 1](iterations);

	gettimeofday(&start_time);

	err = tests[testno - 1](iterations);

	gettimeofday(&end_time);

	deltatime(&start_time, &end_time, &delta_time);

	cleanup[testno - 1](&delta_time);

	print_time(test_names[testno - 1], &delta_time, iterations);

	return(err);
}

void
print_time(
	char		*string,
	struct timeval *time,
	int		iterations)
{
	struct timeval	iter_time;
	float		time1;
	float		time2;

	time_divide(time, iterations, &iter_time);

	time1 = time->tv_sec + ((float)time->tv_usec / 1000000);
	time2 = iter_time.tv_sec + ((float)iter_time.tv_usec / 1000000);
	printf("%s: %f s elapsed, %f per iteration\n",
		string, time1, time2);
}

void
time_divide(
	struct timeval	*time,
	int		div,
	struct timeval	*result)
{
	long	ex_secs;

	ex_secs = time->tv_sec % div;
	result->tv_sec = time->tv_sec / div;
	result->tv_usec = (time->tv_usec + (ex_secs * 1000000)) / div;
}

void
deltatime(
	struct timeval	*time1,
	struct timeval	*time2,
	struct timeval	*result)
{
	result->tv_usec = time2->tv_usec - time1->tv_usec;
	if (result->tv_usec < 0) {
		time2->tv_sec--;
		result->tv_usec += 1000000;
	}
	result->tv_sec = time2->tv_sec - time1->tv_sec;      
}

void *
shmalloc(
	int	nbytes)
{
	int	shared_id;
	void	*addr;

	if ((shared_id = shmget(SharedKey, nbytes, IPC_CREAT|0777)) < 0) {
		perror("shmalloc: shmget");
		return((void *)0);
	}
	if ((addr = shmat(shared_id, (void *) 0, 0)) == (void *)-1L) {
		perror("shmalloc: shmat");
		return((void *)0);
	}
	if (shmctl(shared_id, IPC_RMID, 0) < 0) {
		perror("shmalloc: shmctl");
		shmdt(addr);
		return((void *)0);
	}
	return(addr);
}

void
shmfree(
	void	*addr)
{
	shmdt(addr);
}
