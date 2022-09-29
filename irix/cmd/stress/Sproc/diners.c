/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.5 $"

/*
 *   This file contains a program which demonstrates
 *   Dijkstra's Dining Philosophers Problem.
 *
 *
 *                        PHIL1    |    PHIL2                       
 *                    \\                        /                 
 *                                                                
 *                PHIL5          (rice)           PHIL3          
 *                                                              
 *                                                             
 *                     /         PHIL4         \\               
 *                                                           
 */

#include	<sys/types.h>
#include	"unistd.h"
#include	"stdlib.h"
#include	"stdio.h"
#include	"ulocks.h" 
#include	"string.h" 
#include	"time.h" 
#include	"assert.h"

#define    NUM_PHILOSOPHERS 5	/* number of dining philosophers  */
#define    SLEEP_TIME      10	/* maximum amount of time (ticks) the philosophers eat and think */

void sleeper(void);
void hungry(int p_num);
void thinking(int p_num);
int eating(int p_num);
void philosopher(int nloops);

usptr_t *handle;			/* handle on semaphore arena */
usema_t *chopstick[NUM_PHILOSOPHERS];	/* semaphore on each chopstick */
int verbose = 0;

int
main(int argc, char **argv)
{
	int i, nloops;

	if (argc < 2) {
		fprintf(stderr, "Usage:diners [-v] nloops\n");
		exit(1);
	}
	if (strcmp(argv[1], "-v") == 0)
		verbose++;
	if (argc == 3)
		nloops = atoi(argv[2]);
	else
		nloops = atoi(argv[1]);

	handle = usinit("/usr/tmp/diners");
	printf("doing %d loops with %d philosophers\n", nloops, NUM_PHILOSOPHERS);
	/* initialize the five semaphores to 1 */
	for (i = 0; i < NUM_PHILOSOPHERS; i++) {
	        if((chopstick[i] = usnewsema(handle,1) ) == NULL) {
                	perror("The usnewsema call failed. NO DINNER.");
			unlink("/usr/tmp/diners");
                	exit(1);
		}
        }
	/* set the number of processes to spawn */
	m_set_procs(NUM_PHILOSOPHERS);
	/* sproc off five dining philosophers (children)*/
	m_fork(philosopher, nloops);
	/* kill off idle processes */
	m_kill_procs();	
	unlink("/usr/tmp/diners");
	return 0;
}

/*
 * philosopher
 *	This is the function that each spawned shared process executes.
 */
void
philosopher(int nloops)
{
	int p_num;

	/* get my id, so I know which chpstock to go for */
	p_num = m_get_myid();
	srand((unsigned)time(NULL));

	/* philosopher thinks, becomes hungry, eats, ...*/
	while (nloops--) {
        	thinking(p_num + 1);
    		hungry(p_num + 1);
       		eating(p_num + 1);
        }
}

/*  sleeper: randomly sleeps from 0 to SLEEP_TIME */
void
sleeper(void)
{
        (void)sginap(rand() % SLEEP_TIME);
}

/*  hungry: a philosopher being hungry */
void
hungry(int p_num)
{
	if (verbose)
		(void)printf("Philosopher %d is hungry...\n",p_num);
}

/* thinking: a philosopher thinking for a random amount of time.*/
void
thinking(int p_num)
{
	if (verbose)
		(void)printf("Philosopher %d is thinking...\n",p_num);
        sleeper();
}

/*   eating: a philosopher eating for a random amount of time.*/
int
eating(int p_num)
{
        int n = NUM_PHILOSOPHERS;

	/* to prevent deadlock, even philosopher chooses right chopstick first*/
        if (!(p_num % 2)) {

		/* P(right chopstick) operation*/
                if (uspsema(chopstick[p_num-1]) == -1) {
                     perror("uspsema error");
		     return(-1);
                }
		if (verbose)
			(void)printf("Philosopher %d is hungry & has right chopstick...\n",p_num);

		/* P(left chopstick) operation*/
                if (uspsema(chopstick[p_num%n]) == -1) {
                     usvsema(chopstick[p_num-1]);
                     perror("uspsema errr");
		     return(-1);
                }
        
        	/* philosopher's critical section begins*/
		if (verbose)
			(void)printf("Philosopher %d is eating...\n",p_num);
                sleeper();
        	/* Philosopher's critical section ends*/

		/* V(right chopstick) operation*/
                usvsema(chopstick[p_num-1]);
		/* V(left chopstick operation)*/
                usvsema(chopstick[p_num%n]);
        } else {
		/* to prevent deadlock, odd philosopher chooses left chopstick first*/

		/* P(left chopstick operation)*/
                if (uspsema(chopstick[p_num%n]) == -1) {
                     perror("uspsema error");
                     return(-1);
                }
		if (verbose)
			(void)printf("Philosopher %d is hungry & has left chopstick...\n",p_num);
		/* P(right chopstick operation*/
                if (uspsema(chopstick[p_num-1]) == -1) {
                     usvsema(chopstick[p_num%n]);
                     perror("uspsema error");
                     return(-1);
                }

        	/* philosopher's critical section begins*/
		if (verbose)
			(void)printf("Philosopher %d is eating...\n",p_num);
                sleeper();
        	/* philosopher's critical section ends*/

		/* V(left chopstick) operation*/
                usvsema(chopstick[p_num%n]);
		/* V(right chopstick) operation*/
                usvsema(chopstick[p_num-1]);
        }
	return(0);
}
