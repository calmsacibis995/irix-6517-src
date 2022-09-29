/*
 * sem_fieldtest:
 *
 * This simple program merely records the semaphore structure size
 * and field offsets in a file (/usr/tmp/sem_field.shm).
 *
 * This file is then used by sem_fieldtest2 to verify that the
 * various ABIs all agree on the size and offset values.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stddef.h>
#include <semaphore_internal.h>

typedef struct smem_template_s {
	int sem_size;
	int sem_trace_size;

	int off_value;
	int off_rlevel;
	int off_flags;
	int off_xtrace;
	int off_usptr;
	int off_pfd;
	int off_opid;
	int off_otid;
	int off_dev;
} smem_template_t;

main(int argc, char **argv)
{
	int fd;
	smem_template_t *sz;

	if (argc > 1) {
		shm_unlink("/usr/tmp/sem_field.shm");
		printf("sem_fieldtest: test file removed\n");
		exit(0);
	}

	fd = shm_open ("/usr/tmp/sem_field.shm", O_CREAT | O_RDWR , 0666);
	if (fd == -1) {
		perror("FAILED: shm_open");
		exit(1);
	}

	if (ftruncate(fd, sizeof(smem_template_t)) < 0) {
		perror("FAILED: ftruncate");
		exit(1);
	}

	sz = (smem_template_t *) mmap(0,
				      sizeof(smem_template_t),
				      PROT_READ|PROT_WRITE,
				      MAP_SHARED, fd, 0);
	
	if ((int) sz == -1) {
		perror("FAILED: mmap");
		exit(1);
	}

	sz->sem_size = sizeof(sem_t);
	sz->sem_trace_size = sizeof(sem_trace_t);
	sz->off_value = offsetof(sem_t, sem_value);
	sz->off_rlevel = offsetof(sem_t, sem_rlevel);
	sz->off_flags = offsetof(sem_t, sem_flags);
	sz->off_xtrace = offsetof(sem_t, sem_xtrace);
	sz->off_usptr = offsetof(sem_t, sem_usptr);
	sz->off_pfd = offsetof(sem_t, sem_pfd);
	sz->off_opid = offsetof(sem_t, sem_opid);
	sz->off_otid = offsetof(sem_t, sem_otid);
	sz->off_dev = offsetof(sem_t, sem_dev);

	printf("sem_fieldtest: semaphore size = %d\n", sz->sem_size);
	printf("sem_fieldtest: semaphore trace size = %d\n",
	       sz->sem_trace_size);

	close(fd);
	exit(0);
}
