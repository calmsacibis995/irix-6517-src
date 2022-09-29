/* 
** File:  shmzero.c
** ----------------
** program make sure that we are getting a zeroed out page from 
** the clshm driver.
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "xp_shm_wrap.h"


/* defaults for parameters into share_mem */
#define DEFAULT_PART_SIZE	getpagesize()

/* usage string */ 
static const char *usage = "%s [-s<size of shared segment>]\n"
       "\tdefault is %d\n";


/* prototypes */
void parse_args(int argc, char *argv[], int *part_size);

/* parse_args
** ----------
** parse arguments and make sure most arguments make sense.
*/
void parse_args(int argc, char *argv[], int *part_size)
{
    extern char *optarg;
    extern int optind;
    int c;
    int error = 0;

    *part_size = DEFAULT_PART_SIZE;

    if(argc > 1)   {
	while((c = getopt(argc, argv, "s:")) != EOF) {
	    switch(c)  {
	    case 's':	{
		*part_size = atoi(optarg);
		if(*part_size > 0) {
		    break;
		}
		printf("\tsize must be > 0\n");
		/* fall through for bad part size */
	    }
	    case 'h':
	    default:  {
		printf(usage, argv[0], DEFAULT_PART_SIZE);
		if(c == 'h') 
		    exit(EXIT_SUCCESS);
		else exit(EXIT_FAILURE);
	    }
	    }
	}
    }
}


/* main */
int main(int argc, char *argv[])
{
    int length, i;
    char *buffer;
    key_t key;
    int shmid;

    set_debug_level(10);
    parse_args(argc, argv, &length);

    key = xp_ftok(NULL, 1);
    if(key == -1) {
	printf("can't get key\n");
	return(EXIT_FAILURE);
    }
    
    shmid = xp_shmget(key, length, 0);
    if(shmid == -1) {
	printf("can't get shmid\n");
	return(EXIT_FAILURE);
    }

    buffer = xp_shmat(shmid, NULL, 0);
    if(!buffer) {
	printf("can't get shmaddr from shmat\n");
	return(EXIT_FAILURE);
    }

    for(i = 0; i < length; i++) {
	if(buffer[i] != 0) {
	    printf("found %c at index %d\n", buffer[i], i);
	    return(EXIT_FAILURE);
	}
    }

    if(xp_shmdt(buffer) < 0) {
#if (_MIPS_SIM == _ABI64)
	printf("can't detach shmaddr 0x%llx\n", buffer);
#else
	printf("can't detach shmaddr 0x%x\n", buffer);
#endif
	return(EXIT_FAILURE);
    }

    if(xp_shmctl(shmid, IPC_RMID) < 0) {
	printf("can't rmid shmid 0x%x\n", shmid);
	return(EXIT_FAILURE);
    }

    printf("All the pages we got have been zero'd\n");
    return(EXIT_SUCCESS);
}

