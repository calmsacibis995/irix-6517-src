#ifndef _KERNEL
/* Source for the branch tool libraries. */

#include "btool_defs.h"		/* For other declarations. */
#include <stdio.h>

btool_log_t Btool_branches;
static need_case = 0;

/*
NAME:

	long
	btool_branch(number, truth_value)
	long truth_value;
	long number;
PURPOSE:
	This routine is called every time an expression is evaluated in order
	to make a branch decision (e.g. in an if statement, while statement
	etc.). It keeps track of the branches that were taken and the
	direction they were taken. This is done by updating the btool_branches
	array.

PRECONDITIONS:
	1. 0 <= number < NUM_BRANCHES
	2. truth_value is equal to 0 or 1

POSTCONDITIONS:
	If truth_value is equal to 0 the branch-taken-false-counter for
	the branch with number number will be incremented by one\**  
	The counters are not checked for overflows because even if an 
	overflow occurs, you still will get the information that this branch
	was taken, but the number will be incorrect.  
	If truth_value is equal to 1 the branch-taken-true-counter
	will be incremented. The return value is truth_value.

ERRORS:
	1. The routine ignores this branch (caller's responsibility).
	2. It is not checked if truth_value is in its range
	   (caller's responsibility).
*/

long
btool_branch(number, truth_value)
     long number, truth_value;
{
    if (number < 0 || number >= NUM_BRANCHES)
    {
	fprintf(stderr,"Bogus number %d ignored\n", number);
    }
    else
    {
	if (truth_value)
	{
	    Btool_branches[number].true_count++;
	}
	else
	{
	    Btool_branches[number].false_count++;
	}
    }
    return (truth_value);
}

void
btool_func(number)
     long number;
{
    if (number < 0 || number >= NUM_BRANCHES)
	fprintf(stderr,"Bogus number %d ignored\n", number);
    else
	Btool_branches[number].true_count++;
    return;
}

/*
NAME:
	long
	btool_switch(switch_expr)
	long switch_expr;

PURPOSE:
	This function is called when a switch statement is encountered. It
	purpose is to set the need_case flag to 1. This tells the
	btool_case routine that the switch statement was entered by
	evaluating the switch expression. This is a way to determine the
	fall-through and goto-into-case-statement cases.

PRECONDITIONS:

POSTCONDITIONS:  
	The need_case variable is set equal to 1. The routine's return
	value is switch_expr.

ERRORS:
*/

long
btool_switch(switch_expr)
     long switch_expr;
{

  need_case = 1;
  return(switch_expr);
}


/*
NAME:
	btool_case(number);
	long number;

PURPOSE:
	This routine is called every time a case statement is encountered. 
	Its function is to keep track of the cases that were taken. Like
	btool_branch it also uses the btool_branches array to store this
	information. The need_case flag is checked to see if the case
	statement was entered by evaluating the switch statement.

PRECONDITIONS:
	1. 0 <= number < NUM_BRANCHES

POSTCONDITIONS:
	If the need_case flag is equal to 1 then the
	branch-taken-true-counter for the branch (case) with number 
	number will be incremented by one and need_case will be set to 0. 
	If need_case is not equal to 1 nothing will happen.

ERRORS:
	1. The routine ignores the branch (case) (caller's responsibility).
*/


void
btool_case(number)
	long number;
{
    if (number < 0 || number >= NUM_BRANCHES)
    {
	fprintf(stderr,"Bogus number %d ignored\n", number);
    }
    else
    {
	if (need_case)
	{
	    Btool_branches[number].true_count++;
	    need_case =0;  
	}
	/* Else this must be a fall-through case */
    }
}


/*
NAME:
	btool_writelog(filename);
	char *filename;

PURPOSE:
	This is the last routine called. Its purpose is to write out all
	the data collected by the other routines. The input for this routine
	is the name of the log file. You can use any name you want. 

PRECONDITIONS:
	1. It must be possible to open the file for writing.
	2. It must be possible to write all data to file.

POSTCONDITIONS:
	A log file names 'filename' will be created. The return code is 0.

ERRORS:
	1. The routine will return with error code -1.
	2. The routine will return with error code -1.
*/

int
btool_writelog(filename)
	char *filename;
{
    register int error = 0;
    register int i;
    register FILE *file;

    file = fopen(filename, "w");
    if (NULL == file)
    {
	fprintf(stderr, "Can't open branch output file %s.\n", filename);
	error = -1;
    }
    
    for (i = 0; i < NUM_BRANCHES; i++)
    {
        if (fprintf(file, "%08d %d %d\n", i, Btool_branches[i].true_count, 
		Btool_branches[i].false_count)<0) 
	  {
	    error = -1;
	    i=NUM_BRANCHES;
	  }
    }
    if (EOF == fclose(file))
      {
      fprintf(stderr, "Can't close branch output file %s.\n", filename);
      error = -1;
      }

    return(error);
}
#else /* _KERNEL */

#ifdef __BTOOL__
#include "btool_defs.h"		/* For other declarations. */
#include "sys/types.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/atomic_ops.h"

int badbtool;
btool_log_t Btool_branches;

#pragma btool_prevent

long
btool_branch(unsigned long number, long truth_value)
{
    int *a;

    if (number >= NUM_BRANCHES) {
	badbtool++;
    } else {
	if (truth_value)
	    a = &Btool_branches[number].true_count;
	else
	    a = &Btool_branches[number].false_count;
	atomicAddInt(a, 1);
    }
    return (truth_value);
}

void
btool_func(unsigned long number)
{
    if (number >= NUM_BRANCHES)
	badbtool++;
    else
	atomicAddInt(&Btool_branches[number].true_count, 1);
    return;
}

/*
 * The need_case stuff just plain doesn't work in the face of interrupts.
 * From a code coverage perspective I don't see why it matters - ones
 * wants to know whether code was executed - doesn't really matter how
 * it got there..
 */

long
btool_switch(long switch_expr)
{
  return(switch_expr);
}

void
btool_case(unsigned long number)
{
    if (number >= NUM_BRANCHES)
	badbtool++;
    else
        atomicAddInt(&Btool_branches[number].true_count, 1);
}

/*
 * data access routines - called from syssgi
 */

int
btool_size(void)
{
	return sizeof(Btool_branches);
}

int
btool_getinfo(void *b)
{
	if (copyout(Btool_branches, b, sizeof(Btool_branches)))
		return EFAULT;
	return 0;
}

void
btool_reinit(void)
{
	bzero(Btool_branches, sizeof(Btool_branches));
}
#pragma btool_unprevent

#else /* !__BTOOL__ */
#include "sys/errno.h"

int btool_size(void) { return 0; }

/* ARGSUSED */
int btool_getinfo(void *b) { return EINVAL; }

void btool_reinit(void) {}
#endif

#endif
