#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "stress.h"
#include <sys/procfs.h>
#include <sys/pmo.h>

/*
 * mlocker.c
 * test program for mlock, munlock, mlockall, munlockall
 */

/*
 * mlock,munlock
 *	 memory should remain locked until unlocked, exec or exit occurs
 *	 if the pages are locked by another process, they should not
 *	 be affected in the other process, when munlocked
 */

#define TRUE 1
#define FALSE 0

#define MEM_SIZE (5*1024*1024)
#define NPMS 5		/* 5 policy modules - 1Mb per */
#define PMSEGSIZE (1024*1024)
pmo_handle_t pmos[NPMS];
policy_set_t ps;

int ignore_handler = FALSE;
int recursive = 0;
#define SWAP	 	0x01
#define LOCKALL 	0x04
#define LOCKER	 	0x08
#define PLOCKT	 	0x10
#define FUTURE	 	0x20
#define GROW	 	0x40
#define STACK		0x80

void *cp;
int pagesize;
int page;
size_t mem_size = MEM_SIZE;
char c;
int verbose;
int usepm;	 /* use policy modules to set page sizes */

void lock_test(void);
void swap_test(void);
void lockall_test(void);
void plock_test(void);
void future_test(int);
void autogrow_test(int);
void stack_test(void);
int stack_grow(void);
void sig_handler(int, int);
int cntlocked(pid_t pid);
void slave(void *, size_t);
void doit(int);
char *Cmd;
char *tname, *tname2;
pid_t mypid;
int numpages(caddr_t, size_t);

main(int argc, char *argv[])
{
	int todo = 0;
	int i, parse;
	int doshared = 0;
	struct sigaction act;
	sigset_t set;

	Cmd = errinit(argv[0]);
	while ((parse = getopt(argc, argv, "lSvm:kfrapsgi")) != EOF)
		switch (parse) {
		case 'S':
			doshared = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'm':
			mem_size = atoi(optarg);
			break;
		case 's':
			todo |= SWAP;
			break;
		case 'r':
			recursive = 1;
			break;
		case 'a':
			todo |= LOCKALL;
			break;
		case 'p':
			todo |= PLOCKT;
			break;
		case 'f':
			todo |= FUTURE;
			break;
		case 'g':
			todo |= GROW;
			break;
		case 'k':
			todo |= STACK;
			break;
		case 'i':
			ignore_handler = TRUE;
			break;
		case 'l':
			usepm = 1;
			break;
		default:
			printf ("usage: %s [-m memory_size] [-l][-s] [-r] [-a] [-p] [-f] [-g] [-k]\n",argv[0]);
			exit(1);
		}

	if (todo == 0)
		todo = LOCKER|LOCKALL|SWAP|PLOCKT|STACK|GROW|FUTURE;

	tname = tempnam(NULL, "mlocker");
	tname2 = tempnam(NULL, "mlocker2");
	pagesize = getpagesize();

	/* round mem_size up to even page boundary */
	mem_size = (mem_size + pagesize - 1) / pagesize;
	if (mem_size & 0x1)
		mem_size++;
	mem_size *= pagesize;

	if (!ignore_handler) {
		act.sa_flags = 0;
		act.sa_handler = sig_handler;
		sigemptyset(&set);
		act.sa_mask = set;
		sigaction(SIGSEGV, &act, NULL);
	}
	if (usepm) {
		pm_filldefault(&ps);
		ps.page_size = 4096;
		for (i = 0; i < NPMS; i++) {
			ps.page_size *= 4;
			if ((pmos[i] = pm_create(&ps)) == -1) {
				if (errno == ENOSYS) {
					printf("%s:no PM in system\n", Cmd);
					break;
				} else {
					errprintf(ERR_ERRNO_EXIT,
						"pm_create i:%d", i);
					/* NOTREACHED */
				}
			}
		}
	}

	if (doshared) {
		sprocsp(slave, PR_SALL, (void *)(__psint_t)todo, NULL, 32*1024*1024);
		while (wait(NULL) >= 0 || errno != ECHILD)
			errno = 0;

	} else {
		doit(todo);
	}

	return 0;
}

void
doit(int todo)
{
	mypid = getpid();
	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & LOCKER)
		lock_test();

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & LOCKALL)
		lockall_test();

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & SWAP)
		swap_test();

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & PLOCKT)
		plock_test();

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & FUTURE) {
		future_test(0);
		future_test(1);
	}

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & GROW) {
		autogrow_test(0);
		/*
		autogrow_test(1);
		*/
	}

	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
	if (todo & STACK)
		stack_test();
	if (verbose)
		printf("brk @ 0x%x\n", sbrk(0));
}

/* ARGSUSED */
void
slave(void *a, size_t len)
{
	int todo = (int)(__psint_t)a;
	doit(todo);
}

void
sig_handler(int sig, int code)
{
	if (sig == SIGSEGV && code == ENOMEM) {
		printf ("SIGSEGV signal caught: unable to lockdown memory\n");
		munlockall();
	}
	else
		printf ("unknown signal ... sig = %d  code = %d\n", sig, code);

	exit(1);
}

void
swap_test(void)
{
	cp = malloc(mem_size);
	if (cp == NULL) {
		errprintf(ERR_EXIT, "malloc failed");
		/* NOTREACHED */
	}

	/* touch all pages */
	for (page = 0; page < (mem_size/pagesize); page ++) {
		((char *)cp)[page *pagesize] = '0';
	}
	free(cp);
}

void
lock_test(void)
{
	int i, x, nlocked;

	cp = malloc(mem_size);
	if (cp == NULL) {
		errprintf(ERR_EXIT, "malloc failed");
		/* NOTREACHED */
	}
	if (usepm) {
		char *ncp = cp;
		for (i = 0; i < NPMS; i++, ncp += PMSEGSIZE) {
			if (pm_attach(pmos[i], ncp, PMSEGSIZE) != 0) {
				if (errno == ENOSYS) {
					printf("%s:no PM in system\n", Cmd);
					break;
				} else {
					errprintf(ERR_ERRNO_EXIT,
						"lock_test:pm_attach: i %d\n",
						i);
					/* NOTREACHED */
				}
			}
		}
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	if (mlock(cp, mem_size)) {
		if (errno == EAGAIN) {
			printf("%s:lock_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			goto out;
		} else {
			errprintf(ERR_ERRNO_EXIT, "mlock failed");
			/* NOTREACHED */
		}
	}
	if ((nlocked = cntlocked(mypid)) != numpages(cp, mem_size)) {
		errprintf(ERR_EXIT, "wrong lockcnt is %d should be %d",
				nlocked, mem_size/pagesize);
		/* NOTREACHED */
	}

	if (recursive) {
		for (x=0; x < 10; x++) {
			if (mlock(cp, mem_size)) {
				errprintf(ERR_ERRNO_EXIT, "recursive mlock failed iter %d:", x);
				/* NOTREACHED */
			}
		}
	}
	if ((nlocked = cntlocked(mypid)) != numpages(cp, mem_size)) {
		errprintf(ERR_EXIT, "wrong lockcnt is %d should be %d",
				nlocked, mem_size/pagesize);
		/* NOTREACHED */
	}

	if (munlock(cp, mem_size)) {
		errprintf(ERR_ERRNO_EXIT, "munlock failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}
out:
	free(cp);
}

void
lockall_test(void)
{
	int fd, fd2;
	void *addr, *addr2;
	int x, heappages, baselocked, totpages, nlocked;

	/*
	 * alas, if some other tests have already run, our
	 * heap might already be allocated and therefore show
	 * up in the 'baselocked' ..
	 */
	if (mlockall(MCL_CURRENT)) {
		if (errno == EAGAIN) {
			printf("%s:lockall_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			return;
		} else {
			errprintf(ERR_ERRNO_EXIT, "mlockall failed");
			/* NOTREACHED */
		}
	}
	baselocked = cntlocked(mypid);
	if (munlockall()) {
		errprintf(ERR_ERRNO_EXIT, "munlockall failed");
		/* NOTREACHED */
	}

	cp = malloc(mem_size/2);
	if (cp == NULL) {
		errprintf(ERR_EXIT, "malloc failed");
		/* NOTREACHED */
	}
	heappages = numpages(cp, mem_size/2);
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	fd = shm_open(tname, O_CREAT | O_RDWR, 666);
	ftruncate(fd, (off_t)mem_size/2);
	addr = mmap(0, mem_size/2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap failed");
		/* NOTREACHED */
	}
	totpages = numpages(addr, mem_size/2);
	shm_unlink(tname);

	/* Technically this memory doesn't exist, since it's mapped autogrow
	 * and isn't touched.  As a result, the call to mlockall will not lock
	 * down the addr2 shared memory.
	 */
	fd2 = shm_open(tname2, O_CREAT | O_RDWR, 666);
	addr2 = mmap(0, mem_size, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_AUTOGROW, fd2, 0);
	if (addr2 == MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap2 failed");
		/* NOTREACHED */
	}
	shm_unlink(tname2);

	if (recursive) {
		for (x = 0; x < 10; x++) {
			if (mlockall(MCL_CURRENT)) {
				if (errno == EAGAIN) {
					printf("%s:lockall_test1:WARNING:not enough memory to run test - skipping\n", Cmd);
					goto out;
				} else {
					errprintf(ERR_ERRNO_EXIT, "mlockall1 failed");
					/* NOTREACHED */
				}
			}
			nlocked = cntlocked(mypid);
			if ((nlocked != (baselocked + totpages)) &&
			    (nlocked != (baselocked + totpages + heappages))) {
				errprintf(ERR_EXIT, "wrong lockcnt is %d should be %d or %d",
					nlocked,
					baselocked + totpages,
					baselocked + totpages + heappages);
				/* NOTREACHED */
			}
		}
	} else {
		if (mlockall(MCL_CURRENT)) {
			if (errno == EAGAIN) {
				printf("%s:lockall_test:WARNING:not enough memory to run test - skipping\n", Cmd);
				goto out;
			} else {
				errprintf(ERR_ERRNO_EXIT, "mlockall failed");
				/* NOTREACHED */
			}
		}
		nlocked = cntlocked(mypid);
		if ((nlocked != (baselocked + totpages)) &&
		    (nlocked != (baselocked + totpages + heappages))) {
			errprintf(ERR_EXIT, "wrong lockcnt is %d should be %d or %d",
					nlocked,
					baselocked + totpages,
					baselocked + totpages + heappages);
			/* NOTREACHED */
		}
	}

	if (munlockall()) {
		errprintf(ERR_ERRNO_EXIT, "munlock2 failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

out:
	if (munmap(addr, mem_size/2) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr);
		/* NOTREACHED */
	}
	if (munmap(addr2, mem_size) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr2);
		/* NOTREACHED */
	}
	close(fd);
	close(fd2);
	free(cp);
}

void
plock_test(void)
{
	int fd;
	void *addr;
	int nlocked, ntextlocked;
	int totpages;

	cp = malloc(mem_size/2);
	if (cp == NULL) {
		errprintf(ERR_EXIT, "malloc failed");
		/* NOTREACHED */
	}
	totpages = numpages(cp, mem_size/2);
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	fd = shm_open(tname, O_CREAT | O_RDWR, 666);
	ftruncate(fd, (off_t)mem_size/2);
	addr = mmap(0, mem_size/2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap failed");
		/* NOTREACHED */
	}
	totpages += numpages(addr, mem_size/2);
	shm_unlink(tname);

	if (plock(TXTLOCK)) {
		if (errno == EAGAIN) {
			printf("%s:plock_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			goto out;
		} else {
			errprintf(ERR_ERRNO_EXIT, "plock1 failed");
			/* NOTREACHED */
		}
	}
	ntextlocked = cntlocked(mypid);

	if (plock(DATLOCK)) {
		if (errno == EAGAIN) {
			printf("%s:plock_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			goto out;
		} else {
			errprintf(ERR_ERRNO_EXIT, "plock2 failed");
			/* NOTREACHED */
		}
	}
	nlocked = cntlocked(mypid);
	if ((nlocked - ntextlocked) < totpages) {
		errprintf(ERR_EXIT, "only %d pages locked should be at least %d",
				nlocked-ntextlocked, totpages);
		/* NOTREACHED */
	}

	if (plock(UNLOCK)) {
		errprintf(ERR_ERRNO_EXIT, "plock3 failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	if (plock(PROCLOCK)) {
		if (errno == EAGAIN) {
			printf("%s:plock_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			goto out;
		} else {
			errprintf(ERR_ERRNO_EXIT, "plock4 failed");
			/* NOTREACHED */
		}
	}
	nlocked = cntlocked(mypid);
	if (nlocked < (totpages + ntextlocked)) {
		errprintf(ERR_EXIT, "only %d pages locked should be at least %d",
				nlocked, totpages + ntextlocked);
		/* NOTREACHED */
	}

	if (plock(UNLOCK)) {
		errprintf(ERR_ERRNO_EXIT, "plock5 failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

out:
	if (munmap(addr, mem_size/2) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr);
		/* NOTREACHED */
	}
	free(cp);
	close(fd);
}

void
future_test(int dochild)
{
	int fd, ch_fd;
	void *addr, *ch_addr, *ch_addr2;
	int nlocked, tlocked;
	pid_t pid, ppid;

	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		if (errno == EAGAIN) {
			printf("%s:future_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			return;
		} else {
			errprintf(ERR_ERRNO_EXIT, "mlockall FUTURE failed");
			/* NOTREACHED */
		}
	}

	if (dochild) {
		pid = fork();
		if (pid == 0) {
			pid = getpid();
			ppid = getppid();
			/* child shouldn't start w/ any pages locked */
			if ((nlocked = cntlocked(pid)) != 0) {
				errprintf(ERR_EXIT, "child already have locked pages! %d", nlocked);
				/* NOTREACHED */
			}
			ch_addr = malloc(mem_size/2);
			if (ch_addr == NULL) {
				errprintf(ERR_EXIT, "child malloc failed");
				/* NOTREACHED */
			}

			ch_fd = shm_open(tname2, O_CREAT | O_RDWR, 666);
			ftruncate(ch_fd, (off_t)mem_size/2);
			ch_addr2 = mmap(0, mem_size/2, PROT_READ | PROT_WRITE,
			    MAP_SHARED, ch_fd, 0);
			if (ch_addr2 == MAP_FAILED) {
				errprintf(ERR_ERRNO_EXIT, "child mmap failed");
				/* NOTREACHED */
			}

			/* let parent lock it all */
			unblockproc(ppid);
			blockproc(pid);

			if ((nlocked = cntlocked(pid)) != 0) {
				errprintf(ERR_EXIT, "child wrong lockcnt is %d should be 0",
						nlocked);
				/* NOTREACHED */
			}
			mlockall(MCL_CURRENT);
			munlockall();
			if ((nlocked = cntlocked(pid)) != 0) {
				errprintf(ERR_EXIT, "child wrong lockcnt2 is %d should be 0",
						nlocked);
				/* NOTREACHED */
			}
			unblockproc(ppid);
			exit(0);
		}
	}

	cp = malloc(mem_size/2);
	if (cp == NULL) {
		errprintf(ERR_EXIT, "malloc failed");
		/* NOTREACHED */
	}

	if (dochild) {
		/* wait till child sets up shm/mmap */
		blockproc(mypid);

		if ((fd = shm_open(tname2, O_RDWR, 0)) == -1) {
			errprintf(ERR_ERRNO_EXIT, "shm_open failed");
			/* NOTREACHED */
		}
	} else {
		if ((fd = shm_open(tname2, O_RDWR|O_CREAT, 0666)) == -1) {
			errprintf(ERR_ERRNO_EXIT, "shm_open failed");
			/* NOTREACHED */
		}
	}
	addr = mmap(0, mem_size/2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap failed");
		/* NOTREACHED */
	}
	shm_unlink(tname2);
	if ((nlocked = cntlocked(mypid)) <= (mem_size / pagesize)/2) {
		errprintf(ERR_EXIT, "parent should have at least %d pages locked but has %d",
			(mem_size / pagesize)/2, nlocked);
		/* NOTREACHED */
	}

	if (dochild) {
		/* let child play */
		unblockproc(pid);
		blockproc(mypid);
	}

	/* parent should still have all pages locked */
	if ((tlocked = cntlocked(mypid)) != nlocked) {
		errprintf(ERR_EXIT, "parent should have at %d pages locked but has %d",
			nlocked, tlocked);
		/* NOTREACHED */
	}

	if (munlockall()) {
		errprintf(ERR_ERRNO_EXIT, "munlockall failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}
	if (munmap(addr, mem_size/2) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr);
		/* NOTREACHED */
	}
	free(cp);
	close(fd);
}

void
autogrow_test(int future)
{
	int fd;
	void *addr;
	int nlocked, curlocked;
	int flags = MCL_CURRENT;

	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	if (future)
		flags |= MCL_FUTURE;

	if (mlockall(flags) < 0) {
		if (errno == EAGAIN) {
			printf("%s:autogrow_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			return;
		} else {
			errprintf(ERR_ERRNO_EXIT, "mlockall FUTURE failed");
			/* NOTREACHED */
		}
	}
	curlocked = cntlocked(mypid);

	if ((fd = shm_open(tname, O_CREAT | O_EXCL | O_RDWR, 666)) < 0) {
		shm_unlink(tname);
		fd = shm_open(tname, O_CREAT | O_EXCL | O_RDWR, 666);
	}
	addr = mmap(0, mem_size, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_AUTOGROW, fd, 0);
	if (addr == MAP_FAILED) {
		errprintf(ERR_ERRNO_EXIT, "mmap failed");
		/* NOTREACHED */
	}

	/* touch all pages */
	pagesize = getpagesize();
	for (page = 0; page < (mem_size/pagesize); page ++) {
		((char *)addr)[page *pagesize] = '0';
	}
	nlocked = cntlocked(mypid);
	if (future) {
		if ((nlocked - curlocked) != (mem_size/pagesize)) {
			errprintf(ERR_EXIT, "%d pages locked should be %d\n",
				nlocked - curlocked, mem_size/pagesize);
		}
	} else {
		/* not future so nothing should be locked */
		if (nlocked != curlocked) {
			errprintf(ERR_EXIT, "%d pages locked should be %d\n",
				nlocked, curlocked);
		}
	}

	if (munmap(addr, mem_size/2) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr);
		/* NOTREACHED */
	}
	nlocked = cntlocked(mypid);
	if (future) {
		if ((nlocked - curlocked) != (mem_size/pagesize)/2) {
			errprintf(ERR_EXIT, "%d pages locked should be %d\n",
				nlocked - curlocked, (mem_size/pagesize)/2);
		}
	} else {
		/* not future so nothing should be locked */
		if (nlocked != curlocked) {
			errprintf(ERR_EXIT, "%d pages locked should be %d\n",
				nlocked, curlocked);
		}
	}

	if (munlockall()) {
		errprintf(ERR_ERRNO_EXIT, "munlockall failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "still have locked pages! %d", nlocked);
		/* NOTREACHED */
	}

	shm_unlink(tname);
	close(fd);
	if (munmap((caddr_t)addr+mem_size/2, mem_size/2) != 0) {
		errprintf(ERR_ERRNO_EXIT, "munmap at 0x%x failed", addr);
		/* NOTREACHED */
	}
}

void
stack_test(void)
{
	int n, nlocked, curlocked;

	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		if (errno == EAGAIN) {
			printf("%s:stack_test:WARNING:not enough memory to run test - skipping\n", Cmd);
			return;
		} else {
			errprintf(ERR_ERRNO_EXIT, "mlockall CUR/FUT failed");
			/* NOTREACHED */
		}
	}
	curlocked = cntlocked(mypid);

	n = stack_grow();
	nlocked = cntlocked(mypid);
	/* 
	 * it's really hard to guarantee exactly how many pages are locked
	 * since some pages might hav already been locked and some
	 * additional pages might be locked due to pre-growth of stack
	 * Here we try to make sure the basic stuff is locked down
	 */
	if (((nlocked - curlocked) < n) || ((nlocked - curlocked) > (n+2))) {
		errprintf(ERR_EXIT, "%d pages locked should be between %d and %d\n",
			nlocked - curlocked, n, n+2);
	}

	if (munlockall()) {
		errprintf(ERR_ERRNO_EXIT, "munlockall failed");
		/* NOTREACHED */
	}
	if ((nlocked = cntlocked(mypid)) != 0) {
		errprintf(ERR_EXIT, "already have locked pages! %d", nlocked);
		/* NOTREACHED */
	}
}

int
stack_grow(void)
{
	/* this array makes sure we're at the deepest stack in all the tests */
	/* REFERENCED */
	char y[40*1024];

	/* REFERENCED */
	char x[MEM_SIZE];
	int n;

	y[40*1024-1] = 'a';

	/* REFERENCED */
	/* touch all pages */
	pagesize = getpagesize();
	n = numpages(x, MEM_SIZE);
	for (page = 0; page < n; page++) {
		x[page*pagesize] = '0';
	}
	if (verbose)
		printf("&x 0x%x &x[MEM_SIZE] 0x%x pages %d\n",
			x, &x[MEM_SIZE], n);
	return n;
}

int
cntlocked(pid_t pid)
{
	int i, fd, nmaps;
	char myproc[24];
	prmap_sgi_arg_t args;
	prmap_sgi_t prbuf[100];
	int locked = 0;

	sprintf(myproc, "/proc/%05d", pid);
	if ((fd = open(myproc, O_RDWR)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "open of %s", myproc);
		/* NOTREACHED */
	}

	args.pr_vaddr = (caddr_t)&prbuf;
	args.pr_size = sizeof(prbuf);

	if ((nmaps = ioctl(fd, PIOCMAP_SGI, &args)) < 0) {
		errprintf(ERR_ERRNO_EXIT, "PIOCMAP failed");
		/* NOTREACHED */
	}

	for (i = 0; i < nmaps; i++) {
		if (prbuf[i].pr_lockcnt)
		locked += prbuf[i].pr_size / pagesize;
	}
	close(fd);
	return locked;
}

int
numpages(caddr_t vaddr, size_t len)
{
	size_t t;

	t = ((__psunsigned_t)vaddr & (pagesize - 1)) + len;
	t = (t + (pagesize - 1)) / pagesize;
	return (int)t;
}
