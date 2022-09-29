
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_cmd.c tests the rate that Unix commands run at. */

#include "unixperf.h"
#include "test_cmd.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

static char tmpdir[1024];
static char filename[200];
static char digits[] = "0123456789";
static char dc_command[] = "500 k 2 v p q";

Bool
InitCompress(Version version)
{
    FILE *file;
    int i;

    sprintf(tmpdir, "%s/unixperf-%d", TmpDir, getpid());
    rc = mkdir(tmpdir, 0755);
    SYSERROR(rc, "mkdir");
    rc = chdir(tmpdir);
    SYSERROR(rc, "chdir");
    file = fopen("data", "w");
    if(file == NULL) {
        return FALSE;
    }
    OSIsrandom(23123);
    for(i=0;i<1000;i++) {
        fprintf(file, "%d %d %d %d %d\n",
            OSIrandom(), OSIrandom(), OSIrandom(), OSIrandom(), OSIrandom());
    }
    fclose(file);
    return TRUE;
}

unsigned int
DoCompress(void)
{
    system("/usr/bsd/compress data ; /usr/bsd/uncompress data");
    return 1;
}

void
CleanupCompress(void)
{
    rc = unlink("data");
    SYSERROR(rc, "unlink");
    rc = chdir("/");
    SYSERROR(rc, "chdir");
    rc = rmdir(tmpdir);
    SYSERROR(rc, "rmdir");
}

Bool
InitCompile(Version version)
{
    FILE *file;

    sprintf(tmpdir, "%s/unixperf-%d", TmpDir, getpid());
    rc = mkdir(tmpdir, 0755);
    SYSERROR(rc, "mkdir");
    rc = chdir(tmpdir);
    SYSERROR(rc, "chdir");
    file = fopen("foo.c", "w");
    if(file == NULL) {
	return FALSE;
    }
    fprintf(file, "\n#include <stdio.h>\n\n");
    fprintf(file, "int main(int argc, char *argv[])\n");
    fprintf(file, "{\n  printf(\"hello world\\n\");\n  return 0;\n}\n");
    fclose(file);
    return TRUE;
}

#if 0  /* Parallel version below. */
unsigned int
DoDcSqrt2(void)
{
  int p[2], devnull, status;
  pid_t pid;
  int rc;

  devnull = open("/dev/null", O_WRONLY);
  rc = pipe(p);
  SYSERROR(rc, "pipe");
  pid = fork();
  SYSERROR(pid, "fork");
  if (pid == 0) {
    close(p[1]);
    close(0);
    close(1);
    close(2);
    fcntl(p[0], F_DUPFD, 0);
    close(p[0]);
    fcntl(devnull, F_DUPFD, 1);
    fcntl(devnull, F_DUPFD, 2);
    close(devnull);
    execl("/usr/bin/dc", "dc", (char *)0);
    _exit(127);  /* POSIX.2 value */
  }
  close(p[0]);
  close(devnull);
  write(p[1], dc_command, sizeof(dc_command));
  close(p[1]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      TestProblemOccured = 1;
    }
  }
  return 1;
}
#endif

static void
spawnMultiDcs(int num)
{
  int p[2], devnull, status;
  pid_t pid;
  int rc;

  devnull = open("/dev/null", O_WRONLY);
  for(; num>0; num--) {
    rc = pipe(p);
    SYSERROR(rc, "pipe");
    pid = fork();
    SYSERROR(pid, "fork");
    if (pid == 0) {
      close(p[1]);
      close(0);
      close(1);
      close(2);
      fcntl(p[0], F_DUPFD, 0);
      close(p[0]);
      fcntl(devnull, F_DUPFD, 1);
      fcntl(devnull, F_DUPFD, 2);
      close(devnull);
      execl("/usr/bin/dc", "dc", (char *)0);
      _exit(127);  /* POSIX.2 value */
    }
    close(p[0]);
    write(p[1], dc_command, sizeof(dc_command));
    close(p[1]);
  }
  close(devnull);
}

static void
waitForDcs(int num)
{
  int status;
  
  for(; num>0; num--) {
    while (wait(&status) < 0) {
      if (errno != EINTR) {
        TestProblemOccured = 1;
      }
    }
  }
}

unsigned int
DoDcSqrt2x1(void)
{
  int num = 1;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcSqrt2x2(void)
{
  int num = 2;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcSqrt2x4(void)
{
  int num = 4;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcSqrt2x8(void)
{
  int num = 8;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcSqrt2x12(void)
{
  int num = 12;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcSqrt2x16(void)
{
  int num = 16;

  spawnMultiDcs(num);
  waitForDcs(num);
  return num;
}

unsigned int
DoDcFactorial(void)
{
  static char dc_command[] = "[la1+dsa*pla1000>y]sy 0sa1 lyx";
  int p[2], devnull, status;
  pid_t pid;
  int rc;

  devnull = open("/dev/null", O_WRONLY);
  rc = pipe(p);
  SYSERROR(rc, "pipe");
  pid = fork();
  SYSERROR(pid, "fork");
  if (pid == 0) {
    close(p[1]);
    close(0);
    close(1);
    close(2);
    fcntl(p[0], F_DUPFD, 0);
    close(p[0]);
    fcntl(devnull, F_DUPFD, 1);
    fcntl(devnull, F_DUPFD, 2);
    close(devnull);
    execl("/usr/bin/dc", "dc", (char *)0);
    _exit(127);  /* POSIX.2 value */
  }
  close(p[0]);
  close(devnull);
  write(p[1], dc_command, sizeof(dc_command));
  close(p[1]);
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      TestProblemOccured = 1;
    }
  }
  return 1;
}

unsigned int
DoCompile(void)
{
    system("/usr/bin/cc -o foo foo.c");
    return 1;
}

void
CleanupCompile(void)
{
    rc = unlink("foo.c");
    SYSERROR(rc, "unlink");
    rc = unlink("foo");
    SYSERROR(rc, "unlink");
    rc = chdir("/");
    SYSERROR(rc, "chdir");
    rc = rmdir(tmpdir);
    SYSERROR(rc, "rmdir");
}

