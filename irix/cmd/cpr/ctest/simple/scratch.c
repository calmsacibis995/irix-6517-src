
/*
 * Usage: scratch delay
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#define	FILE1	"file1"
#define	FILE2	"file2"
#define	FILE3	"file3"

#define	MSG1	"hello!\0"
#define	MSG2	"123456789\0"
#define	MSG3	"stock is down, world\0"


void
on_checkpoint()
{
  printf("\n\t*** scratch: SIGCKPT received ***\n\n");
}

main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  int fd1, fd2, fd3;
  char buf[64];
  off_t off;
  int succeeded = 1;
  signal(SIGCKPT, on_checkpoint);
  fd1 = open(FILE1, O_CREAT|O_RDWR, S_IRWXU);
  fd2 = open(FILE2, O_CREAT|O_RDWR, S_IRWXU);
  fd3 = open(FILE3, O_CREAT|O_RDWR, S_IRWXU);
  
  if (fd1 == -1 || fd2 == -1 || fd3 == -1) 
    {
      perror("open");
      return (-1);
    }

	
  write(fd1, "HEADING", strlen("HEADING"));
  write(fd2, "HEADING", strlen("HEADING"));
  write(fd3, "HEADING", strlen("HEADING"));
  
  write(fd1, MSG1, strlen(MSG1));
  write(fd2, MSG2, strlen(MSG2));
  write(fd3, MSG3, strlen(MSG3));
  
  pause();
  
  if ((off = lseek(fd1, -7, SEEK_CUR)) == -1)
    perror("fd1 lseek");
  read(fd1, &buf, 7);
  printf("\n\t*** %s = %s ***\n", buf, MSG1);
  if (strcmp(buf, MSG1) != 0)
    {
      printf("\n\t*** scratch: fd1 comparison error after restart ***\n\n");
      succeeded = 0;
    }

  if (lseek(fd2, -10, SEEK_CUR) == -1)
    perror("fd2 lseek");
  read(fd2, &buf, 10);
  printf("\t*** %s = %s ***\n", buf, MSG2);
  if (strcmp(buf, MSG2) != 0)
    {
      printf("\n\t*** scratch: fd2 comparison error after restart ***\n\n");
      succeeded = 0;
    }
  
  if (lseek(fd3, -21, SEEK_CUR) == -1)
    perror("fd3 lseek");
  read(fd3, &buf, 21);
  printf("\t*** %s = %s ***\n", buf, MSG3);
  if (strcmp(buf, MSG3) != 0)
    {
      printf("\n\t*** scratch: fd3 comparison error after restart ***\n\n");
      succeeded = 0;
    }
  
  close(fd1);
  close(fd2);
  close(fd3);
  
  unlink(FILE1);
  unlink(FILE2);
  unlink(FILE3);
  printf("\n\t*** scratch: Done ***\n");
  if (succeeded)
    kill(ctest_pid, SIGUSR1);
  else
    kill(ctest_pid, SIGUSR2);
}
