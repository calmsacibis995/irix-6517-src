#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#define DEBUG(x) (void) printf x ;/* debug macro */
#define ERROR (errno > LASTERRNO ? "unknown error" : sys_errlist[errno] )

extern int	semctl(int, int, int, union semun);
extern int	semget(key_t, int, int);
extern int	semop(int, struct sembuf *, int);

void
semDestroy(int semid)
/*
 * Function: semDestroy
 * Purpose: To destroy a semaphore
 * Parameters: semid - the ID of the semaphore to destroy
 * Returns: nothing
 * Ignores error code, since it can be called multiple times.
 */
{
    union semun semarg;

    semarg.val = 0;
    (void) semctl(semid, 0, IPC_RMID, semarg);
}

int
semCreate(int semcnt)
/*
 * Function: semCreate
 * Purpose: To create a new semaphore
 * Parameters: # of values in semaphore
 * Returns: Semaphore ID if success, -1 if failed.
 */
{
    union semun semarg;
    int	semid;

    semid = semget(0, 1, IPC_EXCL | IPC_CREAT | 0666);
    if (0 > semid) {
	DEBUG(("semget failed: %s (probably not enough semaphores)\n",
	       ERROR));
	exit(1);
    } 
    semarg.val = semcnt;
    if (0 > semctl(semid, 0, SETVAL, semarg)) {
	DEBUG(("semctl failed to set initial value: %s\n", ERROR));
	semDestroy(semid);
	return(-1);
    }
    return(semid);
}

int
semDown(int semid)
/*
 * Function: semDown
 * Purpose: To decrement a semaphore and wait if it would go <0.
 * Parameters: semid - id of semaphore to decrement
 * Returns: 0 -  success
 *	    -1 - failed
 */
{
    struct sembuf semopt;

    semopt.sem_num = 0;
    semopt.sem_op = -1;
    semopt.sem_flg = 0;
    
    if (0 > semop(semid, &semopt, 1)) {
	DEBUG(("semop failed: %s\n", ERROR));
	return(-1);
    } else {
	return(0);
    }
}

int
semUp(int semid)
/*
 * Function: semUp
 * Purpose: To increment a semaphore.
 * Parameters: semid - id of semaphore to increment
 * Returns: 0 -  success
 *	    -1 - failed
 */
{
    struct sembuf semopt;

    semopt.sem_num = 0;
    semopt.sem_op = +1;
    semopt.sem_flg = 0;
    
    if (0 > semop(semid, &semopt, 1)) {
	DEBUG(("semop failed: %s\n", ERROR));
	return(-1);
    } else {
	return(0);
    }
}

int
semOp(int semid, int op)
/*
 * Function: semOp
 * Purpose: To up or down a semaphore
 * Parameters: semid - id of semaphore to operate on
 *		op - value to increment semaphore by (may be <0)
 * Returns: 0 - success
 *	   -1 - failed
 */
{
    struct sembuf semopt;

    semopt.sem_num = 0;
    semopt.sem_op = op;
    semopt.sem_flg = 0;
    
    if (0 > semop(semid, &semopt, 1)) {
	DEBUG(("semop failed: %s\n", ERROR));
	return(-1);
    } else {
	return(0);
    }
}

int
semVal(int semid)
/*
 * Function: semVal
 * Purpose: To get the value of a semaphore
 * Parameters: semid - id of semaphore to operate on
 * Returns: -1 - failed, else result of semaphore.
 *	   
 */
{
  union semun semarg;
  int value;

  if (0 > (value = semctl(semid, 0, GETVAL, semarg))) {
    DEBUG(("warning: semctl failed to remove semaphore %d: %s\n", 
		semid, ERROR));
    }
  return(value);
}
