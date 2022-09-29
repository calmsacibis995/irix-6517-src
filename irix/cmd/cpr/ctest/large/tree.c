
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/mman.h>

#define SIGRESTART 34

void start_child(int, int);
int count_tree(int, int);
int fd;
int count_returned = 0;
FILE * temp;

void
print_time(void)
{
	time_t t;

	time(&t);
	printf("\ttree: PID %d restart time: %s", getpid(), ctime(&t));
	fprintf(temp, "i");
}

main(int argc, char *argv[])
{
	int depth = atoi(argv[1]);
	int span = atoi(argv[2]);
	int stat;
	int count_initial;
	pid_t ctest_pid = getppid();
	pid_t root = getpid();
	temp = tmpfile();

	if (depth == 0)
		depth = 2;
	if (span == 0)
		span = 3;
	signal(SIGRESTART, print_time);
	count_initial = count_tree(depth, span);
	printf("\n\ttree: Initial # of Processes: %d\n\n", count_initial);
	printf("\ttree: PID %4d (depth, span)=(%d,%d) ROOT\n", root, depth, span);
	fd = open("parent_open", O_CREAT|O_RDWR, S_IRWXU);
	write(fd, "stock is down way down, world\n\0", 31);
	if (depth > 0 && span > 0 && depth < 200 && span < 200)
		start_child(--depth, span);
	wait(&stat);
	sleep(10);
	rewind(temp);
	while(getc(temp) != EOF) count_returned++;
	printf("\ttree: Processes Returned: %d\n", count_returned);
	if (count_returned == count_initial)
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
	printf("\n\ttree: Done\n");
	close(fd);
	unlink("parent_open");
}

void	
start_child(int depth, int span)
{	
	pid_t child;
	int stat;
	int i;
	
	for (i = 0; i < span; i++) {
		if ((child = fork()) == 0) {
			char path1[128], path2[128];
			int mfd;
			void *addr;
	
			close(fd);
			if (depth > 0)
				start_child(--depth, span);
	
			sprintf(path1, "openfile.%d", getpid());
			fd = open(path1, O_CREAT|O_RDWR, S_IRWXU);
			write(fd, "stock is down, world\n\0", 22);
	
			sprintf(path2, "mapfile.%d", getpid());
			mfd = open(path2, O_CREAT|O_RDWR, S_IRWXU);
			addr = mmap(NULL, 1024, (PROT_READ | PROT_WRITE), 
				MAP_PRIVATE, mfd, 0);
			write(fd, "stock is down, world\n\0", 22);
			close(mfd);
			sleep(20);
			close(fd);
			unlink(path1);
			unlink(path2);
			exit(0);
		}
		if (child < 0) {
			perror("fork");
			return;
		}
		printf("\ttree: PID %4d (depth, span)=(%d,%d)\n", child, depth, i);
	}
	wait(&stat);
}	
	
int	
count_tree(int d, int s)
{	
	int count = 0, i, j, mul;
	
	for (i = 0; i <= d; i++) {
		mul = 1;
		for (j = 0; j < i; j++)
			mul *= s;
		count += mul;
	}
	return (count);
}
