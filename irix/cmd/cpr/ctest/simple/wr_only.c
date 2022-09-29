#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define FILE_NAME "temp"

void
on_checkpoint()
{
	printf("\t*** wr_only: SIGCKPT received ***\n");
}


main()
{
  pid_t ctest_pid = getppid();
  FILE * test_file;
  signal(SIGCKPT, on_checkpoint);
  if ((test_file = fopen(FILE_NAME, "w")) == NULL)
    {
      perror("wr_only: fopen");
      exit(1);
    }
  pause();
  fclose(test_file);
  unlink(FILE_NAME);
  printf("\t*** wr_only: Done ***\n");
  kill(ctest_pid, SIGUSR1);  
}
