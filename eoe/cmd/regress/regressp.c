#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <malloc.h>
#include "regressp.h"


typedef struct namelist {
    char *name;
    struct namelist *next;
} NameList;

NameList *dirListHead = NULL;
NameList *testListHead = NULL;
NameList *failedTestsHead = NULL;
NameList *failedTestsTail = NULL;

int num_succ = 0;
int num_fail = 0;
int num_not_config = 0;

int num_dirs = 0;
int num_not_dirs = 0;

int verboseFlag = 0;
int logFlag = 0;
int rootFlag = 0;
int noExecuteFlag = 0;

char *root = NULL;

void enter_directory(char *path);
void cleanup_environ();

extern int errno;

FILE *logfile = stdout;

/*
 * new_env_variable - Generic routine to create an environment variable.  Does
 * some checking based on tst_typ to ensure that value exists.  Also, if
 * use_root is true, then the value of $ROOT is prepended to the path.
 */
void
new_env_variable(char *var, char *value, int vrbs, int tst_typ, int use_root)
{
    char *buf;
    struct stat tmp;

    if (use_root) {
	buf = (char *)malloc(strlen(value) + strlen(root) + 1);
	sprintf(buf, "%s%s", root, value);
	value = buf;
    }
    
    if (tst_typ == TEST_FOR_DIR) {
	if (stat(value, &tmp)) {
	    if (mkdir(value, 0777)) {
		fprintf(logfile, "%s: %s\n", value, strerror(errno));
		exit(-1);
	    }
	}
    } else if (tst_typ == TEST_FOR_EXEC) {
	if (stat(value, &tmp)) {
	    /* scripts may rely on these.  if file does not exist...exit */
	    fprintf(logfile, "%s: %s\n", value, strerror(errno));
	    exit(-1);
	}
	if (!(tmp.st_mode & S_IXUSR || tmp.st_mode & S_IXGRP ||
	      tmp.st_mode & S_IXOTH)) {
	    /* file is not executable...exit */
	    fprintf(logfile, "\"%s\" is not marked executable...exiting.\n",
		    value);
	    exit(-1);
	}
    }
    if ((buf = (char *)malloc(strlen(var) + strlen(value) + 1)) == NULL) {
	fprintf(stderr, "Out of memory.\n");
	exit(-1);
    }
    sprintf(buf, "%s%s", var, value);
    putenv(buf);
    if (vrbs)
	fprintf(logfile, "%s\n", buf);
    if (rootFlag)
	free(value);
    /* free(buf); */  /* XXX - should do this? */
}

/*
 * terminate - signal handler
 */
void
terminate(int sig)
{
    if (sig == SIGINT)
	fprintf(logfile, "\n\nCaught SIGINT...cleaning up then aborting.\n\n");
    else
	fprintf(logfile, "\n\nCaught SIGQUIT...cleaning up then aborting.\n\n");
    cleanup_environ();
    exit(-1);
}

/*
 * setup_environ - Routine to get everything into a runnable state including
 * setting up the signal handler and creating the appropriate environment
 * variables.
 */
void
setup_environ()
{
    char *buf, *tmpdir;
    FILE *fp;
    struct sigaction sigact;
    int mod_tmp;
    time_t t;

    /* set up signal handling */
    sigact.sa_flags = 0;
    sigact.sa_handler = terminate;
    bzero((void *)&sigact.sa_mask, sizeof(sigact.sa_mask));

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    /* TESTSAVE */
    if ((buf = (char *)malloc(strlen(TESTSAVE) + 6 + 1)) == NULL) {
	fprintf(stderr, "Out of memory.\n");
	exit(-1);
    }
    sprintf(buf, "%s/%d", TESTSAVE, getpid());
    new_env_variable("TESTSAVE=", buf, verboseFlag, TEST_FOR_DIR, rootFlag);
    if (logFlag) {
	if ((buf = realloc(buf, strlen(buf) + 4 + 1)) == NULL) {
	    fprintf(stderr, "Out of memory.\n");
	    exit(-1);
	}
	sprintf(buf, "%s/LOG", buf);
	if ((logfile = fopen(buf, "w")) == NULL) {
	    fprintf(logfile, "%s: %s\n", buf, strerror(errno));
	    fprintf(logfile, "Resetting logfile to stdout.\n");
	    logfile = stdout;
	} else
	    fprintf(stderr, "INFO: Logfile in %s\n", buf);
    }
    free(buf);
    fprintf(logfile, "Save directory:\n");
    if (rootFlag)
	fprintf(logfile, "TESTSAVE=%s%s/%d\n", root, TESTSAVE, getpid());
    else
	fprintf(logfile, "TESTSAVE=%s/%d\n", TESTSAVE, getpid());

    /* TMPDIR */
    /* First, try to create a directory under TMPDIR.  If TMPDIR is not
     * defined, use our own directory for it.
     */
    if (!(tmpdir = getenv("TMPDIR"))) {
	tmpdir = TMPDIR;
	mod_tmp = 1;  /* Allow value of $ROOT to be prepended to tmpdir */
    } else
	mod_tmp = 0;  /* Do not allow $ROOT to be prepended to tmpdir */
    fprintf(logfile, "\nTemporary directory:\n");
    if ((buf = (char *)malloc(strlen(tmpdir) + 6 + 1)) == NULL) {
	fprintf(stderr, "Out of memory.\n");
	exit(-1);
    }
    sprintf(buf, "%s/%d", tmpdir, getpid());
    new_env_variable("TMPDIR=", buf, verboseFlag, TEST_FOR_DIR, mod_tmp & rootFlag);
    free(buf);

    fprintf(logfile, "\n");

    /* Do we want to make these be (potentially) user-configurable? */
    if (verboseFlag)
	fprintf(logfile, "Standardization Scripts:\n");
    new_env_variable("ERROR=", ERROR, verboseFlag, TEST_FOR_EXEC, rootFlag);
    new_env_variable("WARNING=", WARNING, verboseFlag, TEST_FOR_EXEC, rootFlag);
    new_env_variable("INFO=", INFO, verboseFlag, TEST_FOR_EXEC, rootFlag);
    new_env_variable("SHELL=", "/sbin/sh", verboseFlag, TEST_FOR_EXEC, 0);

    fprintf(logfile, "\n");

    t = time((time_t)0);
    if ((buf = asctime(localtime(&t)))) {
	    fprintf(logfile, "Starting tests at: %s\n", buf);
    } else
	fprintf(logfile, "Starting tests.\n");
}

/*
 * cleanup_environ - Routine to print out final status after all of the tests
 * have been executed.  Also, clean out TMPDIR and TESTSAVE (if empty).
 */
void
cleanup_environ()
{
    char *buf;
    FILE *fp;
    char *dir;
    time_t t;

    t = time((time_t)0);
    if ((buf = asctime(localtime(&t)))) {
	    fprintf(logfile, "Tests completed at: %s", buf);
    } else
	fprintf(logfile, "Tests completed.\n");

    fprintf(logfile,"\n");
    fprintf(logfile,"-----------------------------------------------------\n");
    fprintf(logfile,"Number of successful tests:         %4d\n",num_succ);
    fprintf(logfile,"Number of failed tests:             %4d\n",num_fail);
    fprintf(logfile,"Number of unconfigured tests:       %4d\n",num_not_config);
    fprintf(logfile,"-----------------------------------------------------\n");
    fprintf(logfile,"Number of directories entered:      %4d\n",num_dirs);
    fprintf(logfile,"Number of unconfigured directories: %4d\n",num_not_dirs);
    fprintf(logfile,"-----------------------------------------------------\n");

    if (num_fail) {
	NameList *tmp = failedTestsHead;

	fprintf(logfile, "\nFailed tests:\n");
	while (tmp) {
	    fprintf(logfile, "\t%s\n", tmp->name);
	    tmp = tmp->next;
	}
    }
    
    if (system(NULL)) {
	if (verboseFlag)
	    fprintf(logfile, "\nCleaning out TMPDIR...\n");
	dir = getenv("TMPDIR");
	if ((buf=(char *)malloc(strlen("rm -rf /* /.* 2>/dev/null 1>/dev/null")
				+ 2*strlen(dir) + 1)) == NULL) {
	    fprintf(stderr, "Out of memory.\n");
	    exit(-1);
	}
	sprintf(buf, "/bin/rm -rf %s 2>/dev/null 1>/dev/null", dir);
	/* XXX - should be careful if this turns into suid program */
        /* not exactly correct...but it is rm's fault */
	if (system(buf) < 0)
	    fprintf(logfile,"Error cleaning TMPDIR...must be done manually.\n");
	else
	    if (verboseFlag)
		fprintf(logfile, "...done cleaning TMPDIR\n");
	free(buf);

	if (dir = getenv("TESTSAVE"))
	    rmdir(dir);   /* don't care if this succeeds or fails */
    } else
	fprintf(logfile,"\nError cleaning TMPDIR...must be done manually.\n");
    
    /* close to flush out anything that might be remaining */
    fclose(logfile);
}

/*
 * execute_tests - This is called with either the pathname of each test
 * or just the test name (in which case, we can assumed we are in the
 * correct directory) we think we can run.  We first look at the "conf"
 * file to determine if we should run this test.  If the conf file
 * doesn't exist or it returns a zero value, run the test.  Grab all of
 * the output from the test and put it in the log file and print out
 * whether success or failure by reading the test's return value.
 */
void
execute_test(char *path)
{
    char *buf;
    struct stat tmp;
    int conf_exist = 1;
    int retval = 0;
    FILE *fp;
    char msg[1024];
    char prog[80] = { NULL, };
    char *p, savedir[1024] = { NULL, };

    /* If bogus path, return immediately */
    if (path == NULL)
	return;

/*********
6/23/94 NEW
**********/
	if ((p=strrchr(path, '/')) == NULL) {
		/* path _is_ program */
		strcpy(prog, "./");
		strcat(prog, path);
	}
	else {
		if (getcwd(savedir, sizeof(savedir)) == NULL) {
			fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    		fprintf(logfile, "%s: Test `%s'\n", notConfigMsg, path);
			return;
		}

		/* in case it is path with '/' at end */
		if ( !*(p+1) ) {
	    		fprintf(logfile, "%s: Test `%s'\n", notConfigMsg, path);
			return;
		}
		strcpy(prog, "./");
		strcat(prog, p+1);
		*p = '\0';
		if (chdir(path) == -1) {
			fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    		fprintf(logfile, "%s: Test `%s'\n", notConfigMsg, path);
			return;
		}
	}

    /* Create the name of the configuration file... */
    if ((buf = (char *)malloc(strlen(prog) + strlen(".conf") + 1)) == NULL) {
	fprintf(stderr, "Out of memory.\n");
	exit(-1);
    }
    sprintf(buf, "%s.conf", prog);

    /* Check for existence of the .conf file */
    if (stat(buf, &tmp)) {
	/* No problem...path.conf doesn't exist or is not readable. */
	conf_exist = 0;
    }

    /* If the conf file exists, and it is executable by somebody, run it. */
    if (!noExecuteFlag && conf_exist &&
		(tmp.st_mode & S_IXUSR || tmp.st_mode & S_IXGRP ||
		       tmp.st_mode & S_IXOTH)) {
	if ((fp = popen(buf, "r")) == NULL) {
	    /* Can't run the conf file; return */
	    fprintf(logfile, "%s: %s\n", buf, strerror(errno));
	    fprintf(logfile, "%s: Test `%s'\n", notConfigMsg, path);
	    free(buf);
	    goto done1;
	} else {
	    /* Running the conf file; print out all output to the log file */
	    while (fgets(msg, 1024, fp) != NULL)
		fprintf(logfile, "%s", msg);
	    retval = pclose(fp);
	}
    }
    free(buf);

    /* Depending on the return value from the conf file (0 by default)... */
    if (retval) {
	/* Test not configured; print message and increment counter */
	fprintf(logfile, "%s: Test `%s'\n", notConfigMsg, path);
	num_not_config++;
    } else if (!noExecuteFlag) {
	/* run the test */
	fprintf(logfile, "\n%s: %s\n", startMsg, prog);

	if ((fp = popen(prog, "r")) == NULL) {
	    fprintf(logfile, "%s: %s\n", prog, strerror(errno));
	    goto done1;
	} else
	    while (fgets(msg, 1024, fp) != NULL)
		fprintf(logfile, "%s", msg);

	/* Print final status based on return value from the test. */
	if (pclose(fp)) {
	    /* Keep track of failed tests to print out at end. */
	    NameList *tmp = (NameList *)malloc(sizeof(*tmp));

	    if (tmp == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(-1);
	    }

	    tmp->name = strdup(path);
	    tmp->next = NULL;

	    /* Want to keep list in order so keep two pointers and append new
	     * elements to the end of the list.
	     */
	    if (failedTestsTail == NULL) {
		failedTestsHead = failedTestsTail = tmp;
	    } else {
		failedTestsTail->next = tmp;
		failedTestsTail = tmp;
	    }
	    fprintf(logfile, "%s: %s\n\n", failureMsg, path);
	    num_fail++;
	} else {
	    fprintf(logfile, "%s: %s\n\n", successMsg, prog);
	    num_succ++;
	}
    }
done1:
	if (*savedir && chdir(savedir) == -1) {
		perror("chdir");
		fprintf(stderr, "Unable to chdir back to %s\n", savedir);
		exit(-1);
	}
}

#define ARRAYCHUNK 64
/*
 * read_test_file - This routine creates a data structure identical to that
 * returned by scandir except that the names for each entry come from fd
 * instead of from a directory.  The return value is the number of items on
 * the list or -1 if an error occurred.
 */
int
read_test_file(FILE *fd, struct dirent *(*namelist[]))
{
    char test_name[256];
    int arraysz = ARRAYCHUNK;
    struct dirent *p, **names, **tmp;
    char *cp1, *cp2;
    int num_ents = 0;
    int i;

    /* Create an array of dirent pointers. */
    names = (struct dirent **)malloc(arraysz * sizeof(p));
    if (names == NULL)
	return -1;

    /* Read each item from the file and put it into the array */
    while (fgets(test_name, 256, fd)) {
	p = (struct dirent *)malloc(sizeof(*p) + strlen(test_name) + 1);
	if (p == NULL)
	    goto freeall;
	for (cp1 = p->d_name, cp2 = test_name; *cp2; ) {
	    if (*cp2 == '\n') {
		cp2++;
		continue;
	    }
	    *cp1++ = *cp2++;
	}
	*cp1 = NULL;
	/* If we run out of space, grab some more and copy everything over */
	if (num_ents >= arraysz) {
	    arraysz += ARRAYCHUNK;
	    tmp = (struct dirent **)malloc(arraysz * sizeof(p));
	    if (tmp == NULL)
		goto freeall;
	    bcopy(names, tmp, num_ents * sizeof(p));
	    free(names);
	    names = tmp;
	}
	names[num_ents++] = p;
    }
    *namelist = names;
    return(num_ents);
  
freeall:
    for (i = 0; i < num_ents; i++)
	free(names[i]);
    free(names);
    return -1;
}

/*
 * select_files - Routine called by scandir to "weed" out certain files we
 * know that  we don't want.
 */
int
select_files(struct dirent *dirent)
{
    /* just in case... */
    if (!dirent)
	return 0;

    /* don't want dot files including "." and ".." */
    if (dirent->d_name[0] == '.')
	return 0;

    /* don't want CONF file */
    if (strcmp(dirent->d_name, "CONF") == 0)
	return 0;

    /* don't want INFO file */
    if (strcmp(dirent->d_name, "INFO") == 0)
	return 0;

    /* don't want TEST_FILES file */
    if (strcmp(dirent->d_name, "TEST_FILES") == 0)
	return 0;

    /* don't want .conf files */
    if (strstr(dirent->d_name, ".conf") != NULL)
	return 0;

    /* don't want .info files */
    if (strstr(dirent->d_name, ".info") != NULL)
    return 0;

    /* everything else should be okay */
    return 1;
}

/*
 * execute_directory - Once in a given directory, we need to determine what
 * to do with everything in the directory.  First look for a TEST_FILES file.
 * If it exists, then it contains all of the tests for the directory and we
 * need look no further.  Otherwise, make a call to scandir to find out
 * everything in the directory, using select_files() to help cull out the
 * garbage.  Next, go through each entry and determine if it is a directory,
 * a test (i.e. is executable) or is something else.  Discard any regular files,
 * immediately recurse into any subdirectories, and save executables for the
 * next loop.  The next loop goes through the each entry still in the array and
 * calls execute_test with the full path name.
 */
void
execute_directory(char *path)
{
    int num_ents, i;
    struct dirent **entries;
    char *new_path;
    struct stat tmp;
    FILE *fd;
    static dirDepth = 0;

    if (verboseFlag) {
	dirDepth++;
	fprintf(logfile, "%s: ", changeDirMsg);
	for (i=0; i<dirDepth; i++)
	    fprintf(logfile, "-");
	fprintf(logfile, "> %s\n", path);
    }

    new_path = (char *)malloc(strlen("TEST_FILES") + 1);
    sprintf(new_path, "%s", "TEST_FILES");

    /* Check to see if the TEST_FILES exists.  If yes, then read the test file
     * to find all of the files (XXX and directories) we need to worry about.
     * If no, then call scandir on the directory.
     */
    if (fd = fopen(new_path, "r")) {
	free(new_path);
	if ((num_ents = read_test_file(fd, &entries)) == -1) {
	    fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    fclose(fd);
	    return;
	}
	fclose(fd);
    } else if ((num_ents = scandir(".", &entries, select_files, alphasort))
	       == -1) {
	fprintf(logfile, "%s: %s\n", path, strerror(errno));
	free(new_path);
	return;
    } else
	free(new_path);
  
    /* Go through each entry and determine if a directory.  If so, recurse
     * with new path.  Free upon return and set pointer to NULL.  Prevents
     * extra work during next loop.
     */
    for (i = 0; i < num_ents; i++) {
	new_path = (char *)malloc(strlen(entries[i]->d_name) + 2);
	if (new_path == NULL) {
	    fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    return;
	}
	sprintf(new_path, "%s", entries[i]->d_name);
	if (stat(new_path, &tmp)) {
	    fprintf(logfile, "%s: %s\n", new_path, strerror(errno));
	    free(new_path);
	    free(entries[i]);
	    entries[i] = NULL;
	    continue;
	}
	if (S_ISDIR(tmp.st_mode)) {
	    enter_directory(new_path);
	    free(entries[i]);
	    entries[i] = NULL;
	}
	/* Any file that we might want to execute has to have at least one
	 * executable bit set... check and if we don't find it, remove it.
	 */
	if (!(tmp.st_mode & S_IXUSR || tmp.st_mode & S_IXGRP ||
	      tmp.st_mode & S_IXOTH)) {
	    /* non-executable regular file (such as .info), remove from list */
	    free(entries[i]);
	    entries[i] = NULL;
	}
	/* Annoying that we have to free this, only to recalculate it again
	   right below here.  If only struct dirent had a char * instead... */
	free(new_path);
    }

    /* Go through each entry again.  This time though, everything we find
     * is a test so call execute_test with the full path name.
     */
    for (i = 0; i < num_ents; i++) {
	if (entries[i] == NULL)
	    continue;
	new_path = (char *)malloc(strlen(entries[i]->d_name) + 2);
	if (new_path == NULL) {
	    fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    return;
	}
	sprintf(new_path, "%s", entries[i]->d_name);
	execute_test(new_path);
	free(new_path);
	free(entries[i]);
    }

    free(entries);

    if (verboseFlag) {
	fprintf(logfile, "%s: <", changeDirMsg);
	for (i=0; i<dirDepth; i++)
	    fprintf(logfile, "-");
	fprintf(logfile, " %s\n", path);
	dirDepth--;
    }
}

/*
 * enter_directory - This routine checks a given directory to see if, based on
 * the CONF file, we should execute the tests therein.
 */
void
enter_directory(char *path)
{
    char buf[] = "./CONF";
    struct stat tmp;
    int conf_exist = 1;
    int retval = 0;
    FILE *fp;
    char msg[1024];
    char savedir[1024];

    /* If we are given a bogus path, return immediately. */
    if (path == NULL)
	return;

/*********
6/23/94 NEW
**********/
	if (getcwd(savedir, sizeof(savedir)) == NULL) {
		fprintf(logfile, "%s: %s\n", path, strerror(errno));
		return;
	}
	if (chdir(path) == -1) {
		fprintf(logfile, "%s: %s\n", path, strerror(errno));
		return;
	}

    /* Grab information about the CONF file */
    if (stat(buf, &tmp)) {
	/* Oops!  Either the CONF file doesn't exist, or we can't read it.  If
	 * it doesn't exist, no problem, but if we can't access the directory,
	 * then we have a problem.
	 */
	if (stat(".", &tmp)) {
	    /* Can't access the directory...print out the error and return */
	    fprintf(logfile, "%s: %s\n", path, strerror(errno));
	    goto done2;
	}
	/* no problem...CONF doesn't exist or is not readable. */
	conf_exist = 0;
    }

    /* If the CONF file exists and it is executable, try to run it. */
    if ( !noExecuteFlag && conf_exist &&
		(tmp.st_mode & S_IXUSR || tmp.st_mode & S_IXGRP ||
		       tmp.st_mode & S_IXOTH)) {
	if ((fp = popen(buf, "r")) == NULL) {
	    fprintf(logfile, "%s: %s\n", buf, strerror(errno));
	    fprintf(logfile, "%s: Directory `%s'\n", notConfigMsg, path);
	    goto done2;
	} else {
	    /* Print out all of the messages we get back from it to the log. */
	    while (fgets(msg, 1024, fp) != NULL) {
		fprintf(logfile, "%s", msg);
		}
	    retval = pclose(fp);
	}
    }

    /* If the return value from the CONF file was non-zero, return.  Otherwise
     * execute this directory.
     */
    if (retval) {
	fprintf(logfile, "%s: Directory `%s'\n", notConfigMsg, path);
	num_not_dirs++;
    } else {
	execute_directory(path);
	num_dirs++;
    }
done2:
	if (chdir(savedir) == -1) {
		perror("chdir");
		fprintf(stderr, "Unable to chdir back to %s\n", savedir);
		exit(-1);
	}
}

/*
 * main - Parse options, set log file, then run through either the given tests
 * and directories or start at the default.
 */
void
main(int argc, char **argv)
{
    int c, errFlag = 0;
    extern char *optarg;
    NameList *tmp;
    int alternate_log = 0;
    char *path;

    while ((c = getopt(argc, argv, "d:t:vrhlL:n")) != EOF)
	switch(c) {
	    case 'd':
		if ((tmp = (NameList *)malloc(sizeof(*tmp))) == NULL) {
		    fprintf(stderr, "Out of memory.\n");
		    exit(-1);
		}
		tmp->name = optarg;
		tmp->next = dirListHead;
		dirListHead = tmp;
		break;
	    case 't':
		if ((tmp = (NameList *)malloc(sizeof(*tmp))) == NULL) {
		    fprintf(stderr, "Out of memory.\n");
		    exit(-1);
		}
		tmp->name = optarg;
		tmp->next = testListHead;
		testListHead = tmp;
		break;
	    case 'v':
		verboseFlag = 1;
		break;
	    case 'r':
		rootFlag++;
		if ((root = getenv("ROOT")) == NULL) {
		    rootFlag = 0;
		    fprintf(stderr, "$ROOT undefined...ignoring -r.\n\n");
		}
		break;
	    case 'h':
		errFlag++;
		break;
	    case 'l':
		if (alternate_log++)
		    errFlag++;
		else
		    logFlag = 1;
		break;
	    case 'L':
		if (alternate_log++)
		    errFlag++;
		else
		    if ((logfile = fopen(optarg, "w")) == NULL) {
			logfile = stdout;
			fprintf(logfile, "%s: %s\n", optarg, strerror(errno));
			fprintf(logfile, "Resetting logfile to stdout.\n");
		    }
		break;
	    case 'n':
		noExecuteFlag++;
		break;
	    case '?':
		errFlag++;
		break;
	}

    if (errFlag) {
	fprintf(stderr, "Usage: %s [-d<dir>] [-t<testfile>] [-vrhn] [-l | -L<logfile>]\n", argv[0]);
	exit(-1);
    }

    setup_environ();

    if (dirListHead || testListHead) {
	while (dirListHead != NULL) {
	    if (rootFlag) {
		path = malloc(strlen(root) + strlen(dirListHead->name) + 2);
		if (dirListHead->name[0] == '/')
		    sprintf(path, "%s%s", root, dirListHead->name);
		else
		    sprintf(path, "%s/%s", root, dirListHead->name);
		enter_directory(path);
		free(path);
	    } else
		enter_directory(dirListHead->name);
	    tmp = dirListHead;
	    dirListHead = dirListHead->next;
	    free(tmp);
	}
	while (testListHead != NULL) {
	    if (rootFlag) {
		path = malloc(strlen(root) + strlen(testListHead->name) + 2);
		if (testListHead->name[0] == '/')
		    sprintf(path, "%s%s", root, testListHead->name);
		else
		    sprintf(path, "%s/%s", root, testListHead->name);
		enter_directory(path);
		free(path);
	    } else
		execute_test(testListHead->name);
	    tmp = testListHead;
	    testListHead = testListHead->next;
	    free(tmp);
	}
    } else {
	/* just do a regular directory descent from default location */
	if (rootFlag) {
	    path = malloc(strlen(root) + strlen(DEFAULT_DIR) + 2);
	    if (DEFAULT_DIR[0] == '/')
		sprintf(path, "%s%s", root, DEFAULT_DIR);
	    else
		sprintf(path, "%s/%s", root, DEFAULT_DIR);
	    enter_directory(path);
	    free(path);
	} else
	    enter_directory(DEFAULT_DIR);
    }

    cleanup_environ();
    exit(num_fail);
}
