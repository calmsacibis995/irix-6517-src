#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <signal.h>

/*
 * Simple sproc test case 1
 */
void *
stkaddr(void)
{
	int stkparam;

	return ((void *)&stkparam);
}

void
sproc_entry(void *arg)
{
	int fd1;
	char buf[20];
	int id = *(int *)arg;
	char fname[20];

	sleep(id);

	printf("\n\tsproc_many: in the sproc'd child:%d, my stk addr: %lx\n", id, stkaddr());

	sprintf(fname, "foo.%d", id);
	fd1 = open(fname, O_RDWR|O_CREAT, 0777);
        printf("\n\tsproc_many: child:%d fd1 %d\n", id, fd1);
	if (id & 1)
        	unlink(fname);
        write(fd1, "foo\n", 4);
        write(fd1, "bar\n\0", 5);
        lseek(fd1, 0, 0);
      
	sleep(20);

	printf("\n\tsproc_many: back in the sproc'd child:%d, my stk addr: %lx\n", id, stkaddr());

	read(fd1, buf, 20);
	printf("\n\tsproc_many: child:%d fd %d:%s\n", id, fd1, buf);
	unlink(fname);
	exit(0);
}

void on_ckpt() {;}

main()
{
	pid_t ctest_pid = getppid();
	int fd1;
	char buf[20];
	int args[6];
	int i;
	signal(SIGCKPT, on_ckpt);

	for (i = 0; i < 6; i++) {
		args[i] = i+1;
		sproc(sproc_entry, PR_SALL|PR_NOLIBC, (void *)&args[i]);
	}
	printf("\n\tsproc_many: in the parent, my stkaddr: %lx\n", stkaddr());

	fd1 = open("./bar", O_RDWR|O_CREAT, 0777);
        printf("\n\tsproc_many: parent fd1 %d\n", fd1);
        write(fd1, "hey\n", 4);
        write(fd1, "you\n\0", 5);
        lseek(fd1, 0, 0);
      
	pause();

	printf("\n\tsproc_many: back in the parent, my stkaddr: %lx\n", stkaddr());

	read(fd1, buf, 20);
	printf("\n\tsproc_many: parent fd %d:%s\n", fd1, buf);
	while (wait(&i) != -1 && errno != ECHILD);
	unlink("./bar");
	kill(ctest_pid, SIGUSR1);
	printf("\n\tsproc_many: Done\n");
}
