
/*
 * Usage: scratch delay
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>

void
on_ckpt()
{;}

main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  int fd1, fd2, fd3, mfd;
  char buf[32];
  int i;
  void *addr;
  sigset (SIGCKPT, on_ckpt);

#define	MAX 4

 
  fd1 = open("file1", O_CREAT|O_RDWR, S_IRWXU);
  fd2 = open("file2", O_CREAT|O_RDWR, S_IRWXU);
  fd3 = open("file3", O_CREAT|O_RDWR, S_IRWXU);
  mfd = open("mfile", O_CREAT|O_RDWR, S_IRWXU);
  
  
  if (fd1 == -1 || fd2 == -1 || fd3 == -1 || mfd == -1) 
    {
      perror("open");
      return (-1);
    }
  
  for (i = 0; i < MAX; i++) 
    {
      write(fd1, "hello!\n\0", 8);
    write(fd2, "123456789\n\0", 11);
    write(fd3, "stock is down, world\n\0", 22);
    }
#ifndef NO
  addr = mmap(NULL, 1024, (PROT_READ | PROT_WRITE), MAP_PRIVATE, mfd, 0);
  printf("\n\t*** mapf: ready for CPR (mapaddr=0x%x) ***\n\n", addr);
#endif
  close(mfd);  
  printf("before\n");
  pause();
  printf("after\n");
/*  printf("%s", (char *) addr);*/
  close(fd1);
  close(fd2);
  close(fd3);
  
  unlink("file1");
  unlink("file2");
  unlink("file3");
  unlink("mfile");
  kill(ctest_pid, SIGUSR1);
  printf("\n\t*** mapf: Done ***\n\n");
}












