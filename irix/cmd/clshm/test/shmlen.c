/* 
** File:  shmlen.c
** ---------------
** program make sure that we can pass different lengths to the driver
** with it catching the error and not barfing.
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "xp_shm_wrap.h"


int main(int argc, char *argv[])
{
    key_t key;
    int shmid1, shmid2;
    void *addr;

    key = xp_ftok(NULL, 1);

    shmid1 = xp_shmget(key, 2000, 0);
    
    shmid2 = xp_shmget(key, 20000, 0);

    if(shmid1 == -1) {
	printf("should have got the first one\n");
    }

    if(shmid2 != -1) {
	printf("shouldn't have gotten the second one\n");
    }

    addr = xp_shmat(shmid1, NULL, 0);

    if(!addr) {
	printf("should have attached\n");
    }

    xp_shmdt(addr);
    xp_shmctl(shmid1, IPC_RMID);
}

