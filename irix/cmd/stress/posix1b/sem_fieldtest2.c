/*
 * sem_fieldtest2:
 *
 * This program reads a file set-up by sem_fieldtest (/usr/tmp/sem_field.shm)
 * and makes sure the size of the semaphore structure and its field offsets
 * are what they are supposed to be.
 *
 * These values should be the same for all ABIs
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

main() {
	int fd;
	smem_template_t *sz;
	char abi_type[50];


#if _MIPS_SIM == _MIPS_SIM_ABI32
	sprintf(abi_type, "O32 alignment test");
#endif

#if _MIPS_SIM == _MIPS_SIM_NABI32
	sprintf(abi_type, "N32 alignment test");
#endif

#if  _MIPS_SIM == _MIPS_SIM_ABI64
	sprintf(abi_type, "N64 alignment test");
#endif

	printf("sem_fieldtest2: %s...\n", abi_type);

	fd = shm_open ("/usr/tmp/sem_field.shm", O_RDWR , 0666);
	if (fd == -1) {
		printf("sem_fieldtest2: %s missing file\n", abi_type);
		printf("sem_fieldtest2: you must run sem_fieldtest first\n");
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

	if (sz->sem_size != sizeof(sem_t)) {
		printf("sem_fieldtest2: %s alignment error - sem size %d:%d\n",
		       sz->sem_size, sizeof(sem_t));
		exit(1);
	}

	if (sz->sem_trace_size != sizeof(sem_trace_t)) {
		printf("sem_fieldtest2: %s alignment error - sem trace %d:%d\n",
		       sz->sem_trace_size, sizeof(sem_trace_t));
		exit(1);
	}

	if (sz->off_value != offsetof(sem_t, sem_value)) {
		printf("sem_fieldtest2: %s alignment error - sem value %d:%d\n",
		       sz->off_value, offsetof(sem_t, sem_value));
		exit(1);
	}

	if (sz->off_rlevel != offsetof(sem_t, sem_rlevel)) {
		printf("sem_fieldtest2: %s alignment error - sem rlevel %d:%d\n",
		       sz->off_rlevel, offsetof(sem_t, sem_rlevel));
		exit(1);
	}

	if (sz->off_flags != offsetof(sem_t, sem_flags)) {
		printf("sem_fieldtest2: %s alignment error - sem flags %d:%d\n",
		       sz->off_flags, offsetof(sem_t, sem_flags));
		exit(1);
	}

	if (sz->off_xtrace != offsetof(sem_t, sem_xtrace)) {
		printf("sem_fieldtest2: %s alignment error - sem xtrace %d:%d\n",
		       sz->off_xtrace, offsetof(sem_t, sem_xtrace));
		exit(1);
	}

	if (sz->off_usptr != offsetof(sem_t, sem_usptr)) {
		printf("sem_fieldtest2: %s alignment error - sem usptr %d:%d\n",
		       sz->off_usptr, offsetof(sem_t, sem_usptr));
		exit(1);
	}

	if (sz->off_pfd != offsetof(sem_t, sem_pfd)) {
		printf("sem_fieldtest2: %s alignment error - sem pfd %d:%d\n",
		       sz->off_pfd, offsetof(sem_t, sem_pfd));
		exit(1);
	}

	if (sz->off_opid != offsetof(sem_t, sem_opid)) {
		printf("sem_fieldtest2: %s alignment error - sem opid %d:%d\n",
		       sz->off_opid, offsetof(sem_t, sem_opid));
		exit(1);
	}

	if (sz->off_otid != offsetof(sem_t, sem_otid)) {
		printf("sem_fieldtest2: %s alignment error - sem otid %d:%d\n",
		       sz->off_otid, offsetof(sem_t, sem_otid));
		exit(1);
	}

	if (sz->off_dev != offsetof(sem_t, sem_dev)) {
		printf("sem_fieldtest2: %s alignment error - sem dev %d:%d\n",
		       sz->off_dev, offsetof(sem_t, sem_dev));
		exit(1);
	}

	printf("sem_fieldtest2: %s PASSED\n", abi_type);
	exit(0);
}
