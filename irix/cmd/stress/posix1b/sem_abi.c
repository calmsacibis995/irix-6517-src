#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/sem.h>
#include <unistd.h>

#define SEM_MAX    5
#define SEM_TARGET 3


int s_create = -1;
int s_post = 0;
int s_wait = 0;
int s_destroy = 0;

struct data_template {
	int slam1;
	sem_t sem[SEM_MAX];
	int slam2;
	int sem_size;
};

struct data_template *temp;


#if _MIPS_SIM == _MIPS_SIM_ABI32
char *abi_type = "MIPS O32";
#endif

#if _MIPS_SIM == _MIPS_SIM_NABI32
char *abi_type = "MIPS N32";
#endif

#if  _MIPS_SIM == _MIPS_SIM_ABI64
char *abi_type = "MIPS N64";
#endif

main(int argc, char *argv[])
{
	int parse;
	sem_t *semaphore;
	int fd;
	int x;
	int numints;
	
	while ((parse = getopt(argc, argv, "c:p:w:d")) != EOF)
		switch (parse) {
		case 'c':
			s_create = atoi(optarg);
			break;
		case 'd':
			s_destroy = 1;
			break;
		case 'p':
			s_post = atoi(optarg);
			break;
		case 'w':
			s_wait = atoi(optarg);
			break;
		default:
			printf("sem_abi -c # -p # -w # -d\n");
			exit(1);
		}

	if (s_create != -1) {
		printf("sem_abi: %s creating semaphore count=%d\n", abi_type, s_create);

		fd = shm_open("/usr/tmp/sem_mem.shm", O_CREAT|O_RDWR, 0666);
		if (fd == -1) {
			printf("sem_abi: FAILED %s shm_open", abi_type);
			exit(1);
		}

		if (ftruncate(fd, sizeof(struct data_template)) < 0) {
			printf("sem_abi: FAILED: %s ftruncate", abi_type);
			exit(1);
		}

		temp = (struct data_template *) mmap(0, sizeof(struct data_template),
						     PROT_READ|PROT_WRITE,
						     MAP_SHARED, fd, 0);

		if ((int) temp == -1) {
			printf("sem_abi: FAILED %s mmap", abi_type);
			exit(1);
		}
		
		numints = sizeof(struct data_template) / sizeof(int);
		for (x=0; x < numints; x++) {
			/*
			 * Zero out the memory to cause a failure if an
			 * uninitialized semaphore is referenced.
			 */
			((int*) temp)[x] = 0;
		}

		semaphore = &temp->sem[SEM_TARGET];

		if (sem_init(semaphore, 1, s_create) < 0) {
			printf("sem_abi: %s sem_init", abi_type);
			exit(1);
		}

		temp->sem_size = sizeof(sem_t);

	} else {
		fd = shm_open ("/usr/tmp/sem_mem.shm", O_RDWR , 0666);
		if (fd == -1) {
			printf("sem_abi: FAILED %s shm_open", abi_type);
			exit(1);
		}

		temp = (struct data_template *) mmap(0, sizeof(struct data_template),
						     PROT_READ|PROT_WRITE,
						     MAP_SHARED, fd, 0);
		if ((int) temp == -1) {
			printf("sem_abi: FAILED %s mmap", abi_type);
			exit(1);
		}

		semaphore = &temp->sem[SEM_TARGET];
	}

	if (temp->sem_size != sizeof(sem_t)) {
		printf("sem_abi: FAILED %s semaphore incompatibilty error (%d/%d)\n",
		       abi_type, temp->sem_size, sizeof(sem_t));
		exit(1);
	}
	
	if (s_post) {
		printf("sem_abi: %s posting semaphore %d times\n", abi_type, s_post);
		while (s_post--) {
			temp->slam1 = -1;
			if (sem_post(semaphore) < 0) {
				printf("sem_abi: %s sem_post", abi_type);
				exit(1);
			}
			temp->slam2 = -1024;
		}
	}

	if (s_wait) {
		printf("sem_abi: %s waiting on semaphore %d times\n", abi_type, s_wait);
		while (s_wait--) {
			temp->slam1 = -1;
			if (sem_wait(semaphore) < 0) {
				printf("sem_abi: FAILED %s sem_wait", abi_type);
				exit(1);
			}
			temp->slam2 = -1024;
		}
	}

	if (s_destroy) {
		int value;

		if (sem_getvalue(semaphore, &value) < 0) {
			printf("sem_abi: FAILED %s sem_getvalue", abi_type);
			exit(1);
		}

		printf("sem_abi: %s destroying semaphore (%d)\n", abi_type, value);
		(void) sem_destroy(semaphore);
		
		if (shm_unlink("/usr/tmp/sem_mem.shm") < 0) {
			perror("shm_unlink");
			exit(1);
		}
	}

	close(fd);
	return 0;
}
