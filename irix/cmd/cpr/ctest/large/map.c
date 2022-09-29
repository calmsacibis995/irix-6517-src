
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void on_ckpt() {;}


main()
{
	int fd;
	void *address;
	pid_t ctest_pid = getppid();
	signal(SIGCKPT, on_ckpt);
	if ((fd = open("./FILE", O_CREAT|O_RDWR, S_IRWXU)) == -1)
	{
		perror("large");
		exit(1);
	}
	write(fd, "stock is up, world\n\0", 20); 
	if ((int) (address = mmap(NULL, 16, (PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0)) == -1)
	{
		perror("large");
		exit(1);
	}
	printf("address=%x\n", address);
	pause();
	kill(ctest_pid, SIGUSR1);
	printf("map: Done\n");
}
