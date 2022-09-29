#include <stdio.h>
#include <fcntl.h>
#include "xp_shm_wrap.h"

#define flush_printf(x) \
{			\
    printf x;		\
    fflush(stdout);	\
} 

int main(int argc, char *argv[])
{
    partid_t part_owner, my_part;
    key_t key1, key2;
    void *addr1, *addr2;
    int shmid1, shmid2;
    partid_t part_list[MAX_PARTITIONS];
    char name[256];
    int num_parts, i;

    set_debug_level(5);
    key1 = xp_ftok(NULL, /*"/hw/partition/1",*/ 1);
    flush_printf(("part 1 key = %x\n", key1));

    key2 = xp_ftok("/hw/partition/2", 1);
    flush_printf(("part 2 key = %x\n", key2));

    num_parts = part_getlist(part_list, MAX_PARTITIONS);
    if(num_parts < 1) {
	flush_printf(("problems with getting partition number list\n"));
	exit(1);
    }
    flush_printf(("HOSTNAMES:\n"));
    for(i = 0; i < num_parts; i++) {
	part_gethostname(part_list[i], name, 256);
	flush_printf(("\t%s\n", name));
    }

    shmid1 = xp_shmget(key1, 20000, 0);
    flush_printf(("shmid1 is %x\n", shmid1));
    
    shmid2 = xp_shmget(key2, 200000, 0);
    flush_printf(("shmid2 is %x\n", shmid2));
    
    addr1 = xp_shmat(shmid1, NULL, 0);
#if (_MIPS_SIM == _ABI64)
    flush_printf(("addr1 is %llx\n", addr1));
#else 
    flush_printf(("addr1 is %x\n", addr1));
#endif

    addr2 = xp_shmat(shmid2, NULL, 0);
#if (_MIPS_SIM == _ABI64)
    flush_printf(("addr2 is %llx\n", addr2));
#else
    flush_printf(("addr2 is %x\n", addr2));
#endif

    dump_all_state();

    if(xp_shmdt(addr1) < 0) {
	flush_printf(("addr1 can't be detached\n"));
    }

    if(xp_shmdt(addr2) < 0) {
	flush_printf(("addr2 can't be detached\n"));
    }

    if(xp_shmctl(shmid1, IPC_RMID) < 0) {
	flush_printf(("shmid1 can't be removed\n"));
    }

    if(xp_shmctl(shmid2, IPC_RMID) < 0) {
	flush_printf(("shmid2 can't be removed\n"));
    }

    dump_all_state();

    return 0;
}

