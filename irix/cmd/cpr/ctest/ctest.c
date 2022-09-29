/* This is just a cleaned-up version of 'ctest.c.works'. Both this code
and the 'ctest.c.works' code should be funcionally equivalent.  However,
if this code fails for any reason the 'ctest.c.works' code should at
least, as is advertised, work. */


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/procfs.h>
#include <ckpt.h>

/*
 * synopsis:	ctest cycles delay
 *
 * TEST_LIST synopsis:
 *		TYPE CMD
 *		e.g. PID simple/alarm
 *		     HID simple/tree
 */

#define	TEST_LIST		"TEST_LIST"
#define FAILED_LOG                "FAILED_LOG"
#define	CT_MIN_SLEEP_ALLOWED	3

#define	ISSPACE(c)	((c) == ' ' || (c) == '\t')
#define	ISEND(c)	((c) == '\n' || (c) == 0)

static void ct_start_each_test(char *, char *);

static int force_checkpoint;


FILE * failed_log;


void
main(int argc, char *argv[])
{
  int cycles = atoi(argv[1]);
  int delay = atoi(argv[2]);
  FILE *test_file;
  char line[256], path[256], type[16], cmd[256], fail_path[256];
  int i;
  char *pwd;
  if (!cycles)
    cycles++;

/* PWD is the working directory you are running from */
  if ((pwd = getenv("PWD")) != NULL) 
    {
      sprintf(path, "%s/%s", pwd, TEST_LIST);
      sprintf(fail_path, "%s/%s", pwd, FAILED_LOG);
    } 
  else 
    {
      perror("getenv");
      exit(1);
    }

  if ((test_file = fopen(path, "r")) == NULL)
    {
      printf("CTEST ERR: failed to open %s\n", path);
      perror("fopen");
      exit(1);
    }
  
  if ((failed_log = fopen(fail_path, "w+")) == NULL)
    {
      printf("CTEST ERR: failed to open %s\n", fail_path);
      printf("Proceeding without log...");
      perror("fopen");
    }
  else
      setbuf(failed_log, NULL);

  for (i = 0; i < cycles; i++) 
    {
      /*
       * Go through each test
       */
      rewind(test_file);
      while (fgets(cmd, 256, test_file)) 
	{
	  int n = 0;
	  int m = 0;
	  force_checkpoint = 0;
	  if (cmd[0] == '#')
	    continue;
	  
	  while (ISSPACE(cmd[n])) n++;
	  if (ISEND(cmd[n]))
	    continue;
	  
	  /*
	   * Get type
	   */
	  /*n = 0;*/
	  while (!ISSPACE(cmd[n]))
	    type[m++] = cmd[n++];
	  type[m] = 0;
	  
	  while (ISSPACE(cmd[n])) n++;
	  
	  /* remove \n */
	  while (!ISEND(cmd[m])) 
	    {
	      if (cmd[m] == '*')
		{
		  force_checkpoint = 1;  /* forces ckpt if test needs to be active */
		  cmd[m] = 0;
		}
	      else m++;
	    }
	  cmd[m] = 0;
	  
	  if (cmd[n] == '/')
	    ct_start_each_test(type, cmd+n);
	  else 
	    {
	      sprintf(line, "%s/%s", pwd, cmd+n);
	      ct_start_each_test(type, line);
	    }
	}
      sleep(delay);
    }
  printf("\n[CTEST] ALL TESTS FINISHED. CHECK FAILED_LOG FOR FAILED TESTS.\n");
  fclose (failed_log);
  fclose (test_file);
}

static int
ct_test_ready_for_cpr(int cpid)
{
  char proc[32];
  int procfd;
  int ready_count = 0;
  prstatus_t prstatus;
  
again:
  sprintf(proc, "/proc/%d", cpid);
  if ((procfd = open(proc, O_RDONLY)) == -1) 
    {
      perror("open");
      return (-1);
    }	
  if (ioctl(procfd, PIOCSTATUS, &prstatus) == -1) 
    {
      close(procfd);
      perror("ioctl");
      return (-1);
    }	
  close(procfd);

  if (prstatus.pr_flags & PR_STOPPED || prstatus.pr_flags & PR_ASLEEP) 
    {
      if (ready_count)
	return (1);
      
      ready_count++;
      sleep(CT_MIN_SLEEP_ALLOWED);
      goto again;
    }
  if (force_checkpoint) 
    {
      sleep(3 * CT_MIN_SLEEP_ALLOWED);
      printf("[CTEST] FORCE ACTIVE CHECKPOINT\n");
      return (1);
    }
  return (0);
}

int test_passed = -1;

void
success()
{test_passed = 1;}

void
failure()
{test_passed = 0;}

static void
ct_start_each_test(char *type, char *testcmd)
{
  int test_pid, cpr_pid;
  int stat1, stat2;
  char pid_str[16];
  int rc = 0;
  signal(SIGUSR1, success);
  signal(SIGUSR2, failure);
  sighold(SIGUSR1);
  sighold(SIGUSR2);
  printf("\n\n----------------------------------------------------------------\n\n");
  if ((test_pid = fork()) == 0) 
    {
      printf("\n[CTEST] STARTING TEST %s [PID %d]\n", testcmd, getpid());
      execl(testcmd, testcmd, 0);
      perror("execl");
      fprintf(stderr, "CTEST ERR: cannot execute test file %s\n", testcmd);
      exit(2);
    }
  /*
   * Let the test program fork and exec
   */
  while ((rc = ct_test_ready_for_cpr(test_pid)) == 0)
    sginap(0);
  
  if (rc == -1) 
    {
      printf("CTEST ERR: cannot find test process %s\n", testcmd);
      printf("CTEST ERR: continuing with the rest of the tests...\n");
      fprintf(failed_log, "%s\tFAILED before checkpoint\n", testcmd);
      return;
    }

  /*
   * Ready for checkpoint 
   */

  sprintf(pid_str, "%d:%s", test_pid, type);
  if ((cpr_pid = fork()) == 0) 
    {
      printf("\n[CTEST] ABOUT TO CHECKPOINT TEST %s\n", testcmd);
      execl(CPR, CPR, "-c", pid_str, "-p", pid_str, "-k", 0);
      perror("execl");
      exit(3);
    }
  cpr_pid = waitpid(cpr_pid, &stat2, 0);
  if ((WIFEXITED(stat2) == 0) || (WEXITSTATUS(stat2) != 0))        /* if CPR did not terminate
								      normally */
    {
      printf("CTEST ERR: cpr process exited abnormally\n");
      fprintf(failed_log, "%s\tFAILED; could not checkpoint\n", testcmd);
      return;
    }

  test_pid = waitpid(test_pid, &stat1, 0);
  if (WIFSIGNALED(stat1) == 0)       /* if the test didn't terminate
					due to a SIGCKPT */
    {
      printf("CTEST ERR: test program terminated abnormally\n");
      fprintf(failed_log, "%s\tFAILED; could not checkpoint\n", testcmd);
      return;
    }

  /*
   * Ready for restart 
   */

  printf("\n[CTEST] ABOUT TO RESTART TEST %s\n", testcmd);
  if ((cpr_pid = fork()) == 0)
    {
      execl(CPR, CPR, "-r", pid_str, 0);
      perror("execl");
      exit(3);
    }
  cpr_pid = waitpid(cpr_pid, &stat2, 0);
  if ((WIFEXITED(stat2) == 0) || (WEXITSTATUS(stat2) != 0))      /* if CPR did not terminate
				      normally */
    {
      printf("CTEST ERR: cpr process exited abnormally\n");
      printf("CTEST ERR: keeping statefile and continuing with rest of tests...\n");
      fprintf(failed_log, "%s\t FAILED on restart\n", testcmd);
      return;
    }
  
   printf("\n[CTEST] ABOUT TO REMOVE THE STATEFILE FOR TEST %s\n", testcmd);
  
  if ((cpr_pid = fork()) == 0) 
    {      
      execl(CPR, CPR, "-D", pid_str, 0);
      perror("execl");
      exit(3);
    } 
  cpr_pid = waitpid(cpr_pid, &stat2, 0);
  if (WIFEXITED(stat2) == 0)
    printf("CTEST ERR: cpr process exited abnormally\n");
  sigrelse(SIGUSR1);
  sigrelse(SIGUSR2);
  if (test_passed == -1)
    pause();
  if (test_passed)
    printf("\n[CTEST] TEST [%s] WAS SUCCESSFUL\n", testcmd);
  else
    {
      printf("\n[CTEST] TEST [%s] FAILED\n", testcmd);
      if (test_passed == 0)
	fprintf(failed_log, "%s\tFAILED test criteria\n", testcmd);
      else
	fprintf(failed_log, "%s\tFAILED to terminate normally\n", testcmd);
    }
  test_passed = -1;
}	
