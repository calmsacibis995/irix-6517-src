#include <errno.h>
#include <limits.h>
#include <ftw.h>
#include <unistd.h>

int f(const char *name,const struct stat *info,int type,struct FTW *S)
{
        return 0;
}

void main()
{
  char path[5012];
  int  i;
  int num_errors = 0;
  
  for (i = 0; i <= 255 ; i++)
    path[i] = 'd';
  path[++i] = '\0';

  errno = 0;
  if (nftw(path,f,2,FTW_MOUNT) != -1)
    printf("WARNING: -1 not returned.\n");
  printf("INFO: errno = %d (should be %d)\n", errno, ENAMETOOLONG);
  if (errno != ENAMETOOLONG)
    num_errors++;

  for (i = 0; i <= 1025 ; i++)
    path[i] = 'd';
  path[++i] = '\0';
  
  errno = 0;
  if (nftw(path,f,2,FTW_MOUNT) != -1)
    printf("WARNING: -1 not returned.\n");
  printf("INFO: errno = %d (should be %d)\n", errno, ENAMETOOLONG);
  if (errno != ENAMETOOLONG)
    num_errors++;

  exit(num_errors);
}
