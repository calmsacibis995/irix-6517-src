/*
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

/* 
 * irixbtest.c -- main(), get_timestring(), w_sys_error(),
 *                w_func_error(), w_raw_log(), w_error(),
 *                w_sum_log(), flush_raw_log(), flush_sum_log()
 */

#include "irixbtest.h"
#define NUMCMDS         6   /* Number of cmd lines passed to system() */

/* 
 * These are command lines passed to system() to execute the utilities
 * date, uname, and hinv and incorporate their output into the log files. 
 */
char *cmdline[] = {
    "echo Test begun:    `date`  >> %s \n",
    "echo System Name:   `uname -n` >> %s \n",
    "echo OS Release:    `uname -r` >> %s \n",
    "echo OS Version:    `uname -v` >> %s \n",
    "echo Hardware Inventory: >> %s \n",
    "hinv >> %s \n" };

static char **enp;          /* Holds envp passed to main. */
static FILE *raw_fp;        /* Raw logfile pointer. */
static FILE *sum_fp;        /* Summary logfile pointer. */
static short elflag = 0;    /* Flag set if error log is requested. */
static char err_file[SLEN]; /* Name of error log file. */
static int  err_fd;         /* File descriptor to error log file */

short	acl_enabled;	/* TRUE if ACL configured */
short	audit_enabled;	/* TRUE if AUDIT configured */
short	cipso_enabled;	/* TRUE if SC_IP_SECOPTS configured */
short	mac_enabled;	/* TRUE if MAC configured */
short	cap_enabled;	/* TRUE if CAPabilites configured */
long    sys_config_mask; /* Mask of the above capabilities */

short skip_logs = 0;       /* Are we only debugging? */
short list_only = 0;        /* Display a list of tests only */

/*
 * Main uses getopt(3c) to parse the command line.  The options
 * -m, -f, and -s are mutually exclusive, and cause main() 
 * to call domenu(), dofile(), or dostrings(), respectively.
 * The purpose of the "do" functions is to queue for exection
 * the tests that the user wishes to run.
 * The -d flag causes irixbtest to skip actually running the tests.
 * The -e flag causes error messages to be written
 * to an error log file as well as to the screen.
 */
    
int
main(int argc, char **argv, char **envp)
{
    mac_t maclp;               /* MAC label pointer */
    struct stat statbuf;       /* For stat of current directory */
    register int c;            /* Command line option letter. */
    register short i;          /* Array index. */
    short usrerrflag = 0;      /* User error on cmd line. */
    short mflag = 0;           /* Menu requested. */
    short sflag = 0;           /* Tests are listed on cmd line. */
    short fflag = 0;           /* Tests are listed in a file. */    
    char *input_file;          /* File name specified with -f option. */
    char test_dir[SLEN];       /* Name of directory all tests run in.  */
    char raw_file[SLEN];       /* Name of raw log file.  */
    char sum_file[SLEN];       /* Name of summary log file.  */
    char fmt_file[SLEN];       /* Name of formatted log file.  */
    char str[MSGLEN];          /* Strings passed to various functions */
    int	capc;		       /* For capability acquiring / surrendering */
    cap_t ocap;		       /* For capability acquiring / surrendering */
 
    /* 
     * Setbuf null.
     */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    /* 
     * Verify system configuration dependant components first
     */
     
    /* 
     * If euid != superuser, print message and exit.
     */
    if ( geteuid() ) {
	w_error(GENERAL, err[UIDNOTSU], NULSTR, 0);
	exit(FAIL);
    }

    /* 
     * Check for TRIX components.
     */
    acl_enabled		= ( sysconf(_SC_ACL) > 0)? ACL : 0;
    audit_enabled	= ( sysconf(_SC_AUDIT) > 0)? AUDIT : 0;
    cipso_enabled	= ( sysconf(_SC_IP_SECOPTS) > 0)? CIPSO : 0;
    mac_enabled		= ( sysconf(_SC_MAC) > 0)? MAC : 0;
    cap_enabled		= ( (capc = sysconf(_SC_CAP)) > 0)? CAP : 0;

    /*
     * This is used to compare to tests' flags to see if
     *  the system has enough marbles to run that test 
     */
    sys_config_mask =
      	acl_enabled | audit_enabled | cipso_enabled | mac_enabled | cap_enabled;

#ifdef DEBUG
      Dbug(("main(): acl_enabled is %s\n", acl_enabled? "TRUE":"FALSE"));
      Dbug(("main(): audit_enabled is %s\n", audit_enabled? "TRUE":"FALSE"));
      Dbug(("main(): cipso_enabled is %s\n", cipso_enabled? "TRUE":"FALSE"));
      Dbug(("main(): mac_enabled is %s\n", mac_enabled? "TRUE":"FALSE"));
      Dbug(("main(): cap_enabled is %s\n", cap_enabled? "TRUE":"FALSE"));
#endif

#ifdef CAP_SU
    /*
     * Now, check for things only if the system knows how 
     */
      if (cap_enabled && capc < 2) {
	  w_error(GENERAL, err[NOESUSER], NULSTR, 0);
	  exit(FAIL);
      }
#endif

    /* 
     * Make create mask null.
     */
    umask(0);

    /* 
     * Current directory label must be MAC dominated by
     * all process labels. If not, print message and exit.
     */
    if (mac_enabled) {
	maclp = mac_get_file(".");
	if (maclp == NULL) {
		w_error(GENERAL, err[GETCDLABEL], NULSTR, 0);
		exit(FAIL);
	}
	if ( ( (maclp->ml_msen_type != MSEN_LOW_LABEL) && 
	       (maclp->ml_msen_type != MSEN_EQUAL_LABEL) ) ||
	     ( (maclp->ml_mint_type != MINT_HIGH_LABEL) && 
	       (maclp->ml_mint_type != MINT_EQUAL_LABEL) ) )  {
		w_error(GENERAL, err[BADCDLABEL], NULSTR, 0);
		exit(FAIL);
    	}
	free(maclp);
    }
    
    /* 
     * Current directory must be searchable and writeable by
     * all. If not, print message and exit.
     */
    if ( stat(".", &statbuf) < 0) {
	w_error(GENERAL, err[STATDIR], NULSTR, 0);
	exit(FAIL);
    }
    if (   ( (statbuf.st_mode&001) == 0) || 
         ( ( (statbuf.st_mode >>3)&001) == 0) ||
         ( ( (statbuf.st_mode >>6)&001) == 0) ||
	   ( (statbuf.st_mode&002) == 0) ||
	 ( ( (statbuf.st_mode >>3)&002) == 0) ||
	 ( ( (statbuf.st_mode >>6)&002) == 0) ) {
	w_error(GENERAL, err[BADCDMODE], NULSTR, 0);
	exit(FAIL);
    }

    /* 
     * Set process label to msenhigh, mintequal.
     */
    if ( mac_enabled ) {
	maclp = mac_get_proc();
	maclp->ml_msen_type = (unsigned char)MSEN_HIGH_LABEL;
	maclp->ml_mint_type = (unsigned char)MINT_EQUAL_LABEL;
	if ( cap_setplabel(maclp) ) {
	    w_error(GENERAL, err[F_SETPLABEL], NULSTR, 0);
	    exit(FAIL);
 	  }
	mac_free(maclp);
    }

    /*
     * Turn off all effective capabilities and only allow permitted - 
     * only enable effective capabilities as needed 
     */
    if ( cap_enabled ) {
	ocap = cap_get_proc();
	ocap->cap_effective = 0;
	if ( cap_set_proc(ocap) != 0 ) {
		w_error(GENERAL, err[CAPSETPROC], NULSTR, errno);
		exit(FAIL);
	}
	cap_free(ocap);
    }

    /*
     * Process command line options.
     */
    enp = envp;                   /* Set static var = envp. */

    while ( ( c = getopt(argc, argv, "delmf:s:") ) != -1  )
	switch(c) {
		case 'e':	/* error log requested */
			elflag++;
			break;

		case 'l':	/* list requested, twice for list of lists */
			++list_only;
			/* Fall thru intentional */

		case 'd':
			skip_logs++;
			break;

		case 'm':	/* menu requested */
			if (sflag || fflag )
				usrerrflag++;
			else
				mflag++;       
			break;

		case 'f':	/* tests listed in a file */
			if (sflag || mflag)
				usrerrflag++;
			else {
				input_file = optarg;
				fflag++;
			}
			break;
		case 's':	/* tests listed as cmd line args */
			if (mflag || fflag) 
				usrerrflag++;
			else
				sflag++;
			break;
		default:	/* default */         
			usrerrflag++;
	} /* end switch */

    /*
     * Terminate if usrerrflag is set.
     */ 
    if ( usrerrflag ) {
	w_error(GENERAL, err[CMDLINE], NULSTR, 0);
	exit(FAIL);
    }

    /*
     * Did the user just want a list of test suites?
     */
    if ( list_only > 1 ) {
    	list_test_suites();
	exit(PASS);
    }

    /* 
     * It's an error if NONE of the options -m, -s, or -f was specified.
     */
    if ( ( !mflag ) && ( !sflag ) && ( !fflag ) ) {
	w_error(GENERAL, err[MISSINGOPT], NULSTR, 0);
	exit(FAIL);
    }

    /*
     * Check if the logs should be created.
     */
    if ( !skip_logs ) {
    
	/*
	 * Get a timestring and generate log file names. 
	 */
	if ( get_timestring(str) ) {
		w_error(GENERAL, err[TIMESTRING], NULSTR, 0);
		exit(FAIL);
	}
	sprintf(test_dir, "ibt%s", str);
	sprintf(raw_file, "raw%s", str);
	sprintf(sum_file, "sum%s", str);
	sprintf(fmt_file, "fmt%s", str);
	sprintf(err_file, "err%s", str);

	if ( cap_mkdir(test_dir, 0777) != 0 ) {
	w_error(SYSCALL, test_dir, err[F_MKDIR], errno);
		exit(FAIL);
	}
	/* Set dir's mode */
	if ( cap_chmod(test_dir, 0777) != 0 ) {
		w_error(SYSCALL, test_dir, err[F_CHMOD], errno);
		cap_rmdir(test_dir);
		exit(FAIL);
	}

	/*
	 * If MAC component exists, set dir's label.
	 */
	if ( mac_enabled ) {
		maclp = mac_get_file(test_dir);
		maclp->ml_msen_type = MSEN_EQUAL_LABEL;
		maclp->ml_mint_type = MINT_EQUAL_LABEL;
		if ( cap_setlabel(test_dir, maclp) != 0 ) {
			w_error(SYSCALL, test_dir, err[SETLABEL_DIR], errno);
			cap_rmdir(test_dir);
			mac_free(maclp);
			exit(FAIL);
		}
		mac_free(maclp);
	}
	/*
	 * Finally, chdir() there!
	 */
	if ( cap_chdir(test_dir) != 0 ) {
		w_error(SYSCALL, test_dir, err[F_CHDIR], errno);
		cap_rmdir(test_dir);
		exit(FAIL);
	}

	/* 
	 * Open the raw and summary log files.  The raw log file
	 * indicates pass/fail at the TEST CASE level.  The summary
	 * log file indicates pass/fail at the TEST level (for
	 * example, kill_dac).  Both of these files record results in
	 * order of execution.  A separate program will reformat the
	 * output by security policy.  
	 */
	if ( ( raw_fp = fopen(raw_file, "a") ) == (FILE *)NULL ) {
		w_error(GENERAL, err[FILEOPEN], raw_file, 0);
		exit(FAIL);
	}
	if ( ( sum_fp = fopen(sum_file, "a") ) == (FILE *)NULL ) {
		w_error(GENERAL, err[FILEOPEN], sum_file, 0);
		exit(FAIL);
	}

	/*
	 * Make the log files read/write by all labels and uids.
	 */
	if ( mac_enabled ) {
		maclp = mac_get_file(".");
		maclp->ml_msen_type = (unsigned char)MSEN_EQUAL_LABEL;
		maclp->ml_mint_type = (unsigned char)MINT_EQUAL_LABEL;
		if ( cap_setlabel(raw_file, maclp) ) {
			w_error(GENERAL, err[SETLABEL_FILE], raw_file, 0);
			exit(FAIL);
		}
		if ( cap_setlabel(sum_file, maclp) ) {
			w_error(GENERAL, err[SETLABEL_FILE], sum_file, 0);
		exit(FAIL);
		}
	}
	if ( cap_chmod(raw_file, 0666) ) {
		w_error(SYSCALL, raw_file, err[F_CHMOD], errno);
		exit(INCOMPLETE);
	}
   	if ( cap_chmod(sum_file, 0666) ) {
		w_error(SYSCALL, sum_file, err[F_CHMOD], errno);
		exit(INCOMPLETE);
	}
    
	/* 
	 * If an error log was requested, create one and make it writeable
	 * and readable by all uids and all labels.
	 */
	if ( elflag ) {
		if ( (err_fd = cap_open(err_file, O_CREAT, 0666)) < 0 ) {
			w_error(GENERAL, err[FILEOPEN], err_file, 0);
			exit(FAIL);
		}
		close(err_fd);
		if ( cap_chmod(err_file, 0666) ) {
			w_error(SYSCALL, err_file, err[F_CHMOD], errno);
			exit(INCOMPLETE);
		}
		if ( mac_enabled && cap_setlabel(err_file, maclp) ) {
			w_error(GENERAL, err[SETLABEL_FILE], err_file, 0);
			exit(FAIL);
		}
	}
	if ( mac_enabled )
		mac_free(maclp);

	/*
	 * Write header to raw and summary log files.
	 */
	RAW_INIT1;
	SUM_INIT1;

	/*
	 * Write system info to raw and summary log files.
	 */
	for ( i = 0; i < NUMCMDS; i++ ) {
		sprintf(str, cmdline[i], raw_file );
		system(str);
		sprintf(str, cmdline[i], sum_file );
		system(str);
	}	
	sleep(1);

	SUM_INIT2;
	RAW_INIT2;
    } /* end skip_log*/

    /*
     * All the preliminary error-checking and setup are done.
     * Now call the appropriate "do" function.  
     */

#ifdef UNIMPLEMENTED /* Needs further development, do not use. */
    if ( mflag ) {
	/*
	 * Call domenu().
	 */
	if ( domenu() ) {
	    w_error(GENERAL, err[FUNCTION], "domenu()", 0);
	    exit(FAIL);
	}
    }
#endif /* UNIMPLEMENTED */

    if ( sflag ) {
	/*
	 * Pass the command line arguments after the "-s"
	 * to the dostrings() routine.
	 */
	argc -= (optind - 1);
	argv += (optind - 1);	
	/* 
	 * The "-s" must be the last option. Verify this,
         * then call dostrings().
	 */
	for ( i = 0; i < argc; i++ ) {
	    if ( *argv[i] == '-' ) {
		w_error(GENERAL, err[S_OPTION], NULSTR, 0);
		exit(FAIL);
	    }
	}
	if ( dostrings(argc, argv) ) {
#ifdef DEBUG
	    w_error(GENERAL, err[FUNCTION], "dostrings()", 0);
#endif
	    exit(FAIL);
	}
    }
    
    if ( fflag ) {
	/*
	 * Pass the input file name to the dofile() routine.
	 */
	if ( dofile(input_file) ) {
#ifdef DEBUG
	    w_error(GENERAL, err[FUNCTION], "dofile()", 0);
#endif /* DEBUG */
	    exit(FAIL);
	}
    }

    if (list_only) {
    	list_queued_tests();
	exit(PASS);
    }

    /* 
     * Call the test functions that were queued.
     */
     run_queued_tests();

    /* 
     * Close the log files.
     */
    fclose(raw_fp);
    fclose(sum_fp);

    /* 
     * Exec the program that generates an output file formatted 
     * by security policy.  This will probably be a shell script 
     * that uses awk to reformat info from the summary log file.
     *  
     * execl("fmt.sh", sum_file, fmt_file, NULSTR);  
     */
    exit(PASS);
    return(PASS);
}

/*
 * Get_envp() returns the environment pointer that was passed
 * to main().  This is used by one or more of the "exec" tests.
 */
char **get_envp(void)
{
    return(enp);
}
 

/*
 * Get_timestring() copies a formatted date/time string into a buffer.
 * It is called once by main.
 */
int 
get_timestring(char *buf)
{
    time_t timeval;          /* Value returned by time().  */
    
    if ( (timeval = time((time_t *)0)) == (time_t)-1 ) {
#ifdef DEBUG
	w_error(SYSCALL, "get_timestring()" err[TIMESTRING], errno);
#endif /* DEBUG */
	return(FAIL);
    }

    if ( !(cftime(buf, "%m%d%y.%H%M%S", &timeval)) ) {
#ifdef DEBUG
	w_error(GENERAL, err[FUNCTION], "cftime()", 0);
#endif /* DEBUG */
	return(FAIL);
    }
    return(PASS);
}

/*
 * The next five functions are in this file because each uses
 * one of the static FILE pointers.
 */

/*
 * w_error prints an error message to stderr and to the error log, 
 * if there is one.  The format of the message depends on errtype.
 * If errtype is SYSCALL, string0 and string1 are printed, followed 
 * by the message sys_errlist[numarg] (the list used by perror(3)).
 * If errtype is GENERAL, string0, and string1 (if non-null) 
 * are printed.
 * PRINTNUM is the same as GENERAL, except that numarg is also 
 * printed on the line.
 * w_error adds spaces between strings and a terminal newline.
 */
void
w_error(int errtype, const char *string0, const char *string1, int numarg)
{
    extern char *sys_errlist[];
    extern int sys_nerr;
    char str[MSGLEN];
    register char *c;

    memset((void *) str, '\0', sizeof(str));
    if (errtype == SYSCALL) {
	c = "Unknown error";
	if(numarg >= 0 && numarg < sys_nerr)
	    c = sys_errlist[numarg];
	fprintf(stderr, "%s %s %s \n", string0, string1, c);
	fflush(stderr);
	sprintf(str, "%s %s %s \n", string0, string1, c);
   }
	
    else {   /* errtype is GENERAL or PRINTNUM */
	if ( string1 ) {
	    sprintf(str, "%s %s", string0, string1);
	}
	else {
	    sprintf(str, "%s", string0);
	}

	if ( errtype == PRINTNUM ) {  /* add numeric arg to string */
	    sprintf(str, "%s %d \n", str, numarg);
	} 
	else {
	    sprintf(str, "%s \n", str);
	}

	fprintf(stderr, "%s", str);
	fflush(stderr);
    }

    /*
     * If there is an elog, print it there too.
     */
    if ( elflag ) {
	err_fd = open(err_file, O_RDWR | O_APPEND);
        if ( err_fd < 0 ) {
	    sprintf(str, "%s %s \n", err[FILEOPEN], err_file);
	    perror("str");
	    exit(FAIL);
	}
	write(err_fd, str, strlen(str));
	close(err_fd);
    }
}


/*
 * w_raw_log() uses fprintf() to write a string to the raw log.
 */
void w_raw_log(const char *str)
{
    fprintf(raw_fp, "%s", str);
}


/*
 * flush_raw_log() flushes output to the raw log file.
 */
void flush_raw_log(void)
{
    fflush(raw_fp);
}


/*
 * w_sum_log() uses fprintf() to write a string to the summary log.
 */
void w_sum_log(const char *str)
{
    fprintf(sum_fp, "%s", str);
}

/*
 * flush_sum_log() flushes output to the summary log file.
 */
void flush_sum_log(void)
{
    fflush(sum_fp);
}
