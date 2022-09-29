#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "xp_shm_wrap.h"

char *usage = "%s shmid <shmid> ....: removes shmids from clshm space\n";

int main(int argc, char *argv[])
{
    int *shmids;
    int i, j, error = 0;
    int base;
    
    if(argc < 2) {
	printf(usage, argv[0]);
	return(EXIT_FAILURE);
    }

    set_debug_level(10);
    shmids = malloc(sizeof(int) * (argc - 1));
    for(i = 1; i < argc; i++) {
	for(j = strlen(argv[i]) - 1; j > 1; j--) {
	    if(!isdigit(argv[i][j])) {
		error = 1;
		break;
	    }
	}

	base = 10;
	if(strlen(argv[i]) > 1) {
	    if(isdigit(argv[i][1])) {
		if(!isdigit(argv[i][0]) && argv[i][0] != '-') {
		    error = 1;
		}
	    } else if(argv[i][1] == 'x' || argv[i][1] == 'X') {
		base = 16;
		if(argv[i][0] != '0') {
		    error = 1;
		}
	    } else {
		error = 1;
	    }
	} else {
	    if(strlen(argv[i]) < 1 || !isdigit(argv[i][0])) {
		error = 1;
	    }
	}

	if(error) {
	    printf("keys must be in hex or a whole number\n");
	    printf(usage, argv[0]);
	    return(EXIT_FAILURE);
	}

	shmids[i-1] = strtol(argv[i], NULL, base);
    }

    error = EXIT_SUCCESS;
    for(i = 0; i < argc - 1; i++) {
	if(xp_shmctl(shmids[i], IPC_RMID) < 0) {
	    printf("can't remove shmid 0x%x (%d)\n", shmids[i], shmids[i]);
	    error = EXIT_FAILURE;
	} else {
	    printf("removed shmid 0x%x (%d)\n", shmids[i], shmids[i]);
	}
    }

    return(error);
}

