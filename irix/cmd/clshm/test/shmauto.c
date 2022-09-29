/* 
** File:  shmauto.c
** ----------------
** make sure that we test out the IPC_AUTORMID functionality of the 
** driver/daemon/usr lib
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "xp_shm_wrap.h"

enum {
    BEFORE_ATTACHES, 
    BETWEEN_ATTACHES, 
    AFTER_ATTACHES, 
    BETWEEN_DETACHES,
    AFTER_DETACHES,
    AFTER_RMID
};

/* defaults for parameters into share_mem */
#define DEFAULT_PART_SIZE	getpagesize()

/* usage string */ 
static const char *usage = "%s [-s<size of shared segment>]\n"
       "\tdefault is %d\n";


/* prototypes */
void parse_args(int argc, char *argv[], int *part_size);
void do_memory(int place_attach, int length);


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
    
    set_debug_level(10);
    parse_args(argc, argv, &length);

    for(i = BEFORE_ATTACHES; i <= AFTER_RMID; i++) {
	do_memory(i, length);
    }

    printf("success!!\n");
    return(EXIT_SUCCESS);
}


void do_memory(int place_attach, int length) 
{
    char *buffer1, *buffer2;
    key_t key;
    int shmid;

    key = xp_ftok(NULL, 1);
    if(key == -1) {
	printf("can't get key\n");
	exit(EXIT_FAILURE);
    }
    
    shmid = xp_shmget(key, length, 0);
    if(shmid == -1) {
	printf("can't get shmid\n");
	exit(EXIT_FAILURE);
    }

    if(place_attach == BEFORE_ATTACHES) {
	if(xp_shmctl(shmid, IPC_AUTORMID) < 0) {
	    printf("can't IPC_AUTORMID 1\n");
	    exit(EXIT_FAILURE);
	}
    }

    buffer1 = xp_shmat(shmid, NULL, 0);
    if(!buffer1) {
	printf("can't get shmaddr from shmat\n");
	exit(EXIT_FAILURE);
    }

    if(place_attach == BETWEEN_ATTACHES) {
	if(xp_shmctl(shmid, IPC_AUTORMID) < 0) {
	    printf("can't IPC_AUTORMID 2\n");
	    exit(EXIT_FAILURE);
	}
    }

    buffer2 = xp_shmat(shmid, NULL, 0);
    if(!buffer2) {
	printf("can't get shmaddr from shmat\n");
	exit(EXIT_FAILURE);
    }

    if(place_attach == AFTER_ATTACHES) {
	if(xp_shmctl(shmid, IPC_AUTORMID) < 0) {
	    printf("can't IPC_AUTORMID 3\n");
	    exit(EXIT_FAILURE);
	}
    }

    if(xp_shmdt(buffer1) < 0) {
#if (_MIPS_SIM == _ABI64)
	printf("can't detach shmaddr 0x%llx\n", buffer1);
#else
	printf("can't detach shmaddr 0x%x\n", buffer1);
#endif
	exit(EXIT_FAILURE);
    }

    if(place_attach == BETWEEN_DETACHES) {
	if(xp_shmctl(shmid, IPC_AUTORMID) < 0) {
	    printf("can't IPC_AUTORMID 4\n");
	    exit(EXIT_FAILURE);
	}
    }

    if(xp_shmdt(buffer2) < 0) {
#if (_MIPS_SIM == _ABI64)
	printf("can't detach shmaddr 0x%llx\n", buffer2);
#else
	printf("can't detach shmaddr 0x%x\n", buffer2);
#endif
	exit(EXIT_FAILURE);
    }

    if(place_attach == AFTER_DETACHES) {
	if(xp_shmctl(shmid, IPC_AUTORMID) < 0) {
	    printf("can't IPC_AUTORMID 5\n");
	    exit(EXIT_FAILURE);
	}
    }

    sginap(100);
    if(place_attach == AFTER_RMID) {
	if(xp_shmctl(shmid, IPC_RMID) < 0) {
	    printf("can't IPC_RMID call\n");
	    exit(EXIT_FAILURE);
	}

	if(xp_shmctl(shmid, IPC_AUTORMID) >= 0) {
	    printf("shouldn't have succeeded IPC_AUTORMID\n");
	    exit(EXIT_FAILURE);
	}
    } else {
	if(xp_shmctl(shmid, IPC_RMID) >= 0) {
	    printf("should have failed IPC_RMID call\n");
	    exit(EXIT_FAILURE);
	}
    }	
}



