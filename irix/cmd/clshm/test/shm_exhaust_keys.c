#include <stdlib.h>
#include <stdio.h>
#include "xp_shm_wrap.h"

main()
{
    int i;
    key_t key;

    for(i = 0; ; i++) {
	key = xp_ftok(NULL, 1);
	printf("key 0x%x got for index %d\n", key, i);
	fflush(stdout);
    }
}
