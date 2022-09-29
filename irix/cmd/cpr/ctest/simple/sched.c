#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>

main(int argc, char **argv)
{
  int slice, new_slice = atol(argv[1]);
  
  if (new_slice == 0)
    new_slice = 1;
  printf("I got here.\n");
  if ((slice = schedctl(SLICE, 0, new_slice)) == -1) 
    {
      perror("schedctl SLICE");
      exit (1);
    }
  printf("I got past the schedctl!\n");
  printf("orig slice: %d\n", slice);
  pause();
  if ((slice = schedctl(SLICE, 0, slice)) == -1) 
    {
      perror("schedctl SLICE 2");
      exit (1);
    }
  printf("new slice: %d\n", slice);
}
