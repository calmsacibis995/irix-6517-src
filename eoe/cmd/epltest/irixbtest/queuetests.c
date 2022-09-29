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
#include "irixbtest.h"
#include "queuetests.h"
#include <ctype.h>

/*
 * queuetests.c -- dostrings(), dofile(), domenu(),
 *                 lookup_tests(), queue_multi(),
 *		   and run_queued_tests().
 */
extern void displaymenu(short menu_num);
extern void domenu_policy(void);
extern void domenu_syscall(void);
extern void domenu_mac(void);
extern void domenu_dac(void);

extern short list_only;

static short next = 1;              /* t_order[] index during queueing  */
static struct test_item		    /* Holds the index of each function */
	*t_order[sizeof (tests)];   /* to be run, in requested order.   */

/* 
 * Only one of the three "do" functions, dostrings(), dofile(), and
 * domenu(), is called on a given invocation of the suite.  Each of
 * the do functions determines which test functions the user wishes to
 * run, and in what order, and calls queue_tests() to queue them.
 */

/*
 * dostrings() calls queue_test() with each string in its argument vector
 * If queue_test() returns zero, dostrings() returns non-zero.
 */
int
dostrings(int argcount, char *argvec[])
{
    int		i;

    /* 
     * Look at each string. 
     */
    for (i = 0; i < argcount; i++) {   
	if (strcmp(argvec[i], "+s") == 0)
		break;
	if (!queue_tests(argvec[i])) {
	    w_error(GENERAL, err[TESTNOTSUP], argvec[i], 0);
	    return(FAIL);
	}
    }  

    if (i < argcount && strcmp(argvec[i], "+s") == 0) {
	for (++i; i < argcount; ++i)
	    unqueue_tests(argvec[i]);
    }

    return(PASS);
}

/* 
 * dofile() is similar to dostrings() above, except that the strings 
 * are in a file instead of an argument vector.  A '#' is considered 
 * the beginning of a comment.  Everything from the '#' to the next
 * newline is ignored.
 */
int
dofile(char *input_filename)
{
    FILE *input_fp;             /* Input file pointer. */
    register char *p;           /* fstr pointer */
    char fstr[SLEN];            /* Individual strings from input file. */
    int  rval;			/* Return value from this function */
    
    rval = 0;

    /*
     * Try to open the input file.
     */
    if ( (input_fp = fopen(input_filename, "r")) == (FILE *)NULL ) {
	w_error(GENERAL, err[FILEOPEN], input_filename, 0);
	return(FAIL);
    }

    /* 
     * Read each string from the file
     */
    while ( fgets(input_fp, sizeof(fstr), fstr) != (char *)0 ) {
	/*
	 * Is there a '#' in this string?
	 */
	for (p = fstr; *p; ++p) {	/* Skip white space */
		if (!isspace(*p))
			break;
	}
	if (*p == '#')
		continue;
	
	/* 
	 * Skip string if it is null.
	 */
	if (*fstr == '\0')
		continue;

	/*
	 * Otherwise try to look up the string
	 */
	if (!queue_tests(fstr)) {
		w_error(GENERAL, err[TESTNOTSUP], fstr, 0);
		rval = 1;
		break;
	}
    }
    fclose(input_fp);
    return(rval);
}

#ifdef UNIMPLEMENTED
/* NEEDS WORK, DON'T USE YET */

/* 
 * domenu() displays a top-level menu.  Based on the user's input to 
 * that menu and other menus displayed by functions call by domenu(),
 * tests are queued for execution.  One user response in domenu()
 * causes domenu() to return zero, so that main calls run_queued_tests() and
 * the queued tests are called.  Another choice causes domenu() to
 * return non-zero, thus terminating the program.
 */
int
domenu(void)
{
    register i;       /* Index into str[]. */
    register int c;   /* Holds input character. */
    short inpt;       /* Holds input as integer. */
    short ready;      /* Flag: user is ready to run queued tests. */
    char str[4];      /* Holds input string. */
    
    for ( ; ; ) {
	/*
	 * Initialize data for this iteration.
	 */
        inpt = 0;
	ready = 0;
	i = 0;
	/* 
	 * Display the top-level menu. 
	 */
	displaymenu(0);  
	/*
	 * Get input string and convert to integer.
	 */
	while ( (c = getchar()) != '\n') {
	    str[i++] = c;
	}
	str[i] = '\0';
	inpt = atoi(str);
	/*
	 * Respond to user input.
	 */
	switch (inpt) {
	case (1):     /* User wants to queue all tests. */
	    queue_multi(ALL);
	    break;
	case (2):     /* User wants to see the policy menu. */
	    domenu_policy();
	    break;
	case (3):     /* User wants to see the syscall menu. */
	    domenu_syscall();
	    break;
	case (4):     /* User ready to execute queued tests. */
	    ready++;
	    break;
	case (5):     /* User wants to quit without running tests. */
	    return(FAIL);
	default:      /* User error. */
	    w_error(GENERAL, err[MENUCHOICE], str[i], 0);
	    break;
	}
	if (ready)
	    break;
    }
    return(PASS);
}
#endif /*UNIMPLEMENTED*/

static short marbles = 1;
void
noMarbles(struct test_item *tp)
{
    if (list_only) {
    	marbles = 0;
    	printf("skipping %s\n", tp->name);
	fflush(stdout);
	return;
    }
    if (marbles) {
    	marbles = 0;
	fprintf(stderr, "Cannot run the following test(s), they requires the ");
	fprintf(stderr, "listed items be configured, \nwhich are not");
 	fprintf(stderr, "present:\n");


    }
    fprintf(stderr, "    test \"%s\" needs:\n", tp->name);
    if ((tp->flags & ACL) == ACL && !acl_enabled)
    	fprintf(stderr, "\tAccess Control Lists\n");
    if ((tp->flags & AUDIT) == AUDIT && !audit_enabled)
    	fprintf(stderr, "\tSecurity Audit Trail\n");
    if ((tp->flags & CIPSO) == CIPSO && !cipso_enabled)
    	fprintf(stderr, "\tIP Security Options\n");
    if ((tp->flags & MAC) == MAC && !mac_enabled)
    	fprintf(stderr, "\tMandatory Access Control\n");
}

/*
 * _queue() adds the requested test to the queue of tests to run
 */
void
_queue(struct test_item *tp)
{
	if (!tp->queued) {
		tp->queued = next;
#ifdef DEBUG
		if (list_only) {
			printf("queueing %s\n", tp->name);
			fflush(stdout);
		}
#endif /*DEBUG*/
		t_order[next++] = tp;
	} 
}

/*
 * _unqueue() removes the requested test from the queue of tests to run
 */
void
_unqueue(struct test_item *tp)
{
	int	i;

	if (tp->queued) {
#ifdef DEBUG
		if (list_only) {
			printf("unqueueing %s\n", tp->name);
			fflush(stdout);
		}
#endif /*DEBUG*/
		--next;
		for (i = tp->queued; i < next; ++i) {
			t_order[i] = t_order[i+1];
			t_order[i]->queued -= 1;
		}
		tp->queued = 0;
	} 
}

/* 
 * queue_tests() looks for the name of a test.  If it finds it, it queues
 * it for execution.  If not, it looks to see if the name given is a suite
 * of tests (the name "all", for example, is the suite of all tests).
 * It returns 0 if the name is not found.
 * It returns 1 if the name queued any tests.
 */
int
queue_tests(char *name)
{
    struct multiqueue_item *mp;
    struct test_item	*tp;
    char	**cp;
    int		rval;

    /* 
     * Does the string match a test function? 
     */
    for (tp = tests; tp->name; ++tp) {	 
	if (strcasecmp(name, tp->name) == 0) {
	    /* Is the system configured with what we need for this test? */
	    if (list_only || (sys_config_mask & tp->flags) == tp->flags) {
		    _queue(tp);
		    return(FAIL);
	    }

	    /* We don't have enough marbles to run this test */
	    noMarbles(tp);
	    return(FAIL); /* We have to return `1', we did *find* the test! */
	}
    }

    /*
     * Does it match the name of a suite of tests listed in multiqueue[]?
     * If so, go recursive with the name of each listed test.
     */
    for (mp = multiqueue; mp->name; ++mp) {
	if (strcasecmp(name, mp->name) == 0) {
		rval = 0;
		for (cp = mp->tests; *cp; ++cp) {
		    if (queue_tests(*cp))
		    	++rval;
#ifdef DEBUG
		    else
			fprintf(stderr, "INTERNAL ERROR: suite \"%s\" lists
			non-existant test \"%s\"!\n", mp->name, *cp);
#endif /* DEBUG */
		}
	    return rval;
	}
    }

    /*
     * If it doesn't match either set, this is a user error.
     */
    return(PASS);
}

/* 
 * unqueue_tests() looks for the name of a test.  If it finds it, it unqueues
 * it for execution.  If not, it looks to see if the name given is a suite
 * of tests (the name "all", for example, is the suite of all tests).
 */
int
unqueue_tests(char *name)
{
    struct multiqueue_item *mp;
    struct test_item	*tp;
    char	**cp;

    /* 
     * Does the string match a test function? 
     */
    for (tp = tests; tp->name; ++tp) {	 
	if (strcasecmp(name, tp->name) == 0) {
		_unqueue(tp);
		return(PASS);
	}
    }

    /*
     * Does it match the name of a suite of tests listed in multiqueue[]?
     * If so, go recursive with the name of each listed test.
     */
    for (mp = multiqueue; mp->name; ++mp) {
	if (strcasecmp(name, mp->name) == 0) {
		for (cp = mp->tests; *cp; ++cp)
		    unqueue_tests(*cp);
		return(PASS);
	}
    }

    /* Oh, well... Never mind, I guess.  */
    return(FAIL);
}

/* 
 * list_test_suites simply lists all tests in multiqueue[]
 */
void
list_test_suites(void)
{
    struct multiqueue_item *mp;
    struct test_item	*tp;
    char	**cp;

    /*
     * Does it match the name of a suite of tests listed in multiqueue[]?
     * If so, go recursive with the name of each listed test.
     */
    for (mp = multiqueue; mp->name; ++mp) {
        printf("%s:\t", mp->name);
	for (cp = mp->tests; *cp; ++cp)
	    printf(" %s", *cp);
	printf("\n");
    }
}

/* 
 * list_queued_tests() simply outputs the list of queued tests
 */
void
list_queued_tests(void)
{
    register struct test_item	*tp;
    register i;           /* Test array index  */

    for (i = 1; i < next; ++i) {
	tp = t_order[i];
	printf("%s\n", tp->name);
	fflush(stdout);
    }
}

/* 
 * run_queued_tests() prints a reference string to the raw and summary
 * files for each queued test function, then calls the test function.
 * The reference string is useful for locating errors in the raw and
 * summary file after finding them in the formatted output file.
 */
void
run_queued_tests(void)
{
    register struct test_item	*tp;
    register i;           /* Test array index  */
    char str[MSGLEN];     /* Strings printed to log files. */

    SUM_INIT0;
    for (i = 1; i < next; ++i) {
	tp = t_order[i];

	SUM_STARTTEST(i, tp->name);
	
	/*
	 * Initialize raw and summary log for this test function,
	 * invoke the function, and write output to the summary log
	 * based on the return value of the function.
	 */
	RAW_STARTTEST(i);
	printf("\nexecuting %s\n", tp->name);
	fflush(stdout);

	switch ((*tp->func)()) {
	    case PASS:
		w_sum_log("pass\n");
		break;
	    case FAIL:
		w_sum_log("fail\n");
		break;
	    case INCOMPLETE:
		w_sum_log("incomplete\n");
		break;
	    
	}
	fflush(stderr);
	flush_sum_log();

	if (access("./core", F_OK) == 0)
	{
	    char  newcorename[40], datestr[40];

	    get_timestring(datestr);
	    sprintf(newcorename, "core.%s.%s", tp->name, datestr);
	    if (rename("./core", newcorename) < 0)
	        w_error(SYSCALL, "run_queued_tests()", err[F_RENAME], errno);
	}
    }
}
